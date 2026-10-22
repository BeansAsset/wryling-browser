/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_COLOR_WRYLING_MENU_COLOR_MIXER_H_
#define BRAVE_BROWSER_UI_COLOR_WRYLING_MENU_COLOR_MIXER_H_

namespace ui {
class ColorProvider;
struct ColorProviderKey;
}  // namespace ui

void AddWrylingMenuColorMixer(ui::ColorProvider* provider,
                             const ui::ColorProviderKey& key);

#endif  // BRAVE_BROWSER_UI_COLOR_WRYLING_MENU_COLOR_MIXER_H_
