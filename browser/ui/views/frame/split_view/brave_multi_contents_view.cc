/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/frame/split_view/brave_multi_contents_view.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

#include "base/check.h"
#include "base/i18n/rtl.h"
#include "brave/browser/ui/views/frame/split_view/wryling_split_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/split_tab_data.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "base/strings/string_number_conversions.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_commands.h"
#include "components/vector_icons/vector_icons.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"
#include "brave/browser/ui/views/frame/split_view/brave_contents_container_view.h"
#include "chrome/browser/devtools/devtools_ui_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_background_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_delegate.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

// A gutter between two panes (or two groups of panes) that resizes them.
class WrylingSplitDivider : public views::View {
  METADATA_HEADER(WrylingSplitDivider, views::View)

 public:
  explicit WrylingSplitDivider(BraveMultiContentsView* owner) : owner_(owner) {
    SetVisible(false);
  }

  void SetSpec(const wryling::SplitLayout::Divider& spec, bool is_root,
               int gutter) {
    spec_ = spec;
    is_root_ = is_root;
    gutter_ = gutter;
  }

  // views::View:
  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::Cursor(spec_.axis == wryling::SplitLayout::Axis::kSideBySide
                          ? ui::mojom::CursorType::kColumnResize
                          : ui::mojom::CursorType::kRowResize);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (!event.IsOnlyLeftMouseButton()) {
      return false;
    }
    if (event.GetClickCount() == 2) {
      owner_->OnDividerDragged(spec_.node, is_root_, 0.5, /*done=*/true);
      return true;
    }
    dragging_ = true;
    SchedulePaint();
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (dragging_) {
      owner_->OnDividerDragged(spec_.node, is_root_, RatioFor(event),
                               /*done=*/false);
    }
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (dragging_) {
      dragging_ = false;
      owner_->OnDividerDragged(spec_.node, is_root_, RatioFor(event),
                               /*done=*/true);
      SchedulePaint();
    }
  }

  void OnMouseCaptureLost() override {
    dragging_ = false;
    SchedulePaint();
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    hovered_ = true;
    SchedulePaint();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    hovered_ = false;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    if (!hovered_ && !dragging_) {
      return;
    }
    // A small grip in the middle of the gutter.
    const bool dark = color_utils::IsDark(
        GetColorProvider()->GetColor(ui::kColorFrameActive));
    const bool side = spec_.axis == wryling::SplitLayout::Axis::kSideBySide;
    gfx::RectF grip(GetLocalBounds());
    const float thick = 4, length = 36;
    grip.ClampToCenteredSize(side ? gfx::SizeF(thick, length)
                                  : gfx::SizeF(length, thick));
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(dark ? SK_ColorWHITE : SK_ColorBLACK,
                               dragging_ ? 0x8C : 0x59));
    canvas->DrawRoundRect(grip, thick / 2, flags);
  }

 private:
  double RatioFor(const ui::MouseEvent& event) const {
    gfx::Point p = event.location();
    views::View::ConvertPointToTarget(this, parent(), &p);
    const bool side = spec_.axis == wryling::SplitLayout::Axis::kSideBySide;
    const double total =
        (side ? spec_.extent.width() : spec_.extent.height()) - gutter_;
    if (total <= 0) {
      return 0.5;
    }
    const double first =
        (side ? p.x() - spec_.extent.x() : p.y() - spec_.extent.y()) -
        gutter_ / 2.0;
    // Snap to the middle within 15 points.
    if (std::abs(first - total / 2) < 15) {
      return 0.5;
    }
    return std::clamp(first / total, 0.05, 0.95);
  }

  raw_ptr<BraveMultiContentsView> owner_;
  wryling::SplitLayout::Divider spec_;
  bool is_root_ = false;
  int gutter_ = 0;
  bool hovered_ = false;
  bool dragging_ = false;
};

BEGIN_METADATA(WrylingSplitDivider)
END_METADATA

// The slim header a split pane shows while the pointer is at its top edge:
// the site's icon and domain, then "separate" and "close".
class WrylingPaneHeader : public views::View {
  METADATA_HEADER(WrylingPaneHeader, views::View)

 public:
  static constexpr int kHeight = 30;

  // Dragging the header (not its buttons) moves the pane: phase 0 update,
  // 1 drop, 2 cancel; `screen` is the pointer.
  using DragCallback =
      base::RepeatingCallback<void(int phase, const gfx::Point& screen)>;

  WrylingPaneHeader(base::RepeatingClosure copy_link,
                    base::RepeatingClosure separate,
                    base::RepeatingClosure close,
                    DragCallback drag)
      : drag_(std::move(drag)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetOpacity(0.f);
    SetVisible(false);
    SetProperty(views::kViewIgnoredByLayoutKey, true);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::TLBR(0, 10, 0, 4),
        7));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout_ = layout;
    favicon_ = AddChildView(std::make_unique<views::ImageView>());
    favicon_->SetImageSize(gfx::Size(14, 14));
    domain_ = AddChildView(std::make_unique<views::Label>());
    domain_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    domain_->SetElideBehavior(gfx::ELIDE_HEAD);
    domain_->SetSubpixelRenderingEnabled(false);
    domain_->SetAutoColorReadabilityEnabled(false);
    domain_->SetFontList(domain_->font_list().DeriveWithSizeDelta(1).DeriveWithWeight(
        gfx::Font::Weight::MEDIUM));
    layout->SetFlexForView(domain_, 1);
    copy_link_ = AddButton(
        base::BindRepeating(
            [](WrylingPaneHeader* header, base::RepeatingClosure copy) {
              copy.Run();
              header->ShowCopied();
            },
            base::Unretained(this), std::move(copy_link)),
        vector_icons::kLinkIcon, u"Copy link");
    separate_ = AddButton(std::move(separate), kSplitSceneIcon, u"Separate");
    close_ = AddButton(std::move(close), kCloseTabChromeRefreshOldIcon,
                       u"Close pane");
  }

  void SetSite(const ui::ImageModel& favicon, const std::u16string& domain) {
    favicon_->SetImage(favicon);
    domain_->SetText(domain);
  }

  // kSlim: a label on the window background with separate and close.
  // kChip: a small raised pill in a pane's corner, site only.
  enum class Style { kSlim, kChip };
  void SetStyle(Style style) {
    if (style_ == style) {
      return;
    }
    style_ = style;
    const bool chip = style == Style::kChip;
    copy_link_->SetVisible(!chip);
    separate_->SetVisible(!chip);
    close_->SetVisible(!chip);
    layout_->set_inside_border_insets(chip ? gfx::Insets::VH(0, 9)
                                           : gfx::Insets::TLBR(0, 10, 0, 4));
    layout_->set_between_child_spacing(chip ? 6 : 7);
    domain_->SetFontList(views::Label::GetDefaultFontList().DeriveWithSizeDelta(
        chip ? 0 : 1).DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    InvalidateLayout();
    SchedulePaint();
  }
  bool is_chip() const { return style_ == Style::kChip; }

  void OnPaint(gfx::Canvas* canvas) override {
    if (style_ != Style::kChip) {
      return;  // Slim: the window background shows through.
    }
    // The Wryling raised surface with a hairline, like menus and popovers.
    const ui::ColorProvider* cp = GetColorProvider();
    const bool dark =
        color_utils::IsDark(cp->GetColor(ui::kColorBubbleBackground));
    gfx::RectF r(GetLocalBounds());
    r.Inset(0.5f);
    const float radius = r.height() / 2;
    cc::PaintFlags fill;
    fill.setAntiAlias(true);
    fill.setColor(SkColorSetA(cp->GetColor(ui::kColorBubbleBackground), 0xF2));
    canvas->DrawRoundRect(r, radius, fill);
    cc::PaintFlags stroke;
    stroke.setAntiAlias(true);
    stroke.setStyle(cc::PaintFlags::kStroke_Style);
    stroke.setStrokeWidth(1);
    stroke.setColor(SkColorSetA(dark ? SK_ColorWHITE : SK_ColorBLACK, 0x24));
    canvas->DrawRoundRect(r, radius, stroke);
  }

  views::View* separate_button() { return separate_; }

  // A check mark on the copy button for a moment (Chromium also shows its
  // "Link copied" toast), like the address bar's copy button.
  void ShowCopied() {
    SetCopyIcon(kCheckIcon, u"Copied");
    copied_timer_.Start(FROM_HERE, base::Milliseconds(1500),
                        base::BindOnce(&WrylingPaneHeader::SetCopyIcon,
                                       base::Unretained(this),
                                       std::cref(vector_icons::kLinkIcon),
                                       u"Copy link"));
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (!event.IsOnlyLeftMouseButton()) {
      return false;
    }
    press_ = event.location();
    dragging_ = false;
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (!dragging_ && (std::abs(event.x() - press_.x()) > 4 ||
                       std::abs(event.y() - press_.y()) > 4)) {
      dragging_ = true;
    }
    if (dragging_) {
      gfx::Point screen = event.location();
      views::View::ConvertPointToScreen(this, &screen);
      drag_.Run(0, screen);
    }
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (dragging_) {
      dragging_ = false;
      gfx::Point screen = event.location();
      views::View::ConvertPointToScreen(this, &screen);
      drag_.Run(1, screen);
    }
  }

  void OnMouseCaptureLost() override {
    if (dragging_) {
      dragging_ = false;
      drag_.Run(2, gfx::Point());
    }
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::Cursor(dragging_ ? ui::mojom::CursorType::kGrabbing
                                : ui::mojom::CursorType::kGrab);
  }
  views::View* close_button() { return close_; }

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const ui::ColorProvider* cp = GetColorProvider();
    domain_->SetEnabledColor(cp->GetColor(ui::kColorDialogForeground));
  }

 private:
  views::ImageButton* AddButton(base::RepeatingClosure callback,
                                const gfx::VectorIcon& icon,
                                std::u16string name) {
    auto* button = AddChildView(std::make_unique<views::ImageButton>(
        base::BindRepeating([](base::RepeatingClosure c) { c.Run(); },
                            std::move(callback))));
    button->SetPreferredSize(gfx::Size(24, 24));
    button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    views::SetImageFromVectorIconWithColor(
        button, icon, 14,
        views::IconColors(ui::kColorDialogForeground,
                          ui::kColorDialogForeground));
    button->SetTooltipText(name);
    button->GetViewAccessibility().SetName(name);
    views::InstallRoundRectHighlightPathGenerator(button, gfx::Insets(), 6);
    return button;
  }

  raw_ptr<views::BoxLayout> layout_ = nullptr;
  Style style_ = Style::kSlim;
  raw_ptr<views::ImageView> favicon_ = nullptr;
  raw_ptr<views::Label> domain_ = nullptr;
  void SetCopyIcon(const gfx::VectorIcon& icon, std::u16string name) {
    views::SetImageFromVectorIconWithColor(
        copy_link_, icon, 14,
        views::IconColors(ui::kColorDialogForeground,
                          ui::kColorDialogForeground));
    copy_link_->SetTooltipText(name);
  }

  raw_ptr<views::ImageButton> copy_link_ = nullptr;
  base::OneShotTimer copied_timer_;
  raw_ptr<views::ImageButton> separate_ = nullptr;
  raw_ptr<views::ImageButton> close_ = nullptr;
  DragCallback drag_;
  gfx::Point press_;
  bool dragging_ = false;
};

BEGIN_METADATA(WrylingPaneHeader)
END_METADATA

namespace {

// Gap between split panes.
constexpr int kWrylingPaneGutter = 8;
constexpr base::TimeDelta kPaneSlideDuration = base::Milliseconds(220);
constexpr base::TimeDelta kHeaderSlideDuration = base::Milliseconds(160);

// Returns |corner_radii| with the corners that meet the split divider replaced
// by the smaller border radius.
gfx::RoundedCornersF GetSplitContentsCornerRadii(
    const gfx::RoundedCornersF& corner_radii,
    bool is_first,
    bool is_last) {
  if (corner_radii.IsEmpty()) {
    return corner_radii;
  }

  // In RTL, the first split view is on the right and the last is on the left.
  // Swap directions before setting the visual right and left radii.
  if (base::i18n::IsRTL()) {
    is_last = std::exchange(is_first, is_last);
  }

  const float adjacent_edge_radius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokensOverride::kRoundedCornersBorderRadius);

  gfx::RoundedCornersF split_radii = corner_radii;
  if (!is_first) {
    split_radii.set_upper_left(adjacent_edge_radius);
    split_radii.set_lower_left(adjacent_edge_radius);
  }
  if (!is_last) {
    split_radii.set_upper_right(adjacent_edge_radius);
    split_radii.set_lower_right(adjacent_edge_radius);
  }

  return split_radii;
}

}  // namespace

// static
BraveMultiContentsView* BraveMultiContentsView::From(MultiContentsView* view) {
  CHECK(view);
  return static_cast<BraveMultiContentsView*>(view);
}

BraveMultiContentsView::BraveMultiContentsView(
    BrowserView* browser_view,
    std::unique_ptr<MultiContentsViewDelegate> delegate)
    : MultiContentsView(browser_view, std::move(delegate)) {
  // Use rounded corners margin as resize area's width.
  resize_area_->SetPreferredSize(
      gfx::Size(kRoundedCornersContentsViewMargin, 0));

  // Wryling: one divider per split in the arrangement (three at most).
  for (int i = 0; i < wryling::SplitLayout::kMaxPanes - 1; ++i) {
    dividers_.push_back(
        AddChildView(std::make_unique<WrylingSplitDivider>(this)));
  }
  for (int i = 0; i < wryling::SplitLayout::kMaxPanes; ++i) {
    headers_.push_back(AddChildView(std::make_unique<WrylingPaneHeader>(
        base::BindRepeating(
            [](BraveMultiContentsView* view, int pane) {
              const auto contents = view->GetPaneContents();
              if (pane < static_cast<int>(contents.size())) {
                // Copies the address and shows the "Link copied" toast.
                chrome::CopyURL(view->browser_view_->browser(), contents[pane]);
              }
            },
            base::Unretained(this), i),
        base::BindRepeating(
            [](BraveMultiContentsView* view, int pane) {
              const auto contents = view->GetPaneContents();
              if (pane < static_cast<int>(contents.size()) &&
                  view->split_controller()) {
                view->HidePaneHeaderNow();
                view->split_controller()->SeparatePane(contents[pane]);
              }
            },
            base::Unretained(this), i),
        base::BindRepeating(
            [](BraveMultiContentsView* view, int pane) {
              const auto contents = view->GetPaneContents();
              if (pane >= static_cast<int>(contents.size())) {
                return;
              }
              view->HidePaneHeaderNow();
              TabStripModel* model =
                  view->browser_view_->browser()->GetTabStripModel();
              const int index = model->GetIndexOfWebContents(contents[pane]);
              if (index != TabStripModel::kNoTab) {
                model->CloseWebContentsAt(index,
                                          TabCloseTypes::CLOSE_USER_GESTURE);
              }
            },
            base::Unretained(this), i),
        base::BindRepeating(&BraveMultiContentsView::OnPaneHeaderDrag,
                            base::Unretained(this), i))));
  }
  pane_animation_.SetDuration(kPaneSlideDuration);
  header_slide_.SetSlideDuration(kHeaderSlideDuration);
  header_slide_.SetTweenType(gfx::Tween::EASE_OUT);
  split_controller_ =
      std::make_unique<wryling::WrylingSplitController>(this, browser_view);
}

BraveMultiContentsView::~BraveMultiContentsView() {
  hover_monitor_.reset();
  pane_labels_registrar_.reset();
  hide_header_timer_.Stop();
  pane_animation_.Stop();
  header_slide_.Reset(0);
  split_controller_.reset();
}

void BraveMultiContentsView::UseContentsContainerViewForWebPanel() {
  if (!contents_container_view_for_web_panel_) {
    contents_container_view_for_web_panel_ =
        AddChildView(std::make_unique<BraveContentsContainerView>(
            browser_view_, /*for_web_panel*/ true));
    contents_container_view_for_web_panel_->SetVisible(false);

    auto& view_map =
        container_focusable_map_[contents_container_view_for_web_panel_];
    auto* contents_view =
        contents_container_view_for_web_panel_->contents_view();
    view_map[contents_view->GetClassName()] = contents_view;

    contents_focused_subscriptions_.push_back(
        contents_container_view_for_web_panel_->contents_view()
            ->AddWebContentsFocusedCallback(base::BindRepeating(
                &BraveMultiContentsView::OnWebContentsFocused,
                base::Unretained(this))));
    DevtoolsUIController::From(browser_view_->browser())
        ->MakeSureControllerExists(contents_container_view_for_web_panel_);
  }
}

void BraveMultiContentsView::SetWebPanelContents(
    content::WebContents* web_contents) {
  CHECK(contents_container_view_for_web_panel_);
  contents_container_view_for_web_panel_->contents_view()->SetWebContents(
      web_contents);
  contents_container_view_for_web_panel_->SetVisible(web_contents);
  UpdateContentsBorderAndOverlay();
}

bool BraveMultiContentsView::IsWebPanelVisible() const {
  CHECK(contents_container_view_for_web_panel_);
  return contents_container_view_for_web_panel_->GetVisible();
}

views::View*
BraveMultiContentsView::GetWebPanelContentsViewForTesting()  // IN-TEST
    const {
  return contents_container_view_for_web_panel_
             ? contents_container_view_for_web_panel_->contents_view()
             : nullptr;
}

void BraveMultiContentsView::SetWebPanelWidth(int width) {
  web_panel_width_ = width;
  InvalidateLayout();
}

void BraveMultiContentsView::SetWebPanelOnLeft(bool left) {
  web_panel_on_left_ = left;
  InvalidateLayout();
}

void BraveMultiContentsView::OnShowActiveContentsDomainChanged() {
  UpdateContentsBorderAndOverlay();
}

void BraveMultiContentsView::UpdateContentsCornerRadii(
    const gfx::RoundedCornersF& corner_radii) {
  if (contents_container_view_for_web_panel_) {
    contents_container_view_for_web_panel_->SetContentsCornerRadii(
        corner_radii);
  }
  contents_corner_radii_ = corner_radii;
  ApplyPaneCornerRadii();
  UpdateContentsBorderAndOverlay();
}

void BraveMultiContentsView::ApplyPaneCornerRadii() {
  const gfx::RoundedCornersF& corner_radii = contents_corner_radii_;
  const bool is_in_split = IsInSplitView();
  const size_t view_count = contents_container_views_.size();
  const float adjacent_edge_radius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokensOverride::kRoundedCornersBorderRadius);
  for (size_t i = 0; i < view_count; ++i) {
    auto* container_view =
        BraveContentsContainerView::From(contents_container_views_[i]);
    if (!is_in_split || corner_radii.IsEmpty()) {
      container_view->SetContentsCornerRadii(corner_radii);
      continue;
    }
    if (i >= pane_targets_.size()) {
      container_view->SetContentsCornerRadii(
          GetSplitContentsCornerRadii(corner_radii, i == 0, false));
      continue;
    }
    // Wryling: only corners on the outside of the split keep the page's
    // radius; corners that meet another pane are small.
    const gfx::Rect& r = pane_targets_[i];
    const gfx::Rect& region = pane_region_;
    const bool left = r.x() <= region.x();
    const bool right = r.right() >= region.right();
    const bool top = r.y() <= region.y();
    const bool bottom = r.bottom() >= region.bottom();
    gfx::RoundedCornersF radii = corner_radii;
    if (!(top && left)) {
      radii.set_upper_left(adjacent_edge_radius);
    }
    if (!(top && right)) {
      radii.set_upper_right(adjacent_edge_radius);
    }
    if (!(bottom && left)) {
      radii.set_lower_left(adjacent_edge_radius);
    }
    if (!(bottom && right)) {
      radii.set_lower_right(adjacent_edge_radius);
    }
    container_view->SetContentsCornerRadii(radii);
  }
}

views::ProposedLayout BraveMultiContentsView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  if (!size_bounds.is_fully_bounded()) {
    return MultiContentsView::CalculateProposedLayout(size_bounds);
  }

  views::SizeBounds new_size_bounds = size_bounds;

  // Negative to shrink to make room for web panel.
  const int web_panel_width = GetWebPanelWidth();
  new_size_bounds.Enlarge(-web_panel_width, 0);
  views::ProposedLayout layouts =
      MultiContentsView::CalculateProposedLayout(new_size_bounds);
  LayOutWrylingPanes(layouts);

  // Always hide |background_view_|. Due to layered |background_view_|,
  // our custom border drawn by BraveContentsContainerView is not visible
  // because BraveContentsContainerView doesn't have layer and its parent
  // layer is behind the |background_view_|.
  // We can handle this by having BraveContentsContainerView's own layer but
  // it's resource waste because we don't need |background_view_|. We alreay
  // have CustomBackground that fills contents area with toolbar color.
  auto* background_view_layout = layouts.GetLayoutFor(background_view_.get());
  CHECK(background_view_layout);
  background_view_layout->visible = false;

  if (!contents_container_view_for_web_panel_) {
    return layouts;
  }

  if (web_panel_on_left_) {
    for (auto& layout : layouts.child_layouts) {
      // Move all other views to right to put web panel on left side.
      layout.bounds.Offset(web_panel_width, 0);
    }
  }

  int host_width = size_bounds.width().value();
  int host_height = size_bounds.height().value();
  const int web_panel_x = web_panel_on_left_ ? 0 : host_width - web_panel_width;
  gfx::Rect web_panel_rect(web_panel_x, 0, web_panel_width, host_height);
  layouts.child_layouts.emplace_back(
      contents_container_view_for_web_panel_.get(),
      contents_container_view_for_web_panel_->GetVisible(), web_panel_rect);

  layouts.host_size = gfx::Size(host_width, host_height);
  return layouts;
}

void BraveMultiContentsView::ResetResizeArea() {
  // Give same width on both contents view.
  // Pass true to make delegate save ratio in session service like resizing
  // complete.
  delegate_->ResizeWebContents(0.5, /*done_resizing=*/true);
}

void BraveMultiContentsView::UpdateContentsBorderAndOverlay() {
  if (!contents_container_view_for_web_panel_ ||
      !contents_container_view_for_web_panel_->GetVisible()) {
    MultiContentsView::UpdateContentsBorderAndOverlay();
    return;
  }

  if (!contents_container_view_for_web_panel_->IsActive()) {
    // Web panel is visible but inactive. Hide border of web panel.
    contents_container_view_for_web_panel_->UpdateBorderAndOverlay(
        /*is_in_split*/ false, /*is_active*/ false,
        /*is_highlighted*/ false);
    MultiContentsView::UpdateContentsBorderAndOverlay();
    return;
  }

  // When web panel is active, only it should have active border.
  contents_container_view_for_web_panel_->UpdateBorderAndOverlay(
      /*is_in_split*/ false, /*is_active*/ true,
      /*is_highlighted*/ false);

  for (ContentsContainerView* contents_container_view :
       contents_container_views_) {
    contents_container_view->UpdateBorderAndOverlay(IsInSplitView(),
                                                    /*is_active*/ false,
                                                    /*is_highlighted*/ false);
  }
}

void BraveMultiContentsView::OnWebContentsFocused(views::WebView* web_view) {
  // Early return if web panel is not used.
  if (!contents_container_view_for_web_panel_ ||
      !contents_container_view_for_web_panel_->GetVisible()) {
    MultiContentsView::OnWebContentsFocused(web_view);
    return;
  }

  // When a tab is detached, focus manager could make another web contents
  // focused. We don't need to make web contents' tab activated here. Next
  // active tab could be selected by tab strip model and its web contents will
  // get focused.
  if (!GetActiveContentsView()->web_contents()) {
    return;
  }

  // When tab is activated from Tab UI, we don't need to ask activate
  // |web_view|'s contents to TabStripModel again. It's not activated here when
  // activating tab by clicking contents.
  if (auto* tab =
          tabs::TabInterface::MaybeGetFromContents(web_view->web_contents());
      tab && tab->IsActivated()) {
    return;
  }

  // Base class only gives focus for inactive split tab because that inactive
  // split tab could get focused in upstream.
  // With web panel feature, other tabs also could get focused.
  // When web panel has focus, previously active split tab could get focus.
  // Also, web panel's tab also needs focus.
  // So, notify always.
  delegate_->WebContentsFocused(web_view->web_contents());
}

void BraveMultiContentsView::ExecuteOnEachVisibleContentsView(
    base::RepeatingCallback<void(ContentsWebView*)> callback) {
  if (contents_container_view_for_web_panel_ &&
      contents_container_view_for_web_panel_->GetVisible()) {
    callback.Run(contents_container_view_for_web_panel_->contents_view());
  }

  MultiContentsView::ExecuteOnEachVisibleContentsView(callback);
}

int BraveMultiContentsView::GetWebPanelWidth() const {
  if (!contents_container_view_for_web_panel_ ||
      !contents_container_view_for_web_panel_->GetVisible()) {
    return 0;
  }

  return web_panel_width_;
}

ContentsContainerView* BraveMultiContentsView::GetActiveContentsContainerView()
    const {
  if (is_web_panel_active_) {
    return contents_container_view_for_web_panel_;
  }

  return MultiContentsView::GetActiveContentsContainerView();
}

ContentsWebView* BraveMultiContentsView::GetActiveContentsView() const {
  if (is_web_panel_active_) {
    return contents_container_view_for_web_panel_->contents_view();
  }

  return MultiContentsView::GetActiveContentsView();
}

ContentsContainerView* BraveMultiContentsView::GetContentsContainerViewFor(
    content::WebContents* web_contents) const {
  if (is_web_panel_active_ &&
      contents_container_view_for_web_panel_->contents_view()->web_contents() ==
          web_contents) {
    return contents_container_view_for_web_panel_;
  }

  return MultiContentsView::GetContentsContainerViewFor(web_contents);
}

// Wryling split view -------------------------------------------------------

std::vector<content::WebContents*> BraveMultiContentsView::GetPaneContents()
    const {
  std::vector<content::WebContents*> contents;
  for (const auto& container : contents_container_views_) {
    if (container->GetVisible() && container->contents_view()->web_contents()) {
      contents.push_back(container->contents_view()->web_contents());
    }
    if (!IsInSplitView()) {
      break;
    }
  }
  return contents;
}

std::vector<gfx::Rect> BraveMultiContentsView::GetPaneTargetBounds() const {
  return pane_targets_;
}

int BraveMultiContentsView::GetPaneGutter() const {
  return kWrylingPaneGutter;
}

wryling::SplitLayout BraveMultiContentsView::LayoutForPanes(
    const std::vector<content::WebContents*>& contents) const {
  if (contents.size() < 2) {
    return wryling::SplitLayout::Default(1);
  }
  tabs::TabInterface* first =
      tabs::TabInterface::MaybeGetFromContents(contents[0]);
  if (!first || !first->GetSplit() || !split_controller_) {
    return wryling::SplitLayout::Default(static_cast<int>(contents.size()));
  }
  TabStripModel* model = browser_view_->browser()->GetTabStripModel();
  if (!model->ContainsSplit(*first->GetSplit())) {
    return wryling::SplitLayout::Default(static_cast<int>(contents.size()));
  }
  return split_controller_->LayoutFor(
      *first->GetSplit(), static_cast<int>(contents.size()),
      *model->GetSplitData(*first->GetSplit())->visual_data());
}

void BraveMultiContentsView::LayOutWrylingPanes(
    views::ProposedLayout& layouts) const {
  auto* first = layouts.GetLayoutFor(contents_container_views_[0].get());
  auto* second = layouts.GetLayoutFor(contents_container_views_[1].get());
  auto* resize = layouts.GetLayoutFor(resize_area_.get());
  if (!first || !second || !resize) {
    return;
  }
  const bool split = IsInSplitView();
  gfx::Rect region = first->bounds;
  if (split) {
    region.Union(second->bounds);
    region.Union(resize->bounds);
    // Upstream's resize handle stays "visible" (it means "is split") but
    // takes no room; Wryling's dividers replace it.
    resize->bounds = gfx::Rect();
  }

  const std::vector<content::WebContents*> contents = GetPaneContents();
  wryling::SplitLayout layout = LayoutForPanes(contents);
  std::vector<gfx::Rect> targets;
  if (split && layout.pane_count() == static_cast<int>(contents.size())) {
    const int min_size = std::min(200, std::max(50, region.width() / 10));
    targets = layout.PaneBounds(region, kWrylingPaneGutter, min_size);
    divider_specs_ = layout.Dividers(region, kWrylingPaneGutter, min_size);
    current_layout_ = layout;
  } else {
    targets = {region};
    divider_specs_.clear();
    current_layout_ = wryling::SplitLayout();
  }

  // Only the real layout (not a size query) moves panes.
  const bool is_real =
      layouts.host_size == gfx::Size(width() - GetWebPanelWidth(), height());
  if (is_real) {
    pane_targets_ = targets;
    pane_region_ = region;
  }

  // Start a slide when the panes or their arrangement change (not on a
  // window resize or a divider drag).
  const int layout_signature =
      layout.pane_count() * 2 +
      (layout.root_axis() == wryling::SplitLayout::Axis::kStacked ? 1 : 0);
  auto* self = const_cast<BraveMultiContentsView*>(this);
  if (is_real && !hold_panes_ &&
      (contents != pane_signature_ ||
       layout_signature != pane_signature_layout_)) {
    std::map<content::WebContents*, gfx::Rect> from;
    bool moves = false;
    for (size_t i = 0; i < contents.size() && i < targets.size(); ++i) {
      gfx::Rect start = targets[i];
      if (auto it = shown_rects_.find(contents[i]); it != shown_rects_.end()) {
        start = it->second;
      } else if (split_controller_) {
        if (auto origin = split_controller_->TakeDropOrigin(contents[i])) {
          start = *origin;
        }
      }
      moves |= start != targets[i];
      from[contents[i]] = start;
    }
    pane_signature_ = contents;
    pane_signature_layout_ = layout_signature;
    if (moves && !pane_signature_.empty() &&
        gfx::Animation::ShouldRenderRichAnimation()) {
      from_rects_ = std::move(from);
      self->pane_animation_.Start();
    }
  }

  const double t =
      pane_animation_.is_animating()
          ? gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT,
                                       pane_animation_.GetCurrentValue())
          : 1.0;
  std::map<content::WebContents*, gfx::Rect> shown;
  for (size_t i = 0; i < contents_container_views_.size(); ++i) {
    ContentsContainerView* container = contents_container_views_[i];
    gfx::Rect rect;
    if (i < contents.size() && i < targets.size()) {
      rect = targets[i];
      if (hold_panes_) {
        if (auto it = shown_rects_.find(contents[i]); it != shown_rects_.end()) {
          rect = it->second;
        }
      } else if (t < 1.0) {
        if (auto it = from_rects_.find(contents[i]); it != from_rects_.end()) {
          rect = gfx::Tween::RectValueBetween(t, it->second, targets[i]);
        }
      }
      shown[contents[i]] = rect;
      // Room for the pane's header while it shows: the pane slides down
      // (or, with a pane above it, up from the bottom).
      const bool always = pane_labels_ == PaneLabels::kAlways;
      if (split && (always || static_cast<int>(i) == shown_header_)) {
        const int space = static_cast<int>(std::lround(
            WrylingPaneHeader::kHeight *
            (always ? 1.0 : header_slide_.GetCurrentValue())));
        // Always-shown labels sit above every pane.
        if (always || targets[i].y() <= region.y() + 1) {
          rect.Inset(gfx::Insets::TLBR(space, 0, 0, 0));
        } else {
          rect.Inset(gfx::Insets::TLBR(0, 0, space, 0));
        }
      }
    }
    if (auto* child = layouts.GetLayoutFor(container)) {
      if (i < contents.size()) {
        child->bounds = rect;
      }
    } else {
      layouts.child_layouts.emplace_back(container, container->GetVisible(),
                                         rect);
    }
  }
  if (is_real && !hold_panes_) {
    shown_rects_ = std::move(shown);
  }

  for (size_t i = 0; i < dividers_.size(); ++i) {
    gfx::Rect bounds;
    const bool visible = i < divider_specs_.size() && t >= 1.0;
    if (i < divider_specs_.size()) {
      bounds = divider_specs_[i].bounds;
    }
    if (auto* child = layouts.GetLayoutFor(dividers_[i].get())) {
      child->visible = visible;
      child->bounds = bounds;
    } else {
      layouts.child_layouts.emplace_back(dividers_[i].get(), visible, bounds);
    }
  }
}

void BraveMultiContentsView::Layout(PassKey) {
  LayoutSuperclass<MultiContentsView>(this);
  // Hand each divider what it divides.
  for (size_t i = 0; i < dividers_.size() && i < divider_specs_.size(); ++i) {
    dividers_[i]->SetSpec(divider_specs_[i], /*is_root=*/i == 0,
                          kWrylingPaneGutter);
  }
  ApplyPaneCornerRadii();
  UpdatePaneHeaderBounds();
  UpdateStaticPaneHeaders();
}

void BraveMultiContentsView::AddedToWidget() {
  MultiContentsView::AddedToWidget();
  hover_monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, GetWidget()->GetNativeWindow(),
      {ui::EventType::kMouseMoved, ui::EventType::kMouseExited,
       ui::EventType::kMouseEntered});
  if (!pane_labels_registrar_) {
    PrefService* prefs = browser_view_->browser()->GetProfile()->GetPrefs();
    // Test only: start with a label style (0-3, see PaneLabels).
    if (const char* test = std::getenv("WRYLING_TEST_PANE_LABELS")) {
      int mode = 0;
      if (base::StringToInt(test, &mode)) {
        prefs->SetInteger("wryling.split_view.pane_labels", mode);
      }
    }
    pane_labels_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pane_labels_registrar_->Init(prefs);
    pane_labels_registrar_->Add(
        "wryling.split_view.pane_labels",
        base::BindRepeating(&BraveMultiContentsView::OnPaneLabelsPrefChanged,
                            base::Unretained(this)));
    OnPaneLabelsPrefChanged();
  }
  if (split_controller_) {
    split_controller_->RunTestSwitchesIfSet();
  }
}

void BraveMultiContentsView::OnPaneLabelsPrefChanged() {
  const int mode = pane_labels_registrar_->prefs()->GetInteger(
      "wryling.split_view.pane_labels");
  pane_labels_ = mode >= 0 && mode <= static_cast<int>(PaneLabels::kOff)
                     ? static_cast<PaneLabels>(mode)
                     : PaneLabels::kOnHover;
  HidePaneHeaderNow();
  for (WrylingPaneHeader* header : headers_) {
    header->SetStyle(pane_labels_ == PaneLabels::kCorner
                         ? WrylingPaneHeader::Style::kChip
                         : WrylingPaneHeader::Style::kSlim);
    header->SetVisible(false);
  }
  InvalidateLayout();
}

void BraveMultiContentsView::OnPaneSiteChanged() {
  if (pane_labels_ == PaneLabels::kAlways ||
      pane_labels_ == PaneLabels::kCorner) {
    UpdateStaticPaneHeaders();
  }
}

void BraveMultiContentsView::SetHeaderSite(int pane,
                                           content::WebContents* contents) {
  std::u16string domain = base::UTF8ToUTF16(contents->GetVisibleURL().host());
  if (base::StartsWith(domain, u"www.")) {
    domain = domain.substr(4);
  }
  if (domain.empty()) {
    domain = contents->GetTitle();
  }
  ui::ImageModel favicon;
  if (auto* tab = tabs::TabInterface::MaybeGetFromContents(contents)) {
    if (auto* helper = TabUIHelper::From(tab)) {
      favicon = helper->GetFavicon();
    }
  }
  headers_[pane]->SetSite(favicon, domain);
}

void BraveMultiContentsView::UpdateStaticPaneHeaders() {
  const bool always = pane_labels_ == PaneLabels::kAlways;
  const bool corner = pane_labels_ == PaneLabels::kCorner;
  const std::vector<content::WebContents*> contents = GetPaneContents();
  const bool split = IsInSplitView() && pane_targets_.size() >= 2 &&
                     !pane_animation_.is_animating();
  for (size_t i = 0; i < headers_.size(); ++i) {
    WrylingPaneHeader* header = headers_[i];
    const bool show = (always || corner) && split && i < contents.size() &&
                      i < pane_targets_.size();
    if (!show) {
      if (always || corner) {
        header->SetVisible(false);
      }
      continue;
    }
    SetHeaderSite(static_cast<int>(i), contents[i]);
    const gfx::Rect& pane = pane_targets_[i];
    gfx::Rect r;
    if (always) {
      // Above the pane, in the room the layout left for it.
      r = gfx::Rect(pane.x(), pane.y(), pane.width(), WrylingPaneHeader::kHeight);
    } else {
      // A small pill in the pane's bottom-right corner.
      constexpr int kChipHeight = 24;
      constexpr int kChipInset = 8;
      const int width = std::min(header->GetPreferredSize().width(),
                                 pane.width() - 2 * kChipInset);
      r = gfx::Rect(pane.right() - kChipInset - width,
                    pane.bottom() - kChipInset - kChipHeight, width,
                    kChipHeight);
    }
    header->SetBoundsRect(r);
    if (!header->GetVisible()) {
      ReorderChildView(header, children().size());
      header->SetVisible(true);
    }
    header->layer()->SetOpacity(1.f);
  }
}

void BraveMultiContentsView::OnDividerDragged(int node,
                                              bool is_root,
                                              double ratio,
                                              bool done) {
  const std::vector<content::WebContents*> contents = GetPaneContents();
  if (contents.size() < 2 || !split_controller_) {
    return;
  }
  tabs::TabInterface* first =
      tabs::TabInterface::MaybeGetFromContents(contents[0]);
  if (!first || !first->GetSplit()) {
    return;
  }
  split_controller_->SetRatio(*first->GetSplit(), node, is_root, ratio, done);
}

void BraveMultiContentsView::HoldPanes(bool hold) {
  hold_panes_ = hold;
  InvalidateLayout();
}

void BraveMultiContentsView::OnEvent(const ui::Event& event) {
  if (header_dragging_ || pane_labels_ != PaneLabels::kOnHover) {
    return;
  }
  if (!IsInSplitView() || pane_targets_.size() < 2 || !GetWidget() ||
      !GetWidget()->IsVisible()) {
    HidePaneHeader();
    return;
  }
  gfx::Point p = display::Screen::Get()->GetCursorScreenPoint();
  views::View::ConvertPointFromScreen(this, &p);
  // Over the header that's showing: keep it.
  if (shown_header_ >= 0 && headers_[shown_header_]->GetVisible()) {
    gfx::Rect r = headers_[shown_header_]->bounds();
    r.Outset(gfx::Outsets::VH(4, 4));
    if (r.Contains(p)) {
      hide_header_timer_.Stop();
      return;
    }
  }
  // Only the window border outside a pane, never the page itself: the strip
  // above a top pane, or below a pane with another pane above it (so working
  // near the bottom of the upper pane doesn't bring up the lower one's).
  constexpr int kBorderZone = 10;
  for (size_t i = 0; i < pane_targets_.size(); ++i) {
    const gfx::Rect& pane = pane_targets_[i];
    const bool at_top = pane.y() <= pane_region_.y() + 1;
    const gfx::Rect zone =
        at_top ? gfx::Rect(pane.x(), pane.y() - kBorderZone, pane.width(),
                           kBorderZone)
               : gfx::Rect(pane.x(), pane.bottom(), pane.width(), kBorderZone);
    if (zone.Contains(p)) {
      hide_header_timer_.Stop();
      ShowPaneHeader(static_cast<int>(i));
      return;
    }
  }
  if (shown_header_ >= 0 && !hide_header_timer_.IsRunning()) {
    hide_header_timer_.Start(FROM_HERE, base::Milliseconds(250),
                             base::BindOnce(&BraveMultiContentsView::HidePaneHeader,
                                            base::Unretained(this)));
  }
}

void BraveMultiContentsView::ShowPaneHeader(int pane) {
  if (shown_header_ == pane) {
    header_slide_.Show();  // Back out if it was sliding away.
    return;
  }
  HidePaneHeaderNow();
  const std::vector<content::WebContents*> contents = GetPaneContents();
  if (pane < 0 || pane >= static_cast<int>(contents.size()) ||
      pane >= static_cast<int>(headers_.size())) {
    return;
  }
  SetHeaderSite(pane, contents[pane]);
  WrylingPaneHeader* header = headers_[pane];
  shown_header_ = pane;
  ReorderChildView(header, children().size());
  header->SetVisible(true);
  header_slide_.SetSlideDuration(gfx::Animation::ShouldRenderRichAnimation()
                                     ? kHeaderSlideDuration
                                     : base::TimeDelta());
  header_slide_.Show();
  InvalidateLayout();
}

void BraveMultiContentsView::HidePaneHeader() {
  hide_header_timer_.Stop();
  if (shown_header_ < 0) {
    return;
  }
  header_slide_.Hide();  // AnimationEnded() finishes the hide.
}

void BraveMultiContentsView::HidePaneHeaderNow() {
  hide_header_timer_.Stop();
  if (shown_header_ < 0) {
    return;
  }
  WrylingPaneHeader* header = headers_[shown_header_];
  shown_header_ = -1;
  header_slide_.Reset(0);
  header->layer()->SetOpacity(0.f);
  header->SetVisible(false);
  InvalidateLayout();
}

void BraveMultiContentsView::UpdatePaneHeaderBounds() {
  if (shown_header_ < 0) {
    return;
  }
  if (shown_header_ >= static_cast<int>(pane_targets_.size()) ||
      !IsInSplitView()) {
    HidePaneHeaderNow();
    return;
  }
  // In the room the pane made by sliding aside, on the window background.
  const gfx::Rect& pane = pane_targets_[shown_header_];
  const bool at_top = pane.y() <= pane_region_.y() + 1;
  gfx::Rect r = pane;
  r.set_y(at_top ? pane.y() : pane.bottom() - WrylingPaneHeader::kHeight);
  r.set_height(WrylingPaneHeader::kHeight);
  headers_[shown_header_]->SetBoundsRect(r);
  headers_[shown_header_]->layer()->SetOpacity(
      static_cast<float>(header_slide_.GetCurrentValue()));
}

void BraveMultiContentsView::OnPaneHeaderDrag(int pane,
                                              int phase,
                                              const gfx::Point& screen) {
  const std::vector<content::WebContents*> contents = GetPaneContents();
  if (!split_controller_ || pane >= static_cast<int>(contents.size())) {
    return;
  }
  gfx::Point p = screen;
  views::View::ConvertPointFromScreen(this, &p);
  const bool over_page = GetLocalBounds().Contains(p);
  std::optional<wryling::WrylingSplitController::DropZone> zone;
  if (phase != 2 && over_page) {
    zone = split_controller_->ZoneAt(p, contents[pane]);
  }
  if (phase == 0) {
    header_dragging_ = true;
    hide_header_timer_.Stop();
    split_controller_->ShowPreview(zone);
    return;
  }
  header_dragging_ = false;
  split_controller_->ShowPreview(std::nullopt);
  HidePaneHeader();
  if (phase != 1) {
    return;
  }
  if (zone) {
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(contents[pane]);
    if (tab) {
      split_controller_->Drop(tab->GetHandle(), *zone);
    }
  } else if (!over_page && GetWidget() &&
             GetWidget()->GetWindowBoundsInScreen().Contains(screen)) {
    // Dropped on the sidebar (or anywhere off the page): out of the split.
    split_controller_->SeparatePane(contents[pane]);
  }
}

std::optional<gfx::Rect> BraveMultiContentsView::GetPaneHeaderButtonForTesting(
    int pane,
    bool close) const {
  if (pane < 0 || pane >= static_cast<int>(headers_.size()) ||
      !headers_[pane]->GetVisible() || headers_[pane]->is_chip()) {
    return std::nullopt;
  }
  WrylingPaneHeader* header = headers_[pane];
  return (close ? header->close_button() : header->separate_button())
      ->GetBoundsInScreen();
}

void BraveMultiContentsView::AnimationProgressed(
    const gfx::Animation* animation) {
  InvalidateLayout();
}

void BraveMultiContentsView::AnimationEnded(const gfx::Animation* animation) {
  if (animation == &header_slide_ && header_slide_.GetCurrentValue() == 0) {
    HidePaneHeaderNow();
  }
  InvalidateLayout();
}

BEGIN_METADATA(BraveMultiContentsView)
END_METADATA
