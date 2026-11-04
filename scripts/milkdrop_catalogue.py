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
        if len(result) >= 128:
            break
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
    return result
