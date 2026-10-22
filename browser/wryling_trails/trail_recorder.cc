/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/wryling_trails/trail_recorder.h"

#include <algorithm>
#include <utility>

namespace wryling_trails {

namespace {

bool SameUrl(const GURL& a, const GURL& b) {
  return a.EqualsIgnoringRef(b);
}

// Whether a restore of `node` that committed `commit` landed on it, allowing
// for redirects on either visit.
bool LandsOn(const TrailNode& node, const Commit& commit) {
  if (SameUrl(node.url, commit.url)) {
    return true;
  }
  return std::ranges::any_of(
             commit.redirects,
             [&](const GURL& url) { return SameUrl(node.url, url); }) ||
         std::ranges::any_of(node.redirects, [&](const GURL& url) {
           return SameUrl(url, commit.url);
         });
}

}  // namespace

Commit::Commit() = default;
Commit::Commit(const Commit&) = default;
Commit& Commit::operator=(const Commit&) = default;
Commit::~Commit() = default;

TabState::TabState() = default;
TabState::TabState(const TabState&) = default;
TabState& TabState::operator=(const TabState&) = default;
TabState::~TabState() = default;

bool IsRecordable(const GURL& url, const RecordContext& context) {
  return !context.off_the_record && !context.requested_off_the_record &&
         !context.history_disabled && url.SchemeIsHTTPOrHTTPS();
}

bool RecordCommit(TrailGraph& graph, TabState& tab, const Commit& commit) {
  // Pages can disappear under a tab (history deletion, pruning).
  if (!graph.GetNode(tab.cursor)) {
    tab.cursor = kNoNode;
  }
  if (!graph.GetNode(tab.opener)) {
    tab.opener = kNoNode;
  }

  if (!commit.recordable) {
    // Nothing reached from an unrecorded page may hang off the page before
    // it, so the tab starts over.
    tab.cursor = kNoNode;
    tab.opener = kNoNode;
    tab.expected_restore = kNoNode;
    return false;
  }

  auto map_entry = [&](NodeId node) {
    if (commit.entry_id) {
      tab.entry_nodes[commit.entry_id] = node;
    }
  };
  auto land_on = [&](NodeId node) {
    tab.cursor = node;
    tab.opener = kNoNode;
    map_entry(node);
    graph.MarkVisited(node, commit.time);
  };

  if (tab.expected_restore != kNoNode &&
      commit.kind != Commit::Kind::kSameDocument) {
    const NodeId expected = std::exchange(tab.expected_restore, kNoNode);
    const TrailNode* node = graph.GetNode(expected);
    if (node && LandsOn(*node, commit)) {
      land_on(expected);
      return true;
    }
  }

  NodeId mapped = kNoNode;
  if (auto it = tab.entry_nodes.find(commit.entry_id);
      commit.entry_id && it != tab.entry_nodes.end() &&
      graph.GetNode(it->second)) {
    mapped = it->second;
  }
  const TrailNode* cursor = graph.GetNode(tab.cursor);

  Commit::Kind kind = commit.kind;
  switch (kind) {
    case Commit::Kind::kSameDocument:
      if (mapped) {
        tab.cursor = mapped;
        return false;
      }
      if (cursor && SameUrl(cursor->url, commit.url)) {
        map_entry(cursor->id);
        return false;
      }
      // pushState to a different URL: a new page as far as the user knows.
      kind = Commit::Kind::kNew;
      break;
    case Commit::Kind::kReload:
      if (mapped) {
        tab.cursor = mapped;
        return false;
      }
      if (cursor && SameUrl(cursor->url, commit.url)) {
        map_entry(cursor->id);
        return false;
      }
      [[fallthrough]];
    case Commit::Kind::kHistory:
    case Commit::Kind::kRestored: {
      const NodeId node = mapped ? mapped : graph.FindLatestWithUrl(commit.url);
      if (node) {
        land_on(node);
        return true;
      }
      // An entry from before recording began: start a trail from it.
      kind = Commit::Kind::kStartsTrail;
      break;
    }
    case Commit::Kind::kClientRedirect:
      if (cursor) {
        const bool changed = graph.Redirect(cursor->id, commit.url);
        map_entry(cursor->id);
        return changed;
      }
      kind = Commit::Kind::kNew;
      break;
    case Commit::Kind::kNew:
    case Commit::Kind::kStartsTrail:
      break;
  }

  NodeId parent = kNoNode;
  bool opened_in_new_tab = false;
  if (kind == Commit::Kind::kNew) {
    if (cursor) {
      parent = cursor->id;
    } else if (tab.opener) {
      parent = tab.opener;
      opened_in_new_tab = true;
    }
  }
  // A link to the page already shown is not a step anywhere.
  if (cursor && parent == cursor->id && SameUrl(cursor->url, commit.url) &&
      !commit.form_submission) {
    map_entry(cursor->id);
    return false;
  }

  Visit visit;
  visit.url = commit.url;
  visit.title = commit.title;
  visit.time = commit.time;
  visit.form_submission = commit.form_submission;
  visit.opened_in_new_tab = opened_in_new_tab;
  visit.redirects = commit.redirects;
  visit.search_terms = commit.search_terms;
  land_on(graph.AddVisit(visit, parent));
  return true;
}

int EntryForNode(const TabState& tab, NodeId node) {
  int best = 0;
  for (const auto& [entry_id, mapped] : tab.entry_nodes) {
    // Unique ids grow, so the last match is the most recent entry.
    if (mapped == node) {
      best = entry_id;
    }
  }
  return best;
}

}  // namespace wryling_trails
