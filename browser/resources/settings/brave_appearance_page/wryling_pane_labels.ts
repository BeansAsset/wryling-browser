// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

// Wryling: how split view panes show their site. A picker of four drawn
// previews for the "wryling.split_view.pane_labels" pref (0 on hover,
// 1 always, 2 corner chip, 3 off; BraveMultiContentsView::PaneLabels).

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js'
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js'

import '../settings_shared.css.js'
import '../settings_vars.css.js'
import {getTemplate} from './wryling_pane_labels.html.js'

const PANE_LABELS_PREF = 'wryling.split_view.pane_labels'

const SettingsWrylingPaneLabelsElementBase = I18nMixin(PolymerElement)

class SettingsWrylingPaneLabelsElement extends
    SettingsWrylingPaneLabelsElementBase {
  static get is() { return 'settings-wryling-pane-labels' }
  static get template() { return getTemplate() }

  static get properties() {
    return {
      mode_: {type: Number, value: 0},
      ready_: {type: Boolean, value: false},
    }
  }

  private declare mode_: number
  private declare ready_: boolean

  private onPrefsChanged_ = (prefs: chrome.settingsPrivate.PrefObject[]) => {
    const pref = prefs.find(p => p.key === PANE_LABELS_PREF)
    if (pref && typeof pref.value === 'number') {
      this.mode_ = pref.value
    }
  }

  override connectedCallback() {
    super.connectedCallback()
    chrome.settingsPrivate.onPrefsChanged.addListener(this.onPrefsChanged_)
    void chrome.settingsPrivate.getPref(PANE_LABELS_PREF).then(pref => {
      if (typeof pref.value === 'number') {
        this.mode_ = pref.value
      }
      this.ready_ = true
    })
  }

  override disconnectedCallback() {
    chrome.settingsPrivate.onPrefsChanged.removeListener(this.onPrefsChanged_)
    super.disconnectedCallback()
  }

  private isSelected_(mode: number, selected: number): string {
    return String(mode === selected)
  }

  private onOptionClick_(event: Event) {
    const mode = Number((event.currentTarget as HTMLButtonElement).dataset['mode'])
    if (Number.isNaN(mode) || mode === this.mode_) {
      return
    }
    this.mode_ = mode
    void chrome.settingsPrivate.setPref(PANE_LABELS_PREF, mode, '')
  }
}

customElements.define(SettingsWrylingPaneLabelsElement.is,
                      SettingsWrylingPaneLabelsElement)
