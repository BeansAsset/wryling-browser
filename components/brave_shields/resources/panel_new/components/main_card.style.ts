/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { scoped } from '$web-common/scoped_css'

export const style = scoped.css`
  & {
    display: flex;
    flex-direction: column;
    gap: var(--wry-space-2);
  }
  &[data-shields-off='true'] .site-icon img {
    filter: grayscale(100%);
  }
  .site-info {
    display: flex;
    align-items: center;
    gap: var(--wry-space-2);
    min-height: var(--wry-row-height);
  }
  .site-text {
    flex: 1;
    min-width: 0;
  }
  .site-text h3 {
    font: var(--wry-headline);
    color: var(--wry-text);
  }
  .site-icon {
    flex: 0 0 var(--wry-icon);
    width: var(--wry-icon);
    height: var(--wry-icon);
  }
  .site-icon img {
    display: block;
    width: 100%;
    height: 100%;
    object-fit: contain;
  }
  .shields-status {
    font: var(--wry-callout);
    color: var(--wry-text-secondary);
  }
  .shields-status strong {
    font-weight: 400;
  }
  .block-info,
  .report-prompt {
    display: flex;
    align-items: center;
    justify-content: flex-start;
    flex-wrap: wrap;
    gap: var(--wry-space-2);
    padding: var(--wry-space-2) var(--wry-space-3);
    background: var(--wry-raised);
    border-radius: var(--wry-radius-l);
    font: var(--wry-callout);
  }
  .block-info {
    background: var(--wry-fill);
  }
  .block-info .count {
    font: var(--wry-headline);
    color: var(--wry-text);
  }
  .block-info .blocked-items {
    display: none;
  }
  .report-prompt leo-button {
    margin-inline-start: auto;
  }
`
