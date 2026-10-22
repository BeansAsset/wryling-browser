/* Copyright (c) 2026 The Wryling Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_WRYLING_SPLIT_LAYOUT_H_
#define BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_WRYLING_SPLIT_LAYOUT_H_

#include <vector>

#include "ui/gfx/geometry/rect.h"

namespace wryling {

// Where a dragged tab lands relative to a pane.
enum class SplitEdge { kLeft, kRight, kTop, kBottom, kReplace };

// The arrangement of a split's panes: a binary tree with up to
// `kMaxPanes` leaves. Leaf i shows the split's i-th tab, so leaves are always
// numbered in reading order (in-order traversal). Inner nodes put their two
// children side by side (kSideBySide) or one above the other (kStacked).
class SplitLayout {
 public:
  static constexpr int kMaxPanes = 4;

  enum class Axis { kSideBySide, kStacked };

  struct Divider {
    int node = -1;
    Axis axis = Axis::kSideBySide;
    gfx::Rect bounds;    // The gutter between the two children.
    gfx::Rect extent;    // Both children together (what the ratio divides).
  };

  SplitLayout();
  SplitLayout(const SplitLayout&);
  SplitLayout& operator=(const SplitLayout&);
  ~SplitLayout();

  // 1: one pane. 2: side by side. 3: one left, two stacked on the right.
  // 4: 2x2 grid.
  static SplitLayout Default(int panes);
  static SplitLayout Two(Axis axis, double ratio);

  int pane_count() const { return pane_count_; }
  bool empty() const { return nodes_.empty(); }

  // Splits `pane` at `edge`; the new pane takes half of it. Returns the new
  // pane's index (panes after it shift up by one). kReplace is a no-op that
  // returns `pane`.
  int Insert(int pane, SplitEdge edge);
  // Removes `pane`; its sibling takes its place.
  void Remove(int pane);
  // Moves `pane` to `edge` of `target` (both are current indices). Returns the
  // moved pane's new index.
  int Move(int pane, int target, SplitEdge edge);

  // Pane rectangles inside `bounds`, `gutter` apart, by pane index. A pane is
  // never squeezed below `min_size` while there is room.
  std::vector<gfx::Rect> PaneBounds(const gfx::Rect& bounds,
                                    int gutter,
                                    int min_size) const;
  std::vector<Divider> Dividers(const gfx::Rect& bounds,
                                int gutter,
                                int min_size) const;

  double ratio(int node) const { return nodes_[node].ratio; }
  // Sets a divider's ratio. In a grid (both halves of the root split the
  // other way) the matching divider moves too, so the grid stays aligned.
  void SetRatio(int node, double ratio);

  // The root's axis and ratio; only meaningful with two or more panes.
  Axis root_axis() const;
  double root_ratio() const;
  void SetRoot(Axis axis, double ratio);

  // Which pane is under `point`, or -1.
  int PaneAt(const gfx::Rect& bounds,
             int gutter,
             int min_size,
             const gfx::Point& point) const;

  bool operator==(const SplitLayout&) const;

 private:
  struct Node {
    bool leaf = true;
    int pane = 0;
    Axis axis = Axis::kSideBySide;
    double ratio = 0.5;
    int first = -1;
    int second = -1;
    int parent = -1;
    bool operator==(const Node&) const = default;
  };

  int LeafNode(int pane) const;
  void Renumber();
  void Walk(int node,
            const gfx::Rect& rect,
            int gutter,
            int min_size,
            std::vector<gfx::Rect>* panes,
            std::vector<Divider>* dividers) const;
  int GridPartner(int node) const;

  std::vector<Node> nodes_;
  int root_ = -1;
  int pane_count_ = 0;
};

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_VIEWS_FRAME_SPLIT_VIEW_WRYLING_SPLIT_LAYOUT_H_
