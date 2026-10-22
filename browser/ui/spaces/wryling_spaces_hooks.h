/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_SPACES_WRYLING_SPACES_HOOKS_H_
#define BRAVE_BROWSER_UI_SPACES_WRYLING_SPACES_HOOKS_H_

// Wryling Spaces: the few entry points that Chromium's tab code (the tab strip
// model, tab container, session code) calls. Kept free of includes so it can
// be included anywhere without widening rebuilds. See SPACES-design.md.

#include <map>
#include <string>

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace wryling {

// Session extra-data key that holds a tab's space ID.
inline constexpr char kSpaceSessionKey[] = "wryling_space";

// True when `tab` belongs to a space other than its window's current one, so
// the tab strip hides it and tab selection skips it.
bool IsTabHiddenBySpace(const tabs::TabInterface* tab);

// True when the group's tabs belong to a space that isn't current.
bool IsGroupHiddenBySpace(BrowserWindowInterface* browser,
                          const tab_groups::TabGroupId& group);

// True for a space's Home tab, which the user can't close (⌘W, the tab
// menu and middle-click leave it; closing the window still closes it).
bool IsSpaceHomeTab(const tabs::TabInterface* tab);

// The space ID to save for `tab` (empty when spaces don't apply).
std::string GetTabSpaceForSession(const tabs::TabInterface* tab);

// Puts a tab restored from a session (or ⌘⇧T) back in its saved space.
void RestoreTabSpace(BrowserWindowInterface* browser,
                     content::WebContents* contents,
                     const std::map<std::string, std::string>& extra_data);

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_SPACES_WRYLING_SPACES_HOOKS_H_
