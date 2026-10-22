/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/ai_chat/core/common/pref_names.h"

#include <string_view>

#include "base/time/time.h"
#include "brave/components/ai_chat/core/common/constants.h"
#include "brave/components/ai_chat/core/common/features.h"
#include "components/prefs/pref_registry_simple.h"

namespace ai_chat::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (ai_chat::features::IsAIChatEnabled()) {
    registry->RegisterTimePref(kLastAcceptedDisclaimer, {});
    registry->RegisterBooleanPref(kBraveChatStorageEnabled, true);
    // Wryling: Brave extras are off until turned on in Settings.
    registry->RegisterBooleanPref(kBraveChatAutocompleteProviderEnabled, false);
    registry->RegisterBooleanPref(kUserDismissedPremiumPrompt, false);
    registry->RegisterBooleanPref(kUserDismissedStorageNotice, false);
#if BUILDFLAG(IS_ANDROID)
    registry->RegisterBooleanPref(kBraveChatSubscriptionActiveAndroid, false);
    registry->RegisterStringPref(kBraveChatPurchaseTokenAndroid, "");
    registry->RegisterStringPref(kBraveChatPackageNameAndroid, "");
    registry->RegisterStringPref(kBraveChatProductIdAndroid, "");
    registry->RegisterStringPref(kBraveChatOrderIdAndroid, "");
    // 0 is not linked
    registry->RegisterIntegerPref(kBraveChatSubscriptionLinkStatusAndroid, 0);
#endif
    registry->RegisterBooleanPref(kBraveAIChatContextMenuEnabled, false);
    registry->RegisterBooleanPref(kBraveAIChatShowToolbarButton, false);
    registry->RegisterBooleanPref(kBraveAIChatToolbarButtonOpensFullPage,
                                  false);
    registry->RegisterBooleanPref(kBraveAIChatTabOrganizationEnabled, false);
    registry->RegisterStringPref(kBraveAIChatTabOrganizationModelKey,
                                 kChatAutomaticModelKey);
    registry->RegisterBooleanPref(kBraveAIChatUserCustomizationEnabled, true);
    registry->RegisterBooleanPref(kBraveAIChatUserMemoryEnabled, true);
    registry->RegisterDictionaryPref(kBraveAIChatUserCustomizations);
    registry->RegisterListPref(kBraveAIChatUserMemories);
    registry->RegisterDictionaryPref(kBraveAIChatSkills);
    registry->RegisterBooleanPref(kBraveAIChatOllamaFetchEnabled, false);
    registry->RegisterStringPref(kBraveAIChatConversationShares, "");
    registry->RegisterDictionaryPref(kAIChatObliviousHttpKeyConfigs);
    registry->RegisterTimePref(kRemoteModelsCachedAt, {});
  }
  // Wryling: Leo is off until turned on in Settings › Extras.
  registry->RegisterBooleanPref(kEnabledByPolicy, false);
}

void RegisterProfilePrefsForMigration(PrefRegistrySimple* registry) {
  if (ai_chat::features::IsAIChatEnabled()) {
    registry->RegisterBooleanPref(kObseleteBraveChatAutoGenerateQuestions,
                                  false);
  }
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // Added 11/2023
  registry->RegisterDictionaryPref(kBraveChatPremiumCredentialCache);
  registry->RegisterBooleanPref(kNtpInputDayZeroEnabled, false);
  registry->RegisterStringPref(kNtpInputSourceSuffix, "");
}

}  // namespace ai_chat::prefs
