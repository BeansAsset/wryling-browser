/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_LOCATION_BAR_WRYLING_COPY_LINK_BUTTON_H_
#define BRAVE_BROWSER_UI_VIEWS_LOCATION_BAR_WRYLING_COPY_LINK_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

// Wryling: the "copy link" button in the sidebar's address pill.
// A click copies the page's address; the link icon pops into a checkmark in
// the accent colour for a moment and a "Link copied" toast appears.
class WrylingCopyLinkButton : public views::ImageButton {
  METADATA_HEADER(WrylingCopyLinkButton, views::ImageButton)

 public:
  explicit WrylingCopyLinkButton(BrowserWindowInterface* browser);
  WrylingCopyLinkButton(const WrylingCopyLinkButton&) = delete;
  WrylingCopyLinkButton& operator=(const WrylingCopyLinkButton&) = delete;
  ~WrylingCopyLinkButton() override;

  // Whether there's an address worth copying in |contents| (http, https or
  // file pages; not the new tab page or other internal pages).
  static bool CanCopy(content::WebContents* contents);

  // views::ImageButton:
  void OnThemeChanged() override;

 private:
  void OnPressed();
  void ShowCopied(bool copied);
  void Pop();

  const raw_ptr<BrowserWindowInterface> browser_;
  bool showing_copied_ = false;
  base::OneShotTimer revert_timer_;
  base::WeakPtrFactory<WrylingCopyLinkButton> weak_factory_{this};
};

#endif  // BRAVE_BROWSER_UI_VIEWS_LOCATION_BAR_WRYLING_COPY_LINK_BUTTON_H_
