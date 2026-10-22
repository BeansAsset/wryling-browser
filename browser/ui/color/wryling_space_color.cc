/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/color/wryling_space_color.h"

#include <algorithm>
#include <atomic>

#include "ui/color/color_provider_key.h"
#include "ui/gfx/color_utils.h"

namespace wryling {

namespace {
std::atomic<int> g_tint_strength{kDefaultTintOpacity};
}  // namespace

bool SetTintStrength(int percent) {
  const int clamped = std::clamp(percent, 0, 100);
  return g_tint_strength.exchange(clamped) != clamped;
}

int GetTintStrength() {
  return g_tint_strength.load();
}

SkColor GetBaseSpaceColor(const ui::ColorProviderKey& key) {
  const bool dark = key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  if (!key.user_color) {
    // The default violet; deeper in dark mode.
    return dark ? SkColorSetRGB(0x3B, 0x2F, 0x63)
                : SkColorSetRGB(0x8F, 0x7D, 0xC9);
  }

  // Keep the user's hue and saturation; bound the lightness so text stays
  // readable (the defaults above sit at about 0.29 and 0.64).
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(*key.user_color, &hsl);
  hsl.l = dark ? std::clamp(hsl.l, 0.12, 0.34) : std::clamp(hsl.l, 0.55, 0.80);
  return color_utils::HSLToSkColor(hsl, SK_AlphaOPAQUE);
}

SkColor GetSpaceTint(const ui::ColorProviderKey& key) {
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(GetBaseSpaceColor(key), &hsl);
  // Strength 0 is a neutral grey, the default 50 the colour as picked, and
  // 100 twice as saturated (up to fully).
  hsl.s = std::clamp(hsl.s * GetTintStrength() / kDefaultTintOpacity, 0.0,
                     1.0);
  return color_utils::HSLToSkColor(hsl, SK_AlphaOPAQUE);
}

}  // namespace wryling
