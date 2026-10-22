/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_WRYLING_TRAILS_TRAIL_GRAPH_H_
#define BRAVE_BROWSER_WRYLING_TRAILS_TRAIL_GRAPH_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/time/time.h"
#include "base/values.h"
#include "url/gurl.h"

// Wryling Trails: a record of which page led to which, kept above Chromium's
// own history. Session history forgets forward entries when the user goes
// back and takes another link; a trail keeps both lines as branches.
namespace wryling_trails {

using NodeId = int64_t;
using TrailId = int64_t;
inline constexpr NodeId kNoNode = 0;

// One committed page visit.
struct TrailNode {
  TrailNode();
  TrailNode(const TrailNode&);
  TrailNode& operator=(const TrailNode&);
  ~TrailNode();

  NodeId id = kNoNode;
  TrailId trail_id = 0;
  // The page this one was reached from. kNoNode for the page that started the
  // trail (or, after a deletion, a page whose parent was removed).
  NodeId parent_id = kNoNode;
  GURL url;
  std::u16string title;
  GURL favicon_url;
  base::Time time;
  // Pages on one line of exploration share a branch. The first page a page
  // leads to continues its branch; every later one starts a new branch.
  int branch = 0;
  // Reached by a form POST: it can't be recreated, only visited afresh.
  bool form_submission = false;
  bool opened_in_new_tab = false;
  // URLs the visit passed through before landing on `url`.
  std::vector<GURL> redirects;
  // Derived (not stored): pages this one led to, in visit order.
  std::vector<NodeId> children;
};

struct Trail {
  Trail();
  Trail(const Trail&);
  Trail& operator=(const Trail&);
  ~Trail();

  TrailId id = 0;
  // Set when the trail began on a search results page.
  std::u16string search_terms;
  base::Time last_visit;
  int next_branch = 1;
  // Derived: the trail's pages in visit order.
  std::vector<NodeId> nodes;
};

// A page visit to record.
struct Visit {
  Visit();
  Visit(const Visit&);
  Visit& operator=(const Visit&);
  ~Visit();

  GURL url;
  std::u16string title;
  base::Time time;
  bool form_submission = false;
  bool opened_in_new_tab = false;
  std::vector<GURL> redirects;
  std::u16string search_terms;
};

class TrailGraph {
 public:
  // Oldest trails are dropped past this many pages.
  static constexpr size_t kMaxNodes = 20000;

  TrailGraph();
  TrailGraph(TrailGraph&&);
  TrailGraph& operator=(TrailGraph&&);
  TrailGraph(const TrailGraph&) = delete;
  TrailGraph& operator=(const TrailGraph&) = delete;
  ~TrailGraph();

  // Records `visit` as reached from `parent`; kNoNode starts a new trail.
  NodeId AddVisit(const Visit& visit, NodeId parent);
  bool SetTitle(NodeId id, const std::u16string& title);
  bool SetFavicon(NodeId id, const GURL& favicon_url);
  // The page at `id` sent the tab on to `url` without a new history entry
  // (a client redirect): the node becomes `url` and remembers where it was.
  bool Redirect(NodeId id, const GURL& url);
  // The user returned to `id` (back/forward, restore): keeps its trail recent.
  void MarkVisited(NodeId id, base::Time time);

  const TrailNode* GetNode(NodeId id) const;
  const Trail* GetTrail(TrailId id) const;
  // The trail's name: its search terms, else its first page's title or host.
  std::u16string GetTitle(TrailId id) const;
  // Where the line leading to `id` branched off: the nearest ancestor that led
  // to more than one page. kNoNode when the line never branched.
  NodeId FindBranchPoint(NodeId id) const;
  // The most recent page at `url` (ignoring the fragment), or kNoNode.
  NodeId FindLatestWithUrl(const GURL& url) const;
  // Trails, most recently visited first.
  std::vector<const Trail*> TrailsByRecency() const;

  // Removes matching pages. Their children move up to the removed page's
  // parent; trails left empty go too. Returns the number removed.
  size_t RemoveNodesIf(base::FunctionRef<bool(const TrailNode&)> pred);
  // Forgets redirect hops matching `pred`. Returns the number removed.
  size_t RemoveRedirectsIf(
      base::FunctionRef<bool(const TrailNode&, const GURL&)> pred);
  bool RemoveTrail(TrailId id);
  // Drops trails not visited since `cutoff`.
  void RemoveTrailsBefore(base::Time cutoff);
  void Clear();

  size_t node_count() const { return nodes_.size(); }
  size_t trail_count() const { return trails_.size(); }

  base::DictValue ToValue() const;
  static TrailGraph FromValue(const base::DictValue& value);

 private:
  void RebuildDerived();
  void EnforceLimit();

  std::map<NodeId, TrailNode> nodes_;
  std::map<TrailId, Trail> trails_;
  NodeId next_node_id_ = 1;
  TrailId next_trail_id_ = 1;
};

}  // namespace wryling_trails

#endif  // BRAVE_BROWSER_WRYLING_TRAILS_TRAIL_GRAPH_H_
