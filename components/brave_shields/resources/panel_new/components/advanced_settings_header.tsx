/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import { useShieldsApi } from '../api/shields_api_context'
import { getString } from './strings'

import { style } from './advanced_settings_header.style'

export function AdvancedSettingsHeader() {
  const api = useShieldsApi()
  const { data: siteBlockInfo } = api.useGetSiteBlockInfo()
  const showAdvancedView = api.useGetAdvancedViewEnabledData()

  if (!siteBlockInfo) {
    return null
  }

  const shieldsEnabled = siteBlockInfo.isBraveShieldsEnabled
  const adblockOnlyEnabled = siteBlockInfo.isBraveShieldsAdBlockOnlyModeEnabled

  if (!shieldsEnabled || adblockOnlyEnabled) {
    return null
  }

  return (
    <div
      data-css-scope={style.scope}
      data-expanded={showAdvancedView}
    >
      <button
        className='view-toggle'
        aria-expanded={showAdvancedView}
        aria-controls='shields-advanced-settings'
        onClick={() => {
          api.setAdvancedViewEnabled([!showAdvancedView])
        }}
      >
        <span>{getString('BRAVE_SHIELDS_ADVANCED_OPTIONS')}</span>
        <svg
          viewBox='0 0 16 16'
          fill='none'
          aria-hidden='true'
        >
          <path
            d='m4 6 4 4 4-4'
            stroke='currentColor'
            strokeWidth='1.5'
            strokeLinecap='round'
            strokeLinejoin='round'
          />
        </svg>
      </button>
    </div>
  )
}
