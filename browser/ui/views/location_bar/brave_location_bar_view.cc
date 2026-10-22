/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/location_bar/brave_location_bar_view.h"

#include <cstdlib>
#include <memory>

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/feature_list.h"
#include "brave/app/vector_icons/vector_icons.h"
#include "brave/browser/themes/brave_theme_service.h"
#include "brave/browser/ui/color/brave_color_id.h"
#include "brave/browser/ui/page_info/features.h"
#include "brave/browser/ui/tabs/brave_tab_prefs.h"
#include "brave/browser/ui/views/brave_actions/brave_actions_container.h"
#include "brave/browser/ui/views/location_bar/brave_search_conversion/promotion_button_controller.h"
#include "brave/browser/ui/views/location_bar/brave_search_conversion/promotion_button_view.h"
#include "brave/browser/ui/views/location_bar/brave_shields_page_info_controller.h"
#include "brave/browser/ui/views/location_bar/wryling_copy_link_button.h"
#include "brave/browser/ui/views/frame/wryling_command_bar.h"
#include "brave/browser/ui/views/toolbar/brave_toolbar_view.h"
#include "brave/components/commander/common/buildflags/buildflags.h"
#include "brave/components/playlist/core/common/buildflags/buildflags.h"
#include "brave/grit/brave_theme_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/grit/branded_strings.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/vector_icons/vector_icons.h"
#include "components/grit/brave_components_strings.h"
#include "components/version_info/channel.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view_tracker.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

#if BUILDFLAG(ENABLE_PLAYLIST_WEBUI)
#include "brave/browser/ui/tabs/public/brave_tab_features.h"
#include "brave/browser/ui/views/page_action/playlist_page_action_controller.h"
#include "components/tabs/public/tab_interface.h"
#endif

#if BUILDFLAG(ENABLE_COMMANDER)
#include "brave/browser/ui/commander/commander_service_factory.h"
#include "brave/components/commander/browser/commander_frontend_delegate.h"
#include "brave/components/commander/common/features.h"
#endif

namespace {

class BraveLocationBarViewFocusRingHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  BraveLocationBarViewFocusRingHighlightPathGenerator() = default;
  BraveLocationBarViewFocusRingHighlightPathGenerator(
      const BraveLocationBarViewFocusRingHighlightPathGenerator&) = delete;
  BraveLocationBarViewFocusRingHighlightPathGenerator& operator=(
      const BraveLocationBarViewFocusRingHighlightPathGenerator&) = delete;

  // HighlightPathGenerator
  SkPath GetHighlightPath(const views::View* view) override {
    return static_cast<const BraveLocationBarView*>(view)
        ->GetFocusRingHighlightPath();
  }
};

#if BUILDFLAG(IS_MAC)
// Wryling: no focus ring in the sidebar's pill (its focused fill shows focus)
// or the command bar; elsewhere upstream's rule. An empty highlight
// path isn't enough: FocusRing then falls back to upstream's capsule, which
// doesn't match the pill's corners.
//
// FocusRing drops its rule whenever its view leaves the view tree
// (OnViewHierarchyWillBeDeleted), and the address bar moves (into the sidebar,
// into the command bar and back), so this is re-applied after every move; see
// BraveLocationBarView::ViewHierarchyChanged().
void InstallWrylingFocusRingRule(views::FocusRing* focus_ring) {
  focus_ring->SetHasFocusPredicate(base::BindRepeating([](const views::View*
                                                              view) {
    const auto* bar = static_cast<const BraveLocationBarView*>(view);
    if (!bar->IsInitialized() ||
        BraveToolbarView::IsLocationBarInWrylingSidebar(bar) ||
        WrylingCommandBar::IsLocationBarInCommandBar(bar)) {
      return false;
    }
    return bar->GetOmniboxController()->edit_model()->is_caret_visible() &&
           !bar->GetOmniboxController()->IsPopupOpen();
  }));
}
#endif

std::optional<BraveColorIds> GetFocusRingColor(Profile* profile) {
  if (profile->IsGuestSession()) {
    // Don't update color.
    return std::nullopt;
  }

  return kColorLocationBarFocusRing;
}

}  // namespace

BraveLocationBarView::BraveLocationBarView(BrowserWindowInterface* browser,
                                           Profile* profile,
                                           CommandUpdater* command_updater,
                                           Delegate* delegate,
                                           bool is_popup_mode)
    : LocationBarView(browser,
                      profile,
                      command_updater,
                      delegate,
                      is_popup_mode) {}

BraveLocationBarView::~BraveLocationBarView() = default;

void BraveLocationBarView::Init() {
  // base method calls Update and Layout
  LocationBarView::Init();
#if BUILDFLAG(IS_MAC)
  // Wryling, test only (never set in normal use): types this text into the
  // address bar a few seconds after the window opens, so scripted captures of
  // a scratch profile can see the suggestions dropdown without keyboard
  // access.
  if (const char* text = std::getenv("WRYLING_TEST_OMNIBOX_TEXT");
      text && *text) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<views::ViewTracker> tracker,
               std::u16string text) {
              auto* self = static_cast<BraveLocationBarView*>(tracker->view());
              if (!self || !self->omnibox_view()) {
                return;
              }
              // The dropdown only stays open in the active window.
              if (self->GetWidget()) {
                self->GetWidget()->Activate();
              }
              self->FocusLocation(/*is_user_initiated=*/false,
                                  /*clear_focus_if_failed=*/false);
              self->omnibox_view()->SetUserText(text, /*update_popup=*/true);
            },
            std::make_unique<views::ViewTracker>(this),
            base::UTF8ToUTF16(text)),
        base::Seconds(4));
  }
#endif
  // Change focus ring highlight path
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  if (focus_ring) {
    focus_ring->SetPathGenerator(
        std::make_unique<
            BraveLocationBarViewFocusRingHighlightPathGenerator>());
    if (const auto color_id = GetFocusRingColor(GetProfile())) {
      focus_ring->SetColorId(color_id.value());
    }
#if BUILDFLAG(IS_MAC)
    InstallWrylingFocusRingRule(focus_ring);
#endif
  }

  if (PromotionButtonController::PromotionEnabled(GetProfile()->GetPrefs())) {
    promotion_button_ = AddChildView(std::make_unique<PromotionButtonView>());
    promotion_controller_ = std::make_unique<PromotionButtonController>(
        promotion_button_, omnibox_view_, browser());
  }

  if (page_info::features::IsShowBraveShieldsInPageInfoEnabled()) {
    shields_page_info_controller_ =
        std::make_unique<BraveShieldsPageInfoController>(location_icon_view());
  }

  // Wryling: copy-link button, just left of Shields.
  copy_link_button_ =
      AddChildView(std::make_unique<WrylingCopyLinkButton>(browser_));
  copy_link_button_->SetVisible(false);

  // brave action buttons
  brave_actions_ = AddChildView(
      std::make_unique<BraveActionsContainer>(browser_, GetProfile()));
  brave_actions_->Init();
  // Call Update again to cause a Layout
  Update(nullptr);

  // Stop slide animation for all content settings views icon.
  for (ContentSettingImageView* content_setting_view : content_setting_views_) {
    content_setting_view->disable_animation();
  }
}

#if BUILDFLAG(ENABLE_PLAYLIST_WEBUI)
void BraveLocationBarView::ShowPlaylistBubble(
    playlist::PlaylistBubblesController::BubbleType type) {
  content::WebContents* const contents = GetWebContents();
  if (!contents) {
    return;
  }
  tabs::TabInterface* const tab =
      tabs::TabInterface::MaybeGetFromContents(contents);
  if (!tab) {
    return;
  }
  page_actions::PlaylistPageActionController* const controller =
      tabs::BraveTabFeatures::FromTabFeatures(tab->GetTabFeatures())
          ->playlist_page_action_controller();
  if (!controller) {
    return;
  }
  controller->ShowBubble(type);
}
#endif  // BUILDFLAG(ENABLE_PLAYLIST_WEBUI)

void BraveLocationBarView::Update(content::WebContents* contents) {
  // base Init calls update before our Init is run, so our children
  // may not be initialized yet
  if (brave_actions_) {
    brave_actions_->Update();
  }

  if (shields_page_info_controller_) {
    shields_page_info_controller_->UpdateWebContents(contents);
  }

  UpdateCopyLinkButton();
  LocationBarView::Update(contents);
}

void BraveLocationBarView::OnOmniboxBlurred() {
#if BUILDFLAG(ENABLE_COMMANDER)
  if (base::FeatureList::IsEnabled(features::kBraveCommander)) {
    if (auto* commander_service =
            commander::CommanderServiceFactory::GetForBrowserContext(
                profile_)) {
      commander_service->Hide();
    }
  }
#endif
  LocationBarView::OnOmniboxBlurred();
}

void BraveLocationBarView::Layout(PassKey) {
  if (ignore_layout_) {
    return;
  }

  LayoutSuperclass<LocationBarView>(this);
}

void BraveLocationBarView::OnVisibleBoundsChanged() {
  if (ignore_layout_) {
    return;
  }

  LocationBarView::OnVisibleBoundsChanged();
}

void BraveLocationBarView::OnChanged() {
  auto hide_page_actions = ShouldHidePageActionIcons();
  if (brave_actions_) {
    brave_actions_->SetShouldHide(hide_page_actions);
  }

  if (promotion_controller_) {
    const bool show_button =
        promotion_controller_->ShouldShowSearchPromotionButton() &&
        !ShouldChipOverrideLocationIcon() && !ShouldShowKeywordBubble();
    promotion_controller_->Show(show_button);
  }

  UpdateCopyLinkButton();

  // OnChanged calls Layout
  LocationBarView::OnChanged();
}

std::vector<views::View*> BraveLocationBarView::GetRightMostTrailingViews() {
  std::vector<views::View*> views;
  if (copy_link_button_) {
    views.push_back(copy_link_button_);
  }
  if (brave_actions_) {
    views.push_back(brave_actions_);
  }

  return views;
}

views::View* BraveLocationBarView::GetSearchPromotionButton() const {
  return promotion_button_;
}

ui::ImageModel BraveLocationBarView::GetLocationIcon(
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) {
#if BUILDFLAG(IS_MAC)
  // Wryling: the command bar's field shows a magnifier, like Spotlight.
  if (WrylingCommandBar::IsLocationBarInCommandBar(this)) {
    return ui::ImageModel::FromVectorIcon(
        vector_icons::kSearchIcon, kColorOmniboxResultsIcon,
        GetLayoutConstant(LayoutConstant::kLocationBarIconSize));
  }
  // Internal pages get a quiet gear, like the design mock, instead of the
  // coloured product icon.
  if (!IsEditingOrEmpty() &&
      GetLocationBarModel()->GetURL().SchemeIs(content::kChromeUIScheme)) {
    return ui::ImageModel::FromVectorIcon(
        vector_icons::kSettingsIcon, kColorOmniboxSecurityChipDefault,
        GetLayoutConstant(LayoutConstant::kLocationBarIconSize));
  }
#endif
  return LocationBarView::GetLocationIcon(std::move(on_icon_fetched));
}

void BraveLocationBarView::RefreshBackground() {
  LocationBarView::RefreshBackground();

  // Wryling: over glass the address bar's colour is translucent. Upstream
  // gives the text field the same colour (it assumes an opaque one), which
  // then shows as a second, lighter box behind the URL. Let the bar's own
  // background show through instead.
  if (const auto* colors = GetColorProvider();
      colors && omnibox_view() &&
      SkColorGetA(colors->GetColor(kColorLocationBarBackground)) !=
          SK_AlphaOPAQUE) {
    omnibox_view()->SetBackgroundColor(SK_ColorTRANSPARENT);
  }

#if BUILDFLAG(IS_MAC)
  // Wryling: in the command bar the text sits straight on the panel's glass.
  if (WrylingCommandBar::IsLocationBarInCommandBar(this)) {
    SetBackground(nullptr);
    if (omnibox_view()) {
      omnibox_view()->SetBackgroundColor(SK_ColorTRANSPARENT);
    }
  }
#endif

#if BUILDFLAG(IS_MAC)
  // Wryling: in the sidebar the suggestions float below the bar as their own
  // popover, so a focused bar stays a pill of the space colour, a step
  // brighter, instead of taking the dropdown's colour (upstream's "expanded
  // address bar" look).
  if (const auto* colors = GetColorProvider();
      colors && BraveToolbarView::IsLocationBarInWrylingSidebar(this)) {
    const auto* model = GetOmniboxController()->edit_model();
    if (model->is_caret_visible() || model->user_input_in_progress()) {
      SetBackground(CreateRoundRectBackground(
          colors->GetColor(kColorLocationBarBackgroundHovered),
          SK_ColorTRANSPARENT, size(), SkBlendMode::kSrcOver,
          /*antialias=*/true, /*should_border_scale=*/true));
    }
  }
#endif

  if (shadow_) {
    bool show_shadow =
        IsMouseHovered() &&
        !GetOmniboxController()->edit_model()->is_caret_visible();
#if BUILDFLAG(IS_MAC)
    // Wryling: the sidebar's pill brightens on hover instead (no shadow, as
    // on macOS); Brave's capsule-shaped shadow showed square-ish corners
    // around the pill's smaller radius.
    if (BraveToolbarView::IsLocationBarInWrylingSidebar(this) ||
        WrylingCommandBar::IsLocationBarInCommandBar(this)) {
      show_shadow = false;
    }
#endif
    shadow_->SetVisible(show_shadow);
    return;
  }
}

void BraveLocationBarView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  LocationBarView::OnBoundsChanged(previous_bounds);
  // Wryling: a narrow sidebar leaves the address bar little room; drop the
  // Rewards button there so the address isn't squeezed to "...".
  constexpr int kCompactWidth = 240;
  if (brave_actions_) {
    brave_actions_->SetCompact(width() > 0 && width() < kCompactWidth);
  }
  UpdateCopyLinkButton();
}

void BraveLocationBarView::UpdateCopyLinkButton() {
  if (!copy_link_button_) {
    return;
  }
  // Only in the sidebar's address pill, for a real page, and not while the
  // address is being edited or the bar is very narrow.
  constexpr int kMinWidth = 150;
  const bool show =
      BraveToolbarView::IsLocationBarInWrylingSidebar(this) &&
      !WrylingCommandBar::IsLocationBarInCommandBar(this) &&
      !ShouldHidePageActionIcons() && width() >= kMinWidth &&
      WrylingCopyLinkButton::CanCopy(GetWebContents());
  if (copy_link_button_->GetVisible() != show) {
    copy_link_button_->SetVisible(show);
  }
}

int BraveLocationBarView::GetMinimumTrailingWidth() const {
  int trailing_width = LocationBarView::GetMinimumTrailingWidth();
  const int elem_pad =
      GetLayoutConstant(LayoutConstant::kLocationBarElementPadding);

  if (brave_actions_ && brave_actions_->GetVisible()) {
    trailing_width += brave_actions_->GetMinimumSize().width() + elem_pad;
  }

  return trailing_width;
}

gfx::Size BraveLocationBarView::GetMinimumSize() const {
  gfx::Size min_size = LocationBarView::GetMinimumSize();
  if (!IsInitialized()) {
    return min_size;
  }

  // Skip the additive formula for non-normal browser windows (popups, apps).
  //
  // The additive minimum-width formula produces a larger minimum width than
  // upstream's max(omnibox, leading+trailing) formula. For popup windows opened
  // via JavaScript (e.g. window.open(..., 'width=200')), Chromium's
  // popup-clamping code computes the on-screen position using the *requested*
  // width, then the window is later expanded to meet the minimum. If our
  // minimum exceeds the requested width, the position calculated for the
  // requested width is no longer valid for the actual (larger) window, causing
  // the popup to extend partially outside the work area. Verified by
  // PopupTest.OpenClampedToCurrentDisplay.
  if (browser_->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL) {
    return min_size;
  }

  // Unlike upstream which uses max(omnibox, leading+trailing), reserve space
  // for all children simultaneously so the omnibox always has its minimum
  // alongside all visible decorations.
  const int padding =
      GetLayoutConstant(LayoutConstant::kLocationBarElementPadding);
  const int width = GetInsets().width() + GetMinimumLeadingWidth() + padding +
                    omnibox_view_->GetMinimumSize().width() +
                    GetMinimumTrailingWidth();
  min_size.set_width(width);
  return min_size;
}

void BraveLocationBarView::OnThemeChanged() {
  LocationBarView::OnThemeChanged();

  if (!IsInitialized()) {
    return;
  }

  Update(nullptr);
  SetupShadow();
}

void BraveLocationBarView::AddedToWidget() {
  SetupShadow();
}

void BraveLocationBarView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  LocationBarView::ViewHierarchyChanged(details);
#if BUILDFLAG(IS_MAC)
  // The address bar (or the toolbar holding it) was re-attached somewhere.
  // FocusRing loses both its rule and its focus observation when its view is
  // moved, and then sticks in whatever state it was in (the stray capsule
  // ring). In the sidebar and the command bar there is no ring at all;
  // anywhere else, a fresh one with upstream's rule.
  if (!details.is_add ||
      (details.child != this && !details.child->Contains(this))) {
    return;
  }
  const bool wants_ring =
      !BraveToolbarView::IsLocationBarInWrylingSidebar(this) &&
      !WrylingCommandBar::IsLocationBarInCommandBar(this);
  if (!wants_ring) {
    if (views::FocusRing::Get(this)) {
      views::FocusRing::Remove(this);
      wryling_removed_focus_ring_ = true;
    }
    return;
  }
  // Only put back a ring we took away (popup windows never had one).
  if (wryling_removed_focus_ring_ && !views::FocusRing::Get(this)) {
    wryling_removed_focus_ring_ = false;
    views::FocusRing::Install(this);
    auto* focus_ring = views::FocusRing::Get(this);
    focus_ring->SetOutsetFocusRingDisabled(true);
    focus_ring->SetPathGenerator(
        std::make_unique<BraveLocationBarViewFocusRingHighlightPathGenerator>());
    if (const auto color_id = GetFocusRingColor(GetProfile())) {
      focus_ring->SetColorId(color_id.value());
    }
    InstallWrylingFocusRingRule(focus_ring);
  }
#endif
}

void BraveLocationBarView::ChildVisibilityChanged(views::View* child) {
  LocationBarView::ChildVisibilityChanged(child);
  // Normally, PageActionIcons are in a container which is always visible, only
  // the size changes when an icon is shown or hidden. The LocationBarView
  // does not listen to ChildVisibilityChanged events so we must make we Layout
  // and re-caculate trailing decorator positions when a child changes.
  if (std::ranges::contains(GetLeftMostTrailingViews(), child) ||
      std::ranges::contains(GetRightMostTrailingViews(), child)) {
    DeprecatedLayoutImmediately();
    SchedulePaint();
  }
}

void BraveLocationBarView::SetupShadow() {
  const auto* const color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }

  const int radius = GetBorderRadius();
  ViewShadow::ShadowParameters shadow{
      .offset_x = 0,
      .offset_y = 1,
      .blur_radius = radius,
      .shadow_color = color_provider->GetColor(kColorLocationBarHoveredShadow)};

  shadow_ =
      std::make_unique<ViewShadow>(this, gfx::RoundedCornersF(radius), shadow);
}

int BraveLocationBarView::GetBorderRadius() const {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, size());
}

void BraveLocationBarView::FocusLocation(bool is_user_initiated,
                                         bool clear_focus_if_failed) {
  if (base::FeatureList::IsEnabled(tabs::kBraveSharedPinnedTabs) &&
      browser_->GetProfile()->GetPrefs()->GetBoolean(
          brave_tabs::kSharedPinnedTab)) {
    // When updating dummy contents, this could be called even when the widget
    // is inactive. We shouldn't focus the omnibox in that case.
    if (auto* widget = GetWidget(); !widget || !widget->IsActive()) {
      return;
    }
  }

  omnibox_view_->SetFocus(is_user_initiated);
}

SkPath BraveLocationBarView::GetFocusRingHighlightPath() const {
#if BUILDFLAG(IS_MAC)
  // Wryling: in the command bar the panel itself is the focus, and the
  // sidebar's pill shows focus with its own fill; no ring in
  // either. Upstream's capsule ring didn't match the pill's corners.
  if (WrylingCommandBar::IsLocationBarInCommandBar(this) ||
      BraveToolbarView::IsLocationBarInWrylingSidebar(this)) {
    return SkPath();
  }
#endif
  const SkScalar radius = GetBorderRadius();
  return SkPath::RRect(gfx::RectToSkRect(GetLocalBounds()), radius, radius);
}

ContentSettingImageView*
BraveLocationBarView::GetContentSettingsImageViewForTesting(size_t idx) {
  DCHECK(idx < content_setting_views_.size());
  return content_setting_views_[idx];
}

BEGIN_METADATA(BraveLocationBarView)
END_METADATA
