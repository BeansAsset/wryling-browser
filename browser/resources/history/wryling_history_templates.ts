// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js'
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js'
import type {HistoryEntry} from 'chrome://resources/cr_components/history/history.mojom-webui.js'
import type {Trail, TrailNode} from './wryling_trails.mojom-webui.js'

const dayKey = (time: number) => new Date(time).toLocaleDateString()
const timeLabel = (time: number) => new Date(time).toLocaleTimeString(undefined, {hour: '2-digit', minute: '2-digit', hourCycle: 'h23'})
const domain = (url: string) => { try { return new URL(url).hostname } catch { return url } }
// chrome://favicon2 reads camelCase keys (components/favicon_base/
// favicon_url_parser.cc); unknown keys fall back to the default globe.
const favicon = (url: string) => {
  const icon = new URL('chrome://favicon2/')
  icon.searchParams.set('pageUrl', url)
  icon.searchParams.set('size', '16')
  icon.searchParams.set('scaleFactor', `${Math.max(1, Math.round(window.devicePixelRatio || 1))}x`)
  icon.searchParams.set('allowGoogleServerFallback', '0')
  return icon.href
}

export interface HistoryView {
  entries: HistoryEntry[]
  term: string
  finished: boolean
  querying: boolean
  expanded_: Set<bigint>
  chains_: Set<bigint>
  pendingDelete_: HistoryEntry|null
  message_: string
  index_: Map<string, Trail>
  onSearch_: (event: InputEvent) => void
  toggleTrail_: (id: bigint) => void
  restore_: (node: TrailNode) => Promise<void>
  delete_: () => Promise<void>
  setPendingDelete: (entry: HistoryEntry|null) => void
  expandChain: (id: bigint) => void
}

// Recursive trail fragments share the same host snapshot and actions.
export function renderHistory(view: HistoryView): TemplateResult {
  const entryRow_ = (entry: HistoryEntry, trail?: Trail): TemplateResult => {
    return html`<div class="row" data-history-url=${entry.url}>
      <time>${timeLabel(entry.time)}</time><img src=${favicon(entry.url)} alt="">
      <a class="title" href=${entry.url}>${entry.title || entry.url}</a><span class="domain">${domain(entry.url)}</span>
      ${trail ? html`<button class="trail-toggle" aria-label=${'Open trail · ' + trail.title} @click=${() => view.toggleTrail_(trail.id)}><svg class="symbol" width="16" height="16" viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="4" cy="3.5" r="1.7"/><circle cx="4" cy="12.5" r="1.7"/><circle cx="12" cy="6.5" r="1.7"/><path d="M4 5.2v5.6M12 8.2c0 2.3-2 3.3-6.3 3.9"/></svg><span class="text">Open trail</span></button>` : nothing}
      <button class="delete" aria-label=${'Remove ' + (entry.title || entry.url) + ' from history'} @click=${() => { view.setPendingDelete(entry) }}>×</button>
    </div>`
  }
  const tree_ = (trail: Trail): TemplateResult => {
    const nodes = new Map(trail.nodes.map(n => [n.id, n]))
    const children = new Map<bigint, TrailNode[]>()
    for (const node of trail.nodes) { const list = children.get(node.parent) || []; list.push(node); children.set(node.parent, list) }
    const descendants = (id: bigint): TrailNode[] => {
      const found: TrailNode[] = [], seen = new Set<bigint>([id]), queue = [...(children.get(id) || [])]
      while (queue.length) { const n = queue.pop()!; if (seen.has(n.id)) continue; seen.add(n.id); found.push(n); queue.push(...(children.get(n.id) || [])) }
      return found
    }
    const cameBack = (node: TrailNode) => node.openInTab >= 0 && descendants(node.id).length > 0 && !descendants(node.id).some(n => n.openInTab >= 0)
    const row = (node: TrailNode, returned = false) => html`<div class="row ${node.openInTab >= 0 && (!cameBack(node) || returned) ? 'current' : ''}" data-node=${String(node.id)}>
      <time>${timeLabel(node.time)}</time><img src=${favicon(node.url)} alt="">
      <a class="title" href=${node.url} @click=${(e: MouseEvent) => { if (e.button === 0 && !e.metaKey && !e.ctrlKey && !e.shiftKey) { e.preventDefault(); void view.restore_(node) } }}>${node.title || domain(node.url)}</a>
      <span class="domain">${domain(node.url)}</span>${returned ? html`<span class="state">Came back</span>` : nothing}
      ${node.openInTab >= 0 && (!cameBack(node) || returned) ? html`<button @click=${() => void view.restore_(node)}>Open now</button>` : nothing}
    </div>`
    const renderNode = (node: TrailNode, depth: number, ancestors: Set<bigint>): TemplateResult|typeof nothing => {
      if (ancestors.has(node.id)) return nothing
      const path = new Set(ancestors).add(node.id), nested = children.get(node.id) || [], parent = nodes.get(node.parent)
      const isBranch = parent && parent.branch !== node.branch
      const branchPages = isBranch ? descendants(node.id).length + 1 : 0
      const chainOpen = depth < 5 || view.chains_.has(node.id)
      const content = html`${row(node)}${nested.length ? html`<div class="children">
        ${chainOpen ? nested.map(child => renderNode(child, view.chains_.has(node.id) ? 0 : depth + 1, path)) : html`<button class="more-pages" @click=${() => { view.expandChain(node.id) }}>${descendants(node.id).length} more pages</button>`}
      </div>` : nothing}${cameBack(node) ? row(node, true) : nothing}`
      return isBranch ? html`<section class="branch" style=${'--branch-fill:var(--wry-trail-branch-' + (Math.abs(node.branch) % 5) + ')'}>
        <div class="branch-header"><strong>${node.title || domain(node.url)}</strong><span>· ${branchPages} ${branchPages === 1 ? 'page' : 'pages'} · branched from ${parent.title || domain(parent.url)}</span></div>${content}
      </section>` : content
    }
    return html`<section class="trail" data-trail=${String(trail.id)}>
      <div class="trail-header"><strong>Trail · ${trail.title}</strong><button @click=${() => view.toggleTrail_(trail.id)}>Close trail</button></div>
      ${trail.nodes.filter(n => !nodes.has(n.parent)).map(n => renderNode(n, 0, new Set()))}
    </section>`
  }
  const groups_ = () => {
    const groups = new Map<string, HistoryEntry[]>()
    for (const entry of view.entries) { const key = dayKey(entry.time), list = groups.get(key) || []; list.push(entry); groups.set(key, list) }
    const rendered = new Set<bigint>()
    return [...groups.values()].map(entries => {
      const date = new Date(entries[0]!.time), today = dayKey(date.getTime()) === dayKey(Date.now())
      const label = (today ? 'Today ' : '') + date.toLocaleDateString(undefined, {weekday: 'long', day: 'numeric', month: 'long'})
      return html`<section><h2>${label}</h2><div class="day">${entries.map(entry => {
        const trail = view.index_.get(entry.url + '\n' + dayKey(entry.time))
        if (!trail || !view.expanded_.has(trail.id)) return entryRow_(entry, trail)
        if (rendered.has(trail.id)) return nothing
        rendered.add(trail.id)
        return tree_(trail)
      })}</div></section>`
    })
  }
    return html`<main>
      <header><h1>History</h1><label class="search"><svg viewBox="0 0 16 16" aria-hidden="true"><circle cx="6.5" cy="6.5" r="4.5"></circle><path d="m10 10 4 4"></path></svg><input type="search" aria-label="Search history" placeholder="Search history" .value=${view.term} @input=${view.onSearch_}></label><a class="tool" href="chrome://settings/clearBrowserData">Clear history…</a><a class="tool" href="chrome://history/syncedTabs">Other devices</a></header>
      <div class="status" role="status">${view.message_}</div>
      ${view.entries.length ? groups_() : html`<div class="empty"><strong>${view.querying ? 'Loading history…' : view.term ? 'No matching history' : 'Your history starts here'}</strong>${view.querying ? '' : view.term ? 'Try a different word or address.' : 'Pages you visit will appear here, grouped by day.'}</div>`}
      ${!view.finished && view.entries.length ? html`<div class="footer"><button ?disabled=${view.querying} @click=${() => document.dispatchEvent(new CustomEvent('query-history', {detail: true}))}>${view.querying ? 'Loading…' : 'Show more history'}</button></div>` : nothing}
      ${view.pendingDelete_ ? html`<cr-dialog show-on-attach @close=${() => { view.setPendingDelete(null) }}><div slot="title">Remove from history?</div><div slot="body">${view.pendingDelete_.title || view.pendingDelete_.url}</div><div slot="button-container"><cr-button @click=${() => { view.setPendingDelete(null) }}>Cancel</cr-button><cr-button class="action-button" @click=${() => void view.delete_()}>Remove</cr-button></div></cr-dialog>` : nothing}
    </main>`
}
