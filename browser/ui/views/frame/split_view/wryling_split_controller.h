/* Copyright (c) 2026 The Wryling Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_WRYLING_SPLIT_CONTROLLER_H_
#define BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_WRYLING_SPLIT_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "brave/browser/ui/views/frame/split_view/wryling_split_layout.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/geometry/rect.h"

class BraveMultiContentsView;
class BrowserView;
class TabDragTarget;
class TabStripModel;

namespace tabs {
class TabModel;
}  // namespace tabs

namespace content {
class WebContents;
}  // namespace content

namespace wryling {

class SplitDropTarget;
class SplitDropPreview;

// Per window: the Wryling side of split view. Keeps each split's pane
// arrangement (2-4 panes), handles tabs dropped onto the page, and re-splits
// the remaining panes when one pane of a 3-4 pane split closes.
class WrylingSplitController : public TabStripModelObserver {
 public:
  // A place a dragged tab can land.
  struct DropZone {
    // The tabs on screen, in pane order (one when the page isn't split).
    std::vector<tabs::TabHandle> panes;
    std::optional<split_tabs::SplitTabId> split_id;
    SplitLayout layout;  // The arrangement `panes` are in.
    int pane = 0;        // Which pane the tab is over.
    SplitEdge edge = SplitEdge::kRight;
    gfx::Rect preview;   // In multi-contents-view coordinates.
  };

  WrylingSplitController(BraveMultiContentsView* view,
                         BrowserView* browser_view);
  WrylingSplitController(const WrylingSplitController&) = delete;
  WrylingSplitController& operator=(const WrylingSplitController&) = delete;
  ~WrylingSplitController() override;

  // The arrangement for `split_id` with `pane_count` panes; the root follows
  // the split's own (saved) layout and ratio.
  SplitLayout LayoutFor(split_tabs::SplitTabId split_id,
                        int pane_count,
                        const split_tabs::SplitTabVisualData& visual_data);

  // Divider dragged. The root ratio goes through the tab strip model (it's
  // saved with the session); the others stay here.
  void SetRatio(split_tabs::SplitTabId split_id,
                int node,
                bool is_root,
                double ratio,
                bool done);

  TabDragTarget* GetDropTargetAt(const gfx::Point& point_in_screen);
  bool IsDropTarget(const TabDragTarget* target) const;

  // Works out where `dragged` would land at `point` (view coordinates).
  std::optional<DropZone> ZoneAt(const gfx::Point& point,
                                 content::WebContents* dragged) const;
  void ShowPreview(const std::optional<DropZone>& zone);
  // Puts `dragged` where `zone` says, as one model update.
  void Drop(tabs::TabHandle dragged, const DropZone& zone);

  // When the page that was showing before a tab drag started differs from
  // the dragged tab, shows it again so the tab can be dropped next to it.
  void ShowPageUnderDrag(content::WebContents* dragged);

  // Where the drop preview was when the last tab was dropped, so the new
  // pane can grow out of it.
  std::optional<gfx::Rect> TakeDropOrigin(content::WebContents* contents);

  // `dragged` was dropped onto `target` in the sidebar: they become a split
  // (or `dragged` joins `target`'s split).
  void MergeTabs(tabs::TabHandle dragged,
                 tabs::TabHandle target,
                 SplitEdge edge);

  // While a sidebar tab hovers over `target` to merge: shows `target` (or
  // its split) on the page with the drop preview where the new pane would
  // go, `edge` of `target`'s pane. Null hides it.
  void PreviewMerge(tabs::TabInterface* target, SplitEdge edge);

  // Takes `tab` out of its split without moving or activating it; the other
  // panes stay split if there are at least two.
  void DetachFromSplit(tabs::TabInterface* tab);

  // Takes `contents`'s tab out of its split; the other panes stay split if
  // there are at least two. The separated tab goes right after them.
  void SeparatePane(content::WebContents* contents);

  // Whether `tab` is in this window, and moving one in from elsewhere.
  bool OwnsTab(tabs::TabInterface* tab) const;
  void AdoptTab(std::unique_ptr<tabs::TabModel> tab);

  void RunTestSwitchesIfSet();

  base::WeakPtr<WrylingSplitController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override;

 private:
  TabStripModel* model() const;
  // Moves `tabs` next to each other in this order and splits them.
  std::optional<split_tabs::SplitTabId> SplitInOrder(
      const std::vector<tabs::TabInterface*>& tabs,
      const SplitLayout& layout);
  void ResplitSurvivors(std::vector<tabs::TabHandle> tabs, SplitLayout layout);
  void RunTestStep(std::string spec, int step);

  raw_ptr<BraveMultiContentsView> view_;
  raw_ptr<BrowserView> browser_view_;
  std::unique_ptr<SplitDropTarget> drop_target_;
  raw_ptr<SplitDropPreview> preview_ = nullptr;

  std::map<split_tabs::SplitTabId, SplitLayout> layouts_;
  tabs::TabHandle previous_active_;
  // Set while this class rearranges splits itself.
  bool rearranging_ = false;
  // The dropped tab and where its preview was.
  raw_ptr<content::WebContents> dropped_contents_ = nullptr;
  gfx::Rect dropped_from_;

  base::WeakPtrFactory<WrylingSplitController> weak_factory_{this};
};

// Test only: replays tab drags with synthetic mouse events when
// WRYLING_TEST_DRAG is set (macOS; wryling_split_test_drag_mac.mm).
void RunTestDragIfSet(BrowserView* browser_view);

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_WRYLING_SPLIT_CONTROLLER_H_
