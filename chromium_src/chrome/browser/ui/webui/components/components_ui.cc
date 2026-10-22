// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "content/public/common/bindings_policy.h"

// The standard theme updater needs Mojo on this legacy page.
#define BRAVE_COMPONENTS_UI_CONSTRUCTOR                              \
  web_ui->SetBindings(content::BindingsPolicySet(                     \
      {content::BindingsPolicyValue::kWebUi,                         \
       content::BindingsPolicyValue::kMojoWebUi}));
#include <chrome/browser/ui/webui/components/components_ui.cc>
#undef BRAVE_COMPONENTS_UI_CONSTRUCTOR
