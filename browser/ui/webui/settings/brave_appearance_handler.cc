/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/webui/settings/brave_appearance_handler.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "brave/browser/new_tab/new_tab_shows_options.h"
#include "brave/browser/profiles/profile_util.h"
#include "brave/browser/ui/color/wryling_space_color.h"
#include "brave/components/constants/pref_names.h"
#include "brave/components/ntp_background_images/common/pref_names.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_ui.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/color/color_provider_key.h"

namespace {

base::DictValue WrylingAppearanceState(Profile* profile) {
  ui::ColorProviderKey key;
  key.color_mode = ui::ColorProviderKey::ColorMode::kLight;
  const SkColor color =
      ThemeServiceFactory::GetForProfile(profile)->GetUserColor().value_or(
          wryling::GetBaseSpaceColor(key));
  return base::DictValue()
      .Set("color", static_cast<int>(color & 0xFFFFFF))
      .Set(
          "opacity",
          std::clamp(profile->GetPrefs()->GetInteger(wryling::kTintOpacityPref),
                     0, 100))
      .Set("clearGlass",
           profile->GetPrefs()->GetBoolean(wryling::kClearGlassPref));
}

}  // namespace

BraveAppearanceHandler::BraveAppearanceHandler() = default;

BraveAppearanceHandler::~BraveAppearanceHandler() {
  if (command_updater_) {
    command_updater_->RemoveCommandObserver(IDC_TOGGLE_VERTICAL_TABS, this);
  }
}

// TODO(simonhong): Use separate handler for NTP settings.
void BraveAppearanceHandler::RegisterMessages() {
  profile_ = Profile::FromWebUI(web_ui());
  profile_state_change_registrar_.Init(profile_->GetPrefs());
  for (const char* pref :
       {prefs::kUserColor, prefs::kCurrentThemeID, wryling::kTintOpacityPref,
        wryling::kClearGlassPref}) {
    profile_state_change_registrar_.Add(
        pref, base::BindRepeating(&BraveAppearanceHandler::OnPreferenceChanged,
                                  base::Unretained(this)));
  }
  web_ui()->RegisterMessageCallback(
      "getWrylingAppearance",
      base::BindRepeating(&BraveAppearanceHandler::GetWrylingAppearance,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setWrylingSpaceColor",
      base::BindRepeating(&BraveAppearanceHandler::SetWrylingSpaceColor,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setWrylingTintOpacity",
      base::BindRepeating(&BraveAppearanceHandler::SetWrylingTintOpacity,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setWrylingClearGlass",
      base::BindRepeating(&BraveAppearanceHandler::SetWrylingClearGlass,
                          base::Unretained(this)));
  profile_state_change_registrar_.Add(
      kNewTabPageShowsOptions,
      base::BindRepeating(&BraveAppearanceHandler::OnPreferenceChanged,
                          base::Unretained(this)));
  profile_state_change_registrar_.Add(
      prefs::kHomePageIsNewTabPage,
      base::BindRepeating(&BraveAppearanceHandler::OnPreferenceChanged,
                          base::Unretained(this)));
  profile_state_change_registrar_.Add(
      prefs::kHomePage,
      base::BindRepeating(&BraveAppearanceHandler::OnPreferenceChanged,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNewTabShowsOptionsList",
      base::BindRepeating(&BraveAppearanceHandler::GetNewTabShowsOptionsList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "shouldShowNewTabDashboardSettings",
      base::BindRepeating(
          &BraveAppearanceHandler::ShouldShowNewTabDashboardSettings,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getIsVerticalTabsToggleEnabled",
      base::BindRepeating(
          &BraveAppearanceHandler::GetIsVerticalTabsToggleEnabled,
          base::Unretained(this)));

  if (auto* tab = tabs::TabInterface::MaybeGetFromContents(
          web_ui()->GetWebContents())) {
    auto* browser_window_interface = tab->GetBrowserWindowInterface();
    command_updater_ =
        browser_window_interface->GetFeatures().browser_command_controller();
    command_updater_->AddCommandObserver(IDC_TOGGLE_VERTICAL_TABS, this);

    web_ui()->RegisterMessageCallback(
        "getIsCompactModeToggleEnabled",
        base::BindRepeating(
            &BraveAppearanceHandler::GetIsCompactModeToggleEnabled,
            base::Unretained(this)));

#if BUILDFLAG(IS_MAC)
    // Unretained() is safe: |fullscreen_subscription_| is a member, so it
    // unsubscribes this callback on our destruction, before |this| dangles.
    fullscreen_subscription_ =
        browser_window_interface->GetFeatures()
            .exclusive_access_manager()
            ->fullscreen_controller()
            ->RegisterOnFullscreenStateChanged(base::BindRepeating(
                &BraveAppearanceHandler::OnFullscreenStateChanged,
                base::Unretained(this)));
#endif
  }
}

void BraveAppearanceHandler::EnabledStateChangedForCommand(int id,
                                                           bool enabled) {
  DCHECK_EQ(id, IDC_TOGGLE_VERTICAL_TABS);
  if (IsJavascriptAllowed()) {
    FireWebUIListener("vertical-tabs-toggle-enabled-changed",
                      base::Value(enabled));
  }
}

bool BraveAppearanceHandler::IsCompactModeToggleEnabled() {
#if BUILDFLAG(IS_MAC)
  auto* tab =
      tabs::TabInterface::MaybeGetFromContents(web_ui()->GetWebContents());
  if (!tab) {
    return true;
  }

  auto* browser_window_interface = tab->GetBrowserWindowInterface();
  auto* exclusive_access_manager =
      browser_window_interface
          ? browser_window_interface->GetFeatures().exclusive_access_manager()
          : nullptr;

  // Compact mode is incompatible with immersive fullscreen (see
  // WindowFeatureController::UsesImmersiveFullscreenMode()), so the toggle
  // is disabled while the browser window is fullscreen. Uses
  // IsFullscreenForBrowser() rather than the raw window fullscreen state so
  // that tab-initiated fullscreen (e.g. a fullscreened video) doesn't disable
  // the toggle.
  return !exclusive_access_manager ||
         !exclusive_access_manager->fullscreen_controller()
              ->IsFullscreenForBrowser();
#else
  return true;
#endif
}

void BraveAppearanceHandler::OnFullscreenStateChanged() {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("compact-mode-toggle-enabled-changed",
                      base::Value(IsCompactModeToggleEnabled()));
  }
}

void BraveAppearanceHandler::GetIsCompactModeToggleEnabled(
    const base::ListValue& args) {
  CHECK_EQ(args.size(), 1U);
  AllowJavascript();
  ResolveJavascriptCallback(args[0], base::Value(IsCompactModeToggleEnabled()));
}

void BraveAppearanceHandler::OnPreferenceChanged(const std::string& pref_name) {
  if (IsJavascriptAllowed()) {
    if (pref_name == prefs::kUserColor || pref_name == prefs::kCurrentThemeID ||
        pref_name == wryling::kTintOpacityPref ||
        pref_name == wryling::kClearGlassPref) {
      FireWebUIListener("wryling-appearance-changed",
                        base::Value(WrylingAppearanceState(profile_)));
      return;
    }
    if (pref_name == kNewTabPageShowsOptions || pref_name == prefs::kHomePage ||
        pref_name == prefs::kHomePageIsNewTabPage) {
      FireWebUIListener(
          "show-new-tab-dashboard-settings-changed",
          base::Value(brave::ShouldNewTabShowDashboard(profile_)));
      return;
    }
  }
}

void BraveAppearanceHandler::GetNewTabShowsOptionsList(
    const base::ListValue& args) {
  CHECK_EQ(args.size(), 1U);
  AllowJavascript();
  ResolveJavascriptCallback(
      args[0], base::Value(brave::GetNewTabShowsOptionsList(profile_)));
}

void BraveAppearanceHandler::ShouldShowNewTabDashboardSettings(
    const base::ListValue& args) {
  CHECK_EQ(args.size(), 1U);
  AllowJavascript();
  ResolveJavascriptCallback(
      args[0], base::Value(brave::ShouldNewTabShowDashboard(profile_)));
}

void BraveAppearanceHandler::GetIsVerticalTabsToggleEnabled(
    const base::ListValue& args) {
  CHECK_EQ(args.size(), 1U);
  AllowJavascript();
  const bool enabled = !command_updater_ || command_updater_->IsCommandEnabled(
                                                IDC_TOGGLE_VERTICAL_TABS);
  ResolveJavascriptCallback(args[0], base::Value(enabled));
}

void BraveAppearanceHandler::GetWrylingAppearance(const base::ListValue& args) {
  CHECK_EQ(args.size(), 1U);
  AllowJavascript();
  ResolveJavascriptCallback(args[0], WrylingAppearanceState(profile_));
}

void BraveAppearanceHandler::SetWrylingSpaceColor(const base::ListValue& args) {
  if (args.size() != 1U || !args[0].is_int() || args[0].GetInt() < 0 ||
      args[0].GetInt() > 0xFFFFFF) {
    return;
  }
  ThemeServiceFactory::GetForProfile(profile_)
      ->SetUserColorAndBrowserColorVariant(
          SkColorSetA(static_cast<SkColor>(args[0].GetInt()), SK_AlphaOPAQUE),
          ui::mojom::BrowserColorVariant::kTonalSpot);
}

void BraveAppearanceHandler::SetWrylingTintOpacity(
    const base::ListValue& args) {
  if (args.size() != 1U || !args[0].is_int() || args[0].GetInt() < 0 ||
      args[0].GetInt() > 100) {
    return;
  }
  profile_->GetPrefs()->SetInteger(wryling::kTintOpacityPref, args[0].GetInt());
}

void BraveAppearanceHandler::SetWrylingClearGlass(const base::ListValue& args) {
  if (args.size() != 1U || !args[0].is_bool()) {
    return;
  }
  profile_->GetPrefs()->SetBoolean(wryling::kClearGlassPref, args[0].GetBool());
}
