/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { scoped } from '$web-common/scoped_css'

export const style = scoped.css`
  .view-toggle {
    display: flex;
    align-items: center;
    gap: var(--wry-space-2);
    width: 100%;
    min-height: 30px;
    padding: 0 var(--wry-space-2);
    background: transparent;
    border-radius: var(--wry-radius-m);
    color: var(--wry-text);
    font: var(--wry-body);
  }
  .view-toggle span {
    flex: 1;
  }
  .view-toggle:hover {
    background: var(--wry-hover);
  }
  .view-toggle svg {
    width: var(--wry-icon);
    height: var(--wry-icon);
    color: var(--wry-text-secondary);
    transition: transform var(--wry-motion);
  }
  &[data-expanded='true'] .view-toggle svg {
    transform: rotate(180deg);
  }
`
