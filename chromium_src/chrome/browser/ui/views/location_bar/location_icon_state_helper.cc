/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "chrome/browser/ui/views/location_bar/location_icon_state_helper.h"

#include "build/build_config.h"

#define ShouldShowSecurityChipText ShouldShowSecurityChipText_ChromiumImpl

#include <chrome/browser/ui/views/location_bar/location_icon_state_helper.cc>

#undef ShouldShowSecurityChipText

namespace location_bar {

bool ShouldShowSecurityChipText(const LocationBarModel* model,
                                content::WebContents* web_contents,
                                bool is_editing_or_empty) {
#if BUILDFLAG(IS_MAC)
  // Wryling: the address pill is just the page icon and the address, so the
  // identity chips ("Wryling" on internal pages, "File", extension names,
  // "Reader mode") are dropped; the address already says where you are.
  // Security warnings ("Not secure", "Dangerous") still show.
  const GURL& url = model->GetURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs(url::kFileScheme) ||
      url.SchemeIs(dom_distiller::kDomDistillerScheme) ||
      model->IsContextualTasksPage()) {
    return false;
  }
  if (web_contents &&
      !extensions::ui_util::GetEnabledExtensionNameForUrl(url, *web_contents)
           .empty()) {
    return false;
  }
#endif
  return ShouldShowSecurityChipText_ChromiumImpl(model, web_contents,
                                                 is_editing_or_empty);
}

}  // namespace location_bar
