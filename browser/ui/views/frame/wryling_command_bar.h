/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_FRAME_WRYLING_COMMAND_BAR_H_
#define BRAVE_BROWSER_UI_VIEWS_FRAME_WRYLING_COMMAND_BAR_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;

namespace views {
class View;
}

// Wryling: a command bar. ⌘T and the sidebar's New Tab row show a
// centred floating glass panel over the current page, with an empty search
// field and its suggestions inside. Enter (or a click on a suggestion) opens
// the choice in a new tab; Escape, clicking elsewhere or switching tabs just
// closes the panel. No tab exists until something is chosen.
//
// The panel is its own small window because web pages on macOS draw above
// the browser window's own views. The sidebar's address bar is moved into it
// and back out, so there is only ever one address bar.
class WrylingCommandBar : public views::WidgetObserver,
                          public TabStripModelObserver {
 public:
  explicit WrylingCommandBar(BrowserView* browser_view);
  WrylingCommandBar(const WrylingCommandBar&) = delete;
  WrylingCommandBar& operator=(const WrylingCommandBar&) = delete;
  ~WrylingCommandBar() override;

  // Shows the panel if the address bar is in the sidebar. Returns false
  // otherwise (the caller then opens a tab the usual way).
  bool Show();
  // Puts the address bar back (showing the current page's address again) and
  // closes the panel.
  void Hide();
  // Hide(), just after the current event (e.g. once a choice has opened).
  void HideSoon();
  bool IsShowing() const { return !!widget_; }

  // Whether |location_bar| is currently inside the panel.
  static bool IsLocationBarInCommandBar(const views::View* location_bar);
  // The panel holding |location_bar|, if any.
  static WrylingCommandBar* FromLocationBar(const views::View* location_bar);

  // The suggestions (a separate, transparent window drawn over the panel)
  // report their screen bounds here, or nullopt when they close. The panel
  // grows to sit behind them, so field and suggestions are one glass shape.
  void SetSuggestionsBounds(std::optional<gfx::Rect> screen_bounds);

 private:
  gfx::Rect GetPanelBounds() const;
  void UpdateGlass();
  void ApplySuggestionsBounds(std::optional<gfx::Rect> screen_bounds);

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void RunTestSwitchIfSet();

  const raw_ptr<BrowserView> browser_view_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> panel_ = nullptr;
  // Where the address bar lives when it isn't in the panel.
  raw_ptr<views::View> home_parent_ = nullptr;
  std::optional<size_t> home_index_;
  // The panel's own (field) bounds, before growing for suggestions.
  gfx::Rect field_bounds_;
  // The address bar's text size before it was enlarged for the panel.
  std::optional<gfx::FontList> home_font_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  // Set while Hide() runs, so the panel's own deactivation doesn't re-enter.
  bool hiding_ = false;

  base::WeakPtrFactory<WrylingCommandBar> weak_factory_{this};
};

#endif  // BRAVE_BROWSER_UI_VIEWS_FRAME_WRYLING_COMMAND_BAR_H_
