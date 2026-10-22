/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/spaces/wryling_home_navigation_throttle.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "brave/browser/ui/spaces/wryling_spaces.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "ui/base/window_open_disposition.h"

namespace wryling {

namespace {

void OpenInNewTab(base::WeakPtr<content::WebContents> home,
                  GURL url,
                  content::Referrer referrer) {
  tabs::TabInterface* tab =
      home ? tabs::TabInterface::MaybeGetFromContents(home.get()) : nullptr;
  BrowserWindowInterface* browser =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  if (!browser) {
    return;
  }
  // New tabs go in the window's current space, which is Home's.
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.referrer = referrer;
  Navigate(&params);
}

}  // namespace

// static
void HomeNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (registry.GetNavigationHandle().IsInPrimaryMainFrame()) {
    registry.AddThrottle(std::make_unique<HomeNavigationThrottle>(registry));
  }
}

HomeNavigationThrottle::HomeNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

HomeNavigationThrottle::~HomeNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
HomeNavigationThrottle::WillStartRequest() {
  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* contents = handle->GetWebContents();
  if (!contents || handle->IsSameDocument() || IsHomeURL(handle->GetURL()) ||
      handle->GetRequestMethod() != net::HttpRequestHeaders::kGetMethod) {
    return PROCEED;
  }
  tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(contents);
  SpacesController* controller =
      tab ? SpacesController::From(tab->GetBrowserWindowInterface()) : nullptr;
  if (!controller || !controller->IsHomeTab(tab)) {
    return PROCEED;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OpenInNewTab, contents->GetWeakPtr(), handle->GetURL(),
                     content::Referrer(handle->GetReferrer())));
  return CANCEL_AND_IGNORE;
}

const char* HomeNavigationThrottle::GetNameForLogging() {
  return "WrylingHomeNavigationThrottle";
}

}  // namespace wryling
