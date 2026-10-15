"""Render Time Station using an independent HLE PSF emulator (test only).

Official Audacious PSF sources stay ignored under extract/codex/deps; the native
port does not link this emulator. No BIOS image or global installation required.
    python tools/codex/render_psf_reference.py --seconds 30
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import urllib.request
import wave
import zipfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from audit_time_station import psf_payload

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/audio-audit'
SOURCE=ROOT/'extract/codex/deps/audacious-psf'
COMMIT='ee2516c561452a33c8b90a15ae9dbc4ded1721f3'
REPO='audacious-media-player/audacious-plugins'


def get(url):
    request=urllib.request.Request(url,headers={'User-Agent':'ApeEscapeDecomp PSF research'})
    with urllib.request.urlopen(request,timeout=60) as response:return response.read(16000000)


def setup():
    manifest=SOURCE/'source.json'
    if not manifest.exists():
        tree=json.loads(get(f'https://api.github.com/repos/{REPO}/git/trees/{COMMIT}?recursive=1'))['tree']
        files=[entry['path'] for entry in tree if entry['type']=='blob' and entry['path'].startswith('src/psf/')]
        SOURCE.mkdir(parents=True,exist_ok=True)
        def save(name):
            target=SOURCE/name.removeprefix('src/psf/')
            if not target.resolve().is_relative_to(SOURCE.resolve()):raise ValueError('source path escape')
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(get(f'https://raw.githubusercontent.com/{REPO}/{COMMIT}/{name}'))
        with ThreadPoolExecutor(max_workers=6) as pool:list(pool.map(save,files))
        manifest.write_text(json.dumps(dict(repo=REPO,commit=COMMIT,files=files),indent=2))
    assert json.loads(manifest.read_text())['commit']==COMMIT
    compat=SOURCE/'compat/libaudcore';compat.mkdir(parents=True,exist_ok=True)
    # The standalone core only requires little-endian conversion macros and a
    # forward declaration from Audacious's frontend headers.
    (compat/'audio.h').write_text('#pragma once\n#define FROM_LE32(x) (x)\n#define FROM_LE16(x) (x)\n')
    (compat/'index.h').write_text('#pragma once\ntemplate<class T> class Index;\n')
    sources=['psx.cc','psx_hw.cc','peops/spu.cc','peops2/spu.cc','peops2/dma.cc','peops2/registers.cc']
    harness=ROOT/'tools/codex/psf_oracle_harness.cc'
    executable=OUT/'psf_oracle.exe'
    subprocess.run([shutil.which('g++'),'-std=c++11','-O2','-fpermissive','-I',str(SOURCE),'-I',str(SOURCE/'compat'),
                    *[str(SOURCE/name) for name in sources],str(harness),'-o',str(executable),
                    '-static-libgcc','-static-libstdc++','-lm'],check=True)
    return executable


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds',type=int,default=30)
    args=parser.parse_args()
    if not 1<=args.seconds<=600:parser.error('seconds must be 1..600')
    OUT.mkdir(parents=True,exist_ok=True)
    executable=setup();archive=OUT/'ape-escape-psf.zip'
    with zipfile.ZipFile(archive) as z:
        payload,tags=psf_payload(z.read('05 - Time Station.psf'))
        library=tags['_lib']
        assert '/' not in library and '\\' not in library
        driver,driver_tags=psf_payload(z.read(library))
        assert not any(key.startswith('_lib') for key in driver_tags)
        assert not any(key.startswith('_lib') and key!='_lib' for key in tags)
    program=OUT/'time_station.exe';program.write_bytes(payload)
    library_path=OUT/'time_station_driver.exe';library_path.write_bytes(driver)
    output=OUT/f'time_station_psf_{args.seconds}s.pcm'
    subprocess.run([str(executable),str(library_path),str(program),str(output),str(args.seconds)],check=True,timeout=300)
    pcm=output.read_bytes();assert len(pcm)==args.seconds*44100*4
    path=output.with_suffix('.wav')
    with wave.open(str(path),'wb') as wav:
        wav.setnchannels(2);wav.setsampwidth(2);wav.setframerate(44100);wav.writeframes(pcm)
    metadata=dict(emulator_commit=COMMIT,archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
                  track='05 - Time Station.psf',tags=tags,seconds=args.seconds,rate=44100,
                  emulator='Audacious PSF / Audio Overload / PEOpS with HLE',bios_image=False,
                  wav_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    path.with_suffix('.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(f'Rendered independent PSF reference: {path}')


if __name__=='__main__':main()
