/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { scoped } from '$web-common/scoped_css'

// Wryling theme tokens (the --wry-* variables), with fallbacks for
// the moment before the theme's variables arrive.
const text = 'var(--wry-text, light-dark(#1e1b25, #f4f2f8))'
const textSecondary =
  'var(--wry-text-secondary, light-dark(rgba(30,27,37,.66), rgba(244,242,248,.68)))'
const textTertiary =
  'var(--wry-text-tertiary, light-dark(rgba(30,27,37,.5), rgba(244,242,248,.48)))'
const stroke = 'var(--wry-stroke, light-dark(rgba(0,0,0,.09), rgba(255,255,255,.09)))'
const fill = 'var(--wry-fill, light-dark(rgba(0,0,0,.05), rgba(255,255,255,.07)))'
const hover = 'var(--wry-hover, light-dark(rgba(0,0,0,.04), rgba(255,255,255,.06)))'
const surface = 'var(--wry-surface, light-dark(#fff, #1e1a29))'
const raised = 'var(--wry-raised, light-dark(#fff, #262133))'
const accent = 'var(--wry-accent, #6f52dc)'
const onAccent = 'var(--wry-on-accent, #fff)'
const accentText = 'var(--wry-accent-text, light-dark(#5b3fc4, #b9a8f7))'
const destructive = 'var(--wry-destructive, light-dark(#d70015, #ff453a))'
const glass =
  'var(--wry-web-glass, light-dark(rgba(255,255,255,.72), rgba(30,26,41,.72)))'
const glassFilter = 'var(--wry-web-glass-filter, blur(24px) saturate(1.4))'
const shadow =
  'var(--wry-shadow, 0 8px 24px rgba(0,0,0,.28), 0 1px 2px rgba(0,0,0,.2))'
const focusRing = `0 0 0 1px transparent, 0 0 0 4px color-mix(in srgb, ${accent} 50%, transparent)`

export const style = scoped.css`
  & {
    color-scheme: light dark;
    width: 100%;
    display: flex;
    justify-content: center;
    font: 13px/18px system-ui, -apple-system, sans-serif;
    color: ${text};
  }

  &.dashboard-placeholder {
    height: 280px;
  }

  /* The shared Wryling component sheet styles every button (fill, padding,
     min-height); the dashboard's buttons are drawn here instead. */
  button {
    min-height: 0 !important;
    padding: 0 !important;
    border: 0 !important;
    background: transparent !important;
    font: inherit !important;
    color: inherit;
  }

  .card {
    box-sizing: border-box;
    width: min(820px, 100%);
    padding: 24px 28px 22px;
    border-radius: 18px;
    background: ${glass};
    backdrop-filter: ${glassFilter};
    border: 1px solid ${stroke};
    transition: box-shadow 120ms ease-out, border-color 120ms ease-out;

    &.drop-target {
      border-color: ${accent};
      box-shadow: ${focusRing};
    }

    @media (prefers-reduced-transparency: reduce) {
      background: ${raised};
      backdrop-filter: none;
    }
  }

  header {
    display: flex;
    align-items: flex-end;
    gap: 16px;
    padding: 0 6px 20px;
    border-bottom: 1px solid ${stroke};

    .time {
      font-size: 64px;
      font-weight: 200;
      line-height: 1;
      letter-spacing: -2px;
      font-variant-numeric: tabular-nums;

      .day-period {
        font-size: 15px;
        font-weight: 500;
        letter-spacing: 0;
        margin-inline-start: 6px;
      }
    }

    .date {
      flex: 1 1 auto;
      font-size: 15px;
      color: ${textSecondary};
      padding-bottom: 7px;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    .space {
      display: flex;
      align-items: center;
      gap: 6px;
      color: ${textSecondary};
      padding-bottom: 7px;
      white-space: nowrap;
    }

    .space-dot {
      width: 8px;
      height: 8px;
      border-radius: 4px;
    }

    .edit-button,
    .done-button {
      height: 28px;
      padding: 0 12px !important;
      margin-bottom: 2px;
      border-radius: 10px;
      font-size: 13px;
      transition: background 120ms ease-out, opacity 120ms ease-out;

      &:focus-visible {
        outline: none;
        box-shadow: ${focusRing};
      }
    }

    .edit-button {
      color: ${accentText};
      opacity: 0;

      &:hover {
        background: ${hover} !important;
      }
    }

    .done-button {
      background: ${accent} !important;
      color: ${onAccent};
      font-weight: 500 !important;
    }
  }

  .card:hover .edit-button,
  .edit-button:focus-visible {
    opacity: 1;
  }

  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(92px, 1fr));
    gap: 20px 4px;
    margin-top: 24px;
  }

  .tile {
    position: relative;
    display: flex;
    justify-content: center;

    > a,
    &.add > button {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 8px;
      width: 92px;
      padding: 2px 0 !important;
      color: inherit;
      text-decoration: none;
      border-radius: 14px;
      outline: none;
      -webkit-user-drag: element;
    }

    .icon {
      position: relative;
      width: 60px;
      height: 60px;
      border-radius: 15px;
      overflow: hidden;
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 1px 2px light-dark(rgba(0, 0, 0, .14), rgba(0, 0, 0, .45));
      transition: transform 120ms ease-out, box-shadow 120ms ease-out;
    }

    .app-icon {
      background: #fff;

      img {
        width: 100%;
        height: 100%;
        object-fit: cover;
      }
    }

    .small-icon {
      background: ${surface};

      img {
        width: 32px;
        height: 32px;
      }
    }

    .letter-icon {
      color: #fff;
      font-size: 26px;
      font-weight: 600;
    }

    .add-icon {
      box-sizing: border-box;
      border: 1.5px dashed ${textTertiary};
      color: ${textTertiary};
      box-shadow: none;
    }

    .label {
      max-width: 88px;
      font-size: 12px;
      line-height: 15px;
      color: ${textSecondary};
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    > a:hover .icon,
    &.add > button:hover .icon {
      transform: translateY(-2px);
      box-shadow: 0 6px 14px light-dark(rgba(0, 0, 0, .16), rgba(0, 0, 0, .5));
    }

    > a:hover .label,
    &.add > button:hover .label {
      color: ${text};
    }

    > a:focus-visible .icon,
    &.add > button:focus-visible .icon {
      box-shadow: ${focusRing};
    }

    &.add button:hover .add-icon {
      border-color: ${textSecondary};
      color: ${textSecondary};
      box-shadow: none;
    }

    &.dragging {
      opacity: 0.35;
    }
  }

  /* iOS-style delete badge: a small red circle on the icon's top-left
     corner, half outside it. */
  .remove-badge {
    position: absolute;
    top: -5px;
    inset-inline-start: calc(50% - 37px);
    z-index: 1;
    width: 20px;
    height: 20px;
    padding: 0 !important;
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    background: ${destructive} !important;
    color: #fff !important;
    border: 0 !important;
    box-shadow: 0 1px 3px rgba(0, 0, 0, .3), 0 0 0 1.5px light-dark(#fff, rgba(0, 0, 0, .35));
    transition: transform 120ms ease-out;

    &:hover {
      transform: scale(1.1);
    }

    &:focus-visible {
      outline: none;
      box-shadow: ${focusRing};
    }
  }

  @keyframes wiggle {
    0%, 100% { transform: rotate(-1.4deg); }
    50% { transform: rotate(1.4deg); }
  }

  .editing .tile:not(.add):not(.dragging) .icon {
    animation: wiggle 260ms ease-in-out infinite;
    animation-delay: calc(var(--tile-index) * -47ms);

    @media (prefers-reduced-motion: reduce) {
      animation: none;
    }
  }

  .footnote {
    margin: 20px 6px 0;
    font-size: 12px;
    line-height: 16px;
    color: ${textTertiary};
  }

  .empty {
    display: flex;
    flex-direction: column;
    align-items: center;
    text-align: center;
    padding: 40px 12px 24px;

    svg {
      color: ${textTertiary};
    }

    h3 {
      margin: 14px 0 0;
      font-size: 13px;
      line-height: 16px;
      font-weight: 600;
    }

    p {
      margin: 4px 0 0;
      color: ${textSecondary};
    }

    .primary {
      margin-top: 16px;
    }
  }

  .primary,
  .quiet {
    height: 28px;
    padding: 0 14px !important;
    border-radius: 10px;
    font-size: 13px;
    transition: filter 120ms ease-out;

    &:focus-visible {
      outline: none;
      box-shadow: ${focusRing};
    }
  }

  .primary {
    background: ${accent} !important;
    color: ${onAccent};
    font-weight: 500 !important;

    &:hover {
      filter: brightness(1.08);
    }
  }

  .quiet {
    background: ${fill} !important;
    color: ${text};

    &:hover {
      background: ${hover} !important;
    }
  }

  .site-editor {
    position: fixed;
    inset: 0;
    margin: auto !important;
    height: fit-content;
    width: 440px;
    max-width: calc(100vw - 32px);
    box-sizing: border-box;
    padding: 20px;
    border-radius: 18px;
    background: ${raised};
    color: ${text};
    border: 1px solid ${stroke};
    box-shadow: ${shadow};

    &::backdrop {
      background: rgba(0, 0, 0, .32);
    }

    &[open] {
      animation: sheet-in 220ms cubic-bezier(.34, 1.3, .64, 1);

      @media (prefers-reduced-motion: reduce) {
        animation: none;
      }
    }

    h2 {
      margin: 0;
      font-size: 15px;
      line-height: 20px;
      font-weight: 600;
    }

    .editor-body {
      display: flex;
      gap: 16px;
      margin-top: 18px;
    }

    .preview .icon {
      width: 60px;
      height: 60px;
      border-radius: 15px;
      overflow: hidden;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 26px;
      font-weight: 600;
      color: #fff;

      &.app-icon {
        background: #fff;
      }

      &.small-icon {
        background: ${surface};
        border: 1px solid ${stroke};
      }

      img {
        width: 100%;
        height: 100%;
        object-fit: cover;
      }

      &.small-icon img {
        width: 32px;
        height: 32px;
      }
    }

    .fields {
      flex: 1 1 auto;
      display: flex;
      flex-direction: column;
      gap: 10px;
    }

    label {
      display: flex;
      flex-direction: column;
      gap: 4px;
      font-size: 12px;
      color: ${textSecondary};
    }

    input {
      height: 28px;
      padding: 0 10px;
      border: 0;
      border-radius: 8px;
      background: ${fill};
      color: ${text};
      font: 13px system-ui, -apple-system, sans-serif;

      &::placeholder {
        color: ${textTertiary};
      }

      &:focus-visible {
        outline: none;
        box-shadow: ${focusRing};
      }
    }

    .error {
      margin: 0;
      font-size: 12px;
      color: ${destructive};
    }

    .actions {
      display: flex;
      justify-content: flex-end;
      gap: 8px;
      margin-top: 20px;
    }
  }

  @keyframes sheet-in {
    from { opacity: 0; transform: scale(.96); }
    to { opacity: 1; transform: none; }
  }

  .site-menu {
    position: fixed;
    inset: auto;
    margin: 0;
    min-width: 200px;
    padding: 5px;
    border-radius: 12px;
    background: ${raised};
    color: ${text};
    border: 1px solid ${stroke};
    box-shadow: ${shadow};

    button {
      display: flex;
      align-items: center;
      width: 100%;
      height: 26px;
      padding: 0 10px !important;
      border-radius: 7px;
      font-size: 13px;
      color: inherit;
      outline: none;

      &:hover,
      &:focus-visible {
        background: ${accent} !important;
        color: ${onAccent} !important;
        outline: none !important;
      }

      &.destructive:not(:hover):not(:focus-visible) {
        color: ${destructive} !important;
      }
    }

    .separator {
      height: 1px;
      margin: 4px 7px;
      background: ${stroke};
    }
  }

  .undo-toast {
    position: fixed;
    inset-block-end: 28px;
    inset-inline-start: 50%;
    transform: translateX(-50%);
    z-index: 5;
    display: flex;
    align-items: center;
    gap: 14px;
    padding: 6px 6px 6px 16px;
    border-radius: 999px;
    background: ${raised};
    border: 1px solid ${stroke};
    box-shadow: ${shadow};
    white-space: nowrap;
    animation: toast-in 200ms ease-out;

    button {
      height: 28px;
      padding: 0 12px !important;
      border-radius: 999px;
      color: ${accentText};
      font-weight: 500 !important;

      &:hover {
        background: ${hover} !important;
      }

      &:focus-visible {
        outline: none;
        box-shadow: ${focusRing};
      }
    }
  }

  @keyframes toast-in {
    from { opacity: 0; transform: translate(-50%, 8px); }
    to { opacity: 1; transform: translateX(-50%); }
  }
`
