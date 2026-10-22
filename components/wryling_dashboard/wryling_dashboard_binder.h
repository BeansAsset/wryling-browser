/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_WRYLING_DASHBOARD_WRYLING_DASHBOARD_BINDER_H_
#define BRAVE_COMPONENTS_WRYLING_DASHBOARD_WRYLING_DASHBOARD_BINDER_H_

#include "brave/components/wryling_dashboard/wryling_dashboard.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class WebContents;
}  // namespace content

namespace wryling_dashboard {

// The New Tab page binds the dashboard API through this, so its WebUI target
// needn't depend on //brave/browser/ui, where the dashboard lives (and which
// depends on the WebUI).
using Binder = void (*)(content::WebContents* web_contents,
                        mojo::PendingReceiver<mojom::PageHandler> receiver);

void SetBinder(Binder binder);
// Drops the receiver (the page gets a closed pipe) when nothing is set.
void BindPageHandler(content::WebContents* web_contents,
                     mojo::PendingReceiver<mojom::PageHandler> receiver);

}  // namespace wryling_dashboard

#endif  // BRAVE_COMPONENTS_WRYLING_DASHBOARD_WRYLING_DASHBOARD_BINDER_H_
