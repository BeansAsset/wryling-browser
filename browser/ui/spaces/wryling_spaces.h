/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_SPACES_WRYLING_SPACES_H_
#define BRAVE_BROWSER_UI_SPACES_WRYLING_SPACES_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "brave/browser/ui/spaces/wryling_spaces_hooks.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserView;
class GURL;
class PrefRegistrySimple;
class PrefService;

namespace wryling {

// Profile pref: the list of spaces, shared by every window of the profile.
// Each entry is {"id", "name", "icon", "color"}; "color" is an ARGB int and
// is missing when the space uses the theme colour.
inline constexpr char kSpacesPref[] = "wryling.spaces";

// The tab context menu's "Move to space ▸" item (after Brave's own commands
// in TabContextMenuCommand, below the enum's 6-bit range end).
inline constexpr int kMoveToSpaceTabMenuCommand = 62;

struct Space {
  Space();
  Space(const Space&);
  Space& operator=(const Space&);
  ~Space();

  std::string id;
  std::string name;
  // An SF Symbol name; empty shows a coloured dot.
  std::string icon;
  // Unset: the theme colour from Settings.
  std::optional<SkColor> color;
};

// Every space has a Home tab: pinned first in the space, showing the space's
// dashboard. It is the space's start page and can't be closed or navigated
// away (links from it open in new tabs).
inline constexpr char kHomeURL[] = "chrome://newtab/?home";
bool IsHomeURL(const GURL& url);

// The profile's spaces. Never empty: with nothing saved yet it is the one
// default space, "Personal".
std::vector<Space> GetSpaces(const PrefService* prefs);
void SetSpaces(PrefService* prefs, const std::vector<Space>& spaces);
void RegisterSpacesProfilePrefs(PrefRegistrySimple* registry);

// The symbols offered in the space editor, and the colour swatches.
const std::vector<std::string>& GetSpaceIconChoices();
const std::vector<SkColor>& GetSpaceColorChoices();

// One per normal browser window: which space each tab is in, the current
// space, and switching between them. The current space is always the space
// of the active tab; activating a tab in another space (from an extension,
// "Switch to tab", ⌘⇧T...) switches to that space.
class SpacesController : public TabStripModelObserver {
 public:
  DECLARE_USER_DATA(SpacesController);

  class Observer : public base::CheckedObserver {
   public:
    // `direction` is +1 when the new space is after the old one in the
    // switcher, -1 when before, 0 when the change isn't a move (first run).
    virtual void OnCurrentSpaceChanged(int direction) {}
    // Spaces were added, removed, renamed or recoloured.
    virtual void OnSpacesChanged() {}
  };

  SpacesController(BrowserView* browser_view, TabStripModel* model);
  SpacesController(const SpacesController&) = delete;
  SpacesController& operator=(const SpacesController&) = delete;
  ~SpacesController() override;

  static SpacesController* From(BrowserWindowInterface* browser);
  static const SpacesController* From(const BrowserWindowInterface* browser);

  base::WeakPtr<SpacesController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const std::vector<Space>& spaces() const { return spaces_; }
  TabStripModel* model() { return model_; }
  BrowserWindowInterface* browser();
  const std::string& current_space_id() const { return current_space_; }
  const Space* GetSpace(const std::string& id) const;
  int GetSpaceIndex(const std::string& id) const;
  int GetCurrentSpaceIndex() const { return GetSpaceIndex(current_space_); }
  // The colour to show for a space: its own, or the theme colour.
  SkColor GetDisplayColor(const Space& space) const;

  // The space `tab` is in (always a space that exists).
  std::string GetTabSpace(const tabs::TabInterface* tab) const;
  bool IsTabHidden(const tabs::TabInterface* tab) const;
  int GetTabCount(const std::string& space_id) const;

  // Switching. An empty space gets a fresh New Tab.
  void SwitchToSpace(const std::string& id);
  void SwitchToSpaceAt(int index);
  void SwitchBy(int delta);

  // Editing. CreateSpace() returns the new space's ID; it becomes current.
  std::string CreateSpace();
  void UpdateSpace(const Space& space);
  // Closes the space's tabs. The last remaining space can't be deleted.
  void DeleteSpace(const std::string& id);

  void MoveTabToSpace(tabs::TabInterface* tab, const std::string& id);

  // The space's Home tab, if it has one yet.
  tabs::TabInterface* GetHomeTab(const std::string& space_id) const;
  bool IsHomeTab(const tabs::TabInterface* tab) const;
  // A Home tab the user can't close (all of them, except while their space
  // is being deleted or a duplicate is removed).
  bool IsHomeProtected(const tabs::TabInterface* tab) const {
    return !closing_home_ && IsHomeTab(tab);
  }
  // Gives the space a Home tab (and removes extra ones). `activate` shows it.
  void EnsureHomeTab(const std::string& space_id, bool activate);

  // Colours the window in the current space's colour (once the window's
  // widget exists; later changes apply themselves).
  void ApplyTint();
  void SetTabSpace(tabs::TabInterface* tab, const std::string& id);

  // Run after the visible tabs change, to lay the tab strip out again.
  void set_relayout_callback(base::RepeatingClosure callback) {
    relayout_callback_ = std::move(callback);
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(TabStripModel* tab_strip_model,
                              const TabStripModelChange& change,
                              const TabStripSelectionChange& selection) override;

 private:
  std::string NormalizeSpaceId(const std::string& id) const;
  void SetCurrentSpace(const std::string& id);
  void RelayoutTabStrip();
  void SaveTabSpace(tabs::TabInterface* tab);
  void OnSpacesPrefChanged();
  tabs::TabInterface* FindTabToActivate(const std::string& space_id) const;
  void FillEmptySpace(const std::string& space_id);
  // At startup: Home replaces the plain New Tab a fresh window opens with.
  void SetUpHomeAtStartup();

  const raw_ptr<BrowserView> browser_view_;
  const raw_ptr<TabStripModel> model_;
  ui::ScopedUnownedUserData<SpacesController> scoped_user_data_;
  PrefChangeRegistrar pref_registrar_;

  std::vector<Space> spaces_;
  std::string current_space_;
  // Every tab of the window, mapped to the space ID it was put in.
  std::map<const tabs::TabInterface*, std::string> tab_spaces_;
  // The tab that was active when each space was last left.
  std::map<std::string, raw_ptr<tabs::TabInterface>> last_active_;

  base::RepeatingClosure relayout_callback_;
  // Set while Home tabs are closed on purpose.
  bool closing_home_ = false;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<SpacesController> weak_factory_{this};
};

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_SPACES_WRYLING_SPACES_H_
