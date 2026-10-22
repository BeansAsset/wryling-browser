/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_COLOR_WRYLING_SPACE_COLOR_H_
#define BRAVE_BROWSER_UI_COLOR_WRYLING_SPACE_COLOR_H_

#include "third_party/skia/include/core/SkColor.h"

namespace ui {
struct ColorProviderKey;
}  // namespace ui

namespace wryling {

// Profile pref ("Tint strength" in settings): how strong the space colour
// is, 0-100. Low values fade it toward a neutral grey everywhere (frame,
// tabs, menus); it also sets how strongly it tints the Liquid Glass panel.
// 50 is the colour as picked.
inline constexpr char kTintOpacityPref[] = "wryling.tint_opacity";
inline constexpr int kDefaultTintOpacity = 50;

// Profile pref ("Clear glass" in settings): the sidebar's Liquid Glass has no
// tint and the window behind it is glass too, so the desktop shows through
// (macOS 26 Finder style). Off by default.
inline constexpr char kClearGlassPref[] = "wryling.sidebar_clear_glass";

// The pref isn't part of the colour provider key, so GetSpaceTint() reads a
// process-wide copy, set from the active window's profile. Returns true when
// the value changed; the caller then refreshes the colour providers.
bool SetTintStrength(int percent);
int GetTintStrength();

// The one source for Wryling's space colour: the colour the user picked
// (Chromium's theme user colour, ThemeService::SetUserColor, which arrives as
// ColorProviderKey::user_color), or Wryling's default violet. Its lightness
// is kept in a readable range for the light/dark mode, so white (dark mode)
// or dark (light mode) text stays legible on it, and its saturation follows
// the tint strength. Every Wryling colour mixer
// (window chrome, menus, ...) derives from this.
SkColor GetSpaceTint(const ui::ColorProviderKey& key);

// The space colour before the tint strength is applied: what the user
// picked, or the default violet. For showing the choice itself (settings).
SkColor GetBaseSpaceColor(const ui::ColorProviderKey& key);

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_COLOR_WRYLING_SPACE_COLOR_H_
