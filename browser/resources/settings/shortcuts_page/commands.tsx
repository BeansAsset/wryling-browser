// Copyright (c) 2023 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import './strings'

// This page contains a copy of Svelte (via @brave/leo) which will attempt
// to register its own trusted-types policy which duplicates the one registered
// by the main page.
import '$web-common/disableDuplicateSvelteTrustedPolicies'

import * as CommandsMojo from 'gen/brave/components/commands/common/commands.mojom.m.js'
import * as React from 'react'
import { createRoot } from 'react-dom/client'
import styled, { StyleSheetManager } from 'styled-components'
import StyledComponentsProvider from '$web-common/StyledComponentsProvider'
import Command from './components/Command'
import { CommandsCache } from './utils/commandsCache'
import { match } from './utils/match'
import { getLocale } from '$web-common/locale'

import Icon, { setIconBasePath } from '@brave/leo/react/icon'
import Button from '@brave/leo/react/button'
setIconBasePath('chrome://resources/brave-icons')

const Container = styled.div`
  padding: 0;
  color: var(--wry-text);
  background: var(--wry-canvas);
  font: var(--wry-body);
`
const CommandsContainer = styled.div`
  display: flex;
  flex-direction: column;
  margin-bottom: var(--wry-space-4);
  background: var(--wry-raised);
  border: var(--wry-hairline) solid var(--wry-stroke);
  border-radius: var(--wry-radius-xl);
  overflow: hidden;
`
const SearchContainer = styled.div.attrs({className: 'wry-command-search'})`
  position: relative;
  display: flex;
  align-items: center;
  margin-bottom: var(--wry-space-4);
`
const SearchBox = styled.input`
  width: 100%;
  min-width: 0;
  height: var(--wry-control-height);
  box-sizing: border-box;
  border: 0;
  border-radius: var(--wry-radius-m);
  background: var(--wry-fill);
  color: var(--wry-text);
  font: var(--wry-body);
  padding-inline: var(--wry-space-8) var(--wry-space-2) !important;
`
const SearchIcon = styled(Icon)`
  --leo-icon-size: var(--wry-search-icon);
  width: var(--wry-search-icon);
  height: var(--wry-search-icon);
  position: absolute;
  left: var(--wry-search-icon-inset);
  top: 50%;
  transform: translateY(-50%);
  pointer-events: none;
`

export const commandsCache = new CommandsCache()
export function useCommands() {
  const [commands, setCommands] = React.useState<{
    [id: number]: CommandsMojo.Command
  }>({})
  React.useEffect(() => {
    const listener = (l: { [command: number]: CommandsMojo.Command }) => {
      setCommands(l)
    }
    commandsCache.addListener(listener)
    return () => commandsCache.removeListener(listener)
  }, [])
  return commands
}

function App() {
  const [filter, setFilter] = React.useState('')

  const commands = useCommands()

  const filteredCommands = React.useMemo(
    () => Object.values(commands)?.filter((c) => match(filter, c)),
    [filter, commands]
  )
  return (
    <Container>
      <SearchContainer>
          <SearchIcon name="search" />
          <SearchBox
            placeholder={getLocale(S.SHORTCUTS_PAGE_SEARCH_PLACEHOLDER)}
            value={filter}
            onChange={(e) => {
              setFilter(e.target.value)
            }}
          />
      </SearchContainer>
      <CommandsContainer>
        {filteredCommands?.map((c) => (
          <Command key={c.id} command={c} />
        ))}
      </CommandsContainer>
      <Button kind="plain-faint" onClick={() => commandsCache.resetAll()}>
        {getLocale(S.SHORTCUTS_PAGE_RESET_ALL)}
      </Button>
    </Container>
  )
}

export const mount = (at: HTMLElement) => {
  createRoot(at).render(
    <StyledComponentsProvider>
      <StyleSheetManager target={at}>
        <App />
      </StyleSheetManager>
    </StyledComponentsProvider>
  )
}
