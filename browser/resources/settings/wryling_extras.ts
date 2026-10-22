// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js'
import {loadTimeData} from './i18n_setup.js'
import {Router} from './router.js'
import type {Route} from './router.js'
import type {SettingsUiElement} from './settings_ui/settings_ui.js'
import {getSearchManager} from './search_settings.js'
import {pageVisibility} from './br/page_visibility.js'

const extras = [
  {key: 'brave.ai_chat.enabled_by_policy', label: 'Leo AI', inverted: false},
  {key: 'brave.wallet.disabled_by_policy', label: 'Wallet', inverted: true},
  {key: 'brave.rewards.disabled_by_policy', label: 'Rewards', inverted: true},
  {key: 'brave.news.disabled_by_policy', label: 'News', inverted: true},
  {key: 'brave.brave_vpn.disabled_by_policy', label: 'VPN', inverted: true},
  {key: 'brave.talk.disabled_by_policy', label: 'Talk', inverted: true},
  {key: 'brave.playlist.enabled', label: 'Playlist', inverted: false},
]
const states = new Map<string, chrome.settingsPrivate.PrefObject>()
let initialized = false
export const extrasReady = chrome.settingsPrivate.getAllPrefs().then(prefs => {
  for (const pref of prefs) states.set(pref.key, pref)
  initialized = true
  return prefs
})
const enabled = (key: string) => {
  const pref = states.get(key)
  const extra = extras.find(e => e.key === key)
  return !!pref && !!extra && (extra.inverted ? !pref.value : !!pref.value)
}

export function guardExtraRoute(route: Route): boolean {
  if (!initialized) return false
  const key = route.section === 'leoAssistant' ? extras[0].key :
      route.section === 'web3' ? extras[1].key : ''
  if (!key || enabled(key)) return false
  const destination = Router.getInstance().getRouteForPath('/extras')
  if (destination) queueMicrotask(() => Router.getInstance().navigateTo(destination))
  return true
}

export function installExtrasVisibility(ui: SettingsUiElement) {
  const apply = (prefs: chrome.settingsPrivate.PrefObject[]) => {
    for (const pref of prefs) states.set(pref.key, pref)
    initialized = true
    const leo = enabled(extras[0].key), wallet = enabled(extras[1].key)
    pageVisibility.leoAssistant = leo
    pageVisibility.leoPersonalization = leo
    pageVisibility.leoModels = leo
    pageVisibility.braveWallet = wallet
    for (const [name, value] of Object.entries({leoAssistant: leo, braveWallet: wallet})) {
      ui.$.main.set('pageVisibility_.' + name, value)
    }
    for (const [path, visible] of [['/leo-ai', leo], ['/web3', wallet]] as const) {
      for (const menu of [ui.$.leftMenu, ui.shadowRoot!.querySelector('cr-drawer settings-menu')]) {
        const link = menu?.shadowRoot?.querySelector<HTMLAnchorElement>(`a[href="${path}"]`)
        if (link) link.hidden = !visible
      }
    }
    guardExtraRoute(Router.getInstance().getCurrentRoute())
  }
  void extrasReady.then(apply)
  // This listener belongs to this WebUI document and is torn down with it.
  chrome.settingsPrivate.onPrefsChanged.addListener(apply)
}

class SettingsWrylingExtras extends PolymerElement {
  static get is() { return 'settings-wryling-extras' }
  static get properties() { return {extras_: Array, error_: String, description_: {type: String, value: () => loadTimeData.getString('wrylingExtrasDescription')}, hint_: {type: String, value: () => loadTimeData.getString('wrylingExtrasHint')}} }
  static get template() {
    return html`
      <style>
        :host { display: block; }
        p { color: var(--wry-text-secondary); font: var(--wry-body); margin: 0 0 var(--wry-space-6); }
        .group { background: var(--wry-surface); border-radius: var(--wry-radius-xl); }
        .row { display: flex; align-items: center; min-height: var(--wry-row-height); padding: 0 var(--wry-space-4); gap: var(--wry-space-4); }
        .row + .row { border-top: var(--wry-hairline) solid var(--wry-separator); }
        .label { flex: 1; font: var(--wry-body); }
        button { flex: none; }
        .hint { margin-top: var(--wry-space-4); font: var(--wry-callout); }
        [role=alert] { color: var(--wry-destructive); }
      </style>
      <p>[[description_]]</p>
      <div class="group">
        <template is="dom-repeat" items="[[extras_]]">
          <div class="row">
            <span class="label" id$="extra-[[index]]">[[item.label]]</span>
            <button type="button" role="switch" class="wry-switch"
                data-pref$="[[item.key]]" aria-labelledby$="extra-[[index]]"
                aria-checked$="[[item.checked]]" disabled="[[item.managed]]" on-click="toggle_"></button>
          </div>
        </template>
      </div>
      <p class="hint">[[hint_]]</p>
      <p role="alert">[[error_]]</p>`
  }
  private declare extras_: Array<{key: string, label: string, checked: string, managed: boolean}>
  private declare error_: string
  private declare description_: string
  private declare hint_: string
  private onPrefs_ = () => chrome.settingsPrivate.getAllPrefs().then(prefs => {
    this.extras_ = extras.flatMap(extra => {
      const pref = prefs.find(p => p.key === extra.key)
      return pref && typeof pref.value === 'boolean' ? [{...extra,
        checked: String(extra.inverted ? !pref.value : pref.value),
        managed: pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED}] : []
    })
  })
  override connectedCallback() {
    super.connectedCallback()
    this.onPrefs_()
    chrome.settingsPrivate.onPrefsChanged.addListener(this.onPrefs_)
  }
  override disconnectedCallback() {
    chrome.settingsPrivate.onPrefsChanged.removeListener(this.onPrefs_)
    super.disconnectedCallback()
  }
  private toggle_(event: Event) {
    const button = event.currentTarget as HTMLButtonElement
    const extra = extras.find(e => e.key === button.dataset['pref'])
    if (!extra || button.disabled) return
    const on = button.getAttribute('aria-checked') !== 'true'
    void chrome.settingsPrivate.setPref(extra.key, extra.inverted ? !on : on, '').then(success => {
      this.error_ = success ? '' : loadTimeData.getString('wrylingExtrasError')
      void this.onPrefs_()
    }).catch(() => { this.error_ = loadTimeData.getString('wrylingExtrasError') })
  }
  async searchContents(query: string) {
    return (await getSearchManager().search(query, this)).getSearchResult()
  }
}
customElements.define(SettingsWrylingExtras.is, SettingsWrylingExtras)
