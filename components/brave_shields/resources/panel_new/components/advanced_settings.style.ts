/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { scoped } from '$web-common/scoped_css'

export const style = scoped.css`
  & {
    --leo-icon-size: var(--wry-icon);
    display: flex;
    flex-direction: column;
    background: var(--wry-raised);
    border-radius: var(--wry-radius-l);
    padding: var(--wry-space-2);
  }
  & > * {
    display: flex;
    align-items: center;
    gap: var(--wry-space-2);
    padding: var(--wry-space-1) 0;
  }
  & > * + * {
    border-top: 1px solid var(--wry-separator);
  }
  & > * > leo-icon {
    color: var(--wry-text-secondary);
    flex: none;
  }
  & > * > :last-child {
    min-width: 0;
  }
  & > * > leo-button {
    flex: none;
    border-radius: var(--wry-radius-m);
  }
  & > * > leo-button:hover {
    background: var(--wry-hover);
  }
  & > * > leo-button[isdisabled='true'] {
    visibility: hidden;
  }
  .toggle {
    flex: 1;
    min-width: 0;
    display: flex;
    align-items: center;
    gap: var(--wry-space-2);
    padding: var(--wry-space-1) 0;
    font: var(--wry-callout);
  }
  .toggle > :last-child {
    margin-inline-start: auto;
  }
  leo-dropdown {
    flex: 1;
    min-width: 0;
  }
  leo-dropdown [slot='value'] {
    display: flex;
    align-items: center;
    width: 100%;
    gap: var(--wry-space-2);
    font: var(--wry-callout);
  }
  .counter {
    border-radius: var(--wry-radius-s);
    padding: 0 var(--wry-space-1);
    background: var(--wry-fill);
    color: var(--wry-text-secondary);
    font: var(--wry-caption);
  }
  .block-scripts-text > * {
    display: flex;
    align-items: center;
    gap: var(--wry-space-2);
  }
  .block-scripts-text .note {
    font: var(--wry-caption);
    color: var(--wry-text-secondary);
  }
  .block-elements {
    flex: 1;
    padding: var(--wry-space-1) 0;
  }
  .block-elements button {
    color: var(--wry-accent-text);
    font: var(--wry-callout);
    text-decoration: underline;
  }
`
