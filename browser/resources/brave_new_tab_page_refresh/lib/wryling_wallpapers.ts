/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

// Wryling's own wallpapers. Each is drawn on a canvas from the space's colour
// (so every space has its own version) and the light or dark appearance. The
// background pref stores one as the "colour" type with the value
// "wryling:<id>" (see background.tsx and background_panel.tsx).

export const wallpaperPrefix = 'wryling:'
export const defaultWallpaperId = 'aurora'

export interface WallpaperColor {
  // OKLCH hue in degrees.
  hue: number
  // A near-grey space colour: chroma is kept low so it stays neutral.
  grey: boolean
}

export interface Wallpaper {
  id: string
  name: string
  // False when the colours are fixed (the same in every space).
  followsSpace: boolean
  draw: (
    ctx: CanvasRenderingContext2D,
    w: number,
    h: number,
    color: WallpaperColor,
    dark: boolean,
  ) => void
}

export function isWallpaperValue(value: string | undefined | null) {
  return !!value && value.startsWith(wallpaperPrefix)
}

export function wallpaperValue(id: string) {
  return wallpaperPrefix + id
}

export function findWallpaper(value: string) {
  const id = value.slice(wallpaperPrefix.length)
  return (
    wallpapers.find((w) => w.id === id)
    ?? wallpapers.find((w) => w.id === defaultWallpaperId)!
  )
}

function c(L: number, C: number, h: number, a = 1) {
  return `oklch(${L} ${C} ${((h % 360) + 360) % 360} / ${a})`
}

function radial(
  ctx: CanvasRenderingContext2D,
  x: number,
  y: number,
  r: number,
  stops: Array<[number, string]>,
) {
  const g = ctx.createRadialGradient(x, y, 0, x, y, r)
  for (const [offset, color] of stops) {
    g.addColorStop(offset, color)
  }
  ctx.fillStyle = g
  ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height)
}

const clear = (hue: number) => c(0.5, 0, hue, 0)

export const wallpapers: Wallpaper[] = [
  {
    id: 'aurora',
    name: 'Aurora',
    followsSpace: true,
    draw(ctx, w, h, { hue, grey }, dark) {
      const k = grey ? 0.25 : 1
      ctx.fillStyle = c(dark ? 0.14 : 0.95, 0.03 * k, hue)
      ctx.fillRect(0, 0, w, h)
      ctx.save()
      ctx.filter = `blur(${Math.round(w * 0.075)}px)`
      const blob = (
        x: number,
        y: number,
        rx: number,
        ry: number,
        color: string,
        rotation: number,
      ) => {
        ctx.fillStyle = color
        ctx.beginPath()
        ctx.ellipse(x, y, rx, ry, rotation, 0, Math.PI * 2)
        ctx.fill()
      }
      blob(w * 0.22, h * 0.25, w * 0.32, h * 0.28,
        c(dark ? 0.55 : 0.8, 0.16 * k, hue, dark ? 0.75 : 0.65), 0.4)
      blob(w * 0.72, h * 0.18, w * 0.3, h * 0.22,
        c(dark ? 0.5 : 0.84, 0.14 * k, hue + 48, dark ? 0.6 : 0.55), -0.3)
      blob(w * 0.6, h * 0.85, w * 0.38, h * 0.26,
        c(dark ? 0.45 : 0.86, 0.12 * k, hue - 55, dark ? 0.6 : 0.5), 0.1)
      blob(w * 0.08, h * 0.95, w * 0.22, h * 0.2,
        c(dark ? 0.4 : 0.88, 0.1 * k, hue + 110, 0.45), 0)
      ctx.restore()
    },
  },
  {
    id: 'tint',
    name: 'Tint',
    followsSpace: true,
    draw(ctx, w, h, { hue, grey }, dark) {
      const k = grey ? 0.25 : 1
      ctx.fillStyle = c(dark ? 0.17 : 0.93, 0.05 * k, hue)
      ctx.fillRect(0, 0, w, h)
      radial(ctx, w * 0.18, h * 0.05, w, [
        [0, c(dark ? 0.46 : 0.82, 0.13 * k, hue, 0.95)], [1, clear(hue)]])
      radial(ctx, w * 0.95, h * 1.05, w * 0.75, [
        [0, c(dark ? 0.32 : 0.86, 0.09 * k, hue + 25, 0.8)], [1, clear(hue)]])
    },
  },
  {
    id: 'ribbon',
    name: 'Ribbon',
    followsSpace: true,
    draw(ctx, w, h, { hue, grey }, dark) {
      const k = grey ? 0.25 : 1
      ctx.fillStyle = c(dark ? 0.15 : 0.94, 0.04 * k, hue)
      ctx.fillRect(0, 0, w, h)
      radial(ctx, w * 0.5, h * 0.4, w * 0.8, [
        [0, c(dark ? 0.24 : 0.97, 0.06 * k, hue)], [1, clear(hue)]])
      const lit = (L: number, a = 1) =>
        c(dark ? L : Math.min(0.97, L + 0.38), 0.15 * k, hue, a)
      const band = (
        points: Array<[number, number]>,
        from: [number, number],
        to: [number, number],
        a: string,
        b: string,
      ) => {
        const g = ctx.createLinearGradient(
          from[0] * w, from[1] * h, to[0] * w, to[1] * h)
        g.addColorStop(0, a)
        g.addColorStop(1, b)
        ctx.fillStyle = g
        ctx.beginPath()
        points.forEach(([x, y], i) =>
          i ? ctx.lineTo(x * w, y * h) : ctx.moveTo(x * w, y * h))
        ctx.closePath()
        ctx.fill()
      }
      ctx.save()
      ctx.shadowColor = dark ? 'rgba(0,0,0,.45)' : 'rgba(40,20,80,.18)'
      ctx.shadowBlur = w * 0.04
      band([[-0.1, 0.18], [0.3, 1.1], [0.46, 1.1], [0.06, 0.18]],
        [0, 0.2], [0.4, 1], lit(0.52), lit(0.34))
      band([[0.3, 1.1], [0.58, 0.32], [0.7, 0.32], [0.46, 1.1]],
        [0.4, 1], [0.65, 0.3], lit(0.28), lit(0.46))
      band([[0.58, 0.32], [0.86, 1.1], [1.02, 1.1], [0.7, 0.32]],
        [0.6, 0.3], [0.9, 1], lit(0.6), lit(0.4))
      band([[0.86, 1.1], [1.2, 0.05], [1.32, 0.05], [1.02, 1.1]],
        [0.9, 1], [1.2, 0], lit(0.3), lit(0.44, 0.9))
      ctx.restore()
    },
  },
  {
    id: 'contours',
    name: 'Contours',
    followsSpace: true,
    draw(ctx, w, h, { hue, grey }, dark) {
      const k = grey ? 0.25 : 1
      ctx.fillStyle = c(dark ? 0.16 : 0.95, 0.045 * k, hue)
      ctx.fillRect(0, 0, w, h)
      radial(ctx, w * 0.3, h * 0.3, w * 0.9, [
        [0, c(dark ? 0.28 : 0.9, 0.08 * k, hue, 0.9)], [1, clear(hue)]])
      const image = ctx.getImageData(0, 0, w, h)
      const line = dark ? [255, 255, 255] : [40, 30, 70]
      const width = 0.06 + 0.02 * (w / 1440)
      const strength = dark ? 0.16 : 0.13
      const s = 6 / w
      for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
          const u = x * s
          const v = y * s
          const f = Math.sin(u * 0.9 + Math.sin(v * 0.7) * 1.4)
            + Math.cos(v * 1.1 - u * 0.4) * 0.8
            + Math.sin((u + v) * 0.5) * 0.6
            - Math.hypot(u - 2.2, v - 1.6) * 0.35
          const t = f * 4.5
          const d = Math.abs(t - Math.round(t))
          if (d < width) {
            const a = (1 - d / width) * strength
            const i = (y * w + x) * 4
            image.data[i] += (line[0] - image.data[i]) * a
            image.data[i + 1] += (line[1] - image.data[i + 1]) * a
            image.data[i + 2] += (line[2] - image.data[i + 2]) * a
          }
        }
      }
      ctx.putImageData(image, 0, 0)
    },
  },
  {
    id: 'prism',
    name: 'Prism',
    followsSpace: true,
    draw(ctx, w, h, { hue, grey }, dark) {
      const k = grey ? 0.25 : 1
      ctx.fillStyle = c(dark ? 0.15 : 0.94, 0.05 * k, hue)
      ctx.fillRect(0, 0, w, h)
      radial(ctx, w * 0.85, h * 0.1, w * 0.9, [
        [0, c(dark ? 0.4 : 0.85, 0.12 * k, hue + 20, 0.85)], [1, clear(hue)]])
      ctx.save()
      ctx.translate(w / 2, h / 2)
      ctx.rotate(-0.42)
      ctx.filter = `blur(${Math.round(w * 0.012)}px)`
      const bands = [[-0.55, 0.1, 0.1], [-0.38, 0.05, 0.16], [-0.22, 0.14, 0.07],
        [0.02, 0.06, 0.13], [0.16, 0.16, 0.06], [0.42, 0.08, 0.11]]
      for (const [x, bw, a] of bands) {
        const g = ctx.createLinearGradient(x * w, 0, (x + bw) * w, 0)
        g.addColorStop(0, 'rgba(255,255,255,0)')
        g.addColorStop(0.5, `rgba(255,255,255,${dark ? a : a * 1.8})`)
        g.addColorStop(1, 'rgba(255,255,255,0)')
        ctx.fillStyle = g
        ctx.fillRect(x * w, -h, bw * w, h * 3)
      }
      ctx.restore()
    },
  },
  {
    id: 'halo',
    name: 'Halo',
    followsSpace: true,
    draw(ctx, w, h, { hue, grey }, dark) {
      const k = grey ? 0.25 : 1
      ctx.fillStyle = c(dark ? 0.13 : 0.95, 0.04 * k, hue)
      ctx.fillRect(0, 0, w, h)
      const x = w * 0.78
      const y = h * 0.62
      const r = w * 0.42
      ctx.save()
      ctx.filter = `blur(${Math.round(w * 0.02)}px)`
      const g = ctx.createRadialGradient(x, y, r * 0.55, x, y, r * 1.05)
      g.addColorStop(0, clear(hue))
      g.addColorStop(0.62, c(dark ? 0.62 : 0.8, 0.16 * k, hue, dark ? 0.85 : 0.7))
      g.addColorStop(0.72, c(dark ? 0.82 : 0.92, 0.1 * k, hue + 30, dark ? 0.7 : 0.8))
      g.addColorStop(1, clear(hue))
      ctx.fillStyle = g
      ctx.fillRect(0, 0, w, h)
      ctx.restore()
      radial(ctx, x - r * 0.3, y - r * 0.35, r * 0.6, [
        [0, c(dark ? 0.35 : 0.98, 0.08 * k, hue, 0.5)], [1, clear(hue)]])
    },
  },
  {
    id: 'dusk',
    name: 'Dusk',
    followsSpace: false,
    draw(ctx, w, h, _color, dark) {
      const g = ctx.createLinearGradient(0, 0, 0, h)
      if (dark) {
        g.addColorStop(0, c(0.16, 0.07, 285))
        g.addColorStop(0.55, c(0.3, 0.12, 320))
        g.addColorStop(1, c(0.55, 0.14, 35))
      } else {
        g.addColorStop(0, c(0.9, 0.05, 280))
        g.addColorStop(0.55, c(0.88, 0.07, 340))
        g.addColorStop(1, c(0.9, 0.09, 55))
      }
      ctx.fillStyle = g
      ctx.fillRect(0, 0, w, h)
      radial(ctx, w * 0.2, h * 1.05, w * 0.7, [
        [0, c(dark ? 0.72 : 0.95, 0.14, 50, 0.7)], [1, clear(50)]])
      radial(ctx, w * 0.9, 0, w * 0.5, [
        [0, c(dark ? 0.3 : 0.92, 0.08, 270, 0.6)], [1, clear(270)]])
    },
  },
  {
    id: 'mist',
    name: 'Mist',
    followsSpace: true,
    draw(ctx, w, h, { hue, grey }, dark) {
      const k = grey ? 0.25 : 1
      const g = ctx.createLinearGradient(0, 0, w, h)
      g.addColorStop(0, c(dark ? 0.22 : 0.95, 0.035 * k, hue))
      g.addColorStop(1, c(dark ? 0.12 : 0.89, 0.05 * k, hue + 20))
      ctx.fillStyle = g
      ctx.fillRect(0, 0, w, h)
      radial(ctx, w * 0.5, -h * 0.2, w * 0.9, [
        [0, c(dark ? 0.32 : 0.99, 0.03 * k, hue, 0.6)], [1, clear(hue)]])
    },
  },
]

// The OKLCH hue (and whether it is near grey) of an sRGB colour.
export function colorFromRGB(r: number, g: number, b: number): WallpaperColor {
  const lin = (v: number) => {
    v /= 255
    return v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4)
  }
  const [R, G, B] = [lin(r), lin(g), lin(b)]
  const l = Math.cbrt(0.4122214708 * R + 0.5363325363 * G + 0.0514459929 * B)
  const m = Math.cbrt(0.2119034982 * R + 0.6806995451 * G + 0.1073969566 * B)
  const s = Math.cbrt(0.0883024619 * R + 0.2817188376 * G + 0.6299787005 * B)
  const A = 1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s
  const Bb = 0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s
  const chroma = Math.hypot(A, Bb)
  const hue = ((Math.atan2(Bb, A) * 180) / Math.PI + 360) % 360
  return { hue, grey: chroma < 0.04 }
}

export function colorFromARGB(argb: number): WallpaperColor | null {
  if (!argb) {
    return null
  }
  return colorFromRGB((argb >>> 16) & 0xff, (argb >>> 8) & 0xff, argb & 0xff)
}

// Draws `wallpaper` on `canvas` at the canvas's displayed size.
export function drawWallpaper(
  canvas: HTMLCanvasElement,
  wallpaper: Wallpaper,
  color: WallpaperColor,
  dark: boolean,
  maxScale = 1,
) {
  const rect = canvas.getBoundingClientRect()
  const scale = Math.min(window.devicePixelRatio || 1, maxScale)
  const w = Math.max(1, Math.round(rect.width * scale))
  const h = Math.max(1, Math.round(rect.height * scale))
  canvas.width = w
  canvas.height = h
  const ctx = canvas.getContext('2d')
  if (!ctx) {
    return
  }
  ctx.clearRect(0, 0, w, h)
  wallpaper.draw(ctx, w, h, color, dark)
}
