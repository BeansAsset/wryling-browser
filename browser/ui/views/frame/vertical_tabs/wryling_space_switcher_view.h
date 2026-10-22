/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_FRAME_VERTICAL_TABS_WRYLING_SPACE_SWITCHER_VIEW_H_
#define BRAVE_BROWSER_UI_VIEWS_FRAME_VERTICAL_TABS_WRYLING_SPACE_SWITCHER_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "brave/browser/ui/spaces/wryling_spaces.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace views {
class Button;
class MenuRunner;
}  // namespace views

// Wryling Spaces: the row of space buttons at the bottom of the sidebar
// panel. Click, ⌃1 to ⌃9 or a two-finger sideways swipe on the sidebar switch
// spaces; right-click edits or deletes one; "+" adds one. Switching slides the
// tab list in from the side. See SPACES-design.md.
class WrylingSpaceSwitcherView : public views::View,
                                 public wryling::SpacesController::Observer {
  METADATA_HEADER(WrylingSpaceSwitcherView, views::View)

 public:
  static constexpr int kHeight = 40;
  // Room between the buttons' pills and the glass panel's edges: the panel's
  // radius (14) minus the pill's (8), so the corners stay concentric.
  static constexpr int kEdgeInset = 6;

  // `swipe_area` gets the swipe handler (the whole sidebar); `slide_target`
  // is what slides when the space changes (the tab list).
  WrylingSpaceSwitcherView(wryling::SpacesController* controller,
                           views::View* swipe_area,
                           views::View* slide_target);
  WrylingSpaceSwitcherView(const WrylingSpaceSwitcherView&) = delete;
  WrylingSpaceSwitcherView& operator=(const WrylingSpaceSwitcherView&) =
      delete;
  ~WrylingSpaceSwitcherView() override;

  wryling::SpacesController* controller() { return controller_; }

  // Opens the name/icon/colour popover for a space.
  void ShowEditor(const std::string& space_id, bool focus_name);
  void ConfirmDelete(const std::string& space_id);

  // views::View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;
  void AddedToWidget() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // wryling::SpacesController::Observer:
  void OnCurrentSpaceChanged(int direction) override;
  void OnSpacesChanged() override;

  // Sideways two-finger swipes anywhere on the sidebar.
  void HandleSidebarScroll(ui::ScrollEvent* event);

 private:
  class SpaceButton;
  class SwipeHandler;

  void RebuildButtons();
  void UpdateButtonStates();
  void AddSpace();
  SpaceButton* GetButtonFor(const std::string& space_id);
  void SlideIn(int direction);

  // WRYLING_TEST_SPACES="step;step;...": runs one step every 2.2 s and logs
  // "WRYLING_SPACES:" lines to stderr, so states can be checked without
  // clicking; WRYLING_TEST_SPACES_SHOTS=<dir> also captures the windows after
  // each step. Steps: log, create, add, edit:N, delete:N, deletenow:N,
  // switch:N, key:N (⌃N+1 through the focus manager), swipe:DX[:DY], newtab,
  // close, pin, group, move:T:N (tab T of the strip to space N), menu:N (the
  // real tab menu's "Move to space ▸" entry N), rename:N:NAME, color:N:I,
  // icon:N:I, quit (clean exit, so the session is saved).
  void RunTestStep(std::vector<std::string> steps, size_t index);
  void LogTestState(const std::string& step);

  raw_ptr<wryling::SpacesController> controller_;
  raw_ptr<views::View> swipe_area_;
  raw_ptr<views::View> slide_target_;
  raw_ptr<views::Button> add_button_ = nullptr;

  // Pre-target handler on the sidebar, forwarding to HandleSidebarScroll().
  std::unique_ptr<SwipeHandler> swipe_handler_;
  // Sideways swipe state for the current trackpad gesture.
  enum class SwipeAxis { kUndecided, kSideways, kVertical };
  float swipe_x_ = 0;
  float swipe_y_ = 0;
  SwipeAxis swipe_axis_ = SwipeAxis::kUndecided;
  bool swipe_done_ = false;
  bool swipe_in_gesture_ = false;
  base::TimeTicks swipe_last_event_;

  base::ScopedObservation<wryling::SpacesController,
                          wryling::SpacesController::Observer>
      observation_{this};
  base::WeakPtrFactory<WrylingSpaceSwitcherView> weak_factory_{this};
};

// SF Symbol `name` as an image, `size` points, in `color`. Empty if the symbol
// doesn't exist (or off macOS).
// Test only (WRYLING_TEST_SPACES_SHOTS): captures the app's windows.
void CaptureWrylingWindowsForTest(const std::string& path_prefix);

gfx::ImageSkia GetWrylingSymbolImage(const std::string& name,
                                     int size,
                                     SkColor color);

#endif  // BRAVE_BROWSER_UI_VIEWS_FRAME_VERTICAL_TABS_WRYLING_SPACE_SWITCHER_VIEW_H_
