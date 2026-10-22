/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/spaces/wryling_spaces.h"

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace wryling {

namespace {

constexpr char kDefaultSpaceId[] = "default";

Space MakeDefaultSpace() {
  Space space;
  space.id = kDefaultSpaceId;
  space.name = "Personal";
  space.icon = "person.fill";
  return space;
}

}  // namespace

Space::Space() = default;
Space::Space(const Space&) = default;
Space& Space::operator=(const Space&) = default;
Space::~Space() = default;

std::vector<Space> GetSpaces(const PrefService* prefs) {
  std::vector<Space> spaces;
  if (prefs && prefs->FindPreference(kSpacesPref)) {
    for (const base::Value& item : prefs->GetList(kSpacesPref)) {
      const base::DictValue* dict = item.GetIfDict();
      if (!dict) {
        continue;
      }
      const std::string* id = dict->FindString("id");
      if (!id || id->empty()) {
        continue;
      }
      Space space;
      space.id = *id;
      if (const std::string* name = dict->FindString("name")) {
        space.name = *name;
      }
      if (const std::string* icon = dict->FindString("icon")) {
        space.icon = *icon;
      }
      if (std::optional<double> color = dict->FindDouble("color")) {
        space.color = static_cast<SkColor>(static_cast<int64_t>(*color));
      }
      spaces.push_back(std::move(space));
    }
  }
  if (spaces.empty()) {
    spaces.push_back(MakeDefaultSpace());
  }
  return spaces;
}

void SetSpaces(PrefService* prefs, const std::vector<Space>& spaces) {
  base::ListValue list;
  for (const Space& space : spaces) {
    base::DictValue dict;
    dict.Set("id", space.id);
    dict.Set("name", space.name);
    dict.Set("icon", space.icon);
    if (space.color) {
      // Stored as a double: an ARGB colour doesn't fit a signed int.
      dict.Set("color", static_cast<double>(*space.color));
    }
    list.Append(std::move(dict));
  }
  prefs->SetList(kSpacesPref, std::move(list));
}

void RegisterSpacesProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kSpacesPref);
}

const std::vector<std::string>& GetSpaceIconChoices() {
  static const base::NoDestructor<std::vector<std::string>> kIcons({
      "person.fill",     "briefcase.fill",  "house.fill",
      "star.fill",       "heart.fill",      "book.fill",
      "graduationcap.fill", "hammer.fill",  "paintbrush.fill",
      "music.note",      "gamecontroller.fill", "cart.fill",
      "airplane",        "leaf.fill",       "flame.fill",
      "moon.fill",       "sun.max.fill",    "bolt.fill",
      "globe",           "terminal.fill",
  });
  return *kIcons;
}

const std::vector<SkColor>& GetSpaceColorChoices() {
  // Space colours: violet, blue, teal, green, yellow, orange, red,
  // pink, graphite, sand.
  static const base::NoDestructor<std::vector<SkColor>> kColors({
      SkColorSetRGB(0x7B, 0x61, 0xD9), SkColorSetRGB(0x3E, 0x7B, 0xE6),
      SkColorSetRGB(0x2A, 0xA5, 0xA8), SkColorSetRGB(0x4C, 0xAF, 0x50),
      SkColorSetRGB(0xE0, 0xB3, 0x2E), SkColorSetRGB(0xE8, 0x83, 0x3A),
      SkColorSetRGB(0xD9, 0x4A, 0x4A), SkColorSetRGB(0xD9, 0x5B, 0xA6),
      SkColorSetRGB(0x6B, 0x70, 0x7A), SkColorSetRGB(0xB8, 0x9B, 0x72),
  });
  return *kColors;
}

DEFINE_USER_DATA(SpacesController);

SpacesController::SpacesController(BrowserView* browser_view,
                                   TabStripModel* model)
    : browser_view_(browser_view),
      model_(model),
      scoped_user_data_(browser_view->browser()->GetUnownedUserDataHost(),
                        *this) {
  PrefService* prefs = browser_view_->browser()->GetProfile()->GetPrefs();
  spaces_ = GetSpaces(prefs);
  current_space_ = spaces_.front().id;
  pref_registrar_.Init(prefs);
  pref_registrar_.Add(kSpacesPref,
                      base::BindRepeating(&SpacesController::OnSpacesPrefChanged,
                                          base::Unretained(this)));
  // Tabs already in the strip (there normally are none yet) go in the first
  // space.
  for (tabs::TabInterface* tab : *model_) {
    tab_spaces_[tab] = current_space_;
  }
  model_->AddObserver(this);
  // Once the window's first tabs (or its restored session) are in.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SpacesController::SetUpHomeAtStartup,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(1200));
}

SpacesController::~SpacesController() {
  model_->RemoveObserver(this);
}

// static
SpacesController* SpacesController::From(BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

// static
const SpacesController* SpacesController::From(
    const BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

BrowserWindowInterface* SpacesController::browser() {
  return browser_view_->browser();
}

void SpacesController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SpacesController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const Space* SpacesController::GetSpace(const std::string& id) const {
  for (const Space& space : spaces_) {
    if (space.id == id) {
      return &space;
    }
  }
  return nullptr;
}

int SpacesController::GetSpaceIndex(const std::string& id) const {
  for (size_t i = 0; i < spaces_.size(); ++i) {
    if (spaces_[i].id == id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

SkColor SpacesController::GetDisplayColor(const Space& space) const {
  if (space.color) {
    return *space.color;
  }
  if (const ThemeService* theme = ThemeServiceFactory::GetForProfile(
          browser_view_->browser()->GetProfile())) {
    if (std::optional<SkColor> color = theme->GetUserColor()) {
      return *color;
    }
  }
  // Wryling's default violet (see wryling_space_color.cc).
  return SkColorSetRGB(0x8F, 0x7D, 0xC9);
}

std::string SpacesController::NormalizeSpaceId(const std::string& id) const {
  return GetSpace(id) ? id : spaces_.front().id;
}

std::string SpacesController::GetTabSpace(
    const tabs::TabInterface* tab) const {
  auto it = tab_spaces_.find(tab);
  return NormalizeSpaceId(it != tab_spaces_.end() ? it->second
                                                  : current_space_);
}

bool SpacesController::IsTabHidden(const tabs::TabInterface* tab) const {
  return spaces_.size() > 1 && GetTabSpace(tab) != current_space_;
}

int SpacesController::GetTabCount(const std::string& space_id) const {
  int count = 0;
  for (tabs::TabInterface* tab : *model_) {
    if (GetTabSpace(tab) == space_id) {
      ++count;
    }
  }
  return count;
}

tabs::TabInterface* SpacesController::FindTabToActivate(
    const std::string& space_id) const {
  auto it = last_active_.find(space_id);
  if (it != last_active_.end() && it->second &&
      model_->GetIndexOfTab(it->second) != TabStripModel::kNoTab &&
      GetTabSpace(it->second) == space_id) {
    return it->second;
  }
  // Otherwise the last tab of the space (with no record, the newest tab is
  // the best guess).
  tabs::TabInterface* found = nullptr;
  for (tabs::TabInterface* tab : *model_) {
    if (GetTabSpace(tab) == space_id) {
      found = tab;
    }
  }
  return found;
}

void SpacesController::SwitchToSpace(const std::string& id) {
  if (!GetSpace(id) || id == current_space_) {
    return;
  }
  if (tabs::TabInterface* tab = FindTabToActivate(id)) {
    // Activation makes the space current (OnTabStripModelChanged).
    model_->ActivateTab(tab, TabStripUserGestureDetails(
                                 TabStripUserGestureDetails::GestureType::kOther));
    if (current_space_ != id) {
      SetCurrentSpace(id);
    }
    return;
  }
  SetCurrentSpace(id);
  FillEmptySpace(id);
}

void SpacesController::SwitchToSpaceAt(int index) {
  if (index >= 0 && index < static_cast<int>(spaces_.size())) {
    SwitchToSpace(spaces_[index].id);
  }
}

void SpacesController::SwitchBy(int delta) {
  const int index = GetCurrentSpaceIndex() + delta;
  // No wrap-around: the first and last spaces are edges.
  SwitchToSpaceAt(std::clamp(index, 0, static_cast<int>(spaces_.size()) - 1));
}

void SpacesController::FillEmptySpace(const std::string& space_id) {
  if (current_space_ != space_id || model_->closing_all() ||
      GetTabCount(space_id) > 0) {
    return;
  }
  // The space's start page.
  EnsureHomeTab(space_id, /*activate=*/true);
}

bool IsHomeURL(const GURL& url) {
  return url.SchemeIs("chrome") && url.host() == "newtab" &&
         url.query() == "home";
}

bool SpacesController::IsHomeTab(const tabs::TabInterface* tab) const {
  if (!tab || !tab->GetContents() ||
      model_->GetIndexOfTab(tab) == TabStripModel::kNoTab ||
      !model_->IsTabPinned(model_->GetIndexOfTab(tab))) {
    return false;
  }
  content::WebContents* contents = tab->GetContents();
  const GURL& committed = contents->GetLastCommittedURL();
  return IsHomeURL(committed.is_empty() ? contents->GetVisibleURL()
                                        : committed);
}

tabs::TabInterface* SpacesController::GetHomeTab(
    const std::string& space_id) const {
  for (tabs::TabInterface* tab : *model_) {
    if (GetTabSpace(tab) == space_id && IsHomeTab(tab)) {
      return tab;
    }
  }
  return nullptr;
}

void SpacesController::EnsureHomeTab(const std::string& space_id,
                                     bool activate) {
  // Home tabs are created for the current space only (new tabs land there),
  // so spaces never opened don't each hold a page in memory.
  if (space_id != current_space_ || !GetSpace(space_id) ||
      model_->closing_all()) {
    return;
  }
  std::vector<tabs::TabInterface*> homes;
  for (tabs::TabInterface* tab : *model_) {
    if (GetTabSpace(tab) == space_id && IsHomeTab(tab)) {
      homes.push_back(tab);
    }
  }
  {
    base::AutoReset<bool> closing(&closing_home_, true);
    for (size_t i = 1; i < homes.size(); ++i) {
      homes[i]->Close();
    }
  }
  if (!homes.empty()) {
    if (activate && !homes.front()->IsActivated()) {
      model_->ActivateTab(homes.front(),
                          TabStripUserGestureDetails(
                              TabStripUserGestureDetails::GestureType::kOther));
    }
    return;
  }
  NavigateParams params(browser_view_->browser(), GURL(kHomeURL),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = activate ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  params.tabstrip_index = 0;
  params.tabstrip_add_types =
      AddTabTypes::ADD_PINNED | (activate ? AddTabTypes::ADD_ACTIVE : 0);
  Navigate(&params);
}

void SpacesController::SetUpHomeAtStartup() {
  if (model_->closing_all()) {
    return;
  }
  // A fresh window opens with one plain New Tab; Home takes its place.
  tabs::TabInterface* active = model_->GetActiveTab();
  // (It may still be loading, so look at the address it is showing.)
  const GURL url =
      active && active->GetContents() ? active->GetContents()->GetVisibleURL()
                                      : GURL();
  const bool plain_new_tab = url.SchemeIs("chrome") &&
                             url.host() == "newtab" && !IsHomeURL(url) &&
                             GetTabCount(current_space_) == 1;
  EnsureHomeTab(current_space_, /*activate=*/plain_new_tab);
  if (plain_new_tab && !active->IsActivated()) {
    active->Close();
  }
}

std::string SpacesController::CreateSpace() {
  Space space;
  space.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  // "Space N" with the first N no other space is called.
  for (size_t n = spaces_.size() + 1;; ++n) {
    space.name = "Space " + base::NumberToString(n);
    if (std::ranges::none_of(spaces_, [&space](const Space& s) {
          return s.name == space.name;
        })) {
      break;
    }
  }
  // A colour and icon that aren't used yet, so new spaces look distinct.
  const auto& colors = GetSpaceColorChoices();
  const auto& icons = GetSpaceIconChoices();
  for (SkColor color : colors) {
    if (std::ranges::none_of(spaces_, [color](const Space& s) {
          return s.color == color;
        })) {
      space.color = color;
      break;
    }
  }
  for (const std::string& icon : icons) {
    if (std::ranges::none_of(
            spaces_, [&icon](const Space& s) { return s.icon == icon; })) {
      space.icon = icon;
      break;
    }
  }
  std::vector<Space> spaces = spaces_;
  spaces.push_back(space);
  SetSpaces(browser_view_->browser()->GetProfile()->GetPrefs(), spaces);
  // The pref observer has refreshed `spaces_`.
  SwitchToSpace(space.id);
  return space.id;
}

void SpacesController::UpdateSpace(const Space& space) {
  std::vector<Space> spaces = spaces_;
  for (Space& s : spaces) {
    if (s.id == space.id) {
      s = space;
    }
  }
  SetSpaces(browser_view_->browser()->GetProfile()->GetPrefs(), spaces);
}

void SpacesController::DeleteSpace(const std::string& id) {
  if (spaces_.size() <= 1 || !GetSpace(id)) {
    return;
  }
  const int index = GetSpaceIndex(id);
  if (id == current_space_) {
    // Leave for the neighbour first, so closing doesn't pick a tab itself.
    SwitchToSpaceAt(index > 0 ? index - 1 : 1);
  }
  std::vector<tabs::TabInterface*> to_close;
  for (tabs::TabInterface* tab : *model_) {
    if (GetTabSpace(tab) == id) {
      to_close.push_back(tab);
    }
  }
  // Close first: until the space is gone its tabs stay hidden. A tab that
  // asks to stay (beforeunload) ends up in the first space.
  {
    // The deleted space's Home goes with it.
    base::AutoReset<bool> closing(&closing_home_, true);
    for (tabs::TabInterface* tab : to_close) {
      tab->Close();
    }
  }
  std::vector<Space> spaces = spaces_;
  std::erase_if(spaces, [&id](const Space& s) { return s.id == id; });
  SetSpaces(browser_view_->browser()->GetProfile()->GetPrefs(), spaces);
}

void SpacesController::MoveTabToSpace(tabs::TabInterface* tab,
                                      const std::string& id) {
  // Each space keeps its own Home.
  if (!GetSpace(id) || GetTabSpace(tab) == id || IsHomeTab(tab)) {
    return;
  }
  const bool was_active = tab->IsActivated();
  if (tab->IsSplit() || tab->GetGroup()) {
    // A tab leaving for another space can't stay in a split or group here.
    if (tab->IsSplit()) {
      model_->RemoveSplit(tab->GetSplit().value());
    }
    if (tab->GetGroup()) {
      model_->RemoveFromGroup({model_->GetIndexOfTab(tab)});
    }
  }
  tab_spaces_[tab] = id;
  SaveTabSpace(tab);
  if (was_active) {
    // Stay in the current space: activate another of its tabs, or fill it.
    if (tabs::TabInterface* next = FindTabToActivate(current_space_)) {
      model_->ActivateTab(next, TabStripUserGestureDetails(
                                    TabStripUserGestureDetails::GestureType::kOther));
    } else {
      // Following the tab is the only option left.
      SetCurrentSpace(id);
    }
  }
  RelayoutTabStrip();
}

void SpacesController::SetTabSpace(tabs::TabInterface* tab,
                                   const std::string& id) {
  if (!tab || model_->GetIndexOfTab(tab) == TabStripModel::kNoTab) {
    return;
  }
  tab_spaces_[tab] = id;
  SaveTabSpace(tab);
  if (tab->IsActivated()) {
    SetCurrentSpace(GetTabSpace(tab));
  } else {
    RelayoutTabStrip();
  }
}

void SpacesController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  bool closed_last_in_current = false;
  bool closed_home = false;
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& inserted : change.GetInsert()->contents) {
      // New tabs (and tabs dragged in from another window) open in the
      // current space. Restored tabs are moved to theirs afterwards
      // (RestoreTabSpace).
      if (!tab_spaces_.contains(inserted.tab)) {
        tab_spaces_[inserted.tab] = current_space_;
        SaveTabSpace(inserted.tab);
      }
    }
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& removed : change.GetRemove()->contents) {
      auto it = tab_spaces_.find(removed.tab);
      if (it == tab_spaces_.end()) {
        continue;
      }
      if (NormalizeSpaceId(it->second) == current_space_ &&
          removed.remove_reason == TabRemovedReason::kDeleted) {
        closed_last_in_current = true;
        content::WebContents* contents = removed.contents;
        closed_home = closed_home || (contents &&
                                      IsHomeURL(contents->GetLastCommittedURL()));
      }
      tab_spaces_.erase(it);
      for (auto& [space, last] : last_active_) {
        if (last == removed.tab) {
          last = nullptr;
        }
      }
    }
    closed_last_in_current =
        closed_last_in_current && model_->count() > 0 &&
        !model_->closing_all() &&
        GetTabCount(current_space_) == 0;
  }

  if (closed_last_in_current) {
    // The last tab of this space was closed and the strip picked a tab from
    // another space. Stay here instead, with a fresh New Tab.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SpacesController::FillEmptySpace,
                                  weak_factory_.GetWeakPtr(), current_space_));
    RelayoutTabStrip();
    return;
  }

  if (closed_home && !model_->closing_all()) {
    // Home can't be closed: it comes back (closing it by its menu or a
    // middle-click just returns it to the dashboard).
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SpacesController::EnsureHomeTab,
                                  weak_factory_.GetWeakPtr(), current_space_,
                                  /*activate=*/false));
  }

  if (selection.active_tab_changed() && selection.new_tab) {
    const std::string space = GetTabSpace(selection.new_tab);
    last_active_[space] = selection.new_tab.get();
    if (space != current_space_) {
      SetCurrentSpace(space);
    }
  }
}

void SpacesController::SetCurrentSpace(const std::string& id) {
  if (id == current_space_) {
    return;
  }
  const int old_index = GetCurrentSpaceIndex();
  current_space_ = id;
  const int new_index = GetCurrentSpaceIndex();
  ApplyTint();
  RelayoutTabStrip();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SpacesController::EnsureHomeTab,
                                weak_factory_.GetWeakPtr(), id,
                                /*activate=*/false));
  const int direction = new_index > old_index ? 1 : -1;
  for (Observer& observer : observers_) {
    observer.OnCurrentSpaceChanged(direction);
  }
}

void SpacesController::ApplyTint() {
  BrowserWidget* widget = browser_view_->browser_widget();
  if (!widget) {
    return;
  }
  const Space* space = GetSpace(current_space_);
  const std::optional<SkColor> color =
      space && spaces_.size() > 1 ? space->color : std::nullopt;
  if (widget->user_color_override() == color) {
    return;
  }
  widget->SetUserColorOverride(color);
  widget->ThemeChanged();
}

void SpacesController::RelayoutTabStrip() {
  if (relayout_callback_) {
    relayout_callback_.Run();
  }
}

void SpacesController::SaveTabSpace(tabs::TabInterface* tab) {
  content::WebContents* contents = tab->GetContents();
  BrowserWindowInterface* browser = browser_view_->browser();
  SessionService* session =
      SessionServiceFactory::GetForProfileIfExisting(browser->GetProfile());
  if (!session || !contents) {
    return;
  }
  session->AddTabExtraData(browser->GetSessionID(),
                           sessions::SessionTabHelper::IdForTab(contents),
                           kSpaceSessionKey, GetTabSpace(tab));
}

void SpacesController::OnSpacesPrefChanged() {
  spaces_ = GetSpaces(pref_registrar_.prefs());
  if (!GetSpace(current_space_)) {
    // Deleted (here or in another window): follow the active tab.
    tabs::TabInterface* active = model_->GetActiveTab();
    SetCurrentSpace(active ? GetTabSpace(active) : spaces_.front().id);
  }
  ApplyTint();
  RelayoutTabStrip();
  for (Observer& observer : observers_) {
    observer.OnSpacesChanged();
  }
}

// Hooks -----------------------------------------------------------------

bool IsTabHiddenBySpace(const tabs::TabInterface* tab) {
  if (!tab) {
    return false;
  }
  const SpacesController* controller =
      SpacesController::From(tab->GetBrowserWindowInterface());
  return controller && controller->IsTabHidden(tab);
}

bool IsGroupHiddenBySpace(BrowserWindowInterface* browser,
                          const tab_groups::TabGroupId& group) {
  const SpacesController* controller = SpacesController::From(browser);
  if (!controller) {
    return false;
  }
  TabGroupModel* groups = browser->GetTabStripModel()->group_model();
  if (!groups || !groups->ContainsTabGroup(group)) {
    return false;
  }
  const tabs::TabInterface* first = groups->GetTabGroup(group)->GetFirstTab();
  return first && controller->IsTabHidden(first);
}

bool IsSpaceHomeTab(const tabs::TabInterface* tab) {
  const SpacesController* controller =
      tab ? SpacesController::From(tab->GetBrowserWindowInterface()) : nullptr;
  return controller && controller->IsHomeProtected(tab);
}

std::string GetTabSpaceForSession(const tabs::TabInterface* tab) {
  const SpacesController* controller =
      tab ? SpacesController::From(tab->GetBrowserWindowInterface()) : nullptr;
  return controller ? controller->GetTabSpace(tab) : std::string();
}

void RestoreTabSpace(BrowserWindowInterface* browser,
                     content::WebContents* contents,
                     const std::map<std::string, std::string>& extra_data) {
  SpacesController* controller = SpacesController::From(browser);
  auto it = extra_data.find(kSpaceSessionKey);
  if (!controller || !contents || it == extra_data.end()) {
    return;
  }
  controller->SetTabSpace(tabs::TabInterface::MaybeGetFromContents(contents),
                          it->second);
}

}  // namespace wryling
