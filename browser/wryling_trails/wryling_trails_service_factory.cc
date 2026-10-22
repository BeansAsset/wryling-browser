/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/wryling_trails/wryling_trails_service_factory.h"

#include "base/no_destructor.h"
#include "brave/browser/wryling_trails/wryling_trails_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"

namespace wryling_trails {

// static
WrylingTrailsService* WrylingTrailsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<WrylingTrailsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
WrylingTrailsServiceFactory* WrylingTrailsServiceFactory::GetInstance() {
  static base::NoDestructor<WrylingTrailsServiceFactory> instance;
  return instance.get();
}

WrylingTrailsServiceFactory::WrylingTrailsServiceFactory()
    : ProfileKeyedServiceFactory("WrylingTrailsService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

WrylingTrailsServiceFactory::~WrylingTrailsServiceFactory() = default;

std::unique_ptr<KeyedService>
WrylingTrailsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<WrylingTrailsService>(
      Profile::FromBrowserContext(context));
}

}  // namespace wryling_trails
