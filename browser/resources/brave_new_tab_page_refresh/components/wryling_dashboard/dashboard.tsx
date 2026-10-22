/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import { ClockFormat } from '../../state/new_tab_store'
import { useNewTabState } from '../../context/new_tab_context'
import {
  DashboardSite,
  getDashboardHandler,
  useDashboard,
} from './dashboard_api'

import { style } from './dashboard.style'

// The site's own app icon fills a tile from this size up; smaller icons
// (favicons) sit in the middle of a plain tile.
const largeIconSize = 64
const maxPinnedSites = 48
const undoTimeout = 5000
const internalDragType = 'application/x-wryling-dashboard-site'

function hostOf(url: string) {
  try {
    return new URL(url).hostname.replace(/^www\./, '')
  } catch {
    return url
  }
}

// A steady colour per site for its letter tile, dark enough for white text.
function letterTileColor(url: string) {
  let hash = 0
  for (const char of hostOf(url)) {
    hash = (hash * 31 + char.charCodeAt(0)) >>> 0
  }
  return `oklch(0.5 0.13 ${hash % 360})`
}

function letterOf(site: { title: string; url: string }) {
  const text = site.title.trim() || hostOf(site.url)
  return Array.from(text)[0]?.toLocaleUpperCase() ?? '?'
}

function spaceColorCss(argb: number) {
  if (!argb) {
    return 'var(--wry-accent, #6f52dc)'
  }
  const r = (argb >>> 16) & 0xff
  const g = (argb >>> 8) & 0xff
  const b = argb & 0xff
  return `rgb(${r}, ${g}, ${b})`
}

// The date and time, refreshed every few seconds. The time follows the New
// Tab clock setting (12/24-hour or automatic).
function useDateTime(clockFormat: ClockFormat) {
  const [now, setNow] = React.useState(() => new Date())
  React.useEffect(() => {
    const timer = setInterval(() => setNow(new Date()), 5000)
    return () => clearInterval(timer)
  }, [])
  const timeFormat = React.useMemo(
    () =>
      new Intl.DateTimeFormat(undefined, {
        hour: 'numeric',
        minute: 'numeric',
        hourCycle:
          clockFormat === ClockFormat.k12
            ? 'h12'
            : clockFormat === ClockFormat.k24
              ? 'h23'
              : undefined,
      }),
    [clockFormat],
  )
  return {
    time: timeFormat.formatToParts(now),
    date: new Intl.DateTimeFormat(undefined, {
      weekday: 'long',
      day: 'numeric',
      month: 'long',
    }).format(now),
  }
}

function TileIcon(props: { site: { url: string; title: string; icon?: string; iconSize?: number } }) {
  const { site } = props
  const [failed, setFailed] = React.useState(false)
  React.useEffect(() => setFailed(false), [site.icon])
  if (site.icon && !failed) {
    const large = (site.iconSize ?? 0) >= largeIconSize
    return (
      <span className={large ? 'icon app-icon' : 'icon small-icon'}>
        <img
          src={site.icon}
          alt=''
          draggable={false}
          onError={() => setFailed(true)}
        />
      </span>
    )
  }
  return (
    <span
      className='icon letter-icon'
      style={{ background: letterTileColor(site.url) }}
    >
      {letterOf(site)}
    </span>
  )
}

interface EditorState {
  site: DashboardSite | null
}

function SiteEditor(props: {
  spaceName: string
  state: EditorState | null
  onClose: () => void
}) {
  const dialogRef = React.useRef<HTMLDialogElement>(null)
  const [url, setUrl] = React.useState('')
  const [title, setTitle] = React.useState('')
  const [error, setError] = React.useState('')
  const editing = props.state?.site ?? null

  React.useEffect(() => {
    const dialog = dialogRef.current
    if (!dialog) {
      return
    }
    if (props.state) {
      setUrl(editing?.url ?? '')
      setTitle(editing?.title ?? '')
      setError('')
      if (!dialog.open) {
        dialog.showModal()
      }
    } else if (dialog.open) {
      dialog.close()
    }
  }, [props.state])

  async function save(event: React.FormEvent) {
    event.preventDefault()
    if (!url.trim()) {
      setError('Enter the site’s address.')
      return
    }
    const handler = getDashboardHandler()
    const result = editing
      ? (await handler.updateSite(editing.url, url, title)).updated
      : (await handler.addSite(url, title)).added
    if (!result) {
      setError('That isn’t a web address. Try one like example.com.')
      return
    }
    props.onClose()
  }

  const preview = {
    url: url.includes('://') ? url : `https://${url || 'example.com'}`,
    title: title || (url ? hostOf(url.includes('://') ? url : `https://${url}`) : ''),
    icon: editing && url === editing.url ? editing.icon : '',
    iconSize: editing && url === editing.url ? editing.iconSize : 0,
  }

  return (
    <dialog
      ref={dialogRef}
      className='site-editor'
      onClose={props.onClose}
      onMouseDown={(event) => {
        if (event.target === dialogRef.current) {
          props.onClose()
        }
      }}
    >
      <form onSubmit={save}>
        <h2>
          {editing ? 'Edit site' : `Add site to ${props.spaceName}`}
        </h2>
        <div className='editor-body'>
          <div className='preview'>
            <TileIcon site={preview} />
          </div>
          <div className='fields'>
            <label>
              Address
              <input
                type='text'
                inputMode='url'
                autoFocus
                spellCheck={false}
                placeholder='example.com'
                value={url}
                aria-invalid={!!error}
                onChange={(event) => {
                  setUrl(event.target.value)
                  setError('')
                }}
              />
            </label>
            <label>
              Name
              <input
                type='text'
                placeholder={url ? hostOf(preview.url) : 'Example'}
                value={title}
                onChange={(event) => setTitle(event.target.value)}
              />
            </label>
            {error && <p className='error'>{error}</p>}
          </div>
        </div>
        <div className='actions'>
          <button
            type='button'
            className='quiet'
            onClick={props.onClose}
          >
            Cancel
          </button>
          <button
            type='submit'
            className='primary'
          >
            {editing ? 'Save' : 'Add'}
          </button>
        </div>
      </form>
    </dialog>
  )
}

interface MenuState {
  site: DashboardSite
  x: number
  y: number
}

function SiteMenu(props: {
  state: MenuState | null
  spaceName: string
  onEdit: (site: DashboardSite) => void
  onRemove: (site: DashboardSite) => void
  onEditDashboard: () => void
  onClose: () => void
}) {
  const ref = React.useRef<HTMLDivElement>(null)

  React.useLayoutEffect(() => {
    ref.current?.setAttribute('popover', 'auto')
  }, [])

  React.useEffect(() => {
    const onToggle = (event: ToggleEvent) => {
      if (event.newState === 'closed') {
        props.onClose()
      }
    }
    const menu = ref.current
    menu?.addEventListener('toggle', onToggle)
    return () => menu?.removeEventListener('toggle', onToggle)
  }, [props.onClose])

  React.useEffect(() => {
    const menu = ref.current
    if (!menu) {
      return
    }
    if (props.state) {
      menu.style.left = `${Math.min(props.state.x, innerWidth - 230)}px`
      menu.style.top = `${Math.min(props.state.y, innerHeight - 200)}px`
      if (!menu.matches(':popover-open')) {
        menu.showPopover()
      }
      menu.querySelector('button')?.focus()
    } else if (menu.matches(':popover-open')) {
      menu.hidePopover()
    }
  }, [props.state])

  const site = props.state?.site
  const act = (fn: () => void) => () => {
    fn()
    props.onClose()
  }

  return (
    <div
      ref={ref}
      className='site-menu'
      role='menu'
    >
      {site && (
        <>
          <button
            role='menuitem'
            onClick={act(() => window.open(site.url, '_blank', 'noopener'))}
          >
            Open in new tab
          </button>
          <div className='separator' />
          {site.pinned ? (
            <button
              role='menuitem'
              onClick={act(() => props.onEdit(site))}
            >
              Edit…
            </button>
          ) : (
            <button
              role='menuitem'
              onClick={act(() =>
                getDashboardHandler().moveSite(site.url, maxPinnedSites),
              )}
            >
              Keep on dashboard
            </button>
          )}
          <button
            role='menuitem'
            onClick={act(props.onEditDashboard)}
          >
            Rearrange…
          </button>
          <div className='separator' />
          <button
            role='menuitem'
            className='destructive'
            onClick={act(() => props.onRemove(site))}
          >
            {site.pinned ? 'Remove' : `Don’t show in ${props.spaceName}`}
          </button>
        </>
      )}
    </div>
  )
}

export function WrylingDashboard() {
  const dashboard = useDashboard()
  const clockFormat = useNewTabState((s) => s.clockFormat)
  const { date, time } = useDateTime(clockFormat)

  const [editMode, setEditMode] = React.useState(false)
  const [editor, setEditor] = React.useState<EditorState | null>(null)
  const [menu, setMenu] = React.useState<MenuState | null>(null)
  const [removed, setRemoved] = React.useState<DashboardSite | null>(null)
  const [dragUrl, setDragUrl] = React.useState<string | null>(null)
  const [dropIndex, setDropIndex] = React.useState<number | null>(null)
  const [externalDrag, setExternalDrag] = React.useState(false)

  React.useEffect(() => {
    if (!removed) {
      return
    }
    const timer = setTimeout(() => setRemoved(null), undoTimeout)
    return () => clearTimeout(timer)
  }, [removed])

  React.useEffect(() => {
    if (!editMode) {
      return
    }
    const onKey = (event: KeyboardEvent) => {
      if (event.key === 'Escape' && !editor) {
        setEditMode(false)
      }
    }
    addEventListener('keydown', onKey)
    return () => removeEventListener('keydown', onKey)
  }, [editMode, editor])

  if (!dashboard) {
    return <div data-css-scope={style.scope} className='dashboard-placeholder' />
  }

  const sites = dashboard.sites
  const pinnedCount = sites.filter((site) => site.pinned).length
  const hasFrequent = pinnedCount < sites.length
  const spaceName = dashboard.spaceName || 'this space'

  // While dragging, show the tiles in the order they would drop in.
  let shown = sites
  if (dragUrl !== null && dropIndex !== null) {
    const from = sites.findIndex((site) => site.url === dragUrl)
    if (from >= 0) {
      shown = sites.slice()
      const [moving] = shown.splice(from, 1)
      shown.splice(dropIndex, 0, moving)
    }
  }

  function targetIndex(index: number, draggedPinned: boolean) {
    return Math.max(0, Math.min(index, pinnedCount - (draggedPinned ? 1 : 0)))
  }

  function onRemove(site: DashboardSite) {
    getDashboardHandler().removeSite(site.url)
    setRemoved(site)
  }

  function onTileKeyDown(
    event: React.KeyboardEvent,
    site: DashboardSite,
    index: number,
  ) {
    if (!editMode || !site.pinned) {
      return
    }
    const delta =
      event.key === 'ArrowLeft' ? -1 : event.key === 'ArrowRight' ? 1 : 0
    if (delta) {
      event.preventDefault()
      getDashboardHandler().moveSite(
        site.url,
        targetIndex(index + delta, true),
      )
    } else if (event.key === 'Backspace' || event.key === 'Delete') {
      event.preventDefault()
      onRemove(site)
    }
  }

  function onGridDragOver(event: React.DragEvent) {
    const types = Array.from(event.dataTransfer.types)
    if (dragUrl === null && types.includes('text/uri-list')) {
      event.preventDefault()
      event.dataTransfer.dropEffect = 'copy'
      setExternalDrag(true)
    }
  }

  async function onGridDrop(event: React.DragEvent) {
    setExternalDrag(false)
    if (dragUrl !== null) {
      return
    }
    const url = event.dataTransfer
      .getData('text/uri-list')
      .split('\n')
      .find((line) => line && !line.startsWith('#'))
    if (url) {
      event.preventDefault()
      await getDashboardHandler().addSite(url.trim(), '')
    }
  }

  const empty = sites.length === 0

  return (
    <div
      data-css-scope={style.scope}
      className='dashboard-root'
    >
      <section
        className={`card${editMode ? ' editing' : ''}${
          externalDrag ? ' drop-target' : ''
        }`}
        aria-label={`${spaceName} dashboard`}
        onDragOver={onGridDragOver}
        onDragLeave={(event) => {
          if (event.currentTarget === event.target) {
            setExternalDrag(false)
          }
        }}
        onDrop={onGridDrop}
      >
        <header>
          <span className='time'>
            {time.map((part, i) =>
              part.type === 'dayPeriod' ? (
                <span
                  key={i}
                  className='day-period'
                >
                  {part.value}
                </span>
              ) : (
                part.value
              ),
            )}
          </span>
          <span className='date'>{date}</span>
          <span className='space'>
            <span
              className='space-dot'
              style={{ background: spaceColorCss(dashboard.spaceColor) }}
            />
            {dashboard.spaceName}
          </span>
          {!empty && (
            <button
              className={editMode ? 'done-button' : 'edit-button'}
              onClick={() => setEditMode(!editMode)}
            >
              {editMode ? 'Done' : 'Edit'}
            </button>
          )}
        </header>

        {empty ? (
          <div className='empty'>
            <svg
              width='40'
              height='40'
              viewBox='0 0 40 40'
              fill='none'
              stroke='currentColor'
              strokeWidth='1.5'
              aria-hidden='true'
            >
              <rect x='5' y='5' width='12' height='12' rx='3.5' />
              <rect x='23' y='5' width='12' height='12' rx='3.5' />
              <rect x='5' y='23' width='12' height='12' rx='3.5' />
              <path d='M29 24v10M24 29h10' />
            </svg>
            <h3>{spaceName} has no sites yet</h3>
            <p>
              Add the sites you open in this space. Ones you visit often appear
              here too.
            </p>
            <button
              className='primary'
              onClick={() => setEditor({ site: null })}
            >
              Add site
            </button>
          </div>
        ) : (
          <div
            className='grid'
            role='list'
          >
            {shown.map((site, index) => (
              <div
                key={site.url}
                role='listitem'
                className={`tile${site.pinned ? ' pinned' : ' frequent'}${
                  site.url === dragUrl ? ' dragging' : ''
                }`}
                style={{ '--tile-index': index } as React.CSSProperties}
                onDragOver={(event) => {
                  if (dragUrl === null) {
                    return
                  }
                  event.preventDefault()
                  event.dataTransfer.dropEffect = 'move'
                  const dragged = sites.find((s) => s.url === dragUrl)
                  setDropIndex(targetIndex(index, !!dragged?.pinned))
                }}
                onDrop={(event) => {
                  if (dragUrl === null) {
                    return
                  }
                  event.preventDefault()
                  event.stopPropagation()
                  const dragged = sites.find((s) => s.url === dragUrl)
                  getDashboardHandler().moveSite(
                    dragUrl,
                    dropIndex ?? targetIndex(index, !!dragged?.pinned),
                  )
                  setDragUrl(null)
                  setDropIndex(null)
                }}
              >
                <a
                  href={site.url}
                  title={
                    site.pinned
                      ? site.url
                      : `${site.title}, visited often in ${spaceName}`
                  }
                  draggable
                  onClick={(event) => {
                    if (editMode) {
                      event.preventDefault()
                    }
                  }}
                  onDragStart={(event) => {
                    event.dataTransfer.setData(internalDragType, site.url)
                    event.dataTransfer.setData('text/uri-list', site.url)
                    event.dataTransfer.effectAllowed = 'copyMove'
                    setDragUrl(site.url)
                  }}
                  onDragEnd={() => {
                    setDragUrl(null)
                    setDropIndex(null)
                  }}
                  onContextMenu={(event) => {
                    event.preventDefault()
                    const rect = event.currentTarget.getBoundingClientRect()
                    setMenu({
                      site,
                      x: event.clientX || rect.left + rect.width / 2,
                      y: event.clientY || rect.bottom,
                    })
                  }}
                  onKeyDown={(event) => onTileKeyDown(event, site, index)}
                >
                  <TileIcon site={site} />
                  <span className='label'>{site.title}</span>
                </a>
                {editMode && (
                  <button
                    className='remove-badge'
                    aria-label={
                      site.pinned
                        ? `Remove ${site.title}`
                        : `Don’t show ${site.title} in ${spaceName}`
                    }
                    onClick={() => onRemove(site)}
                  >
                    <svg
                      width='10'
                      height='10'
                      viewBox='0 0 10 10'
                      aria-hidden='true'
                    >
                      <path
                        d='M2 2l6 6M8 2l-6 6'
                        stroke='currentColor'
                        strokeWidth='2'
                        strokeLinecap='round'
                      />
                    </svg>
                  </button>
                )}
              </div>
            ))}
            {pinnedCount < maxPinnedSites && (
              <div
                role='listitem'
                className='tile add'
              >
                <button onClick={() => setEditor({ site: null })}>
                  <span className='icon add-icon'>
                    <svg
                      width='22'
                      height='22'
                      viewBox='0 0 16 16'
                      fill='none'
                      stroke='currentColor'
                      strokeWidth='1.3'
                      aria-hidden='true'
                    >
                      <path d='M8 3v10M3 8h10' />
                    </svg>
                  </span>
                  <span className='label'>Add site</span>
                </button>
              </div>
            )}
          </div>
        )}

        {!empty && (
          <p className='footnote'>
            {editMode
              ? 'Drag to rearrange. Sites you drag in front are kept.'
              : hasFrequent
                ? `Your sites first, then the ones you visit most in ${spaceName}`
                : `Sites you visit often in ${spaceName} will appear here too`}
          </p>
        )}
      </section>

      <SiteEditor
        spaceName={spaceName}
        state={editor}
        onClose={() => setEditor(null)}
      />
      <SiteMenu
        state={menu}
        spaceName={spaceName}
        onEdit={(site) => setEditor({ site })}
        onRemove={onRemove}
        onEditDashboard={() => setEditMode(true)}
        onClose={() => setMenu(null)}
      />
      {removed && (
        <div
          className='undo-toast'
          role='status'
        >
          <span>
            {removed.pinned
              ? `Removed ${removed.title}`
              : `${removed.title} won’t show in ${spaceName}`}
          </span>
          <button
            onClick={() => {
              getDashboardHandler().undoRemove()
              setRemoved(null)
            }}
          >
            Undo
          </button>
        </div>
      )}
    </div>
  )
}
