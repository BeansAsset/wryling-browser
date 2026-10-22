/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/wryling_trails/wryling_trails_page_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "brave/browser/wryling_trails/wryling_trails_service_factory.h"
#include "brave/browser/wryling_trails/wryling_trails_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace wryling_trails {

namespace {

constexpr int kDefaultMaxTrails = 50;
constexpr base::TimeDelta kNotifyDelay = base::Milliseconds(300);

}  // namespace

void BindPageHandler(content::RenderFrameHost* frame,
                     mojo::PendingReceiver<mojom::PageHandler> receiver) {
  // Only the History page's own top frame may read trails.
  content::WebUI* web_ui = frame->GetWebUI();
  if (frame->GetParent() || !web_ui || !web_ui->GetController() ||
      !web_ui->GetController()->GetAs<HistoryUI>()) {
    mojo::ReportBadMessage("Wryling Trails requested by an unauthorized page.");
    return;
  }
  Profile* profile = Profile::FromBrowserContext(frame->GetBrowserContext());
  // No service in incognito or guest: the page gets a closed pipe.
  WrylingTrailsService* service =
      WrylingTrailsServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WrylingTrailsPageHandler>(
          service, content::WebContents::FromRenderFrameHost(frame)),
      std::move(receiver));
}

WrylingTrailsPageHandler::WrylingTrailsPageHandler(
    WrylingTrailsService* service,
    content::WebContents* web_contents)
    : service_(service->GetWeakPtr()),
      web_contents_(web_contents->GetWeakPtr()) {
  observation_.Observe(service);
}

WrylingTrailsPageHandler::~WrylingTrailsPageHandler() = default;

void WrylingTrailsPageHandler::SetPage(mojo::PendingRemote<mojom::Page> page) {
  page_.reset();
  page_.Bind(std::move(page));
}

mojom::TrailPtr WrylingTrailsPageHandler::ToMojom(TrailId id) const {
  const TrailGraph& graph = service_->graph();
  const Trail* trail = graph.GetTrail(id);
  if (!trail) {
    return nullptr;
  }
  auto result = mojom::Trail::New();
  result->id = trail->id;
  result->title = base::UTF16ToUTF8(graph.GetTitle(id));
  result->last_visit = trail->last_visit.InMillisecondsFSinceUnixEpoch();
  for (NodeId node_id : trail->nodes) {
    const TrailNode* node = graph.GetNode(node_id);
    auto out = mojom::TrailNode::New();
    out->id = node->id;
    out->parent = node->parent_id;
    out->url = node->url;
    out->title = base::UTF16ToUTF8(node->title);
    out->favicon_url = node->favicon_url;
    out->time = node->time.InMillisecondsFSinceUnixEpoch();
    out->branch = node->branch;
    out->form_submission = node->form_submission;
    out->opened_in_new_tab = node->opened_in_new_tab;
    out->redirects = node->redirects;
    WrylingTrailsTabHelper* tab = service_->FindTabShowing(node->id);
    out->open_in_tab = tab ? tab->tab_id() : -1;
    result->nodes.push_back(std::move(out));
  }
  return result;
}

void WrylingTrailsPageHandler::GetTrails(double since,
                                         int32_t max_count,
                                         GetTrailsCallback callback) {
  if (!service_) {
    std::move(callback).Run({});
    return;
  }
  service_->RunWhenLoaded(base::BindOnce(
      [](base::WeakPtr<WrylingTrailsPageHandler> self, double since,
         int32_t max_count, GetTrailsCallback callback) {
        std::vector<mojom::TrailPtr> trails;
        if (!self || !self->service_) {
          std::move(callback).Run(std::move(trails));
          return;
        }
        const size_t limit = max_count > 0 ? max_count : kDefaultMaxTrails;
        const base::Time cutoff =
            since > 0 ? base::Time::FromMillisecondsSinceUnixEpoch(since)
                      : base::Time();
        for (const Trail* trail : self->service_->graph().TrailsByRecency()) {
          if (trail->last_visit < cutoff || trails.size() >= limit) {
            break;
          }
          // A single page is plain history, not a trail.
          if (trail->nodes.size() >= 2) {
            trails.push_back(self->ToMojom(trail->id));
          }
        }
        std::move(callback).Run(std::move(trails));
      },
      weak_factory_.GetWeakPtr(), since, max_count, std::move(callback)));
}

void WrylingTrailsPageHandler::GetTrailForNode(
    int64_t node_id,
    GetTrailForNodeCallback callback) {
  if (!service_) {
    std::move(callback).Run(nullptr);
    return;
  }
  service_->RunWhenLoaded(base::BindOnce(
      [](base::WeakPtr<WrylingTrailsPageHandler> self, int64_t node_id,
         GetTrailForNodeCallback callback) {
        const TrailNode* node = self && self->service_
                                    ? self->service_->graph().GetNode(node_id)
                                    : nullptr;
        std::move(callback).Run(node ? self->ToMojom(node->trail_id) : nullptr);
      },
      weak_factory_.GetWeakPtr(), node_id, std::move(callback)));
}

void WrylingTrailsPageHandler::GetTrailForTab(int32_t tab_id,
                                              GetTrailForTabCallback callback) {
  if (!service_) {
    std::move(callback).Run(nullptr, kNoNode);
    return;
  }
  service_->RunWhenLoaded(base::BindOnce(
      [](base::WeakPtr<WrylingTrailsPageHandler> self, int32_t tab_id,
         GetTrailForTabCallback callback) {
        WrylingTrailsTabHelper* tab =
            self && self->service_ ? self->service_->FindTab(tab_id) : nullptr;
        const TrailNode* node =
            tab ? self->service_->graph().GetNode(tab->current_node())
                : nullptr;
        if (!node) {
          std::move(callback).Run(nullptr, kNoNode);
          return;
        }
        std::move(callback).Run(self->ToMojom(node->trail_id), node->id);
      },
      weak_factory_.GetWeakPtr(), tab_id, std::move(callback)));
}

void WrylingTrailsPageHandler::Restore(int64_t node_id,
                                       RestoreCallback callback) {
  if (!service_ || !service_->loaded()) {
    std::move(callback).Run(mojom::RestoreResult::kNotFound);
    return;
  }
  std::move(callback).Run(service_->Restore(node_id, web_contents_.get()));
}

void WrylingTrailsPageHandler::BackToBranchPoint(
    int32_t tab_id,
    BackToBranchPointCallback callback) {
  if (!service_ || !service_->loaded()) {
    std::move(callback).Run(mojom::RestoreResult::kNotFound, kNoNode);
    return;
  }
  NodeId node = kNoNode;
  const mojom::RestoreResult result =
      service_->BackToBranchPoint(tab_id, &node);
  std::move(callback).Run(result, node);
}

void WrylingTrailsPageHandler::DeleteTrail(int64_t trail_id) {
  if (service_ && service_->loaded() &&
      service_->graph().RemoveTrail(trail_id)) {
    service_->OnGraphChanged();
  }
}

void WrylingTrailsPageHandler::OnTrailsChanged() {
  if (page_) {
    notify_timer_.Start(FROM_HERE, kNotifyDelay,
                        base::BindOnce(&WrylingTrailsPageHandler::NotifyPage,
                                       base::Unretained(this)));
  }
}

void WrylingTrailsPageHandler::OnTrailsServiceShutdown() {
  observation_.Reset();
  notify_timer_.Stop();
}

void WrylingTrailsPageHandler::NotifyPage() {
  if (page_) {
    page_->OnTrailsChanged();
  }
}

}  // namespace wryling_trails
