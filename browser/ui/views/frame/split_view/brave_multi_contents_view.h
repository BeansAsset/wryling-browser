/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_BRAVE_MULTI_CONTENTS_VIEW_H_
#define BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_BRAVE_MULTI_CONTENTS_VIEW_H_

#include <map>
#include <optional>
#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "brave/browser/ui/views/frame/split_view/wryling_split_layout.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "base/timer/timer.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace sidebar {
FORWARD_DECLARE_TEST(SidebarBrowserWithWebPanelTest, WebPanelTest);
}  // namespace sidebar

class BraveContentsContainerView;
class WrylingSplitDivider;
class WrylingPaneHeader;

namespace views {
class EventMonitor;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

class PrefChangeRegistrar;

namespace wryling {
class WrylingSplitController;
}  // namespace wryling

namespace views {
class WebView;
}  // namespace views

class BraveMultiContentsView : public MultiContentsView,
                               public gfx::AnimationDelegate,
                               public ui::EventObserver {
  METADATA_HEADER(BraveMultiContentsView, MultiContentsView)

 public:
  static BraveMultiContentsView* From(MultiContentsView* view);

  BraveMultiContentsView(BrowserView* browser_view,
                         std::unique_ptr<MultiContentsViewDelegate> delegate);
  ~BraveMultiContentsView() override;

  void UseContentsContainerViewForWebPanel();
  void SetWebPanelContents(content::WebContents* web_contents);
  bool IsWebPanelVisible() const;

  void SetWebPanelWidth(int width);
  void SetWebPanelOnLeft(bool left);

  void set_web_panel_active(bool active) { is_web_panel_active_ = active; }
  bool is_web_panel_active_for_testing() const { return is_web_panel_active_; }
  views::View* GetWebPanelContentsViewForTesting() const;

  // Notifies the multi-contents view that the browser's active content domain
  // display state has changed.
  void OnShowActiveContentsDomainChanged();

  // Updates the contents area corner radii for the hosted contents areas, based
  // on the specified corner radii for the multi-contents view as a whole.
  void UpdateContentsCornerRadii(const gfx::RoundedCornersF& corner_radii);

  // MultiContentsView:
  // Give web panel's ContentsContainerView/ContentsWebView if
  // |is_web_panel_active_| is true.
  ContentsContainerView* GetActiveContentsContainerView() const override;
  ContentsWebView* GetActiveContentsView() const override;
  ContentsContainerView* GetContentsContainerViewFor(
      content::WebContents* web_contents) const override;

  // Wryling split view (2-4 panes; see wryling_split_layout.h).
  wryling::WrylingSplitController* split_controller() {
    return split_controller_.get();
  }
  // The visible panes' contents and final (not mid-animation) bounds, in
  // pane order. One pane when the page isn't split.
  std::vector<content::WebContents*> GetPaneContents() const;
  std::vector<gfx::Rect> GetPaneTargetBounds() const;
  // The arrangement on screen (empty when not split).
  wryling::SplitLayout GetCurrentLayout() const { return current_layout_; }
  int GetPaneGutter() const;
  void OnDividerDragged(int node, bool is_root, double ratio, bool done);
  // Holds panes where they are while a split is being rebuilt, so they can
  // slide to their new places afterwards instead of jumping.
  void HoldPanes(bool hold);
  // A pane's site changed (navigation, favicon): refresh shown labels.
  void OnPaneSiteChanged();

  // How panes show their site (the "wryling.split_view.pane_labels" pref,
  // registered in brave_profile_prefs.cc and picked in Settings > Tabs).
  enum class PaneLabels { kOnHover = 0, kAlways = 1, kCorner = 2, kOff = 3 };

  // Test only: the screen bounds of pane `pane`'s header button (close or
  // separate), if that header is showing.
  std::optional<gfx::Rect> GetPaneHeaderButtonForTesting(int pane,
                                                         bool close) const;
  int GetShownPaneHeaderForTesting() const { return shown_header_; }

  // ui::EventObserver: tracks the pointer (also over web pages) to show a
  // pane's header when it reaches the pane's top edge.
  void OnEvent(const ui::Event& event) override;

  // views::View:
  void Layout(PassKey) override;
  void AddedToWidget() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  friend class SplitViewBrowserTest;
  friend class SpeedReaderWithSplitViewBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(SplitViewBrowserTest, BraveMultiContentsViewTest);
  FRIEND_TEST_ALL_PREFIXES(sidebar::SidebarBrowserWithWebPanelTest,
                           WebPanelTest);

  // MultiContentsView:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;
  void ResetResizeArea() override;
  void UpdateContentsBorderAndOverlay() override;
  void OnWebContentsFocused(views::WebView* web_view) override;
  void ExecuteOnEachVisibleContentsView(
      base::RepeatingCallback<void(ContentsWebView*)> callback) override;

  int GetWebPanelWidth() const;

  std::vector<ContentsContainerView*> contents_container_views_for_testing()
      const {
    return std::vector<ContentsContainerView*>(
        contents_container_views_.begin(), contents_container_views_.end());
  }

  // Lays the panes out from the split's arrangement, animating changes.
  void LayOutWrylingPanes(views::ProposedLayout& layouts) const;
  wryling::SplitLayout LayoutForPanes(
      const std::vector<content::WebContents*>& contents) const;
  void ApplyPaneCornerRadii();

  std::unique_ptr<wryling::WrylingSplitController> split_controller_;
  std::vector<raw_ptr<WrylingSplitDivider>> dividers_;
  gfx::RoundedCornersF contents_corner_radii_;

  // Layout results, kept for hit testing, dividers and the next animation.
  mutable wryling::SplitLayout current_layout_;
  mutable std::vector<gfx::Rect> pane_targets_;
  mutable gfx::Rect pane_region_;
  mutable std::vector<wryling::SplitLayout::Divider> divider_specs_;
  mutable std::vector<content::WebContents*> pane_signature_;
  mutable int pane_signature_layout_ = -1;
  mutable std::map<content::WebContents*, gfx::Rect> shown_rects_;
  mutable std::map<content::WebContents*, gfx::Rect> from_rects_;
  mutable gfx::LinearAnimation pane_animation_{this};
  bool hold_panes_ = false;

  // Slim per-pane headers (site, separate, close), shown on hover of the
  // pane's top edge (bottom edge for a pane with one above it). The pane
  // slides aside to make room, so the header never covers the page.
  void ShowPaneHeader(int pane);
  // Slides the header away; HidePaneHeaderNow() drops it at once.
  void HidePaneHeader();
  void HidePaneHeaderNow();
  void UpdatePaneHeaderBounds();
  // A pane's header dragged: 0 update, 1 drop, 2 cancel.
  void OnPaneHeaderDrag(int pane, int phase, const gfx::Point& screen);
  std::vector<raw_ptr<WrylingPaneHeader>> headers_;
  std::unique_ptr<views::EventMonitor> hover_monitor_;
  // Always-shown labels (kAlways, kCorner): every pane's header, laid out
  // with the panes.
  void UpdateStaticPaneHeaders();
  void SetHeaderSite(int pane, content::WebContents* contents);
  void OnPaneLabelsPrefChanged();
  PaneLabels pane_labels_ = PaneLabels::kOnHover;
  std::unique_ptr<PrefChangeRegistrar> pane_labels_registrar_;
  // The pane whose header is showing or sliding away (kOnHover); -1 for none.
  int shown_header_ = -1;
  gfx::SlideAnimation header_slide_{this};
  bool header_dragging_ = false;
  base::OneShotTimer hide_header_timer_;

  int web_panel_width_ = 0;
  bool web_panel_on_left_ = false;

  // true when web panel is activated.
  bool is_web_panel_active_ = false;
  raw_ptr<BraveContentsContainerView> contents_container_view_for_web_panel_ =
      nullptr;
};

#endif  // BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_BRAVE_MULTI_CONTENTS_VIEW_H_
