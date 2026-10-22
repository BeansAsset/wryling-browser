// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.

import styled from 'styled-components'
import Icon from '@brave/leo/react/icon'
import { Column } from '../../../../components/shared/style'

export const WelcomePageBackground = styled.div`
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  background: var(--wry-canvas);
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 0;

`

export const WelcomePageWrapper = styled(Column)`
  display: flex;
  justify-content: center;
  align-items: center;
  min-width: 100%;
  padding-block: var(--wry-space-12);
`

export const Content = styled(Column)`
  display: flex;
  align-items: center;
  justify-content: flex-start;
  z-index: 3;
  padding: 0 var(--wry-space-6);
  max-width: var(--wry-content-width);

  @media screen and (min-width: 768px) and (max-width: 1024px) {
    width: 100%;
  }
`

export const BraveIcon = styled(Icon).attrs({
  name: 'brave-icon-release-color',
})`
  --leo-icon-size: var(--wry-icon);
`

export const Title = styled.h4`
  font: var(--wry-headline);
  color: var(--wry-text);
  margin: 0;
`

export const Heading = styled.h1`
  font: var(--wry-large-title);
  color: var(--wry-text);
  margin: 0;
  padding-bottom: var(--wry-space-4);
`

export const SubHeading = styled.h3`
  color: var(--wry-text-secondary);
  font: var(--wry-body);
  margin: 0;
  padding: 0 0 var(--wry-space-6);
`

export const ActionsContainer = styled.div`
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  width: 100%;
  gap: var(--wry-space-2);

  /* media query for desktop */
  @media (min-width: 1025px) {
    flex-direction: row;
  }
`

export const Footer = styled.footer`
  color: var(--wry-text-secondary);
  font: var(--wry-callout);
  margin: 0;
  padding-bottom: var(--wry-space-6);
`
