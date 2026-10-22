/* Copyright (c) 2026 The Wryling Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/frame/split_view/wryling_split_controller.h"

#include <algorithm>
#include <climits>
#include <set>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "brave/browser/ui/color/wryling_tokens.h"
#include "brave/browser/ui/views/frame/split_view/brave_multi_contents_view.h"
#include "brave/browser/ui/views/frame/split_view/wryling_split_drop.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_target.h"
#include "components/tabs/public/split_tab_data.h"
#include "content/public/browser/web_contents.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace wryling {

namespace {

constexpr base::TimeDelta kPreviewMoveDuration = base::Milliseconds(160);

split_tabs::SplitTabLayout ToSplitLayout(SplitLayout::Axis axis) {
  return axis == SplitLayout::Axis::kSideBySide
             ? split_tabs::SplitTabLayout::kSideBySide
             : split_tabs::SplitTabLayout::kStacked;
}

SplitLayout::Axis ToAxis(split_tabs::SplitTabLayout layout) {
  return layout == split_tabs::SplitTabLayout::kSideBySide
             ? SplitLayout::Axis::kSideBySide
             : SplitLayout::Axis::kStacked;
}

// Half of `pane` on `edge`, `gutter` from the other half; all of it for
// kReplace.
gfx::Rect PreviewRect(const gfx::Rect& pane, SplitEdge edge, int gutter) {
  gfx::Rect r = pane;
  const int half_w = (pane.width() - gutter) / 2;
  const int half_h = (pane.height() - gutter) / 2;
  switch (edge) {
    case SplitEdge::kLeft:
      r.set_width(half_w);
      break;
    case SplitEdge::kRight:
      r.set_x(pane.right() - half_w);
      r.set_width(half_w);
      break;
    case SplitEdge::kTop:
      r.set_height(half_h);
      break;
    case SplitEdge::kBottom:
      r.set_y(pane.bottom() - half_h);
      r.set_height(half_h);
      break;
    case SplitEdge::kReplace:
      r.Inset(gutter * 2);
      break;
  }
  return r;
}

}  // namespace

// The rounded shape that shows where a dragged tab will land. It glides
// between zones.
class SplitDropPreview : public views::View, public gfx::AnimationDelegate {
  METADATA_HEADER(SplitDropPreview, views::View)

 public:
  SplitDropPreview() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetCanProcessEventsWithinSubtree(false);
    SetVisible(false);
    animation_.SetDuration(kPreviewMoveDuration);
  }

  void ShowAt(const gfx::Rect& target) {
    if (!GetVisible()) {
      from_ = target;
      from_.Inset(gfx::Insets::VH(target.height() / 10, target.width() / 10));
    } else {
      from_ = bounds();
    }
    target_ = target;
    SetVisible(true);
    if (gfx::Animation::ShouldRenderRichAnimation()) {
      animation_.Start();
    } else {
      SetBoundsRect(target_);
    }
  }

  void Hide() {
    animation_.Stop();
    SetVisible(false);
  }

  const gfx::Rect& target() const { return target_; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    // The Wryling accent (see AddWrylingTintColorMixer).
    const SkColor accent =
        GetColorProvider()->GetColor(ui::kColorButtonBackgroundProminent);
    gfx::RectF r(GetLocalBounds());
    r.Inset(1.f);
    cc::PaintFlags fill;
    fill.setAntiAlias(true);
    fill.setColor(SkColorSetA(accent, 0x66));
    canvas->DrawRoundRect(r, kRadiusXl, fill);
    cc::PaintFlags stroke;
    stroke.setAntiAlias(true);
    stroke.setStyle(cc::PaintFlags::kStroke_Style);
    stroke.setStrokeWidth(2.f);
    stroke.setColor(accent);
    canvas->DrawRoundRect(r, kRadiusXl, stroke);
  }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    const double t =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, animation->GetCurrentValue());
    SetBoundsRect(gfx::Tween::RectValueBetween(t, from_, target_));
  }
  void AnimationEnded(const gfx::Animation* animation) override {
    SetBoundsRect(target_);
  }

 private:
  gfx::Rect from_;
  gfx::Rect target_;
  gfx::LinearAnimation animation_{this};
};

BEGIN_METADATA(SplitDropPreview)
END_METADATA

// Every live SplitDropTarget (one per window).
std::set<const TabDragTarget*>& LiveDropTargets() {
  static base::NoDestructor<std::set<const TabDragTarget*>> targets;
  return *targets;
}

// The page as a target for a tab dragged out of the sidebar.
class SplitDropTarget : public TabDragTarget {
 public:
  explicit SplitDropTarget(WrylingSplitController* controller)
      : controller_(controller) {
    LiveDropTargets().insert(this);
  }
  ~SplitDropTarget() override {
    LiveDropTargets().erase(this);
    on_destroy_.Notify();
  }

  void set_view(BraveMultiContentsView* view) { view_ = view; }

  // TabDragTarget:
  void OnTabDragEntered() override { entered_ = false; }

  TabDragContext* OnTabDragUpdated(TabDragTarget::DragController& controller,
                                   const gfx::Point& point_in_screen) override {
    const DragSessionData& data = controller.GetSessionData();
    content::WebContents* dragged = nullptr;
    // One tab at a time, and not a whole group (Brave turned Chromium's drop
    // target off because group drags crashed it).
    if (data.num_dragging_tabs() == 1 && data.dragging_groups.empty() &&
        !data.group_header_id()) {
      dragged = data.source_dragged_contents();
    }
    if (dragged && !entered_) {
      entered_ = true;
      controller_->ShowPageUnderDrag(dragged);
    }
    zone_.reset();
    if (dragged) {
      dragged_ = dragged;
      gfx::Point point = point_in_screen;
      views::View::ConvertPointFromScreen(view_, &point);
      zone_ = controller_->ZoneAt(point, dragged);
    }
    controller_->ShowPreview(zone_);
    // Keep the tab attached to the sidebar: no torn-off window over the page.
    return const_cast<TabDragContext*>(controller.GetAttachedContext());
  }

  void OnTabDragExited(const gfx::Point& point_in_screen) override {
    zone_.reset();
    entered_ = false;
    controller_->ShowPreview(std::nullopt);
  }

  void OnTabDragEnded() override {
    zone_.reset();
    entered_ = false;
    controller_->ShowPreview(std::nullopt);
  }

  bool CanDropTab() override { return zone_.has_value() && dragged_; }

  void HandleTabDrop(DragController& controller) override {
    if (!zone_ || !dragged_) {
      return;
    }
    tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(dragged_);
    WrylingSplitController::DropZone zone = *zone_;
    zone_.reset();
    controller_->ShowPreview(std::nullopt);
    if (!tab) {
      return;
    }
    // A tab from another window (or one torn off into its own window) moves
    // into this one first, at the end; Drop() then puts it in place.
    if (!controller_->OwnsTab(tab)) {
      std::unique_ptr<tabs::TabModel> detached =
          controller.DetachTabAtForInsertion(
              controller.GetSessionData().source_view_index_);
      if (!detached) {
        return;
      }
      tab = detached.get();
      controller_->AdoptTab(std::move(detached));
    }
    // After the drag controller has finished with the tab strip.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WrylingSplitController::Drop,
                       controller_->GetWeakPtr(), tab->GetHandle(),
                       std::move(zone)));
  }

  base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback) override {
    return on_destroy_.Add(std::move(callback));
  }

 private:
  raw_ptr<WrylingSplitController> controller_;
  raw_ptr<BraveMultiContentsView> view_ = nullptr;
  std::optional<WrylingSplitController::DropZone> zone_;
  raw_ptr<content::WebContents> dragged_ = nullptr;
  bool entered_ = false;
  base::OnceClosureList on_destroy_;
};

// ---------------------------------------------------------------------------

TabDragTarget* GetSplitDropTarget(views::View* multi_contents_view,
                                  const gfx::Point& point_in_screen) {
  if (!multi_contents_view) {
    return nullptr;
  }
  auto* controller = static_cast<BraveMultiContentsView*>(multi_contents_view)
                         ->split_controller();
  return controller ? controller->GetDropTargetAt(point_in_screen) : nullptr;
}

bool IsSplitDropTarget(const TabDragTarget* target) {
  return target && LiveDropTargets().contains(target);
}

namespace {

// The accent outline around the sidebar tab a dragged tab would merge with.
class MergeHighlight : public views::View {
  METADATA_HEADER(MergeHighlight, views::View)

 public:
  // Which side of the tab the dragged tab will join; a thick bar marks it.
  void SetSide(bool left) {
    if (left_ != left) {
      left_ = left;
      SchedulePaint();
    }
  }

  MergeHighlight() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetCanProcessEventsWithinSubtree(false);
    SetProperty(views::kViewIgnoredByLayoutKey, true);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const SkColor accent =
        GetColorProvider()->GetColor(ui::kColorButtonBackgroundProminent);
    gfx::RectF r(GetLocalBounds());
    r.Inset(1.f);
    cc::PaintFlags fill;
    fill.setAntiAlias(true);
    fill.setColor(SkColorSetA(accent, 0x55));
    canvas->DrawRoundRect(r, kRadiusL, fill);
    cc::PaintFlags stroke;
    stroke.setAntiAlias(true);
    stroke.setStyle(cc::PaintFlags::kStroke_Style);
    stroke.setStrokeWidth(2.f);
    stroke.setColor(accent);
    canvas->DrawRoundRect(r, kRadiusL, stroke);
    cc::PaintFlags bar;
    bar.setAntiAlias(true);
    bar.setColor(accent);
    const float x = left_ ? r.x() + 3 : r.right() - 7;
    canvas->DrawRoundRect(gfx::RectF(x, r.y() + 5, 4, r.height() - 10), 2, bar);
  }

 private:
  bool left_ = false;
};

BEGIN_METADATA(MergeHighlight)
END_METADATA

struct MergeState {
  tabs::TabHandle dragged;
  tabs::TabHandle target;
  SplitEdge edge = SplitEdge::kRight;
  bool previewing = false;
  base::WeakPtr<WrylingSplitController> controller;
  views::ViewTracker highlight;
};

MergeState& Merge() {
  static base::NoDestructor<MergeState> state;
  return *state;
}

void ClearMerge() {
  MergeState& merge = Merge();
  merge.target = tabs::TabHandle();
  if (merge.previewing && merge.controller) {
    merge.controller->PreviewMerge(nullptr, SplitEdge::kRight);
  }
  merge.previewing = false;
  if (views::View* highlight = merge.highlight.view()) {
    merge.highlight.SetView(nullptr);
    highlight->parent()->RemoveChildViewT(highlight);
  }
}

const views::View* SidebarOf(const views::View* view) {
  const views::View* sidebar = nullptr;
  for (const views::View* v = view; v; v = v->parent()) {
    const std::string_view name = v->GetClassName();
    if (name == "BraveVerticalTabStripContainerView" ||
        name == "BraveVerticalTabStripRegionView") {
      sidebar = v;
    }
  }
  return sidebar;
}

}  // namespace

bool UpdateSidebarMergeTarget(TabDragContext* context,
                              const DragSessionData& drag_data,
                              const gfx::Point& point_in_screen) {
  MergeState& merge = Merge();
  const views::View* sidebar = context ? SidebarOf(context) : nullptr;
  // One ungrouped tab, inside Wryling's sidebar.
  if (!sidebar || drag_data.num_dragging_tabs() != 1 ||
      !drag_data.dragging_groups.empty() || drag_data.group_header_id()) {
    ClearMerge();
    return false;
  }
  content::WebContents* dragged = drag_data.source_dragged_contents();
  TabStripModel* model = context->GetTabStripModel();
  tabs::TabInterface* dragged_tab =
      dragged ? tabs::TabInterface::MaybeGetFromContents(dragged) : nullptr;
  if (!dragged_tab || !model) {
    ClearMerge();
    return false;
  }

  // The tab whose row's middle half is under the pointer (the top and bottom
  // quarters still reorder).
  tabs::TabInterface* target = nullptr;
  gfx::Rect target_bounds;
  for (int i = 0; i < model->count(); ++i) {
    content::WebContents* contents = model->GetWebContentsAt(i);
    if (contents == dragged) {
      continue;
    }
    TabSlotView* view = context->GetTabForContents(contents);
    if (!view || !view->GetVisible()) {
      continue;
    }
    const gfx::Rect r = view->GetBoundsInScreen();
    const int band = r.height() / 4;
    if (r.Contains(point_in_screen) && point_in_screen.y() >= r.y() + band &&
        point_in_screen.y() < r.bottom() - band) {
      target = model->GetTabAtIndex(i);
      target_bounds = r;
      break;
    }
  }
  if (target) {
    // Not into a full split, nor the split the tab is already in.
    if (std::optional<split_tabs::SplitTabId> split = target->GetSplit()) {
      auto tabs = model->GetSplitData(*split)->ListTabs();
      if (tabs.size() >= SplitLayout::kMaxPanes ||
          std::ranges::contains(tabs, dragged_tab)) {
        target = nullptr;
      }
    }
  }
  if (!target) {
    // The preview showed the target's page by activating it; give the drag
    // its tab back, or a reorder would move the target (the selection).
    if (merge.previewing && model->GetActiveTab() != dragged_tab) {
      const int index = model->GetIndexOfTab(dragged_tab);
      if (index != TabStripModel::kNoTab) {
        model->ActivateTabAt(index);
      }
    }
    ClearMerge();
    return false;
  }

  merge.dragged = dragged_tab->GetHandle();
  merge.target = target->GetHandle();
  // The left third of the tab joins on its left, the rest on its right.
  merge.edge = point_in_screen.x() < target_bounds.x() + target_bounds.width() / 3
                   ? SplitEdge::kLeft
                   : SplitEdge::kRight;
  if (BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
          context->GetWidget()->GetNativeWindow())) {
    merge.controller = BraveMultiContentsView::From(
                           browser_view->multi_contents_view())
                           ->split_controller()
                           ->GetWeakPtr();
  }

  // Highlight the target row, drawn in the sidebar above the tabs.
  views::View* parent = const_cast<views::View*>(sidebar);
  views::View* highlight = merge.highlight.view();
  if (!highlight || highlight->parent() != parent) {
    ClearMerge();
    merge.target = target->GetHandle();
    highlight = parent->AddChildView(std::make_unique<MergeHighlight>());
    merge.highlight.SetView(highlight);
  }
  gfx::Point origin = target_bounds.origin();
  views::View::ConvertPointFromScreen(parent, &origin);
  gfx::Rect local(origin, target_bounds.size());
  local.Outset(2);
  highlight->SetBoundsRect(local);
  static_cast<MergeHighlight*>(highlight)->SetSide(merge.edge ==
                                                   SplitEdge::kLeft);
  highlight->SetVisible(true);
  // And on the page: where the new pane would go.
  if (merge.controller) {
    merge.controller->PreviewMerge(target, merge.edge);
    merge.previewing = true;
  }
  return true;
}

void DetachDraggedTabFromSplit(TabDragContext* context,
                               const DragSessionData& drag_data) {
  // Called before the drag attaches its views, so count by contents
  // (num_dragging_tabs() reads the not-yet-attached views).
  if (!context || !GetSidebarDragArea(context) ||
      drag_data.tab_drag_data_.size() != 1 || drag_data.group_header_id() ||
      !drag_data.tab_drag_data_[0].contents) {
    return;
  }
  content::WebContents* dragged = drag_data.source_dragged_contents();
  tabs::TabInterface* tab =
      dragged ? tabs::TabInterface::MaybeGetFromContents(dragged) : nullptr;
  if (!tab || !tab->GetSplit()) {
    return;
  }
  if (BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
          context->GetWidget()->GetNativeWindow())) {
    BraveMultiContentsView::From(browser_view->multi_contents_view())
        ->split_controller()
        ->DetachFromSplit(tab);
  }
}

void FinishSidebarMerge(bool completed) {
  MergeState& merge = Merge();
  const tabs::TabHandle dragged = merge.dragged;
  const tabs::TabHandle target = merge.target;
  const SplitEdge edge = merge.edge;
  base::WeakPtr<WrylingSplitController> controller = merge.controller;
  const bool had_target = target.Get() != nullptr;
  ClearMerge();
  merge.dragged = tabs::TabHandle();
  if (!completed || !had_target || !controller) {
    return;
  }
  // After the drag controller has put the tab strip back.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WrylingSplitController::MergeTabs, controller,
                                dragged, target, edge));
}

std::optional<gfx::Rect> GetSidebarDragArea(const views::View* tab_strip) {
  // The outermost sidebar view above the tab strip, if it is in one.
  const views::View* sidebar = nullptr;
  for (const views::View* v = tab_strip; v; v = v->parent()) {
    const std::string_view name = v->GetClassName();
    if (name == "BraveVerticalTabStripContainerView" ||
        name == "BraveVerticalTabStripRegionView") {
      sidebar = v;
    }
  }
  if (!sidebar || !sidebar->GetVisible() || sidebar->bounds().IsEmpty()) {
    return std::nullopt;
  }
  return sidebar->GetBoundsInScreen();
}

// ---------------------------------------------------------------------------

WrylingSplitController::WrylingSplitController(BraveMultiContentsView* view,
                                               BrowserView* browser_view)
    : view_(view), browser_view_(browser_view) {
  drop_target_ = std::make_unique<SplitDropTarget>(this);
  drop_target_->set_view(view);
  preview_ = view_->AddChildView(std::make_unique<SplitDropPreview>());
  model()->AddObserver(this);
}

WrylingSplitController::~WrylingSplitController() {
  if (TabStripModel* m = model()) {
    m->RemoveObserver(this);
  }
  drop_target_.reset();
}

bool WrylingSplitController::OwnsTab(tabs::TabInterface* tab) const {
  return model()->GetIndexOfTab(tab) != TabStripModel::kNoTab;
}

void WrylingSplitController::AdoptTab(std::unique_ptr<tabs::TabModel> tab) {
  TabStripModel* m = model();
  m->InsertDetachedTabAt(m->count(), std::move(tab), AddTabTypes::ADD_NONE);
}

TabStripModel* WrylingSplitController::model() const {
  return browser_view_->browser()->GetTabStripModel();
}

SplitLayout WrylingSplitController::LayoutFor(
    split_tabs::SplitTabId split_id,
    int pane_count,
    const split_tabs::SplitTabVisualData& visual_data) {
  auto it = layouts_.find(split_id);
  if (it == layouts_.end() || it->second.pane_count() != pane_count) {
    it = layouts_.insert_or_assign(split_id, SplitLayout::Default(pane_count))
             .first;
  }
  it->second.SetRoot(ToAxis(visual_data.split_layout()),
                     visual_data.split_ratio());
  return it->second;
}

void WrylingSplitController::SetRatio(split_tabs::SplitTabId split_id,
                                      int node,
                                      bool is_root,
                                      double ratio,
                                      bool done) {
  if (!model()->ContainsSplit(split_id)) {
    return;
  }
  if (is_root) {
    model()->UpdateSplitRatio(split_id, ratio, /*is_intermediate=*/!done);
    return;
  }
  auto it = layouts_.find(split_id);
  if (it == layouts_.end()) {
    return;
  }
  it->second.SetRatio(node, ratio);
  view_->InvalidateLayout();
}

TabDragTarget* WrylingSplitController::GetDropTargetAt(
    const gfx::Point& point_in_screen) {
  if (!browser_view_->GetIsNormalType() || !view_->GetVisible() ||
      !view_->GetWidget()) {
    return nullptr;
  }
  // The whole window except the sidebar (the tab strip claims that first):
  // a tab dragged over the gaps around the page must not tear off into its
  // own window. Points off the page snap to the nearest pane.
  if (!browser_view_->GetBoundsInScreen().Contains(point_in_screen)) {
    return nullptr;
  }
  return drop_target_.get();
}

bool WrylingSplitController::IsDropTarget(const TabDragTarget* target) const {
  return target == drop_target_.get();
}

std::optional<WrylingSplitController::DropZone> WrylingSplitController::ZoneAt(
    const gfx::Point& point,
    content::WebContents* dragged) const {
  const std::vector<content::WebContents*> contents = view_->GetPaneContents();
  const std::vector<gfx::Rect> rects = view_->GetPaneTargetBounds();
  if (contents.empty() || contents.size() != rects.size()) {
    return std::nullopt;
  }

  DropZone zone;
  for (content::WebContents* c : contents) {
    tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(c);
    if (!tab) {
      return std::nullopt;
    }
    zone.panes.push_back(tab->GetHandle());
  }
  if (contents.size() >= 2) {
    tabs::TabInterface* first = zone.panes[0].Get();
    zone.split_id = first->GetSplit();
    if (!zone.split_id) {
      return std::nullopt;
    }
    zone.layout = view_->GetCurrentLayout();
    if (zone.layout.pane_count() != static_cast<int>(contents.size())) {
      return std::nullopt;
    }
  } else {
    zone.layout = SplitLayout::Default(1);
  }

  // The pane under the pointer (or the nearest one, over a gutter).
  int pane = -1;
  int best = INT_MAX;
  for (size_t i = 0; i < rects.size(); ++i) {
    if (rects[i].Contains(point)) {
      pane = static_cast<int>(i);
      break;
    }
    const int dx = std::max({rects[i].x() - point.x(), 0, point.x() - rects[i].right()});
    const int dy = std::max({rects[i].y() - point.y(), 0, point.y() - rects[i].bottom()});
    if (dx + dy < best) {
      best = dx + dy;
      pane = static_cast<int>(i);
    }
  }
  if (pane < 0) {
    return std::nullopt;
  }
  const gfx::Rect& r = rects[pane];
  zone.pane = pane;

  // Dropping a pane on itself does nothing.
  const auto dragged_pane = std::ranges::find_if(
      contents, [dragged](content::WebContents* c) { return c == dragged; });
  const bool dragged_in_split = dragged_pane != contents.end();
  if (dragged_in_split &&
      std::distance(contents.begin(), dragged_pane) == pane) {
    return std::nullopt;
  }
  if (contents.size() == 1 && dragged_in_split) {
    return std::nullopt;
  }

  // Nearest edge, by the pane's diagonals.
  const double px = (point.x() - r.x()) / std::max(1.0, double(r.width()));
  const double py = (point.y() - r.y()) / std::max(1.0, double(r.height()));
  const double d_left = px, d_right = 1 - px, d_top = py, d_bottom = 1 - py;
  const double nearest = std::min({d_left, d_right, d_top, d_bottom});
  if (nearest == d_left) {
    zone.edge = SplitEdge::kLeft;
  } else if (nearest == d_right) {
    zone.edge = SplitEdge::kRight;
  } else if (nearest == d_top) {
    zone.edge = SplitEdge::kTop;
  } else {
    zone.edge = SplitEdge::kBottom;
  }
  // The middle of a split pane, or any pane once there are four, swaps the
  // dragged tab in for that pane's tab.
  const bool full = static_cast<int>(contents.size()) >= SplitLayout::kMaxPanes &&
                    !dragged_in_split;
  if (full || (contents.size() >= 2 && nearest > 0.3)) {
    zone.edge = SplitEdge::kReplace;
  }
  zone.preview = PreviewRect(r, zone.edge, view_->GetPaneGutter());
  return zone;
}

void WrylingSplitController::ShowPreview(const std::optional<DropZone>& zone) {
  if (!zone) {
    preview_->Hide();
    return;
  }
  // Above the panes and dividers.
  view_->ReorderChildView(preview_, view_->children().size());
  preview_->ShowAt(zone->preview);
}

void WrylingSplitController::ShowPageUnderDrag(content::WebContents* dragged) {
  TabStripModel* m = model();
  tabs::TabInterface* active = m->GetActiveTab();
  tabs::TabInterface* previous = previous_active_.Get();
  if (!active || active->GetContents() != dragged || !previous ||
      previous == active) {
    return;
  }
  const int index = m->GetIndexOfTab(previous);
  if (index != TabStripModel::kNoTab) {
    m->ActivateTabAt(index);
  }
}

std::optional<gfx::Rect> WrylingSplitController::TakeDropOrigin(
    content::WebContents* contents) {
  if (!contents || contents != dropped_contents_) {
    return std::nullopt;
  }
  dropped_contents_ = nullptr;
  if (dropped_from_.IsEmpty()) {
    return std::nullopt;
  }
  return dropped_from_;
}

void WrylingSplitController::DetachFromSplit(tabs::TabInterface* tab) {
  TabStripModel* m = model();
  if (!tab || !OwnsTab(tab) || !tab->GetSplit()) {
    return;
  }
  const split_tabs::SplitTabId split = *tab->GetSplit();
  std::vector<tabs::TabInterface*> tabs = m->GetSplitData(split)->ListTabs();
  SplitLayout layout = LayoutFor(split, static_cast<int>(tabs.size()),
                                 *m->GetSplitData(split)->visual_data());
  const int pane = static_cast<int>(
      std::distance(tabs.begin(), std::ranges::find(tabs, tab)));
  std::vector<tabs::TabInterface*> rest = tabs;
  rest.erase(rest.begin() + pane);
  layout.Remove(pane);
  rearranging_ = true;
  m->RemoveSplit(split);
  if (rest.size() >= 2) {
    SplitInOrder(rest, layout);
  }
  rearranging_ = false;
}

void WrylingSplitController::SeparatePane(content::WebContents* contents) {
  TabStripModel* m = model();
  tabs::TabInterface* tab =
      contents ? tabs::TabInterface::MaybeGetFromContents(contents) : nullptr;
  if (!tab || !OwnsTab(tab) || !tab->GetSplit()) {
    return;
  }
  std::vector<tabs::TabInterface*> rest =
      m->GetSplitData(*tab->GetSplit())->ListTabs();
  std::erase(rest, tab);
  DetachFromSplit(tab);
  // Right after the others.
  const int from = m->GetIndexOfTab(tab);
  int last = -1;
  for (tabs::TabInterface* other : rest) {
    last = std::max(last, m->GetIndexOfTab(other));
  }
  if (last >= 0 && from < last) {
    m->MoveWebContentsAt(from, last, /*select_after_move=*/false);
  }
  const int index = m->GetIndexOfTab(tab);
  if (index != TabStripModel::kNoTab) {
    m->ActivateTabAt(index);
  }
}

void WrylingSplitController::PreviewMerge(tabs::TabInterface* target,
                                          SplitEdge edge) {
  if (!target || !OwnsTab(target)) {
    ShowPreview(std::nullopt);
    return;
  }
  const std::vector<content::WebContents*> contents = view_->GetPaneContents();
  const auto it = std::ranges::find(contents, target->GetContents());
  if (it == contents.end()) {
    // Show the target (or its split) first; the preview follows on the
    // next drag update, once the page has laid out.
    ShowPreview(std::nullopt);
    model()->ActivateTabAt(model()->GetIndexOfTab(target));
    return;
  }
  const std::vector<gfx::Rect> rects = view_->GetPaneTargetBounds();
  const size_t pane = std::distance(contents.begin(), it);
  if (pane >= rects.size()) {
    return;
  }
  DropZone zone;
  zone.preview = PreviewRect(rects[pane], edge, view_->GetPaneGutter());
  ShowPreview(zone);
}

void WrylingSplitController::MergeTabs(tabs::TabHandle dragged_handle,
                                       tabs::TabHandle target_handle,
                                       SplitEdge edge) {
  TabStripModel* m = model();
  tabs::TabInterface* dragged = dragged_handle.Get();
  tabs::TabInterface* target = target_handle.Get();
  if (!dragged || !target || dragged == target || !OwnsTab(dragged) ||
      !OwnsTab(target)) {
    return;
  }
  DropZone zone;
  if (std::optional<split_tabs::SplitTabId> split = target->GetSplit()) {
    std::vector<tabs::TabInterface*> tabs = m->GetSplitData(*split)->ListTabs();
    if (tabs.size() >= SplitLayout::kMaxPanes ||
        std::ranges::contains(tabs, dragged)) {
      return;
    }
    for (tabs::TabInterface* tab : tabs) {
      zone.panes.push_back(tab->GetHandle());
    }
    zone.split_id = split;
    zone.layout = LayoutFor(*split, static_cast<int>(tabs.size()),
                            *m->GetSplitData(*split)->visual_data());
    zone.pane = static_cast<int>(
        std::distance(tabs.begin(), std::ranges::find(tabs, target)));
    zone.edge = edge;
  } else {
    zone.panes = {target->GetHandle()};
    zone.layout = SplitLayout::Default(1);
    zone.pane = 0;
    zone.edge = edge;
  }
  Drop(dragged_handle, zone);
}

void WrylingSplitController::Drop(tabs::TabHandle dragged_handle,
                                  const DropZone& zone) {
  TabStripModel* m = model();
  tabs::TabInterface* dragged = dragged_handle.Get();
  if (!dragged || m->GetIndexOfTab(dragged) == TabStripModel::kNoTab) {
    return;
  }
  std::vector<tabs::TabInterface*> tabs;
  for (const tabs::TabHandle& handle : zone.panes) {
    tabs::TabInterface* tab = handle.Get();
    if (!tab || m->GetIndexOfTab(tab) == TabStripModel::kNoTab) {
      return;  // Something closed meanwhile.
    }
    tabs.push_back(tab);
  }
  if (zone.split_id && (!m->ContainsSplit(*zone.split_id) ||
                        tabs[0]->GetSplit() != zone.split_id)) {
    return;
  }

  SplitLayout layout = zone.layout;
  const auto existing = std::ranges::find(tabs, dragged);
  if (existing != tabs.end()) {
    // Moving a pane within its split.
    const int from = static_cast<int>(std::distance(tabs.begin(), existing));
    if (zone.edge == SplitEdge::kReplace) {
      std::swap(tabs[from], tabs[zone.pane]);
    } else {
      tabs.erase(tabs.begin() + from);
      const int to = layout.Move(from, zone.pane, zone.edge);
      tabs.insert(tabs.begin() + to, dragged);
    }
  } else if (zone.edge == SplitEdge::kReplace) {
    tabs[zone.pane] = dragged;
  } else {
    if (layout.pane_count() >= SplitLayout::kMaxPanes) {
      return;
    }
    const int at = layout.Insert(zone.pane, zone.edge);
    tabs.insert(tabs.begin() + at, dragged);
  }

  dropped_contents_ = dragged->GetContents();
  dropped_from_ = zone.preview;

  rearranging_ = true;
  if (zone.split_id) {
    m->RemoveSplit(*zone.split_id);
  }
  if (std::optional<split_tabs::SplitTabId> own = dragged->GetSplit()) {
    m->RemoveSplit(*own);
  }
  SplitInOrder(tabs, layout);
  rearranging_ = false;

  const int index = m->GetIndexOfTab(dragged);
  if (index != TabStripModel::kNoTab) {
    m->ActivateTabAt(index);
  }
}

std::optional<split_tabs::SplitTabId> WrylingSplitController::SplitInOrder(
    const std::vector<tabs::TabInterface*>& tabs,
    const SplitLayout& layout) {
  TabStripModel* m = model();
  if (tabs.size() < 2) {
    return std::nullopt;
  }
  // Match pinnedness to the first tab that isn't moving in from elsewhere:
  // use the majority (the block being joined).
  int pinned = 0;
  for (auto* tab : tabs) {
    pinned += tab->IsPinned() ? 1 : 0;
  }
  const bool want_pinned = pinned * 2 > static_cast<int>(tabs.size());
  for (auto* tab : tabs) {
    if (tab->IsPinned() != want_pinned) {
      m->SetTabPinned(m->GetIndexOfTab(tab), want_pinned);
    }
  }

  // Gather them at the leftmost of their positions, in order.
  int start = INT_MAX;
  for (auto* tab : tabs) {
    start = std::min(start, m->GetIndexOfTab(tab));
  }
  for (size_t i = 0; i < tabs.size(); ++i) {
    const int from = m->GetIndexOfTab(tabs[i]);
    const int to = start + static_cast<int>(i);
    if (from != to) {
      m->MoveWebContentsAt(from, to, /*select_after_move=*/false);
    }
  }
  std::vector<int> indices;
  for (auto* tab : tabs) {
    indices.push_back(m->GetIndexOfTab(tab));
  }
  std::ranges::sort(indices);
  for (size_t i = 1; i < indices.size(); ++i) {
    if (indices[i] != indices[i - 1] + 1) {
      LOG(ERROR) << "Wryling split: tabs didn't line up";
      return std::nullopt;
    }
  }
  const split_tabs::SplitTabId id = m->AddToNewSplit(
      indices,
      split_tabs::SplitTabVisualData(ToSplitLayout(layout.root_axis()),
                                     layout.root_ratio()),
      split_tabs::SplitTabCreatedSource::kDragAndDropTab);
  layouts_.insert_or_assign(id, layout);
  view_->InvalidateLayout();
  return id;
}

void WrylingSplitController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed() && selection.old_tab) {
    previous_active_ = selection.old_tab->GetHandle();
  }
}

void WrylingSplitController::OnTabChangedAt(tabs::TabInterface* tab,
                                            TabChangeType change_type) {
  // A pane's site or favicon may have changed.
  view_->OnPaneSiteChanged();
}

void WrylingSplitController::OnSplitTabChanged(const SplitTabChange& change) {
  if (change.type != SplitTabChange::Type::kRemoved) {
    return;
  }
  auto it = layouts_.find(change.split_id);
  if (it == layouts_.end()) {
    return;
  }
  SplitLayout layout = it->second;
  layouts_.erase(it);
  const SplitTabChange::RemovedChange* removed = change.GetRemovedChange();
  if (rearranging_ || model()->closing_all() ||
      removed->reason() !=
          SplitTabChange::SplitTabRemoveReason::kSplitTabRemoved ||
      removed->tabs().size() < 3 ||
      static_cast<int>(removed->tabs().size()) != layout.pane_count()) {
    return;
  }
  std::vector<tabs::TabHandle> tabs;
  for (const auto& [tab, index] : removed->tabs()) {
    tabs.push_back(tab->GetHandle());
  }
  // Once the close (or move) that broke the split up has finished. Until
  // then the other panes stay where they are.
  view_->HoldPanes(true);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WrylingSplitController::ResplitSurvivors,
                                weak_factory_.GetWeakPtr(), std::move(tabs),
                                std::move(layout)));
}

void WrylingSplitController::ResplitSurvivors(
    std::vector<tabs::TabHandle> handles,
    SplitLayout layout) {
  TabStripModel* m = model();
  absl::Cleanup release = [this] { view_->HoldPanes(false); };
  if (m->closing_all()) {
    return;
  }
  std::vector<tabs::TabInterface*> survivors;
  for (int i = static_cast<int>(handles.size()) - 1; i >= 0; --i) {
    tabs::TabInterface* tab = handles[i].Get();
    if (!tab || m->GetIndexOfTab(tab) == TabStripModel::kNoTab ||
        tab->IsSplit()) {
      layout.Remove(i);
    } else {
      survivors.insert(survivors.begin(), tab);
    }
  }
  // All still here: the split was undone on purpose ("Exit split").
  if (survivors.size() == handles.size() || survivors.size() < 2) {
    return;
  }
  rearranging_ = true;
  SplitInOrder(survivors, layout);
  rearranging_ = false;
}

// ---------------------------------------------------------------------------
// Test switches (never set in normal use): build a split at startup so
// scripted captures of a scratch profile can see every arrangement.
//   WRYLING_TEST_SPLIT=h|v|3|grid     arrangement
//   WRYLING_TEST_SPLIT_CLOSE=<pane>   then close that pane
//   WRYLING_TEST_SPLIT_RATIO=<0..1>   then move the root divider
//   WRYLING_TEST_DROP=<pane>,<l|r|t|b|c>  then show the drop preview there

void WrylingSplitController::RunTestSwitchesIfSet() {
#if BUILDFLAG(IS_MAC)
  RunTestDragIfSet(browser_view_);
#endif
  const char* spec = std::getenv("WRYLING_TEST_SPLIT");
  if (!spec || !*spec) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WrylingSplitController::RunTestStep,
                     weak_factory_.GetWeakPtr(), std::string(spec), 0),
      base::Seconds(3));
}

void WrylingSplitController::RunTestStep(std::string spec, int step) {
  static const char* const kUrls[] = {
      "https://en.wikipedia.org/wiki/Split_screen_(computing)",
      "https://news.ycombinator.com/", "https://www.apple.com/",
      "https://developer.mozilla.org/"};
  const int panes = spec == "h" || spec == "v" ? 2 : spec == "3" ? 3 : 4;
  BrowserWindowInterface* browser = browser_view_->browser();
  TabStripModel* m = model();
  auto next = [&](base::TimeDelta delay) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WrylingSplitController::RunTestStep,
                       weak_factory_.GetWeakPtr(), spec, step + 1),
        delay);
  };

  if (step == 0) {
    // Fresh tabs for the panes beyond the first.
    for (int i = 1; i < panes; ++i) {
      chrome::AddTabAt(browser, GURL(kUrls[i]), -1, /*foreground=*/false);
    }
    next(base::Seconds(2));
    return;
  }
  if (step <= panes - 1) {
    // Drop tab `step` like a drag would: h -> right of pane 0; v -> below it;
    // 3 -> below pane 1; grid -> below pane 0.
    tabs::TabInterface* first = m->GetActiveTab();
    std::vector<tabs::TabInterface*> shown;
    if (auto split = first->GetSplit()) {
      shown = m->GetSplitData(*split)->ListTabs();
    } else {
      shown = {first};
    }
    tabs::TabInterface* last = shown.back();
    tabs::TabInterface* dragged = m->GetTabAtIndex(m->GetIndexOfTab(last) + 1);
    DropZone zone;
    for (auto* t : shown) {
      zone.panes.push_back(t->GetHandle());
    }
    zone.split_id = first->GetSplit();
    zone.layout = shown.size() >= 2 ? view_->GetCurrentLayout()
                                    : SplitLayout::Default(1);
    if (step == 1) {
      zone.pane = 0;
      zone.edge = spec == "v" ? SplitEdge::kBottom : SplitEdge::kRight;
    } else if (step == 2) {
      zone.pane = 1;
      zone.edge = SplitEdge::kBottom;
    } else {
      zone.pane = 0;
      zone.edge = SplitEdge::kBottom;
    }
    const std::vector<gfx::Rect> rects = view_->GetPaneTargetBounds();
    if (zone.pane < static_cast<int>(rects.size())) {
      zone.preview = PreviewRect(rects[zone.pane], zone.edge,
                                 view_->GetPaneGutter());
    }
    LOG(ERROR) << "WRYLING_SPLIT: test drop " << step;
    Drop(dragged->GetHandle(), zone);
    next(base::Milliseconds(1500));
    return;
  }

  // Follow-ups, once.
  if (const char* ratio = std::getenv("WRYLING_TEST_SPLIT_RATIO");
      ratio && *ratio) {
    double value = 0.5;
    if (base::StringToDouble(ratio, &value)) {
      if (auto split = m->GetActiveTab()->GetSplit()) {
        SetRatio(*split, 0, /*is_root=*/true, value, /*done=*/true);
      }
    }
  }
  if (const char* close = std::getenv("WRYLING_TEST_SPLIT_CLOSE");
      close && *close) {
    int pane = 0;
    if (base::StringToInt(close, &pane)) {
      if (auto split = m->GetActiveTab()->GetSplit()) {
        auto tabs = m->GetSplitData(*split)->ListTabs();
        if (pane >= 0 && pane < static_cast<int>(tabs.size())) {
          m->CloseWebContentsAt(m->GetIndexOfTab(tabs[pane]),
                                TabCloseTypes::CLOSE_USER_GESTURE);
        }
      }
    }
  }
  if (const char* drop = std::getenv("WRYLING_TEST_DROP"); drop && *drop) {
    std::vector<std::string> parts = base::SplitString(
        drop, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    int pane = 0;
    if (parts.size() == 2 && base::StringToInt(parts[0], &pane)) {
      const std::vector<gfx::Rect> rects = view_->GetPaneTargetBounds();
      if (pane >= 0 && pane < static_cast<int>(rects.size())) {
        const char e = parts[1][0];
        const SplitEdge edge = e == 'l'   ? SplitEdge::kLeft
                               : e == 'r' ? SplitEdge::kRight
                               : e == 't' ? SplitEdge::kTop
                               : e == 'b' ? SplitEdge::kBottom
                                          : SplitEdge::kReplace;
        DropZone zone;
        zone.preview = PreviewRect(rects[pane], edge, view_->GetPaneGutter());
        ShowPreview(zone);
      }
    }
  }
  LOG(ERROR) << "WRYLING_SPLIT: test ready";
}

#if BUILDFLAG(IS_MAC)
// ---------------------------------------------------------------------------
// Test only: WRYLING_TEST_DRAG replays tab drags with synthetic mouse events
// (the Cocoa half is wryling_split_test_drag_mac.mm).
//   same   drag the 2nd tab onto the page's right half
//   gap    same, pausing over the gap beside the page
//   cross  drag a tab from a second window onto the page
//   back   drag a second window's tab back into the first sidebar
//   merge  drop the 2nd sidebar tab onto the 1st (they become a split)
//   reorder  drag the 2nd tab above the 1st (they swap; no split)
//   header   split, then hover pane 2's top edge (header) and its middle
//   separate / close  split, hover pane 2's top edge, click that button
//   unsplit  split, drag pane 2's sidebar segment below the row
//   panedrag / paneout  split, drag pane 2's header onto pane 1's bottom
//            edge / onto the sidebar
//   mergeleft  like merge, onto the left third (joins on the left)
// Logs "WRYLING_DRAG: hover <n>" while holding still over a drop spot (2 s,
// for captures), then "WRYLING_DRAG: result ..." and "WRYLING_SPLIT: test
// ready".

namespace test_drag {
void SetOverride(bool on);
void PostMouse(int type, int x, int y);  // 0 press, 1 drag, 2 release, 3 move.
}  // namespace test_drag

namespace {

std::vector<BrowserView*> AllBrowserViews() {
  std::vector<BrowserView*> views;
  for (BrowserWindowInterface* window : GetAllBrowserWindowInterfaces()) {
    if (BrowserView* view = BrowserView::GetBrowserViewForBrowser(window)) {
      views.push_back(view);
    }
  }
  return views;
}

TabStrip* FindTabStrip(views::View* root) {
  const std::string_view name = root->GetClassName();
  if (name == "BraveTabStrip" || name == "TabStrip") {
    return static_cast<TabStrip*>(root);
  }
  for (views::View* child : root->children()) {
    if (TabStrip* found = FindTabStrip(child)) {
      return found;
    }
  }
  return nullptr;
}

gfx::Point TabCenter(BrowserView* view, int index) {
  TabStrip* strip = FindTabStrip(view->GetWidget()->GetRootView());
  if (!strip || index >= strip->GetTabCount()) {
    return gfx::Point();
  }
  return strip->tab_at(index)->GetBoundsInScreen().CenterPoint();
}

std::string DescribeWindows() {
  std::string out;
  for (BrowserView* view : AllBrowserViews()) {
    TabStripModel* model = view->browser()->GetTabStripModel();
    out += "[window tabs=" + base::NumberToString(model->count());
    out += " first=" +
           base::UTF16ToUTF8(model->GetWebContentsAt(0)->GetTitle()).substr(0, 12);
    if (tabs::TabInterface* active = model->GetActiveTab();
        active && active->GetSplit()) {
      out += " split=" + base::NumberToString(
                             model->GetSplitData(*active->GetSplit())
                                 ->ListTabs()
                                 .size());
    }
    out += "] ";
  }
  return out;
}

struct DragStep {
  int type;  // 0 press, 1 drag, 2 release.
  gfx::Point at;
  base::TimeDelta wait;
  std::string log;
};

void FinishDrag() {
  LOG(ERROR) << "WRYLING_DRAG: result " << DescribeWindows();
  LOG(ERROR) << "WRYLING_SPLIT: test ready";
}

void RunDragSteps(std::vector<DragStep> steps,
                  size_t next,
                  base::OnceClosure done) {
  if (next >= steps.size()) {
    test_drag::SetOverride(false);
    std::move(done).Run();
    return;
  }
  const DragStep step = steps[next];
  test_drag::PostMouse(step.type, step.at.x(), step.at.y());
  if (!step.log.empty()) {
    LOG(ERROR) << "WRYLING_DRAG: " << step.log << " " << DescribeWindows();
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunDragSteps, std::move(steps), next + 1,
                     std::move(done)),
      step.wait);
}

// Press at `from`, glide to each of `via` (holding still at the ones marked),
// release at the last one.
void Glide(const gfx::Point& from,
           const std::vector<std::pair<gfx::Point, bool>>& via) {
  std::vector<DragStep> steps;
  steps.push_back({0, from, base::Milliseconds(150), ""});
  gfx::Point at = from;
  int hover = 0;
  for (const auto& [to, hold] : via) {
    constexpr int kSteps = 14;
    for (int i = 1; i <= kSteps; ++i) {
      steps.push_back({1,
                       gfx::Point(at.x() + (to.x() - at.x()) * i / kSteps,
                                  at.y() + (to.y() - at.y()) * i / kSteps),
                       base::Milliseconds(25), ""});
    }
    if (hold) {
      steps.push_back(
          {1, to, base::Seconds(2), "hover " + base::NumberToString(++hover)});
    }
    at = to;
  }
  steps.push_back({2, at, base::Seconds(1), "released"});
  test_drag::SetOverride(true);
  RunDragSteps(std::move(steps), 0, base::BindOnce(&FinishDrag));
}

// Moves the pointer (no button) through `points`, holding still where
// marked, then runs `done`.
void Hover(gfx::Point from,
           const std::vector<std::pair<gfx::Point, bool>>& points,
           base::OnceClosure done) {
  std::vector<DragStep> steps;
  gfx::Point at = from;
  steps.push_back({3, at, base::Milliseconds(100), ""});
  int hover = 0;
  for (const auto& [to, hold] : points) {
    constexpr int kSteps = 10;
    for (int i = 1; i <= kSteps; ++i) {
      steps.push_back({3,
                       gfx::Point(at.x() + (to.x() - at.x()) * i / kSteps,
                                  at.y() + (to.y() - at.y()) * i / kSteps),
                       base::Milliseconds(25), ""});
    }
    if (hold) {
      steps.push_back(
          {3, to, base::Seconds(2), "hover " + base::NumberToString(++hover)});
    }
    at = to;
  }
  test_drag::SetOverride(true);
  RunDragSteps(std::move(steps), 0, std::move(done));
}

// A click on `at` (press and release), then FinishDrag.
void ClickAt(gfx::Point at) {
  std::vector<DragStep> steps = {
      {3, at, base::Milliseconds(100), ""},
      {0, at, base::Milliseconds(80), ""},
      {2, at, base::Seconds(2), "clicked"},
  };
  test_drag::SetOverride(true);
  RunDragSteps(std::move(steps), 0, base::BindOnce(&FinishDrag));
}

void RunDragScenario(base::WeakPtr<views::Widget> widget,
                     std::string scenario,
                     int phase) {
  if (!widget) {
    return;
  }
  BrowserView* first =
      BrowserView::GetBrowserViewForNativeWindow(widget->GetNativeWindow());
  if (!first) {
    return;
  }
  const gfx::Rect window = first->GetBoundsInScreen();
  const gfx::Rect page = first->contents_container()
                             ? first->contents_container()->GetBoundsInScreen()
                             : window;

  if (scenario == "same" || scenario == "gap") {
    std::vector<std::pair<gfx::Point, bool>> via;
    if (scenario == "gap") {
      // Just left of the page: the gap between sidebar and page.
      via.push_back({gfx::Point(page.x() - 3, page.CenterPoint().y()), true});
    }
    via.push_back({gfx::Point(page.right() - page.width() / 6,
                              page.CenterPoint().y()),
                   true});
    Glide(TabCenter(first, 1), via);
    return;
  }

  if (scenario == "header" || scenario == "separate" || scenario == "close" ||
      scenario == "unsplit" || scenario == "panedrag" ||
      scenario == "paneout") {
    auto* mcv = BraveMultiContentsView::From(first->multi_contents_view());
    if (phase == 0) {
      // Split the two tabs, then come back.
      TabStripModel* model = first->browser()->GetTabStripModel();
      // The last two tabs (a space may start with its pinned Home tab).
      const int n = model->count();
      if (n >= 2) {
        mcv->split_controller()->MergeTabs(
            model->GetTabAtIndex(n - 1)->GetHandle(),
            model->GetTabAtIndex(n - 2)->GetHandle(), SplitEdge::kRight);
      }
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(&RunDragScenario, widget, scenario, 1),
          base::Seconds(3));
      return;
    }
    std::vector<gfx::Rect> panes = mcv->GetPaneTargetBounds();
    if (panes.size() < 2) {
      LOG(ERROR) << "WRYLING_DRAG: no split";
      FinishDrag();
      return;
    }
    gfx::Rect pane = panes[1];
    gfx::Point origin = pane.origin();
    views::View::ConvertPointToScreen(mcv, &origin);
    pane.set_origin(origin);
    const gfx::Point middle = pane.CenterPoint();
    // The window border just above the pane (the header's trigger), and just
    // inside the page (which must not trigger it).
    const gfx::Point top(pane.CenterPoint().x(), pane.y() - 4);
    const gfx::Point inside(pane.CenterPoint().x(), pane.y() + 6);
    if (scenario == "unsplit") {
      // Drag pane 2's tab (its sidebar segment) below the row: it leaves the
      // split.
      const gfx::Point from = TabCenter(first, 1);
      Glide(from, {{gfx::Point(from.x(), from.y() + 70), true}});
      return;
    }
    if (scenario == "panedrag" || scenario == "paneout") {
      // Hover pane 2's top edge, then drag its header: onto pane 1's bottom
      // edge (panedrag), or onto the sidebar (paneout).
      gfx::Rect left = panes[0];
      gfx::Point lo = left.origin();
      views::View::ConvertPointToScreen(mcv, &lo);
      left.set_origin(lo);
      const gfx::Point to =
          scenario == "panedrag"
              ? gfx::Point(left.CenterPoint().x(), left.bottom() - 30)
              : gfx::Point(first->GetBoundsInScreen().x() + 120,
                           first->GetBoundsInScreen().CenterPoint().y());
      Hover(middle, {{top, true}},
            base::BindOnce(
                [](base::WeakPtr<views::Widget> widget, gfx::Point to) {
                  BrowserView* view =
                      widget ? BrowserView::GetBrowserViewForNativeWindow(
                                   widget->GetNativeWindow())
                             : nullptr;
                  auto button = view ? BraveMultiContentsView::From(
                                           view->multi_contents_view())
                                           ->GetPaneHeaderButtonForTesting(
                                               1, /*close=*/false)
                                     : std::nullopt;
                  if (!button) {
                    LOG(ERROR) << "WRYLING_DRAG: header not showing";
                    FinishDrag();
                    return;
                  }
                  // Grab the header to the left of its buttons.
                  Glide(gfx::Point(button->x() - 80, button->CenterPoint().y()),
                        {{to, true}});
                },
                widget, to));
      return;
    }
    if (scenario == "header") {
      // Just inside the page (no header), the border above (header shows,
      // the pane slides down), then into the page (it slides away).
      Hover(middle, {{inside, true}, {top, true}, {middle, true}},
          base::BindOnce(&FinishDrag));
      return;
    }
    // Top edge, then click the header's separate or close button.
    Hover(middle, {{top, true}},
          base::BindOnce(
              [](base::WeakPtr<views::Widget> widget, bool close) {
                BrowserView* view = widget
                                        ? BrowserView::GetBrowserViewForNativeWindow(
                                              widget->GetNativeWindow())
                                        : nullptr;
                auto button =
                    view ? BraveMultiContentsView::From(
                               view->multi_contents_view())
                               ->GetPaneHeaderButtonForTesting(1, close)
                         : std::nullopt;
                if (!button) {
                  LOG(ERROR) << "WRYLING_DRAG: header not showing";
                  FinishDrag();
                  return;
                }
                ClickAt(button->CenterPoint());
              },
              widget, scenario == "close"));
    return;
  }

  if (scenario == "reorder") {
    // The 2nd tab dragged above the 1st: they swap, no split.
    TabStrip* strip = FindTabStrip(first->GetWidget()->GetRootView());
    const gfx::Rect top = strip && strip->GetTabCount()
                              ? strip->tab_at(0)->GetBoundsInScreen()
                              : gfx::Rect();
    Glide(TabCenter(first, 1),
          {{gfx::Point(top.CenterPoint().x(), top.y() + 2), true}});
    return;
  }

  if (scenario == "merge" || scenario == "mergeleft") {
    // The 2nd tab dropped onto the middle of the 1st tab's row (mergeleft:
    // its left third, so it joins on the left).
    TabStrip* strip = FindTabStrip(first->GetWidget()->GetRootView());
    gfx::Point to = TabCenter(first, 0);
    if (scenario == "mergeleft" && strip && strip->GetTabCount()) {
      const gfx::Rect r = strip->tab_at(0)->GetBoundsInScreen();
      to = gfx::Point(r.x() + r.width() / 6, r.CenterPoint().y());
    }
    Glide(TabCenter(first, 1), {{to, true}});
    return;
  }

  if (scenario != "cross" && scenario != "back") {
    LOG(ERROR) << "WRYLING_DRAG: unknown scenario " << scenario;
    FinishDrag();
    return;
  }
  if (phase == 0) {
    // A second window over the lower right of the first.
    BrowserWindowInterface* second = CreateBrowserWindow(
        BrowserWindowCreateParams(first->GetProfile(), true));
    chrome::AddTabAt(second, GURL("https://www.apple.com/"), -1, true);
    second->GetWindow()->SetBounds(gfx::Rect(
        window.x() + window.width() / 2, window.y() + window.height() / 2,
        window.width() / 2, window.height() / 2));
    second->GetWindow()->Show();
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RunDragScenario, widget, scenario, 1),
        base::Seconds(3));
    return;
  }
  BrowserView* second = nullptr;
  for (BrowserView* view : AllBrowserViews()) {
    if (view != first) {
      second = view;
    }
  }
  if (!second) {
    LOG(ERROR) << "WRYLING_DRAG: no second window";
    FinishDrag();
    return;
  }
  const gfx::Point from = TabCenter(second, 0);
  if (scenario == "cross") {
    // Onto the left of the first window's page, above the second window.
    Glide(from, {{gfx::Point(page.x() + page.width() / 6,
                             page.y() + page.height() / 3),
                  true}});
  } else {
    // Back into the first window's sidebar, below its tabs.
    Glide(from, {{gfx::Point(window.x() + 120, window.y() + window.height() / 2),
                  true}});
  }
}

}  // namespace

void RunTestDragIfSet(BrowserView* browser_view) {
  const char* scenario = std::getenv("WRYLING_TEST_DRAG");
  if (!scenario || !*scenario) {
    return;
  }
  static bool started = false;
  if (started) {
    return;
  }
  started = true;
  // Once the window is up: a second tab to drag (same/gap), then the replay.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<views::Widget> widget, std::string scenario) {
            if (!widget) {
              return;
            }
            BrowserView* view = BrowserView::GetBrowserViewForNativeWindow(
                widget->GetNativeWindow());
            if (!view) {
              return;
            }
            if (scenario == "same" || scenario == "gap" ||
                scenario == "merge" || scenario == "reorder" ||
                scenario == "header" || scenario == "separate" ||
                scenario == "close" || scenario == "unsplit" ||
                scenario == "panedrag" || scenario == "paneout" ||
                scenario == "mergeleft") {
              chrome::AddTabAt(view->browser(),
                               GURL("https://news.ycombinator.com/"), -1,
                               false);
            }
            base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(&RunDragScenario, widget, scenario, 0),
                base::Seconds(3));
          },
          browser_view->GetWidget()->GetWeakPtr(), std::string(scenario)),
      base::Seconds(3));
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace wryling
