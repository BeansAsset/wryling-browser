/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import {
  Wallpaper,
  WallpaperColor,
  colorFromARGB,
  colorFromRGB,
  drawWallpaper,
} from '../../lib/wryling_wallpapers'
import { useDashboard } from './dashboard_api'

const darkQuery = '(prefers-color-scheme: dark)'

export function useDarkMode() {
  const [dark, setDark] = React.useState(() => matchMedia(darkQuery).matches)
  React.useEffect(() => {
    const query = matchMedia(darkQuery)
    const onChange = () => setDark(query.matches)
    query.addEventListener('change', onChange)
    return () => query.removeEventListener('change', onChange)
  }, [])
  return dark
}

// The theme colour, from the Wryling accent token, for spaces without their
// own colour.
function themeColor(): WallpaperColor {
  const probe = document.createElement('span')
  probe.style.color = 'var(--wry-accent, #7b61d9)'
  probe.style.display = 'none'
  document.body.appendChild(probe)
  const rgb = getComputedStyle(probe).color.match(/[\d.]+/g)?.map(Number)
  probe.remove()
  return rgb && rgb.length >= 3
    ? colorFromRGB(rgb[0], rgb[1], rgb[2])
    : { hue: 292, grey: false }
}

// The colour of the space this tab is in.
export function useWallpaperColor(): WallpaperColor {
  const dashboard = useDashboard()
  const [theme, setTheme] = React.useState(themeColor)
  React.useEffect(() => {
    // The theme's colours arrive (and change) through the colour listener.
    const observer = new MutationObserver(() => setTheme(themeColor()))
    observer.observe(document.documentElement, {
      attributes: true,
      attributeFilter: ['style', 'class'],
    })
    const timer = setTimeout(() => setTheme(themeColor()), 500)
    return () => {
      observer.disconnect()
      clearTimeout(timer)
    }
  }, [])
  const own = dashboard ? colorFromARGB(dashboard.spaceColor) : null
  return own ?? theme
}

interface Props {
  wallpaper: Wallpaper
  color: WallpaperColor
  dark: boolean
  className?: string
  // Wallpapers are soft; drawing at screen resolution (not Retina) is
  // enough and keeps Contours quick.
  maxScale?: number
}

export function WallpaperCanvas(props: Props) {
  const ref = React.useRef<HTMLCanvasElement>(null)
  const { wallpaper, color, dark } = props

  React.useEffect(() => {
    const canvas = ref.current
    if (!canvas) {
      return
    }
    let frame = 0
    const draw = () => {
      cancelAnimationFrame(frame)
      frame = requestAnimationFrame(() =>
        drawWallpaper(canvas, wallpaper, color, dark, props.maxScale ?? 1))
    }
    draw()
    const observer = new ResizeObserver(draw)
    observer.observe(canvas)
    return () => {
      cancelAnimationFrame(frame)
      observer.disconnect()
    }
  }, [wallpaper, color.hue, color.grey, dark, props.maxScale])

  return (
    <canvas
      ref={ref}
      className={props.className}
      aria-hidden='true'
      style={{ display: 'block', width: '100%', height: '100%' }}
    />
  )
}
