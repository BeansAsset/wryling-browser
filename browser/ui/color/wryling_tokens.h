/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_COLOR_WRYLING_TOKENS_H_
#define BRAVE_BROWSER_UI_COLOR_WRYLING_TOKENS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace ui {
struct ColorProviderKey;
}  // namespace ui

namespace wryling {

// Wryling's colour tokens. Every native colour mixer and the WebUI theme read these, so
// a surface never defines its own colours. All colours follow the theme
// colour and its strength (see wryling_space_color.h).
struct Tokens {
  // Solid depths of the theme colour, low chroma.
  SkColor canvas;   // Page and side panel background.
  SkColor surface;  // Grouped lists and cards.
  SkColor raised;   // Dialogs, popovers and menus without glass.
  // Translucent overlays (white in dark mode, black in light mode).
  SkColor fill;       // Fields, secondary buttons, segmented track.
  SkColor hover;      // Hover on rows and controls.
  SkColor pressed;    // Pressed state.
  SkColor separator;  // Hairlines between rows.
  SkColor stroke;     // 1px outline on raised and glass shapes.
  SkColor selected;   // Selected row: the accent, translucent.
  // Text.
  SkColor text;
  SkColor text_secondary;
  SkColor text_tertiary;
  // Accents, at full chroma.
  SkColor accent;       // Fills: default buttons, selected rows, toggles.
  SkColor on_accent;    // Text and icons on `accent` (white).
  SkColor accent_text;  // Links, plain buttons, focus rings on surfaces.
  SkColor destructive;
  // The window frame behind the glass sidebar (the space tint).
  SkColor frame;
};

Tokens GetTokens(const ui::ColorProviderKey& key);

// Geometry (section 2.2/2.3), in DIPs.
inline constexpr int kRadiusXs = 4;    // Checkbox, badge.
inline constexpr int kRadiusS = 6;     // Tooltip, keycap.
inline constexpr int kRadiusM = 8;     // Row highlight, field, nav row.
inline constexpr int kRadiusL = 10;    // Button, select, segmented control.
inline constexpr int kRadiusXl = 14;   // Card, list, popover, panels.
inline constexpr int kRadiusXxl = 18;  // Dialog, sheet.
inline constexpr int kControlHeight = 28;

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_COLOR_WRYLING_TOKENS_H_
