"""Retain compact verification evidence; discard audio from newly owned test runs."""
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import stat
import uuid

MARKER = '.mixworks-test-run.json'
FORMAT = 'mixworks-disposable-test-audio'
BULK = {'.wav', '.aif', '.aiff', '.flac', '.pcm', '.mwpcm', '.f32', '.s16', '.raw'}
PROTECTED = {'source-baselines', 'deps', 'archive'}


def linked(path):
    info = Path(path).lstat()
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, 'st_file_attributes', 0) & 1024)


def safe_root(folder, workspace):
    folder, workspace = Path(folder).absolute(), Path(workspace).resolve()
    local = workspace / '.local'
    if folder == local or not folder.is_relative_to(local) or folder.relative_to(local).parts[0] in PROTECTED:
        raise ValueError('Test retention is restricted to owned runs below this project .local.')
    for parent in (folder, *folder.parents):
        if parent.exists() and linked(parent):
            raise ValueError('Test retention refuses links/reparse points.')
        if parent == workspace:
            break
    resolved = folder.resolve()
    if (resolved == local or not resolved.is_relative_to(local)
            or resolved.relative_to(local).parts[0] in PROTECTED):
        raise ValueError('Resolved test directory escaped .local.')
    return resolved


def begin_run(folder, workspace):
    folder = safe_root(folder, workspace)
    if any(folder.iterdir()):
        raise ValueError('Only a freshly created empty test directory may be owned.')
    owner = dict(format=FORMAT, schemaVersion=1, token=uuid.uuid4().hex,
                 root=str(folder), workspace=str(Path(workspace).resolve()), status='running')
    with (folder / MARKER).open('x', encoding='utf-8') as stream:
        json.dump(owner, stream, indent=2)
    return owner


def finish_run(folder, owner):
    folder = safe_root(folder, owner['workspace'])
    actual = json.loads((folder / MARKER).read_text(encoding='utf-8'))
    if actual != owner or actual['format'] != FORMAT or actual['root'] != str(folder):
        raise ValueError('Test ownership changed; generated audio was kept.')
    manifest = folder / 'test-audio-retention.json'
    if manifest.exists():
        raise ValueError('Keep existing retention evidence; refuse a second cleanup.')
    result = dict(policy='discard-generated-audio-after-process-exit', files=[], bytes=0, errors=[])
    # A journal is written before unlinking each file. Reports, analysis JSON,
    # source archives, images, binaries and original inputs outside the run survive.
    journal = folder / 'test-audio-removals.jsonl'
    with journal.open('x', encoding='utf-8') as log:
        for directory, children, names in os.walk(folder):
            children[:] = [n for n in children if not linked(Path(directory) / n)]
            for name in names:
                path = Path(directory) / name
                generated_session = re.search(r'\.mixworks(?:\(\d+\))?(?:\.bak)?$', name, re.I)
                if path.suffix.lower() not in BULK and not generated_session:
                    continue
                try:
                    if linked(path) or not path.resolve().is_relative_to(folder):
                        continue
                    info = path.stat()
                    if info.st_nlink != 1:
                        continue
                    with path.open('rb') as stream:
                        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
                    again = path.stat()
                    if (info.st_ino, info.st_size, info.st_mtime_ns) != (again.st_ino, again.st_size, again.st_mtime_ns):
                        raise ValueError('File changed during retention hashing.')
                    item = dict(path=str(path.relative_to(folder)), bytes=info.st_size, sha256=digest)
                    log.write(json.dumps(dict(event='planned', **item)) + '\n'); log.flush()
                    path.chmod(info.st_mode | stat.S_IWRITE)
                    path.unlink()
                    log.write(json.dumps(dict(event='removed', **item)) + '\n'); log.flush()
                    result['files'].append(item); result['bytes'] += info.st_size
                except (OSError, ValueError) as error:
                    result['errors'].append(dict(path=str(path), error=str(error)))
    actual['status'] = 'complete'
    (folder / MARKER).write_text(json.dumps(actual, indent=2), encoding='utf-8')
    manifest.write_text(json.dumps(result, indent=2), encoding='utf-8')
    return dict(path=str(manifest), files=len(result['files']), bytes=result['bytes'], errors=result['errors'])


def require_space(path, minimum=6 * 1024**3):
    free = shutil.disk_usage(path).free
    if free < minimum:
        raise OSError(f'Test preflight needs {minimum / 1024**3:g} GiB free; only {free / 1024**3:.2f} GiB is available. No user data was removed.')
    return free
