/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_SERVICE_H_
#define BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_SERVICE_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/files/important_file_writer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "brave/browser/wryling_trails/trail_graph.h"
#include "brave/browser/wryling_trails/wryling_trails.mojom-shared.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class WebContents;
}

namespace wryling_trails {

class WrylingTrailsTabHelper;

// The profile's trails: owns the graph, saves it as JSON in the profile
// directory, forgets pages the user deletes from history, and knows which tab
// shows which page.
class WrylingTrailsService : public KeyedService,
                             public history::HistoryServiceObserver,
                             public base::ImportantFileWriter::DataSerializer {
 public:
  using RestoreResult = mojom::RestoreResult;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnTrailsChanged() = 0;
    // The profile is going away; drop any reference to the service.
    virtual void OnTrailsServiceShutdown() {}
  };

  // Trails not visited for this long are dropped on load.
  static constexpr base::TimeDelta kKeepFor = base::Days(90);
  static constexpr char kFileName[] = "Wryling Trails";

  explicit WrylingTrailsService(Profile* profile);
  WrylingTrailsService(const WrylingTrailsService&) = delete;
  WrylingTrailsService& operator=(const WrylingTrailsService&) = delete;
  ~WrylingTrailsService() override;

  // Runs `task` once the saved trails are loaded; at once if they are.
  void RunWhenLoaded(base::OnceClosure task);
  bool loaded() const { return loaded_; }

  TrailGraph& graph() { return graph_; }
  // Call after changing graph(): saves soon and tells observers.
  void OnGraphChanged();
  // History saving turned off by pref or policy.
  bool IsHistoryDisabled() const;

  void AddTab(WrylingTrailsTabHelper* tab);
  void RemoveTab(WrylingTrailsTabHelper* tab);
  WrylingTrailsTabHelper* FindTab(int32_t tab_id) const;
  WrylingTrailsTabHelper* FindTabShowing(NodeId node) const;

  // Opens `node` for the History page (`initiator` is its WebContents).
  RestoreResult Restore(NodeId node, content::WebContents* initiator);
  // Takes the tab to where its current line branched off; `node` gets the
  // branch point.
  RestoreResult BackToBranchPoint(int32_t tab_id, NodeId* node);

  base::WeakPtr<WrylingTrailsService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // base::ImportantFileWriter::DataSerializer:
  std::optional<std::string> SerializeData() override;

 private:
  struct Deletion;

  void OnLoaded(std::optional<std::string> json);
  void ApplyDeletion(const Deletion& deletion);
  void ActivateTab(content::WebContents* contents);

  raw_ptr<Profile> profile_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::ImportantFileWriter writer_;
  TrailGraph graph_;
  bool loaded_ = false;
  std::vector<base::OnceClosure> pending_;
  std::set<raw_ptr<WrylingTrailsTabHelper>> tabs_;
  base::ObserverList<Observer> observers_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};
  base::WeakPtrFactory<WrylingTrailsService> weak_factory_{this};
};

}  // namespace wryling_trails

#endif  // BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_SERVICE_H_
