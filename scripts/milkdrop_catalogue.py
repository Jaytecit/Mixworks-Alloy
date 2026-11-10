"""Read-only, content-addressed local MilkDrop imports, separate from owned scenes."""
import hashlib
import json
import re
from pathlib import Path


def catalogue(library):
    result = {}
    if library is None or not Path(library).is_dir():
        return result
    library = Path(library).resolve()
    for folder in sorted(library.iterdir()):
        if not re.fullmatch('[0-9a-f]{64}', folder.name) or not folder.is_dir():
            continue
        if folder.resolve().parent != library:
            continue
        try:
            metadata, preset = folder/'import.json', folder/'preset.milk'
            if metadata.stat().st_size > 32768 or not 16 <= preset.stat().st_size <= 1024*1024:
                continue
            if preset.resolve().parent != folder.resolve():
                continue
            record = json.loads(metadata.read_text(encoding='utf-8-sig'))
            if not isinstance(record, dict):
                continue
            if record.get('schema') != 1 or record.get('sha256') != folder.name:
                continue
            if hashlib.sha256(preset.read_bytes()).hexdigest() != folder.name:
                continue
            title, paths = record.get('title'), record.get('texturePaths', [])
            if not isinstance(title, str) or not 1 <= len(title) <= 96:
                continue
            if not isinstance(paths, list) or len(paths) > 2 or any(
                    not isinstance(p, str) or not Path(p).is_absolute() for p in paths):
                continue
            result['milkdrop/'+folder.name] = dict(title='MilkDrop: '+title,
                preset=str(preset), texturePaths=paths, imageReady=False,
                controls={'intensity': dict(min=0, max=1, default=.5)}, presets=[])
        except (OSError, ValueError, TypeError):
            continue
    linked = library/'linked'
    for manifest in sorted(linked.glob('*.json')):
        try:
            record = json.loads(manifest.read_text(encoding='utf-8-sig'))
            if not isinstance(record, dict) or record.get('schema') != 2:
                continue
            root, paths, entries = record.get('root'), record.get('texturePaths'), record.get('entries')
            if (not isinstance(root, str) or not Path(root).is_absolute()
                    or not isinstance(paths, list) or len(paths) > 2
                    or any(not isinstance(p, str) or not Path(p).is_absolute() for p in paths)
                    or not isinstance(entries, list)):
                continue
            for value in entries:
                if not isinstance(value, dict):
                    continue
                identity, title, source = value.get('sha256'), value.get('title'), value.get('source')
                if (not isinstance(identity, str) or not re.fullmatch('[0-9a-f]{64}', identity)
                        or not isinstance(title, str) or not 1 <= len(title) <= 96
                        or not isinstance(source, str) or not Path(source).is_absolute()
                        or not Path(source).is_relative_to(Path(root))):
                    continue
                # Do not open thousands of presets at player startup. Validate the
                # exact selected source immediately before projectM loads it.
                result.setdefault('milkdrop/'+identity, dict(title='MilkDrop: '+title,
                    preset=source, sha256=identity, texturePaths=list(dict.fromkeys([str(Path(source).parent), *paths])),
                    imageReady=False, controls={'intensity': dict(min=0, max=1, default=.5)}, presets=[]))
        except (OSError, ValueError, TypeError):
            continue
    return result


def validate_source(entry):
    """Fail visibly if a linked preset disappeared or changed after indexing."""
    if 'sha256' not in entry:
        return
    preset = Path(entry['preset'])
    try:
        if not 16 <= preset.stat().st_size <= 1024*1024 or hashlib.sha256(preset.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError('MilkDrop preset changed. Link its folder again to rescan.')
    except OSError as error:
        raise ValueError('MilkDrop preset unavailable. Reconnect its folder or link its new location.') from error
