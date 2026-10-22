/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { color, font, radius, spacing } from '@brave/leo/tokens/css/variables'
import { scoped } from '$web-common/scoped_css'

export const style = scoped.css`
  & {
    --leo-dialog-width: 860px;
    --leo-dialog-padding: 0;
    --leo-dialog-background: ${color.container.background};

    height: 0;
  }

  .panel-body {
    display: flex;
    max-height: calc(100vh - ${spacing['4Xl']});
  }

  nav {
    flex: 0 0 228px;
    display: flex;
    flex-direction: column;
    gap: ${spacing['2Xl']};
    padding: ${spacing['2Xl']} 0;
    white-space: nowrap;
  }

  h4 {
    color: ${color.text.primary};
    padding: 0 ${spacing['2Xl']};
  }

  /* Wryling source list: rows 32 high, inset 8
     from the edges, radius-m, a 16pt icon and body text; the selected row
     is the accent-tinted "selected" shape, never an edge bar. The shared
     component sheet styles every button, hence the !important. */
  .nav-list {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: 0 8px;
  }

  .nav-row {
    --leo-icon-size: 16px;

    display: flex !important;
    align-items: center;
    gap: 10px;
    width: 100%;
    height: 32px;
    min-height: 32px !important;
    padding: 0 10px !important;
    border: 0 !important;
    border-radius: var(--wry-radius-m, 8px) !important;
    background: transparent !important;
    color: var(--wry-text, ${color.text.primary}) !important;
    font: var(--wry-body, 13px/18px system-ui) !important;
    text-align: start;
    cursor: pointer;

    leo-icon {
      color: var(--wry-text-secondary, ${color.icon.default});
    }

    &:hover {
      background: var(--wry-hover, rgba(127, 127, 127, .12)) !important;
    }

    &.selected {
      background: var(--wry-selected,
          color-mix(in srgb, var(--wry-accent, #6f52dc) 22%, transparent))
          !important;

      leo-icon {
        color: var(--wry-accent-text, ${color.icon.interactive});
      }
    }

    &:focus-visible {
      outline: 3px solid var(--wry-focus, rgba(111, 82, 220, .5)) !important;
      outline-offset: 1px;
    }
  }

  section {
    flex: 1 1 auto;
    min-height: 0;
    padding: ${spacing.m} ${spacing.m} ${spacing.m} 0;
    overflow: auto;
    overscroll-behavior: contain;
    scrollbar-width: thin;
    display: flex;
    flex-direction: column;

    .panel-body[data-resizing] & {
      overflow: hidden;
    }

    > * {
      flex-grow: 1;
      padding: ${spacing['4Xl']};
      border-radius: ${radius.xl};
      background: ${color.page.background};
    }
  }

  .label .subtext {
    font: ${font.small.regular};
    color: ${color.text.secondary};

    a {
      color: inherit;
    }
  }
`
