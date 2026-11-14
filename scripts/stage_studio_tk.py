"""Add the matching installed CPython 3.12 Tk runtime to Alloy's private Python.

Run with the same CPython major/minor as the existing embedded runtime. No
packages are installed and the user's interpreter is never modified.
"""
import argparse
import filecmp
from pathlib import Path
import shutil
import sys
import tkinter
import _tkinter


def copy_if_changed(source, destination):
    if Path(destination).is_file() and filecmp.cmp(source, destination, shallow=False):
        return destination
    return shutil.copy2(source, destination)


def stage(destination):
    destination = Path(destination).resolve()
    if not (destination / f'python{sys.version_info.major}{sys.version_info.minor}.dll').is_file():
        raise ValueError('Run with the same Python major/minor as Alloy visual-python.')
    source = Path(sys.base_prefix)
    shutil.copytree(Path(tkinter.__file__).parent, destination / 'Lib/tkinter', dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('__pycache__', 'test'), copy_function=copy_if_changed)
    (destination / 'DLLs').mkdir(exist_ok=True)
    for name in ('_tkinter.pyd', 'tcl86t.dll', 'tk86t.dll'):
        copy_if_changed(Path(_tkinter.__file__).parent / name, destination / 'DLLs' / name)
    shutil.copytree(source / 'tcl', destination / 'tcl', dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('demos', '__pycache__'), copy_function=copy_if_changed)
    copy_if_changed(source / 'LICENSE.txt', destination / 'Tk-Python-LICENSE.txt')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    stage(parser.parse_args().destination)
