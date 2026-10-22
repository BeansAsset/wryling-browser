// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/browser/ui/color/wryling_webui_theme.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <vector>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "brave/browser/ui/color/wryling_tokens.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/color_utils.h"

std::string GetWrylingWebUIThemeCss(const ui::ColorProviderKey& key,
                                  bool shadow_host) {
  const auto tokens = wryling::GetTokens(key);
  const bool dark = key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  const SkColor tint = tokens.frame;
  const SkColor page = tokens.canvas;
  const SkColor card = tokens.surface;
  const SkColor text = tokens.text;
  const SkColor secondary = tokens.text_secondary;
  const SkColor accent = tokens.accent;
  const SkColor button = tokens.accent;
  const SkColor on_button = tokens.on_accent;
  const SkColor on_accent = tokens.on_accent;
  const SkColor selected = tokens.selected;
  const SkColor on_selected = tokens.text;
  const SkColor outline = tokens.stroke;
  const SkColor divider = tokens.separator;
  const SkColor disabled = tokens.text_tertiary;
  const SkColor hover = tokens.hover;
  // Leave system high-contrast palettes in charge when forced colours are on.
  std::string css = "@media (forced-colors: none){";
  css.append(shadow_host ? ":host:not(#z):not(#z){"
                         : "html:not(#z):not(#z){");
  const auto set = [&css](SkColor color,
                         std::initializer_list<std::string_view> names) {
    for (const auto name : names) {
      base::StrAppend(&css, {"--", name, ":",
          ui::ConvertSkColorToCSSColor(color), ";"});
      base::StrAppend(&css, {"--", name, "-rgb:",
          color_utils::SkColorToRgbString(color), ";"});
    }
  };
  set(tokens.canvas, {"wry-canvas"});
  set(tokens.surface, {"wry-surface"});
  set(tokens.raised, {"wry-raised"});
  set(tokens.fill, {"wry-fill"});
  set(tokens.hover, {"wry-hover"});
  set(tokens.pressed, {"wry-pressed"});
  set(tokens.separator, {"wry-separator"});
  set(tokens.stroke, {"wry-stroke"});
  set(tokens.selected, {"wry-selected"});
  set(tokens.text, {"wry-text"});
  set(tokens.text_secondary, {"wry-text-secondary", "wry-text-2"});
  set(tokens.text_tertiary, {"wry-text-tertiary", "wry-text-3"});
  set(tokens.accent, {"wry-accent"});
  set(tokens.accent_text, {"wry-accent-text"});
  set(tokens.on_accent, {"wry-on-accent"});
  set(tokens.destructive, {"wry-destructive"});
  set(tokens.frame, {"wry-frame"});
  const SkColor trail_card = SkColorSetARGB(14, 255, 255, 255);
  const SkColor trail_current = SkColorSetARGB(36, 255, 255, 255);
  std::vector<SkColor> trail_backgrounds;
  for (SkColor background :
       {tokens.canvas, color_utils::AlphaBlend(tokens.frame, tokens.canvas,
                                              0.3f)}) {
    trail_backgrounds.push_back(background);
    for (int i = 0; i < 2; ++i) {
      background = color_utils::GetResultingPaintColor(trail_card, background);
      trail_backgrounds.push_back(background);
    }
  }
  for (int i = 0; i < 5; ++i) {
    trail_backgrounds.push_back(color_utils::AlphaBlend(
        tokens.accent, tokens.surface, static_cast<SkAlpha>(20 + i * 9)));
  }
  const size_t background_count = trail_backgrounds.size();
  for (size_t i = 0; i < background_count; ++i) {
    const SkColor background = trail_backgrounds[i];
    const SkColor current =
        color_utils::GetResultingPaintColor(trail_current, background);
    trail_backgrounds.push_back(current);
    for (SkColor base : {background, current}) {
      const SkColor hovered =
          color_utils::GetResultingPaintColor(tokens.hover, base);
      trail_backgrounds.push_back(hovered);
      trail_backgrounds.push_back(
          color_utils::GetResultingPaintColor(tokens.hover, hovered));
      for (SkColor fill_base : {base, hovered}) {
        trail_backgrounds.push_back(color_utils::GetResultingPaintColor(
            SkColorSetA(tokens.accent, 51), fill_base));
      }
    }
  }
  const auto trail_contrast = [&trail_backgrounds](SkColor foreground) {
    float minimum = 21.0f;
    for (SkColor background : trail_backgrounds) {
      minimum = std::min(minimum, color_utils::GetContrastRatio(
          color_utils::GetResultingPaintColor(foreground, background),
          background));
    }
    return minimum;
  };
  // History's gradient and white overlays differ from the standard surfaces.
  // Keep the shared hue, raising text contrast on their actual composites.
  SkColor trail_secondary = tokens.text_secondary;
  while (trail_contrast(trail_secondary) < 4.6f &&
         SkColorGetA(trail_secondary) < SK_AlphaOPAQUE) {
    trail_secondary =
        SkColorSetA(trail_secondary, SkColorGetA(trail_secondary) + 1);
  }
  SkColor trail_accent_text = tokens.accent_text;
  for (int alpha = 1; trail_contrast(trail_accent_text) < 4.6f &&
                      alpha <= SK_AlphaOPAQUE; ++alpha) {
    trail_accent_text = color_utils::AlphaBlend(
        tokens.text, tokens.accent_text, static_cast<SkAlpha>(alpha));
  }
  set(trail_secondary, {"wry-trail-secondary", "wry-trail-domain"});
  set(trail_accent_text, {"wry-trail-accent-text"});
  set(trail_card, {"wry-trail-card"});
  set(SkColorSetARGB(15, 255, 255, 255), {"wry-trail-stroke"});
  set(SkColorSetARGB(36, 255, 255, 255), {"wry-trail-current"});
  set(SkColorSetA(tokens.accent, 51), {"wry-trail-open-fill"});
  // Each trail branch gets its own soft hue (accent, then 72° steps round
  // the wheel) so neighbouring branches are told apart at a glance; all at
  // the same gentle strength over the card.
  color_utils::HSL accent_hsl;
  color_utils::SkColorToHSL(tokens.accent, &accent_hsl);
  for (int i = 0; i < 5; ++i) {
    const std::string name = "wry-trail-branch-" + base::NumberToString(i);
    color_utils::HSL hue = accent_hsl;
    hue.h = accent_hsl.h + i * 0.2;
    hue.h -= static_cast<int>(hue.h);
    hue.s = std::max(hue.s, 0.55);
    set(color_utils::AlphaBlend(color_utils::HSLToSkColor(hue, SK_AlphaOPAQUE),
                                tokens.surface, static_cast<SkAlpha>(38)),
        {name});
  }
  css.append("--wry-trail-canvas:linear-gradient(140deg,color-mix(in srgb,var(--wry-frame) 30%,var(--wry-canvas)),var(--wry-canvas) 70%);"
             "--wry-trail-radius:16px;--wry-trail-row-height:34px;--wry-trail-time-width:62px;--wry-trail-indent:18px;");
  const auto geometry = [&css](int value, std::string_view name) {
    base::StrAppend(&css, {"--wry-", name, ":", base::NumberToString(value), "px;"});
  };
  geometry(wryling::kRadiusXs, "radius-xs");
  geometry(wryling::kRadiusS, "radius-s");
  geometry(wryling::kRadiusM, "radius-m");
  geometry(wryling::kRadiusL, "radius-l");
  geometry(wryling::kRadiusXl, "radius-xl");
  geometry(wryling::kRadiusXxl, "radius-xxl");
  geometry(999, "radius-pill");
  geometry(wryling::kControlHeight, "control-height");
  css.append("--wry-space-1:4px;--wry-space-2:8px;--wry-space-3:12px;"
             "--wry-space-4:16px;--wry-space-5:20px;--wry-space-6:24px;"
             "--wry-space-8:32px;--wry-space-12:48px;"
             "--wry-row-height:44px;--wry-dense-row-height:36px;"
             "--wry-small-control-height:22px;--wry-large-control-height:32px;"
             "--wry-icon:16px;--wry-toolbar-icon:20px;--wry-empty-icon:40px;"
             "--wry-settings-content-width:600px;--wry-settings-side-padding:36px;--wry-nav-row-height:30px;--wry-nav-gap:10px;--wry-menu-padding:5px;--wry-menu-row-height:26px;--wry-menu-item-radius:6px;--wry-content-width:680px;--wry-wide-content-width:920px;"
             "--wry-font:system-ui,-apple-system,BlinkMacSystemFont,sans-serif;"
             "--wry-body:400 13px/18px var(--wry-font);"
             "--wry-callout:400 12px/16px var(--wry-font);"
             "--wry-caption:400 11px/14px var(--wry-font);"
             "--wry-headline:600 13px/16px var(--wry-font);"
             "--wry-title:600 20px/25px var(--wry-font);"
             "--wry-large-title:700 26px/32px var(--wry-font);"
             "--wry-motion:120ms ease-out;--wry-panel-motion:220ms ease-out;"
             "--wry-focus:color-mix(in srgb,var(--wry-accent) 50%,transparent);"
             "--wry-web-glass:color-mix(in srgb,var(--wry-surface) 72%,transparent);"
             "--wry-web-glass-filter:blur(24px) saturate(1.4);");
  css.append("--wry-button-padding:14px;--wry-toggle-width:32px;--wry-toggle-height:18px;"
             "--wry-checkbox-size:16px;--wry-checkmark-size:10px;--wry-search-icon:14px;--wry-search-icon-inset:10px;--wry-segment-inset:2px;--wry-hairline:1px;"
             "--wry-source-width:220px;--wry-search-width:280px;--wry-switch-thumb:#fff;"
             "--wry-knob-shadow:0 1px 2px #00000033;");
  css.append(dark ? "--wry-shadow:0 8px 24px #00000047,0 1px 2px #00000033;"
                  : "--wry-shadow:0 8px 24px #00000024,0 1px 2px #0000001a;");
  set(page, {
      "color-sys-base",
      "color-sys-surface",
      "color-webui-page-background",
      "wryling-theme-page",
      "color-side-panel-content-background",
      "color-side-panel-background",
      "color-side-panel-content-area-background",
      "color-side-panel-customize-chrome-theme-background",
      "color-settings-webui-page-background",
      "md-background-color",
      "leo-color-schemes-background",
      "leo-color-schemes-surface"});
  set(card, {
      "color-sys-base-container",
      "color-sys-base-container-elevated",
      "color-sys-surface1",
      "color-sys-surface2",
      "color-sys-surface3",
      "color-sys-surface4",
      "color-sys-surface5",
      "color-sys-neutral-container",
      "color-button-background-prominent-disabled",
      "color-button-background-tonal-disabled",
      "color-checkbox-container-disabled",
      "color-toggle-button-track-on-disabled",
      "color-toggle-button-thumb-on-disabled",
      "color-toggle-button-track-off",
      "color-slider-disabled-container",
      "color-sys-surface-variant",
      "color-webui-card-background",
      "wryling-theme-card",
      "leo-color-material-thin",
      "leo-color-material-regular",
      "leo-color-material-thick",
      "color-side-panel-card-background",
      "color-side-panel-search-field-background",
      "color-side-panel-customize-chrome-custom-option-background",
      "color-side-panel-customize-chrome-theme-snapshot-background",
      "color-webui-dialog-background",
      "color-webui-dialog-container-background",
      "color-settings-info-card-background",
      "color-download-manager-item-background",
      "color-download-manager-item-background-inactive",
      "cr-card-background-color",
      "cr-menu-background-color",
      "cr-dialog-background-color",
      "cr-input-background-color",
      "color-toolbar-search-field-background",
      "color-toolbar-search-field-background-hover",
      "cr-toolbar-search-field-background",
      "leo-color-schemes-surface-container-lowest",
      "leo-color-schemes-surface-variant",
      "leo-color-container-background",
      "leo-color-schemes-surface-container",
      "leo-color-schemes-surface-container-low",
      "leo-color-schemes-surface-container-high",
      "leo-color-schemes-surface-container-highest"});
  set(text, {
      "color-sys-on-surface",
      "color-sys-on-surface-primary",
      "color-sys-on-base",
      "color-primary-foreground",
      "wryling-theme-text",
      "color-side-panel-card-primary-foreground",
      "color-side-panel-badge-foreground",
      "sp-empty-state-heading-color",
      "color-history-page-text",
      "color-bookmark-manager-item-text",
      "color-bookmark-manager-item-title",
      "cr-primary-text-color",
      "cr-title-text-color",
      "cr-input-color",
      "color-toolbar-search-field-foreground",
      "color-toolbar-search-field-icon",
      "cr-toolbar-search-field-input-text-color",
      "cr-toolbar-search-field-input-icon-color",
      "cr-toolbar-search-field-icon-color",
      "cr-toolbar-search-field-input-caret-color",
      "cr-icon-button-fill-color",
      "leo-color-text-primary",
      "leo-color-schemes-on-background",
      "leo-color-schemes-on-surface"});
  set(secondary, {
      "color-sys-on-surface-variant",
      "color-sys-on-surface-secondary",
      "color-sys-on-surface-subtle",
      "color-secondary-foreground",
      "wryling-theme-secondary",
      "color-side-panel-card-secondary-foreground",
      "sp-empty-state-body-color",
      "color-settings-columned-section-description-header",
      "color-download-manager-item-inactive",
      "md-loading-message-color",
      "cr-secondary-text-color",
      "cr-input-label-color",
      "cr-input-placeholder-color",
      "cr-toolbar-search-field-prompt-color",
      "history-item-time-color",
      "sidebar-footer-text-color",
      "leo-color-text-secondary",
      "leo-color-text-tertiary",
      "leo-color-icon-default",
      "leo-color-icon-secondary",
      "leo-color-schemes-on-surface-variant"});
  set(accent, {
      "color-sys-primary",
      "wryling-theme-accent",
      "color-side-panel-bookmarks-selected-folder-icon",
      "color-side-panel-badge-foreground-updated",
      "color-button-background-prominent",
      "color-button-foreground",
      "color-toggle-button-track-on",
      "color-slider-active",
      "color-slider-active-container",
      "color-slider-active-ripple",
      "color-tab-nav-item-selected",
      "color-sys-secondary",
      "color-sys-tertiary",
      "color-link-foreground-default",
      "color-checkbox-foreground-checked",
      "color-sys-state-focus-ring",
      "color-sys-state-ripple-primary",
      "color-history-page-bookmark-star",
      "color-download-manager-progress",
      "cr-link-color",
      "cr-checked-color",
      "cr-focus-outline-color",
      "cr-tabs-selected-color",
      "cr-button-text-color",
      "cr-checkbox-checked-box-color",
      "cr-checkbox-checked-box-background-color",
      "cr-radio-button-checked-color",
      "cr-toggle-checked-bar-color",
      "cr-input-focus-color",
      "cr-progress-active-color",
      "interactive-color",
      "folder-icon-color",
      "controlled-by-active-color",
      "controlled-by-link-color",
      "leo-color-schemes-primary",
      "leo-color-schemes-secondary",
      "leo-color-schemes-tertiary",
      "leo-color-button-background",
      "leo-color-text-interactive",
      "leo-color-text-interactive-visited",
      "leo-color-icon-interactive"});
  set(on_accent, {
      "color-sys-on-primary",
      "color-button-foreground-prominent",
      "color-checkbox-check",
      "color-toggle-button-thumb-on",
      "color-toggle-button-thumb-on-hover",
      "color-slider-active-text",
      "color-sys-on-secondary",
      "color-sys-on-tertiary",
      "cr-checkbox-mark-color",
      "cr-toggle-checked-button-color",
      "leo-color-schemes-on-primary",
      "leo-color-schemes-on-secondary",
      "leo-color-schemes-on-tertiary"});
  set(selected, {
      "color-sys-primary-container",
      "color-side-panel-bookmarks-selected-folder-background",
      "color-side-panel-badge-background",
      "color-side-panel-badge-background-updated",
      "color-side-panel-customize-chrome-theme-checkmark-background",
      "color-button-background-tonal",
      "color-nav-menu-item-background-selected",
      "color-sys-secondary-container",
      "color-sys-tertiary-container",
      "color-sys-tonal-container",
      "color-sys-base-tonal-container",
      "color-sys-state-text-highlight",
      "color-sys-state-focus-highlight",
      "color-bookmark-manager-item-background-selected",
      "highlight-color",
      "cr-progress-container-color",
      "leo-color-container-highlight",
      "leo-color-container-interactive",
      "leo-color-schemes-primary-container",
      "leo-color-schemes-secondary-container",
      "leo-color-schemes-tertiary-container"});
  set(on_selected, {
      "color-sys-on-primary-container",
      "color-side-panel-bookmarks-selected-folder-foreground",
      "color-side-panel-customize-chrome-theme-checkmark-foreground",
      "color-side-panel-customize-chrome-custom-option-foreground",
      "color-button-foreground-tonal",
      "color-nav-menu-item-selected",
      "color-sys-on-secondary-container",
      "color-sys-on-tertiary-container",
      "color-sys-on-tonal-container",
      "color-sys-on-base-tonal-container",
      "color-sys-state-on-text-highlight",
      "leo-color-schemes-on-primary-container",
      "leo-color-schemes-on-secondary-container",
      "leo-color-schemes-on-tertiary-container"});
  set(outline, {
      "color-sys-outline",
      "color-side-panel-scrollbar-thumb",
      "color-button-border",
      "color-checkbox-foreground-unchecked",
      "color-toggle-button-thumb-off",
      "color-sys-tonal-outline",
      "color-sys-neutral-outline",
      "color-bookmark-manager-item-outline",
      "cr-checkbox-unchecked-box-color",
      "cr-radio-button-unchecked-color",
      "cr-toggle-unchecked-bar-color",
      "cr-toggle-unchecked-button-color",
      "leo-color-schemes-outline",
      "leo-color-divider-strong"});
  set(divider, {
      "color-sys-divider",
      "color-side-panel-divider",
      "color-side-panel-edit-footer-border",
      "color-side-panel-customize-chrome-corner-ntp-border",
      "color-side-panel-customize-chrome-web-store-border",
      "color-button-border-disabled",
      "color-checkbox-outline-disabled",
      "color-history-page-separator",
      "cr-separator-color",
      "cr-button-border-color",
      "card-border-color",
      "separator-color",
      "leo-color-divider-faint",
      "leo-color-divider-subtle",
      "leo-color-divider-interactive",
      "leo-color-schemes-outline-variant"});
  set(disabled, {
      "color-sys-state-disabled",
      "color-button-foreground-disabled",
      "color-checkbox-check-disabled",
      "color-toggle-button-thumb-off-disabled",
      "color-slider-disabled",
      "cr-disabled-text-color",
      "controlled-by-inactive-color",
      "folder-inactive-color",
      "leo-color-text-disabled",
      "leo-color-icon-disabled"});
  set(hover, {
      "color-sys-state-hover-on-subtle",
      "color-sys-state-pressed",
      "color-sys-state-hover-on-prominent",
      "color-sys-state-ripple-neutral-on-subtle",
      "cr-hover-background-color",
      "leo-color-button-hover"});
  set(button, {"wryling-theme-button", "color-button-background-prominent",
               "leo-color-button-background", "leo-color-schemes-primary"});
  set(tokens.raised, {"cr-menu-background-color", "cr-dialog-background-color",
      "color-webui-dialog-background", "color-webui-dialog-container-background"});
  set(tokens.fill, {"color-button-background-tonal", "color-sys-base-tonal-container"});
  set(tokens.text, {"color-button-foreground-tonal"});
  set(on_button, {"color-button-foreground-prominent",
                  "leo-color-schemes-on-primary"});
  color_utils::HSL illustration;
  color_utils::SkColorToHSL(tint, &illustration);
  base::StrAppend(&css, {"--wryling-illustration-filter:grayscale(1) sepia(1) "
      "saturate(", base::NumberToString(illustration.s * 2), ") hue-rotate(",
      base::NumberToString(illustration.h * 360 - 40), "deg);",
      "--leo-button-radius:var(--wry-radius-l);--cr-button-border-radius:var(--wry-radius-l);"});
  base::StrAppend(&css, {"--brave-toolbar-gradient:linear-gradient(",
      ui::ConvertSkColorToCSSColor(page), ",",
      ui::ConvertSkColorToCSSColor(page), ");--md-toolbar-color:",
      ui::ConvertSkColorToCSSColor(page), ";"});
  if (!shadow_host) {
    base::StrAppend(&css, {"accent-color:",
        ui::ConvertSkColorToCSSColor(accent), ";color:",
        ui::ConvertSkColorToCSSColor(text), ";background-color:",
        ui::ConvertSkColorToCSSColor(page), ";"});
  }
  css.push_back('}');
  if (!shadow_host) {
    css.append("html:not(#z):not(#z) body{background-color:"
               "var(--wryling-theme-page);}");
  }
  css.push_back('}');
  return css;
}
