/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_FRAME_WRYLING_UI_DUMP_H_
#define BRAVE_BROWSER_UI_VIEWS_FRAME_WRYLING_UI_DUMP_H_

class BrowserView;

namespace wryling {

// Test only (never set in normal use): with WRYLING_DUMP_UI=1 in the
// environment, a few seconds after the window opens this writes the app menu
// (every item, nested, with visible/enabled state) and the visible views of
// the toolbar and address bar to stderr as "WRYLING_UI: ..." lines, so
// scripted checks can verify menus and chrome without opening them by hand.
void MaybeScheduleUiDump(BrowserView* browser_view);

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_VIEWS_FRAME_WRYLING_UI_DUMP_H_
