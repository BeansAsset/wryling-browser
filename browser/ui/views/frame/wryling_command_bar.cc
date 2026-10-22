/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/frame/wryling_command_bar.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "brave/browser/ui/views/toolbar/brave_toolbar_view.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_MAC)
#include "ui/views/bubble/popup_glass_mac.h"
#endif

namespace {

// The panel's corner radius (a capsule around the field, like Spotlight; the
// field is 44 tall), how much of the space colour tints its glass (light, so
// the page shows through), its widest width, and how much larger the field's
// text is than the sidebar address bar's.
constexpr int kPanelRadius = 22;
constexpr float kPanelTintScale = 0.3f;
constexpr int kPanelMaxWidth = 640;
constexpr int kFieldFontSizeDelta = 3;

// Padding around the address bar inside the panel: the same insets the
// suggestions use around it, so their (transparent) address-bar area lines
// up exactly over the field and the two read as one panel.
gfx::Insets PanelPadding() {
  return RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets();
}

// The panel's contents: the address bar at the top, inset; the panel grows
// below it while suggestions show. Escape closes the panel.
class CommandBarPanel : public views::View {
  METADATA_HEADER(CommandBarPanel, views::View)

 public:
  CommandBarPanel(WrylingCommandBar* owner, base::RepeatingClosure on_escape)
      : owner_(owner), on_escape_(std::move(on_escape)) {
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  }

  WrylingCommandBar* owner() const { return owner_; }

  // views::View:
  void Layout(PassKey) override {
    if (children().empty()) {
      return;
    }
    views::View* const field = children().front();
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(PanelPadding());
    bounds.set_height(field->GetPreferredSize().height());
    field->SetBoundsRect(bounds);
  }
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    on_escape_.Run();
    return true;
  }

 private:
  const raw_ptr<WrylingCommandBar> owner_;
  base::RepeatingClosure on_escape_;
};

BEGIN_METADATA(CommandBarPanel)
END_METADATA

}  // namespace

WrylingCommandBar::WrylingCommandBar(BrowserView* browser_view)
    : browser_view_(browser_view) {
  // Switching tabs while the panel is open closes it.
  browser_view_->browser()->tab_strip_model()->AddObserver(this);
  RunTestSwitchIfSet();
}

WrylingCommandBar::~WrylingCommandBar() {
  Hide();
  browser_view_->browser()->tab_strip_model()->RemoveObserver(this);
}

// static
bool WrylingCommandBar::IsLocationBarInCommandBar(
    const views::View* location_bar) {
  return location_bar && location_bar->parent() &&
         views::IsViewClass<CommandBarPanel>(location_bar->parent());
}

// static
WrylingCommandBar* WrylingCommandBar::FromLocationBar(
    const views::View* location_bar) {
  const auto* panel = location_bar && location_bar->parent()
                          ? views::AsViewClass<CommandBarPanel>(
                                location_bar->parent())
                          : nullptr;
  return panel ? panel->owner() : nullptr;
}

gfx::Rect WrylingCommandBar::GetPanelBounds() const {
  // Centred over the page (right of the sidebar), a fifth of the way down.
  const gfx::Rect window =
      browser_view_->GetWidget()->GetWindowBoundsInScreen();
  int left = window.x();
  if (views::View* toolbar = browser_view_->toolbar()) {
    left = std::max(left, toolbar->GetBoundsInScreen().right());
  }
  const int area = window.right() - left;
  const int width = std::min(kPanelMaxWidth, area - 2 * 40);
  const int height =
      browser_view_->GetLocationBarView()->GetPreferredSize().height() +
      PanelPadding().height();
  return gfx::Rect(left + (area - width) / 2,
                   window.y() + window.height() / 5, width, height);
}

void WrylingCommandBar::UpdateGlass() {
#if BUILDFLAG(IS_MAC)
  if (widget_) {
    views::UpdateWrylingPopupGlass(
        widget_.get(),
        gfx::Rect(widget_->GetClientAreaBoundsInScreen().size()),
        kPanelRadius, kPanelTintScale);
  }
#endif
}

void WrylingCommandBar::SetSuggestionsBounds(
    std::optional<gfx::Rect> screen_bounds) {
  // Reported from the suggestions' own layout; resizing the panel there would
  // lay out the address bar in the middle of the popup opening or closing.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WrylingCommandBar::ApplySuggestionsBounds,
                                weak_factory_.GetWeakPtr(), screen_bounds));
}

void WrylingCommandBar::ApplySuggestionsBounds(
    std::optional<gfx::Rect> screen_bounds) {
  if (!widget_) {
    return;
  }
  // The suggestions always start at the field's top-left (their address-bar
  // area lies over it), so only their height matters. Their position can
  // still be stale while their window is being placed.
  gfx::Rect bounds = field_bounds_;
  if (screen_bounds) {
    bounds.set_height(std::max(bounds.height(), screen_bounds->height()));
  }
  if (bounds != widget_->GetWindowBoundsInScreen()) {
    widget_->SetBounds(bounds);
    UpdateGlass();
  }
}

bool WrylingCommandBar::Show() {
#if BUILDFLAG(IS_MAC)
  if (IsShowing()) {
    return true;
  }
  auto* toolbar =
      views::AsViewClass<BraveToolbarView>(browser_view_->toolbar());
  LocationBarView* location_bar = browser_view_->GetLocationBarView();
  if (!toolbar || !toolbar->IsInWrylingSidebar() || !location_bar ||
      location_bar->parent() != toolbar || !location_bar->omnibox_view()) {
    return false;
  }

  // The sidebar address bar may have focus and its suggestions open; close
  // them before moving it, so the popup's state and the address bar agree.
  location_bar->GetOmniboxController()->StopAutocomplete(
      /*clear_result=*/true);
  if (views::FocusManager* focus_manager = location_bar->GetFocusManager()) {
    focus_manager->ClearFocus();
  }

  home_parent_ = location_bar->parent();
  home_index_ = home_parent_->GetIndexOf(location_bar);

  widget_ = std::make_unique<ThemeCopyingWidget>(browser_view_->GetWidget());
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = browser_view_->GetWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDefault;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  field_bounds_ = GetPanelBounds();
  params.bounds = field_bounds_;
  params.name = "WrylingCommandBar";
  widget_->Init(std::move(params));

  panel_ = widget_->SetContentsView(std::make_unique<CommandBarPanel>(
      this, base::BindRepeating(&WrylingCommandBar::Hide,
                                weak_factory_.GetWeakPtr())));
  panel_->AddChildView(home_parent_->RemoveChildViewT(location_bar));
  home_parent_->InvalidateLayout();
  widget_observation_.Observe(widget_.get());

  // An empty field with larger text, sitting straight on the glass (see
  // BraveLocationBarView::RefreshBackground), with a magnifier for its icon
  // (BraveLocationBarView::GetLocationIcon).
  OmniboxViewViews* omnibox = location_bar->omnibox_view();
  home_font_ = omnibox->GetFontList();
  omnibox->SetFontList(home_font_->DeriveWithSizeDelta(kFieldFontSizeDelta));
  omnibox->SetUserText(std::u16string(), /*update_popup=*/false);
  location_bar->location_icon_view()->Update(/*suppress_animations=*/true,
                                             /*force_hide_background=*/false);
  // The panel itself shows focus; the address bar's own ring would draw a
  // second outline inside it.
  if (views::FocusRing* ring = views::FocusRing::Get(location_bar)) {
    ring->SetVisible(false);
  }

  widget_->Show();
  widget_->LayoutRootViewIfNecessary();
  UpdateGlass();
  location_bar->FocusLocation(/*is_user_initiated=*/true,
                              /*clear_focus_if_failed=*/false);
  return true;
#else
  return false;
#endif
}

void WrylingCommandBar::Hide() {
  if (!widget_ || hiding_) {
    return;
  }
  base::AutoReset<bool> reset(&hiding_, true);
  widget_observation_.Reset();

  // The address bar goes back to its place in the sidebar, showing the
  // current page's address again.
  LocationBarView* location_bar = browser_view_->GetLocationBarView();
  if (location_bar && home_parent_ && location_bar->parent() == panel_) {
    location_bar->GetOmniboxController()->StopAutocomplete(
        /*clear_result=*/true);
    std::unique_ptr<LocationBarView> owned =
        panel_->RemoveChildViewT(location_bar);
    if (home_font_ && owned->omnibox_view()) {
      owned->omnibox_view()->SetFontList(*home_font_);
    }
    const size_t index =
        std::min(home_index_.value_or(home_parent_->children().size()),
                 home_parent_->children().size());
    home_parent_->AddChildViewAt(std::move(owned), index);
    home_parent_->InvalidateLayout();
    location_bar->Revert();
    if (views::FocusRing* ring = views::FocusRing::Get(location_bar)) {
      ring->SetVisible(true);
    }
    location_bar->location_icon_view()->Update(
        /*suppress_animations=*/true, /*force_hide_background=*/false);
  }
  panel_ = nullptr;
  widget_->CloseNow();
  widget_.reset();
  home_parent_ = nullptr;
  home_index_.reset();
  home_font_.reset();

  // Keyboard focus returns to the page.
  if (content::WebContents* contents = browser_view_->GetActiveWebContents()) {
    contents->Focus();
  }
}

void WrylingCommandBar::HideSoon() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WrylingCommandBar::Hide, weak_factory_.GetWeakPtr()));
}

void WrylingCommandBar::OnWidgetActivationChanged(views::Widget* widget,
                                                  bool active) {
  if (active || hiding_) {
    return;
  }
  // Clicking elsewhere closes the panel. Wait a moment, so a click on the
  // suggestions (their own window) can open the choice first.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<WrylingCommandBar> self) {
            if (self && self->widget_ && !self->widget_->IsActive()) {
              self->Hide();
            }
          },
          weak_factory_.GetWeakPtr()),
      base::Milliseconds(150));
}

void WrylingCommandBar::OnWidgetDestroying(views::Widget* widget) {
  // The panel went away some other way (e.g. the browser window closing).
  if (!hiding_) {
    Hide();
  }
}

void WrylingCommandBar::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Switching tabs (including to the one a choice just opened) closes the
  // panel, once the tab strip has finished notifying.
  if (IsShowing() && selection.active_tab_changed()) {
    HideSoon();
  }
}

void WrylingCommandBar::RunTestSwitchIfSet() {
#if BUILDFLAG(IS_MAC)
  // Test only (never set in normal use): opens the panel a few seconds after
  // the window opens and, if the value isn't "1", types it in, so scripted
  // captures of a scratch profile can see the command bar.
  const char* value = std::getenv("WRYLING_TEST_COMMAND_BAR");
  if (!value || !*value) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<WrylingCommandBar> self, std::string text) {
            if (!self) {
              return;
            }
            self->browser_view_->GetWidget()->Activate();
            chrome::ExecuteCommand(self->browser_view_->browser(),
                                   IDC_NEW_TAB);
            if (text == "1") {
              return;
            }
            base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::WeakPtr<WrylingCommandBar> self,
                       std::u16string text) {
                      LocationBarView* location_bar =
                          self ? self->browser_view_->GetLocationBarView()
                               : nullptr;
                      if (location_bar && location_bar->omnibox_view()) {
                        location_bar->omnibox_view()->SetUserText(
                            text, /*update_popup=*/true);
                      }
                    },
                    self, base::UTF8ToUTF16(text)),
                base::Milliseconds(1500));
          },
          weak_factory_.GetWeakPtr(), std::string(value)),
      base::Seconds(4));
#endif
}
