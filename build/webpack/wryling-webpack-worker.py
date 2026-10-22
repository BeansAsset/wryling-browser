#!/usr/bin/env python3
"""Run an original WebUI compiler command inside a bounded Deck scope."""
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import sys


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1048576), b''):
            result.update(chunk)
    return result.hexdigest()


def main():
    request = json.load(sys.stdin)
    root = Path(request['root']).resolve()
    run = Path(request['run']).resolve()
    run.relative_to(root / 'runs')
    source = run / 'src'
    cwd = source / 'out/Wryling'
    options = dict(arg[2:].split('=', 1) for arg in request['args']
                   if arg.startswith('--') and '=' in arg)
    resource = options.get('resource_name', '')
    if not re.fullmatch(r'[a-z][a-z0-9_]*', resource):
        raise RuntimeError('Invalid opted-in resource name')
    if options.get('output_dir') != 'gen/brave/web-ui-' + resource:
        raise RuntimeError('Unexpected resource output directory')
    if options.get('grd_path') != options['output_dir'] + '/' + resource + '.grd':
        raise RuntimeError('Unexpected resource GRD path')
    output = (cwd / options['output_dir']).resolve()
    output.relative_to(cwd / 'gen/brave')
    output.mkdir(parents=True, exist_ok=True)
    runtime = root / 'tools' / ('node-' + request['version'] + '-linux-x64/bin/node')
    version = subprocess.check_output([str(runtime), '--version'], text=True).strip()
    if version != request['version']:
        raise RuntimeError('Remote Node version does not match Chromium')
    command = [
        'systemd-run', '--user', '--scope', '--quiet',
        '-p', 'MemoryMax=6G', '-p', 'MemorySwapMax=0', '-p', 'CPUQuota=300%',
        'env', 'NODE_OPTIONS=--max-old-space-size=4096', str(runtime),
        str(source / 'brave/build/webpack/transpile-web-ui.ts'), *request['args'],
    ]
    result = subprocess.run(command, cwd=cwd)
    if result.returncode:
        return result.returncode
    grd = (cwd / options['grd_path']).resolve()
    depfile = (cwd / options['depfile_path']).resolve()
    if not grd.is_file() or not depfile.is_file():
        raise RuntimeError('Compiler did not produce its GRD and dependency file')
    files = {}
    for path in output.rglob('*'):
        if path.is_symlink():
            raise RuntimeError('Unexpected symlink in generated resources')
        if path.is_file():
            files[str(path.relative_to(source))] = digest(path)
    files[str(depfile.relative_to(source))] = digest(depfile)
    (run / 'outputs.json').write_text(json.dumps(files, indent=2))
    print('Deck WebUI compiler passed; verified %d generated files' % len(files), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
