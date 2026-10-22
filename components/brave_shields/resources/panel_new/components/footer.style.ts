/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { scoped } from '$web-common/scoped_css'

export const style = scoped.css`
  & {
    border-top: 1px solid var(--wry-separator);
  }
  .actions {
    padding: var(--wry-space-2) 0;
    display: flex;
    flex-wrap: wrap;
    gap: var(--wry-space-2);
  }
  .actions > * {
    flex: 1 1 auto;
    min-width: 0;
  }
  .try-off {
    padding-top: var(--wry-space-2);
    color: var(--wry-text-secondary);
    font: var(--wry-caption);
    text-align: start;
  }
  .try-off p + p {
    margin-top: var(--wry-space-1);
  }
  .try-off a {
    white-space: nowrap;
    color: var(--wry-accent-text);
  }
`
