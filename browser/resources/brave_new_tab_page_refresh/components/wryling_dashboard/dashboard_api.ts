/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import * as React from 'react'
import * as mojom from 'gen/brave/components/wryling_dashboard/wryling_dashboard.mojom.m.js'

export type DashboardSite = mojom.Site
export type Dashboard = mojom.Dashboard

let handler: mojom.PageHandlerRemote | null = null
const callbackRouter = new mojom.PageCallbackRouter()

// The dashboard API: one connection per page.
export function getDashboardHandler() {
  if (!handler) {
    handler = mojom.PageHandler.getRemote()
    handler.setPage(callbackRouter.$.bindNewPipeAndPassRemote())
  }
  return handler
}

// The dashboard of the space this tab is in, fetched again whenever it (or
// the tab's space) changes, and when the tab is shown.
export function useDashboard(): Dashboard | null {
  const [dashboard, setDashboard] = React.useState<Dashboard | null>(null)

  React.useEffect(() => {
    let current = 0
    const fetch = async () => {
      const request = ++current
      const { dashboard } = await getDashboardHandler().getDashboard()
      if (request === current) {
        setDashboard(dashboard)
      }
    }
    void fetch()
    const listener = callbackRouter.onDashboardChanged.addListener(() => {
      void fetch()
    })
    const onVisibility = () => {
      if (document.visibilityState === 'visible') {
        void fetch()
      }
    }
    document.addEventListener('visibilitychange', onVisibility)
    return () => {
      callbackRouter.removeListener(listener)
      document.removeEventListener('visibilitychange', onVisibility)
    }
  }, [])

  return dashboard
}
