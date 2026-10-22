/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_PAGE_HANDLER_H_
#define BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "brave/browser/wryling_trails/wryling_trails.mojom.h"
#include "brave/browser/wryling_trails/wryling_trails_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace wryling_trails {

// Binds the Trails API for the wryling://history (chrome://history) page.
void BindPageHandler(content::RenderFrameHost* frame,
                     mojo::PendingReceiver<mojom::PageHandler> receiver);

class WrylingTrailsPageHandler : public mojom::PageHandler,
                                 public WrylingTrailsService::Observer {
 public:
  WrylingTrailsPageHandler(WrylingTrailsService* service,
                           content::WebContents* web_contents);
  WrylingTrailsPageHandler(const WrylingTrailsPageHandler&) = delete;
  WrylingTrailsPageHandler& operator=(const WrylingTrailsPageHandler&) = delete;
  ~WrylingTrailsPageHandler() override;

  // mojom::PageHandler:
  void SetPage(mojo::PendingRemote<mojom::Page> page) override;
  void GetTrails(double since,
                 int32_t max_count,
                 GetTrailsCallback callback) override;
  void GetTrailForNode(int64_t node_id,
                       GetTrailForNodeCallback callback) override;
  void GetTrailForTab(int32_t tab_id, GetTrailForTabCallback callback) override;
  void Restore(int64_t node_id, RestoreCallback callback) override;
  void BackToBranchPoint(int32_t tab_id,
                         BackToBranchPointCallback callback) override;
  void DeleteTrail(int64_t trail_id) override;

  // WrylingTrailsService::Observer:
  void OnTrailsChanged() override;
  void OnTrailsServiceShutdown() override;

 private:
  mojom::TrailPtr ToMojom(TrailId id) const;
  void NotifyPage();

  base::WeakPtr<WrylingTrailsService> service_;
  base::WeakPtr<content::WebContents> web_contents_;
  mojo::Remote<mojom::Page> page_;
  base::OneShotTimer notify_timer_;
  base::ScopedObservation<WrylingTrailsService, WrylingTrailsService::Observer>
      observation_{this};
  base::WeakPtrFactory<WrylingTrailsPageHandler> weak_factory_{this};
};

}  // namespace wryling_trails

#endif  // BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_PAGE_HANDLER_H_
