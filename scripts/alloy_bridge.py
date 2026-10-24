"""Repository-owned client for Alloy's isolated local development bridge (stdlib only)."""
import argparse
import json
import math
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import time
import uuid

from test_artifacts import linked, safe_root
from verification import bundle_identity, write_json

WORKSPACE = Path(__file__).resolve().parents[1]
PROTOCOL = 1
COMMANDS = ('status', 'import', 'add', 'correct_source', 'transition_preset', 'play', 'stop',
            'seek', 'view', 'panel', 'select_clip', 'select_transition', 'control', 'save',
            'open', 'screenshot', 'undo', 'close', 'timing', 'audio_probe', 'jump_bars', 'move_clip', 'transition_edit', 'insertion_preset')


class BridgeError(RuntimeError):
    pass


def initialise(root):
    """Only a new directory under this repository's .local may become a dev workspace."""
    root = safe_root(Path(root), WORKSPACE)
    root.mkdir(parents=True, exist_ok=False)
    for name in ('control/requests', 'control/responses', 'control/history', 'data/temp',
                 'audio', 'sessions', 'evidence'):
        (root/name).mkdir(parents=True, exist_ok=True)
    (root/'.alloy-promo').write_text('alloy-promo-v1\n', encoding='utf-8')
    (root/'control/token.txt').write_text(secrets.token_hex(32), encoding='utf-8')
    write_json(root/'workspace.json', dict(format='alloy-development-workspace', schemaVersion=1,
                                         root=str(root), created=time.time()))
    return root


def validate_root(root):
    root = safe_root(Path(root), WORKSPACE)
    for relative in ('.alloy-promo', 'workspace.json', 'control/token.txt', 'control/requests',
                     'control/responses', 'control/history', 'data/temp', 'evidence'):
        path = root/relative
        if not path.exists() or any(linked(p) for p in (path, *path.parents) if p.exists()):
            raise BridgeError('Missing or linked development workspace path: ' + relative)
    if (root/'.alloy-promo').read_text().strip() != 'alloy-promo-v1':
        raise BridgeError('This is not an owned Alloy bridge workspace')
    meta = json.loads((root/'workspace.json').read_text())
    if meta.get('format') != 'alloy-development-workspace' or meta.get('root') != str(root):
        raise BridgeError('Workspace identity changed')
    return root


class Client:
    def __init__(self, root):
        self.root = validate_root(root)
        self.token = (self.root/'control/token.txt').read_text().strip()
        if not 32 <= len(self.token) <= 128:
            raise BridgeError('Invalid bridge token length')

    def call(self, command, *, timeout=15, **arguments):
        if not math.isfinite(timeout) or timeout <= 0:
            raise BridgeError('Timeout must be finite and positive before sending a command')
        if command not in COMMANDS:
            raise BridgeError('Unsupported bridge command: ' + command)
        if {'id', 'command', 'token', 'timestamp'} & arguments.keys():
            raise BridgeError('Protocol fields cannot be overridden')
        validate_root(self.root)
        ident = uuid.uuid4().hex
        request = dict(id=ident, command=command, token=self.token,
                       timestamp=time.time()*1000, **arguments)
        encoded = json.dumps(request, allow_nan=False).encode('utf-8')
        if len(encoded) > 65536:
            raise BridgeError('Request exceeds 64 KiB')
        path = self.root/f'control/requests/{ident}.json'
        temporary = path.with_suffix('.tmp')
        temporary.write_bytes(encoded)
        temporary.replace(path)
        # Trace arguments and outcomes, never credentials. Native replies/claims remain intact.
        with (self.root/'evidence/commands.jsonl').open('a', encoding='utf-8') as log:
            log.write(json.dumps(dict(id=ident, command=command, arguments=arguments,
                                      timestamp=request['timestamp']))+'\n')
        reply = self.root/f'control/responses/{ident}.json'
        deadline = time.monotonic()+timeout
        while time.monotonic() < deadline:
            if reply.exists():
                if linked(reply) or reply.stat().st_size > 8*1024*1024:
                    raise BridgeError('Unsafe or oversized bridge response')
                try:
                    result = json.loads(reply.read_text(encoding='utf-8-sig'))
                except json.JSONDecodeError:
                    time.sleep(.02)
                    continue
                if result.get('id') != ident:
                    raise BridgeError('Response ID mismatch')
                if result.get('ok') is not True:
                    raise BridgeError(result.get('error', 'Command failed'))
                state = result.get('state', {})
                if (state.get('protocolVersion') != PROTOCOL
                        or os.path.normcase(state.get('root', '')) != os.path.normcase(str(self.root))):
                    raise BridgeError('Bridge protocol/workspace mismatch; use a current development build')
                return result
            time.sleep(.05)
        raise TimeoutError(f'{command} timed out (request {ident}); outcome may be uncertain. '
                           'Inspect status and the response/claim before issuing a new request. No retry was sent.')

    def ready(self, timeout=120):
        deadline = time.monotonic()+timeout
        while time.monotonic() < deadline:
            state = self.call('status', timeout=min(15, max(.05, deadline-time.monotonic())))['state']
            if not state['busy']:
                return state
            time.sleep(.1)
        raise TimeoutError('Alloy remains busy; inspect its state and saved evidence')


def freeze_bundle(exe, root):
    """Freeze one executable; hard-link canonical runtime/model instead of duplicating them."""
    exe = Path(exe).resolve(strict=True)
    data = exe.read_bytes()
    # Compatibility preflight prevents an old GUI build treating the new flag as a normal launch.
    # This is not signature verification: the caller must choose their trusted local build.
    if not all(s.encode() in data or s.encode('utf-16-le') in data
               for s in ('--dev-control', 'protocolVersion', 'control/history')):
        raise BridgeError('Executable lacks the development bridge; build the current source first')
    identity = bundle_identity(exe)
    bundle = root/'bundle'
    if bundle.exists():
        if linked(bundle) or any(linked(bundle/name) for name in ('Mixworks.exe','onnxruntime.dll','beat-this-final0.onnx')):
            raise BridgeError('Linked bundle paths are not allowed')
        frozen_identity = bundle_identity(bundle/'Mixworks.exe')
        if any(identity[key] != frozen_identity[key] for key in ('executableSha256','modelSha256','runtimeSha256')):
            raise BridgeError('This workspace has a different frozen bundle; initialise a fresh root')
        return bundle/'Mixworks.exe'
    bundle.mkdir(exist_ok=False)
    shutil.copy2(exe, bundle/'Mixworks.exe')
    for name in ('onnxruntime.dll', 'beat-this-final0.onnx'):
        os.link(exe.with_name(name), bundle/name)
    write_json(root/'evidence/bundle.json', dict(**identity, frozen=str(bundle/'Mixworks.exe'),
                                              assetPolicy='same-volume hard links to canonical files'))
    return bundle/'Mixworks.exe'


def launch(root, exe, *, environment=None, log=None, startup=None):
    root = validate_root(root)
    frozen = freeze_bundle(exe, root)
    env = dict(os.environ if environment is None else environment)
    env.update(TEMP=str(root/'data/temp'), TMP=str(root/'data/temp'))
    if startup is None and hasattr(subprocess, 'STARTUPINFO'):
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    process = subprocess.Popen([str(frozen), '--dev-control', str(root)], cwd=frozen.parent,
                               env=env, startupinfo=startup, stdout=log or subprocess.DEVNULL,
                               stderr=subprocess.STDOUT)
    try:
        state = Client(root).call('status', timeout=20)['state']
        if process.poll() is not None or state.get('pid') != process.pid:
            raise BridgeError('A different process owns this workspace; no existing instance was stopped')
        write_json(root/'evidence/process.json', dict(pid=process.pid, executable=str(frozen),
                                                   root=str(root), started=time.time(), protocolVersion=PROTOCOL))
        return process
    except BaseException:
        # Only this exact child handle, never a PID discovered elsewhere or a production instance.
        if process.poll() is None:
            process.terminate()
        process.wait()
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    for name in ('init', 'start', 'call'):
        p = sub.add_parser(name)
        p.add_argument('root', type=Path)
        if name == 'start':
            p.add_argument('--exe', type=Path, required=True)
        if name == 'call':
            p.add_argument('command', choices=COMMANDS)
            p.add_argument('--args', default='{}', help='JSON object; prefer --args-file in PowerShell')
            p.add_argument('--args-file', type=Path)
            p.add_argument('--timeout', type=float, default=15)
    args = parser.parse_args()
    try:
        if args.action == 'init':
            result = dict(root=str(initialise(args.root)))
        elif args.action == 'start':
            result = dict(pid=launch(args.root, args.exe).pid, root=str(args.root.resolve()))
        else:
            values = json.loads(args.args_file.read_text(encoding='utf-8-sig') if args.args_file else args.args)
            if not isinstance(values, dict):
                raise BridgeError('Command arguments must be a JSON object')
            result = Client(args.root).call(args.command, timeout=args.timeout, **values)
        print(json.dumps(result, indent=2))
        return 0
    except (OSError, ValueError, BridgeError, TimeoutError) as error:
        parser.exit(1, f'Bridge: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
