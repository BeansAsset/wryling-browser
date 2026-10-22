// Copyright (c) 2026 The Wryling Authors. All rights reserved.
import {spawnSync} from 'node:child_process'
import {fileURLToPath} from 'node:url'

if (process.env.WRYLING_REMOTE_WEBUI === '1') {
  const helper = fileURLToPath(new URL('./wryling-remote-webpack.py', import.meta.url))
  const result = spawnSync('/usr/bin/python3', [helper, process.version, ...process.argv.slice(2)], {stdio: 'inherit'})
  if (result.error) throw result.error
  process.exit(result.status ?? 1)
} else {
  await import('./transpile-web-ui.ts')
}
