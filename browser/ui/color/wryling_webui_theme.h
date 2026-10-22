// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_BROWSER_UI_COLOR_WRYLING_WEBUI_THEME_H_
#define BRAVE_BROWSER_UI_COLOR_WRYLING_WEBUI_THEME_H_

#include <string>

namespace ui {
struct ColorProviderKey;
}

// Shared palette for Wryling's browser pages, side panels and new tab page.
std::string GetWrylingWebUIThemeCss(const ui::ColorProviderKey& key,
                                  bool shadow_host);

#endif  // BRAVE_BROWSER_UI_COLOR_WRYLING_WEBUI_THEME_H_
