/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/spaces/wryling_dashboard_service_factory.h"

#include "base/no_destructor.h"
#include "brave/browser/ui/spaces/wryling_dashboard_page_handler.h"
#include "brave/browser/ui/spaces/wryling_dashboard_service.h"
#include "brave/components/wryling_dashboard/wryling_dashboard_binder.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace wryling {

DashboardService* DashboardServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<DashboardService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DashboardServiceFactory* DashboardServiceFactory::GetInstance() {
  static base::NoDestructor<DashboardServiceFactory> instance;
  return instance.get();
}

DashboardServiceFactory::DashboardServiceFactory()
    : ProfileKeyedServiceFactory("WrylingDashboardService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  wryling_dashboard::SetBinder(&CreateDashboardPageHandler);
}

DashboardServiceFactory::~DashboardServiceFactory() = default;

std::unique_ptr<KeyedService>
DashboardServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<DashboardService>(
      Profile::FromBrowserContext(context));
}

void DashboardServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DashboardService::RegisterProfilePrefs(registry);
}

}  // namespace wryling
