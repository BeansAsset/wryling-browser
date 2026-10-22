/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_OMNIBOX_BRAVE_ROUNDED_OMNIBOX_RESULTS_FRAME_H_
#define BRAVE_BROWSER_UI_VIEWS_OMNIBOX_BRAVE_ROUNDED_OMNIBOX_RESULTS_FRAME_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "components/prefs/pref_member.h"

class BrowserWindowInterface;
class LocationBarView;

class BraveRoundedOmniboxResultsFrame : public RoundedOmniboxResultsFrame {
  METADATA_HEADER(BraveRoundedOmniboxResultsFrame, RoundedOmniboxResultsFrame)
 public:
  BraveRoundedOmniboxResultsFrame(views::View* contents,
                                  LocationBarView* location_bar,
                                  bool forward_mouse_events);
  ~BraveRoundedOmniboxResultsFrame() override;

  // views::View:
  void RemovedFromWidget() override;

 private:
  // views::View:
  void Layout(PassKey) override;

  void UpdateShadowBorder();
  // Wryling: true while the address bar is in the sidebar, where the
  // suggestions are a popover below it instead of an expanded address bar.
  bool IsWrylingPopover() const;
  // Wryling: true while the address bar is in the command bar, where the
  // suggestions are transparent over the panel's glass.
  bool IsWrylingCommandBar() const;

  BooleanPrefMember show_vertical_tabs_;
  BooleanPrefMember show_window_title_for_vertical_tabs_;

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ptr<LocationBarView> location_bar_ = nullptr;
  bool wryling_popover_ = false;
  bool wryling_command_bar_ = false;
};

#endif  // BRAVE_BROWSER_UI_VIEWS_OMNIBOX_BRAVE_ROUNDED_OMNIBOX_RESULTS_FRAME_H_
