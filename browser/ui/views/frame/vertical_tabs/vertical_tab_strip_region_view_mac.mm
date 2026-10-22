/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/frame/vertical_tabs/vertical_tab_strip_region_view.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "brave/browser/ui/color/wryling_space_color.h"
#include "brave/browser/ui/tabs/public/vertical_tab_controller.h"
#include "chrome/app/chrome_command_ids.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

static_assert(__OBJC__);
static_assert(BUILDFLAG(IS_APPLE));

// Wryling: the sidebar's Liquid Glass panel. Clicks go through to the tabs,
// which Chromium draws above it.
API_AVAILABLE(macos(26.0))
@interface WrylingSidebarGlassView : NSGlassEffectView
@end

@implementation WrylingSidebarGlassView
- (NSView*)hitTest:(NSPoint)point {
  return nil;
}
@end

// Wryling clear glass: the window background behind the chrome, a strong
// behind-window blur (macOS's vibrancy material, as Finder's sidebar
// uses), so the desktop reads as soft colour rather than shapes.
@interface WrylingWindowBlurView : NSVisualEffectView
@end

@implementation WrylingWindowBlurView
- (NSView*)hitTest:(NSPoint)point {
  return nil;
}
@end

// Wryling: the window's themed background. Instead of one flat
// fill of the space colour: a diagonal gradient between two neighbouring
// shades of it, a soft highlight in the top-left, and fine grain over
// everything, so the chrome laid over it (white or black overlays) has depth.
// Drawn as sublayers of Chromium's opaque background view, which already sits
// below everything Chromium draws. (A sibling view of its own would make
// AppKit re-sort the content view's layers and cover the page.)
namespace {

NSString* const kWrylingThemeLayerName = @"WrylingThemeBackground";
constexpr CGFloat kGrainTile = 64;  // Points; the noise image is 2x.

CGImageRef WrylingGrainImage() {
  static CGImageRef image = nullptr;
  if (!image) {
    constexpr int kSize = 128;
    std::vector<uint8_t> data(kSize * kSize * 2);
    uint32_t seed = 0x9E3779B9u;  // Fixed, so the grain never shimmers.
    for (int i = 0; i < kSize * kSize; ++i) {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      const uint8_t alpha = (seed >> 8) & 0xFF;
      // Premultiplied grey + alpha: a white or black speck.
      data[i * 2] = (seed & 1) ? alpha : 0;
      data[i * 2 + 1] = alpha;
    }
    CGColorSpaceRef grey = CGColorSpaceCreateWithName(kCGColorSpaceGenericGrayGamma2_2);
    CFDataRef bytes = CFDataCreate(nullptr, data.data(), data.size());
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(bytes);
    image = CGImageCreate(kSize, kSize, 8, 16, kSize * 2, grey,
                          kCGImageAlphaPremultipliedLast, provider, nullptr,
                          false, kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    CFRelease(bytes);
    CGColorSpaceRelease(grey);
  }
  return image;
}

// Finds or builds the theme layer inside |host| (Chromium's opaque view).
CALayer* WrylingThemeLayer(NSView* host) {
  for (CALayer* layer in host.layer.sublayers) {
    if ([layer.name isEqualToString:kWrylingThemeLayerName]) {
      return layer;
    }
  }
  CALayer* theme = [CALayer layer];
  theme.name = kWrylingThemeLayerName;
  theme.masksToBounds = YES;

  CAGradientLayer* base = [CAGradientLayer layer];
  base.name = @"base";
  base.startPoint = CGPointMake(0, 1);  // Top-left (layer y is up)...
  base.endPoint = CGPointMake(1, 0);    // ...to bottom-right.
  CAGradientLayer* highlight = [CAGradientLayer layer];
  highlight.name = @"highlight";
  highlight.type = kCAGradientLayerRadial;
  highlight.startPoint = CGPointMake(0.1, 0.95);
  highlight.endPoint = CGPointMake(0.9, -0.2);

  // Grain: one noise tile, repeated across and down.
  CALayer* tile = [CALayer layer];
  tile.contents = (__bridge id)WrylingGrainImage();
  tile.contentsScale = 2;
  tile.frame = CGRectMake(0, 0, kGrainTile, kGrainTile);
  CAReplicatorLayer* row = [CAReplicatorLayer layer];
  row.name = @"row";
  row.instanceTransform = CATransform3DMakeTranslation(kGrainTile, 0, 0);
  [row addSublayer:tile];
  CAReplicatorLayer* grain = [CAReplicatorLayer layer];
  grain.name = @"grain";
  grain.instanceTransform = CATransform3DMakeTranslation(0, kGrainTile, 0);
  [grain addSublayer:row];

  [theme addSublayer:base];
  [theme addSublayer:highlight];
  [theme addSublayer:grain];
  [host.layer insertSublayer:theme atIndex:0];
  return theme;
}

void UpdateWrylingThemeLayer(NSView* host, SkColor space, bool dark,
                             bool visible) {
  CALayer* theme = WrylingThemeLayer(host);
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  theme.hidden = !visible;
  const CGRect bounds = host.layer.bounds;
  theme.frame = bounds;
  CAGradientLayer* base = nil;
  CAGradientLayer* highlight = nil;
  CAReplicatorLayer* grain = nil;
  for (CALayer* layer in theme.sublayers) {
    if ([layer.name isEqualToString:@"base"]) {
      base = static_cast<CAGradientLayer*>(layer);
    } else if ([layer.name isEqualToString:@"highlight"]) {
      highlight = static_cast<CAGradientLayer*>(layer);
    } else if ([layer.name isEqualToString:@"grain"]) {
      grain = static_cast<CAReplicatorLayer*>(layer);
    }
  }
  base.frame = bounds;
  highlight.frame = bounds;
  grain.frame = bounds;
  CAReplicatorLayer* row = static_cast<CAReplicatorLayer*>(
      grain.sublayers.firstObject);
  row.instanceCount = static_cast<NSInteger>(ceil(bounds.size.width / kGrainTile));
  grain.instanceCount =
      static_cast<NSInteger>(ceil(bounds.size.height / kGrainTile));
  grain.opacity = dark ? 0.05 : 0.04;

  NSColor* rgb = [NSColor colorWithSRGBRed:SkColorGetR(space) / 255.0
                                     green:SkColorGetG(space) / 255.0
                                      blue:SkColorGetB(space) / 255.0
                                     alpha:1];
  CGFloat h = 0, s = 0, b = 0, a = 0;
  [rgb getHue:&h saturation:&s brightness:&b alpha:&a];
  const auto shade = [&](CGFloat dh, CGFloat ds, CGFloat db) {
    CGFloat hue = h + dh;
    hue -= floor(hue);
    return (__bridge id)[NSColor
        colorWithHue:hue
          saturation:std::clamp(s + ds, 0.0, 1.0)
          brightness:std::clamp(b + db, 0.0, 1.0)
               alpha:1]
        .CGColor;
  };
  // Two neighbouring shades: a little lighter and cooler top-left, a little
  // deeper and warmer bottom-right.
  base.colors = @[ shade(-0.03, -0.04, dark ? 0.05 : 0.03),
                   shade(0.04, 0.06, dark ? -0.04 : -0.06) ];
  highlight.colors = @[
    (__bridge id)[NSColor colorWithWhite:1 alpha:dark ? 0.07 : 0.22].CGColor,
    (__bridge id)[NSColor colorWithWhite:1 alpha:0].CGColor
  ];
  [CATransaction commit];
}

}  // namespace

namespace {

// Chromium's opaque window background, present only while the window uses the
// glass frame (see BrowserNativeWidgetMac::UpdateGlassEligibility).
NSString* const kOpaqueFrameBackgroundId = @"OpaqueFrameBackgroundView";

NSMenuItem* findMenuItemWithCommandRecursively(NSMenu* menu, int commandId) {
  if (NSMenuItem* menuItem = [menu itemWithTag:commandId]) {
    return menuItem;
  }

  for (NSMenuItem* item in [menu itemArray]) {
    if (NSMenu* submenu = [item submenu]) {
      if (submenu == [NSApp servicesMenu]) {
        continue;
      }

      if (NSMenuItem* result =
              findMenuItemWithCommandRecursively(submenu, commandId)) {
        return result;
      }
    }
  }

  return nil;
}

NSString* keyCombinationForMenuItem(NSMenuItem* item) {
  NSMutableString* string = [NSMutableString string];
  NSUInteger modifiers = item.keyEquivalentModifierMask;

  // Appended "+" after the modifier as key combination
  // is splited to each part and rendered separately.
  if (modifiers & NSEventModifierFlagCommand) {
    [string appendString:@"\u2318+"];
  }
  if (modifiers & NSEventModifierFlagControl) {
    [string appendString:@"\u2303+"];
  }
  if (modifiers & NSEventModifierFlagOption) {
    [string appendString:@"\u2325+"];
  }
  if (modifiers & NSEventModifierFlagShift) {
    [string appendString:@"\u21E7+"];
  }

  [string appendString:[item.keyEquivalent uppercaseString]];
  return string;
}

}  // namespace

std::u16string BraveVerticalTabStripRegionView::GetShortcutTextForNewTabButton(
    BrowserView* browser_view) {
  // On Mac, users can configure accelerators for items in main menu. But
  // Chromium doesn't track them accurately.
  // https://github.com/chromium/chromium/blob/446e417e2661abe0090bfd16a2cbadd1cb88796b/chrome/browser/global_keyboard_shortcuts_mac.h#L76
  // So we should look into main menus in order to get the accurate accelerator
  // for new tab command.

  NSMenuItem* item =
      findMenuItemWithCommandRecursively([NSApp mainMenu], IDC_NEW_TAB);
  if (!item) {
    CHECK_IS_TEST() << "Could be null in unittest with browser view";
    return u"";
  }

  return base::SysNSStringToUTF16(keyCombinationForMenuItem(item));
}

void BraveVerticalTabStripRegionView::UpdateWrylingSidebarGlass() {
  // Re-applied with every layout, since AppKit can re-show the buttons.
  UpdateWrylingTrafficLights();
  if (@available(macOS 26.0, *)) {
    views::Widget* const widget = GetWidget();
    if (!widget) {
      return;
    }
    // Null for out-of-process (app shim) windows, which never get the panel.
    NSWindow* const window = widget->GetNativeWindow().GetNativeNSWindow();
    if (!window) {
      return;
    }
    NSView* const content = window.contentView;

    WrylingSidebarGlassView* glass = nil;
    WrylingWindowBlurView* blur = nil;
    NSView* opaque = nil;
    NSView* window_glass = nil;
    static Class window_glass_class =
        NSClassFromString(@"GlassFrameBackgroundView");
    for (NSView* view in content.subviews) {
      if ([view isKindOfClass:[WrylingSidebarGlassView class]]) {
        glass = static_cast<WrylingSidebarGlassView*>(view);
      } else if ([view isKindOfClass:[WrylingWindowBlurView class]]) {
        blur = static_cast<WrylingWindowBlurView*>(view);
      } else if ([view.identifier isEqualToString:kOpaqueFrameBackgroundId]) {
        opaque = view;
      } else if (window_glass_class &&
                 [view isKindOfClass:window_glass_class]) {
        window_glass = view;
      }
    }

    // Clear glass: Chromium's full-window glass (normally hidden in Wryling,
    // see BrowserNativeWidgetMac::UpdateBackgroundGeometry) replaces the
    // solid window background, without its colour layer, so the desktop
    // shows through the window's chrome and the untinted sidebar panel.
    const bool clear = !wryling_clear_glass_pref_.GetPrefName().empty() &&
                       *wryling_clear_glass_pref_;
    // A neutral frosting keeps clear glass readable: the desktop's colours
    // glow through without competing with the text.
    const SkColor frame_color =
        GetColorProvider() ? GetColorProvider()->GetColor(ui::kColorFrameActive)
                           : SK_ColorBLACK;
    const bool dark = color_utils::IsDark(frame_color);
    const auto frosting = [dark](CGFloat alpha) {
      return dark ? [NSColor colorWithSRGBRed:24 / 255.0
                                        green:24 / 255.0
                                         blue:28 / 255.0
                                        alpha:alpha]
                  : [NSColor colorWithSRGBRed:246 / 255.0
                                        green:246 / 255.0
                                         blue:248 / 255.0
                                        alpha:alpha];
    };
    if (opaque) {
      opaque.hidden = clear;
      // Chromium's own full-window glass stays off; the blur replaces it.
      window_glass.hidden = YES;
      if (clear && !blur) {
        blur = [[WrylingWindowBlurView alloc] initWithFrame:content.bounds];
        blur.material = NSVisualEffectMaterialUnderWindowBackground;
        blur.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        blur.state = NSVisualEffectStateActive;
        blur.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        // At the very back, below everything Chromium and the panel draw.
        [content addSubview:blur positioned:NSWindowBelow relativeTo:nil];
      }
      if (blur) {
        blur.hidden = !clear;
        blur.frame = content.bounds;
      }
      // The themed gradient-and-grain background (not in clear glass, where
      // the desktop blur is the background).
      UpdateWrylingThemeLayer(opaque, frame_color, dark, /*visible=*/!clear);
    }

    // Without the glass frame the window is opaque and the panel could not
    // show through, so only place it while Chromium's background view exists.
    // Floating over the page (hovered open from a hidden sidebar) the panel
    // would sit under the page; the sidebar paints its own background then.
    const bool show =
        opaque && IsDrawn() && width() > 0 && state_ != State::kFloating;
    if (!show) {
      glass.hidden = YES;
      return;
    }

    if (!glass) {
      glass = [[WrylingSidebarGlassView alloc] initWithFrame:NSZeroRect];
      glass.style = NSGlassEffectViewStyleRegular;
      // Directly above the opaque background, below everything Chromium draws.
      [content addSubview:glass positioned:NSWindowAbove relativeTo:opaque];
    }
    glass.hidden = NO;

    // Like Finder's sidebar: full window height (the traffic lights and
    // toolbar buttons sit on it), inset from the window's edges, and wrapping
    // the tab rows with equal room on both sides. The padding that makes room
    // for it is added in UpdateBorder().
    const gfx::Rect rows = ConvertRectToWidget(GetContentsBounds());
    const CGFloat height = content.bounds.size.height;
    // With the toolbar in the sidebar the panel runs from the window's top;
    // otherwise (collapsed sidebar) the toolbar is across the top and the
    // panel starts where the sidebar does.
    const bool holds_toolbar =
        toolbar_container_ && !toolbar_container_->children().empty();
    const CGFloat top = holds_toolbar
                            ? kWrylingPanelInset
                            : ConvertRectToWidget(GetLocalBounds()).y() +
                                  kWrylingPanelInset;
    glass.frame =
        NSMakeRect(rows.x() - kWrylingPanelOutset, kWrylingPanelInset,
                   std::max(0, rows.width() + 2 * kWrylingPanelOutset),
                   std::max<CGFloat>(0, height - top - kWrylingPanelInset));
    // Height follows live window resizes between our Layout() updates.
    glass.autoresizingMask = NSViewHeightSizable;
    glass.cornerRadius = kWrylingPanelCornerRadius;

    // Apple's own glass tint, in the window's space colour, as strong as the
    // user's tint setting.
    if (clear) {
      glass.tintColor = frosting(0.12);
    } else if (const auto* const colors = GetColorProvider()) {
      // A light tint, so the glass's own highlights and depth still show;
      // the strength setting scales it.
      constexpr double kTintScale = 0.6;
      const SkColor tint = colors->GetColor(ui::kColorFrameActive);
      const int opacity = wryling_tint_opacity_pref_.GetPrefName().empty()
                              ? wryling::kDefaultTintOpacity
                              : *wryling_tint_opacity_pref_;
      glass.tintColor = [NSColor
          colorWithSRGBRed:SkColorGetR(tint) / 255.0
                     green:SkColorGetG(tint) / 255.0
                      blue:SkColorGetB(tint) / 255.0
                     alpha:std::clamp(opacity, 0, 100) / 100.0 * kTintScale];
    }
  }
}

void BraveVerticalTabStripRegionView::UpdateWrylingTrafficLights() {
  views::Widget* const widget = GetWidget();
  if (!widget) {
    return;
  }
  NSWindow* const window = widget->GetNativeWindow().GetNativeNSWindow();
  if (!window) {
    return;
  }
  // Fullscreen manages the traffic lights itself.
  const bool hide =
      state_ == State::kCollapsed && !IsBrowserFullscren() &&
      VerticalTabController::FromBrowser(browser_)
          ->ShouldHideVerticalTabsCompletelyWhenCollapsed();
  for (NSWindowButton type :
       {NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton}) {
    [window standardWindowButton:type].hidden = hide;
  }
}
