/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/color/wryling_tokens.h"

#include <algorithm>

#include "brave/browser/ui/color/wryling_space_color.h"
#include "ui/color/color_provider_key.h"
#include "ui/gfx/color_utils.h"

namespace wryling {

namespace {

SkColor Hsl(double h, double s, double l, float alpha = 1.0f) {
  return color_utils::HSLToSkColor({h, s, l}, alpha * SK_AlphaOPAQUE);
}

SkColor WithAlpha(SkColor color, float alpha) {
  return SkColorSetA(color, alpha * SK_AlphaOPAQUE);
}

}  // namespace

// Contrast rules: each accent and text colour is adjusted until it passes
// 4.5:1 against every surface it can sit on.
Tokens GetTokens(const ui::ColorProviderKey& key) {
  const bool dark = key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  color_utils::HSL base;
  color_utils::SkColorToHSL(GetBaseSpaceColor(key), &base);
  const double h = base.h;
  const double k = GetTintStrength() / static_cast<double>(kDefaultTintOpacity);
  // Neutrals carry a little of the hue (tinted, not coloured); accents all.
  const double neutral_s = std::clamp(std::min(base.s, 0.18) * k, 0.0, 0.32);
  const double accent_s = std::clamp(base.s * k, 0.0, 1.0);

  Tokens t;
  if (dark) {
    t.canvas = Hsl(h, neutral_s, 0.085);
    t.surface = Hsl(h, neutral_s, 0.13);
    t.raised = Hsl(h, neutral_s, 0.16);
    t.text = Hsl(h, neutral_s * 0.5, 0.96);
    t.destructive = SkColorSetRGB(0xFF, 0x5A, 0x50);
  } else {
    t.canvas = Hsl(h, neutral_s, 0.965);
    t.surface = Hsl(h, neutral_s, 0.995);
    t.raised = Hsl(h, neutral_s, 0.99);
    t.text = Hsl(h, neutral_s, 0.12);
    t.destructive = SkColorSetRGB(0xE5, 0x35, 0x2B);
  }

  // Accents (section 2.1). Contrast rule: 4.5:1 for text, so each accent
  // walks its lightness until it passes; bright hues (yellow, lime, cyan)
  // end up deeper or lighter than the standard shade.
  //
  // accent: filled things (default buttons, selected rows, toggles), with
  // white text in both modes as in macOS, so it darkens until white passes.
  t.on_accent = SK_ColorWHITE;
  double fill_l = dark ? 0.56 : 0.46;
  t.accent = Hsl(h, accent_s, fill_l);
  while (color_utils::GetContrastRatio(t.accent, t.on_accent) < 4.5 &&
         fill_l > 0.15) {
    fill_l -= 0.02;
    t.accent = Hsl(h, accent_s, fill_l);
  }
  // accent_text: links, plain buttons, focus rings: lighter in dark mode,
  // deeper in light mode, until it reads on every surface it can sit on.
  const auto min_contrast = [&t](SkColor fg) {
    return std::min({color_utils::GetContrastRatio(fg, t.canvas),
                     color_utils::GetContrastRatio(fg, t.surface),
                     color_utils::GetContrastRatio(fg, t.raised)});
  };
  double text_l = dark ? 0.74 : 0.46;
  t.accent_text = Hsl(h, accent_s, text_l);
  while (min_contrast(t.accent_text) < 4.5 &&
         (dark ? text_l < 0.95 : text_l > 0.15)) {
    text_l += dark ? 0.02 : -0.02;
    t.accent_text = Hsl(h, accent_s, text_l);
  }

  const SkColor overlay = dark ? SK_ColorWHITE : SK_ColorBLACK;
  t.fill = WithAlpha(overlay, dark ? 0.07f : 0.05f);
  t.hover = WithAlpha(overlay, dark ? 0.06f : 0.04f);
  t.pressed = WithAlpha(overlay, dark ? 0.10f : 0.07f);
  t.separator = WithAlpha(overlay, dark ? 0.07f : 0.08f);
  t.stroke = WithAlpha(overlay, 0.09f);
  t.selected = WithAlpha(t.accent, dark ? 0.22f : 0.16f);
  // Secondary text is translucent text; raise its opacity until, composited
  // on each surface, it still reads at 4.5:1.
  float secondary_alpha = dark ? 0.64f : 0.62f;
  const auto secondary_on = [&t, &secondary_alpha](SkColor bg) {
    return color_utils::GetContrastRatio(
        color_utils::AlphaBlend(t.text, bg, secondary_alpha), bg);
  };
  while (std::min({secondary_on(t.canvas), secondary_on(t.surface),
                   secondary_on(t.raised)}) < 4.5f &&
         secondary_alpha < 0.95f) {
    secondary_alpha += 0.02f;
  }
  t.text_secondary = WithAlpha(t.text, secondary_alpha);
  t.text_tertiary = WithAlpha(t.text, dark ? 0.40f : 0.38f);
  t.frame = GetSpaceTint(key);
  return t;
}

}  // namespace wryling
