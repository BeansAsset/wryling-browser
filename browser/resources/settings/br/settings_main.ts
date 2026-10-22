// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import {
  html,
  RegisterPolymerPrototypeModification,
  RegisterPolymerTemplateModifications
} from 'chrome://resources/brave/polymer_overriding.js'

import { loadTimeData } from '../i18n_setup.js'

import {extrasReady, guardExtraRoute} from '../wryling_extras.js'
import type {SettingsMainElement} from '../settings_main/settings_main.js'
import type {Route} from '../router.js'
import '../brave_content_page/content_page_index.js'
import '../getting_started_page/getting_started_page_index.js'
import '../brave_origin_page/brave_origin_page.js'
import '../default_brave_shields_page/shields_page_index.js'
import '../brave_default_extensions_page/brave_extensions_page_index.js'
import '../brave_sync_page/brave_sync_page_index.js'
import '../brave_system_page/brave_system_page_index.js'

// <if expr="enable_ai_chat">
import '../brave_leo_assistant_page/brave_leo_assistant_page_index.js'
// </if>

// <if expr="enable_brave_wallet">
import '../brave_wallet_page/wallet_page_index.js'
// </if>

RegisterPolymerTemplateModifications({
  'settings-main': (templateContent) => {
    const header = html`<header class="wry-page-header"><h1 data-wry-title></h1></header>`
    templateContent.querySelector('#switcher')?.before(header)
    // Wryling: the scrolled header is a slim, full-width glass strip with
    // the Headline title, not the large header in a glass
    // box. The header keeps its 84px of layout in both states, so content
    // never jumps and the scroll threshold can't oscillate; it sticks 40px
    // above the top, which leaves only the bottom 44px on screen.
    templateContent.querySelector('#switcher')?.before(html`<style>
      :host(settings-main) .wry-page-header {
        top: -40px; height: 84px; box-sizing: border-box;
        display: flex; align-items: flex-end;
        padding: 0 0 24px !important; isolation: isolate; top: -40px !important;
      }
      :host(settings-main) .wry-page-header::before {
        content: ''; position: absolute; z-index: -1;
        inset: 0 -2000px; opacity: 0; transition: opacity 160ms ease-out;
        background: var(--wry-web-glass);
        backdrop-filter: var(--wry-web-glass-filter);
        border-bottom: var(--wry-hairline) solid var(--wry-separator);
      }
      :host(settings-main) .wry-page-header[data-compact=true] {
        background: transparent !important; backdrop-filter: none !important;
        padding-bottom: 14px !important;
      }
      :host(settings-main) .wry-page-header[data-compact=true]::before { opacity: 1; }
      :host(settings-main) .wry-page-header[data-compact=true] h1 {
        font: var(--wry-headline) !important;
      }
      @media (prefers-reduced-transparency: reduce) {
        :host(settings-main) .wry-page-header::before { background: var(--wry-raised); backdrop-filter: none; }
      }
    </style>`)

    // Always show settings-basic-page
    const templatePrivacy = templateContent.querySelector(
      'template[is=dom-if][if="[[showPage_(pageVisibility_.privacy)]]"]')
    if (!templatePrivacy) {
      throw new Error('[Settings] Missing template for privacy')
    }
    const templateSettingsBasicPage =
      templatePrivacy.content.querySelector('#privacy template')
    if (templateSettingsBasicPage) {
      templateSettingsBasicPage.setAttribute('if', 'true')
    } else {
      throw new Error('[Settings] Missing template for settings-basic-page')
    }

    // Remove the performance page template
    const templatePerformancePageSlot = templateContent.querySelector(
      'template[is=dom-if][if="[[showPage_(pageVisibility_.performance)]]"]')
    if (templatePerformancePageSlot) {
      templatePerformancePageSlot.remove()
    } else {
      throw new Error('[Settings] Missing template for performance page slot')
    }

    // Grab the view manager
    const switcher = templateContent.querySelector('#switcher')
    if (!switcher) {
      throw new Error('[Settings] Missing switcher on settings-basic-page')
    }

    if (!loadTimeData.getBoolean('isGuest')) switcher.appendChild(html`
      <div slot="view" id="extras">
        <template is="dom-if" if="[[renderPlugin_(routes_.WRYLING_EXTRAS, lastRoute_, inSearchMode_)]]">
          <settings-wryling-extras class="cr-centered-card-container"></settings-wryling-extras>
        </template>
      </div>`)

    // Insert the getStarted page into the view manager
    switcher.appendChild(
      html`
        <template is="dom-if" if="[[showPage_(pageVisibility_.getStarted)]]">
          <div slot="view" id="getStarted">
            <template is="dom-if" if="[[renderPlugin_(
          routes_.GET_STARTED, lastRoute_, inSearchMode_)]]">
              <settings-getting-started-page-index
                prefs="{{prefs}}"
                in-search-mode="[[inSearchMode_]]">
              </settings-getting-started-page-index>
            </template>
          </div>
        </template>
      `)

    // Insert the origin page into the view manager
    switcher.appendChild(
      html`
        <template is="dom-if" if="[[showPage_(pageVisibility_.origin)]]">
          <div slot="view" id="origin">
            <template is="dom-if" if="[[renderPlugin_(
          routes_.ORIGIN, lastRoute_, inSearchMode_)]]">
              <settings-brave-origin-page class="cr-centered-card-container"
                prefs="{{prefs}}"
                in-search-mode="[[inSearchMode_]]">
              </settings-brave-origin-page>
            </template>
          </div>
        </template>
      `)

    // Insert the content page into the view manager
    switcher.appendChild(
      html`
        <template is="dom-if"
            if="[[showPage_(pageVisibility_.content)]]">
          <div slot="view" id="content">
            <template is="dom-if" if="[[renderPlugin_(
          routes_.BRAVE_CONTENT, lastRoute_, inSearchMode_)]]">
              <settings-brave-content-page-index
                prefs="{{prefs}}"
                in-search-mode="[[inSearchMode_]]">
              </settings-brave-content-page-index>
            </template>
          </div>
        </template>
      `)

    // Insert the shields page into the view manager
    switcher.appendChild(
      html`
        <template is="dom-if" if="[[showPage_(pageVisibility_.shields)]]">
          <div slot="view" id="shields">
            <template is="dom-if" if="[[renderPlugin_(
          routes_.SHIELDS, lastRoute_, inSearchMode_)]]">
              <settings-shields-page-index
                prefs="{{prefs}}"
                in-search-mode="[[inSearchMode_]]">
              </settings-shields-page-index>
            </template>
          </div>
        </template>
      `)

    // Insert the web3 page into the view manager
    // <if expr="enable_brave_wallet">
    const isBraveWalletAllowed = true
    if (isBraveWalletAllowed) {
      switcher.appendChild(
        html`
          <template is="dom-if" if="[[showPage_(pageVisibility_.braveWallet)]]">
            <div slot="view" id="web3">
              <template is="dom-if" if="[[renderPlugin_(
          routes_.BRAVE_WEB3, lastRoute_, inSearchMode_)]]">
                <settings-wallet-page-index
                  prefs="{{prefs}}"
                  in-search-mode="[[inSearchMode_]]">
                </settings-wallet-page-index>
              </template>
            </div>
          </template>
        `)
    }
    // </if>

    // Insert the leo page into the view manager
    // <if expr="enable_ai_chat">
    switcher.appendChild(
      html`
        <template is="dom-if" if="[[showPage_(pageVisibility_.leoAssistant)]]">
          <div slot="view" id="leoAssistant">
            <template is="dom-if" if="[[renderPlugin_(
          routes_.BRAVE_LEO_ASSISTANT, lastRoute_, inSearchMode_)]]">
              <settings-brave-leo-assistant-page-index
                prefs="{{prefs}}"
                in-search-mode="[[inSearchMode_]]">
              </settings-brave-leo-assistant-page-index>
            </template>
          </div>
        </template>
      `)
    // </if>

    // Insert the sync page into the view manager
    switcher.appendChild(
      html`
        <template is="dom-if" if="[[showPage_(pageVisibility_.braveSync)]]">
          <div slot="view" id="braveSync">
            <template is="dom-if" if="[[renderPlugin_(
              routes_.BRAVE_SYNC, lastRoute_, inSearchMode_)]]">
              <settings-brave-sync-page-index
                prefs="{{prefs}}"
                in-search-mode="[[inSearchMode_]]">
              </settings-brave-sync-page-index>
            </template>
          </div>
        </template>
      `)

    // Insert the extensions page into the view manager
    switcher.appendChild(
      html`
        <template is="dom-if" if="[[showPage_(pageVisibility_.extensions)]]">
          <div slot="view" id="extensions">
            <template is="dom-if" if="true">
              <settings-brave-extensions-page-index
                prefs="{{prefs}}"
                in-search-mode="[[inSearchMode_]]">
              </settings-brave-extensions-page-index>
            </template>
          </div>
        </template>
      `)

    // Replace the system page with the system page index (upstream doesn't have sub pages for system).
    const templateSystemPageSlot = templateContent.querySelector(
      'template[is=dom-if][if="[[showPage_(pageVisibility_.system)]]"]')
    if (templateSystemPageSlot) {
      templateSystemPageSlot.replaceWith(html`<template is="dom-if" if="[[showPage_(pageVisibility_.extensions)]]">
          <div slot="view" id="system">
            <template is="dom-if" if="[[renderPlugin_(
          routes_.SYSTEM, lastRoute_, inSearchMode_)]]">
              <settings-brave-system-page-index
                prefs="{{prefs}}"
                in-search-mode="[[inSearchMode_]]">
              </settings-brave-system-page-index>
            </template>
          </div>
        </template>`)
    } else {
      throw new Error('[Settings] Missing template for system page slot')
    }
  }
})

// Resolve policy state before the view manager attempts to show a hidden page.
RegisterPolymerPrototypeModification({
  'settings-main': (prototype: SettingsMainElement) => {
    const routeChanged = prototype.currentRouteChanged
    prototype.currentRouteChanged = function(this: SettingsMainElement, route: Route) {
      if (route.section !== 'leoAssistant' && route.section !== 'web3') return routeChanged.call(this, route)
      return extrasReady.then(() => {
        return guardExtraRoute(route) ? undefined : routeChanged.call(this, route)
      })
    }
  },
})
