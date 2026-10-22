// Copyright (c) 2023 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import * as React from 'react'
import styled from 'styled-components'

const Kbd = styled.kbd<{ $large?: boolean }>`
  display: inline-flex;
  align-items: center;
  justify-content: center;
  box-sizing: border-box;
  min-width: var(--wry-small-control-height);
  height: ${p => p.$large ? 'var(--wry-small-control-height)' : 'var(--wry-space-5)'};
  padding: 0 var(--wry-space-2);
  border: 0;
  border-radius: var(--wry-radius-s);
  background: var(--wry-fill);
  color: var(--wry-text);
  font: var(--wry-callout);
  white-space: nowrap;
`

const keySymbols: Record<string, string> = {
  Shift: '⇧', Meta: '⌘', Command: '⌘', Alt: '⌥', AltGr: '⌥',
  Option: '⌥', Control: '⌃', Ctrl: '⌃', ' ': 'Space',
}

export default function Keys({
  keys,
  large
}: {
  keys: string[]
  large?: boolean
}) {
  return <Kbd $large={large} aria-label={keys.join(' + ')}>
    {keys.map(k => keySymbols[k] ?? k).join('')}
  </Kbd>
}
