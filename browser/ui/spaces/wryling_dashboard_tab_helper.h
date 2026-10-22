/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_TAB_HELPER_H_
#define BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_TAB_HELPER_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"

class GURL;

namespace wryling {

class DashboardService;

// Counts the sites a tab opens towards its space's dashboard, and learns the
// sites' app icons and names as pages declare them.
class DashboardTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DashboardTabHelper> {
 public:
  // Attaches a helper when the tab's profile has dashboards (not incognito).
  static void MaybeCreateForWebContents(content::WebContents* contents);

  DashboardTabHelper(const DashboardTabHelper&) = delete;
  DashboardTabHelper& operator=(const DashboardTabHelper&) = delete;
  ~DashboardTabHelper() override;

 private:
  friend class content::WebContentsUserData<DashboardTabHelper>;

  explicit DashboardTabHelper(content::WebContents* contents);

  DashboardService* GetService();
  bool IsRequestedOffTheRecord();
  void OnManifest(const GURL& page_url,
                  blink::mojom::ManifestRequestResult result,
                  const GURL& manifest_url,
                  blink::mojom::ManifestPtr manifest);

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates,
      blink::mojom::FaviconUpdateReason reason) override;

  // Hosts whose manifest this tab already asked for.
  std::set<std::string> manifest_asked_;
  // Whether the current page counted as a visit (so its icons are wanted).
  bool page_recorded_ = false;
  base::WeakPtrFactory<DashboardTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace wryling

#endif  // BRAVE_BROWSER_UI_SPACES_WRYLING_DASHBOARD_TAB_HELPER_H_
