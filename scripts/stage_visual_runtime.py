"""Freeze the active owned visual catalogue in a fresh repository .local folder."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from visual_catalogue import catalogue

ROOT = Path(__file__).resolve().parents[1]
MODULES = ('original_visual_gl.py', 'original_scene_player.py', 'visual_catalogue.py',
           'visual_compositor.py', 'visual_handoff.py', 'visual_media.py',
           'visual_presenter.py', 'visual_scene_worker.py')


def stage(output):
    output = Path(output).resolve()
    if not output.is_relative_to(ROOT / '.local') or output == ROOT / '.local':
        raise ValueError('Use a fresh subdirectory of repository .local')
    source = ROOT / 'visuals/original'
    entries = catalogue(source)
    output.mkdir(parents=True, exist_ok=False)
    for name in ('original-scenes.json', 'media-library.json'):
        shutil.copy2(source / name, output / name)
    for key in entries:
        slug = key.split('/')[1]
        shutil.copytree(source / 'scenes' / slug, output / 'scenes' / slug)
    for name in MODULES:
        shutil.copy2(ROOT / 'scripts' / name, output / name)
    hashes = {p.relative_to(output).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in output.rglob('*') if p.is_file()}
    (output / 'identities.json').write_text(json.dumps(hashes, indent=2), encoding='utf-8')
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    print(stage(parser.parse_args().out))
