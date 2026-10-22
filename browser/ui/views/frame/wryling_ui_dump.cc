/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/frame/wryling_ui_dump.h"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "brave/browser/ui/toolbar/brave_app_menu_model.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/glass_frame_service.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

namespace wryling {

namespace {

void Line(const std::string& text) {
  fprintf(stderr, "WRYLING_UI: %s\n", text.c_str());
}

void DumpMenu(ui::MenuModel* model, int depth) {
  if (!model || depth > 4) {
    return;
  }
  const std::string indent(depth * 2, ' ');
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR) {
      // Spacing separators are invisible gaps; only the others draw a line.
      const auto type = model->GetSeparatorTypeAt(i);
      Line("menu " + indent +
           (type == ui::SPACING_SEPARATOR ? "(gap)" : "----"));
      continue;
    }
    std::string label = base::UTF16ToUTF8(model->GetLabelAt(i));
    if (label.empty()) {
      label = "(command " + std::to_string(model->GetCommandIdAt(i)) + ")";
    }
    Line("menu " + indent + label +
         (model->IsVisibleAt(i) ? "" : " [hidden]") +
         (model->IsEnabledAt(i) ? "" : " [disabled]"));
    if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU &&
        model->IsVisibleAt(i)) {
      DumpMenu(model->GetSubmenuModelAt(i), depth + 1);
    }
  }
}

void DumpChildren(const char* what, views::View* parent) {
  if (!parent) {
    return;
  }
  for (views::View* child : parent->children()) {
    const bool shown = child->GetVisible() && !child->bounds().IsEmpty();
    Line(std::string(what) + " " + std::string(child->GetClassName()) +
         (shown ? " " + child->bounds().ToString() : " [not shown]"));
  }
}

void Dump(std::unique_ptr<views::ViewTracker> tracker) {
  auto* browser_view = static_cast<BrowserView*>(tracker->view());
  if (!browser_view) {
    return;
  }
  // WRYLING_DUMP_UI=repeat: dump again every 3 seconds, to catch states that
  // only happen while someone uses the window.
  if (const char* flag = std::getenv("WRYLING_DUMP_UI");
      flag && std::string(flag) == "repeat") {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&Dump,
                       std::make_unique<views::ViewTracker>(browser_view)),
        base::Seconds(3));
  }
  Line(std::string("window active=") +
       (browser_view->GetWidget()->IsActive() ? "yes" : "no"));
  Line("begin");
  if (auto* glass = GlassFrameService::GetInstance()) {
    Line(std::string("glass_eligible ") +
         (glass->IsBrowserWindowEligible(browser_view->browser()) ? "yes"
                                                                   : "no"));
  } else {
    Line("glass_eligible no-service");
  }
  auto* toolbar = browser_view->toolbar();
  {
    BraveAppMenuModel model(toolbar, browser_view->browser());
    model.Init();
    DumpMenu(&model, 0);
  }
  DumpChildren("toolbar", toolbar);
  if (auto* location_bar = browser_view->GetLocationBarView()) {
    DumpChildren("location_bar", location_bar);
    // Focus rings and focus state around the address bar (its own ring and
    // any ring on a child, e.g. the text field).
    std::function<void(views::View*, int)> rings = [&](views::View* v,
                                                        int depth) {
      for (views::View* child : v->children()) {
        if (std::string(child->GetClassName()) == "FocusRing") {
          Line(std::string("focus_ring on ") + std::string(v->GetClassName()) +
               (child->GetVisible() ? " visible " : " hidden ") +
               child->bounds().ToString());
        }
        if (depth < 3) {
          rings(child, depth + 1);
        }
      }
    };
    rings(location_bar, 0);
    Line(std::string("location_bar focus=") +
         (location_bar->omnibox_view() &&
                  location_bar->omnibox_view()->HasFocus()
              ? "omnibox"
              : "none") +
         " background=" +
         (location_bar->GetBackground() ? "set" : "none"));
  }
  Line("end");
}

}  // namespace

void MaybeScheduleUiDump(BrowserView* browser_view) {
  // Test only: Chromium gives a window the glass frame only after it has been
  // activated, and scripted test windows never are, so their captures show
  // the solid fallback. WRYLING_TEST_FORCE_GLASS=1 reports this window as
  // activated so checks can see glass mode.
  if (const char* force = std::getenv("WRYLING_TEST_FORCE_GLASS");
      force && *force) {
    // A moment later: at this point the window isn't in the browser list
    // yet, so the service would ignore it.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<views::ViewTracker> tracker) {
              auto* view = static_cast<BrowserView*>(tracker->view());
              auto* glass = GlassFrameService::GetInstance();
              if (view && glass) {
                static_cast<BrowserCollectionObserver*>(glass)
                    ->OnBrowserActivated(view->browser());
              }
            },
            std::make_unique<views::ViewTracker>(browser_view)),
        base::Seconds(1));
  }

  const char* flag = std::getenv("WRYLING_DUMP_UI");
  if (!flag || !*flag) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Dump, std::make_unique<views::ViewTracker>(browser_view)),
      base::Seconds(5));
}

}  // namespace wryling
