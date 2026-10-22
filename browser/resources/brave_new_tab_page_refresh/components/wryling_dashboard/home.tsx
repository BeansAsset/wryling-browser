/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import * as React from 'react'
import Icon from '@brave/leo/react/icon'

import { SearchBox } from '../search/search_box'
import { Background } from '../background/background'
import { BackgroundCaption } from '../background/background_caption'
import { SettingsModal, SettingsView } from '../settings/settings_modal'
import { useSearchLayoutReady } from '../app_layout_ready'
import { WrylingDashboard } from './dashboard'

import { style } from '../app.style'

// A space's Home tab: chrome://newtab/?home. The plain New Tab page is the
// usual App.
export function isWrylingHome() {
  return new URLSearchParams(location.search).has('home')
}

const homeIconSvg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
<style>path{fill:#3b3550}@media (prefers-color-scheme: dark){path{fill:#ece9f5}}</style>
<path d="M16 4.2c.5 0 1 .2 1.4.5l11 9.1c.8.7.3 2-.8 2H26v10.4c0 1.2-1 2.1-2.1 2.1h-4.6v-7.1c0-.8-.6-1.4-1.4-1.4h-3.8c-.8 0-1.4.6-1.4 1.4v7.1H8.1C7 28.3 6 27.4 6 26.2V15.8H4.4c-1.1 0-1.6-1.3-.8-2l11-9.1c.4-.3.9-.5 1.4-.5z"/>
</svg>`

// The space's dashboard on the space's wallpaper, under the search box. Links
// from here open in new tabs (the browser keeps Home on its dashboard).
export function WrylingHome() {
  const searchLayoutReady = useSearchLayoutReady()
  const [settingsView, setSettingsView] = React.useState<SettingsView | null>(
    null,
  )

  React.useEffect(() => {
    document.title = 'Home'
    // A house for the pinned Home tab, dark or light to match the sidebar.
    for (const old of document.querySelectorAll('link[rel~="icon"]')) {
      old.remove()
    }
    const icon = document.createElement('link')
    icon.rel = 'icon'
    icon.type = 'image/svg+xml'
    icon.href = 'data:image/svg+xml,' + encodeURIComponent(homeIconSvg)
    document.head.appendChild(icon)
  }, [])

  return (
    <div data-css-scope={style.scope}>
      <Background />
      <main className='allow-background-pointer-events'>
        <button
          className='settings'
          aria-label='Customise'
          onClick={() => setSettingsView('background')}
        >
          <Icon name='settings' />
        </button>
        <div
          className='allow-background-pointer-events'
          style={{ flex: '0.6 1 auto', minHeight: 32 }}
        />
        <div className='searchbox-container allow-background-pointer-events'>
          {searchLayoutReady && (
            <SearchBox showSearchSettings={() => setSettingsView('search')} />
          )}
        </div>
        <div
          className='allow-background-pointer-events'
          style={{ alignSelf: 'stretch', marginTop: 8 }}
        >
          <WrylingDashboard />
        </div>
        <div
          className='allow-background-pointer-events'
          style={{ flex: '1 1 auto', minHeight: 32 }}
        />
        <div className='caption-container allow-background-pointer-events'>
          <BackgroundCaption />
        </div>
      </main>
      <SettingsModal
        isOpen={settingsView !== null}
        initialView={settingsView}
        onClose={() => setSettingsView(null)}
      />
    </div>
  )
}
