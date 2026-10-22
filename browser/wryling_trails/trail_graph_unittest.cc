/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/wryling_trails/trail_graph.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "brave/browser/wryling_trails/trail_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wryling_trails {

namespace {

using Kind = Commit::Kind;

// Drives one tab the way the tab helper does: every commit gets a fresh
// NavigationEntry id unless it revisits an entry.
class FakeTab {
 public:
  explicit FakeTab(TrailGraph& graph) : graph_(graph) {}

  // A new entry (link, typed...). Returns its entry id.
  int Go(const std::string& url,
         Kind kind = Kind::kNew,
         const std::u16string& search_terms = u"") {
    Commit commit = Make(url, kind);
    commit.search_terms = search_terms;
    commit.entry_id = next_entry_id_++;
    if (!history_.empty()) {
      history_.resize(index_ + 1);
    }
    history_.push_back({commit.entry_id, url});
    index_ = history_.size() - 1;
    RecordCommit(*graph_, state_, commit);
    return commit.entry_id;
  }

  // Back/forward by `offset` entries, like NavigationController::GoToOffset.
  void GoBy(int offset) {
    index_ += offset;
    Commit commit = Make(history_[index_].url, Kind::kHistory);
    commit.entry_id = history_[index_].entry_id;
    RecordCommit(*graph_, state_, commit);
  }

  void Commit_(Commit commit) {
    commit.entry_id = next_entry_id_++;
    RecordCommit(*graph_, state_, commit);
  }

  Commit Make(const std::string& url, Kind kind) {
    Commit commit;
    commit.kind = kind;
    commit.url = GURL(url);
    commit.time = base::Time::Now();
    return commit;
  }

  const TrailNode* current() const { return graph_->GetNode(state_.cursor); }
  TabState& state() { return state_; }

 private:
  struct Entry {
    int entry_id;
    std::string url;
  };
  const raw_ref<TrailGraph> graph_;
  TabState state_;
  std::vector<Entry> history_;
  size_t index_ = 0;
  // Entry ids are unique across tabs in the browser; keep them apart here.
  static inline int next_entry_id_ = 1;
};

std::string Url(const TrailNode* node) {
  return node ? node->url.spec() : "(none)";
}

}  // namespace

TEST(WrylingTrailGraphTest, LinksInOneTabMakeAChain) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://a.test/", Kind::kStartsTrail);
  tab.Go("https://b.test/");
  tab.Go("https://c.test/");

  ASSERT_EQ(3u, graph.node_count());
  EXPECT_EQ(1u, graph.trail_count());
  const TrailNode* c = tab.current();
  const TrailNode* b = graph.GetNode(c->parent_id);
  const TrailNode* a = graph.GetNode(b->parent_id);
  EXPECT_EQ("https://b.test/", Url(b));
  EXPECT_EQ("https://a.test/", Url(a));
  EXPECT_EQ(kNoNode, a->parent_id);
  EXPECT_EQ(0, a->branch);
  EXPECT_EQ(0, b->branch);
  EXPECT_EQ(0, c->branch);
  EXPECT_EQ(kNoNode, graph.FindBranchPoint(c->id));
}

TEST(WrylingTrailGraphTest, BackThenAnotherLinkBranches) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://a.test/", Kind::kStartsTrail);
  tab.Go("https://b.test/");
  tab.Go("https://c.test/");
  const NodeId c = tab.current()->id;

  // Back to b: no new page.
  tab.GoBy(-1);
  EXPECT_EQ("https://b.test/", Url(tab.current()));
  EXPECT_EQ(3u, graph.node_count());
  const NodeId b = tab.current()->id;

  // Another link from b: a new branch; session history drops c, we don't.
  tab.Go("https://d.test/");
  const TrailNode* d = tab.current();
  EXPECT_EQ(b, d->parent_id);
  EXPECT_EQ(1, d->branch);
  EXPECT_EQ(0, graph.GetNode(c)->branch);
  ASSERT_EQ(4u, graph.node_count());
  EXPECT_EQ(2u, graph.GetNode(b)->children.size());

  // Both lines branched off at b.
  EXPECT_EQ(b, graph.FindBranchPoint(d->id));
  EXPECT_EQ(b, graph.FindBranchPoint(c));

  // A link off the branch keeps the branch.
  tab.Go("https://e.test/");
  EXPECT_EQ(1, tab.current()->branch);
  EXPECT_EQ(b, graph.FindBranchPoint(tab.current()->id));
}

TEST(WrylingTrailGraphTest, BackAndForwardMoveWithoutAdding) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://a.test/", Kind::kStartsTrail);
  tab.Go("https://b.test/");
  tab.Go("https://c.test/");
  tab.GoBy(-2);
  EXPECT_EQ("https://a.test/", Url(tab.current()));
  tab.GoBy(2);
  EXPECT_EQ("https://c.test/", Url(tab.current()));
  tab.GoBy(-1);
  EXPECT_EQ("https://b.test/", Url(tab.current()));
  EXPECT_EQ(3u, graph.node_count());
  EXPECT_EQ(1u, graph.trail_count());
}

TEST(WrylingTrailGraphTest, LinksOpenedInNewTabsHangOffTheOpener) {
  TrailGraph graph;
  FakeTab search(graph);
  search.Go("https://search.test/?q=rail+pass", Kind::kStartsTrail,
            u"rail pass");
  const NodeId root = search.current()->id;

  FakeTab one(graph);
  one.state().opener = search.state().cursor;
  one.Go("https://one.test/");
  FakeTab two(graph);
  two.state().opener = search.state().cursor;
  two.Go("https://two.test/");

  EXPECT_EQ(root, one.current()->parent_id);
  EXPECT_EQ(root, two.current()->parent_id);
  EXPECT_TRUE(one.current()->opened_in_new_tab);
  EXPECT_EQ(0, one.current()->branch);
  EXPECT_EQ(1, two.current()->branch);
  EXPECT_EQ(1u, graph.trail_count());
  EXPECT_EQ(u"rail pass", graph.GetTitle(one.current()->trail_id));
  EXPECT_EQ(root, graph.FindBranchPoint(two.current()->id));

  // The new tab's own links continue from its page, not from the opener.
  one.Go("https://one.test/more");
  EXPECT_EQ("https://one.test/", Url(graph.GetNode(one.current()->parent_id)));
  EXPECT_FALSE(one.current()->opened_in_new_tab);
}

TEST(WrylingTrailGraphTest, TypedNavigationStartsANewTrail) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://a.test/", Kind::kStartsTrail);
  tab.Go("https://b.test/");
  tab.Go("https://news.test/", Kind::kStartsTrail);
  EXPECT_EQ(2u, graph.trail_count());
  EXPECT_EQ(kNoNode, tab.current()->parent_id);
  EXPECT_EQ(u"news.test", graph.GetTitle(tab.current()->trail_id));
  graph.SetTitle(tab.current()->id, u"News");
  EXPECT_EQ(u"News", graph.GetTitle(tab.current()->trail_id));
}

TEST(WrylingTrailGraphTest, RedirectsAreKeptOnThePage) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://a.test/", Kind::kStartsTrail);

  // Server redirects: one page, with the hops remembered.
  Commit commit = tab.Make("https://final.test/", Kind::kNew);
  commit.redirects = {GURL("https://short.test/x"),
                      GURL("https://track.test/y")};
  tab.Commit_(commit);
  ASSERT_EQ(2u, graph.node_count());
  EXPECT_EQ(2u, tab.current()->redirects.size());

  // A client redirect replaces the page in place.
  const NodeId id = tab.current()->id;
  tab.Commit_(tab.Make("https://final.test/landing", Kind::kClientRedirect));
  EXPECT_EQ(2u, graph.node_count());
  EXPECT_EQ(id, tab.current()->id);
  EXPECT_EQ("https://final.test/landing", Url(tab.current()));
  EXPECT_EQ("https://final.test/", tab.current()->redirects.back().spec());
}

TEST(WrylingTrailGraphTest, ReloadsFragmentsAndSelfLinksAddNothing) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://a.test/", Kind::kStartsTrail);
  tab.Commit_(tab.Make("https://a.test/", Kind::kReload));
  tab.Commit_(tab.Make("https://a.test/#section", Kind::kSameDocument));
  tab.Go("https://a.test/");
  EXPECT_EQ(1u, graph.node_count());

  // pushState to another URL counts as a page.
  tab.Commit_(tab.Make("https://a.test/watch?v=2", Kind::kSameDocument));
  EXPECT_EQ(2u, graph.node_count());
}

TEST(WrylingTrailGraphTest, UnrecordableVisitsLeaveNothing) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://a.test/", Kind::kStartsTrail);

  Commit secret = tab.Make("https://secret.test/", Kind::kNew);
  secret.recordable = false;
  tab.Commit_(secret);
  EXPECT_EQ(1u, graph.node_count());
  EXPECT_EQ(nullptr, tab.current());

  // What follows can't be tied to the page before the private visit.
  tab.Go("https://after.test/");
  EXPECT_EQ(kNoNode, tab.current()->parent_id);
  EXPECT_EQ(2u, graph.trail_count());
}

TEST(WrylingTrailGraphTest, PrivateProfilesAndPagesAreNotRecorded) {
  EXPECT_TRUE(IsRecordable(GURL("https://a.test/"), {}));
  EXPECT_TRUE(IsRecordable(GURL("http://a.test/"), {}));
  EXPECT_FALSE(IsRecordable(GURL("https://a.test/"), {.off_the_record = true}));
  EXPECT_FALSE(IsRecordable(GURL("https://a.test/"),
                            {.requested_off_the_record = true}));
  EXPECT_FALSE(
      IsRecordable(GURL("https://a.test/"), {.history_disabled = true}));
  EXPECT_FALSE(IsRecordable(GURL("chrome://history/"), {}));
  EXPECT_FALSE(IsRecordable(GURL("about:blank"), {}));
  EXPECT_FALSE(IsRecordable(GURL("file:///etc/hosts"), {}));
  EXPECT_FALSE(IsRecordable(GURL("data:text/html,hi"), {}));
}

TEST(WrylingTrailGraphTest, RestoreLandsOnTheExistingPage) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://a.test/", Kind::kStartsTrail);
  tab.Go("https://b.test/");
  const NodeId b = tab.current()->id;
  tab.Go("https://c.test/");

  // Reopened from History in a fresh tab: lands on b, then branches from it.
  FakeTab restored(graph);
  restored.state().expected_restore = b;
  restored.Go("https://b.test/", Kind::kStartsTrail);
  EXPECT_EQ(b, restored.current()->id);
  EXPECT_EQ(3u, graph.node_count());
  restored.Go("https://d.test/");
  EXPECT_EQ(b, restored.current()->parent_id);
  EXPECT_EQ(1, restored.current()->branch);

  // A restore that ends up elsewhere is an ordinary visit.
  FakeTab moved(graph);
  moved.state().expected_restore = b;
  moved.Go("https://login.test/", Kind::kStartsTrail);
  EXPECT_NE(b, moved.current()->id);
  EXPECT_EQ(kNoNode, moved.state().expected_restore);
}

TEST(WrylingTrailGraphTest, FormSubmissionsAreMarked) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://shop.test/", Kind::kStartsTrail);
  Commit post = tab.Make("https://shop.test/checkout", Kind::kNew);
  post.form_submission = true;
  tab.Commit_(post);
  EXPECT_TRUE(tab.current()->form_submission);
  EXPECT_FALSE(graph.GetNode(tab.current()->parent_id)->form_submission);
}

TEST(WrylingTrailGraphTest, SurvivesSaveAndLoad) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://search.test/?q=kyoto", Kind::kStartsTrail, u"kyoto");
  tab.Go("https://b.test/");
  graph.SetTitle(tab.current()->id, u"B page");
  graph.SetFavicon(tab.current()->id, GURL("https://b.test/favicon.ico"));
  tab.GoBy(-1);
  Commit post = tab.Make("https://c.test/", Kind::kNew);
  post.form_submission = true;
  post.redirects = {GURL("https://r.test/")};
  tab.Commit_(post);

  // Through JSON text, as on disk.
  std::string json;
  ASSERT_TRUE(base::JSONWriter::Write(graph.ToValue(), &json));
  std::optional<base::Value> parsed =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed && parsed->is_dict());
  TrailGraph loaded = TrailGraph::FromValue(parsed->GetDict());

  ASSERT_EQ(graph.node_count(), loaded.node_count());
  ASSERT_EQ(1u, loaded.trail_count());
  const TrailId trail = loaded.TrailsByRecency().front()->id;
  EXPECT_EQ(u"kyoto", loaded.GetTitle(trail));
  const NodeId b = loaded.FindLatestWithUrl(GURL("https://b.test/"));
  const NodeId c = loaded.FindLatestWithUrl(GURL("https://c.test/"));
  EXPECT_EQ(u"B page", loaded.GetNode(b)->title);
  EXPECT_EQ("https://b.test/favicon.ico",
            loaded.GetNode(b)->favicon_url.spec());
  EXPECT_TRUE(loaded.GetNode(c)->form_submission);
  EXPECT_EQ(1u, loaded.GetNode(c)->redirects.size());
  EXPECT_EQ(1, loaded.GetNode(c)->branch);
  const NodeId root = loaded.GetNode(b)->parent_id;
  EXPECT_EQ(2u, loaded.GetNode(root)->children.size());
  EXPECT_EQ(root, loaded.FindBranchPoint(c));

  // New pages after a restart get fresh ids and branches.
  TabState state;
  Commit restored;
  restored.kind = Kind::kRestored;
  restored.url = GURL("https://c.test/");
  restored.entry_id = 9001;
  RecordCommit(loaded, state, restored);
  EXPECT_EQ(c, state.cursor);
  Commit next;
  next.url = GURL("https://d.test/");
  next.entry_id = 9002;
  RecordCommit(loaded, state, next);
  EXPECT_GT(state.cursor, c);
  EXPECT_EQ(c, loaded.GetNode(state.cursor)->parent_id);
  EXPECT_EQ(1, loaded.GetNode(state.cursor)->branch);
}

TEST(WrylingTrailGraphTest, LoadRejectsBrokenData) {
  std::optional<base::Value> parsed =
      base::JSONReader::Read(R"({
    "version": 1,
    "trails": [{"id": "1"}],
    "nodes": [
      {"id": "1", "trail": "1", "parent": "0", "url": "https://a.test/"},
      {"id": "2", "trail": "7", "parent": "1", "url": "https://b.test/"},
      {"id": "3", "trail": "1", "parent": "9", "url": "https://c.test/"},
      {"id": "4", "trail": "1", "parent": "1", "url": "not a url"},
      "junk"
    ]})",
                             base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed);
  TrailGraph graph = TrailGraph::FromValue(parsed->GetDict());
  EXPECT_EQ(2u, graph.node_count());
  EXPECT_EQ(kNoNode, graph.GetNode(3)->parent_id);
  EXPECT_EQ(0u, TrailGraph::FromValue(base::DictValue()).node_count());
}

TEST(WrylingTrailGraphTest, RemovingPagesKeepsTheTreeConnected) {
  TrailGraph graph;
  FakeTab tab(graph);
  tab.Go("https://a.test/", Kind::kStartsTrail);
  tab.Go("https://b.test/");
  tab.Go("https://c.test/");
  const NodeId a = graph.FindLatestWithUrl(GURL("https://a.test/"));

  EXPECT_EQ(1u, graph.RemoveNodesIf([](const TrailNode& node) {
    return node.url.host() == "b.test";
  }));
  EXPECT_EQ(a, tab.current()->parent_id);

  // The tab's page went away: its next link starts over.
  graph.RemoveNodesIf(
      [](const TrailNode& node) { return node.url.host() == "c.test"; });
  tab.Go("https://d.test/");
  EXPECT_EQ(kNoNode, tab.current()->parent_id);

  graph.Clear();
  EXPECT_EQ(0u, graph.node_count());
  EXPECT_EQ(0u, graph.trail_count());
}

TEST(WrylingTrailGraphTest, DeletedRedirectHopsAreForgotten) {
  TrailGraph graph;
  Visit visit;
  visit.url = GURL("https://final.test/");
  visit.redirects = {GURL("https://track.test/"), GURL("https://keep.test/")};
  const NodeId id = graph.AddVisit(visit, kNoNode);
  EXPECT_EQ(1u, graph.RemoveRedirectsIf([](const TrailNode&, const GURL& url) {
    return url.host() == "track.test";
  }));
  ASSERT_EQ(1u, graph.GetNode(id)->redirects.size());
  EXPECT_EQ("https://keep.test/", graph.GetNode(id)->redirects[0].spec());
}

TEST(WrylingTrailGraphTest, OldTrailsArePruned) {
  TrailGraph graph;
  Visit old_visit;
  old_visit.url = GURL("https://old.test/");
  old_visit.time = base::Time::Now() - base::Days(200);
  graph.AddVisit(old_visit, kNoNode);
  Visit new_visit;
  new_visit.url = GURL("https://new.test/");
  new_visit.time = base::Time::Now();
  graph.AddVisit(new_visit, kNoNode);
  graph.RemoveTrailsBefore(base::Time::Now() - base::Days(90));
  EXPECT_EQ(1u, graph.trail_count());
  EXPECT_EQ(kNoNode, graph.FindLatestWithUrl(GURL("https://old.test/")));
}

}  // namespace wryling_trails
