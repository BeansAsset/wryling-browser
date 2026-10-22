/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/wryling_trails/wryling_trails_service.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/thread_pool.h"
#include "brave/browser/wryling_trails/wryling_trails_tab_helper.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/history/core/common/pref_names.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace wryling_trails {

namespace {

constexpr int64_t kMaxFileSize = 64 * 1024 * 1024;

std::optional<std::string> ReadTrailsFile(const base::FilePath& path) {
  std::string json;
  if (!base::ReadFileToStringWithMaxSize(path, &json, kMaxFileSize)) {
    return std::nullopt;
  }
  return json;
}

}  // namespace

struct WrylingTrailsService::Deletion {
  bool all = false;
  bool has_range = false;
  base::Time begin;
  base::Time end;
  std::optional<std::set<GURL>> restrict_urls;
  std::set<GURL> urls;
};

WrylingTrailsService::WrylingTrailsService(Profile* profile)
    : profile_(profile),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      writer_(profile->GetPath().AppendASCII(kFileName),
              file_task_runner_,
              base::Seconds(3)) {
  if (auto* history = HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)) {
    history_observation_.Observe(history);
  }
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadTrailsFile, writer_.path()),
      base::BindOnce(&WrylingTrailsService::OnLoaded,
                     weak_factory_.GetWeakPtr()));
}

WrylingTrailsService::~WrylingTrailsService() = default;

void WrylingTrailsService::OnLoaded(std::optional<std::string> json) {
  if (json) {
    std::optional<base::Value> value =
        base::JSONReader::Read(*json, base::JSON_PARSE_RFC);
    if (value && value->is_dict()) {
      graph_ = TrailGraph::FromValue(value->GetDict());
    }
  }
  graph_.RemoveTrailsBefore(base::Time::Now() - kKeepFor);
  loaded_ = true;
  for (auto& task : std::exchange(pending_, {})) {
    std::move(task).Run();
  }
}

void WrylingTrailsService::RunWhenLoaded(base::OnceClosure task) {
  if (loaded_) {
    std::move(task).Run();
  } else {
    pending_.push_back(std::move(task));
  }
}

void WrylingTrailsService::OnGraphChanged() {
  writer_.ScheduleWrite(this);
  for (Observer& observer : observers_) {
    observer.OnTrailsChanged();
  }
}

bool WrylingTrailsService::IsHistoryDisabled() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled);
}

void WrylingTrailsService::AddTab(WrylingTrailsTabHelper* tab) {
  tabs_.insert(tab);
}

void WrylingTrailsService::RemoveTab(WrylingTrailsTabHelper* tab) {
  tabs_.erase(tab);
  // The page it showed is no longer open now.
  if (loaded_ && tab->current_node() != kNoNode) {
    for (Observer& observer : observers_) {
      observer.OnTrailsChanged();
    }
  }
}

WrylingTrailsTabHelper* WrylingTrailsService::FindTab(int32_t tab_id) const {
  for (WrylingTrailsTabHelper* tab : tabs_) {
    if (tab->tab_id() == tab_id) {
      return tab;
    }
  }
  return nullptr;
}

WrylingTrailsTabHelper* WrylingTrailsService::FindTabShowing(
    NodeId node) const {
  if (node == kNoNode) {
    return nullptr;
  }
  for (WrylingTrailsTabHelper* tab : tabs_) {
    if (tab->current_node() == node) {
      return tab;
    }
  }
  return nullptr;
}

void WrylingTrailsService::ActivateTab(content::WebContents* contents) {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(contents);
  if (!browser) {
    return;
  }
  TabStripModel* tab_strip = browser->GetTabStripModel();
  const int index = tab_strip->GetIndexOfWebContents(contents);
  if (index != TabStripModel::kNoTab) {
    tab_strip->ActivateTabAt(index);
  }
  browser->GetWindow()->Activate();
}

WrylingTrailsService::RestoreResult WrylingTrailsService::Restore(
    NodeId node_id,
    content::WebContents* initiator) {
  const TrailNode* node = graph_.GetNode(node_id);
  if (!node) {
    return RestoreResult::kNotFound;
  }
  if (WrylingTrailsTabHelper* tab = FindTabShowing(node_id)) {
    ActivateTab(tab->web_contents());
    return RestoreResult::kSwitchedTab;
  }

  BrowserWindowInterface* browser =
      initiator ? GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
                      initiator)
                : nullptr;
  // A plain GET of the URL: a form submission is visited afresh, never
  // resubmitted.
  NavigateParams params =
      browser ? NavigateParams(browser, node->url,
                               ui::PAGE_TRANSITION_AUTO_BOOKMARK)
              : NavigateParams(profile_.get(), node->url,
                               ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  Navigate(&params);
  if (content::WebContents* contents = params.navigated_or_inserted_contents) {
    WrylingTrailsTabHelper::MaybeCreateForWebContents(contents);
    if (auto* tab = WrylingTrailsTabHelper::FromWebContents(contents)) {
      tab->ExpectRestore(node_id);
    }
  }
  return node->form_submission ? RestoreResult::kLoadedFresh
                               : RestoreResult::kLoaded;
}

WrylingTrailsService::RestoreResult WrylingTrailsService::BackToBranchPoint(
    int32_t tab_id,
    NodeId* node) {
  *node = kNoNode;
  WrylingTrailsTabHelper* tab = FindTab(tab_id);
  if (!tab) {
    return RestoreResult::kNotFound;
  }
  *node = graph_.FindBranchPoint(tab->current_node());
  if (*node == kNoNode) {
    return RestoreResult::kNoBranchPoint;
  }
  // Prefer this tab's own back list, then a tab that shows the page (the
  // opener of a link opened in a new tab), then loading it here.
  if (!tab->HasEntryFor(*node)) {
    if (WrylingTrailsTabHelper* other = FindTabShowing(*node)) {
      ActivateTab(other->web_contents());
      return RestoreResult::kSwitchedTab;
    }
  }
  return tab->Show(*node);
}

void WrylingTrailsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WrylingTrailsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WrylingTrailsService::Shutdown() {
  history_observation_.Reset();
  for (Observer& observer : observers_) {
    observer.OnTrailsServiceShutdown();
  }
  for (WrylingTrailsTabHelper* tab : tabs_) {
    tab->OnServiceShutdown();
  }
  tabs_.clear();
  if (writer_.HasPendingWrite()) {
    writer_.DoScheduledWrite();
  }
}

void WrylingTrailsService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  Deletion deletion;
  deletion.all = deletion_info.IsAllHistory();
  if (!deletion.all) {
    if (deletion_info.time_range().IsValid()) {
      deletion.has_range = true;
      deletion.begin = deletion_info.time_range().begin();
      deletion.end = deletion_info.time_range().end();
      deletion.restrict_urls = deletion_info.restrict_urls();
    }
    for (const history::URLRow& row : deletion_info.deleted_rows()) {
      deletion.urls.insert(row.url());
    }
  }
  RunWhenLoaded(base::BindOnce(&WrylingTrailsService::ApplyDeletion,
                               weak_factory_.GetWeakPtr(),
                               std::move(deletion)));
}

void WrylingTrailsService::ApplyDeletion(const Deletion& deletion) {
  if (deletion.all) {
    graph_.Clear();
    OnGraphChanged();
    return;
  }
  auto deleted = [&](const GURL& url, base::Time time) {
    if (deletion.urls.contains(url)) {
      return true;
    }
    return deletion.has_range &&
           (deletion.begin.is_null() || time >= deletion.begin) &&
           (deletion.end.is_null() || time < deletion.end) &&
           (!deletion.restrict_urls || deletion.restrict_urls->empty() ||
            deletion.restrict_urls->contains(url));
  };
  const size_t removed = graph_.RemoveNodesIf(
      [&](const TrailNode& node) { return deleted(node.url, node.time); });
  const size_t hops =
      graph_.RemoveRedirectsIf([&](const TrailNode& node, const GURL& url) {
        return deleted(url, node.time);
      });
  if (removed || hops) {
    OnGraphChanged();
  }
}

std::optional<std::string> WrylingTrailsService::SerializeData() {
  std::string json;
  if (!base::JSONWriter::Write(graph_.ToValue(), &json)) {
    return std::nullopt;
  }
  return json;
}

}  // namespace wryling_trails
