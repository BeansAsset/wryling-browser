/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/color/brave_color_mixer.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "brave/browser/ui/color/brave_color_id.h"
#include "brave/browser/ui/color/color_palette.h"
#include "brave/browser/ui/color/wryling_menu_color_mixer.h"
#include "brave/browser/ui/color/wryling_space_color.h"
#include "brave/browser/ui/color/wryling_tokens.h"
#include "brave/components/brave_vpn/common/buildflags/buildflags.h"
#include "brave/components/brave_wayback_machine/buildflags/buildflags.h"
#include "brave/components/playlist/core/common/buildflags/buildflags.h"
#include "brave/components/speedreader/common/buildflags/buildflags.h"
#include "brave/ui/color/brave_ref_color_mixer.h"
#include "brave/ui/color/nala/nala_color_id.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/color/material_chrome_color_mixer.h"
#include "chrome/browser/ui/color/material_omnibox_color_mixer.h"
#include "chrome/browser/ui/color/material_side_panel_color_mixer.h"
#include "chrome/browser/ui/color/material_tab_strip_color_mixer.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(ENABLE_PLAYLIST_WEBUI)
#include "brave/browser/ui/color/playlist/playlist_color_mixer.h"
#include "brave/components/playlist/core/common/features.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "brave/browser/ui/darker_theme/darker_theme_color_transform_factory.h"
#include "brave/browser/ui/darker_theme/features.h"
#endif  // defined(TOOLKIT_VIEWS)

namespace {

// Copied from //chrome/browser/ui/omnibox//omnibox_theme.h
// As below values are not changed for several years, it would be safe to copy.
// Can't include //chrome/browser/ui/omnibox in here due to circular deps.
constexpr float kOmniboxOpacityHovered = 0.10f;
constexpr float kOmniboxOpacitySelected = 0.16f;

SkColor PickColorContrastingToToolbar(const ui::ColorProviderKey& key,
                                      const ui::ColorMixer& mixer,
                                      SkColor color1,
                                      SkColor color2) {
  // Custom theme's toolbar color will be the final color.
  auto toolbar_color = mixer.GetResultColor(kColorToolbar);
  SkColor custom_toolbar_color;
  if (key.custom_theme &&
      key.custom_theme->GetColor(ThemeProperties::COLOR_TOOLBAR,
                                 &custom_toolbar_color)) {
    toolbar_color = custom_toolbar_color;
  }
  return color_utils::PickContrastingColor(color1, color2, toolbar_color);
}

bool HasCustomToolbarColor(const ui::ColorProviderKey& key) {
  SkColor custom_toolbar_color;
  return key.custom_theme &&
         key.custom_theme->GetColor(ThemeProperties::COLOR_TOOLBAR,
                                    &custom_toolbar_color);
}

#if BUILDFLAG(ENABLE_BRAVE_VPN) || BUILDFLAG(ENABLE_SPEEDREADER)
SkColor PickSimilarColorToToolbar(const ui::ColorProviderKey& key,
                                  const ui::ColorMixer& mixer,
                                  SkColor light_theme_color,
                                  SkColor dark_theme_color) {
  auto toolbar_color = mixer.GetResultColor(kColorToolbar);
  SkColor custom_toolbar_color;
  if (key.custom_theme &&
      key.custom_theme->GetColor(ThemeProperties::COLOR_TOOLBAR,
                                 &custom_toolbar_color)) {
    toolbar_color = custom_toolbar_color;
  }

  // Give min constrast color.
  return color_utils::IsDark(toolbar_color) ? dark_theme_color
                                            : light_theme_color;
}
#endif

bool IsHighContrast() {
#if !defined(USE_AURA)
  ui::NativeTheme* native_theme = nullptr;
#else
  // For high contrast, selected rows use inverted colors to stand out more.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
#endif  // !defined(USE_AURA)
  return native_theme && native_theme->preferred_contrast() ==
                             ui::NativeTheme::PreferredContrast::kMore;
}

SkColor BlendIdTowardsMaxContrast(SkColor color, float opacity) {
  return color_utils::BlendTowardMaxContrast(color,
                                             base::ClampRound(opacity * 0xff));
}

// Hover/Select for the Omnibox are based on opacity and the result color. The
// calculation is always the same.
void AddOmniboxHoverSelect(ui::ColorMixer& mixer) {
  mixer[kColorOmniboxResultsBackgroundHovered] = {BlendIdTowardsMaxContrast(
      mixer.GetResultColor(kColorOmniboxResultsBackground),
      kOmniboxOpacityHovered)};
  mixer[kColorOmniboxResultsBackgroundSelected] = {BlendIdTowardsMaxContrast(
      mixer.GetResultColor(kColorOmniboxResultsBackground),
      kOmniboxOpacitySelected)};
}

#if BUILDFLAG(ENABLE_BRAVE_VPN)
void AddBraveVpnColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  if (key.custom_theme) {
    mixer[kColorBraveVpnButtonIconConnected] = {PickSimilarColorToToolbar(
        key, mixer, mixer.GetResultColor(nala::kColorPrimitiveGreen50),
        mixer.GetResultColor(nala::kColorPrimitiveGreen60))};
    mixer[kColorBraveVpnButtonIconDisconnected] = {PickSimilarColorToToolbar(
        key, mixer, mixer.GetResultColor(nala::kColorPrimitiveNeutral30),
        mixer.GetResultColor(nala::kColorPrimitiveNeutral80))};
    mixer[kColorBraveVpnButtonIconError] = {PickSimilarColorToToolbar(
        key, mixer, mixer.GetResultColor(nala::kColorPrimitiveRed60),
        mixer.GetResultColor(nala::kColorPrimitiveRed50))};
    mixer[kColorBraveVpnButtonErrorBackgroundNormal] = {
        PickSimilarColorToToolbar(
            key, mixer, mixer.GetResultColor(nala::kColorPrimitiveRed95),
            mixer.GetResultColor(nala::kColorPrimitiveRed15))};
  } else {
    mixer[kColorBraveVpnButtonIconConnected] = {
        nala::kColorSystemfeedbackSuccessIcon};
    mixer[kColorBraveVpnButtonIconDisconnected] = {nala::kColorIconDefault};
    mixer[kColorBraveVpnButtonIconError] = {
        nala::kColorSystemfeedbackErrorIcon};
    mixer[kColorBraveVpnButtonErrorBackgroundNormal] = {
        nala::kColorSystemfeedbackErrorBackground};
  }

  mixer[kColorBraveVpnButtonBackgroundNormal] = {kColorToolbar};

#if defined(TOOLKIT_VIEWS)
  if (!base::FeatureList::IsEnabled(
          darker_theme::features::kBraveDarkerTheme) ||
      key.scheme_variant != ui::ColorProviderKey::SchemeVariant::kDarker) {
    return;
  }

  auto& postprocessing_mixer = provider->AddPostprocessingMixer();
  postprocessing_mixer[kColorBraveVpnButtonBackgroundNormal] =
      darker_theme::ApplyDarknessFromColor(nala::kColorPrimitiveNeutral5);
#endif  // defined(TOOLKIT_VIEWS)
}
#endif

#if BUILDFLAG(ENABLE_SPEEDREADER)
void AddBraveSpeedreaderColorMixer(ui::ColorProvider* provider,
                                   const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  if (key.custom_theme) {
    auto mix_custom = [&mixer, &key](ui::ColorId light_id,
                                     ui::ColorId dark_id) -> SkColor {
      return PickSimilarColorToToolbar(key, mixer,
                                       mixer.GetResultColor(light_id),
                                       mixer.GetResultColor(dark_id));
    };

    mixer[kColorSpeedreaderIcon] = {mix_custom(nala::kColorPrimitivePrimary35,
                                               nala::kColorPrimitivePrimary80)};

    mixer[kColorSpeedreaderToolbarForeground] = {mix_custom(
        nala::kColorPrimitiveNeutral30, nala::kColorPrimitiveNeutral80)};

    mixer[kColorSpeedreaderToolbarBorder] = {mix_custom(
        nala::kColorPrimitiveNeutral90, nala::kColorPrimitiveNeutral25)};

    mixer[kColorSpeedreaderToolbarButtonHover] = {mix_custom(
        nala::kColorPrimitiveNeutral95, nala::kColorPrimitiveNeutral10)};
    mixer[kColorSpeedreaderToolbarButtonActive] = {mix_custom(
        nala::kColorPrimitiveNeutral90, nala::kColorPrimitiveNeutral5)};
    mixer[kColorSpeedreaderToolbarButtonActiveText] = {mix_custom(
        nala::kColorPrimitivePrimary35, nala::kColorPrimitivePrimary80)};
    mixer[kColorSpeedreaderToolbarButtonBorder] = {mix_custom(
        nala::kColorPrimitiveNeutral90, nala::kColorPrimitiveNeutral25)};
  } else {
    mixer[kColorSpeedreaderIcon] = {nala::kColorIconInteractive};
    mixer[kColorSpeedreaderToolbarForeground] = {nala::kColorIconDefault};
    mixer[kColorSpeedreaderToolbarBorder] = {
        nala::kColorDesktopbrowserToolbarButtonOutline};

    mixer[kColorSpeedreaderToolbarButtonHover] = {
        nala::kColorDesktopbrowserToolbarButtonHover};
    mixer[kColorSpeedreaderToolbarButtonActive] = {
        nala::kColorDesktopbrowserToolbarButtonActive};
    mixer[kColorSpeedreaderToolbarButtonActiveText] = {
        nala::kColorIconInteractive};
    mixer[kColorSpeedreaderToolbarButtonBorder] = {
        nala::kColorDesktopbrowserToolbarButtonOutline};
  }

  mixer[kColorSpeedreaderToolbarBackground] = {kColorToolbar};
}
#endif

void AddChromeLightThemeColorMixer(ui::ColorProvider* provider,
                                   const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorTabThrobber] = {SkColorSetRGB(0xd7, 0x55, 0x26)};
  mixer[kColorBookmarkBarForeground] = {kColorTabForegroundActiveFrameActive};
  mixer[ui::kColorBadgeBackground] = {SkColorSetRGB(95, 92, 241)};
  mixer[ui::kColorBadgeForeground] = {SkColorSetRGB(245, 244, 254)};
  mixer[kColorForTest] = {kLightColorForTest};
  mixer[kColorNewTabButtonBackgroundFrameActive] = {ui::kColorFrameActive};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {ui::kColorFrameInactive};
  mixer[kColorNewTabPageBackground] = {kBraveNewTabBackgroundLight};
  mixer[kColorTabStrokeFrameActive] = {ui::kColorRefNeutral80};
  mixer[kColorTabStrokeFrameInactive] = {kColorTabStrokeFrameActive};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, kBraveDisabledControlAlpha)};
  mixer[ui::kColorToggleButtonThumbOff] = {SK_ColorWHITE};
  mixer[ui::kColorToggleButtonThumbOn] = {SkColorSetRGB(0x4C, 0x54, 0xD2)};
  mixer[ui::kColorToggleButtonTrackOff] = {SkColorSetRGB(0xDA, 0xDC, 0xE8)};
  mixer[ui::kColorToggleButtonTrackOn] = {SkColorSetRGB(0xE1, 0xE2, 0xF6)};

  mixer[kColorTabCloseButtonFocusRingActive] = {
      ui::kColorFocusableBorderFocused};
  mixer[kColorTabCloseButtonFocusRingInactive] = {
      ui::kColorFocusableBorderFocused};
  mixer[kColorTabFocusRingActive] = {ui::kColorFocusableBorderFocused};
  mixer[kColorTabFocusRingInactive] = {ui::kColorFocusableBorderFocused};

  // Upstream uses tab's background color as omnibox chip background color.
  // In our light mode, there is no difference between location bar's bg
  // color and tab's bg color. So, it looks like chip's bg color is transparent.
  // Use frame color as chip background to have different bg color.
  mixer[kColorOmniboxChipBackground] = {ui::kColorFrameActive};
}

void AddChromeDarkThemeColorMixer(ui::ColorProvider* provider,
                                  const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorTabThrobber] = {SkColorSetRGB(0xd7, 0x55, 0x26)};
  mixer[kColorBookmarkBarForeground] = {kColorTabForegroundActiveFrameActive};
  mixer[ui::kColorBadgeBackground] = {SkColorSetRGB(135, 132, 244)};
  mixer[ui::kColorBadgeForeground] = {SkColorSetRGB(14, 14, 52)};
  mixer[kColorForTest] = {kDarkColorForTest};
  mixer[kColorNewTabButtonBackgroundFrameActive] = {ui::kColorFrameActive};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {ui::kColorFrameInactive};
  mixer[kColorNewTabPageBackground] = {kBraveNewTabBackgroundDark};
  mixer[kColorTabStrokeFrameActive] = {ui::kColorRefNeutral25};
  mixer[kColorTabStrokeFrameInactive] = {kColorToolbar};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, kBraveDisabledControlAlpha)};
  mixer[kColorToolbarTopSeparatorFrameActive] = {kColorToolbar};
  mixer[kColorToolbarTopSeparatorFrameInactive] = {kColorToolbar};
  mixer[ui::kColorToggleButtonThumbOff] = {SK_ColorWHITE};
  mixer[ui::kColorToggleButtonThumbOn] = {SkColorSetRGB(0x44, 0x36, 0xE1)};
  mixer[ui::kColorToggleButtonTrackOff] = {SkColorSetRGB(0x5E, 0x61, 0x75)};
  mixer[ui::kColorToggleButtonTrackOn] = {SkColorSetRGB(0x76, 0x79, 0xB1)};

  mixer[kColorTabCloseButtonFocusRingActive] = {
      ui::kColorFocusableBorderFocused};
  mixer[kColorTabCloseButtonFocusRingInactive] = {
      ui::kColorFocusableBorderFocused};
  mixer[kColorTabFocusRingActive] = {ui::kColorFocusableBorderFocused};
  mixer[kColorTabFocusRingInactive] = {ui::kColorFocusableBorderFocused};
}

void AddChromeColorMixerForAllThemes(ui::ColorProvider* provider,
                                     const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  // Use same ink drop effect for all themes including custome themes.
  // Toolbar button's inkdrop highlight/visible colors depends on toolbar color.
  auto get_toolbar_ink_drop_color = [](float dark_opacity, float light_opacity,
                                       SkColor input,
                                       const ui::ColorMixer& mixer) {
    const float highlight_opacity =
        color_utils::IsDark(mixer.GetResultColor(kColorToolbar))
            ? dark_opacity
            : light_opacity;
    return SkColorSetA(SK_ColorBLACK, 0xFF * highlight_opacity);
  };
  mixer[kColorToolbarInkDropHover] = {
      base::BindRepeating(get_toolbar_ink_drop_color, 0.25f, 0.05f)};
  mixer[kColorToolbarInkDropRipple] = {
      base::BindRepeating(get_toolbar_ink_drop_color, 0.4f, 0.1f)};

  mixer[ui::kColorFrameActive] = {ui::kColorSysHeader};
  // TODO(simonhong): Should we adjust frame color for inactive window?
  mixer[ui::kColorFrameInactive] = {ui::kColorFrameActive};

  if (key.custom_theme) {
    return;
  }

  // We don't show border when omnibox doesn't have focus but still
  // contains in-progress user input.
  mixer[kColorLocationBarBorderOnMismatch] = {SK_ColorTRANSPARENT};
}

void AddBraveColorMixerForAllThemes(ui::ColorProvider* provider,
                                    const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  // Custom theme will use this color. Other themes could have another
  // color by their color mixers.
  mixer[kColorToolbarButtonActivated] = {SkColorSetRGB(0x7C, 0x91, 0xFF)};
  mixer[kColorSidebarButtonPressed] = {kColorToolbarButtonActivated};

  mixer[kColorLocationBarFocusRing] = {nala::kColorPrimitivePrimary50};

  // Search conversion button in omnibox.
  mixer[kColorSearchConversionButtonText] = {nala::kColorPrimary60};
  mixer[kColorSearchConversionButtonBorder] = {nala::kColorPrimary20};
  mixer[kColorSearchConversionButtonBackground] = {nala::kColorPrimary10};
  mixer[kColorSearchConversionButtonBackgroundHovered] = {
      nala::kColorPrimary20};
  mixer[kColorSearchConversionButtonText] = {nala::kColorPrimary60};
  mixer[kColorSearchConversionButtonCloseButton] = {
      ui::AlphaBlend({nala::kColorIconInteractive},
                     {kColorSearchConversionButtonBackground}, 0.5 * 0xff)};
  mixer[kColorSearchConversionButtonCloseButtonHovered] = {
      ui::AlphaBlend({nala::kColorIconInteractive},
                     {kColorSearchConversionButtonBackground}, 0.7 * 0xff)};
  mixer[kColorSearchConversionButtonCaratRight] = {
      kColorSearchConversionButtonCloseButton};
  mixer[kColorSearchConversionButtonShadow1] = {
      SkColorSetA(SK_ColorBLACK, 0.05 * 255)};
  mixer[kColorSearchConversionButtonShadow2] = {
      SkColorSetA(SK_ColorBLACK, 0.1 * 255)};
}

}  // namespace

void AddBravifiedChromeThemeColorMixer(ui::ColorProvider* provider,
                                       const ui::ColorProviderKey& key) {
  AddChromeColorMixerForAllThemes(provider, key);

  AddMaterialChromeColorMixer(provider, key);
  AddMaterialOmniboxColorMixer(provider, key);
  AddMaterialTabStripColorMixer(provider, key);
  AddMaterialSidePanelColorMixer(provider, key);

  // TODO(simonhong): Use leo color when it's ready.
  // TODO(simonhong): Move these overriding to
  // AddChromeColorMixerForAllThemes().
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorAppMenuHighlightSeverityLow] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x00, 0x46, 0x07),
                                    SkColorSetRGB(0x58, 0xE1, 0x55))};
  mixer[kColorAppMenuHighlightSeverityMedium] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x4A, 0x39, 0x00),
                                    SkColorSetRGB(0xF1, 0xC0, 0x0F))};
  mixer[kColorAppMenuHighlightSeverityHigh] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x7D, 0x00, 0x1A),
                                    SkColorSetRGB(0xFF, 0xB3, 0xB2))};

  if (key.custom_theme) {
    return;
  }

  mixer[kColorToolbar] = {ui::kColorSysBase};
  mixer[kColorDownloadToolbarButtonActive] = {nala::kColorIconInteractive};
  mixer[kColorDownloadToolbarButtonRingBackground] = {SkColorSetA(
      mixer.GetResultColor(kColorDownloadToolbarButtonActive), 0.3 * 255)};
  mixer[kColorToolbarActionItemEngaged] = {nala::kColorIconInteractive};

  key.color_mode == ui::ColorProviderKey::ColorMode::kDark
      ? AddChromeDarkThemeColorMixer(provider, key)
      : AddChromeLightThemeColorMixer(provider, key);
}

void AddBraveLightThemeColorMixer(ui::ColorProvider* provider,
                                  const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorForTest] = {kLightColorForTest};

  mixer[kColorBookmarkBarInstructionsText] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x49, 0x50, 0x57),
                                    SkColorSetRGB(0xFF, 0xFF, 0xFF))};
  mixer[kColorMenuItemSubText] = {SkColorSetRGB(0x86, 0x8E, 0x96)};
  // It's "Themeable/Blue/10" but leo/color.h doesn't have it.
  mixer[kColorSearchConversionBannerTypeBackground] = {
      SkColorSetRGB(0xEA, 0xF1, 0xFF)};
  mixer[kColorSearchConversionBannerTypeDescText] = {
      SkColorSetRGB(0x2E, 0x30, 0x39)};
  mixer[kColorSearchConversionBannerTypeBackgroundBorder] = {
      SkColorSetRGB(0xE2, 0xE3, 0xF8)};
  mixer[kColorSearchConversionBannerTypeBackgroundBorderHovered] = {
      SkColorSetRGB(0x83, 0x89, 0xE0)};
  mixer[kColorSearchConversionBannerTypeBackgroundGradientFrom] = {
      SkColorSetARGB(104, 0xFF, 0xFF, 0xFF)};
  mixer[kColorSearchConversionBannerTypeBackgroundGradientTo] = {
      SkColorSetRGB(0xEF, 0xEF, 0xFB)};

  mixer[kColorDialogDontAskAgainButton] = {SkColorSetRGB(0x86, 0x8E, 0x96)};
  mixer[kColorDialogDontAskAgainButtonHovered] = {
      SkColorSetRGB(0x49, 0x50, 0x57)};
  mixer[kColorSidebarAddBubbleHeaderText] = {SkColorSetRGB(0x17, 0x17, 0x1F)};
  mixer[kColorSidebarAddBubbleItemTextBackgroundHovered] = {
      SkColorSetRGB(0x4C, 0x54, 0xD2)};
  mixer[kColorSidebarAddBubbleItemTextHovered] = {
      SkColorSetRGB(0xF0, 0xF2, 0xFF)};
  mixer[kColorSidebarAddBubbleItemTextNormal] = {
      SkColorSetRGB(0x21, 0x25, 0x29)};
  mixer[kColorSidebarArrowBackgroundHovered] = {kColorToolbarInkDropHover};
  mixer[kColorSidebarSeparator] = {SkColorSetRGB(0xE6, 0xE8, 0xF5)};

  mixer[kColorSidebarButtonBase] = {kColorToolbarButtonIcon};
  if (!HasCustomToolbarColor(key)) {
    mixer[kColorSidebarButtonPressed] = {kColorToolbarButtonActivated};
  }

  mixer[kColorSidebarArrowDisabled] = {PickColorContrastingToToolbar(
      key, mixer, SkColorSetARGB(0x8A, 0x49, 0x50, 0x57),
      SkColorSetARGB(0x8A, 0xAE, 0xB1, 0xC2))};
  mixer[kColorSidebarArrowNormal] = {kColorSidebarButtonBase};
  mixer[kColorSidebarItemDragIndicator] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x21, 0x25, 0x29),
                                    SkColorSetRGB(0xC2, 0xC4, 0xCF))};
  mixer[kColorWebDiscoveryInfoBarBackground] = {
      SkColorSetRGB(0xFF, 0xFF, 0xFF)};
  mixer[kColorWebDiscoveryInfoBarMessage] = {SkColorSetRGB(0x1D, 0x1F, 0x25)};
  mixer[kColorWebDiscoveryInfoBarLink] = {SkColorSetRGB(0x4C, 0x54, 0xD2)};
  mixer[kColorWebDiscoveryInfoBarNoThanks] = {SkColorSetRGB(0x6B, 0x70, 0x84)};
  mixer[kColorWebDiscoveryInfoBarClose] = {SkColorSetRGB(0x6B, 0x70, 0x84)};

  // Color for download button when all completed and button needs user
  // interaction.
  mixer[kColorBraveDownloadToolbarButtonActive] = {
      SkColorSetRGB(0x5F, 0x5C, 0xF1)};

  mixer[kColorLocationBarHoveredShadow] = {
      SkColorSetA(SK_ColorBLACK, 0.07 * 255)};

  // Colors for HelpBubble. IDs are defined in
  // chrome/browser/ui/color/chrome_color_id.h
  mixer[kColorFeaturePromoBubbleBackground] = {SK_ColorWHITE};
  mixer[kColorFeaturePromoBubbleForeground] = {SkColorSetRGB(0x42, 0x45, 0x52)};
  mixer[kColorFeaturePromoBubbleCloseButtonInkDrop] = {
      kColorToolbarInkDropHover};

  mixer[kColorTabGroupBackgroundAlpha] = {
      SkColorSetA(SK_ColorBLACK, 0.15 * 255)};

  mixer[ui::kColorMenuButtonBackground] = {SK_ColorTRANSPARENT};
}

void AddBraveDarkThemeColorMixer(ui::ColorProvider* provider,
                                 const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorForTest] = {kDarkColorForTest};

  mixer[kColorBookmarkBarInstructionsText] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x49, 0x50, 0x57),
                                    SkColorSetRGB(0xFF, 0xFF, 0xFF))};
  mixer[kColorMenuItemSubText] = {SkColorSetRGB(0x84, 0x88, 0x9C)};
  mixer[kColorSearchConversionBannerTypeDescText] = {
      SkColorSetRGB(0xE2, 0xE3, 0xE7)};
  mixer[kColorSearchConversionBannerTypeBackgroundBorder] = {
      SkColorSetRGB(0x1F, 0x25, 0x7A)};
  mixer[kColorSearchConversionBannerTypeBackgroundBorderHovered] = {
      SkColorSetRGB(0x5F, 0x67, 0xD7)};
  mixer[kColorSearchConversionBannerTypeBackgroundGradientFrom] = {
      SkColorSetARGB(104, 0x17, 0x19, 0x1E)};
  mixer[kColorSearchConversionBannerTypeBackgroundGradientTo] = {
      SkColorSetARGB(104, 0x1F, 0x25, 0x7A)};
  mixer[kColorDialogDontAskAgainButton] = {SkColorSetRGB(0x84, 0x88, 0x9C)};
  mixer[kColorDialogDontAskAgainButtonHovered] = {
      SkColorSetRGB(0xC2, 0xC4, 0xCF)};
  mixer[kColorSidebarAddBubbleHeaderText] = {SkColorSetRGB(0xF0, 0xF0, 0xFF)};
  mixer[kColorSidebarAddBubbleItemTextBackgroundHovered] = {
      SkColorSetRGB(0x4C, 0x54, 0xD2)};
  mixer[kColorSidebarAddBubbleItemTextHovered] = {
      SkColorSetRGB(0xF0, 0xF0, 0xFF)};
  mixer[kColorSidebarAddBubbleItemTextNormal] = {
      SkColorSetRGB(0xF0, 0xF0, 0xFF)};
  mixer[kColorSidebarArrowBackgroundHovered] = {kColorToolbarInkDropHover};
  mixer[kColorSidebarSeparator] = {SkColorSetRGB(0x5E, 0x61, 0x75)};

  mixer[kColorSidebarButtonBase] = {kColorToolbarButtonIcon};
  if (!HasCustomToolbarColor(key)) {
    mixer[kColorSidebarButtonPressed] = {kColorToolbarButtonActivated};
  }
  mixer[kColorSidebarArrowDisabled] = {PickColorContrastingToToolbar(
      key, mixer, SkColorSetARGB(0x8A, 0x49, 0x50, 0x57),
      SkColorSetARGB(0x8A, 0xAE, 0xB1, 0xC2))};
  mixer[kColorSidebarArrowNormal] = {kColorSidebarButtonBase};
  mixer[kColorSidebarItemDragIndicator] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x21, 0x25, 0x29),
                                    SkColorSetRGB(0xC2, 0xC4, 0xCF))};
  mixer[kColorWebDiscoveryInfoBarBackground] = {
      SkColorSetRGB(0x1A, 0x1C, 0x22)};
  mixer[kColorWebDiscoveryInfoBarMessage] = {SkColorSetRGB(0xFF, 0xFF, 0xFF)};
  mixer[kColorWebDiscoveryInfoBarLink] = {SkColorSetRGB(0xA6, 0xAB, 0xEC)};
  mixer[kColorWebDiscoveryInfoBarNoThanks] = {
      SkColorSetARGB(0xBF, 0xEC, 0xEF, 0xF2)};
  mixer[kColorWebDiscoveryInfoBarClose] = {
      SkColorSetARGB(0xBF, 0x8C, 0x90, 0xA1)};

  mixer[kColorBraveDownloadToolbarButtonActive] = {
      SkColorSetRGB(0x87, 0x84, 0xF4)};

  mixer[kColorLocationBarHoveredShadow] = {
      SkColorSetA(SK_ColorBLACK, 0.4 * 255)};

  // Colors for HelpBubble. IDs are defined in
  // chrome/browser/ui/color/chrome_color_id.h
  mixer[kColorFeaturePromoBubbleBackground] = {SkColorSetRGB(0x12, 0x13, 0x16)};
  mixer[kColorFeaturePromoBubbleForeground] = {SkColorSetRGB(0xC6, 0xC8, 0xD0)};
  mixer[kColorFeaturePromoBubbleCloseButtonInkDrop] = {
      kColorToolbarInkDropHover};

  mixer[kColorTabGroupBackgroundAlpha] = {
      SkColorSetA(SK_ColorBLACK, 0.25 * 255)};

  mixer[ui::kColorMenuButtonBackground] = {SK_ColorTRANSPARENT};

#if defined(TOOLKIT_VIEWS)
  if (!base::FeatureList::IsEnabled(
          darker_theme::features::kBraveDarkerTheme) ||
      key.scheme_variant != ui::ColorProviderKey::SchemeVariant::kDarker) {
    return;
  }

  auto& postprocessing_mixer = provider->AddPostprocessingMixer();
  postprocessing_mixer[kColorForTest] = {kDarkerColorForTest};
  postprocessing_mixer[ui::kColorFrameActive] =
      darker_theme::ApplyDarknessFromColor(nala::kColorPrimitiveNeutral0);
  postprocessing_mixer[ui::kColorFrameInactive] = {
      postprocessing_mixer.GetResultColor(ui::kColorFrameActive)};
  postprocessing_mixer[kColorToolbar] =
      darker_theme::ApplyDarknessFromColor(nala::kColorPrimitiveNeutral5);

  // Override saved tab group button / bookmark folder icon color in darker
  // theme. These colors are originally defined in material_chrome_color_mixer,
  // But we override them here because we want to override these only when it's
  // darker theme mode except Private/Tor profile. Also we want to make it sure
  // they are overridden regardless of order of adding mixers, so we use
  // postprocessing mixer.
  postprocessing_mixer[kColorBookmarkFolderIcon] = {
      nala::kColorPrimitiveNeutral40};
  postprocessing_mixer[kColorBookmarkButtonIcon] = {
      nala::kColorPrimitiveNeutral40};

  // Override menu separator color in darker theme from application side.
  postprocessing_mixer[ui::kColorMenuSeparator] = {
      nala::kColorPrimitiveNeutral15};
  postprocessing_mixer[kColorBraveContentsOutline] = {
      nala::kColorPrimitiveNeutral15};
#endif  // defined(TOOLKIT_VIEWS)
}

namespace {

// Wryling: tinted chrome. One "space colour" fills the frame, the
// vertical sidebar and the toolbar, so the web page sits as a card inside a
// coloured surround; tabs are translucent white on top of it. Added as a
// postprocessing mixer so it wins over the Brave and Chromium theme mixers.
void AddWrylingTintColorMixer(ui::ColorProvider* provider,
                              const ui::ColorProviderKey& key) {
  const bool dark = key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  // The user's chosen space colour (or the default violet).
  const SkColor tint = wryling::GetSpaceTint(key);
  const auto over_tint = [tint](float white_alpha) {
    return color_utils::AlphaBlend(SK_ColorWHITE, tint, white_alpha);
  };

  auto& mixer = provider->AddPostprocessingMixer();
  // The frame colour also tints the glass layer in glass mode.
  mixer[ui::kColorFrameActive] = {tint};
  mixer[ui::kColorFrameInactive] = {tint};
  // Toolbar button hover/press: translucent white like the tabs, not the
  // stock opaque grey square.
  mixer[kColorToolbarInkDropHover] = {
      SkColorSetA(SK_ColorWHITE, 0.12f * SK_AlphaOPAQUE)};
  mixer[kColorToolbarInkDropRipple] = {
      SkColorSetA(SK_ColorWHITE, 0.18f * SK_AlphaOPAQUE)};
  // Same for icons in and beside the address bar (e.g. the Shields button),
  // whose stock colours are opaque greys meant for a solid omnibox.
  mixer[kColorOmniboxIconHover] = {
      SkColorSetA(SK_ColorWHITE, 0.12f * SK_AlphaOPAQUE)};
  mixer[kColorOmniboxIconPressed] = {
      SkColorSetA(SK_ColorWHITE, 0.18f * SK_AlphaOPAQUE)};

  // Glass mode: the window has Liquid Glass behind its chrome, so the toolbar
  // surfaces are see-through. They keep the tint's RGB so code that picks text
  // colours from them still sees a dark (or light) surface.
  const bool glass =
      key.frame_style == ui::ColorProviderKey::FrameStyle::kGlass;
  const SkColor toolbar = glass ? SkColorSetA(tint, SK_AlphaTRANSPARENT) : tint;
  mixer[kColorToolbar] = {toolbar};
  // Colours that Brave and Chromium define as "the toolbar colour" were
  // resolved by earlier mixers, so they would keep Brave's grey (e.g. a grey
  // box behind the VPN button, a grey info bar). Point them at Wryling's.
  for (const ui::ColorId id : {kColorToolbarTopSeparatorFrameActive,
                               kColorToolbarTopSeparatorFrameInactive}) {
    mixer[id] = {toolbar};
  }
  // The address pill's leading icon sits straight on the pill; a solid
  // toolbar colour here drew a dark disc behind it in tinted windows.
  mixer[kColorOmniboxIconBackground] = {SK_ColorTRANSPARENT};
  // The info bar and bookmarks bar sit over the window's solid background,
  // not the glass, and paint on opaque layers (see-through shows as black),
  // so they take the solid space colour.
  mixer[kColorInfoBarBackground] = {tint};
  mixer[kColorBookmarkBarBackground] = {tint};
#if BUILDFLAG(ENABLE_BRAVE_VPN)
  mixer[kColorBraveVpnButtonBackgroundNormal] = {toolbar};
#endif
#if BUILDFLAG(ENABLE_SPEEDREADER)
  mixer[kColorSpeedreaderToolbarBackground] = {toolbar};
#endif

  // A shade of the space colour at a given lightness: the base for text,
  // accents and solid surfaces below, so they all follow the picked colour
  // and its strength.
  const auto surface = [tint](double lightness, float alpha = 1.0f) {
    color_utils::HSL hsl;
    color_utils::SkColorToHSL(tint, &hsl);
    hsl.l = lightness;
    return color_utils::HSLToSkColor(hsl, alpha * SK_AlphaOPAQUE);
  };

  // Tab titles and the new-tab row: light text in dark mode, deep text in
  // light mode (stock greys are unreadable on a light tint).
  const SkColor text_strong = dark ? surface(0.96) : surface(0.14);
  const SkColor text_soft = dark ? surface(0.86, 0.92f) : surface(0.11);
  mixer[kColorTabForegroundActiveFrameActive] = {text_strong};
  mixer[kColorTabForegroundActiveFrameInactive] = {text_soft};
  mixer[kColorTabForegroundInactiveFrameActive] = {text_soft};
  mixer[kColorTabForegroundInactiveFrameInactive] = {
      dark ? surface(0.78, 0.8f) : surface(0.24, 0.82f)};
  mixer[kColorBraveVerticalTabNTBTextColor] = {text_soft};
  mixer[kColorBraveVerticalTabNTBIconColor] = {text_soft};
  mixer[kColorBraveVerticalTabNTBShortcutTextColor] = {text_soft};

  // Wryling tokens (wryling_tokens.h): accents and
  // solid surfaces come from these, never from stock greys or Brave blue.
  const wryling::Tokens tokens = wryling::GetTokens(key);
  const auto accent_shade = [&tokens](double delta) {
    color_utils::HSL hsl;
    color_utils::SkColorToHSL(tokens.accent, &hsl);
    hsl.l = std::clamp(hsl.l + delta, 0.0, 1.0);
    return color_utils::HSLToSkColor(hsl, SK_AlphaOPAQUE);
  };
  // Lighter in dark mode, deeper in light mode: the hover/pressed step.
  const SkColor accent_step = accent_shade(dark ? 0.06 : -0.06);

  // Native accents: default buttons, links.
  mixer[ui::kColorButtonBackgroundProminent] = {tokens.accent};
  mixer[ui::kColorButtonBackgroundProminentFocused] = {accent_step};
  mixer[ui::kColorButtonForegroundProminent] = {tokens.on_accent};
  mixer[ui::kColorButtonForeground] = {tokens.text};
  mixer[ui::kColorButtonForegroundTonal] = {tokens.text};
  mixer[ui::kColorButtonBorder] = {tokens.stroke};
  // Secondary (filled) and plain buttons, see Brave's MdTextButton styles.
  mixer[ui::kColorButtonBackground] = {tokens.fill};
  mixer[ui::kColorButtonBackgroundPressed] = {tokens.pressed};
  mixer[ui::kColorButtonHoverBackgroundText] = {tokens.hover};
  // Disabled controls fade toward the text colour instead of Brave's greys.
  mixer[nala::kColorButtonDisabled] = {tokens.fill};
  mixer[nala::kColorTextDisabled] = {tokens.text_tertiary};
  // Dropdowns (comboboxes) are filled fields without an outline.
  mixer[ui::kColorComboboxBackground] = {tokens.fill};
  mixer[ui::kColorComboboxBackgroundDisabled] = {tokens.hover};
  mixer[ui::kColorComboboxContainerOutline] = {SK_ColorTRANSPARENT};
  mixer[ui::kColorComboboxInkDropHovered] = {tokens.hover};
  mixer[ui::kColorComboboxInkDropRipple] = {tokens.pressed};
  mixer[ui::kColorLinkForeground] = {tokens.accent_text};
  mixer[ui::kColorLinkForegroundPressed] = {tokens.accent_text};
  // Brave draws its buttons from its own (nala) palette; retint the accent
  // entries those buttons use: filled, outline and plain kinds, and hover.
  mixer[nala::kColorButtonBackground] = {tokens.accent};
  mixer[nala::kColorTextInteractive] = {tokens.accent_text};
  mixer[nala::kColorDividerInteractive] = {tokens.stroke};
  mixer[nala::kColorPrimary10] = {tokens.selected};
  mixer[nala::kColorPrimary30] = {SkColorSetA(tokens.accent, 0x80)};
  mixer[nala::kColorPrimary50] = {accent_step};
  mixer[nala::kColorPrimary60] = {accent_step};

  // The address bar dropdown: a popover (glass, or "raised" without it) whose
  // rows highlight like menu rows, solid accent when selected.
  mixer[kColorOmniboxResultsBackground] = {tokens.raised};
  mixer[kColorOmniboxResultsBackgroundHovered] = {tokens.hover};
  mixer[kColorOmniboxResultsBackgroundSelected] = {tokens.accent};
  mixer[kColorOmniboxText] = {tokens.text};
  mixer[kColorOmniboxResultsTextDimmed] = {tokens.text_secondary};
  mixer[kColorOmniboxResultsUrl] = {tokens.accent_text};
  mixer[kColorOmniboxResultsTextSelected] = {tokens.on_accent};
  mixer[kColorOmniboxResultsTextDimmedSelected] = {
      SkColorSetA(tokens.on_accent, 0xC0)};
  mixer[kColorOmniboxResultsIconSelected] = {tokens.on_accent};
  mixer[kColorOmniboxResultsUrlSelected] = {tokens.on_accent};
  mixer[kColorOmniboxResultsFocusIndicator] = {tokens.accent_text};
  mixer[kColorOmniboxResultsButtonBorder] = {tokens.stroke};

  // Bubbles and dialogs (page info, permissions, translate, downloads, ...):
  // the "raised" depth with a hairline stroke, footers a step deeper.
  mixer[ui::kColorBubbleBackground] = {tokens.raised};
  mixer[ui::kColorDialogBackground] = {tokens.raised};
  // Translucent, so it reads as a band on glass and on the solid fallback.
  mixer[ui::kColorBubbleFooterBackground] = {tokens.hover};
  mixer[ui::kColorBubbleBorder] = {tokens.stroke};
  mixer[ui::kColorBubbleFooterBorder] = {tokens.separator};

  // Toasts ("Link copied", ...): the same raised surface as bubbles, so they
  // pick up the glass too, with the accent for their action button.
  mixer[ui::kColorToastBackgroundProminent] = {tokens.raised};
  mixer[ui::kColorToastForeground] = {tokens.text};
  mixer[ui::kColorToastButton] = {tokens.accent_text};

  if (glass) {
    // Overlays: the sidebar's chrome is white (dark mode) or black
    // (light mode) at fixed opacities laid over the window's background, so
    // every surface picks up its colour, gradient and grain instead of being
    // a separately computed solid. The current tab is the exception in light
    // mode: a near-white card (with a soft shadow, see the vertical tab
    // style).
    // White or black by how light the background actually is, not by the
    // light/dark setting: a deep custom colour in light mode still needs
    // white text.
    const bool dark_surface = color_utils::IsDark(tint);
    const auto ink = [dark_surface](float alpha) {
      return SkColorSetA(dark_surface ? SK_ColorWHITE : SK_ColorBLACK,
                         alpha * SK_AlphaOPAQUE);
    };
    const auto white = [](float alpha) {
      return SkColorSetA(SK_ColorWHITE, alpha * SK_AlphaOPAQUE);
    };
    mixer[kColorBraveVerticalTabInactiveBackground] = {
        SkColorSetA(tint, SK_AlphaTRANSPARENT)};
    mixer[kColorBraveVerticalTabActiveBackground] = {
        dark_surface ? white(0.18f) : white(0.85f)};
    mixer[kColorBraveVerticalTabHoveredBackground] = {ink(dark_surface ? 0.10f : 0.06f)};
    mixer[kColorBraveVerticalTabSeparator] = {ink(dark_surface ? 0.12f : 0.10f)};
    // A split is one card in the sidebar, its tabs split by hairlines.
    mixer[kColorBraveSplitViewTileBackgroundVertical] = {
        ink(dark_surface ? 0.07f : 0.05f)};
    mixer[kColorBraveSplitViewTileBackgroundBorder] = {SK_ColorTRANSPARENT};
    mixer[kColorBraveSplitViewTileDivider] = {ink(dark_surface ? 0.16f : 0.14f)};

    // Text: every tab readable (inactive tabs aren't dimmed), the new-tab row
    // and secondary text quieter.
    mixer[kColorTabForegroundActiveFrameActive] = {ink(dark_surface ? 0.95f : 0.88f)};
    mixer[kColorTabForegroundInactiveFrameActive] = {ink(dark_surface ? 0.85f : 0.80f)};
    mixer[kColorTabForegroundActiveFrameInactive] = {ink(dark_surface ? 0.70f : 0.65f)};
    mixer[kColorTabForegroundInactiveFrameInactive] = {ink(dark_surface ? 0.55f : 0.50f)};
    mixer[kColorBraveVerticalTabNTBTextColor] = {ink(0.55f)};
    mixer[kColorBraveVerticalTabNTBIconColor] = {ink(0.55f)};
    mixer[kColorBraveVerticalTabNTBShortcutTextColor] = {ink(0.40f)};

    // The address bar is a slightly deeper (or lighter) patch of the
    // background, not a slab of its own colour.
    mixer[kColorLocationBarBackground] = {ink(dark_surface ? 0.09f : 0.05f)};
    mixer[kColorLocationBarBackgroundHovered] = {ink(dark_surface ? 0.13f : 0.08f)};
    mixer[kColorOmniboxIconHover] = {ink(dark_surface ? 0.12f : 0.07f)};
    mixer[kColorOmniboxIconPressed] = {ink(dark_surface ? 0.18f : 0.11f)};
    const SkColor icon = ink(dark_surface ? 0.62f : 0.55f);
    mixer[kColorOmniboxResultsIcon] = {icon};
    mixer[kColorOmniboxSecurityChipDefault] = {icon};
    mixer[kColorOmniboxTextDimmed] = {ink(dark_surface ? 0.55f : 0.50f)};
    return;
  }

  mixer[kColorBraveVerticalTabActiveBackground] = {over_tint(0.22f)};
  mixer[kColorBraveVerticalTabHoveredBackground] = {over_tint(0.12f)};
  // Inactive tabs have no box of their own, just text on the tint.
  mixer[kColorBraveVerticalTabInactiveBackground] = {tint};
  mixer[kColorBraveVerticalTabSeparator] = {over_tint(0.16f)};
  mixer[kColorBraveSplitViewTileBackgroundVertical] = {over_tint(0.08f)};
  mixer[kColorBraveSplitViewTileBackgroundBorder] = {SK_ColorTRANSPARENT};
  mixer[kColorBraveSplitViewTileDivider] = {over_tint(0.18f)};
  // Address bar as a soft translucent pill rather than a dark slab.
  mixer[kColorLocationBarBackground] = {over_tint(0.10f)};
  mixer[kColorLocationBarBackgroundHovered] = {over_tint(0.15f)};
}

}  // namespace

// Handling dark or light theme on normal profile.
void AddBraveThemeColorMixer(ui::ColorProvider* provider,
                             const ui::ColorProviderKey& key) {
  AddBraveColorMixerForAllThemes(provider, key);

  key.color_mode == ui::ColorProviderKey::ColorMode::kDark
      ? AddBraveDarkThemeColorMixer(provider, key)
      : AddBraveLightThemeColorMixer(provider, key);
#if BUILDFLAG(ENABLE_BRAVE_VPN)
  AddBraveVpnColorMixer(provider, key);
#endif
#if BUILDFLAG(ENABLE_SPEEDREADER)
  AddBraveSpeedreaderColorMixer(provider, key);
#endif

  auto& mixer = provider->AddMixer();
  mixer[kColorIconBase] = {nala::kColorIconDefault};
  mixer[kColorBookmarkBarInstructionsLink] = {nala::kColorTextInteractive};
  mixer[kColorSearchConversionBannerTypeBackground] = {nala::kColorBlue10};
  mixer[kColorSearchConversionCloseButton] = {nala::kColorIconDefault};
  mixer[kColorSidebarPanelHeaderTitle] = {nala::kColorTextPrimary};
  mixer[kColorSidebarPanelHeaderButton] = {nala::kColorIconDefault};
  mixer[kColorSidebarPanelHeaderButtonHovered] = {nala::kColorNeutral60};
  mixer[kColorSidebarAddBubbleBackground] = {nala::kColorContainerBackground};

  if (!HasCustomToolbarColor(key)) {
    mixer[kColorToolbarButtonActivated] = {nala::kColorIconInteractive};
  }

#if BUILDFLAG(ENABLE_BRAVE_WAYBACK_MACHINE)
  mixer[kColorWaybackMachineURLLoaded] = {
      nala::kColorSystemfeedbackSuccessIcon};
  mixer[kColorWaybackMachineURLNotAvailable] = {
      nala::kColorSystemfeedbackErrorIcon};
#endif

  mixer[kColorBraveExtensionMenuIcon] = {nala::kColorIconInteractive};
  mixer[kColorBraveContentsOutline] = {
      nala::kColorDesktopbrowserToolbarButtonOutline};

#if BUILDFLAG(ENABLE_PLAYLIST_WEBUI)
  if (base::FeatureList::IsEnabled(playlist::features::kPlaylist)) {
    playlist::AddThemeColorMixer(provider, key);
  }
#endif

  AddWrylingTintColorMixer(provider, key);
  AddWrylingMenuColorMixer(provider, key);
}

void AddBravePrivateThemeColorMixer(ui::ColorProvider* provider,
                                    const ui::ColorProviderKey& key) {
  AddBraveDarkThemeColorMixer(provider, key);

  // Add private theme specific brave colors here.
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorForTest] = {kPrivateColorForTest};

  mixer[kColorToolbarButtonActivated] = {nala::kColorPrimitivePrivateWindow80};
  mixer[kColorSidebarButtonPressed] = {kColorToolbarButtonActivated};

  mixer[kColorSidebarPanelHeaderTitle] = {nala::kColorPrimitiveNeutral90};
  mixer[kColorSidebarPanelHeaderButton] = {nala::kColorPrimitiveNeutral90};
  mixer[kColorSidebarPanelHeaderButtonHovered] = {
      nala::kColorPrimitiveNeutral80};
}

void AddBraveTorThemeColorMixer(ui::ColorProvider* provider,
                                const ui::ColorProviderKey& key) {
  AddBravePrivateThemeColorMixer(provider, key);
  AddChromeDarkThemeColorMixer(provider, key);

  // Add tor theme specific brave colors here.
}

void AddPrivateThemeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
  AddBravePrivateThemeColorMixer(provider, key);
  AddChromeDarkThemeColorMixer(provider, key);

  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorBookmarkBarForeground] = {nala::kColorPrimitivePrivateWindow100};
  mixer[kColorLocationBarFocusRing] = {nala::kColorPrimitivePrivateWindow80};
  mixer[kColorNewTabButtonBackgroundFrameActive] = {ui::kColorFrameActive};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {ui::kColorFrameInactive};
  mixer[kColorNewTabPageBackground] = {kPrivateFrame};
  mixer[kColorTabBackgroundActiveFrameActive] = {
      nala::kColorPrimitivePrivateWindow20};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorTabBackgroundInactiveFrameActive] = {ui::kColorFrameActive};
  mixer[kColorTabBackgroundInactiveFrameInactive] = {ui::kColorFrameInactive};

  mixer[kColorTabForegroundActiveFrameActive] = {
      nala::kColorPrimitivePrivateWindow95};
  mixer[kColorTabForegroundActiveFrameInactive] = {
      nala::kColorPrimitivePrivateWindow80};
  mixer[kColorTabForegroundInactiveFrameActive] = {
      nala::kColorPrimitivePrivateWindow80};
  mixer[kColorToolbar] = {nala::kColorPrimitivePrivateWindow20};
  mixer[kColorToolbarButtonIcon] = {nala::kColorPrimitivePrivateWindow70};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, kBraveDisabledControlAlpha)};
  mixer[ui::kColorFrameActive] = {kPrivateFrame};
  mixer[ui::kColorFrameInactive] = {kPrivateFrame};
}

void AddTorThemeColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderKey& key) {
  AddBraveTorThemeColorMixer(provider, key);

  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorLocationBarFocusRing] = {nala::kColorPrimitiveTorWindow80};
  mixer[kColorNewTabButtonBackgroundFrameActive] = {ui::kColorFrameActive};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {ui::kColorFrameInactive};
  mixer[kColorNewTabPageBackground] = {kPrivateTorFrame};
  mixer[kColorTabBackgroundActiveFrameActive] = {
      nala::kColorPrimitiveTorWindow20};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorTabBackgroundInactiveFrameActive] = {ui::kColorFrameActive};
  mixer[kColorTabBackgroundInactiveFrameInactive] = {ui::kColorFrameInactive};

  mixer[kColorTabForegroundActiveFrameActive] = {
      nala::kColorPrimitiveTorWindow95};
  mixer[kColorTabForegroundActiveFrameInactive] = {
      nala::kColorPrimitiveTorWindow80};
  mixer[kColorTabForegroundInactiveFrameActive] = {
      nala::kColorPrimitiveTorWindow80};
  mixer[kColorToolbar] = {nala::kColorPrimitiveTorWindow20};
  mixer[kColorToolbarButtonIcon] = {nala::kColorPrimitiveTorWindow70};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, kBraveDisabledControlAlpha)};
  mixer[ui::kColorFrameActive] = {kPrivateTorFrame};
  mixer[ui::kColorFrameInactive] = {kPrivateTorFrame};
  mixer[kColorTabDividerFrameInactive] = {nala::kColorPrimitiveTorWindow35};
  mixer[kColorTabDividerFrameActive] = {nala::kColorPrimitiveTorWindow35};
}

void AddBraveOmniboxPrivateThemeColorMixer(ui::ColorProvider* provider,
                                           const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorToolbarBackgroundSubtleEmphasis] = {kPrivateFrame};
  mixer[kColorToolbarBackgroundSubtleEmphasisHovered] = {
      kColorToolbarBackgroundSubtleEmphasis};

  mixer[kColorOmniboxResultsBackground] = {kPrivateFrame};
  if (IsHighContrast()) {
    mixer[kColorOmniboxResultsBackground] = {
        color_utils::HSLShift(kPrivateFrame, {-1, -1, 0.45})};
  }

  AddOmniboxHoverSelect(mixer);

  mixer[kColorOmniboxIconBackground] = {SK_ColorTRANSPARENT};
  mixer[kColorOmniboxSecurityChipDangerousBackground] = {SK_ColorTRANSPARENT};
  mixer[kColorOmniboxSecurityChipText] = {kColorOmniboxSecurityChipDangerous};
}

void AddBraveOmniboxColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  const bool use_accent_tinted_omnibox = ShouldUseAccentTintedPalette(key);
  const bool is_dark = key.color_mode == ui::ColorProviderKey::ColorMode::kDark;

  if (!key.custom_theme) {
    mixer[kColorToolbarContentAreaSeparator] = {nala::kColorDividerSubtle};

    if (use_accent_tinted_omnibox && is_dark) {
      // Nala omnibox tokens use neutral primitives which do not visibly pick up
      // accent themes. Use the secondary ref palette instead, which is
      // generated from the user's accent color in dark mode.
      mixer[kColorLocationBarBackground] = {ui::kColorRefSecondary15};
      mixer[kColorOmniboxResultsBackground] = {ui::kColorRefSecondary20};
      mixer[kColorOmniboxResultsBackgroundHovered] = {ui::kColorRefSecondary25};
      mixer[kColorOmniboxResultsBackgroundSelected] = {
          ui::kColorRefSecondary25};
    } else if (use_accent_tinted_omnibox) {
      // Light mode uses a tinted surface token so the omnibox container
      // reflects the user's accent color.
      mixer[kColorLocationBarBackground] = {ui::kColorSysSurface4};
      mixer[kColorOmniboxResultsBackground] = {
          nala::kColorDesktopbrowserOmnibarBackgroundActive};
      mixer[kColorOmniboxResultsBackgroundHovered] = {
          nala::kColorDesktopbrowserOmnibarBackgroundHoverItem};
      mixer[kColorOmniboxResultsBackgroundSelected] = {
          nala::kColorDesktopbrowserOmnibarBackgroundSelectedItem};
    } else {
      mixer[kColorLocationBarBackground] = {
          nala::kColorDesktopbrowserOmnibarBackgroundDesktop};
      mixer[kColorOmniboxResultsBackground] = {
          nala::kColorDesktopbrowserOmnibarBackgroundActive};
      mixer[kColorOmniboxResultsBackgroundHovered] = {
          nala::kColorDesktopbrowserOmnibarBackgroundHoverItem};
      mixer[kColorOmniboxResultsBackgroundSelected] = {
          nala::kColorDesktopbrowserOmnibarBackgroundSelectedItem};
    }
  }
  mixer[kColorBraveOmniboxResultViewSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorOmniboxResultsUrl] = {nala::kColorTextInteractive};
  // We use a shadow for the Omnibox hover effect, rather than a color change.
  mixer[kColorLocationBarBackgroundHovered] = {kColorLocationBarBackground};
  mixer[kColorOmniboxIconHover] = {
      ui::SetAlpha(kColorOmniboxText, std::ceil(0.10f * 255.0f))};

  // We don't use bg color for location icon view.
  mixer[kColorOmniboxIconBackground] = {SK_ColorTRANSPARENT};
  mixer[kColorOmniboxSecurityChipDangerousBackground] = {SK_ColorTRANSPARENT};
  mixer[kColorOmniboxSecurityChipText] = {kColorOmniboxSecurityChipDangerous};

#if defined(TOOLKIT_VIEWS)
  if (!base::FeatureList::IsEnabled(
          darker_theme::features::kBraveDarkerTheme) ||
      key.scheme_variant != ui::ColorProviderKey::SchemeVariant::kDarker) {
    return;
  }

  auto& postprocessing_mixer = provider->AddPostprocessingMixer();
  // Location bar
  // Note: The darker (ultra dark) theme intentionally keeps the original
  // omnibar tokens regardless of accent color. Accent tinting only applies to
  // the regular dark-mode branch in AddBraveOmniboxColorMixer's
  // !custom_theme block.
  postprocessing_mixer[kColorLocationBarBackground] = {
      nala::kColorPrimitiveNeutral0};
  postprocessing_mixer[kColorLocationBarBackgroundHovered] = {
      nala::kColorPrimitiveNeutral0};

  // Omnibox
  postprocessing_mixer[kColorOmniboxResultsBackground] = {
      nala::kColorPrimitiveNeutral0};
  postprocessing_mixer[kColorOmniboxResultsBackgroundHovered] = {
      nala::kColorPrimitiveNeutral20};
  postprocessing_mixer[kColorOmniboxResultsBackgroundSelected] = {
      nala::kColorPrimitiveNeutral20};
  postprocessing_mixer[kColorBraveOmniboxResultViewSeparator] = {
      nala::kColorPrimitiveNeutral20};

  // Toolbar
  postprocessing_mixer[kColorToolbarButtonIcon] = {
      nala::kColorPrimitiveNeutral50};
  postprocessing_mixer[kColorToolbarButtonIconPressed] = {
      nala::kColorPrimitiveNeutral60};
  postprocessing_mixer[kColorToolbarButtonIconInactive] = {
      SkColorSetA(postprocessing_mixer.GetResultColor(kColorToolbarButtonIcon),
                  0xff * 0.6)};
  postprocessing_mixer[kColorToolbarButtonIconHovered] =
      darker_theme::ApplyDarknessFromColor(nala::kColorPrimitiveNeutral60);
  postprocessing_mixer[kColorToolbarButtonActivated] = {
      nala::kColorPrimitivePrimary60};
  postprocessing_mixer[kColorToolbarInkDrop] = {nala::kColorPrimitiveNeutral10};
  postprocessing_mixer[kColorToolbarInkDropHover] = {
      nala::kColorPrimitiveNeutral10};
  postprocessing_mixer[kColorToolbarInkDropRipple] = {
      nala::kColorPrimitiveNeutral20};

  // Sidebar button
  postprocessing_mixer[kColorSidebarButtonBase] = {
      postprocessing_mixer.GetResultColor(kColorToolbarButtonIcon)};
  if (!HasCustomToolbarColor(key)) {
    mixer[kColorSidebarButtonPressed] = {
        postprocessing_mixer.GetResultColor(kColorToolbarButtonActivated)};
  }

  if (!key.custom_theme) {
    mixer[kColorToolbarContentAreaSeparator] = {
        postprocessing_mixer.GetResultColor(kColorToolbar)};
  }
#endif  // defined(TOOLKIT_VIEWS)
}

void AddBravifiedTabStripColorMixer(ui::ColorProvider* provider,
                                    const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  // TabStripComboButton's bg color should be different per horizontal tab mode.
  // When horizontal tab, that color should be same with title bar's bg color.
  // When vertical tab, that color should be toolbar's bg color.
  // To make TabStripComboButton have same bg color with title bar or toolbar,
  // simply set its bg color transparent.
  // If this causes other buttons' bg color regressions, we need to change
  // combo button's bg color directly.
  mixer[kColorNewTabButtonCRBackgroundFrameActive] = {SK_ColorTRANSPARENT};
  mixer[kColorNewTabButtonCRBackgroundFrameInactive] = {SK_ColorTRANSPARENT};

  if (key.custom_theme) {
    return;
  }

  mixer[kColorNewTabButtonFocusRing] = {ui::kColorFocusableBorderFocused};
  mixer[kColorTabBackgroundActiveFrameActive] = {kColorToolbar};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorTabDividerFrameActive] =
      ui::AlphaBlend({nala::kColorDividerStrong},
                     {kColorTabBackgroundInactiveFrameActive}, 0.75 * 0xff);
  mixer[kColorTabDividerFrameInactive] = {kColorTabDividerFrameActive};

  // Other tab colors are handled in brave_tab_color_mixer.cc
}
