/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/spaces/wryling_dashboard_tab_helper.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "brave/browser/ui/spaces/wryling_dashboard_service.h"
#include "brave/browser/ui/spaces/wryling_dashboard_service_factory.h"
#include "brave/browser/ui/spaces/wryling_spaces.h"
#include "brave/components/request_otr/common/buildflags/buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(ENABLE_REQUEST_OTR)
#include "brave/components/request_otr/browser/request_otr_storage_tab_helper.h"
#endif

namespace wryling {

namespace {

constexpr size_t kMaxTitleName = 24;

// The space the tab is in, or empty outside a normal browser window.
std::string GetSpaceOf(content::WebContents* contents) {
  tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(contents);
  if (!tab) {
    return std::string();
  }
  SpacesController* controller =
      SpacesController::From(tab->GetBrowserWindowInterface());
  return controller ? controller->GetTabSpace(tab) : std::string();
}

// "Hacker News" from "Hacker News", "YouTube" from "Home - YouTube": the part
// of a home page's title that names the site.
std::string NameFromTitle(const std::u16string& title, const GURL& url) {
  std::string text = base::UTF16ToUTF8(title);
  // Separators sites put in titles: bar, em dash, en dash, middle dot, bullet.
  for (const char* separator :
       {" | ", " \u2014 ", " \u2013 ", " \u00b7 ", " \u2022 "}) {
    base::ReplaceSubstringsAfterOffset(&text, 0, separator, " - ");
  }
  std::vector<std::string> parts = base::SplitStringUsingSubstr(
      text, " - ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.empty()) {
    return std::string();
  }
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  const std::string label =
      base::ToLowerASCII(domain.substr(0, domain.find('.')));
  std::string name;
  for (const std::string& part : parts) {
    std::string squashed = base::ToLowerASCII(part);
    std::erase(squashed, ' ');
    if (!label.empty() && squashed.find(label) != std::string::npos) {
      name = part;
      break;
    }
  }
  if (name.empty()) {
    name = parts.size() == 1 ? parts.front() : parts.back();
  }
  return name.size() <= kMaxTitleName ? name : std::string();
}

}  // namespace

// static
void DashboardTabHelper::MaybeCreateForWebContents(
    content::WebContents* contents) {
  if (FromWebContents(contents)) {
    return;
  }
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (DashboardServiceFactory::GetForProfile(profile)) {
    CreateForWebContents(contents);
  }
}

DashboardTabHelper::DashboardTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<DashboardTabHelper>(*contents) {}

DashboardTabHelper::~DashboardTabHelper() = default;

DashboardService* DashboardTabHelper::GetService() {
  return DashboardServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

bool DashboardTabHelper::IsRequestedOffTheRecord() {
#if BUILDFLAG(ENABLE_REQUEST_OTR)
  auto* storage =
      request_otr::RequestOTRStorageTabHelper::FromWebContents(web_contents());
  return storage && storage->has_requested_otr();
#else
  return false;
#endif
}

void DashboardTabHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted() ||
      handle->IsSameDocument()) {
    return;
  }
  page_recorded_ = false;
  DashboardService* service = GetService();
  const GURL& url = handle->GetURL();
  if (!service || handle->IsErrorPage() || !url.SchemeIsHTTPOrHTTPS() ||
      !handle->ShouldUpdateHistory() || IsRequestedOffTheRecord()) {
    return;
  }
  const ui::PageTransition transition = handle->GetPageTransition();
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) ||
      (transition & ui::PAGE_TRANSITION_FORWARD_BACK)) {
    return;
  }
  const std::string space = GetSpaceOf(web_contents());
  if (space.empty()) {
    return;
  }
  service->RecordVisit(space, url);
  page_recorded_ = !service->IsHistoryDisabled();
}

void DashboardTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  DashboardService* service = GetService();
  if (!service || !page_recorded_ || !entry ||
      entry != web_contents()->GetController().GetLastCommittedEntry() ||
      entry->GetURL().path() != "/") {
    return;
  }
  const std::string name = NameFromTitle(entry->GetTitle(), entry->GetURL());
  if (!name.empty()) {
    service->RecordSiteName(entry->GetURL(), name, /*from_manifest=*/false);
  }
}

void DashboardTabHelper::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates,
    blink::mojom::FaviconUpdateReason reason) {
  DashboardService* service = GetService();
  if (!service || !page_recorded_ || !render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  std::vector<IconCandidate> icons;
  for (const blink::mojom::FaviconURLPtr& candidate : candidates) {
    if (candidate->is_default_icon) {
      continue;
    }
    IconCandidate icon;
    icon.url = candidate->icon_url;
    icon.app_icon =
        candidate->icon_type == blink::mojom::FaviconIconType::kTouchIcon ||
        candidate->icon_type ==
            blink::mojom::FaviconIconType::kTouchPrecomposedIcon;
    for (const gfx::Size& size : candidate->icon_sizes) {
      icon.declared_size = std::max(icon.declared_size, size.width());
    }
    icons.push_back(std::move(icon));
  }
  service->LearnIcons(web_contents()->GetLastCommittedURL(), std::move(icons));
}

void DashboardTabHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  DashboardService* service = GetService();
  if (!service || !page_recorded_ ||
      !render_frame_host->IsInPrimaryMainFrame() ||
      !service->WantsSiteInfo(validated_url)) {
    return;
  }
  // Once per site per tab: the manifest gives the site's short name and its
  // home-screen icons.
  if (!manifest_asked_.insert(std::string(validated_url.host())).second) {
    return;
  }
  web_contents()->GetPrimaryPage().GetManifest(
      base::BindOnce(&DashboardTabHelper::OnManifest,
                     weak_factory_.GetWeakPtr(), validated_url));
}

void DashboardTabHelper::OnManifest(const GURL& page_url,
                                    blink::mojom::ManifestRequestResult result,
                                    const GURL& manifest_url,
                                    blink::mojom::ManifestPtr manifest) {
  DashboardService* service = GetService();
  if (!service || !manifest ||
      result != blink::mojom::ManifestRequestResult::kSuccess) {
    return;
  }
  if (manifest->short_name && !manifest->short_name->empty()) {
    service->RecordSiteName(page_url, base::UTF16ToUTF8(*manifest->short_name),
                            /*from_manifest=*/true);
  } else if (manifest->name && !manifest->name->empty() &&
             manifest->name->size() <= kMaxTitleName) {
    service->RecordSiteName(page_url, base::UTF16ToUTF8(*manifest->name),
                            /*from_manifest=*/true);
  }
  std::vector<IconCandidate> icons;
  for (const blink::Manifest::ImageResource& resource : manifest->icons) {
    if (!std::ranges::contains(
            resource.purpose,
            blink::mojom::ManifestImageResource_Purpose::ANY) &&
        !std::ranges::contains(
            resource.purpose,
            blink::mojom::ManifestImageResource_Purpose::MASKABLE)) {
      continue;
    }
    IconCandidate icon;
    icon.url = resource.src;
    icon.app_icon = true;
    for (const gfx::Size& size : resource.sizes) {
      icon.declared_size = std::max(icon.declared_size, size.width());
    }
    // Skip icons too big to be worth downloading for a 60pt tile.
    if (icon.declared_size > 512) {
      continue;
    }
    icons.push_back(std::move(icon));
  }
  service->LearnIcons(page_url, std::move(icons));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DashboardTabHelper);

}  // namespace wryling
