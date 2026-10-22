// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import {browserProxyFactory, ColorSchemeMode} from 'chrome://resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom-webui.js'
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js'
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js'
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js'
import {sendWithPromise} from 'chrome://resources/js/cr.js'
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js'

import '../settings_shared.css.js'
import '../settings_vars.css.js'
import {getTemplate} from './wryling_space.html.js'

type SpaceAppearance = {color: number, opacity: number, clearGlass: boolean}

const SettingsWrylingSpaceElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement))

class SettingsWrylingSpaceElement extends SettingsWrylingSpaceElementBase {
  static get is() { return 'settings-wryling-space' }
  static get template() { return getTemplate() }

  static get properties() {
    return {
      color_: {type: String, value: '#8f7dc9'},
      mode_: {type: Number, value: 0},
      opacity_: {type: Number, value: 50},
      ready_: {type: Boolean, value: false},
      clearGlass_: {type: Boolean, value: false},
      swatches_: {
        type: Array,
        value: () => [
          {color: '#8f7dc9', label: 'wrylingSpaceViolet'},
          {color: '#568bd0', label: 'wrylingSpaceBlue'},
          {color: '#459d96', label: 'wrylingSpaceTeal'},
          {color: '#7d9c63', label: 'wrylingSpaceGreen'},
          {color: '#d68c59', label: 'wrylingSpaceAmber'},
          {color: '#c96f91', label: 'wrylingSpaceRose'},
        ],
      },
    }
  }

  private declare mode_: ColorSchemeMode
  private modeListener_: number|null = null
  private declare color_: string
  private declare opacity_: number
  private declare ready_: boolean
  private declare clearGlass_: boolean
  private declare swatches_: Array<{color: string, label: string}>

  override connectedCallback() {
    super.connectedCallback()
    ColorChangeUpdater.forDocument().start()
    const proxy = browserProxyFactory.getInstance()
    this.modeListener_ = proxy.callbackRouter.setColorSchemeMode.addListener(
        mode => { this.mode_ = mode })
    proxy.handler.initializeColorSchemeMode()
    this.addWebUiListener('wryling-appearance-changed',
        (state: SpaceAppearance) => this.updateState_(state))
    sendWithPromise<SpaceAppearance>('getWrylingAppearance').then(
        state => this.updateState_(state))
  }

  override disconnectedCallback() {
    if (this.modeListener_ !== null) {
      browserProxyFactory.getInstance().callbackRouter.removeListener(this.modeListener_)
      this.modeListener_ = null
    }
    super.disconnectedCallback()
  }

  private modeSelected_(mode: number, selected: number): string {
    return String(mode === selected)
  }

  private onModeClick_(event: Event) {
    const mode = Number((event.currentTarget as HTMLButtonElement).dataset['mode'])
    browserProxyFactory.getInstance().handler.setColorSchemeMode(mode)
  }

  private updateState_(state: SpaceAppearance) {
    this.color_ = '#' + state.color.toString(16).padStart(6, '0')
    this.opacity_ = state.opacity
    this.clearGlass_ = state.clearGlass
    this.ready_ = true
  }

  private swatchStyle_(color: string): string {
    return `background-color: ${color}`
  }

  private isSelected_(color: string, selected: string): string {
    return String(color === selected)
  }

  private onSwatchClick_(event: Event) {
    const color = (event.currentTarget as HTMLButtonElement).dataset['color']
    if (color) {
      this.setColor_(color)
    }
  }

  private onCustomColorInput_(event: Event) {
    this.setColor_((event.target as HTMLInputElement).value)
  }

  private setColor_(color: string) {
    if (!/^#[0-9a-f]{6}$/i.test(color)) {
      return
    }
    this.color_ = color.toLowerCase()
    chrome.send('setWrylingSpaceColor', [parseInt(color.slice(1), 16)])
  }

  private switchState_(enabled: boolean): string {
    return String(enabled)
  }

  private onClearGlassClick_() {
    this.clearGlass_ = !this.clearGlass_
    chrome.send('setWrylingClearGlass', [this.clearGlass_])
  }

  private onOpacityInput_(event: Event) {
    this.opacity_ = Number((event.target as HTMLInputElement).value)
    chrome.send('setWrylingTintOpacity', [this.opacity_])
  }
}

customElements.define(SettingsWrylingSpaceElement.is, SettingsWrylingSpaceElement)
