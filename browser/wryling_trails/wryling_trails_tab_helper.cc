/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/wryling_trails/wryling_trails_tab_helper.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "brave/browser/wryling_trails/wryling_trails_service.h"
#include "brave/browser/wryling_trails/wryling_trails_service_factory.h"
#include "brave/components/request_otr/common/buildflags/buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(ENABLE_REQUEST_OTR)
#include "brave/components/request_otr/browser/request_otr_storage_tab_helper.h"
#endif

namespace wryling_trails {

namespace {

// Navigations the browser started on the user's behalf from outside any page
// begin a new trail; everything a page itself leads to continues one.
// The index of the entry with `entry_id` in this tab's session history, or -1.
int IndexOfEntry(content::NavigationController& controller, int entry_id) {
  for (int i = 0; entry_id && i < controller.GetEntryCount(); ++i) {
    if (controller.GetEntryAtIndex(i)->GetUniqueID() == entry_id) {
      return i;
    }
  }
  return -1;
}

bool StartsTrail(ui::PageTransition transition) {
  if (transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) {
    return true;
  }
  switch (ui::PageTransitionStripQualifier(transition)) {
    case ui::PAGE_TRANSITION_TYPED:
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
    case ui::PAGE_TRANSITION_GENERATED:
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
    case ui::PAGE_TRANSITION_KEYWORD:
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      return true;
    default:
      return false;
  }
}

}  // namespace

// static
void WrylingTrailsTabHelper::MaybeCreateForWebContents(
    content::WebContents* contents) {
  if (FromWebContents(contents)) {
    return;
  }
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (WrylingTrailsService* service =
          WrylingTrailsServiceFactory::GetForProfile(profile)) {
    CreateForWebContents(contents, service);
  }
}

WrylingTrailsTabHelper::WrylingTrailsTabHelper(content::WebContents* contents,
                                               WrylingTrailsService* service)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<WrylingTrailsTabHelper>(*contents),
      service_(service) {
  service_->AddTab(this);
}

WrylingTrailsTabHelper::~WrylingTrailsTabHelper() {
  if (service_) {
    service_->RemoveTab(this);
  }
}

int32_t WrylingTrailsTabHelper::tab_id() const {
  return sessions::SessionTabHelper::IdForTab(web_contents()).id();
}

bool WrylingTrailsTabHelper::HasEntryFor(NodeId node) const {
  const int entry_id = EntryForNode(state_, node);
  return IndexOfEntry(web_contents()->GetController(), entry_id) >= 0;
}

mojom::RestoreResult WrylingTrailsTabHelper::Show(NodeId node_id) {
  const TrailNode* node =
      service_ ? service_->graph().GetNode(node_id) : nullptr;
  if (!node) {
    return mojom::RestoreResult::kNotFound;
  }
  content::NavigationController& controller = web_contents()->GetController();
  if (const int entry_id = EntryForNode(state_, node_id)) {
    const int index = IndexOfEntry(controller, entry_id);
    if (index >= 0) {
      if (index != controller.GetLastCommittedEntryIndex()) {
        controller.GoToIndex(index);
      }
      return mojom::RestoreResult::kWentBack;
    }
  }
  // A plain GET: a form submission is visited afresh, never resubmitted.
  state_.expected_restore = node_id;
  content::NavigationController::LoadURLParams params(node->url);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  controller.LoadURLWithParams(params);
  return node->form_submission ? mojom::RestoreResult::kLoadedFresh
                               : mojom::RestoreResult::kLoaded;
}

void WrylingTrailsTabHelper::OnServiceShutdown() {
  service_ = nullptr;
}

void WrylingTrailsTabHelper::WhenLoaded(
    base::OnceCallback<void(WrylingTrailsTabHelper*)> task) {
  if (!service_) {
    return;
  }
  service_->RunWhenLoaded(base::BindOnce(
      [](base::WeakPtr<WrylingTrailsTabHelper> self,
         base::OnceCallback<void(WrylingTrailsTabHelper*)> task) {
        if (self && self->service_) {
          std::move(task).Run(self.get());
        }
      },
      weak_factory_.GetWeakPtr(), std::move(task)));
}

void WrylingTrailsTabHelper::Apply(const Commit& commit) {
  const NodeId before = state_.cursor;
  const bool changed = RecordCommit(service_->graph(), state_, commit);
  // A moved cursor changes which page is "open now".
  if (changed || before != state_.cursor) {
    service_->OnGraphChanged();
  }
}

bool WrylingTrailsTabHelper::IsRequestedOffTheRecord() {
#if BUILDFLAG(ENABLE_REQUEST_OTR)
  auto* storage =
      request_otr::RequestOTRStorageTabHelper::FromWebContents(web_contents());
  return storage && storage->has_requested_otr();
#else
  return false;
#endif
}

void WrylingTrailsTabHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!service_ || !handle->IsInPrimaryMainFrame() || !handle->HasCommitted()) {
    return;
  }
  // Only tabs in a browser window (not prerenders or other background
  // contents), as for ordinary history.
  if (!GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          web_contents())) {
    return;
  }
  // Error pages and the like don't move the trail either way.
  if (handle->IsErrorPage() || !handle->ShouldUpdateHistory()) {
    return;
  }
  const ui::PageTransition transition = handle->GetPageTransition();
  Commit commit;
  if (handle->IsSameDocument()) {
    if (handle->IsHistory()) {
      commit.kind = Commit::Kind::kHistory;
    } else if (handle->DidReplaceEntry()) {
      // replaceState: the page tidying its own URL, not a new page.
      return;
    } else {
      commit.kind = Commit::Kind::kSameDocument;
    }
  } else if (handle->GetRestoreType() == content::RestoreType::kRestored) {
    commit.kind = Commit::Kind::kRestored;
  } else if (handle->IsHistory()) {
    commit.kind = Commit::Kind::kHistory;
  } else if (handle->GetReloadType() != content::ReloadType::NONE ||
             ui::PageTransitionCoreTypeIs(transition,
                                          ui::PAGE_TRANSITION_RELOAD)) {
    commit.kind = Commit::Kind::kReload;
  } else if ((transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) &&
             handle->DidReplaceEntry()) {
    commit.kind = Commit::Kind::kClientRedirect;
  } else if (StartsTrail(transition)) {
    commit.kind = Commit::Kind::kStartsTrail;
  } else {
    commit.kind = Commit::Kind::kNew;
  }

  content::NavigationEntry* entry = handle->GetNavigationEntry();
  if (!entry) {
    entry = web_contents()->GetController().GetLastCommittedEntry();
  }
  commit.url = handle->GetURL();
  commit.time = entry && !entry->GetTimestamp().is_null()
                    ? entry->GetTimestamp()
                    : base::Time::Now();
  commit.entry_id = entry ? entry->GetUniqueID() : 0;
  commit.title = entry ? entry->GetTitle() : std::u16string();
  commit.form_submission = handle->IsPost();
  for (const GURL& hop : handle->GetRedirectChain()) {
    if (hop != commit.url) {
      commit.redirects.push_back(hop);
    }
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  commit.recordable = IsRecordable(
      commit.url, {.off_the_record = profile->IsOffTheRecord(),
                   .requested_off_the_record = IsRequestedOffTheRecord(),
                   .history_disabled = service_->IsHistoryDisabled()});
  if (commit.recordable && commit.kind == Commit::Kind::kStartsTrail) {
    if (TemplateURLService* search =
            TemplateURLServiceFactory::GetForProfile(profile)) {
      if (auto metadata = search->ExtractSearchMetadata(commit.url)) {
        commit.search_terms = metadata->search_terms;
      }
    }
  }

  WhenLoaded(
      base::BindOnce([](const Commit& commit,
                        WrylingTrailsTabHelper* self) { self->Apply(commit); },
                     std::move(commit)));
}

void WrylingTrailsTabHelper::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (!service_ || new_contents == web_contents()) {
    return;
  }
  MaybeCreateForWebContents(new_contents);
  auto* opened = FromWebContents(new_contents);
  // Another profile (e.g. "Open link in incognito window") isn't followed.
  if (!opened || opened->service_ != service_) {
    return;
  }
  // The opener page is the one shown now, not wherever this tab goes next.
  WhenLoaded(base::BindOnce(
      [](base::WeakPtr<WrylingTrailsTabHelper> opened,
         WrylingTrailsTabHelper* self) {
        if (opened) {
          opened->state_.opener = self->state_.cursor;
        }
      },
      opened->weak_factory_.GetWeakPtr()));
}

void WrylingTrailsTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  if (!entry || entry->GetTitle().empty()) {
    return;
  }
  WhenLoaded(base::BindOnce(
      [](int entry_id, std::u16string title, WrylingTrailsTabHelper* self) {
        auto it = self->state_.entry_nodes.find(entry_id);
        if (it != self->state_.entry_nodes.end() &&
            self->service_->graph().SetTitle(it->second, title)) {
          self->service_->OnGraphChanged();
        }
      },
      entry->GetUniqueID(), entry->GetTitle()));
}

void WrylingTrailsTabHelper::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates,
    blink::mojom::FaviconUpdateReason reason) {
  if (!render_frame_host->IsInPrimaryMainFrame() || candidates.empty()) {
    return;
  }
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    return;
  }
  GURL icon = candidates.front()->icon_url;
  for (const auto& candidate : candidates) {
    if (candidate->icon_type == blink::mojom::FaviconIconType::kFavicon) {
      icon = candidate->icon_url;
      break;
    }
  }
  WhenLoaded(base::BindOnce(
      [](int entry_id, GURL icon, WrylingTrailsTabHelper* self) {
        auto it = self->state_.entry_nodes.find(entry_id);
        if (it != self->state_.entry_nodes.end() &&
            self->service_->graph().SetFavicon(it->second, icon)) {
          self->service_->OnGraphChanged();
        }
      },
      entry->GetUniqueID(), icon));
}

void WrylingTrailsTabHelper::WebContentsDestroyed() {
  if (service_) {
    service_->RemoveTab(this);
    service_ = nullptr;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WrylingTrailsTabHelper);

}  // namespace wryling_trails
