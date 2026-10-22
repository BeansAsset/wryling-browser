/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/color/wryling_menu_color_mixer.h"

#include "brave/browser/ui/color/wryling_tokens.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_utils.h"

void AddWrylingMenuColorMixer(ui::ColorProvider* provider,
                              const ui::ColorProviderKey& key) {
  const auto tokens = wryling::GetTokens(key);
  const SkColor background = tokens.raised;
  const SkColor foreground = tokens.text;
  const SkColor hovered = tokens.accent;
  const SkColor selected = tokens.accent;
  const SkColor secondary =
      color_utils::GetResultingPaintColor(tokens.text_secondary, background);
  const SkColor disabled =
      color_utils::GetResultingPaintColor(tokens.text_tertiary, background);

  // Keep menu surfaces opaque until their widgets have a native glass backing.
  auto& mixer = provider->AddPostprocessingMixer();
  mixer[ui::kColorMenuBackground] = {background};
  mixer[ui::kColorMenuBorder] = {tokens.stroke};
  mixer[ui::kColorMenuSeparator] = {tokens.separator};
  mixer[ui::kColorMenuItemBackgroundHighlighted] = {hovered};
  mixer[ui::kColorMenuItemBackgroundSelected] = {selected};
  mixer[ui::kColorMenuItemForeground] = {foreground};
  mixer[ui::kColorMenuItemForegroundHighlighted] = {tokens.on_accent};
  mixer[ui::kColorMenuItemForegroundSelected] = {tokens.on_accent};
  mixer[ui::kColorMenuItemForegroundSecondary] = {secondary};
  mixer[ui::kColorMenuItemForegroundDisabled] = {disabled};
  mixer[ui::kColorMenuIcon] = {foreground};
  mixer[ui::kColorMenuIconDisabled] = {disabled};
  mixer[ui::kColorMenuIconOnEmphasizedBackground] = {tokens.on_accent};
  mixer[ui::kColorMenuDropmarker] = {foreground};
  mixer[ui::kColorMenuButtonBackground] = {background};
  mixer[ui::kColorMenuButtonBackgroundSelected] = {selected};

  mixer[ui::kColorAppMenuProfileRowBackground] = {tokens.fill};
  mixer[ui::kColorAppMenuProfileRowChipBackground] = {tokens.raised};
  mixer[ui::kColorAppMenuProfileRowChipHovered] = {tokens.pressed};
  mixer[ui::kColorAppMenuRowBackgroundHovered] = {tokens.hover};
  mixer[ui::kColorAppMenuUpgradeRowBackground] = {tokens.fill};
  mixer[ui::kColorAppMenuUpgradeRowSubstringForeground] = {foreground};
}
