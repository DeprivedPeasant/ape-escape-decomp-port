"""Locate the portable C sources of the native port under src/ by bare file name.

The port lives in src/<subsystem>/ (see src/README.md). Checkers that compile a few modules look them up here
instead of hard-coding the subsystem folder: ``src('native_camera.c')`` -> ``<repo>/src/camera/native_camera.c``.
Includes between modules are relative (``#include "../camera/native_camera.h"``), so no -I flags are needed.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'src'
_INDEX = {}
for _p in sorted(SRC.rglob('*')):
    if _p.suffix in ('.c', '.h'):
        _INDEX.setdefault(_p.name, _p)


def src(name):
    """Absolute path (str) of the source/header file called `name` anywhere below src/."""
    try:
        return str(_INDEX[name])
    except KeyError:
        raise FileNotFoundError(f'{name} not found under {SRC}') from None
