/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import {
  RegisterPolymerTemplateModifications,
  RegisterStyleOverride
} from 'chrome://resources/brave/polymer_overriding.js'

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js'
import {CrChipElement} from 'chrome://resources/cr_elements/cr_chip/cr_chip.js'
import {css} from 'chrome://resources/lit/v3_0/lit.rollup.js'
import {injectStyle} from 'chrome://resources/brave/lit_overriding.js'

injectStyle(CrChipElement, css`
  :host-context(settings-clear-browsing-data-time-picker) button[selected] {
    background: var(--wry-accent) !important;
    color: var(--wry-on-accent) !important;
  }
`)

RegisterStyleOverride(
  'settings-clear-browsing-data-time-picker',
  html`
    <style>
      :host {
        --wry-button-padding: var(--wry-space-2);
        /* Dropdown menu styling */
        --cr-menu-border-radius: 8px;

        --color-chip-background-selected: var(--wry-accent);
        --color-chip-foreground-selected: var(--wry-on-accent);
        --color-chip-foreground: var(--wry-text);
        --color-chip-icon: var(--wry-text-secondary);
        --color-chip-icon-selected: var(--wry-on-accent);
        --color-chip-border: var(--wry-stroke);
      }

      cr-chip {
        --cr-chip-border-radius: var(--wry-radius-l);
        --cr-chip-height: var(--wry-control-height);
        --cr-chip-font-size: inherit;
        --cr-chip-padding-inline: var(--leo-spacing-l);
        font: var(--wry-body);
      }
    </style>
  `
)

RegisterPolymerTemplateModifications({
  'settings-clear-browsing-data-time-picker': (templateContent: DocumentFragment) => {
    // Find all time period chip templates and reorder their children
    // to show label first, then icon (reversed from Chromium's order)
    const chipTemplate = templateContent.querySelector('template[is="dom-repeat"]')
    if (chipTemplate) {
      const chipContent = (chipTemplate as HTMLTemplateElement).content
      const chip = chipContent.querySelector('.time-period-chip')
      if (chip) {
        // Find the cr-icon and text nodes
        const icon = chip.querySelector('cr-icon')
        const labelNode = Array.from(chip.childNodes).find(
          (node: Node) => node.nodeType === Node.TEXT_NODE && node.textContent?.trim()
        )

        if (icon && labelNode) {
          // Change the icon to check-circle-outline
          icon.setAttribute('icon', 'check-circle-outline')

          // Remove both nodes
          chip.removeChild(icon)
          chip.removeChild(labelNode)

          // Re-add in reversed order: label first, then icon
          chip.appendChild(labelNode)
          chip.appendChild(icon)
        }
      }
    }

    // Change the "More" button's icon to carat-down
    const moreButton = templateContent.querySelector('#moreButton')
    if (moreButton) {
      const moreButtonIcon = moreButton.querySelector('cr-icon')
      if (moreButtonIcon) {
        moreButtonIcon.setAttribute('icon', 'carat-down')
      }
    }
  }
})
