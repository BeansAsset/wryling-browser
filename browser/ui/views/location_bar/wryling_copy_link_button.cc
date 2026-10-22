/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/location_bar/wryling_copy_link_button.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "url/gurl.h"

namespace {

// Same footprint as the Shields button next to it.
constexpr int kButtonWidth = 30;
// How long the checkmark stays before the link icon comes back.
constexpr base::TimeDelta kCopiedDuration = base::Milliseconds(1600);

gfx::Transform ScaleAboutCentre(const views::View* view, float scale) {
  return gfx::TransformAboutPivot(
      gfx::RectF(view->GetLocalBounds()).CenterPoint(),
      gfx::Transform::MakeScale(scale));
}

}  // namespace

WrylingCopyLinkButton::WrylingCopyLinkButton(BrowserWindowInterface* browser)
    : views::ImageButton(
          base::BindRepeating(&WrylingCopyLinkButton::OnPressed,
                              base::Unretained(this))),
      browser_(browser) {
  // TODO: localise "Copy link".
  SetTooltipText(u"Copy link");
  GetViewAccessibility().SetName(u"Copy link");
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  SetPreferredSize(gfx::Size(
      kButtonWidth,
       GetLayoutConstant(LayoutConstant::kLocationBarHeight) -
           2 * GetLayoutConstant(LayoutConstant::kLocationBarElementPadding)));
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(),
      GetLayoutConstant(LayoutConstant::kLocationBarChildCornerRadius));
  // Own layer so the confirmation can scale the icon.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  ShowCopied(false);
}

WrylingCopyLinkButton::~WrylingCopyLinkButton() = default;

// static
bool WrylingCopyLinkButton::CanCopy(content::WebContents* contents) {
  if (!contents) {
    return false;
  }
  const GURL& url = contents->GetVisibleURL();
  return url.is_valid() && (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile());
}

void WrylingCopyLinkButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  SetHasInkDropActionOnClick(true);
  ConfigureInkDropForRefresh2023(this, kColorOmniboxIconHover,
                                 kColorOmniboxIconPressed);
}

void WrylingCopyLinkButton::OnPressed() {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!CanCopy(contents)) {
    return;
  }
  // Copies the address and shows Chromium's "Link copied" toast.
  chrome::CopyURL(browser_, contents);

  ShowCopied(true);
  Pop();
  revert_timer_.Start(FROM_HERE, kCopiedDuration,
                      base::BindOnce(&WrylingCopyLinkButton::ShowCopied,
                                     weak_factory_.GetWeakPtr(), false));
}

void WrylingCopyLinkButton::ShowCopied(bool copied) {
  showing_copied_ = copied;
  const int size = GetLayoutConstant(LayoutConstant::kLocationBarIconSize);
  SetImageModel(
      views::Button::STATE_NORMAL,
      copied ? ui::ImageModel::FromVectorIcon(kCheckIcon,
                                              ui::kColorLinkForeground, size)
             : ui::ImageModel::FromVectorIcon(
                   vector_icons::kLinkIcon,
                   kColorOmniboxSecurityChipDefault, size));
  SetTooltipText(copied ? u"Copied" : u"Copy link");
}

void WrylingCopyLinkButton::Pop() {
  // Squash, overshoot, settle: a small spring so the check feels pressed in.
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetTransform(this, ScaleAboutCentre(this, 0.6f))
      .Then()
      .SetDuration(base::Milliseconds(150))
      .SetTransform(this, ScaleAboutCentre(this, 1.15f),
                    gfx::Tween::EASE_OUT)
      .Then()
      .SetDuration(base::Milliseconds(130))
      .SetTransform(this, gfx::Transform(), gfx::Tween::EASE_IN_OUT);
}

BEGIN_METADATA(WrylingCopyLinkButton)
END_METADATA
