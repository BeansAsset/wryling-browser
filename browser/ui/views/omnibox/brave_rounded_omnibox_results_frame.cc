/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/omnibox/brave_rounded_omnibox_results_frame.h"

#include <memory>
#include <utility>

#include "brave/browser/ui/tabs/brave_tab_prefs.h"
#include "brave/browser/ui/tabs/public/vertical_tab_controller.h"
#include "brave/browser/ui/views/frame/wryling_command_bar.h"
#include "brave/browser/ui/views/toolbar/brave_toolbar_view.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"

#if BUILDFLAG(IS_MAC)
#include "ui/views/bubble/popup_glass_mac.h"
#endif

namespace {
// Popover corner radius (radius-xl).
constexpr int kWrylingPopoverRadius = 14;
}  // namespace

BraveRoundedOmniboxResultsFrame::BraveRoundedOmniboxResultsFrame(
    views::View* contents,
    LocationBarView* location_bar,
    bool forward_mouse_events)
    : RoundedOmniboxResultsFrame(contents, location_bar, forward_mouse_events),
      browser_(location_bar->browser()),
      location_bar_(location_bar) {
  wryling_popover_ = IsWrylingPopover();
  wryling_command_bar_ = IsWrylingCommandBar();
  UpdateShadowBorder();

  show_vertical_tabs_.Init(
      brave_tabs::kVerticalTabsEnabled,
      browser_->GetProfile()->GetOriginalProfile()->GetPrefs(),
      base::BindRepeating(&BraveRoundedOmniboxResultsFrame::UpdateShadowBorder,
                          base::Unretained(this)));
  show_window_title_for_vertical_tabs_.Init(
      brave_tabs::kVerticalTabsShowTitleOnWindow,
      browser_->GetProfile()->GetOriginalProfile()->GetPrefs(),
      base::BindRepeating(&BraveRoundedOmniboxResultsFrame::UpdateShadowBorder,
                          base::Unretained(this)));
}

BraveRoundedOmniboxResultsFrame::~BraveRoundedOmniboxResultsFrame() = default;

void BraveRoundedOmniboxResultsFrame::RemovedFromWidget() {
  // The suggestions are closing: the command bar panel shrinks back.
  if (auto* bar = WrylingCommandBar::FromLocationBar(location_bar_)) {
    bar->SetSuggestionsBounds(std::nullopt);
  }
  RoundedOmniboxResultsFrame::RemovedFromWidget();
}

bool BraveRoundedOmniboxResultsFrame::IsWrylingCommandBar() const {
  return WrylingCommandBar::IsLocationBarInCommandBar(location_bar_);
}

bool BraveRoundedOmniboxResultsFrame::IsWrylingPopover() const {
#if BUILDFLAG(IS_MAC)
  return BraveToolbarView::IsLocationBarInWrylingSidebar(location_bar_);
#else
  return false;
#endif
}

void BraveRoundedOmniboxResultsFrame::Layout(PassKey) {
  const bool popover = IsWrylingPopover();
  const bool command_bar = IsWrylingCommandBar();
  if (popover != wryling_popover_ || command_bar != wryling_command_bar_) {
    wryling_popover_ = popover;
    wryling_command_bar_ = command_bar;
    UpdateShadowBorder();
  }
  if (wryling_command_bar_) {
    LayoutSuperclass<RoundedOmniboxResultsFrame>(this);
    if (children().empty()) {
      return;
    }
    // Transparent rows over the command bar panel, which grows behind them
    // and provides the glass: field and suggestions read as one panel.
    views::View* const host = children().front();
    if (host->background()) {
      host->SetBackground(nullptr);
    }
#if BUILDFLAG(IS_MAC)
    if (GetWidget()) {
      views::PrepareViewsForWrylingGlass(GetWidget()->GetRootView());
    }
#endif
    if (auto* bar = WrylingCommandBar::FromLocationBar(location_bar_)) {
      bar->SetSuggestionsBounds(host->GetBoundsInScreen());
    }
    return;
  }
  if (wryling_popover_) {
    // No address-bar section at the top: the popover sits below the field.
    SetCutoutVisibility(false);
  }
  LayoutSuperclass<RoundedOmniboxResultsFrame>(this);
  if (!wryling_popover_ || children().empty()) {
    return;
  }

  // The contents host is the frame's only direct child.
  views::View* const host = children().front();
  host->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kWrylingPopoverRadius));
  if (views::View* const results = GetContents()) {
    gfx::Rect bounds = results->bounds();
    bounds.Inset(gfx::Insets::TLBR(BraveToolbarView::kWrylingPopoverTopPadding,
                                   0, 0, 0));
    results->SetBoundsRect(bounds);
  }

#if BUILDFLAG(IS_MAC)
  // Real glass behind the suggestions, like the sidebar and menus; without
  // it the host keeps its solid "raised" fill.
  const bool glass = GetWidget() &&
                     views::UpdateWrylingPopupGlass(
                         GetWidget(), host->ConvertRectToWidget(
                                          host->GetLocalBounds()),
                         kWrylingPopoverRadius);
  if (glass == !!host->background()) {
    host->SetBackground(
        glass ? nullptr
              : views::CreateSolidBackground(kColorOmniboxResultsBackground));
  }
#endif
}

void BraveRoundedOmniboxResultsFrame::UpdateShadowBorder() {
  if (wryling_command_bar_) {
    // The panel's window shadow covers both; no shadow of their own.
    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::Arrow::NONE, views::BubbleBorder::NO_SHADOW);
    border->set_insets(gfx::Insets());
    border->set_rounded_corners(gfx::RoundedCornersF(22));
    SetBorder(std::move(border));
    return;
  }
  int corner_radius = wryling_popover_
                          ? kWrylingPopoverRadius
                          : views::LayoutProvider::Get()->GetCornerRadiusMetric(
                                views::Emphasis::kHigh);

  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::Arrow::NONE,
      views::BubbleBorder::Shadow::STANDARD_SHADOW);
  border->set_rounded_corners(gfx::RoundedCornersF(corner_radius));
  border->set_md_shadow_elevation(
      RoundedOmniboxResultsFrame::kDefaultElevation);
  // A Wryling popover sits below the address bar, well inside the window,
  // so it keeps its full shadow.
  if (auto* vtc = VerticalTabController::FromBrowser(browser_);
      !wryling_popover_ && vtc && vtc->ShouldShowBraveVerticalTabs() &&
      !vtc->ShouldShowWindowTitleForVerticalTabs()) {
    // Remove top shadow inset so that omnibox popup stays inside browser
    // widget. Especially on Mac, Widgets can't be out of screen so we need to
    // adjust popup position.
    // https://github.com/brave/brave-browser/issues/26573
    border->set_insets(
        RoundedOmniboxResultsFrame::GetShadowInsets().set_top(0));
  }
  SetBorder(std::move(border));
}

BEGIN_METADATA(BraveRoundedOmniboxResultsFrame)
END_METADATA
