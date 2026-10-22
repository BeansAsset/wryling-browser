/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/wryling_dashboard/wryling_dashboard_binder.h"

#include <utility>

namespace wryling_dashboard {

namespace {
Binder g_binder = nullptr;
}  // namespace

void SetBinder(Binder binder) {
  g_binder = binder;
}

void BindPageHandler(content::WebContents* web_contents,
                     mojo::PendingReceiver<mojom::PageHandler> receiver) {
  if (g_binder) {
    g_binder(web_contents, std::move(receiver));
  }
}

}  // namespace wryling_dashboard
