/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { scoped } from '$web-common/scoped_css'

export const style = scoped.css`
  .list-header {
    padding: 8px 16px;
    display: flex;
    align-items: center;
    gap: 8px;
    font: var(--wry-headline);

    button {
      margin-inline-start: auto;
      font: var(--wry-callout);
      text-decoration: underline;
      color: var(--wry-text);
    }
  }

  .allow-header {
    color: var(--wry-text);
    background: var(--wry-fill);
  }

  .block-header {
    color: var(--wry-destructive);
    background: var(--wry-fill);
  }

  .resource-list {
    padding: 8px;
  }
`
