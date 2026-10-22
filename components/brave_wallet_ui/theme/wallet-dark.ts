/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import ITheme from 'brave-ui/theme/theme-interface'
import IThemeWallet from './wallet-theme'
import defaultDarkTheme from 'brave-ui/theme/brave-dark'
import { walletFontFamily } from '../utils/font.utils'

const walletDarkTheme: ITheme & IThemeWallet = {
  ...defaultDarkTheme,
  name: 'Wallet Dark',
  fontFamily: {
    heading: walletFontFamily,
    body: walletFontFamily,
    system: walletFontFamily,
  },
  color: {
    ...defaultDarkTheme.color,
    text: 'var(--wry-text)',
    panelBackground: 'var(--wry-canvas)',
    panelBackgroundSecondary: 'var(--wry-surface)',
    outlineColor: 'var(--wry-stroke)',
  },
}

export default walletDarkTheme
