/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_SERVICE_H_
#define BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/gurl.h"

class Profile;
class SkBitmap;

namespace base {
class SequencedTaskRunner;
}

namespace network {
class SimpleURLLoader;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace wryling {

// Profile prefs.
//
// The sites each space's dashboard shows, keyed by space ID:
//   {"<space id>": {"sites": [{"url", "title"}...], "hidden": ["host"...]}}
// Kept apart from visits and icons so it can be synced on its own.
inline constexpr char kDashboardsPref[] = "wryling.dashboards";
// How often each site was opened in each space (local only):
//   {"<space id>": {"<host>": {"n": count, "t": last visit (ms)}}}
inline constexpr char kDashboardVisitsPref[] = "wryling.dashboard_visits";
// What is known about each site's look, shared by every space:
//   {"<host>": {"src": icon URL, "px": width, "t": fetched (ms),
//               "name": the site's own short name}}
// The icon images themselves are PNG files in "Wryling Dashboard Icons".
inline constexpr char kDashboardSitesInfoPref[] = "wryling.dashboard_site_info";

// One icon a page declares (or that is guessed for it).
struct IconCandidate {
  GURL url;
  // The width it claims, 0 when unknown.
  int declared_size = 0;
  // An apple-touch-icon or web manifest icon (made for home screens).
  bool app_icon = false;
};

// Each space's New Tab dashboard: the sites pinned to it, the ones opened
// most in it, and a cache of the sites' own large icons.
class DashboardService : public KeyedService,
                         public history::HistoryServiceObserver {
 public:
  struct Site {
    GURL url;
    std::string title;
    bool pinned = false;
  };

  struct Icon {
    std::string data_url;
    int size = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    // A space's sites changed (or spaces were deleted).
    virtual void OnDashboardsChanged() {}
    // A site's icon or name became known.
    virtual void OnSiteInfoChanged() {}
    // Stop observing: the profile is going away.
    virtual void OnDashboardServiceShutdown() {}
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit DashboardService(Profile* profile);
  DashboardService(const DashboardService&) = delete;
  DashboardService& operator=(const DashboardService&) = delete;
  ~DashboardService() override;

  base::WeakPtr<DashboardService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // The space's pinned sites in order, then its most visited other sites.
  std::vector<Site> GetSites(const std::string& space_id);
  // The site's icon, when it is in memory. Starts loading (or fetching) it
  // otherwise; observers hear OnSiteInfoChanged() when it arrives.
  const Icon* GetIcon(const GURL& url);
  // The site's own short name (from its web manifest), if known.
  std::string GetSiteName(const GURL& url) const;

  // `url` as a site address: adds https:// to bare hosts, rejects anything
  // that isn't http(s).
  static GURL FixUpSiteURL(const std::string& text);

  bool AddSite(const std::string& space_id,
               const GURL& url,
               const std::string& title);
  bool UpdateSite(const std::string& space_id,
                  const GURL& old_url,
                  const GURL& url,
                  const std::string& title);
  void RemoveSite(const std::string& space_id, const GURL& url);
  void MoveSite(const std::string& space_id, const GURL& url, int index);

  // The space's whole stored entry, for undo.
  base::DictValue GetSpaceEntry(const std::string& space_id) const;
  void SetSpaceEntry(const std::string& space_id, base::DictValue entry);

  // Recording, from the tab helper.
  void RecordVisit(const std::string& space_id, const GURL& url);
  // `from_manifest`: the site's own short name, which replaces a name
  // taken from a page title.
  void RecordSiteName(const GURL& url,
                      const std::string& name,
                      bool from_manifest);
  void LearnIcons(const GURL& page_url, std::vector<IconCandidate> candidates);
  // Whether the site's icon (or name) is still worth looking for.
  bool WantsSiteInfo(const GURL& url) const;
  bool IsHistoryDisabled() const;

  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  struct Fetch;

  static std::string HostKey(const GURL& url);
  // Whether the stored icon is recent enough not to look again.
  bool IconIsFresh(const std::string& host) const;
  std::set<std::string> PinnedHosts() const;
  void NotifyDashboardsChanged();
  void NotifySiteInfoChanged();
  void OnSpacesChanged();
  void ForgetVisits(const std::set<std::string>& hosts,
                    std::optional<std::pair<double, double>> time_range);
  void DropUnusedSiteInfo();

  void LoadIcon(const std::string& host);
  void OnIconRead(const std::string& host, std::optional<std::string> png);
  void StartNextCandidate(const std::string& host);
  void OnIconDownloaded(const std::string& host,
                        std::unique_ptr<network::SimpleURLLoader> loader,
                        std::optional<std::string> body);
  void OnIconDecoded(const std::string& host, const SkBitmap& bitmap);
  void FinishFetch(const std::string& host);
  base::FilePath IconPath(const std::string& host) const;

  raw_ptr<Profile> profile_;
  base::FilePath icon_dir_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  PrefChangeRegistrar pref_registrar_;

  std::map<std::string, Icon> icons_;
  // Hosts whose icon is being read from disk or fetched, or was tried this
  // session and has none.
  std::set<std::string> loading_;
  std::set<std::string> tried_;
  std::map<std::string, std::unique_ptr<Fetch>> fetches_;

  base::ObserverList<Observer> observers_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};
  base::WeakPtrFactory<DashboardService> weak_factory_{this};
};

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_SERVICE_H_
