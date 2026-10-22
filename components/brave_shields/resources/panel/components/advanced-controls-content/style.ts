// Copyright (c) 2021 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.
import styled from 'styled-components'

export const SettingsBox = styled.div`
  padding: var(--wry-space-2);
  background: var(--wry-raised);
  border-radius: var(--wry-radius-l);
`

export const FooterActionBox = styled.div`
  padding: var(--wry-space-2) 0;
  border-top: 1px solid var(--wry-separator);
  display: grid;
  grid-template-columns: 1fr;
  grid-gap: 10px;

  div {
    grid-column: 1;
    display: flex;
    flex-direction: column;
    grid-gap: var(--wry-space-2);
  }

  button {
    --svg-color: var(--wry-accent-text);
    --text-color: var(--wry-accent-text);
    background-color: transparent;
    padding: 0;
    margin: 0;
    border: 0;
    color: var(--text-color);
    font-size: 13px;
    font-weight: 500;
    text-decoration: none;
    display: flex;
    align-items: center;
    cursor: pointer;

    svg > path {
      fill: var(--svg-color);
    }

    &:hover {
      --text-color: var(--wry-accent-text);
      --svg-color: var(--wry-accent-text);
    }
  }

  i {
    display: block;
    width: 17px;
    height: 17px;
    margin: 0 6px;
  }
`

export const SettingsDesc = styled.section`
  display: grid;
  grid-template-columns: 1fr;
  grid-gap: 10px;
  padding: var(--wry-space-2) 0 0;

  p {
    grid-column: 1;
    margin: 0;
    font-size: 12px;
    color: var(--wry-text-secondary);
  }
`

export const ControlGroup = styled.div`
  display: grid;
  grid-gap: 15px;
  grid-template-columns: 1fr 28px;
  grid-gap: 10px;
  align-items: center;
  margin-bottom: 8px;

  .col-2,
  label {
    grid-column: 1;
    width: 100%;
  }

  button {
    grid-column: 2;
  }

  label {
    display: flex;
    align-items: center;
    justify-content: space-between;
    font-family: var(--wry-font);
    font-size: 12px;
    color: var(--wry-text);
    text-indent: 0;
    cursor: pointer;

    span {
      margin-right: 5px;
    }
  }
`

export const LabelContainer = styled.div`
  display: flex;
  flex-direction: column;
`

export const SecondaryText = styled.span`
  color: var(--wry-text-tertiary);
  margin-top: 5px;
`

interface CountButtonProps {
  visible?: boolean
}
export const CountButton = styled.button<CountButtonProps>`
  background-color: transparent;
  border: 0;
  padding: 0;
  text-align: right;
  font-family: var(--wry-font);
  font: var(--wry-body);
  font-weight: 500;
  color: var(--wry-accent-text);
  padding: 5px 6px;
  max-width: 100%;
  border-radius: 4px;
  border: 2px solid transparent;
  cursor: pointer;
  display: ${(props) => (props.visible === false ? 'none' : 'block')};

  &:hover {
    background-color: var(--wry-fill);
  }

  &:focus-visible {
    border-color: var(--wry-accent-text);
  }

  &:disabled,
  [disabled] {
    color: var(--wry-text-tertiary);
    pointer-events: none; /* This disables native title tooltip */

    &:hover {
      background-color: unset;
    }
  }
`
