/* Copyright (c) 2026 The Wryling Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/frame/split_view/wryling_split_layout.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/check.h"
#include "base/check_op.h"

namespace wryling {

SplitLayout::SplitLayout() = default;
SplitLayout::SplitLayout(const SplitLayout&) = default;
SplitLayout& SplitLayout::operator=(const SplitLayout&) = default;
SplitLayout::~SplitLayout() = default;

// static
SplitLayout SplitLayout::Default(int panes) {
  SplitLayout layout;
  if (panes <= 0) {
    return layout;
  }
  layout.nodes_.push_back(Node());
  layout.root_ = 0;
  layout.pane_count_ = 1;
  if (panes >= 2) {
    layout.Insert(0, SplitEdge::kRight);
  }
  if (panes >= 3) {
    layout.Insert(1, SplitEdge::kBottom);
  }
  if (panes >= 4) {
    layout.Insert(0, SplitEdge::kBottom);
  }
  return layout;
}

// static
SplitLayout SplitLayout::Two(Axis axis, double ratio) {
  SplitLayout layout = Default(2);
  layout.SetRoot(axis, ratio);
  return layout;
}

int SplitLayout::LeafNode(int pane) const {
  for (size_t i = 0; i < nodes_.size(); ++i) {
    if (nodes_[i].leaf && nodes_[i].pane == pane) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void SplitLayout::Renumber() {
  // Rebuilds the node list from the root, dropping unreachable nodes, and
  // numbers the leaves in reading order.
  std::vector<Node> old = std::move(nodes_);
  nodes_.clear();
  pane_count_ = 0;
  if (root_ < 0) {
    return;
  }
  auto copy = [&](auto& self, int index, int parent) -> int {
    const int at = static_cast<int>(nodes_.size());
    nodes_.push_back(old[index]);
    nodes_[at].parent = parent;
    if (nodes_[at].leaf) {
      nodes_[at].pane = pane_count_++;
      nodes_[at].first = nodes_[at].second = -1;
      return at;
    }
    const int first = self(self, old[index].first, at);
    const int second = self(self, old[index].second, at);
    nodes_[at].first = first;
    nodes_[at].second = second;
    return at;
  };
  root_ = copy(copy, root_, -1);
}

int SplitLayout::Insert(int pane, SplitEdge edge) {
  if (edge == SplitEdge::kReplace) {
    return pane;
  }
  if (root_ < 0) {
    nodes_.push_back(Node());
    root_ = 0;
    pane_count_ = 1;
    return 0;
  }
  CHECK_LT(pane_count_, kMaxPanes);
  const int leaf = LeafNode(pane);
  CHECK_GE(leaf, 0);

  // `leaf` becomes the inner node; its old content moves to a new child.
  Node existing = nodes_[leaf];
  Node added;
  added.pane = -1;  // Marks the new pane until renumbered.
  const int existing_index = static_cast<int>(nodes_.size());
  nodes_.push_back(existing);
  const int added_index = static_cast<int>(nodes_.size());
  nodes_.push_back(added);

  Node& inner = nodes_[leaf];
  inner.leaf = false;
  inner.ratio = 0.5;
  inner.axis = (edge == SplitEdge::kLeft || edge == SplitEdge::kRight)
                   ? Axis::kSideBySide
                   : Axis::kStacked;
  const bool added_first = edge == SplitEdge::kLeft || edge == SplitEdge::kTop;
  inner.first = added_first ? added_index : existing_index;
  inner.second = added_first ? existing_index : added_index;

  Renumber();
  // Leaves are numbered in reading order, so the two halves of the old pane
  // are `pane` and `pane + 1`.
  return added_first ? pane : pane + 1;
}

void SplitLayout::Remove(int pane) {
  const int leaf = LeafNode(pane);
  if (leaf < 0) {
    return;
  }
  const int parent = nodes_[leaf].parent;
  if (parent < 0) {
    nodes_.clear();
    root_ = -1;
    pane_count_ = 0;
    return;
  }
  const int sibling =
      nodes_[parent].first == leaf ? nodes_[parent].second
                                   : nodes_[parent].first;
  // The sibling takes the parent's place.
  const int grandparent = nodes_[parent].parent;
  nodes_[sibling].parent = grandparent;
  if (grandparent < 0) {
    root_ = sibling;
  } else if (nodes_[grandparent].first == parent) {
    nodes_[grandparent].first = sibling;
  } else {
    nodes_[grandparent].second = sibling;
  }
  Renumber();
}

int SplitLayout::Move(int pane, int target, SplitEdge edge) {
  if (pane == target || edge == SplitEdge::kReplace) {
    return pane;
  }
  Remove(pane);
  if (target > pane) {
    --target;
  }
  return Insert(target, edge);
}

void SplitLayout::Walk(int node,
                       const gfx::Rect& rect,
                       int gutter,
                       int min_size,
                       std::vector<gfx::Rect>* panes,
                       std::vector<Divider>* dividers) const {
  const Node& n = nodes_[node];
  if (n.leaf) {
    if (panes) {
      (*panes)[n.pane] = rect;
    }
    return;
  }
  const bool side = n.axis == Axis::kSideBySide;
  const int total = (side ? rect.width() : rect.height()) - gutter;
  int first = static_cast<int>(std::lround(n.ratio * total));
  if (total >= 2 * min_size) {
    first = std::clamp(first, min_size, total - min_size);
  } else {
    first = std::clamp(first, 0, std::max(0, total));
  }
  const int second = std::max(0, total - first);

  gfx::Rect a = rect, g = rect, b = rect;
  if (side) {
    a.set_width(first);
    g.set_x(a.right());
    g.set_width(gutter);
    b.set_x(g.right());
    b.set_width(second);
  } else {
    a.set_height(first);
    g.set_y(a.bottom());
    g.set_height(gutter);
    b.set_y(g.bottom());
    b.set_height(second);
  }
  if (dividers) {
    dividers->push_back({node, n.axis, g, rect});
  }
  Walk(n.first, a, gutter, min_size, panes, dividers);
  Walk(n.second, b, gutter, min_size, panes, dividers);
}

std::vector<gfx::Rect> SplitLayout::PaneBounds(const gfx::Rect& bounds,
                                               int gutter,
                                               int min_size) const {
  std::vector<gfx::Rect> panes(pane_count_);
  if (root_ >= 0) {
    Walk(root_, bounds, gutter, min_size, &panes, nullptr);
  }
  return panes;
}

std::vector<SplitLayout::Divider> SplitLayout::Dividers(
    const gfx::Rect& bounds,
    int gutter,
    int min_size) const {
  std::vector<Divider> dividers;
  if (root_ >= 0) {
    Walk(root_, bounds, gutter, min_size, nullptr, &dividers);
  }
  return dividers;
}

int SplitLayout::GridPartner(int node) const {
  if (root_ < 0 || nodes_[root_].leaf) {
    return -1;
  }
  const Node& root = nodes_[root_];
  const Node& a = nodes_[root.first];
  const Node& b = nodes_[root.second];
  if (a.leaf || b.leaf || a.axis != b.axis || a.axis == root.axis) {
    return -1;
  }
  if (node == root.first) {
    return root.second;
  }
  if (node == root.second) {
    return root.first;
  }
  return -1;
}

void SplitLayout::SetRatio(int node, double ratio) {
  if (node < 0 || node >= static_cast<int>(nodes_.size()) ||
      nodes_[node].leaf) {
    return;
  }
  ratio = std::clamp(ratio, 0.0, 1.0);
  nodes_[node].ratio = ratio;
  if (const int partner = GridPartner(node); partner >= 0) {
    nodes_[partner].ratio = ratio;
  }
}

SplitLayout::Axis SplitLayout::root_axis() const {
  return root_ >= 0 ? nodes_[root_].axis : Axis::kSideBySide;
}

double SplitLayout::root_ratio() const {
  return root_ >= 0 ? nodes_[root_].ratio : 0.5;
}

void SplitLayout::SetRoot(Axis axis, double ratio) {
  if (root_ < 0 || nodes_[root_].leaf) {
    return;
  }
  nodes_[root_].axis = axis;
  nodes_[root_].ratio = std::clamp(ratio, 0.0, 1.0);
}

int SplitLayout::PaneAt(const gfx::Rect& bounds,
                        int gutter,
                        int min_size,
                        const gfx::Point& point) const {
  const std::vector<gfx::Rect> panes = PaneBounds(bounds, gutter, min_size);
  int best = -1;
  int best_distance = std::numeric_limits<int>::max();
  for (size_t i = 0; i < panes.size(); ++i) {
    if (panes[i].Contains(point)) {
      return static_cast<int>(i);
    }
    const int dx = std::max({panes[i].x() - point.x(), 0,
                             point.x() - panes[i].right()});
    const int dy = std::max({panes[i].y() - point.y(), 0,
                             point.y() - panes[i].bottom()});
    if (dx + dy < best_distance) {
      best_distance = dx + dy;
      best = static_cast<int>(i);
    }
  }
  return bounds.Contains(point) ? best : -1;
}

bool SplitLayout::operator==(const SplitLayout& other) const {
  return root_ == other.root_ && pane_count_ == other.pane_count_ &&
         nodes_ == other.nodes_;
}

}  // namespace wryling
