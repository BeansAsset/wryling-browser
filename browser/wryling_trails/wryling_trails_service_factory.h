/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_SERVICE_FACTORY_H_
#define BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class Profile;

namespace wryling_trails {

class WrylingTrailsService;

// Trails exist only for ordinary profiles: incognito, guest and Tor windows
// get no service, so nothing they visit is recorded.
class WrylingTrailsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static WrylingTrailsService* GetForProfile(Profile* profile);
  static WrylingTrailsServiceFactory* GetInstance();

  WrylingTrailsServiceFactory(const WrylingTrailsServiceFactory&) = delete;
  WrylingTrailsServiceFactory& operator=(const WrylingTrailsServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<WrylingTrailsServiceFactory>;

  WrylingTrailsServiceFactory();
  ~WrylingTrailsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace wryling_trails

#endif  // BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_SERVICE_FACTORY_H_
