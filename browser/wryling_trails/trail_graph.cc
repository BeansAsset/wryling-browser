/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/wryling_trails/trail_graph.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"

namespace wryling_trails {

namespace {

constexpr int kFormatVersion = 1;

GURL WithoutRef(const GURL& url) {
  if (!url.has_ref()) {
    return url;
  }
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

std::optional<int64_t> GetId(const base::DictValue& dict,
                             std::string_view key) {
  return base::ValueToInt64(dict.Find(key));
}

}  // namespace

TrailNode::TrailNode() = default;
TrailNode::TrailNode(const TrailNode&) = default;
TrailNode& TrailNode::operator=(const TrailNode&) = default;
TrailNode::~TrailNode() = default;

Trail::Trail() = default;
Trail::Trail(const Trail&) = default;
Trail& Trail::operator=(const Trail&) = default;
Trail::~Trail() = default;

Visit::Visit() = default;
Visit::Visit(const Visit&) = default;
Visit& Visit::operator=(const Visit&) = default;
Visit::~Visit() = default;

TrailGraph::TrailGraph() = default;
TrailGraph::TrailGraph(TrailGraph&&) = default;
TrailGraph& TrailGraph::operator=(TrailGraph&&) = default;
TrailGraph::~TrailGraph() = default;

NodeId TrailGraph::AddVisit(const Visit& visit, NodeId parent) {
  auto parent_it = nodes_.find(parent);
  TrailNode node;
  node.id = next_node_id_++;
  node.url = visit.url;
  node.title = visit.title;
  node.time = visit.time;
  node.form_submission = visit.form_submission;
  node.opened_in_new_tab = visit.opened_in_new_tab;
  node.redirects = visit.redirects;

  Trail* trail = nullptr;
  if (parent_it == nodes_.end()) {
    Trail new_trail;
    new_trail.id = next_trail_id_++;
    new_trail.search_terms = visit.search_terms;
    trail = &trails_.emplace(new_trail.id, std::move(new_trail)).first->second;
  } else {
    TrailNode& parent_node = parent_it->second;
    trail = &trails_.at(parent_node.trail_id);
    node.parent_id = parent_node.id;
    node.branch = parent_node.children.empty() ? parent_node.branch
                                               : trail->next_branch++;
    parent_node.children.push_back(node.id);
  }
  node.trail_id = trail->id;
  trail->nodes.push_back(node.id);
  trail->last_visit = std::max(trail->last_visit, visit.time);

  const NodeId id = node.id;
  nodes_.emplace(id, std::move(node));
  EnforceLimit();
  return id;
}

bool TrailGraph::SetTitle(NodeId id, const std::u16string& title) {
  auto it = nodes_.find(id);
  if (it == nodes_.end() || it->second.title == title) {
    return false;
  }
  it->second.title = title;
  return true;
}

bool TrailGraph::SetFavicon(NodeId id, const GURL& favicon_url) {
  auto it = nodes_.find(id);
  if (it == nodes_.end() || it->second.favicon_url == favicon_url) {
    return false;
  }
  it->second.favicon_url = favicon_url;
  return true;
}

bool TrailGraph::Redirect(NodeId id, const GURL& url) {
  auto it = nodes_.find(id);
  if (it == nodes_.end() || it->second.url == url) {
    return false;
  }
  TrailNode& node = it->second;
  node.redirects.push_back(node.url);
  node.url = url;
  // The title and icon belonged to the page that went away.
  node.title.clear();
  node.favicon_url = GURL();
  return true;
}

void TrailGraph::MarkVisited(NodeId id, base::Time time) {
  if (const TrailNode* node = GetNode(id)) {
    Trail& trail = trails_.at(node->trail_id);
    trail.last_visit = std::max(trail.last_visit, time);
  }
}

const TrailNode* TrailGraph::GetNode(NodeId id) const {
  auto it = nodes_.find(id);
  return it == nodes_.end() ? nullptr : &it->second;
}

const Trail* TrailGraph::GetTrail(TrailId id) const {
  auto it = trails_.find(id);
  return it == trails_.end() ? nullptr : &it->second;
}

std::u16string TrailGraph::GetTitle(TrailId id) const {
  const Trail* trail = GetTrail(id);
  if (!trail || trail->nodes.empty()) {
    return std::u16string();
  }
  if (!trail->search_terms.empty()) {
    return trail->search_terms;
  }
  const TrailNode& first = nodes_.at(trail->nodes.front());
  if (!first.title.empty()) {
    return first.title;
  }
  return base::UTF8ToUTF16(first.url.host());
}

NodeId TrailGraph::FindBranchPoint(NodeId id) const {
  const TrailNode* node = GetNode(id);
  NodeId current = node ? node->parent_id : kNoNode;
  while (const TrailNode* ancestor = GetNode(current)) {
    if (ancestor->children.size() > 1) {
      return ancestor->id;
    }
    current = ancestor->parent_id;
  }
  return kNoNode;
}

NodeId TrailGraph::FindLatestWithUrl(const GURL& url) const {
  const GURL target = WithoutRef(url);
  for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {
    if (WithoutRef(it->second.url) == target) {
      return it->first;
    }
  }
  return kNoNode;
}

std::vector<const Trail*> TrailGraph::TrailsByRecency() const {
  std::vector<const Trail*> result;
  result.reserve(trails_.size());
  for (const auto& [id, trail] : trails_) {
    result.push_back(&trail);
  }
  std::stable_sort(result.begin(), result.end(),
                   [](const Trail* a, const Trail* b) {
                     return a->last_visit > b->last_visit;
                   });
  return result;
}

size_t TrailGraph::RemoveNodesIf(
    base::FunctionRef<bool(const TrailNode&)> pred) {
  // Each removed page maps to its parent; chains of removed pages resolve to
  // the nearest surviving ancestor.
  std::map<NodeId, NodeId> new_parent;
  for (const auto& [id, node] : nodes_) {
    if (pred(node)) {
      new_parent[id] = node.parent_id;
    }
  }
  if (new_parent.empty()) {
    return 0;
  }
  auto resolve = [&](NodeId parent) {
    for (auto it = new_parent.find(parent); it != new_parent.end();
         it = new_parent.find(parent)) {
      parent = it->second;
    }
    return parent;
  };
  for (auto& [id, node] : nodes_) {
    if (!new_parent.contains(id)) {
      node.parent_id = resolve(node.parent_id);
    }
  }
  for (const auto& [id, parent] : new_parent) {
    nodes_.erase(id);
  }
  RebuildDerived();
  return new_parent.size();
}

size_t TrailGraph::RemoveRedirectsIf(
    base::FunctionRef<bool(const TrailNode&, const GURL&)> pred) {
  size_t removed = 0;
  for (auto& [id, node] : nodes_) {
    removed += std::erase_if(node.redirects,
                             [&](const GURL& url) { return pred(node, url); });
  }
  return removed;
}

bool TrailGraph::RemoveTrail(TrailId id) {
  if (!trails_.contains(id)) {
    return false;
  }
  RemoveNodesIf([id](const TrailNode& node) { return node.trail_id == id; });
  trails_.erase(id);
  return true;
}

void TrailGraph::RemoveTrailsBefore(base::Time cutoff) {
  std::vector<TrailId> old;
  for (const auto& [id, trail] : trails_) {
    if (trail.last_visit < cutoff) {
      old.push_back(id);
    }
  }
  for (TrailId id : old) {
    RemoveTrail(id);
  }
}

void TrailGraph::Clear() {
  nodes_.clear();
  trails_.clear();
}

void TrailGraph::RebuildDerived() {
  for (auto& [id, trail] : trails_) {
    trail.nodes.clear();
  }
  for (auto& [id, node] : nodes_) {
    node.children.clear();
  }
  // Ids grow with time, so map order is visit order.
  for (auto& [id, node] : nodes_) {
    trails_.at(node.trail_id).nodes.push_back(id);
    auto parent = nodes_.find(node.parent_id);
    if (parent != nodes_.end()) {
      parent->second.children.push_back(id);
    }
  }
  std::erase_if(trails_,
                [](const auto& entry) { return entry.second.nodes.empty(); });
}

void TrailGraph::EnforceLimit() {
  while (nodes_.size() > kMaxNodes && trails_.size() > 1) {
    const Trail* oldest = TrailsByRecency().back();
    RemoveTrail(oldest->id);
  }
}

base::DictValue TrailGraph::ToValue() const {
  base::ListValue trails;
  for (const auto& [id, trail] : trails_) {
    trails.Append(base::DictValue()
                      .Set("id", base::Int64ToValue(id))
                      .Set("search_terms", trail.search_terms)
                      .Set("last_visit", base::TimeToValue(trail.last_visit))
                      .Set("next_branch", trail.next_branch));
  }
  base::ListValue nodes;
  for (const auto& [id, node] : nodes_) {
    base::ListValue redirects;
    for (const GURL& url : node.redirects) {
      redirects.Append(url.spec());
    }
    nodes.Append(base::DictValue()
                     .Set("id", base::Int64ToValue(id))
                     .Set("trail", base::Int64ToValue(node.trail_id))
                     .Set("parent", base::Int64ToValue(node.parent_id))
                     .Set("url", node.url.spec())
                     .Set("title", node.title)
                     .Set("favicon", node.favicon_url.spec())
                     .Set("time", base::TimeToValue(node.time))
                     .Set("branch", node.branch)
                     .Set("post", node.form_submission)
                     .Set("new_tab", node.opened_in_new_tab)
                     .Set("redirects", std::move(redirects)));
  }
  return base::DictValue()
      .Set("version", kFormatVersion)
      .Set("next_node_id", base::Int64ToValue(next_node_id_))
      .Set("next_trail_id", base::Int64ToValue(next_trail_id_))
      .Set("trails", std::move(trails))
      .Set("nodes", std::move(nodes));
}

// static
TrailGraph TrailGraph::FromValue(const base::DictValue& value) {
  TrailGraph graph;
  if (value.FindInt("version") != kFormatVersion) {
    return graph;
  }
  if (const base::ListValue* trails = value.FindList("trails")) {
    for (const base::Value& item : *trails) {
      const base::DictValue* dict = item.GetIfDict();
      std::optional<int64_t> id = dict ? GetId(*dict, "id") : std::nullopt;
      if (!id || *id <= 0) {
        continue;
      }
      Trail trail;
      trail.id = *id;
      if (const std::string* terms = dict->FindString("search_terms")) {
        trail.search_terms = base::UTF8ToUTF16(*terms);
      }
      trail.last_visit =
          base::ValueToTime(dict->Find("last_visit")).value_or(base::Time());
      trail.next_branch = dict->FindInt("next_branch").value_or(1);
      graph.next_trail_id_ = std::max(graph.next_trail_id_, trail.id + 1);
      graph.trails_.emplace(trail.id, std::move(trail));
    }
  }
  if (const base::ListValue* nodes = value.FindList("nodes")) {
    for (const base::Value& item : *nodes) {
      const base::DictValue* dict = item.GetIfDict();
      if (!dict) {
        continue;
      }
      std::optional<int64_t> id = GetId(*dict, "id");
      std::optional<int64_t> trail = GetId(*dict, "trail");
      const std::string* url = dict->FindString("url");
      if (!id || *id <= 0 || !trail || !graph.trails_.contains(*trail) ||
          !url || !GURL(*url).is_valid()) {
        continue;
      }
      TrailNode node;
      node.id = *id;
      node.trail_id = *trail;
      node.parent_id = GetId(*dict, "parent").value_or(kNoNode);
      node.url = GURL(*url);
      if (const std::string* title = dict->FindString("title")) {
        node.title = base::UTF8ToUTF16(*title);
      }
      if (const std::string* favicon = dict->FindString("favicon")) {
        node.favicon_url = GURL(*favicon);
      }
      node.time = base::ValueToTime(dict->Find("time")).value_or(base::Time());
      node.branch = dict->FindInt("branch").value_or(0);
      node.form_submission = dict->FindBool("post").value_or(false);
      node.opened_in_new_tab = dict->FindBool("new_tab").value_or(false);
      if (const base::ListValue* redirects = dict->FindList("redirects")) {
        for (const base::Value& redirect : *redirects) {
          if (redirect.is_string()) {
            node.redirects.emplace_back(redirect.GetString());
          }
        }
      }
      graph.next_node_id_ = std::max(graph.next_node_id_, node.id + 1);
      graph.nodes_.emplace(node.id, std::move(node));
    }
  }
  // A parent must exist, belong to the same trail and be older.
  for (auto& [id, node] : graph.nodes_) {
    const TrailNode* parent = graph.GetNode(node.parent_id);
    if (!parent || parent->trail_id != node.trail_id || parent->id >= id) {
      node.parent_id = kNoNode;
    }
  }
  graph.next_node_id_ =
      std::max(graph.next_node_id_, GetId(value, "next_node_id").value_or(1));
  graph.next_trail_id_ =
      std::max(graph.next_trail_id_, GetId(value, "next_trail_id").value_or(1));
  graph.RebuildDerived();
  return graph;
}

}  // namespace wryling_trails
