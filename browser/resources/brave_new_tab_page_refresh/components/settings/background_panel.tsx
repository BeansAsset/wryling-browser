/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import * as React from 'react'
import Icon from '@brave/leo/react/icon'
import ProgressRing from '@brave/leo/react/progressRing'
import Toggle from '@brave/leo/react/toggle'

import {
  useBackgroundState,
  useBackgroundActions,
} from '../../context/background_context'
import { useRewardsState } from '../../context/rewards_context'
import { getString } from '../../lib/strings'
import { inlineCSSVars } from '../../lib/inline_css_vars'
import { BackgroundTypePanel } from './background_type_panel'
import { SettingsPanel } from './settings_panel'
import { Link } from '../common/link'
import { formatString } from '$web-common/formatString'
import classnames from '$web-common/classnames'
import { settingsURL } from '../../../../../components/brave_rewards/resources/shared/lib/rewards_urls'

import {
  SelectedBackgroundType,
  backgroundCSSValue,
  gradientBackgrounds,
  solidPreviewBackground,
} from '../../state/background_store'

import {
  findWallpaper,
  isWallpaperValue,
  wallpaperValue,
  wallpapers,
} from '../../lib/wryling_wallpapers'
import {
  WallpaperCanvas,
  useDarkMode,
  useWallpaperColor,
} from '../wryling_dashboard/wallpaper'

import { style } from './background_panel.style'

const sponsoredImageLearnMoreURL =
  'https://support.brave.app/hc/en-us/articles/35182999599501'

export function BackgroundPanel() {
  const actions = useBackgroundActions()

  const backgroundsEnabled = useBackgroundState((s) => s.backgroundsEnabled)
  const backgroundsCustomizable = useBackgroundState(
    (s) => s.backgroundsCustomizable,
  )
  const sponsoredImagesEnabled = useBackgroundState(
    (s) => s.sponsoredImagesEnabled,
  )
  const selectedBackground = useBackgroundState((s) => s.selectedBackground)
  const braveBackgrounds = useBackgroundState((s) => s.braveBackgrounds)
  const customBackgrounds = useBackgroundState((s) => s.customBackgrounds)
  const rewardsFeatureEnabled = useRewardsState((s) => s.rewardsFeatureEnabled)
  const rewardsEnabled = useRewardsState((s) => s.rewardsEnabled)

  const [panelType, setPanelType] =
    React.useState<SelectedBackgroundType | null>(null)
  // Wryling: the list of Wryling wallpapers.
  const [wallpaperPanel, setWallpaperPanel] = React.useState(false)
  const wallpaperColor = useWallpaperColor()
  const dark = useDarkMode()
  const wallpaperSelected =
    selectedBackground.type === SelectedBackgroundType.kSolid
    && isWallpaperValue(selectedBackground.value)
  const [uploading, setUploading] = React.useState(false)

  React.useEffect(() => {
    setUploading(false)
  }, [selectedBackground, customBackgrounds])

  function getTypePreviewValue(type: SelectedBackgroundType) {
    const isSelectedType = type === selectedBackground.type
    switch (type) {
      case SelectedBackgroundType.kBrave:
        return braveBackgrounds[0]?.imageUrl ?? ''
      case SelectedBackgroundType.kCustom:
        if (isSelectedType && selectedBackground.value) {
          return selectedBackground.value
        }
        return customBackgrounds[0] ?? ''
      case SelectedBackgroundType.kSolid:
        if (isSelectedType && selectedBackground.value && !wallpaperSelected) {
          return selectedBackground.value
        }
        return solidPreviewBackground
      case SelectedBackgroundType.kGradient:
        if (isSelectedType && selectedBackground.value) {
          return selectedBackground.value
        }
        // Wryling: a real gradient (the first one follows the theme and
        // previews as near-black).
        return gradientBackgrounds[1]
      default:
        console.error('Unhandled background type', type)
        return ''
    }
  }

  function renderUploadPreview() {
    return (
      <div className='preview upload'>
        {uploading ? <ProgressRing /> : <Icon name='upload' />}
        {getString(S.NEW_TAB_UPLOAD_BACKGROUND_LABEL)}
      </div>
    )
  }

  function renderTypePreview(type: SelectedBackgroundType) {
    if (
      type === SelectedBackgroundType.kCustom
      && customBackgrounds.length === 0
    ) {
      return renderUploadPreview()
    }
    const selected = type === selectedBackground.type
      && !(type === SelectedBackgroundType.kSolid && wallpaperSelected)
    return (
      <div
        className={classnames({ preview: true, selected })}
        style={inlineCSSVars({
          '--preview-background': backgroundCSSValue(
            type,
            getTypePreviewValue(type),
          ),
        })}
      >
        {selected && (
          <span className='selected-marker'>
            <Icon name='check-normal' />
          </span>
        )}
      </div>
    )
  }

  function showCustomBackgroundChooser() {
    actions.showCustomBackgroundChooser().then((backgroundSelected) => {
      if (backgroundSelected) {
        setUploading(true)
      }
    })
  }

  function onCustomPreviewClick() {
    if (customBackgrounds.length === 0) {
      showCustomBackgroundChooser()
    } else {
      setPanelType(SelectedBackgroundType.kCustom)
    }
  }

  function subPanelTitle(type: SelectedBackgroundType) {
    switch (type) {
      case SelectedBackgroundType.kCustom:
        return getString(S.NEW_TAB_CUSTOM_BACKGROUND_LABEL)
      case SelectedBackgroundType.kGradient:
        return getString(S.NEW_TAB_GRADIENT_BACKGROUND_LABEL)
      case SelectedBackgroundType.kSolid:
        return getString(S.NEW_TAB_SOLID_BACKGROUND_LABEL)
      default:
        return ''
    }
  }

  function renderWallpaperPreview(value: string, selected: boolean) {
    return (
      <div className={classnames({ preview: true, wallpaper: true, selected })}>
        <WallpaperCanvas
          wallpaper={findWallpaper(value)}
          color={wallpaperColor}
          dark={dark}
        />
        {selected && (
          <span className='selected-marker'>
            <Icon name='check-normal' />
          </span>
        )}
      </div>
    )
  }

  if (wallpaperPanel) {
    return (
      <SettingsPanel
        cssScope={style.scope}
        title='Wryling'
        onBack={() => setWallpaperPanel(false)}
      >
        <div className='background-options'>
          {wallpapers.map((wallpaper) => {
            const value = wallpaperValue(wallpaper.id)
            const selected =
              wallpaperSelected && selectedBackground.value === value
            return (
              <div
                key={wallpaper.id}
                className='background-option'
              >
                <button
                  onClick={() => {
                    actions.selectBackground(
                      SelectedBackgroundType.kSolid,
                      value,
                    )
                  }}
                >
                  {renderWallpaperPreview(value, selected)}
                  {wallpaper.name}
                </button>
              </div>
            )
          })}
        </div>
      </SettingsPanel>
    )
  }

  if (panelType !== null) {
    return (
      <SettingsPanel
        cssScope={style.scope}
        title={subPanelTitle(panelType)}
        onBack={() => setPanelType(null)}
      >
        <BackgroundTypePanel
          backgroundType={panelType}
          renderUploadOption={() => (
            <button onClick={showCustomBackgroundChooser}>
              {renderUploadPreview()}
            </button>
          )}
        />
      </SettingsPanel>
    )
  }

  return (
    <SettingsPanel
      cssScope={style.scope}
      title={getString(S.NEW_TAB_BACKGROUND_SETTINGS_TITLE)}
    >
      <Toggle
        className='toggle-row'
        size='small'
        checked={backgroundsEnabled}
        onChange={({ checked }) => {
          actions.setBackgroundsEnabled(checked)
        }}
      >
        <span className='label'>
          {getString(S.NEW_TAB_SHOW_BACKGROUNDS_LABEL)}
        </span>
      </Toggle>
      {backgroundsEnabled && rewardsFeatureEnabled && (
        <Toggle
          className='toggle-row'
          size='small'
          checked={sponsoredImagesEnabled}
          onChange={({ checked }) => {
            actions.setSponsoredImagesEnabled(checked)
          }}
        >
          <div className='label'>
            <div>
              {getString(S.NEW_TAB_SHOW_SPONSORED_IMAGES_LABEL)}
              <div
                className='subtext'
                onClick={(e) => e.stopPropagation()}
              >
                {!rewardsEnabled
                  && formatString(
                    getString(S.NEW_TAB_SHOW_SPONSORED_IMAGES_EARNING_TEXT),
                    {
                      $1: (content) => (
                        <Link
                          url={settingsURL}
                          openInNewTab
                        >
                          {content}
                        </Link>
                      ),
                      $2: (content) => (
                        <Link
                          url={sponsoredImageLearnMoreURL}
                          openInNewTab
                        >
                          {content}
                        </Link>
                      ),
                    },
                  )}
              </div>
            </div>
          </div>
        </Toggle>
      )}
      {backgroundsEnabled && backgroundsCustomizable && (
        <>
          <div className='background-options'>
            <div className='background-option'>
              <button onClick={() => setWallpaperPanel(true)}>
                {renderWallpaperPreview(
                  wallpaperSelected
                    ? selectedBackground.value
                    : wallpaperValue('aurora'),
                  wallpaperSelected,
                )}
                Wryling
              </button>
            </div>
            <div className='background-option'>
              <button
                onClick={() => {
                  actions.selectBackground(SelectedBackgroundType.kBrave, '')
                }}
              >
                {renderTypePreview(SelectedBackgroundType.kBrave)}
                {getString(S.NEW_TAB_BRAVE_BACKGROUND_LABEL)}
              </button>
            </div>
            <div className='background-option'>
              <button onClick={onCustomPreviewClick}>
                {renderTypePreview(SelectedBackgroundType.kCustom)}
                {getString(S.NEW_TAB_CUSTOM_BACKGROUND_LABEL)}
              </button>
            </div>
            <div className='background-option'>
              <button
                onClick={() => setPanelType(SelectedBackgroundType.kSolid)}
              >
                {renderTypePreview(SelectedBackgroundType.kSolid)}
                {getString(S.NEW_TAB_SOLID_BACKGROUND_LABEL)}
              </button>
            </div>
            <div className='background-option'>
              <button
                onClick={() => setPanelType(SelectedBackgroundType.kGradient)}
              >
                {renderTypePreview(SelectedBackgroundType.kGradient)}
                {getString(S.NEW_TAB_GRADIENT_BACKGROUND_LABEL)}
              </button>
            </div>
          </div>
        </>
      )}
    </SettingsPanel>
  )
}
