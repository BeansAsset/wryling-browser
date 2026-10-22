/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_SPACES_WRYLING_HOME_NAVIGATION_THROTTLE_H_
#define BRAVE_BROWSER_UI_SPACES_WRYLING_HOME_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

namespace wryling {

// A space's Home tab never leaves its dashboard: a navigation away from it
// (a tile, the search box, an address typed while on Home) opens in a new tab
// of the same space instead.
class HomeNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit HomeNavigationThrottle(content::NavigationThrottleRegistry& registry);
  HomeNavigationThrottle(const HomeNavigationThrottle&) = delete;
  HomeNavigationThrottle& operator=(const HomeNavigationThrottle&) = delete;
  ~HomeNavigationThrottle() override;

  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;
};

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_SPACES_WRYLING_HOME_NAVIGATION_THROTTLE_H_
