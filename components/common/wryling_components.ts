// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import componentStyles from '!!css-loader?{"url":false,"import":false}!./wryling_components.css'

// A stylesheet on the document cannot style Lit, Polymer or Leo internals.
// Adopt the same sheet in their roots, including lazily upgraded controls.
export async function installWrylingComponents() {
  if (!['chrome:', 'chrome-untrusted:'].includes(location.protocol) ||
      document.documentElement.dataset.wryComponents) {
    return
  }
  document.documentElement.dataset.wryComponents = 'loading'
  document.documentElement.dataset.wryPage = location.hostname
  if (location.protocol === 'chrome:' &&
      !/iPhone|iPad|iPod|Android/i.test(navigator.userAgent)) {
    // The native updater refreshes direct links, not Nala's CSS @import.
    if (!document.querySelector('link[href*="//theme/colors.css"]')) {
      const link = document.createElement('link')
      link.rel = 'stylesheet'
      link.href = 'chrome://theme/colors.css?sets=ui'
      document.head.appendChild(link)
    }
    const themeUpdaterUrl = 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js'
    void import(/* webpackIgnore: true */ themeUpdaterUrl)
      .then(({ColorChangeUpdater}) => ColorChangeUpdater.forDocument().start())
      .catch(error => console.error('Wryling theme updates could not start', error))
  }
  const setKeyboardFocus = (keyboard: boolean) => {
    document.documentElement.dataset.wryKeyboard = String(keyboard)
  }
  setKeyboardFocus(false)
  document.addEventListener('pointerdown', () => setKeyboardFocus(false), {capture: true, passive: true})
  document.addEventListener('pointermove', event => {
    if (event.movementX || event.movementY) setKeyboardFocus(false)
  }, {capture: true, passive: true})
  document.addEventListener('keydown', event => {
    if (!['Meta', 'Alt', 'Control', 'Shift'].includes(event.key)) {
      setKeyboardFocus(true)
    }
  }, {capture: true})
  try {
    const sheet = new CSSStyleSheet()
    sheet.replaceSync(componentStyles.toString())
    const roots = new WeakSet<Document | ShadowRoot>()
    const awaitingUpgrade = new WeakSet<Element>()

    function syncSearchDisclosure(element: Element) {
      if (location.hostname !== 'settings' || !(element instanceof HTMLDetailsElement)) return
      const searching = !!new URLSearchParams(location.search).get('search')
      if (searching) {
        element.dataset.wrySearchWasOpen ??= String(element.open)
        element.open = true
      } else if (element.dataset.wrySearchWasOpen !== undefined) {
        element.open = element.dataset.wrySearchWasOpen === 'true'
        delete element.dataset.wrySearchWasOpen
      }
    }

    function syncSearchDisclosures(root: Document | ShadowRoot) {
      for (const element of root.querySelectorAll('*')) {
        syncSearchDisclosure(element)
        if (element.shadowRoot) syncSearchDisclosures(element.shadowRoot)
      }
    }
    document.addEventListener('wryling-settings-search-changed', () => syncSearchDisclosures(document))

    function visit(element: Element) {
      syncSearchDisclosure(element)
      if (element.localName === 'cr-toolbar' &&
          ['history', 'downloads', 'bookmarks', 'extensions'].includes(location.hostname)) {
        element.setAttribute('narrow-threshold', '700')
      }
      if (element.shadowRoot) {
        adopt(element.shadowRoot)
      } else if (element.namespaceURI === 'http://www.w3.org/1999/xhtml' &&
                 element.localName.includes('-') &&
                 !customElements.get(element.localName) &&
                 !awaitingUpgrade.has(element)) {
        awaitingUpgrade.add(element)
        customElements.whenDefined(element.localName).then(() => visit(element))
      }
      for (const child of element.children) {
        visit(child)
      }
    }

    function syncCategoryRows(root: Document | ShadowRoot) {
      if (root instanceof ShadowRoot && root.host.localName === 'category-reference-card') {
        // A sibling :has() at a shadow boundary asserts in this Chromium build.
        root.host.toggleAttribute('data-wry-has-chips',
          root.querySelector('.chips-container cr-button') !== null)
      }
    }

    function adopt(root: Document | ShadowRoot) {
      syncCategoryRows(root)
      if (roots.has(root)) {
        return
      }
      roots.add(root)
      root.adoptedStyleSheets = [...root.adoptedStyleSheets, sheet]
      for (const child of root.children) {
        visit(child)
      }
      new MutationObserver(records => {
        for (const record of records) {
          for (const node of record.addedNodes) {
            if (node instanceof Element) {
              visit(node)
            }
          }
        }
        syncCategoryRows(root)
      }).observe(root, {childList: true, subtree: true})
      root.addEventListener('scroll', event => {
        const target = event.target
        if (target instanceof HTMLElement && target.id === 'main') {
          const header = target.shadowRoot?.querySelector<HTMLElement>('.wry-page-header')
          if (header) {
            header.dataset.compact = String(target.scrollTop > 16)
          }
        }
      }, {capture: true, passive: true})
    }

    adopt(document)
    document.documentElement.dataset.wryComponents = 'ready'
    document.dispatchEvent(new Event('wryling-components-ready'))
  } catch (error) {
    delete document.documentElement.dataset.wryComponents
    console.error('Wryling components could not load', error)
  }
}
