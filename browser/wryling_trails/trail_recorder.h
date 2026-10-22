/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_WRYLING_TRAILS_TRAIL_RECORDER_H_
#define BRAVE_BROWSER_WRYLING_TRAILS_TRAIL_RECORDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "brave/browser/wryling_trails/trail_graph.h"
#include "url/gurl.h"

namespace wryling_trails {

// One committed main-frame navigation, as the tab helper saw it.
struct Commit {
  enum class Kind {
    // A link, form or script took the tab somewhere new.
    kNew,
    // Typed, searched, bookmark or other browser-started navigation: the
    // start of a new trail.
    kStartsTrail,
    // Back/forward to an entry the tab already had.
    kHistory,
    kReload,
    // Session restore after a restart or reopening a closed tab.
    kRestored,
    // The page replaced itself (meta refresh, location.replace).
    kClientRedirect,
    // pushState, replaceState or fragment change in the same document.
    kSameDocument,
  };

  Commit();
  Commit(const Commit&);
  Commit& operator=(const Commit&);
  ~Commit();

  Kind kind = Kind::kNew;
  GURL url;
  // Server redirects passed through before `url` (excluding `url`).
  std::vector<GURL> redirects;
  base::Time time;
  // NavigationEntry::GetUniqueID() of the committed entry.
  int entry_id = 0;
  bool form_submission = false;
  std::u16string title;
  std::u16string search_terms;
  // False for pages that must not be recorded: non-web schemes, pages that
  // asked to be off the record, or with history saving turned off.
  bool recordable = true;
};

// Where one tab is in the graph.
struct TabState {
  TabState();
  TabState(const TabState&);
  TabState& operator=(const TabState&);
  ~TabState();

  // The page the tab shows now, if it is in the graph.
  NodeId cursor = kNoNode;
  // The page (in another tab) that opened this tab; used by its first commit.
  NodeId opener = kNoNode;
  // A restore asked this tab to load `expected_restore`; when it commits the
  // tab lands on that existing page instead of adding a new one.
  NodeId expected_restore = kNoNode;
  // NavigationEntry unique id -> page, so back/forward finds the right node.
  std::map<int, NodeId> entry_nodes;
};

// Why a visit might have to stay out of the graph.
struct RecordContext {
  // Incognito, guest and Tor profiles.
  bool off_the_record = false;
  // The site asked for an off-the-record visit (Brave Request-OTR).
  bool requested_off_the_record = false;
  // History saving is turned off by policy or pref.
  bool history_disabled = false;
};

// Only ordinary web pages in an ordinary profile enter a trail.
bool IsRecordable(const GURL& url, const RecordContext& context);

// Applies a committed navigation to the graph and moves the tab's cursor.
// Returns true when the graph itself changed.
bool RecordCommit(TrailGraph& graph, TabState& tab, const Commit& commit);

// The entry (of `tab`) most recently mapped to `node`, or 0.
int EntryForNode(const TabState& tab, NodeId node);

}  // namespace wryling_trails

#endif  // BRAVE_BROWSER_WRYLING_TRAILS_TRAIL_RECORDER_H_
