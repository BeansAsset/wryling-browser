/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { scoped } from '$web-common/scoped_css'

export const style = scoped.css`
  .toggle-list {
    max-width: 100%;
    width: 100%;
    margin-left: auto;
    margin-right: auto;
    padding: var(--wry-space-4);
    display: flex;
    flex-direction: column;
    gap: 12px;

    > * {
      display: flex;
      align-items: center;
      gap: 8px;
      text-transform: capitalize;

      leo-toggle {
        margin-inline-start: auto;
      }
    }
  }
`

style.passthrough.css`
  p.header-text {
    margin-top: 16px;
    font: var(--wry-callout);

    button {
      text-decoration: underline;
    }
  }
`
