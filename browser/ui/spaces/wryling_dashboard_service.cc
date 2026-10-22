/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/spaces/wryling_dashboard_service.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "brave/browser/ui/spaces/wryling_spaces.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/common/pref_names.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_constants.h"

namespace wryling {

namespace {

constexpr char kSitesKey[] = "sites";
constexpr char kHiddenKey[] = "hidden";
constexpr char kUrlKey[] = "url";
constexpr char kTitleKey[] = "title";
constexpr char kCountKey[] = "n";
constexpr char kTimeKey[] = "t";
constexpr char kSrcKey[] = "src";
constexpr char kSizeKey[] = "px";
constexpr char kNameKey[] = "name";

// A space keeps at most this many pinned sites and remembered hosts.
constexpr size_t kMaxPinned = 48;
constexpr size_t kMaxVisitedHosts = 100;
// Frequently visited sites fill the grid up to this many tiles in all, and
// at most this many of them; a site needs this many visits to count.
constexpr size_t kMaxTiles = 20;
constexpr size_t kMaxFrequent = 7;
constexpr double kMinVisits = 2;
// Visits count half as much after this long.
constexpr double kHalfLifeDays = 14;

// Icons: what is "large enough" (stop looking), what is stored, and how often
// a site with only a small one is looked at again.
constexpr int kGoodIconSize = 120;
constexpr int kStoredIconSize = 128;
constexpr base::TimeDelta kIconRefresh = base::Days(30);
constexpr base::TimeDelta kSmallIconRetry = base::Days(3);
constexpr size_t kMaxIconBytes = 1024 * 1024;
constexpr size_t kMaxCandidates = 4;

constexpr char kIconDirName[] = "Wryling Dashboard Icons";

double NowMs() {
  return base::Time::Now().InMillisecondsFSinceUnixEpoch();
}

double Score(const base::DictValue& visit, double now_ms) {
  const double count = visit.FindDouble(kCountKey).value_or(0);
  const double age_days =
      std::max(0.0, now_ms - visit.FindDouble(kTimeKey).value_or(0)) /
      base::Days(1).InMillisecondsF();
  return count * std::pow(0.5, age_days / kHalfLifeDays);
}

// "github.com" from "https://www.github.com/...".
std::string StripWww(std::string_view host) {
  if (base::StartsWith(host, "www.")) {
    host.remove_prefix(4);
  }
  return std::string(host);
}

// A plain name for a site with nothing better: "github.com" -> "GitHub" is
// beyond us, so "Github"; "news.ycombinator.com" -> "Ycombinator".
std::string NameFromHost(const std::string& host) {
  std::string name = net::registry_controlled_domains::GetDomainAndRegistry(
      host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (name.empty()) {
    name = host;
  }
  const size_t dot = name.find('.');
  if (dot != std::string::npos && dot > 0) {
    name = name.substr(0, dot);
  }
  if (!name.empty()) {
    name[0] = base::ToUpperASCII(name[0]);
  }
  return name;
}

}  // namespace

struct DashboardService::Fetch {
  std::vector<IconCandidate> candidates;
  size_t next = 0;
  GURL current;
  // The best icon found so far.
  SkBitmap best;
  GURL best_src;
};

// static
void DashboardService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kDashboardsPref);
  registry->RegisterDictionaryPref(kDashboardVisitsPref);
  registry->RegisterDictionaryPref(kDashboardSitesInfoPref);
}

DashboardService::DashboardService(Profile* profile)
    : profile_(profile),
      icon_dir_(profile->GetPath().AppendASCII(kIconDirName)),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(kSpacesPref,
                      base::BindRepeating(&DashboardService::OnSpacesChanged,
                                          base::Unretained(this)));
  pref_registrar_.Add(
      kDashboardsPref,
      base::BindRepeating(&DashboardService::NotifyDashboardsChanged,
                          base::Unretained(this)));
  if (history::HistoryService* history = HistoryServiceFactory::GetForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS)) {
    history_observation_.Observe(history);
  }
}

DashboardService::~DashboardService() = default;

void DashboardService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DashboardService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DashboardService::Shutdown() {
  for (Observer& observer : observers_) {
    observer.OnDashboardServiceShutdown();
  }
  fetches_.clear();
  history_observation_.Reset();
  pref_registrar_.RemoveAll();
}

// static
std::string DashboardService::HostKey(const GURL& url) {
  return StripWww(url.host());
}

// static
GURL DashboardService::FixUpSiteURL(const std::string& text) {
  const std::string trimmed(base::TrimWhitespaceASCII(text, base::TRIM_ALL));
  if (trimmed.empty()) {
    return GURL();
  }
  GURL url(trimmed);
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    url = GURL("https://" + trimmed);
  }
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || url.host().empty() ||
      (url.host().find('.') == std::string::npos && url.host() != "localhost" &&
       !url.HostIsIPAddress())) {
    return GURL();
  }
  return url;
}

base::DictValue DashboardService::GetSpaceEntry(
    const std::string& space_id) const {
  const base::DictValue* entry =
      profile_->GetPrefs()->GetDict(kDashboardsPref).FindDict(space_id);
  return entry ? entry->Clone() : base::DictValue();
}

void DashboardService::SetSpaceEntry(const std::string& space_id,
                                     base::DictValue entry) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(), kDashboardsPref);
  update->Set(space_id, std::move(entry));
}

std::set<std::string> DashboardService::PinnedHosts() const {
  std::set<std::string> hosts;
  for (const auto [space_id, entry] :
       profile_->GetPrefs()->GetDict(kDashboardsPref)) {
    const base::DictValue* dict = entry.GetIfDict();
    const base::ListValue* sites = dict ? dict->FindList(kSitesKey) : nullptr;
    if (!sites) {
      continue;
    }
    for (const base::Value& site : *sites) {
      if (const std::string* url =
              site.is_dict() ? site.GetDict().FindString(kUrlKey) : nullptr) {
        hosts.insert(HostKey(GURL(*url)));
      }
    }
  }
  return hosts;
}

std::vector<DashboardService::Site> DashboardService::GetSites(
    const std::string& space_id) {
  std::vector<Site> result;
  std::set<std::string> taken;

  const base::DictValue entry = GetSpaceEntry(space_id);
  if (const base::ListValue* sites = entry.FindList(kSitesKey)) {
    for (const base::Value& value : *sites) {
      if (!value.is_dict()) {
        continue;
      }
      const std::string* url = value.GetDict().FindString(kUrlKey);
      const std::string* title = value.GetDict().FindString(kTitleKey);
      GURL gurl(url ? *url : std::string());
      if (!gurl.is_valid()) {
        continue;
      }
      Site site;
      site.url = gurl;
      site.title = title && !title->empty() ? *title : GetSiteName(gurl);
      site.pinned = true;
      taken.insert(HostKey(gurl));
      result.push_back(std::move(site));
    }
  }
  if (const base::ListValue* hidden = entry.FindList(kHiddenKey)) {
    for (const base::Value& host : *hidden) {
      if (host.is_string()) {
        taken.insert(host.GetString());
      }
    }
  }

  const base::DictValue* visits =
      profile_->GetPrefs()->GetDict(kDashboardVisitsPref).FindDict(space_id);
  if (visits && result.size() < kMaxTiles) {
    const double now = NowMs();
    std::vector<std::pair<double, std::string>> ranked;
    for (const auto [host, visit] : *visits) {
      if (!visit.is_dict() || taken.contains(host) ||
          visit.GetDict().FindDouble(kCountKey).value_or(0) < kMinVisits) {
        continue;
      }
      ranked.emplace_back(Score(visit.GetDict(), now), host);
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    const size_t room = std::min(kMaxFrequent, kMaxTiles - result.size());
    for (size_t i = 0; i < ranked.size() && i < room; ++i) {
      Site site;
      site.url = GURL("https://" + ranked[i].second + "/");
      site.title = GetSiteName(site.url);
      result.push_back(std::move(site));
    }
  }
  return result;
}

std::string DashboardService::GetSiteName(const GURL& url) const {
  const std::string host = HostKey(url);
  const base::DictValue* info =
      profile_->GetPrefs()->GetDict(kDashboardSitesInfoPref).FindDict(host);
  const std::string* name = info ? info->FindString(kNameKey) : nullptr;
  return name && !name->empty() ? *name : NameFromHost(host);
}

bool DashboardService::AddSite(const std::string& space_id,
                               const GURL& url,
                               const std::string& title) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  base::DictValue entry = GetSpaceEntry(space_id);
  base::ListValue* sites = entry.EnsureList(kSitesKey);
  for (const base::Value& site : *sites) {
    const std::string* existing =
        site.is_dict() ? site.GetDict().FindString(kUrlKey) : nullptr;
    if (existing && GURL(*existing) == url) {
      return true;
    }
  }
  if (sites->size() >= kMaxPinned) {
    return false;
  }
  sites->Append(base::DictValue()
                    .Set(kUrlKey, url.spec())
                    .Set(kTitleKey, std::string(base::TrimWhitespaceASCII(
                                        title, base::TRIM_ALL))));
  // A site added by hand is no longer hidden.
  if (base::ListValue* hidden = entry.FindList(kHiddenKey)) {
    hidden->EraseValue(base::Value(HostKey(url)));
  }
  SetSpaceEntry(space_id, std::move(entry));
  GetIcon(url);
  return true;
}

bool DashboardService::UpdateSite(const std::string& space_id,
                                  const GURL& old_url,
                                  const GURL& url,
                                  const std::string& title) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  base::DictValue entry = GetSpaceEntry(space_id);
  base::ListValue* sites = entry.FindList(kSitesKey);
  if (!sites) {
    return false;
  }
  for (base::Value& site : *sites) {
    if (!site.is_dict()) {
      continue;
    }
    const std::string* existing = site.GetDict().FindString(kUrlKey);
    if (existing && GURL(*existing) == old_url) {
      site.GetDict().Set(kUrlKey, url.spec());
      site.GetDict().Set(kTitleKey, std::string(base::TrimWhitespaceASCII(
                                        title, base::TRIM_ALL)));
      SetSpaceEntry(space_id, std::move(entry));
      GetIcon(url);
      return true;
    }
  }
  return false;
}

void DashboardService::RemoveSite(const std::string& space_id,
                                  const GURL& url) {
  base::DictValue entry = GetSpaceEntry(space_id);
  bool was_pinned = false;
  if (base::ListValue* sites = entry.FindList(kSitesKey)) {
    was_pinned = sites->EraseIf([&](const base::Value& site) {
                   const std::string* existing =
                       site.is_dict() ? site.GetDict().FindString(kUrlKey)
                                      : nullptr;
                   return existing && GURL(*existing) == url;
                 }) > 0;
  }
  if (!was_pinned) {
    // A frequently visited site: keep it off this space's dashboard.
    base::ListValue* hidden = entry.EnsureList(kHiddenKey);
    const std::string host = HostKey(url);
    if (!std::ranges::contains(*hidden, base::Value(host))) {
      hidden->Append(host);
    }
  }
  SetSpaceEntry(space_id, std::move(entry));
}

void DashboardService::MoveSite(const std::string& space_id,
                                const GURL& url,
                                int index) {
  base::DictValue entry = GetSpaceEntry(space_id);
  base::ListValue* sites = entry.EnsureList(kSitesKey);
  std::optional<base::Value> moving;
  for (size_t i = 0; i < sites->size(); ++i) {
    const base::Value& site = (*sites)[i];
    const std::string* existing =
        site.is_dict() ? site.GetDict().FindString(kUrlKey) : nullptr;
    if (existing && GURL(*existing) == url) {
      moving = site.Clone();
      sites->erase(sites->begin() + i);
      break;
    }
  }
  if (!moving) {
    // Dragging a frequently visited site among the pinned ones pins it.
    if (sites->size() >= kMaxPinned) {
      return;
    }
    moving = base::Value(base::DictValue()
                             .Set(kUrlKey, url.spec())
                             .Set(kTitleKey, GetSiteName(url)));
  }
  const size_t at = std::clamp<size_t>(static_cast<size_t>(std::max(index, 0)),
                                       0, sites->size());
  sites->Insert(sites->begin() + at, std::move(*moving));
  SetSpaceEntry(space_id, std::move(entry));
}

bool DashboardService::IsHistoryDisabled() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled);
}

void DashboardService::RecordVisit(const std::string& space_id,
                                   const GURL& url) {
  if (space_id.empty() || !url.SchemeIsHTTPOrHTTPS() || IsHistoryDisabled()) {
    return;
  }
  const std::string host = HostKey(url);
  if (host.empty()) {
    return;
  }
  const double now = NowMs();
  ScopedDictPrefUpdate update(profile_->GetPrefs(), kDashboardVisitsPref);
  base::DictValue* visits = update->EnsureDict(space_id);
  base::DictValue* visit = visits->EnsureDict(host);
  // A visit counts once per minute, so a site's own redirects and reloads
  // don't inflate it.
  const double last = visit->FindDouble(kTimeKey).value_or(0);
  if (now - last > base::Minutes(1).InMillisecondsF()) {
    visit->Set(kCountKey, visit->FindDouble(kCountKey).value_or(0) + 1);
  }
  visit->Set(kTimeKey, now);

  if (visits->size() > kMaxVisitedHosts) {
    std::string weakest;
    double weakest_score = 0;
    for (const auto [key, value] : *visits) {
      if (!value.is_dict() || key == host) {
        continue;
      }
      const double score = Score(value.GetDict(), now);
      if (weakest.empty() || score < weakest_score) {
        weakest = key;
        weakest_score = score;
      }
    }
    visits->Remove(weakest);
  }
}

void DashboardService::RecordSiteName(const GURL& url,
                                      const std::string& name,
                                      bool from_manifest) {
  const std::string trimmed(base::TrimWhitespaceASCII(name, base::TRIM_ALL));
  if (trimmed.empty() || trimmed.size() > 40) {
    return;
  }
  const std::string host = HostKey(url);
  const base::DictValue* info =
      profile_->GetPrefs()->GetDict(kDashboardSitesInfoPref).FindDict(host);
  const std::string* existing = info ? info->FindString(kNameKey) : nullptr;
  if (existing && (*existing == trimmed || !from_manifest)) {
    return;
  }
  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(), kDashboardSitesInfoPref);
    update->EnsureDict(host)->Set(kNameKey, trimmed);
  }
  NotifySiteInfoChanged();
}

bool DashboardService::IconIsFresh(const std::string& host) const {
  const base::DictValue* info =
      profile_->GetPrefs()->GetDict(kDashboardSitesInfoPref).FindDict(host);
  if (!info || !info->FindDouble(kTimeKey)) {
    return false;
  }
  const base::TimeDelta age =
      base::Time::Now() - base::Time::FromMillisecondsSinceUnixEpoch(
                              info->FindDouble(kTimeKey).value_or(0));
  const int size = info->FindInt(kSizeKey).value_or(0);
  return size >= kGoodIconSize ? age < kIconRefresh : age < kSmallIconRetry;
}

bool DashboardService::WantsSiteInfo(const GURL& url) const {
  const std::string host = HostKey(url);
  if (host.empty() || fetches_.contains(host)) {
    return false;
  }
  const base::DictValue* info =
      profile_->GetPrefs()->GetDict(kDashboardSitesInfoPref).FindDict(host);
  return !IconIsFresh(host) || !info || !info->FindString(kNameKey);
}

void DashboardService::LearnIcons(const GURL& page_url,
                                  std::vector<IconCandidate> candidates) {
  const std::string host = HostKey(page_url);
  if (host.empty()) {
    return;
  }
  if (auto it = fetches_.find(host); it != fetches_.end()) {
    // Icons declared later (the manifest after the page's <link>s) join the
    // fetch under way.
    Fetch& fetch = *it->second;
    for (IconCandidate& candidate : candidates) {
      const bool known = std::ranges::any_of(
          fetch.candidates,
          [&](const IconCandidate& c) { return c.url == candidate.url; });
      if (!known && candidate.url.SchemeIs(url::kHttpsScheme) &&
          fetch.candidates.size() < 2 * kMaxCandidates) {
        fetch.candidates.push_back(std::move(candidate));
      }
    }
    return;
  }
  if (IconIsFresh(host)) {
    return;
  }
  const base::DictValue* info =
      profile_->GetPrefs()->GetDict(kDashboardSitesInfoPref).FindDict(host);
  const int have = info ? info->FindInt(kSizeKey).value_or(0) : 0;
  std::erase_if(candidates, [](const IconCandidate& candidate) {
    return !candidate.url.is_valid() ||
           !candidate.url.SchemeIs(url::kHttpsScheme);
  });
  // Biggest first; unknown sizes are what such icons usually are.
  auto expected = [](const IconCandidate& candidate) {
    if (candidate.declared_size > 0) {
      return candidate.declared_size;
    }
    return candidate.app_icon ? 180 : 32;
  };
  std::stable_sort(candidates.begin(), candidates.end(),
                   [&](const IconCandidate& a, const IconCandidate& b) {
                     return expected(a) > expected(b);
                   });
  std::erase_if(candidates, [&](const IconCandidate& candidate) {
    return expected(candidate) <= have;
  });
  if (candidates.size() > kMaxCandidates) {
    candidates.resize(kMaxCandidates);
  }
  if (candidates.empty()) {
    return;
  }
  auto fetch = std::make_unique<Fetch>();
  fetch->candidates = std::move(candidates);
  fetches_[host] = std::move(fetch);
  StartNextCandidate(host);
}

const DashboardService::Icon* DashboardService::GetIcon(const GURL& url) {
  const std::string host = HostKey(url);
  if (host.empty()) {
    return nullptr;
  }
  if (auto it = icons_.find(host); it != icons_.end()) {
    return &it->second;
  }
  if (!loading_.contains(host)) {
    LoadIcon(host);
  }
  return nullptr;
}

void DashboardService::LoadIcon(const std::string& host) {
  loading_.insert(host);
  const base::DictValue* info =
      profile_->GetPrefs()->GetDict(kDashboardSitesInfoPref).FindDict(host);
  if (info && info->FindInt(kSizeKey).value_or(0) > 0) {
    file_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& path) -> std::optional<std::string> {
              std::string png;
              if (!base::ReadFileToStringWithMaxSize(path, &png,
                                                     kMaxIconBytes)) {
                return std::nullopt;
              }
              return png;
            },
            IconPath(host)),
        base::BindOnce(&DashboardService::OnIconRead,
                       weak_factory_.GetWeakPtr(), host));
    return;
  }
  OnIconRead(host, std::nullopt);
}

void DashboardService::OnIconRead(const std::string& host,
                                  std::optional<std::string> png) {
  if (png && !png->empty()) {
    const base::DictValue* info =
        profile_->GetPrefs()->GetDict(kDashboardSitesInfoPref).FindDict(host);
    Icon icon;
    icon.data_url = "data:image/png;base64," + base::Base64Encode(*png);
    icon.size = info ? info->FindInt(kSizeKey).value_or(0) : 0;
    icons_[host] = std::move(icon);
    loading_.erase(host);
    NotifySiteInfoChanged();
    return;
  }
  loading_.erase(host);
  // Never seen it: try the places sites usually keep their app icon. Only
  // once a session, so a site without one isn't asked on every New Tab.
  if (tried_.insert(host).second) {
    const std::string origin = "https://" + host;
    LearnIcons(GURL(origin + "/"),
               {{GURL(origin + "/apple-touch-icon.png"), 180, true},
                {GURL(origin + "/apple-touch-icon-precomposed.png"), 180, true},
                {GURL(origin + "/favicon.ico"), 32, false}});
  }
}

void DashboardService::StartNextCandidate(const std::string& host) {
  auto it = fetches_.find(host);
  if (it == fetches_.end()) {
    return;
  }
  Fetch& fetch = *it->second;
  // Stop once a large icon is in hand, or nothing bigger is left to try.
  if (fetch.next >= fetch.candidates.size() ||
      fetch.best.width() >= kGoodIconSize) {
    FinishFetch(host);
    return;
  }
  const IconCandidate& candidate = fetch.candidates[fetch.next++];
  fetch.current = candidate.url;

  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("wryling_dashboard_icon", R"(
      semantics {
        sender: "Wryling New Tab dashboard"
        description:
          "Fetches the app icon a site declares (apple-touch-icon, web "
          "manifest icon or favicon) to show it as a tile on the New Tab "
          "dashboard."
        trigger:
          "Visiting a site, or adding one to a space's dashboard."
        data: "No user data; cookies are not sent."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: NO
        setting: "Remove the site from the dashboard."
        policy_exception_justification: "Not implemented."
      })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = candidate.url;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 traffic_annotation);
  loader->SetTimeoutDuration(base::Seconds(15));
  network::SimpleURLLoader* raw_loader = loader.get();
  raw_loader->DownloadToString(
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&DashboardService::OnIconDownloaded,
                     weak_factory_.GetWeakPtr(), host, std::move(loader)),
      kMaxIconBytes);
}

void DashboardService::OnIconDownloaded(
    const std::string& host,
    std::unique_ptr<network::SimpleURLLoader> loader,
    std::optional<std::string> body) {
  if (!body || body->empty() || loader->NetError() != net::OK) {
    StartNextCandidate(host);
    return;
  }
  data_decoder::DecodeImageIsolated(
      base::as_byte_span(*body), data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, kMaxIconBytes * 4,
      gfx::Size(kStoredIconSize, kStoredIconSize),
      base::BindOnce(&DashboardService::OnIconDecoded,
                     weak_factory_.GetWeakPtr(), host));
}

void DashboardService::OnIconDecoded(const std::string& host,
                                     const SkBitmap& bitmap) {
  auto it = fetches_.find(host);
  if (it == fetches_.end()) {
    return;
  }
  Fetch& fetch = *it->second;
  const int width = bitmap.width();
  const int height = bitmap.height();
  // Only square-ish icons make a tile; wide logos and banners don't.
  const bool square = width > 0 && height > 0 &&
                      std::abs(width - height) * 8 <= std::max(width, height);
  if (square && width > fetch.best.width()) {
    fetch.best = bitmap;
    fetch.best_src = fetch.current;
  }
  StartNextCandidate(host);
}

void DashboardService::FinishFetch(const std::string& host) {
  auto it = fetches_.find(host);
  if (it == fetches_.end()) {
    return;
  }
  std::unique_ptr<Fetch> fetch = std::move(it->second);
  fetches_.erase(it);

  const base::DictValue* old_info =
      profile_->GetPrefs()->GetDict(kDashboardSitesInfoPref).FindDict(host);
  const int have = old_info ? old_info->FindInt(kSizeKey).value_or(0) : 0;

  SkBitmap bitmap = fetch->best;
  if (bitmap.width() > kStoredIconSize) {
    bitmap = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_BEST, kStoredIconSize,
        kStoredIconSize);
  }
  std::optional<std::vector<uint8_t>> png;
  if (!bitmap.drawsNothing() && bitmap.width() > have) {
    png = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                            /*discard_transparency=*/false);
  }
  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(), kDashboardSitesInfoPref);
    base::DictValue* info = update->EnsureDict(host);
    info->Set(kTimeKey, NowMs());
    if (png) {
      info->Set(kSizeKey, bitmap.width());
      info->Set(kSrcKey, fetch->best_src.spec());
    }
  }
  if (!png) {
    return;
  }
  Icon icon;
  icon.size = bitmap.width();
  icon.data_url = "data:image/png;base64," + base::Base64Encode(*png);
  icons_[host] = std::move(icon);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath path, std::vector<uint8_t> bytes) {
            base::CreateDirectory(path.DirName());
            base::WriteFile(path, bytes);
          },
          IconPath(host), std::move(*png)));
  NotifySiteInfoChanged();
}

base::FilePath DashboardService::IconPath(const std::string& host) const {
  return icon_dir_.AppendASCII(
      base::HexEncode(base::SHA1HashString(host)).substr(0, 20) + ".png");
}

void DashboardService::NotifyDashboardsChanged() {
  for (Observer& observer : observers_) {
    observer.OnDashboardsChanged();
  }
}

void DashboardService::NotifySiteInfoChanged() {
  for (Observer& observer : observers_) {
    observer.OnSiteInfoChanged();
  }
}

void DashboardService::OnSpacesChanged() {
  // Drop the dashboards and visits of spaces that were deleted.
  std::set<std::string> ids;
  for (const Space& space : GetSpaces(profile_->GetPrefs())) {
    ids.insert(space.id);
  }
  for (const char* pref : {kDashboardsPref, kDashboardVisitsPref}) {
    std::vector<std::string> gone;
    for (const auto [id, value] : profile_->GetPrefs()->GetDict(pref)) {
      if (!ids.contains(id)) {
        gone.push_back(id);
      }
    }
    if (gone.empty()) {
      continue;
    }
    ScopedDictPrefUpdate update(profile_->GetPrefs(), pref);
    for (const std::string& id : gone) {
      update->Remove(id);
    }
  }
  NotifyDashboardsChanged();
}

void DashboardService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.is_from_expiration()) {
    return;
  }
  if (deletion_info.IsAllHistory()) {
    profile_->GetPrefs()->ClearPref(kDashboardVisitsPref);
  } else {
    std::set<std::string> hosts;
    for (const history::URLRow& row : deletion_info.deleted_rows()) {
      hosts.insert(HostKey(row.url()));
    }
    std::optional<std::pair<double, double>> range;
    const history::DeletionTimeRange& time_range = deletion_info.time_range();
    if (time_range.IsValid() && !deletion_info.restrict_urls()) {
      range = {time_range.begin().is_null()
                   ? 0
                   : time_range.begin().InMillisecondsFSinceUnixEpoch(),
               time_range.end().is_null()
                   ? NowMs()
                   : time_range.end().InMillisecondsFSinceUnixEpoch()};
    }
    ForgetVisits(hosts, range);
  }
  DropUnusedSiteInfo();
  NotifyDashboardsChanged();
}

void DashboardService::ForgetVisits(
    const std::set<std::string>& hosts,
    std::optional<std::pair<double, double>> time_range) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(), kDashboardVisitsPref);
  for (auto [space_id, value] : *update) {
    base::DictValue* visits = value.GetIfDict();
    if (!visits) {
      continue;
    }
    std::vector<std::string> gone;
    for (const auto [host, visit] : *visits) {
      const double t =
          visit.is_dict() ? visit.GetDict().FindDouble(kTimeKey).value_or(0)
                          : 0;
      if (hosts.contains(host) ||
          (time_range && t >= time_range->first && t <= time_range->second)) {
        gone.push_back(host);
      }
    }
    for (const std::string& host : gone) {
      visits->Remove(host);
    }
  }
}

void DashboardService::DropUnusedSiteInfo() {
  // Keep icons and names only for sites still on some dashboard.
  std::set<std::string> keep = PinnedHosts();
  for (const auto [space_id, visits] :
       profile_->GetPrefs()->GetDict(kDashboardVisitsPref)) {
    if (const base::DictValue* dict = visits.GetIfDict()) {
      for (const auto [host, visit] : *dict) {
        keep.insert(host);
      }
    }
  }
  std::vector<std::string> gone;
  for (const auto [host, info] :
       profile_->GetPrefs()->GetDict(kDashboardSitesInfoPref)) {
    if (!keep.contains(host)) {
      gone.push_back(host);
    }
  }
  if (gone.empty()) {
    return;
  }
  ScopedDictPrefUpdate update(profile_->GetPrefs(), kDashboardSitesInfoPref);
  std::vector<base::FilePath> files;
  for (const std::string& host : gone) {
    update->Remove(host);
    icons_.erase(host);
    files.push_back(IconPath(host));
  }
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::vector<base::FilePath> files) {
                       for (const base::FilePath& file : files) {
                         base::DeleteFile(file);
                       }
                     },
                     std::move(files)));
}

void DashboardService::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_observation_.Reset();
}

}  // namespace wryling
