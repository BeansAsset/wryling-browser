/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { scoped } from '$web-common/scoped_css'

export const style = scoped.css`
  & {
    background: transparent;
    padding: var(--wry-space-4);
  }
`

style.passthrough.css`
  & {
    font: var(--wry-body);
    color: var(--wry-text);
    color-scheme: light dark;
    --leo-control-radius: var(--wry-radius-m);
    --leo-control-font: var(--wry-callout);
    --leo-control-padding: var(--wry-space-1) var(--wry-space-2);
    --leo-control-border-color: transparent;
    --leo-control-text-color: var(--wry-text);
    --leo-color-container-highlight: var(--wry-fill);
    --leo-control-focus-effect: none;
    --leo-effect-elevation-01: none;
  }
  *,
  *::before,
  *::after {
    box-sizing: border-box;
  }
  p,
  h1,
  h2,
  h3,
  h4 {
    margin: 0;
  }
  h1,
  h2,
  h3,
  h4 {
    font: var(--wry-headline);
  }
  a {
    color: var(--wry-accent-text);
  }
  button {
    margin: 0;
    border: 0;
    text-align: start;
    font: inherit;
    cursor: pointer;
  }
  button:disabled {
    cursor: default;
  }
  button:focus-visible {
    outline: 3px solid color-mix(in srgb, var(--wry-accent-text) 50%, transparent);
    outline-offset: 1px;
  }
  leo-toggle {
    --wry-toggle-width: 38px;
    --wry-toggle-height: 22px;
    --leo-toggle-transition-duration: 0;
  }
  leo-toggle:hover {
    --leo-toggle-transition-duration: 0.12s;
  }
  main {
    display: flex;
    flex-direction: column;
    gap: var(--wry-space-2);
    max-height: calc(var(--browser-window-height, 9999px) - 144px);
  }
  main > .scrollable {
    overflow-y: auto;
    flex: 1 1 auto;
  }
  .overflow-ellipsis-start {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    unicode-bidi: isolate;
    &:dir(ltr) {
      direction: rtl;
      text-align: end;
    }
  }
`
