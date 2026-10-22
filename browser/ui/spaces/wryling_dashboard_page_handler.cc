/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/spaces/wryling_dashboard_page_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "brave/browser/ui/spaces/wryling_dashboard_service_factory.h"
#include "brave/browser/ui/spaces/wryling_spaces.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace wryling {

namespace {

constexpr base::TimeDelta kNotifyDelay = base::Milliseconds(100);

}  // namespace

void CreateDashboardPageHandler(
    content::WebContents* web_contents,
    mojo::PendingReceiver<wryling_dashboard::mojom::PageHandler> receiver) {
  if (!web_contents) {
    return;
  }
  // No dashboards in incognito or guest: the page gets a closed pipe.
  DashboardService* service = DashboardServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!service) {
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DashboardPageHandler>(service, web_contents),
      std::move(receiver));
}

DashboardPageHandler::DashboardPageHandler(DashboardService* service,
                                           content::WebContents* web_contents)
    : service_(service->GetWeakPtr()),
      web_contents_(web_contents->GetWeakPtr()) {
  observation_.Observe(service);
}

DashboardPageHandler::~DashboardPageHandler() = default;

void DashboardPageHandler::SetPage(
    mojo::PendingRemote<wryling_dashboard::mojom::Page> page) {
  page_.reset();
  page_.Bind(std::move(page));
}

std::string DashboardPageHandler::GetSpaceId() {
  if (web_contents_) {
    if (tabs::TabInterface* tab =
            tabs::TabInterface::MaybeGetFromContents(web_contents_.get())) {
      if (SpacesController* controller =
              SpacesController::From(tab->GetBrowserWindowInterface())) {
        return controller->GetTabSpace(tab);
      }
    }
  }
  // Not in a window (yet): the profile's first space.
  Profile* profile =
      web_contents_
          ? Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          : nullptr;
  return profile ? GetSpaces(profile->GetPrefs()).front().id : std::string();
}

void DashboardPageHandler::GetDashboard(GetDashboardCallback callback) {
  auto dashboard = wryling_dashboard::mojom::Dashboard::New();
  if (!service_ || !web_contents_) {
    std::move(callback).Run(std::move(dashboard));
    return;
  }
  const std::string space_id = GetSpaceId();
  dashboard->space_id = space_id;
  const std::vector<Space> spaces = GetSpaces(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs());
  dashboard->space_count = static_cast<int32_t>(spaces.size());
  for (const Space& space : spaces) {
    if (space.id == space_id) {
      dashboard->space_name = space.name;
      dashboard->space_color = space.color.value_or(0);
    }
  }
  for (const DashboardService::Site& site : service_->GetSites(space_id)) {
    auto tile = wryling_dashboard::mojom::Site::New();
    tile->url = site.url.spec();
    tile->title = site.title;
    tile->pinned = site.pinned;
    if (const DashboardService::Icon* icon = service_->GetIcon(site.url)) {
      tile->icon = icon->data_url;
      tile->icon_size = icon->size;
    }
    dashboard->sites.push_back(std::move(tile));
  }
  std::move(callback).Run(std::move(dashboard));
}

void DashboardPageHandler::AddSite(const std::string& url,
                                   const std::string& title,
                                   AddSiteCallback callback) {
  const GURL site = DashboardService::FixUpSiteURL(url);
  std::move(callback).Run(service_ && site.is_valid() &&
                          service_->AddSite(GetSpaceId(), site, title));
}

void DashboardPageHandler::UpdateSite(const std::string& old_url,
                                      const std::string& url,
                                      const std::string& title,
                                      UpdateSiteCallback callback) {
  const GURL site = DashboardService::FixUpSiteURL(url);
  std::move(callback).Run(
      service_ && site.is_valid() &&
      service_->UpdateSite(GetSpaceId(), GURL(old_url), site, title));
}

void DashboardPageHandler::RemoveSite(const std::string& url) {
  if (!service_) {
    return;
  }
  const std::string space_id = GetSpaceId();
  undo_.emplace(space_id, service_->GetSpaceEntry(space_id));
  service_->RemoveSite(space_id, GURL(url));
}

void DashboardPageHandler::UndoRemove() {
  if (!service_ || !undo_) {
    return;
  }
  service_->SetSpaceEntry(undo_->first, std::move(undo_->second));
  undo_.reset();
}

void DashboardPageHandler::MoveSite(const std::string& url, int32_t index) {
  if (service_) {
    service_->MoveSite(GetSpaceId(), GURL(url), index);
  }
}

void DashboardPageHandler::OnDashboardsChanged() {
  NotifyPage();
}

void DashboardPageHandler::OnSiteInfoChanged() {
  NotifyPage();
}

void DashboardPageHandler::OnDashboardServiceShutdown() {
  observation_.Reset();
}

void DashboardPageHandler::NotifyPage() {
  if (!page_ || notify_timer_.IsRunning()) {
    return;
  }
  notify_timer_.Start(FROM_HERE, kNotifyDelay,
                      base::BindOnce(
                          [](DashboardPageHandler* self) {
                            if (self->page_) {
                              self->page_->OnDashboardChanged();
                            }
                          },
                          base::Unretained(this)));
}

}  // namespace wryling
