// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js'
import 'chrome://resources/cr_elements/cr_button/cr_button.js'
import 'chrome://resources/brave/leo.bundle.js'
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js'
import {css} from '//resources/lit/v3_0/lit.rollup.js'
import type {CSSResultGroup} from '//resources/lit/v3_0/lit.rollup.js'
import type {HistoryEntry} from 'chrome://resources/cr_components/history/history.mojom-webui.js'
import {BrowserProxyImpl} from './browser_proxy.js'
import {PageHandler, PageCallbackRouter, RestoreResult} from './wryling_trails.mojom-webui.js'
import type {Trail, TrailNode} from './wryling_trails.mojom-webui.js'
import {renderHistory} from './wryling_history_templates.js'

const dayKey = (time: number) => new Date(time).toLocaleDateString()

export class WrylingHistoryElement extends CrLitElement {
  static get is() { return 'wryling-history' }
  static override get styles(): CSSResultGroup {
    return css`
      :host { display: block; height: 100%; overflow: auto; color: var(--wry-text); font: var(--wry-body); background: var(--wry-trail-canvas); }
      * { box-sizing: border-box; }
      main { width: 100%; max-width: var(--wry-wide-content-width); padding: var(--wry-space-6) var(--wry-space-6) var(--wry-space-12); margin: auto; }
      header { display: flex; align-items: center; gap: var(--wry-space-3); margin-bottom: var(--wry-space-6); }
      h1 { font: var(--wry-large-title); margin: 0 auto 0 0; }
      .search { position: relative; width: var(--wry-search-width); max-width: 40%; }
      .search svg { position: absolute; width: var(--wry-search-icon); height: var(--wry-search-icon); inset-inline-start: var(--wry-space-2); top: 50%; transform: translateY(-50%); fill: none; stroke: var(--wry-text-tertiary); stroke-width: 1.5; pointer-events: none; }
      input { width: 100%; height: var(--wry-control-height); border: 0; border-radius: var(--wry-radius-m); font: var(--wry-body); background: var(--wry-fill); color: var(--wry-text); padding: 0 var(--wry-space-2) 0 var(--wry-space-8) !important; }
      input::placeholder { color: var(--wry-text-tertiary); }
      h2 { font: var(--wry-headline); color: var(--wry-trail-secondary); margin: var(--wry-space-6) 0 var(--wry-space-2); }
      .day { background: var(--wry-trail-card); border: var(--wry-hairline) solid var(--wry-trail-stroke); border-radius: var(--wry-trail-radius); padding: var(--wry-space-2); }
      .row { display: flex; align-items: center; gap: var(--wry-space-2); height: var(--wry-trail-row-height); min-width: 0; padding-inline: var(--wry-space-2); border-radius: var(--wry-radius-m); position: relative; }
      .row:hover { background: var(--wry-hover); }
      time { flex: 0 0 var(--wry-trail-time-width); font-variant-numeric: tabular-nums; white-space: nowrap; color: var(--wry-trail-secondary); }
      img { width: var(--wry-icon); height: var(--wry-icon); border-radius: var(--wry-radius-xs); flex: none; }
      .title { color: var(--wry-text); text-decoration: none; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; min-width: 0; }
      .domain { color: var(--wry-trail-domain); font: var(--wry-callout); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; min-width: 0; flex: 1; }
      button, .tool { border: 0; background: transparent !important; border-radius: var(--wry-radius-s) !important; padding: var(--wry-space-1) var(--wry-space-2) !important; color: var(--wry-trail-accent-text) !important; font: var(--wry-callout) !important; min-height: 0 !important; cursor: default; text-decoration: none; white-space: nowrap; }
      button:hover, .tool:hover { background: var(--wry-hover) !important; }
      .trail-toggle { margin-inline-start: auto; flex: none; width: var(--wry-control-height); color: var(--wry-trail-secondary); }
      .trail-toggle .text { display: none; }
      .row:is(:hover, :focus-within) .trail-toggle { width: auto; background: var(--wry-trail-open-fill) !important; color: var(--wry-trail-accent-text) !important; }
      .row:is(:hover, :focus-within) .trail-toggle .text { display: inline; }
      .row:is(:hover, :focus-within) .trail-toggle .symbol { display: none; }
      .delete { opacity: 0; color: var(--wry-trail-secondary); }
      .row:is(:hover, :focus-within) .delete { opacity: 1; }
      .trail { margin: var(--wry-space-2) 0; background: var(--wry-trail-card); border: var(--wry-hairline) solid var(--wry-trail-stroke); border-radius: var(--wry-trail-radius); padding: var(--wry-space-3); }
      .trail-header { display: flex; align-items: center; justify-content: space-between; gap: var(--wry-space-3); padding: 0 var(--wry-space-2) var(--wry-space-2); font: var(--wry-headline); }
      .trail-header strong { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
      .children { margin-inline-start: 0; padding-inline-start: calc(var(--wry-trail-indent) - var(--wry-hairline)); border-inline-start: var(--wry-hairline) solid var(--wry-separator); }
      .branch { margin-top: var(--wry-space-2); background: var(--branch-fill); border-radius: var(--wry-radius-m); }
      .branch-header { display: flex; gap: var(--wry-space-2); padding: var(--wry-space-2); font: var(--wry-callout); color: var(--wry-trail-secondary); }
      .branch-header span { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
      .current { background: var(--wry-trail-current) !important; box-shadow: var(--wry-knob-shadow); }
      .state { color: var(--wry-trail-secondary); font: var(--wry-caption); white-space: nowrap; }
      .more-pages { display: block; min-height: var(--wry-trail-row-height); margin-inline-start: 0; }
      .empty { margin: var(--wry-space-12) auto; text-align: center; color: var(--wry-trail-secondary); }
      .empty strong { display: block; color: var(--wry-text); font: var(--wry-headline); margin-bottom: var(--wry-space-2); }
      .footer { display: flex; justify-content: center; margin-top: var(--wry-space-4); }
      .status { color: var(--wry-trail-secondary); min-height: var(--wry-space-5); font: var(--wry-callout); }
      @media (max-width: 700px) { main { padding-inline: var(--wry-space-4); } header { flex-wrap: wrap; } .search { max-width: none; width: 100%; order: 3; } .domain { display: none; } .title { flex: 1; } }
      @media (forced-colors: active) { :host, .day, .trail { background: Canvas; color: CanvasText; } .day, .trail { border: 1px solid CanvasText; } .row { color: CanvasText; } }
    `
  }

  override render() {
    // Trails updates invalidate the URL/day index used by the renderer.
    void this.trails_
    return renderHistory({
      entries: this.entries, term: this.term, finished: this.finished,
      querying: this.querying, expanded_: this.expanded_, chains_: this.chains_,
      pendingDelete_: this.pendingDelete_, message_: this.message_, index_: this.index_,
      onSearch_: event => this.onSearch_(event),
      toggleTrail_: id => this.toggleTrail_(id),
      restore_: node => this.restore_(node), delete_: () => this.delete_(),
      setPendingDelete: entry => { this.pendingDelete_ = entry },
      expandChain: id => { this.chains_ = new Set(this.chains_).add(id) },
    })
  }
  static override get properties() {
    return {entries: {type: Array}, term: {type: String}, finished: {type: Boolean}, querying: {type: Boolean}, trails_: {type: Array, state: true}, expanded_: {type: Object, state: true}, chains_: {type: Object, state: true}, pendingDelete_: {type: Object, state: true}, message_: {type: String, state: true}}
  }
  accessor entries: HistoryEntry[] = []
  accessor term = ''
  accessor finished = true
  accessor querying = false
  private accessor trails_: Trail[] = []
  private accessor expanded_ = new Set<bigint>()
  private accessor chains_ = new Set<bigint>()
  private accessor pendingDelete_: HistoryEntry|null = null
  private accessor message_ = ''
  private handler_ = PageHandler.getRemote()
  private callback_ = new PageCallbackRouter()
  private listener_: number|null = null
  private fetchGeneration_ = 0
  private searchTimer_: number|undefined
  private index_ = new Map<string, Trail>()


  override connectedCallback() {
    super.connectedCallback()
    this.handler_ = PageHandler.getRemote()
    this.listener_ = this.callback_.onTrailsChanged.addListener(() => void this.loadTrails_())
    this.handler_.setPage(this.callback_.$.bindNewPipeAndPassRemote())
    void this.loadTrails_()
  }
  override disconnectedCallback() {
    if (this.listener_ !== null) this.callback_.removeListener(this.listener_)
    window.clearTimeout(this.searchTimer_)
    ++this.fetchGeneration_
    this.handler_.$.close()
    super.disconnectedCallback()
  }
  private async loadTrails_() {
    const generation = ++this.fetchGeneration_
    try {
      const {trails} = await this.handler_.getTrails(0, 100)
      if (generation !== this.fetchGeneration_) return
      this.index_.clear()
      for (const trail of trails) for (const node of trail.nodes) {
        const key = node.url + '\n' + dayKey(node.time)
        if (!this.index_.has(key)) this.index_.set(key, trail)
      }
      this.trails_ = trails
    } catch {
      // Plain history remains available if the optional Trails service stops.
      this.trails_ = []
      this.index_.clear()
    }
  }
  private onSearch_(event: InputEvent) {
    const term = (event.currentTarget as HTMLInputElement).value
    this.term = term
    this.expanded_ = new Set()
    window.clearTimeout(this.searchTimer_)
    this.searchTimer_ = window.setTimeout(() => document.dispatchEvent(new CustomEvent('change-query', {detail: {search: term, after: null}})), 150)
  }
  private toggleTrail_(id: bigint) {
    const next = new Set(this.expanded_)
    next.has(id) ? next.delete(id) : next.add(id)
    this.expanded_ = next
  }
  private async restore_(node: TrailNode) {
    const {result} = await this.handler_.restore(node.id)
    this.message_ = result === RestoreResult.kLoadedFresh ? 'Opened a fresh visit. The original form was not resubmitted.' :
      result === RestoreResult.kNotFound ? 'This page is no longer available in the trail.' : ''
  }
  private async delete_() {
    const entry = this.pendingDelete_
    if (!entry) return
    await BrowserProxyImpl.getInstance().handler.removeVisits(Object.entries(entry.allTimestamps).map(([url, timestamps]) => ({url, timestamps})))
    this.pendingDelete_ = null
    document.dispatchEvent(new CustomEvent('query-history', {detail: false}))
  }

}
customElements.define(WrylingHistoryElement.is, WrylingHistoryElement)

declare global {
  interface HTMLElementTagNameMap {
    'wryling-history': WrylingHistoryElement
  }
}
