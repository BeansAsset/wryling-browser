/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/version_info/version_info.h"
#include "components/version_info/version_info.h"

#include "content/public/common/bindings_policy.h"

#define GetVersionNumber GetBraveVersionNumberForDisplay
// The standard theme updater needs Mojo on this legacy page.
#define BRAVE_VERSION_UI_CONSTRUCTOR                                 \
  web_ui->SetBindings(content::BindingsPolicySet(                     \
      {content::BindingsPolicyValue::kWebUi,                         \
       content::BindingsPolicyValue::kMojoWebUi}));
#include <chrome/browser/ui/webui/version/version_ui.cc>
#undef BRAVE_VERSION_UI_CONSTRUCTOR
#undef GetVersionNumber
