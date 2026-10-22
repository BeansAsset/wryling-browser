/* Copyright (c) 2026 The Wryling Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

// Test only: the Cocoa half of the WRYLING_TEST_DRAG drag replay (the
// scenarios are in wryling_split_controller.cc). Posts synthetic mouse events
// inside the app, so drags can be checked without Accessibility access or
// moving the real cursor. Like Chromium's ui_controls on Mac, while a replay
// runs +[NSEvent mouseLocation] and pressedMouseButtons report the synthetic
// pointer.
//
// Kept to Cocoa only on purpose: Objective-C++ compiles on the Mac (not the
// Steam Deck), and Chromium's view headers made this file too big for the
// build's memory guard.

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

namespace {

NSPoint g_mouse_location;
bool g_overriding = false;
bool g_button_down = false;
NSWindow* __strong g_window = nil;

}  // namespace

@interface NSEvent (WrylingTestDrag)
+ (NSPoint)wryling_mouseLocation;
+ (NSUInteger)wryling_pressedMouseButtons;
@end

@implementation NSEvent (WrylingTestDrag)
+ (NSPoint)wryling_mouseLocation {
  // After the swap this calls the original implementation.
  return g_overriding ? g_mouse_location : [self wryling_mouseLocation];
}
+ (NSUInteger)wryling_pressedMouseButtons {
  return g_overriding ? (g_button_down ? 1 : 0)
                      : [self wryling_pressedMouseButtons];
}
@end

namespace wryling::test_drag {

void SetOverride(bool on) {
  static bool swizzled = false;
  if (on && !swizzled) {
    swizzled = true;
    Class meta = object_getClass([NSEvent class]);
    method_exchangeImplementations(
        class_getInstanceMethod(meta, @selector(mouseLocation)),
        class_getInstanceMethod(meta, @selector(wryling_mouseLocation)));
    method_exchangeImplementations(
        class_getInstanceMethod(meta, @selector(pressedMouseButtons)),
        class_getInstanceMethod(meta, @selector(wryling_pressedMouseButtons)));
  }
  g_overriding = on;
  if (!on) {
    g_window = nil;
    g_button_down = false;
  }
}

// `type`: 0 press, 1 drag, 2 release, 3 move (no button). (`x`, `y`): screen point with a
// top-left origin (Chromium's screen coordinates). A press picks this app's
// front-most window under the point; the drag and release that follow go to
// the same window, as AppKit does for real drags.
void PostMouse(int type, int x, int y) {
  const CGFloat screen_height = NSScreen.screens.firstObject.frame.size.height;
  const NSPoint cocoa = NSMakePoint(x, screen_height - y);
  g_mouse_location = cocoa;
  if (type == 0) {
    g_button_down = true;
    g_window = nil;
    for (NSWindow* window in NSApp.orderedWindows) {
      if (window.visible && NSPointInRect(cocoa, window.frame)) {
        g_window = window;
        break;
      }
    }
  } else if (type == 2) {
    g_button_down = false;
  } else if (type == 3) {
    // A plain move goes to whichever window is under the pointer.
    g_window = nil;
    for (NSWindow* window in NSApp.orderedWindows) {
      if (window.visible && NSPointInRect(cocoa, window.frame)) {
        g_window = window;
        break;
      }
    }
  }
  if (!g_window) {
    return;
  }
  const NSEventType event_type = type == 0   ? NSEventTypeLeftMouseDown
                                 : type == 1 ? NSEventTypeLeftMouseDragged
                                 : type == 2 ? NSEventTypeLeftMouseUp
                                             : NSEventTypeMouseMoved;
  NSEvent* event =
      [NSEvent mouseEventWithType:event_type
                         location:[g_window convertPointFromScreen:cocoa]
                    modifierFlags:0
                        timestamp:NSProcessInfo.processInfo.systemUptime
                     windowNumber:g_window.windowNumber
                          context:nil
                      eventNumber:0
                       clickCount:1
                         pressure:type == 2 ? 0.0 : 1.0];
  [NSApp postEvent:event atStart:NO];
}

}  // namespace wryling::test_drag
