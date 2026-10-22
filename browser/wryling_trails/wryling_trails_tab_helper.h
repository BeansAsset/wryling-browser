/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_TAB_HELPER_H_
#define BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_TAB_HELPER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "brave/browser/wryling_trails/trail_recorder.h"
#include "brave/browser/wryling_trails/wryling_trails.mojom-shared.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace wryling_trails {

class WrylingTrailsService;

// Records one tab's committed navigations into its profile's trails, and
// which tab opened which.
class WrylingTrailsTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WrylingTrailsTabHelper> {
 public:
  // Attaches a helper when the tab's profile keeps trails (not incognito).
  static void MaybeCreateForWebContents(content::WebContents* contents);

  WrylingTrailsTabHelper(const WrylingTrailsTabHelper&) = delete;
  WrylingTrailsTabHelper& operator=(const WrylingTrailsTabHelper&) = delete;
  ~WrylingTrailsTabHelper() override;

  // SessionID of the tab, as used by the mojo API.
  int32_t tab_id() const;
  NodeId current_node() const { return state_.cursor; }
  bool HasEntryFor(NodeId node) const;

  // Goes to `node` in this tab: back/forward when the tab still has it, else
  // loads its URL and continues the trail from it.
  mojom::RestoreResult Show(NodeId node);
  // The next commit in this tab is a restore of `node`.
  void ExpectRestore(NodeId node) { state_.expected_restore = node; }

  void OnServiceShutdown();

 private:
  friend class content::WebContentsUserData<WrylingTrailsTabHelper>;

  WrylingTrailsTabHelper(content::WebContents* contents,
                         WrylingTrailsService* service);

  // Runs `task` against the loaded graph, unless this tab or the service is
  // gone by then.
  void WhenLoaded(base::OnceCallback<void(WrylingTrailsTabHelper*)> task);
  void Apply(const Commit& commit);
  bool IsRequestedOffTheRecord();

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates,
      blink::mojom::FaviconUpdateReason reason) override;
  void WebContentsDestroyed() override;

  raw_ptr<WrylingTrailsService> service_;
  TabState state_;
  base::WeakPtrFactory<WrylingTrailsTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace wryling_trails

#endif  // BRAVE_BROWSER_WRYLING_TRAILS_WRYLING_TRAILS_TAB_HELPER_H_
