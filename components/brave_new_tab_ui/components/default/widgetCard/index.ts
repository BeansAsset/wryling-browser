/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import styled from 'styled-components'

export const StyledTitleTab = styled.div`
  color: var(--wry-text);
  font: var(--wry-headline);
  cursor: pointer;
  padding: var(--wry-space-3) var(--wry-space-4) var(--wry-space-4);
  margin-bottom: calc(-1 * var(--wry-space-4));
  border-radius: var(--wry-radius-xl) var(--wry-radius-xl) 0 0;
  background: var(--wry-surface);
  display: flex;
  align-items: center;
  gap: var(--wry-space-2);
`

export const StyledCard = styled.div`
  background: var(--wry-surface);
  color: var(--wry-text);
  font: var(--wry-body);
  border-radius: var(--wry-radius-xl);
  padding: var(--wry-space-4);
`
