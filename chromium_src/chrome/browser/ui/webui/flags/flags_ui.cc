// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "brave/browser/ui/color/wryling_space_color.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace {

// Flags does not invalidate its colour provider for browser theme changes.
// Notify the standard CSS listener without reloading the flags or their state.
class WrylingFlagsThemeHandler : public content::WebUIMessageHandler {
 public:
  void RegisterMessages() override {
    prefs_.Init(Profile::FromWebUI(web_ui())->GetPrefs());
    const auto changed = base::BindRepeating(
        &WrylingFlagsThemeHandler::OnThemeChanged, weak_factory_.GetWeakPtr());
    prefs_.Add(prefs::kUserColor, changed);
    prefs_.Add(prefs::kBrowserColorVariant, changed);
    prefs_.Add(wryling::kTintOpacityPref, changed);
  }

 private:
  void OnThemeChanged() {
    if (auto* handler = ui::ColorChangeHandler::GetForCurrentDocument(
            web_ui()->GetWebContents()->GetPrimaryMainFrame())) {
      handler->OnColorProviderChanged();
    }
  }

  PrefChangeRegistrar prefs_;
  base::WeakPtrFactory<WrylingFlagsThemeHandler> weak_factory_{this};
};

}  // namespace

#define BRAVE_FLAGS_UI_CONSTRUCTOR                                     \
  web_ui->SetBindings(content::BindingsPolicySet(                       \
      {content::BindingsPolicyValue::kWebUi,                            \
       content::BindingsPolicyValue::kMojoWebUi}));                     \
  web_ui->AddMessageHandler(std::make_unique<WrylingFlagsThemeHandler>());
#include <chrome/browser/ui/webui/flags/flags_ui.cc>
#undef BRAVE_FLAGS_UI_CONSTRUCTOR
