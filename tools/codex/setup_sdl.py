"""Locate pinned SDL3; download official SDK only with explicit --download.

Third-party files remain ignored under extract/codex/deps, never installed globally.
"""
import argparse
import hashlib
import os
import urllib.request
import zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
VERSION='3.4.18'
SHA256='cd98158c8d025a816f430a600b4e9526524e6219ee884ec72bd498b80fd3bf58'
DEPS=ROOT/'extract/codex/deps'
SDK=DEPS/f'SDL3-{VERSION}'/'x86_64-w64-mingw32'
URL=f'https://github.com/libsdl-org/SDL/releases/download/release-{VERSION}/SDL3-devel-{VERSION}-mingw.zip'

def locate():
    path=Path(os.environ.get('SDL3_DIR',SDK)).resolve()
    for rel in ('include/SDL3/SDL.h','lib/libSDL3.dll.a','bin/SDL3.dll'):
        if not (path/rel).is_file():
            raise RuntimeError(f'SDL3 SDK missing {path/rel}; run uv run --offline python tools/codex/setup_sdl.py --download or set SDL3_DIR')
    return path

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--download',action='store_true');args=parser.parse_args()
    if args.download:
        DEPS.mkdir(parents=True,exist_ok=True);archive=DEPS/f'SDL3-devel-{VERSION}-mingw.zip'
        if not archive.exists():
            req=urllib.request.Request(URL,headers={'User-Agent':'ApeEscapeDecomp'})
            with urllib.request.urlopen(req,timeout=60) as response:
                data=response.read(16000000)
            if hashlib.sha256(data).hexdigest()!=SHA256:raise RuntimeError('Official SDK checksum mismatch; archive not saved')
            archive.write_bytes(data)
        if hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA256:raise RuntimeError('SDK archive checksum mismatch')
        with zipfile.ZipFile(archive) as z:
            for info in z.infolist():
                target=(DEPS/info.filename).resolve()
                if not target.is_relative_to(DEPS.resolve()):raise RuntimeError('SDK archive path escapes dependency directory')
            z.extractall(DEPS)
    print(locate())
if __name__=='__main__':main()
