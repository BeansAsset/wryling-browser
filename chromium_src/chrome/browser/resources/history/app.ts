// Copyright (c) 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import {injectStyle} from '//resources/brave/lit_overriding.js'
import {css, html} from '//resources/lit/v3_0/lit.rollup.js'
import type {HistoryEntry, QueryResult, QueryState} from 'chrome://resources/cr_components/history/history.mojom-webui.js'
import './wryling_history.js'
import type {HistoryQueryManagerElement} from './query_manager.js'

import {HistoryAppElement} from './app-chromium.js'

// <if expr="enable_local_ai">
import {loadTimeData} from '//resources/js/load_time_data.js'
import {
  getBraveHistoryEmbeddingsBrowserProxy,
} from './brave_history_embeddings_browser_proxy.js'

// Refresh loadTimeData so any code reading `enableHistoryEmbeddings` or
// `braveHistoryEmbeddingsNeedsRestart` inline picks up the new values, update
// each history-app's reactive `enableHistoryEmbeddings_` accessor (Lit
// re-renders the app shell), and force a re-render of the toolbar/list whose
// `compute*_` methods cache the value.
function onHistoryEmbeddingsEnabledChanged(
    enabled: boolean, needsRestart: boolean) {
  // Upstream gates its calls into the embeddings service on this.
  const searchEnabled = enabled && !needsRestart
  loadTimeData.overrideValues({
    enableHistoryEmbeddings: searchEnabled,
    braveHistoryEmbeddingsEnabled: enabled,
    braveHistoryEmbeddingsNeedsRestart: needsRestart,
  })
  for (const app of document.querySelectorAll('history-app')) {
    ;(app as unknown as {enableHistoryEmbeddings_: boolean})
        .enableHistoryEmbeddings_ = searchEnabled
  }
  const root = document.querySelector('history-app')?.shadowRoot
  if (!root) {
    return
  }
  for (const el of root.querySelectorAll(
           'history-side-bar, history-toolbar, history-list')) {
    ;(el as unknown as {requestUpdate?: () => void}).requestUpdate?.()
  }
}

// Subscribe directly to the Mojo notification so the brave://history UI reacts
// even when the side bar isn't rendered.
getBraveHistoryEmbeddingsBrowserProxy()
    .callbackRouter.onEnabledChanged.addListener(
        onHistoryEmbeddingsEnabledChanged)
// </if>

injectStyle(HistoryAppElement, css`
        #tabs-content, #tabs-content>* {
            height: 100%;
        }
    .wry-native-history { height: 100%; }
    .wry-native-history[hidden] { display: none !important; }

`)

// Retain Chromium's query controller, URL routing and synced-device view.
// The timeline consumes their results without a second history query pipeline.
type TimelineState = {queryResult_: QueryResult, queryState_: QueryState, selectedPage_: string}
const timelineEntries = new WeakMap<HistoryAppElement, HistoryEntry[]>()
const queryPrototype = HistoryAppElement.prototype as unknown as {
  onQueryFinished_(event: CustomEvent<{result: QueryResult}>): void
}
const originalQueryFinished = queryPrototype.onQueryFinished_
queryPrototype.onQueryFinished_ = function(event) {
  const app = this as unknown as HistoryAppElement
  const query = event.currentTarget as HistoryQueryManagerElement
  if (event.detail.result.info) {
    // The first Lit listener can be installed before this override runs.
    const previous = query.queryState.incremental
      ? timelineEntries.get(app) || (app as unknown as TimelineState).queryResult_.value
      : []
    timelineEntries.set(app, previous.concat(event.detail.result.value))
  }
  originalQueryFinished.call(this, event)
}
const originalRender = HistoryAppElement.prototype.render
HistoryAppElement.prototype.render = function() {
  const state = this as unknown as TimelineState
  const timeline = state.selectedPage_ === 'history'
  return html`<div class="wry-native-history" ?hidden=${timeline} ?inert=${timeline}>${originalRender.call(this)}</div>
    ${timeline ? html`<wryling-history .entries=${timelineEntries.get(this) || state.queryResult_.value}
      .term=${state.queryState_.searchTerm} .querying=${state.queryState_.querying}
      .finished=${state.queryResult_.info?.finished ?? true}></wryling-history>` : null}`
}
const keyboard = HistoryAppElement.prototype as unknown as {onKeyDown_(event: KeyboardEvent): void}
const originalKeyDown = keyboard.onKeyDown_
keyboard.onKeyDown_ = function(event) {
  if ((this as unknown as TimelineState).selectedPage_ !== 'history') originalKeyDown.call(this, event)
}
const originalFind = HistoryAppElement.prototype.handleFindShortcut
HistoryAppElement.prototype.handleFindShortcut = function(modalContextOpen) {
  if ((this as unknown as TimelineState).selectedPage_ !== 'history') return originalFind.call(this, modalContextOpen)
  if (modalContextOpen) return false
  this.shadowRoot?.querySelector('wryling-history')?.shadowRoot?.querySelector('input')?.focus()
  return true
}
const originalFocus = HistoryAppElement.prototype.searchInputHasFocus
HistoryAppElement.prototype.searchInputHasFocus = function() {
  if ((this as unknown as TimelineState).selectedPage_ !== 'history') return originalFocus.call(this)
  return !!this.shadowRoot?.querySelector('wryling-history')?.shadowRoot?.querySelector('input:focus')
}

export * from './app-chromium.js'
