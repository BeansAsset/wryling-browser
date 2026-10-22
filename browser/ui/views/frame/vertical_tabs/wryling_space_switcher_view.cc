/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/frame/vertical_tabs/wryling_space_switcher_view.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "brave/browser/ui/color/wryling_tokens.h"
#include "ui/color/color_id.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "brave/browser/ui/tabs/brave_tab_menu_model.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/focus/focus_manager.h"
#include "url/gurl.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

// A bubble base that Wryling's own popovers can construct (Chromium keeps the
// BubbleDialogDelegateView constructors for a friend list, which names this
// class).
class BraveBubbleDialogDelegateView : public BubbleDialogDelegateView {
  METADATA_HEADER(BraveBubbleDialogDelegateView, BubbleDialogDelegateView)

 public:
  BraveBubbleDialogDelegateView(View* anchor, BubbleBorder::Arrow arrow)
      : BubbleDialogDelegateView(anchor, arrow) {}
};

BEGIN_METADATA(BraveBubbleDialogDelegateView)
END_METADATA

}  // namespace views

namespace {

constexpr int kButtonSize = 28;
constexpr int kButtonSpacing = 4;
constexpr int kSymbolSize = 13;
constexpr int kDotSize = 8;
constexpr char kAddSymbol[] = "plus";

// Label colours inside popovers (text, secondary, tertiary, destructive).
SkColor LabelColor(const views::View* view, ui::ColorId id, SkAlpha alpha = 0xFF) {
  const ui::ColorProvider* colors = view->GetColorProvider();
  return colors ? SkColorSetA(colors->GetColor(id), alpha) : SK_ColorGRAY;
}

// A round button that paints its own hover/selected pill, like the sidebar's
// tab rows (an inset rounded highlight, radius-m).
class PillButton : public views::Button {
  METADATA_HEADER(PillButton, views::Button)

 public:
  explicit PillButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetPreferredSize(gfx::Size(kButtonSize, kButtonSize));
    // The pill is the focus indicator; no Chromium focus ring box.
    SetInstallFocusRingOnFocus(false);
    SetAnimateOnStateChange(false);
  }

  void set_selected(bool selected) {
    if (selected_ != selected) {
      selected_ = selected;
      SchedulePaint();
    }
  }
  bool selected() const { return selected_; }

 protected:
  // Background colour for the current state; transparent at rest.
  SkColor GetPillColor() const {
    const ui::ColorProvider* colors = GetColorProvider();
    if (!colors) {
      return SK_ColorTRANSPARENT;
    }
    if (selected_) {
      return colors->GetColor(kColorTabBackgroundActiveFrameActive);
    }
    if (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED ||
        HasFocus()) {
      return colors->GetColor(kColorToolbarInkDropHover);
    }
    return SK_ColorTRANSPARENT;
  }

  void PaintPill(gfx::Canvas* canvas) {
    const SkColor color = GetPillColor();
    if (SkColorGetA(color) == 0) {
      return;
    }
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color);
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), wryling::kRadiusM,
                          flags);
  }

 private:
  bool selected_ = false;
};

BEGIN_METADATA(PillButton)
END_METADATA

// A choice in the editor: an icon or a colour swatch.
class ChoiceButton : public PillButton {
  METADATA_HEADER(ChoiceButton, PillButton)

 public:
  ChoiceButton(PressedCallback callback,
               std::string symbol,
               std::optional<SkColor> swatch,
               std::u16string name)
      : PillButton(std::move(callback)),
        symbol_(std::move(symbol)),
        swatch_(swatch) {
    SetPreferredSize(gfx::Size(26, 26));
    SetTooltipText(name);
    GetViewAccessibility().SetName(name);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::Rect bounds = GetLocalBounds();
    if (!symbol_.empty()) {
      PaintPill(canvas);
      const gfx::ImageSkia image = GetWrylingSymbolImage(
          symbol_, kSymbolSize,
          LabelColor(this, selected() ? ui::kColorLabelForeground
                                      : ui::kColorLabelForegroundSecondary));
      if (!image.isNull()) {
        canvas->DrawImageInt(image,
                             bounds.CenterPoint().x() - image.width() / 2,
                             bounds.CenterPoint().y() - image.height() / 2);
      }
      return;
    }
    // Colour swatch: a filled circle, with a ring when selected or hovered.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const gfx::PointF center(bounds.CenterPoint());
    if (selected() || GetState() == STATE_HOVERED) {
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(2);
      flags.setColor(selected()
                         ? LabelColor(this, ui::kColorLabelForeground)
                         : LabelColor(this, ui::kColorLabelForeground, 0x66));
      canvas->DrawCircle(center, 11, flags);
    }
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(swatch_.value_or(SK_ColorGRAY));
    canvas->DrawCircle(center, 8, flags);
    if (!swatch_) {
      // "Theme colour": a half-filled circle reads as "automatic".
      flags.setColor(SkColorSetA(SK_ColorWHITE, 0x99));
      canvas->DrawCircle(center, 4, flags);
    }
  }

 private:
  std::string symbol_;
  std::optional<SkColor> swatch_;
};

BEGIN_METADATA(ChoiceButton)
END_METADATA

// The name / icon / colour popover.
class SpaceEditorView : public views::BraveBubbleDialogDelegateView,
                        public views::TextfieldController {
  METADATA_HEADER(SpaceEditorView, views::BraveBubbleDialogDelegateView)

 public:
  SpaceEditorView(views::View* anchor,
                  wryling::SpacesController* controller,
                  const std::string& space_id,
                  bool focus_name)
      : views::BraveBubbleDialogDelegateView(
            anchor, views::BubbleBorder::BOTTOM_CENTER),
        controller_(controller),
        space_id_(space_id) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    set_margins(gfx::Insets(16));
    SetShowCloseButton(false);
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 12));

    const wryling::Space* space = controller_->GetSpace(space_id_);
    name_ = AddChildView(std::make_unique<views::Textfield>());
    name_->SetText(space ? base::UTF8ToUTF16(space->name) : u"");
    name_->SetPlaceholderText(u"Space name");
    name_->GetViewAccessibility().SetName(u"Space name");
    name_->SetController(this);
    name_->SetPreferredSize(gfx::Size(0, wryling::kControlHeight));
    if (focus_name) {
      name_->SelectAll(false);
    }

    // Icons: two rows of ten.
    const auto& icons = wryling::GetSpaceIconChoices();
    views::View* row = nullptr;
    for (size_t i = 0; i < icons.size(); ++i) {
      if (i % 10 == 0) {
        row = AddChildView(std::make_unique<views::View>());
        row->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 4));
      }
      auto* button = row->AddChildView(std::make_unique<ChoiceButton>(
          base::BindRepeating(&SpaceEditorView::SetIcon,
                              base::Unretained(this), icons[i]),
          icons[i], std::nullopt, base::UTF8ToUTF16(icons[i])));
      icon_buttons_.emplace_back(icons[i], button);
    }

    // Colours: the theme colour, then the swatches.
    auto* colors = AddChildView(std::make_unique<views::View>());
    colors->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 4));
    color_buttons_.emplace_back(
        std::nullopt,
        colors->AddChildView(std::make_unique<ChoiceButton>(
            base::BindRepeating(&SpaceEditorView::SetColor,
                                base::Unretained(this), std::nullopt),
            std::string(), std::nullopt, u"Theme colour")));
    for (SkColor color : wryling::GetSpaceColorChoices()) {
      color_buttons_.emplace_back(
          color, colors->AddChildView(std::make_unique<ChoiceButton>(
                     base::BindRepeating(&SpaceEditorView::SetColor,
                                         base::Unretained(this),
                                         std::optional<SkColor>(color)),
                     std::string(), color, u"Colour")));
    }
    UpdateSelection();
  }

  views::View* GetInitiallyFocusedView() override { return name_; }

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    Update([&new_contents](wryling::Space& space) {
      space.name = base::UTF16ToUTF8(
          base::TrimWhitespace(new_contents, base::TRIM_ALL));
    });
  }
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() == ui::EventType::kKeyPressed &&
        key_event.key_code() == ui::VKEY_RETURN) {
      GetWidget()->Close();
      return true;
    }
    return false;
  }

 private:
  void Update(base::FunctionRef<void(wryling::Space&)> change) {
    const wryling::Space* space = controller_->GetSpace(space_id_);
    if (!space) {
      return;
    }
    wryling::Space updated = *space;
    change(updated);
    controller_->UpdateSpace(updated);
    UpdateSelection();
  }

  void SetIcon(const std::string& icon) {
    Update([&icon](wryling::Space& space) { space.icon = icon; });
  }

  void SetColor(std::optional<SkColor> color) {
    Update([color](wryling::Space& space) { space.color = color; });
  }

  void UpdateSelection() {
    const wryling::Space* space = controller_->GetSpace(space_id_);
    if (!space) {
      return;
    }
    for (auto& [icon, button] : icon_buttons_) {
      button->set_selected(icon == space->icon);
    }
    for (auto& [color, button] : color_buttons_) {
      button->set_selected(color == space->color);
    }
  }

  raw_ptr<wryling::SpacesController> controller_;
  std::string space_id_;
  raw_ptr<views::Textfield> name_ = nullptr;
  std::vector<std::pair<std::string, raw_ptr<ChoiceButton>>> icon_buttons_;
  std::vector<std::pair<std::optional<SkColor>, raw_ptr<ChoiceButton>>>
      color_buttons_;
};

BEGIN_METADATA(SpaceEditorView)
END_METADATA

// "Delete “Work” and close its 6 tabs?"
class DeleteSpaceView : public views::BraveBubbleDialogDelegateView {
  METADATA_HEADER(DeleteSpaceView, views::BraveBubbleDialogDelegateView)

 public:
  DeleteSpaceView(views::View* anchor,
                  wryling::SpacesController* controller,
                  const std::string& space_id)
      : views::BraveBubbleDialogDelegateView(
            anchor, views::BubbleBorder::BOTTOM_CENTER) {
    const wryling::Space* space = controller->GetSpace(space_id);
    const int tabs = controller->GetTabCount(space_id);
    const std::u16string name =
        space ? base::UTF8ToUTF16(space->name) : std::u16string();
    SetTitle(u"Delete “" + name + u"”?");
    SetButtonLabel(ui::mojom::DialogButton::kOk, u"Delete");
    SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kCancel));
    set_margins(gfx::Insets(16));
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    auto* label = AddChildView(std::make_unique<views::Label>(
        tabs == 0 ? std::u16string(u"This space has no tabs.")
        : tabs == 1
            ? std::u16string(u"Its tab will be closed.")
            : u"Its " + base::NumberToString16(tabs) +
                  u" tabs will be closed."));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetAcceptCallback(base::BindOnce(
        [](base::WeakPtr<wryling::SpacesController> controller,
           std::string id) {
          if (controller) {
            controller->DeleteSpace(id);
          }
        },
        controller->GetWeakPtr(), space_id));
  }

  void OnThemeChanged() override {
    views::BraveBubbleDialogDelegateView::OnThemeChanged();
    StyleDeleteButton();
  }

  void AddedToWidget() override {
    views::BraveBubbleDialogDelegateView::AddedToWidget();
    StyleDeleteButton();
  }

 private:
  // Destructive: a secondary button with red text.
  void StyleDeleteButton() {
    views::MdTextButton* ok = GetOkButton();
    const ui::ColorProvider* colors = GetColorProvider();
    if (!ok || !colors) {
      return;
    }
    ok->SetStyle(ui::ButtonStyle::kDefault);
    ok->SetEnabledTextColors(colors->GetColor(ui::kColorAlertHighSeverity));
  }
};

BEGIN_METADATA(DeleteSpaceView)
END_METADATA

}  // namespace

// One space (or "+") in the row.
class WrylingSpaceSwitcherView::SpaceButton
    : public PillButton,
      public views::ContextMenuController,
      public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(SpaceButton, PillButton)

 public:
  enum Command { kRename = 1, kStyle, kDelete };

  SpaceButton(WrylingSpaceSwitcherView* switcher, std::string space_id)
      : PillButton(base::BindRepeating(&SpaceButton::OnPressed,
                                       base::Unretained(this))),
        switcher_(switcher),
        space_id_(std::move(space_id)) {
    if (!is_add()) {
      set_context_menu_controller(this);
    }
    UpdateTooltip();
  }

  const std::string& space_id() const { return space_id_; }
  bool is_add() const { return space_id_.empty(); }

  void UpdateTooltip() {
    if (is_add()) {
      SetTooltipText(u"New space");
      GetViewAccessibility().SetName(u"New space");
      return;
    }
    wryling::SpacesController* controller = switcher_->controller();
    const wryling::Space* space = controller->GetSpace(space_id_);
    const int index = controller->GetSpaceIndex(space_id_);
    std::u16string text = space ? base::UTF8ToUTF16(space->name) : u"";
    if (index >= 0 && index < 9) {
      text += u"  ⌃" + base::NumberToString16(index + 1);
    }
    SetTooltipText(text);
    GetViewAccessibility().SetName(space ? base::UTF8ToUTF16(space->name)
                                         : std::u16string());
    SchedulePaint();
  }

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    PaintPill(canvas);
    const ui::ColorProvider* colors = GetColorProvider();
    if (!colors) {
      return;
    }
    const gfx::Point center = GetLocalBounds().CenterPoint();
    const SkColor fg =
        selected()
            ? colors->GetColor(kColorTabForegroundActiveFrameActive)
            : SkColorSetA(
                  colors->GetColor(kColorTabForegroundInactiveFrameActive),
                  GetState() == STATE_HOVERED ? 0xE6 : 0x99);
    std::string symbol = kAddSymbol;
    const wryling::Space* space = nullptr;
    if (!is_add()) {
      space = switcher_->controller()->GetSpace(space_id_);
      symbol = space ? space->icon : std::string();
    }
    if (!symbol.empty()) {
      const gfx::ImageSkia image =
          GetWrylingSymbolImage(symbol, kSymbolSize, fg);
      if (!image.isNull()) {
        canvas->DrawImageInt(image, center.x() - image.width() / 2,
                             center.y() - image.height() / 2);
        return;
      }
    }
    // No icon: a dot in the space's colour.
    if (space) {
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(switcher_->controller()->GetDisplayColor(*space));
      canvas->DrawCircle(gfx::PointF(center), kDotSize / 2.0f, flags);
    }
  }

  void OnThemeChanged() override {
    PillButton::OnThemeChanged();
    SchedulePaint();
  }

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override {
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    menu_model_->AddItem(kRename, u"Rename…");
    menu_model_->AddItem(kStyle, u"Icon and colour…");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kDelete, u"Delete space…");
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
    menu_runner_->RunMenuAt(GetWidget(), nullptr,
                            gfx::Rect(point, gfx::Size()),
                            views::MenuAnchorPosition::kTopLeft, source_type);
  }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override {
    return command_id != kDelete ||
           switcher_->controller()->spaces().size() > 1;
  }
  void ExecuteCommand(int command_id, int event_flags) override {
    // After the menu has closed, so the popover can take focus.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<WrylingSpaceSwitcherView> switcher,
               std::string id, int command) {
              if (!switcher) {
                return;
              }
              if (command == kDelete) {
                switcher->ConfirmDelete(id);
              } else {
                switcher->ShowEditor(id, command == kRename);
              }
            },
            switcher_->weak_factory_.GetWeakPtr(), space_id_, command_id));
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton() && event.GetClickCount() == 2 &&
        !is_add()) {
      switcher_->ShowEditor(space_id_, /*focus_name=*/true);
      return true;
    }
    return PillButton::OnMousePressed(event);
  }

 private:
  void OnPressed(const ui::Event& event) {
    if (is_add()) {
      switcher_->AddSpace();
    } else {
      switcher_->controller()->SwitchToSpace(space_id_);
    }
  }

  raw_ptr<WrylingSpaceSwitcherView> switcher_;
  std::string space_id_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

BEGIN_METADATA(WrylingSpaceSwitcherView, SpaceButton)
END_METADATA

class WrylingSpaceSwitcherView::SwipeHandler : public ui::EventHandler {
 public:
  explicit SwipeHandler(WrylingSpaceSwitcherView* switcher)
      : switcher_(switcher) {}

  // ui::EventHandler:
  void OnScrollEvent(ui::ScrollEvent* event) override {
    switcher_->HandleSidebarScroll(event);
  }

 private:
  raw_ptr<WrylingSpaceSwitcherView> switcher_;
};

WrylingSpaceSwitcherView::WrylingSpaceSwitcherView(
    wryling::SpacesController* controller,
    views::View* swipe_area,
    views::View* slide_target)
    : controller_(controller),
      swipe_area_(swipe_area),
      slide_target_(slide_target) {
  observation_.Observe(controller_);
  RebuildButtons();
  for (int i = 0; i < 9; ++i) {
    AddAccelerator(ui::Accelerator(
        static_cast<ui::KeyboardCode>(ui::VKEY_1 + i), ui::EF_CONTROL_DOWN));
  }
  if (swipe_area_) {
    swipe_handler_ = std::make_unique<SwipeHandler>(this);
    swipe_area_->AddPreTargetHandler(swipe_handler_.get());
  }
}

WrylingSpaceSwitcherView::~WrylingSpaceSwitcherView() {
  if (swipe_area_ && swipe_handler_) {
    swipe_area_->RemovePreTargetHandler(swipe_handler_.get());
  }
}

void WrylingSpaceSwitcherView::RebuildButtons() {
  // Keep existing buttons when the list of spaces is the same, so an open
  // editor stays anchored while it changes names, icons and colours.
  std::vector<std::string> ids;
  for (const wryling::Space& space : controller_->spaces()) {
    ids.push_back(space.id);
  }
  std::vector<std::string> existing;
  for (views::View* child : children()) {
    auto* button = static_cast<SpaceButton*>(child);
    if (!button->is_add()) {
      existing.push_back(button->space_id());
    }
  }
  if (ids != existing || !add_button_) {
    add_button_ = nullptr;
    RemoveAllChildViews();
    for (const std::string& id : ids) {
      AddChildView(std::make_unique<SpaceButton>(this, id));
    }
    add_button_ = AddChildView(std::make_unique<SpaceButton>(this, ""));
  } else {
    for (views::View* child : children()) {
      static_cast<SpaceButton*>(child)->UpdateTooltip();
    }
  }
  UpdateButtonStates();
  PreferredSizeChanged();
  InvalidateLayout();
}

void WrylingSpaceSwitcherView::UpdateButtonStates() {
  for (views::View* child : children()) {
    auto* button = static_cast<SpaceButton*>(child);
    button->set_selected(!button->is_add() &&
                         button->space_id() == controller_->current_space_id());
  }
}

WrylingSpaceSwitcherView::SpaceButton* WrylingSpaceSwitcherView::GetButtonFor(
    const std::string& space_id) {
  for (views::View* child : children()) {
    auto* button = static_cast<SpaceButton*>(child);
    if (!button->is_add() && button->space_id() == space_id) {
      return button;
    }
  }
  return nullptr;
}

gfx::Size WrylingSpaceSwitcherView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int count = static_cast<int>(children().size());
  return gfx::Size(count * kButtonSize + std::max(0, count - 1) * kButtonSpacing,
                   kHeight);
}

void WrylingSpaceSwitcherView::Layout(PassKey) {
  // Spaces centred; "+" at the trailing edge. When there are too
  // many to centre, they start at the leading edge.
  const int count = static_cast<int>(children().size()) - 1;
  const int spaces_width =
      count * kButtonSize + std::max(0, count - 1) * kButtonSpacing;
  // Sitting at the panel's bottom edge, so measured from the bottom.
  const int y = height() - kButtonSize - kEdgeInset;
  const int add_x = width() - kButtonSize - kEdgeInset;
  int x = (width() - spaces_width) / 2;
  if (x + spaces_width > add_x - kButtonSpacing) {
    x = kEdgeInset;
  }
  for (views::View* child : children()) {
    if (child == add_button_) {
      child->SetBounds(add_x, y, kButtonSize, kButtonSize);
    } else {
      child->SetBounds(x, y, kButtonSize, kButtonSize);
      x += kButtonSize + kButtonSpacing;
    }
  }
}

void WrylingSpaceSwitcherView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

void WrylingSpaceSwitcherView::AddedToWidget() {
  // The window was restored on a space with its own colour.
  controller_->ApplyTint();

  static bool test_started = false;
  const char* test = std::getenv("WRYLING_TEST_SPACES");
  if (test && !test_started) {
    test_started = true;
    std::vector<std::string> steps = base::SplitString(
        test, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WrylingSpaceSwitcherView::RunTestStep,
                       weak_factory_.GetWeakPtr(), std::move(steps), 0),
        base::Seconds(4));
  }
}

void WrylingSpaceSwitcherView::LogTestState(const std::string& step) {
  std::string state;
  for (const wryling::Space& space : controller_->spaces()) {
    state += base::StringPrintf(
        " [%s%s \"%s\" icon=%s color=%s tabs=%d]",
        space.id == controller_->current_space_id() ? "*" : "",
        space.id.substr(0, 8).c_str(), space.name.c_str(), space.icon.c_str(),
        space.color ? base::StringPrintf("%08X", *space.color).c_str() : "theme",
        controller_->GetTabCount(space.id));
  }
  TabStripModel* model = controller_->model();
  int visible = 0;
  for (tabs::TabInterface* tab : *model) {
    visible += controller_->IsTabHidden(tab) ? 0 : 1;
  }
  fprintf(stderr, "WRYLING_SPACES: %s |%s | tabs=%d visible=%d active=%d\n",
          step.c_str(), state.c_str(), model->count(), visible,
          model->active_index());
}

void WrylingSpaceSwitcherView::RunTestStep(std::vector<std::string> steps,
                                           size_t index) {
  if (index >= steps.size()) {
    fprintf(stderr, "WRYLING_SPACES: done\n");
    return;
  }
  const std::string& step = steps[index];
  const std::vector<std::string> parts = base::SplitString(
      step, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  auto arg = [&parts](size_t i) {
    int value = 0;
    return i < parts.size() && base::StringToInt(parts[i], &value) ? value : 0;
  };
  auto space_id = [this](int i) {
    const auto& spaces = controller_->spaces();
    return i >= 0 && i < static_cast<int>(spaces.size()) ? spaces[i].id
                                                          : std::string();
  };
  const std::string& name = parts[0];
  if (name == "create") {
    controller_->CreateSpace();
  } else if (name == "add") {
    AddSpace();
  } else if (name == "edit") {
    ShowEditor(space_id(arg(1)), true);
  } else if (name == "delete") {
    ConfirmDelete(space_id(arg(1)));
  } else if (name == "deletenow") {
    controller_->DeleteSpace(space_id(arg(1)));
  } else if (name == "switch") {
    controller_->SwitchToSpaceAt(arg(1));
  } else if (name == "key") {
    if (views::FocusManager* focus = GetFocusManager()) {
      focus->ProcessAccelerator(ui::Accelerator(
          static_cast<ui::KeyboardCode>(ui::VKEY_1 + arg(1)),
          ui::EF_CONTROL_DOWN));
    }
  } else if (name == "swipe") {
    // A trackpad gesture as macOS delivers it: fingers down, moving, lifted,
    // then momentum (see HandleSidebarScroll).
    const float dx = arg(1);
    using Phase = ui::EventMomentumPhase;
    const Phase phases[] = {Phase::MAY_BEGIN, Phase::NONE, Phase::NONE,
                            Phase::NONE,      Phase::NONE, Phase::END,
                            Phase::INERTIAL_UPDATE, Phase::END};
    for (size_t i = 0; i < std::size(phases); ++i) {
      const bool moving = phases[i] == Phase::NONE ||
                          phases[i] == Phase::INERTIAL_UPDATE;
      const float step = moving ? dx / 4 : 0;
      ui::ScrollEvent event(ui::EventType::kScroll, gfx::PointF(10, 100),
                            gfx::PointF(10, 100), base::TimeTicks::Now(), 0,
                            step, arg(2), step, arg(2), 2, phases[i]);
      HandleSidebarScroll(&event);
    }
  } else if (name == "quit") {
    LogTestState(step);
    chrome::AttemptExit();
    return;
  } else if (name == "close") {
    TabStripModel* model = controller_->model();
    model->CloseWebContentsAt(model->active_index(),
                              TabCloseTypes::CLOSE_USER_GESTURE);
  } else if (name == "cmdw") {
    // ⌘W, through the real command.
    chrome::CloseTab(controller_->browser());
  } else if (name == "pin") {
    TabStripModel* model = controller_->model();
    model->SetTabPinned(model->active_index(), true);
  } else if (name == "group") {
    TabStripModel* model = controller_->model();
    model->AddToNewGroup({model->active_index()});
  } else if (name == "menu") {
    // The real tab context menu for the active tab: log its "Move to space"
    // entries, then choose entry N.
    TabStripModel* model = controller_->model();
    BraveTabMenuModel menu(nullptr,
                           TabMenuModelDelegate::From(controller_->browser()),
                           model, model->active_index());
    std::string items;
    for (size_t i = 0; i < menu.GetItemCount(); ++i) {
      if (menu.GetCommandIdAt(i) != wryling::kMoveToSpaceTabMenuCommand) {
        continue;
      }
      ui::MenuModel* sub = menu.GetSubmenuModelAt(i);
      items += base::UTF16ToUTF8(menu.GetLabelAt(i)) + " >";
      for (size_t j = 0; sub && j < sub->GetItemCount(); ++j) {
        items += " " + base::UTF16ToUTF8(sub->GetLabelAt(j));
      }
      if (sub && arg(1) < static_cast<int>(sub->GetItemCount())) {
        sub->ActivatedAt(arg(1));
      }
    }
    fprintf(stderr, "WRYLING_SPACES: menu items: %s\n",
            items.empty() ? "(no Move to space)" : items.c_str());
  } else if (name == "newtab") {
    chrome::AddTabAt(controller_->browser(), GURL("about:blank"), -1, true);
  } else if (name == "move") {
    TabStripModel* model = controller_->model();
    if (model->ContainsIndex(arg(1))) {
      controller_->MoveTabToSpace(model->GetTabAtIndex(arg(1)),
                                  space_id(arg(2)));
    }
  } else if (name == "rename" || name == "color" || name == "icon") {
    if (const wryling::Space* space = controller_->GetSpace(space_id(arg(1)))) {
      wryling::Space updated = *space;
      if (name == "rename" && parts.size() > 2) {
        updated.name = parts[2];
      } else if (name == "color") {
        const auto& colors = wryling::GetSpaceColorChoices();
        updated.color = arg(2) < 0 || arg(2) >= static_cast<int>(colors.size())
                            ? std::nullopt
                            : std::optional<SkColor>(colors[arg(2)]);
      } else if (name == "icon") {
        const auto& icons = wryling::GetSpaceIconChoices();
        updated.icon = arg(2) < 0 || arg(2) >= static_cast<int>(icons.size())
                           ? std::string()
                           : icons[arg(2)];
      }
      controller_->UpdateSpace(updated);
    }
  }
  LogTestState(step);
  // Captures once the slide and popovers have settled.
  if (const char* shots = std::getenv("WRYLING_TEST_SPACES_SHOTS")) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CaptureWrylingWindowsForTest,
                       base::StringPrintf("%s/%02d-%s", shots,
                                          static_cast<int>(index + 1),
                                          parts[0].c_str())),
        base::Milliseconds(900));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WrylingSpaceSwitcherView::RunTestStep,
                     weak_factory_.GetWeakPtr(), std::move(steps), index + 1),
      base::Milliseconds(2200));
}

bool WrylingSpaceSwitcherView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  const int index = accelerator.key_code() - ui::VKEY_1;
  if (index < 0 || index >= static_cast<int>(controller_->spaces().size())) {
    return false;
  }
  controller_->SwitchToSpaceAt(index);
  return true;
}

void WrylingSpaceSwitcherView::OnCurrentSpaceChanged(int direction) {
  UpdateButtonStates();
  if (GetWidget() && GetWidget()->IsVisible()) {
    SlideIn(direction);
  }
}

void WrylingSpaceSwitcherView::OnSpacesChanged() {
  RebuildButtons();
}

void WrylingSpaceSwitcherView::AddSpace() {
  const std::string id = controller_->CreateSpace();
  // After the row has its new button, open the editor on it.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<WrylingSpaceSwitcherView> switcher, std::string id) {
            if (switcher) {
              switcher->ShowEditor(id, /*focus_name=*/true);
            }
          },
          weak_factory_.GetWeakPtr(), id));
}

void WrylingSpaceSwitcherView::ShowEditor(const std::string& space_id,
                                          bool focus_name) {
  SpaceButton* anchor = GetButtonFor(space_id);
  if (!anchor || !GetWidget()) {
    return;
  }
  views::BubbleDialogDelegateView::CreateBubble(
      new SpaceEditorView(anchor, controller_, space_id, focus_name))
      ->Show();
}

void WrylingSpaceSwitcherView::ConfirmDelete(const std::string& space_id) {
  SpaceButton* anchor = GetButtonFor(space_id);
  if (!anchor || controller_->spaces().size() <= 1) {
    return;
  }
  views::BubbleDialogDelegateView::CreateBubble(
      new DeleteSpaceView(anchor, controller_, space_id))
      ->Show();
}

void WrylingSpaceSwitcherView::SlideIn(int direction) {
  if (!slide_target_ || direction == 0) {
    return;
  }
  if (!slide_target_->layer()) {
    slide_target_->SetPaintToLayer();
    slide_target_->layer()->SetFillsBoundsOpaquely(false);
  }
  ui::Layer* layer = slide_target_->layer();
  const bool reduce_motion = gfx::Animation::PrefersReducedMotion();
  gfx::Transform start;
  if (!reduce_motion) {
    // The new list comes in from the side it lives on.
    start.Translate(direction * 28, 0);
  }
  auto done = base::BindRepeating(
      [](base::WeakPtr<WrylingSpaceSwitcherView> switcher) {
        if (switcher && switcher->slide_target_ &&
            switcher->slide_target_->layer() &&
            !switcher->slide_target_->layer()->GetAnimator()->is_animating()) {
          switcher->slide_target_->DestroyLayer();
        }
      },
      weak_factory_.GetWeakPtr());
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(done)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(layer, 0.0f)
      .SetTransform(layer, start)
      .Then()
      .SetDuration(base::Milliseconds(220))
      .SetOpacity(layer, 1.0f, gfx::Tween::EASE_OUT)
      .SetTransform(layer, gfx::Transform(), gfx::Tween::EASE_OUT_3);
}

void WrylingSpaceSwitcherView::HandleSidebarScroll(ui::ScrollEvent* event) {
  if (event->type() != ui::EventType::kScroll ||
      controller_->spaces().size() < 2 || !GetVisible()) {
    return;
  }
  // On macOS a trackpad gesture arrives as: MAY_BEGIN (fingers down), NONE
  // (fingers moving), END (fingers lifted), then INERTIAL_UPDATE... END for
  // momentum. Events without any phase (a mouse wheel) also come as END.
  // The scroll phase itself is never set there.
  const ui::EventMomentumPhase phase = event->momentum_phase();
  const base::TimeTicks now = event->time_stamp();
  const bool stale = now - swipe_last_event_ > base::Milliseconds(300);
  swipe_last_event_ = now;

  if (phase == ui::EventMomentumPhase::MAY_BEGIN ||
      (stale && phase == ui::EventMomentumPhase::NONE)) {
    // A new gesture.
    swipe_x_ = swipe_y_ = 0;
    swipe_axis_ = SwipeAxis::kUndecided;
    swipe_done_ = false;
    swipe_in_gesture_ = true;
  }

  if (phase == ui::EventMomentumPhase::INERTIAL_UPDATE ||
      (phase == ui::EventMomentumPhase::END && !swipe_in_gesture_)) {
    // Momentum (or its end) after a sideways gesture belongs to it.
    if (swipe_axis_ == SwipeAxis::kSideways) {
      event->SetHandled();
    }
    return;
  }

  if (!swipe_in_gesture_) {
    return;
  }

  swipe_x_ += event->x_offset();
  swipe_y_ += event->y_offset();

  // Decide once per gesture whether it's a sideways swipe or a scroll of the
  // tab list, so scrolling never turns into a space switch.
  if (swipe_axis_ == SwipeAxis::kUndecided &&
      std::abs(swipe_x_) + std::abs(swipe_y_) >= 10) {
    swipe_axis_ = std::abs(swipe_x_) > 1.5f * std::abs(swipe_y_)
                      ? SwipeAxis::kSideways
                      : SwipeAxis::kVertical;
  }

  if (swipe_axis_ == SwipeAxis::kSideways) {
    // Don't also scroll the tab list.
    event->SetHandled();
    if (!swipe_done_ && std::abs(swipe_x_) > 50) {
      swipe_done_ = true;
      // Fingers moving left bring in the space on the right.
      controller_->SwitchBy(swipe_x_ < 0 ? 1 : -1);
    }
  }

  if (phase == ui::EventMomentumPhase::END) {
    // Fingers lifted; any momentum that follows is handled above.
    swipe_in_gesture_ = false;
  }

  // WRYLING_LOG_SWIPE=1: the real trackpad stream, for checking by hand.
  static const bool log_swipe = std::getenv("WRYLING_LOG_SWIPE") != nullptr;
  if (log_swipe) {
    fprintf(stderr,
            "WRYLING_SWIPE: phase=%d dx=%.1f dy=%.1f sum=(%.1f,%.1f) axis=%d "
            "done=%d\n",
            static_cast<int>(phase), event->x_offset(), event->y_offset(),
            swipe_x_, swipe_y_, static_cast<int>(swipe_axis_), swipe_done_);
  }
}

BEGIN_METADATA(WrylingSpaceSwitcherView)
END_METADATA
