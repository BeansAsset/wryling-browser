// Copyright (c) 2026 The Wryling Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_BRAVE_SHIELDS_CORE_BROWSER_WRYLING_PUBLIC_LISTS_H_
#define BRAVE_COMPONENTS_BRAVE_SHIELDS_CORE_BROWSER_WRYLING_PUBLIC_LISTS_H_

// Wryling: Shields without Brave's component updater.
//
// Brave ships its filter lists, list catalog and scriptlet resources as
// components from go-updater.brave.com, which refuses requests without the
// BraveServiceKey that only official Brave builds have. Without it the
// components never arrive and the ad-block engines never load (the component
// service manager's gates wait for the catalog forever), so Shields blocks
// nothing - not even custom filters.
//
// When the build has no services key, Wryling instead:
//  - skips registering the ad-block components (they can only fail),
//  - opens the engine gates at startup,
//  - loads scriptlet/redirect resources from a copy bundled in the binary,
//  - subscribes once, by public URL, to the lists Brave's own default
//    components are built from (brave/adblock-resources list_catalog.json),
//    loaded into the default engine so Standard mode keeps first-party
//    protections. They appear under Settings > Content filtering and can be
//    turned off or removed like any other subscription.
//
// Only included by .cc files in brave_shields; keep it that way.

#include <string_view>

#include "brave/components/constants/brave_services_key.h"

namespace brave_shields::wryling {

// Not constexpr, so -Wunreachable-code doesn't flag the branches it guards.
inline bool HasBraveServicesKey() {
  return !std::string_view(BUILDFLAG(BRAVE_SERVICES_KEY)).empty();
}

// Local-state bool: the default subscriptions were added once. Removing one
// afterwards sticks.
inline constexpr char kDefaultListsAddedPref[] =
    "brave.ad_block.wryling_default_lists_added";

// Sources of Brave's "Default Adblock Filters" and "Default Privacy Filters"
// catalog entries (uBO's combined builds where uBO publishes one).
inline constexpr std::string_view kDefaultListUrls[] = {
    "https://easylist.to/easylist/easylist.txt",
    "https://easylist.to/easylist/easyprivacy.txt",
    "https://ublockorigin.github.io/uAssets/filters/filters.min.txt",
    "https://ublockorigin.github.io/uAssets/filters/privacy.min.txt",
    "https://ublockorigin.github.io/uAssets/filters/badware.min.txt",
    "https://ublockorigin.github.io/uAssets/filters/unbreak.min.txt",
    "https://ublockorigin.github.io/uAssets/filters/quick-fixes.min.txt",
    "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/"
    "resource-abuse.txt",
    "https://malware-filter.gitlab.io/malware-filter/"
    "urlhaus-filter-agh-online.txt",
    "https://raw.githubusercontent.com/brave/adblock-lists/master/"
    "brave-unbreak.txt",
    "https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists/"
    "brave-unbreak.txt",
    "https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists/"
    "brave-specific.txt",
};

// Default lists go to the default engine (first-party protections in
// Standard mode, like Brave's components); user-added lists keep Brave's
// behaviour (additional engine).
inline bool IsDefaultListUrl(std::string_view spec) {
  for (std::string_view url : kDefaultListUrls) {
    if (url == spec) {
      return true;
    }
  }
  return false;
}

}  // namespace brave_shields::wryling

#endif  // BRAVE_COMPONENTS_BRAVE_SHIELDS_CORE_BROWSER_WRYLING_PUBLIC_LISTS_H_
