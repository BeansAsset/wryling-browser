/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_SERVICE_FACTORY_H_
#define BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class Profile;

namespace wryling {

class DashboardService;

// Regular profiles only: incognito and guest windows have no dashboard.
class DashboardServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static DashboardService* GetForProfile(Profile* profile);
  static DashboardServiceFactory* GetInstance();

  DashboardServiceFactory(const DashboardServiceFactory&) = delete;
  DashboardServiceFactory& operator=(const DashboardServiceFactory&) = delete;

 private:
  friend base::NoDestructor<DashboardServiceFactory>;

  DashboardServiceFactory();
  ~DashboardServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_SERVICE_FACTORY_H_
