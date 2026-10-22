// Copyright (c) 2023 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import * as React from 'react'
import type * as CommandsMojo from 'gen/brave/components/commands/common/commands.mojom.m.js'
import styled from 'styled-components'
import Keys from './Keys'
import ConfigureShortcut from './ConfigureShortcut'
import { commandsCache } from '../commands'
import { stringToKeys } from '../utils/accelerator'
import Button from '@brave/leo/react/button'
import Icon from '@brave/leo/react/icon'
import { getLocale } from '$web-common/locale'

const Grid = styled.div`
  position: relative;
  display: grid;
  grid-template-columns: minmax(120px, 1fr) minmax(100px, 1fr) auto auto;
  column-gap: var(--wry-space-2);
  align-items: center;
  padding: var(--wry-space-1) var(--wry-space-4);
  min-height: var(--wry-row-height);
  box-sizing: border-box;
  font: var(--wry-body);
  color: var(--wry-text);
  & + &::before { content: ''; position: absolute; inset-inline: var(--wry-space-4); top: 0; height: var(--wry-hairline); background: var(--wry-separator); }
  .wry-shortcut-add { grid-column: 4; }
  .wry-shortcut-reset { grid-column: 3; }
`

const Column = styled.div`
  display: flex;
  flex-direction: column;
  gap: var(--wry-space-1);
`

const RemoveButton = styled(Button).attrs({className: 'wry-shortcut-remove'})`
  visibility: hidden;
  flex-grow: 0;
`

const Row = styled.div`
  display: flex;
  flex-direction: row;
  flex-wrap: nowrap;
  align-items: center;
  gap: var(--wry-space-1);

  &:hover ${RemoveButton} {
    visibility: visible;
  }
`

function Accelerator({
  commandId,
  accelerator
}: {
  commandId: number
  accelerator: CommandsMojo.Accelerator
}) {
  return (
    <Row>
      <Keys keys={stringToKeys(accelerator.keys)} />
      {!accelerator.unmodifiable &&
        <RemoveButton
          size="small"
          kind="plain-faint"
          onClick={() =>
            commandsCache.unassignAccelerator(commandId, accelerator.codes)
          }
        >
          <Icon name="remove-circle-outline" />
        </RemoveButton>}
    </Row>
  )
}

export default function Command({
  command
}: {
  command: CommandsMojo.Command
}) {
  const [adding, setAdding] = React.useState(false)

  return (
    <Grid>
      <div>{command.name}</div>
      <Column>
        {command.accelerators.map((a, i) => (
          <Accelerator key={i} commandId={command.id} accelerator={a} />
        ))}
      </Column>
      {command.modified && (
        <Button
          className="wry-shortcut-reset"
          size="small"
          kind="plain-faint"
          onClick={() => commandsCache.reset(command.id)}
        >
          {getLocale(S.SHORTCUTS_PAGE_RESET_COMMAND)}
        </Button>
      )}
      <Button
        size="small"
        kind="plain"
        className="wry-shortcut-add"
        disabled={adding}
        onClick={() => setAdding(true)}
      >
        {getLocale(S.SHORTCUTS_PAGE_ADD_SHORTCUT)}
      </Button>
      {adding && (
        <ConfigureShortcut
          onChange={(info) => {
            setAdding(false)
            commandsCache.assignAccelerator(command.id, info)
          }}
          onCancel={() => {
            setAdding(false)
          }}
        />
      )}
    </Grid>
  )
}
