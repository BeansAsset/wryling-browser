/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#import <AppKit/AppKit.h>

#include <map>
#include <tuple>

#include "base/no_destructor.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "brave/browser/ui/views/frame/vertical_tabs/wryling_space_switcher_view.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/gfx/image/image_skia_util_mac.h"

gfx::ImageSkia GetWrylingSymbolImage(const std::string& name,
                                     int size,
                                     SkColor color) {
  // Symbols are drawn often (every paint of the row); keep the results.
  using Key = std::tuple<std::string, int, SkColor>;
  static base::NoDestructor<std::map<Key, gfx::ImageSkia>> cache;
  const Key key{name, size, color};
  if (auto it = cache->find(key); it != cache->end()) {
    return it->second;
  }

  gfx::ImageSkia result;
  NSImage* symbol =
      [NSImage imageWithSystemSymbolName:base::SysUTF8ToNSString(name)
                accessibilityDescription:nil];
  if (symbol) {
    NSImageSymbolConfiguration* config = [[NSImageSymbolConfiguration
        configurationWithPointSize:size
                            weight:NSFontWeightMedium
                             scale:NSImageSymbolScaleMedium]
        configurationByApplyingConfiguration:
            [NSImageSymbolConfiguration
                configurationWithPaletteColors:@[ skia::SkColorToSRGBNSColor(
                                                   color) ]]];
    NSImage* configured = [symbol imageWithSymbolConfiguration:config];
    if (configured) {
      result = gfx::ImageSkiaFromResizedNSImage(configured, configured.size);
    }
  }
  if (cache->size() > 256) {
    cache->clear();
  }
  cache->emplace(key, result);
  return result;
}

void CaptureWrylingWindowsForTest(const std::string& path_prefix) {
  // Every visible window of the app (popovers are windows of their own),
  // written as <prefix>-<n>.png by screencapture, asynchronously.
  int n = 1;
  for (NSWindow* window in NSApp.windows) {
    if (!window.isVisible || window.frame.size.width < 40 ||
        window.frame.size.height < 40) {
      continue;
    }
    base::LaunchOptions options;
    base::LaunchProcess(
        {"/usr/sbin/screencapture", "-x", "-o",
         "-l" + base::NumberToString(window.windowNumber),
         path_prefix + "-" + base::NumberToString(n++) + ".png"},
        options);
  }
}
