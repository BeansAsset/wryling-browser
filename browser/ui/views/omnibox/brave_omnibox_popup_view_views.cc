/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/omnibox/brave_omnibox_popup_view_views.h"

#include <algorithm>

#include "brave/browser/ui/tabs/public/vertical_tab_controller.h"
#include "brave/browser/ui/views/frame/wryling_command_bar.h"
#include "brave/browser/ui/views/toolbar/brave_toolbar_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "ui/base/metadata/metadata_impl_macros.h"

BraveOmniboxPopupViewViews::~BraveOmniboxPopupViewViews() = default;

gfx::Rect BraveOmniboxPopupViewViews::GetTargetBounds() const {
  auto bounds = OmniboxPopupViewViews::GetTargetBounds();
#if BUILDFLAG(IS_MAC)
  // Wryling command bar: the suggestions sit exactly over the panel (their
  // address-bar area over its field), without a shadow of their own; the
  // panel grows behind them. See BraveRoundedOmniboxResultsFrame.
  if (WrylingCommandBar::IsLocationBarInCommandBar(location_bar_view_)) {
    bounds.Inset(RoundedOmniboxResultsFrame::GetShadowInsets());
    return bounds;
  }
#endif
#if BUILDFLAG(IS_MAC)
  // Wryling: with the address bar in the sidebar, the suggestions float
  // below it as a popover (no "expanded address bar" cutout), wider than the
  // sidebar and kept inside the window. See BraveRoundedOmniboxResultsFrame.
  if (BraveToolbarView::IsLocationBarInWrylingSidebar(location_bar_view_)) {
    const gfx::Insets shadow = RoundedOmniboxResultsFrame::GetShadowInsets();
    gfx::Rect content = bounds;
    content.Inset(shadow);
    const int results_height =
        content.height() -
        RoundedOmniboxResultsFrame::GetNonResultSectionHeight();
    const gfx::Rect field = location_bar_view_->GetBoundsInScreen();
    const gfx::Rect window =
        location_bar_view_->GetWidget()->GetWindowBoundsInScreen();
    constexpr int kWindowMargin = 8;
    const int width = std::min(
        std::max(field.width(), BraveToolbarView::kWrylingPopoverMinWidth),
        window.width() - 2 * kWindowMargin);
    gfx::Rect popover(
        field.x(), field.bottom() + BraveToolbarView::kWrylingPopoverGap,
        width,
        results_height > 0
            ? results_height + BraveToolbarView::kWrylingPopoverTopPadding
            : 0);
    if (popover.right() > window.right() - kWindowMargin) {
      popover.set_x(std::max(window.x() + kWindowMargin,
                             window.right() - kWindowMargin - width));
    }
    popover.Inset(-shadow);
    return popover;
  }
#endif
  if (auto* vtc =
          VerticalTabController::FromBrowser(location_bar_view_->browser());
      vtc && vtc->ShouldShowBraveVerticalTabs() &&
      !vtc->ShouldShowWindowTitleForVerticalTabs()) {
    // Remove top shadow inset so that omnibox popup stays inside browser
    // widget. Especially on Mac, Widgets can't be out of screen so we need to
    // adjust popup position.
    // https://github.com/brave/brave-browser/issues/26573
    bounds.Inset(gfx::Insets().set_top(
        RoundedOmniboxResultsFrame::GetShadowInsets().top()));
  }

  return bounds;
}

int BraveOmniboxPopupViewViews::GetLocationBarViewWidth() const {
  return location_bar_view()->width();
}

BEGIN_METADATA(BraveOmniboxPopupViewViews)
END_METADATA
