// Copyright (c) 2021 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.
import styled, { css } from 'styled-components'
import { CaratStrongDownIcon } from 'brave-ui/components/icons'

export const Box = styled.div`
  background: transparent;
  width: 100%;
  padding: var(--wry-space-4);
  font: var(--wry-body);
  color: var(--wry-text);
  a {
    color: var(--wry-accent-text);
  }
`

export const HeaderBox = styled.section`
  display: flex;
  flex-direction: column;
  gap: var(--wry-space-2);
  margin-bottom: var(--wry-space-2);
`

export const SiteTitleBox = styled.section`
  display: flex;
  align-items: center;
  gap: var(--wry-space-2);
  min-width: 0;
`

export const FavIconBox = styled.i`
  width: var(--wry-icon);
  height: var(--wry-icon);
  flex: none;
  img {
    width: 100%;
    height: 100%;
    object-fit: contain;
  }
`

export const SiteTitle = styled.h1`
  margin: 0;
  font: var(--wry-headline);
  color: var(--wry-text);
  overflow: hidden;
  unicode-bidi: isolate;
  direction: rtl;
  text-overflow: ellipsis;
  white-space: nowrap;
`

export const CountBox = styled.section`
  display: flex;
  align-items: center;
  gap: var(--wry-space-2);
  padding: var(--wry-space-2) var(--wry-space-3);
  border-radius: var(--wry-radius-l);
  background: var(--wry-fill);
`

export const BlockCount = styled.span`
  order: -1;
  font: var(--wry-headline);
  color: var(--wry-text);
`

export const BlockNote = styled.span`
  a { white-space: nowrap; }
  font: var(--wry-callout);
  color: var(--wry-text-secondary);
`

export const StatusBox = styled.section`
  padding: var(--wry-space-3);
  margin-bottom: var(--wry-space-2);
  background: var(--wry-raised);
  border-radius: var(--wry-radius-l);
`

export const ControlBox = styled.div`
  display: flex;
  align-items: center;
  gap: var(--wry-space-2);
`

export const StatusText = styled.div`
  flex: 1;
  min-width: 0;
  font: var(--wry-body);
  span {
    font-weight: 600;
  }
`

export const StatusToggle = styled.div`
  flex: none;
`

export const StatusFootnoteBox = styled.div`
  margin-top: var(--wry-space-2);
  font: var(--wry-callout);
  color: var(--wry-text-secondary);
`

export const Footnote = styled.div`
  font: var(--wry-callout);
  color: var(--wry-text-secondary);
  span {
    display: block;
  }
`

export const ReportSiteBox = styled.div`
  font: var(--wry-callout);
  color: var(--wry-text-secondary);
`

export const ReportSiteAction = styled.div`
  display: flex;
  flex-direction: column;
  gap: var(--wry-space-2);
  align-items: start;
`

export const PanelContent = styled.section`
  display: flex;
  flex-direction: column;
  gap: var(--wry-space-2);
`

export const AdvancedControlsButton = styled.button`
  display: flex;
  width: 100%;
  align-items: center;
  gap: var(--wry-space-2);
  min-height: 30px;
  padding: 0 var(--wry-space-2);
  background: transparent;
  color: var(--wry-text);
  border: 0;
  border-radius: var(--wry-radius-m);
  font: var(--wry-body);
  text-align: start;
  cursor: pointer;
  i {
    display: none;
  }
  span {
    flex: 1;
  }
  &:hover {
    background: var(--wry-hover);
  }
  &:focus-visible {
    outline: 3px solid var(--wry-focus);
    outline-offset: 1px;
  }
`

export const ManagedText = styled.div`
  font: var(--wry-callout);
  color: var(--wry-text-secondary);
`

export const ShieldsIcon = styled.i<{ isActive: boolean }>`
  width: var(--wry-icon);
  flex: none;
  color: var(--wry-text-secondary);
  svg {
    width: var(--wry-icon);
    height: var(--wry-icon);
  }
  ${(p) =>
    p.isActive
    && css`
      color: var(--wry-accent-text);
    `}
`
export const GlobalDefaultsButton = styled(AdvancedControlsButton)``
export const CaratIcon = styled(CaratStrongDownIcon)<{ isExpanded: boolean }>`
  width: var(--wry-icon);
  height: var(--wry-icon);
  color: var(--wry-text-secondary);
  transform: ${(p) => (p.isExpanded ? 'rotate(180deg)' : 'none')};
`
