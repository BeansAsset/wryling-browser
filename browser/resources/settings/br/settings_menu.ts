/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import {RegisterPolymerTemplateModifications, RegisterStyleOverride} from 'chrome://resources/brave/polymer_overriding.js'
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js'

import {loadTimeData} from '../i18n_setup.js'
import {pageVisibility} from './page_visibility.js'
import 'chrome://resources/brave/leo.bundle.js'

function createMenuElement(
  title: string,
  href: string,
  iconName: string,
  pageVisibilitySection: keyof typeof pageVisibility) {
  const menuEl = document.createElement('a')
  if (pageVisibilitySection) {
    menuEl.hidden = !pageVisibility[pageVisibilitySection]
  }
  menuEl.href = href
  menuEl.setAttribute('role', 'menuitem')
  menuEl.setAttribute('class', 'cr-nav-menu-item')

  const icon = document.createElement('cr-icon')
  icon.setAttribute('icon', iconName)
  menuEl.appendChild(icon)

  const text = document.createTextNode(title)
  menuEl.appendChild(text)
  const crRippleChild = document.createElement('cr-ripple')
  menuEl.appendChild(crRippleChild)
  return menuEl
}

function getMenuElement(
  templateContent: HTMLTemplateElement,
  href: string) {
  let menuEl = templateContent.querySelector(`a[href="${href}"]`)
  if (!menuEl) {
    // Search templates
    const templates = templateContent.querySelectorAll('template')
    for (const template of templates) {
      menuEl = template.content.querySelector(`a[href="${href}"]`)
      if (menuEl) {
        return menuEl
      }
    }
    console.error(`[Settings] Could not find menu item '${href}'`)
  }
  return menuEl
}

RegisterStyleOverride(
  'settings-menu',
  html`
    <style>
      :host {
        display: flex !important; flex-direction: column; box-sizing: border-box; height: 100%; min-width: 0;
        padding: 0 !important; margin: 0 !important; overflow: hidden;
      }
      div[role=navigation] { flex: 1; min-height: 0; overflow: auto; scrollbar-width: thin; }
      #menu { min-width: 0; }
      .cr-nav-menu-item, a[href]:not(#about-menu) {
        box-sizing: border-box !important; display: flex; align-items: center;
        min-height: var(--wry-nav-row-height) !important; height: var(--wry-nav-row-height);
        border-radius: var(--wry-radius-m) !important; padding: 0 var(--wry-space-2) !important;
        margin: 0 0 var(--wry-segment-inset) !important; font: var(--wry-body) !important;
        color: var(--wry-text) !important; gap: var(--wry-nav-gap); text-decoration: none;
      }
      .cr-nav-menu-item[hidden] { display: none !important; }
      .cr-nav-menu-item:hover { background: var(--wry-hover) !important; }
      .cr-nav-menu-item:is([selected], .selected) { background: var(--wry-selected) !important; }
      .cr-nav-menu-item::before { display: none !important; }
      cr-icon, leo-icon {
        --iron-icon-width: var(--wry-icon); --iron-icon-height: var(--wry-icon);
        --iron-icon-fill-color: currentColor; width: var(--wry-icon); height: var(--wry-icon);
        margin: 0 !important; color: var(--wry-text-secondary);
      }
      .cr-nav-menu-item[selected] cr-icon { color: var(--wry-accent); }
      svg.wry-nav-icon { width: var(--wry-icon); height: var(--wry-icon); flex: none; fill: none; stroke: currentColor; stroke-width: 1.5; stroke-linecap: round; stroke-linejoin: round; color: var(--wry-text-secondary); }
      .cr-nav-menu-item:is([selected], .selected) svg { color: var(--wry-accent-text); }
      details { margin-top: var(--wry-nav-gap); }
      summary { display: flex; align-items: center; min-height: var(--wry-nav-row-height); padding-inline: var(--wry-space-2); font: var(--wry-body); color: var(--wry-text-secondary); cursor: default; }
      summary::before { content: "›"; width: var(--wry-icon); margin-inline-end: var(--wry-nav-gap); text-align: center; }
      details[open] > summary::before { transform: rotate(90deg); }
      details a { padding-inline-start: var(--wry-space-6) !important; }
      cr-ripple { display: none; }
      .menu-separator { border: 0 !important; box-shadow: none !important; background: transparent !important; height: var(--wry-nav-gap) !important; margin: 0 !important; }
      #settingsHeader { display: none; }
      #about-menu {
        display: flex; align-items: center; gap: var(--wry-space-2); flex: none;
        margin: var(--wry-space-2) 0 0; padding: var(--wry-space-3) var(--wry-space-2);
        color: var(--wry-text-secondary); text-decoration: none; border-radius: var(--wry-radius-m);
      }
      #about-menu:hover { background: var(--wry-hover); }
      .brave-about-item { display: block; font: var(--wry-caption); }
      .brave-about-menu-link-text { font: var(--wry-headline); color: var(--wry-text); }
      .brave-about-graphic { display: none; }
    </style>
  `
)

RegisterPolymerTemplateModifications({
  'settings-menu': (templateContent) => {
    // Hide performance menu. We moved it under system menu instead.
    const performanceEl = getMenuElement(templateContent, '/performance')
    if (performanceEl) {
      performanceEl.remove()
    }

    // Add 'Get Started' item
    const getStartedEl = createMenuElement(
      loadTimeData.getString('braveGetStartedTitle'),
      '/getStarted',
      'rocket',
      'getStarted'
    )
    const peopleEl = getMenuElement(templateContent, '/people')
    if (peopleEl) {
      peopleEl.insertAdjacentElement('afterend', getStartedEl)
    }

    // Brave Origin
    const originEl = createMenuElement(
      loadTimeData.getString('braveOriginTitle'),
      '/origin',
      'product-origin',
      'origin',
    )
    getStartedEl.insertAdjacentElement('afterend', originEl)

    // Move Appearance item
    const contentEl = createMenuElement(
      loadTimeData.getString('contentSettingsContentSection'),
      '/braveContent',
      'window-content',
      'content',
    )
    const appearanceBrowserEl = getMenuElement(templateContent, '/appearance')
    if (appearanceBrowserEl && contentEl) {
      // Insert after Origin if visible, otherwise after Get Started
      const insertAfter = originEl.hidden ? getStartedEl : originEl
      insertAfter.insertAdjacentElement('afterend', appearanceBrowserEl)
      appearanceBrowserEl.insertAdjacentElement('afterend', contentEl)
    }

    // Add Shields item
    const shieldsEl = createMenuElement(
      loadTimeData.getString('braveShieldsTitle'),
      '/shields',
      'shield-done',
      'shields',
    )
    contentEl.insertAdjacentElement('afterend', shieldsEl)

    // Add privacy item
    const privacyEl = getMenuElement(templateContent, '/privacy')
    if (privacyEl && shieldsEl) {
      shieldsEl.insertAdjacentElement('afterend', privacyEl)
    }

    // Track last inserted element to simplify conditional insertions
    let lastInserted = privacyEl!

    // Add web3 item
    // <if expr="enable_brave_wallet">
    if (true) {
      const web3El = createMenuElement(
        loadTimeData.getString('braveWeb3'),
        '/web3',
        'product-brave-wallet',
        'braveWallet',
      )
      lastInserted = lastInserted.insertAdjacentElement('afterend', web3El)!
    }
    // </if>

    // Add leo item
// <if expr="enable_ai_chat">
    const leoAssistantEl = createMenuElement(
      loadTimeData.getString('leoAssistant'),
      '/leo-ai',
      'product-brave-leo',
      'leoAssistant',
    )
    lastInserted =
      lastInserted.insertAdjacentElement('afterend', leoAssistantEl)!
// </if>

    // Add Sync item
    const syncEl = createMenuElement(
      loadTimeData.getString('braveSync'),
      '/braveSync',
      'product-sync',
      'braveSync',
    )
    lastInserted = lastInserted.insertAdjacentElement('afterend', syncEl)!

    // Add search item
    const searchEl = getMenuElement(templateContent, '/search')
    if (searchEl && syncEl) {
      syncEl.insertAdjacentElement('afterend', searchEl)
    }

    // Add Extensions item
    const extensionEl = createMenuElement(
      loadTimeData.getString('braveDefaultExtensions'),
      '/extensions',
      'browser-extensions',
      'extensions',
    )
    if (extensionEl && searchEl) {
      searchEl.insertAdjacentElement('afterend', extensionEl)
    }

    // Move autofill to advanced
    const autofillEl = getMenuElement(templateContent, '/autofill')
    const languagesEl = getMenuElement(templateContent, '/languages')
    if (autofillEl && languagesEl) {
      languagesEl.insertAdjacentElement('beforebegin', autofillEl)
    }

    // Remove extensions link
    const extensionsLinkEl = templateContent.querySelector('#extensionsLink')
    if (!extensionsLinkEl) {
      console.error('[Settings] Could not find extensionsLinkEl to remove')
      return
    }
    extensionsLinkEl.remove()
    // Add version number to 'about' link
    const aboutEl = templateContent.querySelector('#about-menu')
    if (!aboutEl) {
      console.error('[Settings] Could not find about-menu element')
      return
    }
    const parent = aboutEl.parentNode
    parent.removeChild(aboutEl)

    const newAboutEl = document.createElement('a')
    newAboutEl.setAttribute('href', '/help')
    newAboutEl.setAttribute('id', aboutEl.id)
    newAboutEl.setAttribute('role', 'menuitem')

    const graphicsEl = document.createElement('div')
    graphicsEl.setAttribute('class', 'brave-about-graphic')

    // Use per-channel logo image.
    const icon = document.createElement('img')
    icon.setAttribute('srcset', 'chrome://theme/current-channel-logo@1x, chrome://theme/current-channel-logo@2x 2x')
    icon.setAttribute('width', '20px')
    icon.setAttribute('height', '20px')

    const metaEl = document.createElement('div')
    metaEl.setAttribute('class', 'brave-about-meta')

    const menuLink = document.createElement('span')
    menuLink.setAttribute('class', 'brave-about-item brave-about-menu-link-text')
    menuLink.textContent = 'Wryling'
    newAboutEl.setAttribute('aria-label', aboutEl.textContent?.trim() || 'Wryling')

    const versionEl = document.createElement('span')
    versionEl.setAttribute('class', 'brave-about-item brave-about-menu-version')
    versionEl.textContent = `v ${loadTimeData.getString('braveProductVersion')}`

    const footer = document.createElement('cr-menu-selector')
    footer.setAttribute('selectable', 'a')
    footer.setAttribute('attr-for-selected', 'href')
    footer.setAttribute('on-iron-activate', 'onSelectorActivate_')
    footer.setAttribute('on-click', 'onLinkClick_')
    templateContent.querySelector('div[role=navigation]')?.after(footer)
    footer.appendChild(newAboutEl)
    newAboutEl.appendChild(graphicsEl)
    graphicsEl.appendChild(icon)
    newAboutEl.appendChild(metaEl)
    metaEl.appendChild(menuLink)
    metaEl.appendChild(versionEl)

    const lineIcons: Record<string, Array<{tag: string, attrs: Record<string, string>}>> = {"/getStarted": [{"tag": "circle", "attrs": {"cx": "8", "cy": "8", "r": "5.75"}}, {"tag": "path", "attrs": {"d": "M7 5.8 10.2 8 7 10.2z"}}], "/appearance": [{"tag": "path", "attrs": {"d": "M8 2.25a5.75 5.75 0 1 0 0 11.5c1 0 1.3-.7.9-1.4-.5-.9.1-1.85 1.1-1.85h1.3a2.45 2.45 0 0 0 2.45-2.45C13.75 4.8 11.2 2.25 8 2.25z"}}, {"tag": "circle", "attrs": {"cx": "5.3", "cy": "7", "r": ".6"}}, {"tag": "circle", "attrs": {"cx": "7.6", "cy": "5", "r": ".6"}}, {"tag": "circle", "attrs": {"cx": "10.4", "cy": "5.8", "r": ".6"}}], "/braveContent": [{"tag": "rect", "attrs": {"x": "2.5", "y": "3", "width": "11", "height": "10", "rx": "2"}}, {"tag": "path", "attrs": {"d": "M5 6h6M5 8.5h6M5 11h3.5"}}], "/shields": [{"tag": "path", "attrs": {"d": "M8 2.2 13 4v3.8c0 3-2.1 5.2-5 6-2.9-.8-5-3-5-6V4z"}}, {"tag": "path", "attrs": {"d": "m5.8 8 1.6 1.6L10.4 6.5"}}], "/privacy": [{"tag": "rect", "attrs": {"x": "3.5", "y": "7", "width": "9", "height": "6.5", "rx": "1.8"}}, {"tag": "path", "attrs": {"d": "M5.5 7V5.3a2.5 2.5 0 0 1 5 0V7"}}, {"tag": "circle", "attrs": {"cx": "8", "cy": "10.2", "r": ".7"}}], "/search": [{"tag": "circle", "attrs": {"cx": "7", "cy": "7", "r": "4.25"}}, {"tag": "path", "attrs": {"d": "m10.2 10.2 3.3 3.3"}}], "/extensions": [{"tag": "path", "attrs": {"d": "M3 5.5h2.6a1.6 1.6 0 1 1 3.2 0H11v2.4a1.6 1.6 0 1 1 0 3.2V13.5H3z"}}], "/autofill": [{"tag": "circle", "attrs": {"cx": "5.5", "cy": "8", "r": "2.75"}}, {"tag": "path", "attrs": {"d": "M8.2 8h5.3M11.5 8v2.2M13.5 8v1.6"}}], "/languages": [{"tag": "circle", "attrs": {"cx": "8", "cy": "8", "r": "5.75"}}, {"tag": "path", "attrs": {"d": "M2.4 8h11.2M8 2.3c1.6 1.6 2.4 3.5 2.4 5.7S9.6 12.1 8 13.7C6.4 12.1 5.6 10.2 5.6 8S6.4 3.9 8 2.3z"}}], "/downloads": [{"tag": "path", "attrs": {"d": "M8 2.5v7.5M5 7.2 8 10.2l3-3"}}, {"tag": "path", "attrs": {"d": "M3 11.5v1a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1v-1"}}], "/accessibility": [{"tag": "circle", "attrs": {"cx": "8", "cy": "8", "r": "5.75"}}, {"tag": "circle", "attrs": {"cx": "8", "cy": "5.3", "r": ".8"}}, {"tag": "path", "attrs": {"d": "M5.2 7h5.6M8 7v2.2l-1.6 2.6M8 9.2l1.6 2.6"}}], "/system": [{"tag": "circle", "attrs": {"cx": "8", "cy": "8", "r": "2.1"}}, {"tag": "path", "attrs": {"d": "M8 1.9v1.7M8 12.4v1.7M14.1 8h-1.7M3.6 8H1.9M12.3 3.7l-1.2 1.2M4.9 11.1l-1.2 1.2M12.3 12.3l-1.2-1.2M4.9 4.9 3.7 3.7"}}], "/reset": [{"tag": "path", "attrs": {"d": "M3 8a5 5 0 1 0 1.6-3.7"}}, {"tag": "path", "attrs": {"d": "M3 2.5v3h3"}}], "/braveSync": [{"tag": "circle", "attrs": {"cx": "8", "cy": "8", "r": "5.75"}}, {"tag": "path", "attrs": {"d": "M2.4 8h11.2M8 2.3c1.6 1.6 2.4 3.5 2.4 5.7S9.6 12.1 8 13.7C6.4 12.1 5.6 10.2 5.6 8S6.4 3.9 8 2.3z"}}], "/web3": [{"tag": "circle", "attrs": {"cx": "5.5", "cy": "8", "r": "2.75"}}, {"tag": "path", "attrs": {"d": "M8.2 8h5.3M11.5 8v2.2M13.5 8v1.6"}}], "/leo-ai": [{"tag": "rect", "attrs": {"x": "2.5", "y": "3", "width": "11", "height": "10", "rx": "2"}}, {"tag": "path", "attrs": {"d": "M5 6h6M5 8.5h6M5 11h3.5"}}]}
    function replaceIcons(root: DocumentFragment) {
      for (const link of root.querySelectorAll<HTMLAnchorElement>('a[href]')) {
        const shape = lineIcons[link.getAttribute('href') || '']
        if (!shape) continue
        link.querySelector('cr-icon, leo-icon')?.remove()
        const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg')
        svg.setAttribute('viewBox', '0 0 16 16')
        svg.setAttribute('class', 'wry-nav-icon')
        svg.setAttribute('aria-hidden', 'true')
        for (const part of shape) {
          const node = document.createElementNS('http://www.w3.org/2000/svg', part.tag)
          for (const [key, value] of Object.entries(part.attrs)) node.setAttribute(key, value)
          svg.append(node)
        }
        link.prepend(svg)
      }
      for (const template of root.querySelectorAll('template')) replaceIcons(template.content)
    }
    replaceIcons(templateContent)
    if (loadTimeData.getBoolean('isGuest')) return
    const extras = createMenuElement(loadTimeData.getString('wrylingExtrasTitle'), '/extras', 'settings:extension', 'content')
    extras.prepend(lineIcons['/extensions'] ? (() => {
      const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg')
      svg.setAttribute('viewBox', '0 0 16 16')
      svg.setAttribute('class', 'wry-nav-icon')
      svg.setAttribute('aria-hidden', 'true')
      for (const part of lineIcons['/extensions']) {
        const node = document.createElementNS('http://www.w3.org/2000/svg', part.tag)
        for (const [key, value] of Object.entries(part.attrs)) node.setAttribute(key, value)
        svg.append(node)
      }
      return svg
    })() : document.createTextNode(''))
    extras.querySelector('cr-icon')?.remove()
    const group = document.createElement('div')
    group.id = 'wryExtrasLinks'
    group.style.marginTop = 'var(--wry-nav-gap)'
    group.append(extras)
    for (const href of ['/web3', '/leo-ai']) {
      const item = templateContent.querySelector(`a[href="${href}"]`)
      if (item) group.append(item)
    }
    templateContent.querySelector('#menu')?.append(group)
  }
})
