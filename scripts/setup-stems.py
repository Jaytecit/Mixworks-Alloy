"""Install and register the optional, local-only Windows Demucs runtime.

Downloads public code/model/runtime files; never uploads music or changes analysis.
Use --configure-only to register an already installed, qualified runtime.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
COMMIT = '2883f3db65617d6d178c6ed10d869dc14e44e59b'
MODEL_REVISION = 'cbc8a9b1a87023b7fd74e7b3412e6321c0eab003'
PACKAGES = ['torch', 'demucs', 'numpy', 'soundfile', 'julius', 'safetensors', 'einops', 'sphn']


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=ROOT / '.local/demucs-runtime')
    p.add_argument('--config', type=Path, default=Path(os.environ['LOCALAPPDATA']) / 'Mixworks/StemRuntime-v1.json')
    p.add_argument('--configure-only', action='store_true')
    args = p.parse_args(); runtime = args.root.resolve()
    python = runtime / 'Scripts/python.exe'
    if not args.configure_only:
        if not python.exists():
            subprocess.run([sys.executable, '-m', 'venv', str(runtime)], check=True)
        subprocess.run([str(python), '-m', 'pip', 'install', 'torch==2.6.0',
                        '--index-url', 'https://download.pytorch.org/whl/cu124'], check=True)
        subprocess.run([str(python), '-m', 'pip', 'install', '--no-deps',
                        f'git+https://github.com/adefossez/demucs.git@{COMMIT}'], check=True)
        constraints = runtime / 'torch-constraint.txt'; constraints.write_text('torch==2.6.0+cu124\n')
        subprocess.run([str(python), '-m', 'pip', 'install', '-c', str(constraints),
                        'numpy==2.2.6', 'soundfile==0.13.1', 'einops==0.8.1',
                        'huggingface-hub==0.36.0', 'julius==0.2.7', 'lameenc==1.8.1',
                        'pyyaml==6.0.3', 'safetensors==0.6.2', 'sphn==0.2.1', 'tqdm==4.67.1'], check=True)
    model = runtime / 'models/955717e8.safetensors'
    model.parent.mkdir(exist_ok=True)
    url = f'https://huggingface.co/adefossez/HTDemucs/resolve/{MODEL_REVISION}/955717e8.safetensors'
    if not model.exists():
        temporary = model.with_suffix('.download')
        urllib.request.urlretrieve(url, temporary)
        temporary.replace(model)
    # Upstream Git-LFS SHA-256 is the content identity, fetched at the pinned revision.
    api = f'https://huggingface.co/api/models/adefossez/HTDemucs/revision/{MODEL_REVISION}?blobs=true'
    metadata = json.load(urllib.request.urlopen(api))
    expected = next(x['lfs']['sha256'] for x in metadata['siblings'] if x['rfilename'] == model.name)
    if sha(model) != expected:
        raise ValueError('Downloaded model does not match the pinned upstream SHA-256')
    worker_source = ROOT / 'scripts/stem_worker.py'; worker_hash = sha(worker_source)
    worker = runtime / 'workers' / worker_hash / 'stem_worker.py'
    worker.parent.mkdir(parents=True, exist_ok=True)
    if not worker.exists(): shutil.copy2(worker_source, worker)
    if sha(worker) != worker_hash: raise ValueError('Worker checkpoint mismatch')
    code = 'import importlib.metadata as m,json;print(json.dumps({n:m.version(n) for n in ' + repr(PACKAGES) + '}))'
    packages = json.loads(subprocess.check_output([str(python), '-c', code], text=True))
    if packages['demucs'] != '4.1.0' or packages['torch'] != '2.6.0+cu124':
        raise ValueError('Expected qualified Demucs 4.1.0 and Torch 2.6.0+cu124')
    config = dict(schema=1, python=str(python), worker=str(worker), workerSha256=worker_hash,
                  model=str(model), modelSha256=expected, packages=packages,
                  demucsCommit=COMMIT, modelRevision=MODEL_REVISION, modelUrl=url)
    args.config.parent.mkdir(parents=True, exist_ok=True)
    if args.config.exists():
        backup = args.config.with_name(args.config.name + '.' + sha(args.config)[:12] + '.bak')
        if not backup.exists(): shutil.copy2(args.config, backup)
    temporary = args.config.with_suffix('.tmp')
    temporary.write_text(json.dumps(config, indent=2), encoding='utf-8'); temporary.replace(args.config)
    freeze = subprocess.check_output([str(python), '-m', 'pip', 'freeze'], text=True)
    (runtime / 'requirements-qualified.txt').write_text(freeze, encoding='utf-8')
    print('Local stem runtime registered:', args.config)
    print('Model SHA-256:', expected)


if __name__ == '__main__': main()
