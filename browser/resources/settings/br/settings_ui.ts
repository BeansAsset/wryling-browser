/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import {
  html as braveHtml,
  RegisterPolymerPrototypeModification,
  RegisterPolymerTemplateModifications,
  RegisterStyleOverride
} from 'chrome://resources/brave/polymer_overriding.js'

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js'

import {Router} from '../router.js'
import {installExtrasVisibility, guardExtraRoute} from '../wryling_extras.js'
import type {Route} from '../router.js'
import {loadTimeData} from '../i18n_setup.js'

import {SettingsUiElement} from '../settings_ui/settings_ui.js'
import {setGlobalScrollTarget, resetGlobalScrollTargetForTesting} from '../global_scroll_target_mixin.js';
import * as BraveOriginMojom from '../brave_origin_settings.mojom-webui.js';

// Expose BraveOriginHandler to window object for devtools console access
function initializeBraveOriginHandler() {
  const handler = BraveOriginMojom.BraveOriginSettingsHandler.getRemote();
  (window as unknown as {
    BraveOriginHandler: BraveOriginMojom.BraveOriginSettingsHandlerRemote
  }).BraveOriginHandler = handler;
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initializeBraveOriginHandler);
} else {
  initializeBraveOriginHandler();
}

RegisterStyleOverride(
  'settings-ui',
  html`
    <style>
      :host {
        display: flex; height: 100%; background: var(--wry-canvas);
        --settings-menu-width: var(--wry-source-width) !important;
        --brave-settings-menu-width: var(--wry-source-width);
        --brave-settings-content-max-width: var(--wry-content-width);
        --brave-settings-menu-margin: 0;
      }
      #container { display: flex; overflow: hidden; scrollbar-gutter: auto; }
      #wrySource {
        box-sizing: border-box; width: var(--wry-source-width); flex: 0 0 var(--wry-source-width);
        background: color-mix(in srgb, var(--wry-surface) 55%, var(--wry-canvas));
        display: flex; flex-direction: column; padding: var(--wry-space-3) var(--wry-space-2) 0;
      }
      #toolbar { flex: none; min-height: var(--wry-control-height); margin-bottom: var(--wry-space-2); }
      #left { flex: 1 !important; min-height: 0; height: auto; position: static; }
      #left settings-menu { width: 100% !important; height: 100%; max-height: 100%; overflow: hidden; }
      #main { flex: 1; min-width: 0; margin: 0; border-radius: 0; background: var(--wry-canvas); overflow: auto; }
      #right, .cr-scrollable-top-shadow { display: none !important; }
      cr-drawer settings-menu { height: 100%; }
      @media (max-width: 700px) {
        #container { flex-direction: column; }
        #wrySource { flex: none; width: 100%; }
        #main { min-height: 0; }
      }
    </style>
  `
)

RegisterPolymerTemplateModifications({
  'settings-ui': (templateContent) => {
    const container = templateContent.querySelector('#container')
    const toolbar = templateContent.querySelector('#toolbar')
    const left = templateContent.querySelector('#left')
    if (container && toolbar && left) {
      const source = document.createElement('aside')
      source.id = 'wrySource'
      toolbar.setAttribute('narrow-threshold', '700')
      toolbar.setAttribute('search-prompt', 'Search')
      container.insertBefore(source, left)
      source.append(toolbar, left)
    }
    templateContent.prepend(braveHtml`
      <leo-alertcenter style="position: fixed;
                              z-index: 1;">
      </leo-alertcenter>
    `)
  }
})

RegisterPolymerPrototypeModification({
  'settings-ui': (prototype: SettingsUiElement) => {
    const ready = prototype.ready
    const routeChanged = prototype.currentRouteChanged
    const updateTitle = (ui: SettingsUiElement, route: Route) => queueMicrotask(() => {
      const header = ui.$.main.shadowRoot?.querySelector('h1[data-wry-title]')
      const menu = ui.$.leftMenu.shadowRoot
      if (!header || !menu) {
        return
      }
      let current: Route | null = route
      let title = ''
      while (current && !title) {
        const link = Array.from(menu.querySelectorAll<HTMLAnchorElement>('a[href]'))
            .find(item => item.getAttribute('href') === current!.path)
        title = link?.getAttribute('aria-label') || link?.textContent?.trim() || ''
        current = current.parent
      }
      header.textContent = title || loadTimeData.getString('settings')
      document.dispatchEvent(new Event('wryling-settings-search-changed'))
    })
    prototype.ready = function(this: SettingsUiElement) {
      ready.call(this)
      installExtrasVisibility(this)
      resetGlobalScrollTargetForTesting()
      setGlobalScrollTarget(this.$.main)
      updateTitle(this, Router.getInstance().getCurrentRoute())
    }
    prototype.currentRouteChanged = function(this: SettingsUiElement, route: Route) {
      if (guardExtraRoute(route)) return
      routeChanged.call(this, route)
      updateTitle(this, route)
    }
  },
})
