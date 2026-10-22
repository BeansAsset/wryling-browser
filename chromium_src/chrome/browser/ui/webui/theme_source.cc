// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "brave/browser/ui/color/wryling_webui_theme.h"
#include "build/build_config.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
namespace {
void AppendWrylingThemeCss(content::URLDataSource::GotDataCallback callback,
                          std::string css,
                          scoped_refptr<base::RefCountedMemory> data) {
  if (!data) {
    std::move(callback).Run(nullptr);
    return;
  }
  std::string result(data->begin(), data->end());
  result.append(css);
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(result)));
}
}  // namespace
#endif  // !BUILDFLAG(IS_ANDROID)

// Unlike Chromium we also load the ThemeSource in the system profile, where the
// ThemeService is |nullptr|.
#define BRAVE_THEME_SOURCE_SEND_COLORS_CSS                                    \
  if (!theme_service) {                                                       \
    CHECK(profile_->IsSystemProfile());                                       \
    std::move(callback).Run(nullptr);                                         \
    return;                                                                   \
  }                                                                           \
  const GURL& page_url = wc_getter.Run()->GetLastCommittedURL();               \
  if (page_url.SchemeIs("chrome") || page_url.SchemeIs("chrome-untrusted")) {  \
    ui::ColorProviderKey base_key;                                            \
    base_key.color_mode = wc_getter.Run()->GetColorMode();                     \
    base_key.user_color = theme_service->GetUserColor();                      \
    std::string shadow_host;                                                 \
    net::GetValueForKeyInQuery(url, "shadow_host", &shadow_host);              \
    callback = base::BindOnce(                                               \
        &AppendWrylingThemeCss, std::move(callback),                           \
        GetWrylingWebUIThemeCss(base_key, shadow_host == "true"));                  \
  }

#include <chrome/browser/ui/webui/theme_source.cc>

#undef BRAVE_THEME_SOURCE_SEND_COLORS_CSS
