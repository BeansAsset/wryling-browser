/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_PAGE_HANDLER_H_
#define BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_PAGE_HANDLER_H_

#include <optional>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "brave/components/wryling_dashboard/wryling_dashboard.mojom.h"
#include "brave/browser/ui/spaces/wryling_dashboard_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace wryling {

// Serves the dashboard API to a New Tab page (through
// wryling_dashboard::BindPageHandler, which BraveNewTabPageUI calls).
void CreateDashboardPageHandler(
    content::WebContents* web_contents,
    mojo::PendingReceiver<wryling_dashboard::mojom::PageHandler> receiver);

// The dashboard of the space the New Tab page's tab is in.
class DashboardPageHandler : public wryling_dashboard::mojom::PageHandler,
                             public DashboardService::Observer {
 public:
  DashboardPageHandler(DashboardService* service,
                       content::WebContents* web_contents);
  DashboardPageHandler(const DashboardPageHandler&) = delete;
  DashboardPageHandler& operator=(const DashboardPageHandler&) = delete;
  ~DashboardPageHandler() override;

  // wryling_dashboard::mojom::PageHandler:
  void SetPage(
      mojo::PendingRemote<wryling_dashboard::mojom::Page> page) override;
  void GetDashboard(GetDashboardCallback callback) override;
  void AddSite(const std::string& url,
               const std::string& title,
               AddSiteCallback callback) override;
  void UpdateSite(const std::string& old_url,
                  const std::string& url,
                  const std::string& title,
                  UpdateSiteCallback callback) override;
  void RemoveSite(const std::string& url) override;
  void UndoRemove() override;
  void MoveSite(const std::string& url, int32_t index) override;

  // DashboardService::Observer:
  void OnDashboardsChanged() override;
  void OnSiteInfoChanged() override;
  void OnDashboardServiceShutdown() override;

 private:
  std::string GetSpaceId();
  void NotifyPage();

  base::WeakPtr<DashboardService> service_;
  base::WeakPtr<content::WebContents> web_contents_;
  mojo::Remote<wryling_dashboard::mojom::Page> page_;
  base::OneShotTimer notify_timer_;
  // The space and its stored entry before the last removal.
  std::optional<std::pair<std::string, base::DictValue>> undo_;
  base::ScopedObservation<DashboardService, DashboardService::Observer>
      observation_{this};
};

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_PAGE_HANDLER_H_
