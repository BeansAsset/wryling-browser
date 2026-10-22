#!/usr/bin/env python3
"""Mirror build inputs and return verified output from the Deck's WebUI worker."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import uuid


def run(command, **kwargs):
    return subprocess.run(command, check=True, **kwargs)


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1048576), b''):
            result.update(chunk)
    return result.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version')
    parser.add_argument('compiler_args', nargs=argparse.REMAINDER)
    parsed = parser.parse_args()
    version, args = parsed.version, parsed.compiler_args
    if not re.fullmatch(r'v[0-9]+\.[0-9]+\.[0-9]+', version):
        raise RuntimeError('Invalid Node version')
    options = dict(arg[2:].split('=', 1) for arg in args
                   if arg.startswith('--') and '=' in arg)
    resource = options.get('resource_name', '')
    if not re.fullmatch(r'[a-z][a-z0-9_]*', resource):
        raise RuntimeError('Invalid opted-in resource name')
    if options.get('output_dir') != 'gen/brave/web-ui-' + resource:
        raise RuntimeError('Unexpected resource output directory')
    if options.get('grd_path') != options['output_dir'] + '/' + resource + '.grd':
        raise RuntimeError('Unexpected resource GRD path')
    source = Path(__file__).resolve().parents[3]
    cwd = Path.cwd().resolve()
    if cwd != source / 'out/Wryling':
        raise RuntimeError('Unexpected build working directory')
    host = os.environ['WRYLING_DECK_HOST']
    root = os.environ.get('WRYLING_DECK_BUILD_ROOT', '/home/deck/wryling-webui-build')
    if not root.startswith('/home/deck/') or '..' in Path(root).parts:
        raise RuntimeError('Unexpected remote workspace')
    ssh = ['ssh', '-i', os.environ['WRYLING_DECK_SSH_KEY'],
           '-o', 'UserKnownHostsFile=' + os.environ['WRYLING_DECK_KNOWN_HOSTS'],
           '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10',
           '-o', 'ServerAliveInterval=15', '-o', 'ServerAliveCountMax=3']
    rsync = ['rsync', '-az', '--safe-links', '-e', shlex.join(ssh)]
    remote_run = root + '/runs/' + resource + '-' + uuid.uuid4().hex
    remote_source = remote_run + '/src'
    remote_cwd = remote_source + '/out/Wryling'
    files = set()

    def add(path):
        path = path.resolve()
        relative = path.relative_to(source)
        if path.is_file() and 'node_modules' not in relative.parts:
            if path.name.startswith('.env'):
                raise RuntimeError('Environment files are not build-transfer inputs')
            files.add(str(relative))
            if path.suffix == '.js':
                add(path.with_suffix('.d.ts'))
                add(path.with_suffix('.ts'))

    import_paths = json.loads((cwd / options['import_paths_file']).read_text())
    folders = {'brave/build/webpack', 'brave/components/definitions',
               'brave/components/brave_extension/extension/brave_extension/types'}
    for name in import_paths:
        if not name.startswith('//brave/') or '..' in Path(name).parts:
            raise RuntimeError('Unexpected source import root: ' + name)
        folders.add(name[2:])
    for folder in sorted(folders):
        for path in (source / folder).rglob('*'):
            if path.is_file():
                add(path)
    for name in ['package.json', 'brave/package.json', 'brave/tsconfig.json',
                 'brave/tsconfig-webpack.json', 'out/Wryling/gen/tsconfig-webpack.json',
                 'out/Wryling/gen/brave/build_flags.json']:
        add(source / name)
    for folder in ['out/Wryling/gen/ui/webui', 'out/Wryling/gen/brave/components',
                   'out/Wryling/gen/brave/ui/webui', 'out/Wryling/gen/components/grit',
                   'out/Wryling/gen/components/omnibox']:
        for path in (source / folder).rglob('*'):
            if path.is_file() and path.suffix in {'.ts', '.js', '.json', '.mjs'}:
                add(path)
    depfile = cwd / options['depfile_path']
    if depfile.exists():
        for name in shlex.split(depfile.read_text().replace('\\\n', ' '))[1:]:
            add(cwd / name)
    previous_types = Path('/private/tmp/wryling-remote-ts-files.txt')
    if previous_types.exists():
        for name in previous_types.read_text().splitlines():
            path = source / name
            if path.is_file():
                add(path)
    add(cwd / options['import_paths_file'])
    snapshot = {name: ((source / name).stat().st_size,
                       (source / name).stat().st_mtime_ns) for name in files}
    print('Deck WebUI: syncing dependency cache and %d source/generated inputs' % len(files), flush=True)
    setup = ('from pathlib import Path; import subprocess; '
             'root=Path(' + repr(root) + '); target=Path(' + repr(remote_source) + '); '
             '(root/"deps/brave").mkdir(parents=True,exist_ok=True); '
             '(target/"brave").mkdir(parents=True,exist_ok=True); '
             '(target/"out/Wryling").mkdir(parents=True,exist_ok=True)')
    run(ssh + [host, shlex.join(['python3', '-c', setup])])
    run(rsync + [str(source / 'brave/node_modules') + '/', host + ':' + root + '/deps/brave/node_modules/'])
    link = ('import subprocess; subprocess.run(["cp","-al",' +
            repr(root + '/deps/brave/node_modules') + ',' +
            repr(remote_source + '/brave/node_modules') + '],check=True)')
    run(ssh + [host, shlex.join(['python3', '-c', link])])
    manifest_fd, manifest_name = tempfile.mkstemp(prefix='wryling-webpack-inputs-', suffix='.txt')
    os.close(manifest_fd)
    manifest = Path(manifest_name)
    manifest.write_bytes(b'\0'.join(name.encode() for name in sorted(files)) + b'\0')
    run(rsync + ['--from0', '--files-from=' + str(manifest), str(source) + '/', host + ':' + remote_source + '/'])
    worker = remote_source + '/brave/build/webpack/wryling-webpack-worker.py'
    request = {'root': root, 'run': remote_run, 'version': version, 'args': args}
    print('Deck WebUI: compiling with 6GiB RAM / 3 CPU cores; local compilation disabled', flush=True)
    run(ssh + [host, shlex.join(['python3', worker])], input=json.dumps(request), text=True)
    for name, metadata in snapshot.items():
        path = source / name
        if not path.exists() or (path.stat().st_size, path.stat().st_mtime_ns) != metadata:
            raise RuntimeError('Build input changed during remote compilation: ' + name)
    metadata = subprocess.check_output(ssh + [host, shlex.join(['cat', remote_run + '/outputs.json'])], text=True)
    expected = json.loads(metadata)
    output = cwd / options['output_dir']
    output.mkdir(parents=True, exist_ok=True)
    run(rsync + [host + ':' + remote_cwd + '/' + options['output_dir'] + '/', str(output) + '/'])
    run(rsync + [host + ':' + remote_cwd + '/' + options['depfile_path'], str(depfile)])
    for name, checksum in expected.items():
        path = (source / name).resolve()
        path.relative_to(source / 'out/Wryling/gen')
        if not path.is_file() or digest(path) != checksum:
            raise RuntimeError('Remote output hash mismatch: ' + name)
    print('Deck WebUI: imported %d compiler outputs; hashes verified' % len(expected), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
