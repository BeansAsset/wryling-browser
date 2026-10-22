/* Copyright (c) 2026 The Wryling Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_WRYLING_SPLIT_DROP_H_
#define BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_WRYLING_SPLIT_DROP_H_

// Small on purpose: included by Chromium's browser_view.cc and
// tab_drag_controller.cc.

#include <optional>

#include "ui/gfx/geometry/rect.h"

class TabDragContext;
class TabDragTarget;
struct DragSessionData;

namespace views {
class View;
}  // namespace views

namespace gfx {
class Point;
}  // namespace gfx

namespace wryling {

// The page as a drop target for a tab dragged from the sidebar, or null when
// `point_in_screen` isn't over the page or splitting isn't possible there.
// `multi_contents_view` is the window's (Brave)MultiContentsView; a plain
// views::View here because Brave renames MultiContentsView in some files.
TabDragTarget* GetSplitDropTarget(views::View* multi_contents_view,
                                  const gfx::Point& point_in_screen);

// True for the target above. While it holds a drag, the dragged tab stays
// where it is in the sidebar instead of following the pointer.
bool IsSplitDropTarget(const TabDragTarget* target);

// Wryling's tab list fills a tall sidebar. Chromium only counts a dragged tab
// as "over the tab list" near the existing rows, so a torn-off window could
// hardly be dropped back. Returns the whole sidebar's screen bounds when
// `tab_strip` sits in it, otherwise nothing (use Chromium's rule).
std::optional<gfx::Rect> GetSidebarDragArea(const views::View* tab_strip);

// Dragging a sidebar tab onto another tab merges them into a split (like
// dropping a file into a folder). Called on every drag update with the tab
// strip the tabs are attached to (or null when that doesn't apply, e.g. over
// the page). Returns true while the pointer is over the middle of another
// tab's row: that row is highlighted and the dragged tab should not move.
bool UpdateSidebarMergeTarget(TabDragContext* context,
                              const DragSessionData& drag_data,
                              const gfx::Point& point_in_screen);

// Called when a tab drag starts (past the drag threshold, so not for a
// click). A single tab dragged from a split leaves it; the others stay split
// if at least two are left.
void DetachDraggedTabFromSplit(TabDragContext* context,
                               const DragSessionData& drag_data);

// Called when a tab drag ends. If it ended over a merge target and wasn't
// cancelled, the two tabs become one split.
void FinishSidebarMerge(bool completed);

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_WRYLING_SPLIT_DROP_H_
