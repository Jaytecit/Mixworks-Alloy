"""Freeze only the interpreter and four required visual dependencies into a new directory.

Run using the qualified development interpreter. Never copies the reference
renderer, codecs, capture modules, pip or unrelated site-packages.
"""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import shutil
import sys


def package(destination):
    destination=Path(destination).resolve()
    destination.mkdir(parents=True,exist_ok=False)
    base=Path(sys.base_prefix)
    for name in ('python.exe','pythonw.exe','python3.dll',f'python{sys.version_info.major}{sys.version_info.minor}.dll','vcruntime140.dll','vcruntime140_1.dll','LICENSE.txt'):
        source=base/name
        if source.exists():shutil.copy2(source,destination/name)
    ignore=shutil.ignore_patterns('__pycache__','*.pyc','site-packages','test','tests','idlelib','tkinter','ensurepip')
    for name in ('Lib','DLLs'):
        shutil.copytree(base/name,destination/name,ignore=ignore)
    packages=destination/'Lib/site-packages';packages.mkdir()
    versions={}
    for name,folders in {'numpy':['numpy','numpy.libs'],'Pillow':['PIL','pillow.libs'],'PyOpenGL':['OpenGL'],'glfw':['glfw']}.items():
        dist=importlib.metadata.distribution(name);site=Path(dist.locate_file(''))
        versions[name]=dist.version
        for folder in folders:
            source=site/folder
            if source.exists():shutil.copytree(source,packages/folder,ignore=shutil.ignore_patterns('__pycache__','*.pyc','tests'))
        metadata=Path(dist._path)
        shutil.copytree(metadata,packages/metadata.name)
    # Windows isolated path configuration prevents loading user/prototype packages.
    (destination/f'python{sys.version_info.major}{sys.version_info.minor}._pth').write_text('.\nLib\nDLLs\nLib/site-packages\nimport site\n')
    records={str(p.relative_to(destination)):hashlib.sha256(p.read_bytes()).hexdigest() for p in destination.rglob('*') if p.is_file()}
    (destination/'runtime-manifest.json').write_text(json.dumps(dict(python=sys.version,packages=versions,files=records),indent=2))


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('destination',type=Path);args=ap.parse_args();package(args.destination)
