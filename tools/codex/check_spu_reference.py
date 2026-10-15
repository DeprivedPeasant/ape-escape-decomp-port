"""Compare held music tones against an independent SPU core (test only).

No emulator implementation is copied into the port. Dependencies, decoded game
samples and output remain ignored under extract/codex/. System Python has numpy.
"""
import ctypes as ct
import hashlib
import json
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'extract/codex/audio-audit'
DEPS = ROOT / 'extract/codex/deps/highly-experimental'
COMMIT = '8a045a8619116ce2b4c11387f527cc48636ef7a9'
SHA256 = 'cb5f6f7d04469d2abb7f21859742f6753a5cefe255da86ef30520edcc0de07c3'
sys.path.insert(0, str(ROOT / 'tools'))
import dump_sound as ds
import render_seq as rs


def setup():
    source = DEPS / f'highly_experimental-{COMMIT}'
    if not source.exists():
        DEPS.mkdir(parents=True, exist_ok=True)
        request = urllib.request.Request(f'https://codeload.github.com/myst6re/highly_experimental/zip/{COMMIT}',
                                        headers={'User-Agent': 'ApeEscapeDecomp SPU research'})
        with urllib.request.urlopen(request, timeout=60) as stream:
            data = stream.read(16000000)
        assert hashlib.sha256(data).hexdigest() == SHA256
        archive = DEPS / 'source.zip'
        archive.write_bytes(data)
        with zipfile.ZipFile(archive) as z:
            for entry in z.infolist():
                assert (DEPS / entry.filename).resolve().is_relative_to(DEPS.resolve())
            z.extractall(DEPS)
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / 'spu_oracle.dll'
    subprocess.run([shutil.which('gcc'), '-std=c11', '-O2', '-DEMU_COMPILE', '-DEMU_LITTLE_ENDIAN',
                    '-I', str(source / 'include'), '-shared', str(source / 'src/spucore.c'),
                    '-o', str(dll), '-static-libgcc'], check=True)
    lib = ct.CDLL(str(dll))
    p, u = ct.c_void_p, ct.c_uint
    lib.spucore_get_state_size.restype = u
    lib.spucore_clear_state.argtypes = [p]
    lib.spucore_set_mem_size.argtypes = [p, u]
    lib.spucore_setreg.argtypes = [p, u, u, u]
    lib.spucore_setflag.argtypes = [p, u, ct.c_int]
    lib.spucore_setreg_voice.argtypes = [p, u, u, u, u]
    lib.spucore_render.argtypes = [p, p, p, p, u, ct.c_ubyte, ct.c_ubyte]
    lib.spucore_init()
    return lib


def render(lib, vb, address, pitch, seconds=11):
    state = ct.create_string_buffer(lib.spucore_get_state_size())
    lib.spucore_clear_state(state)
    lib.spucore_set_mem_size(state, 0x80000)
    ram = ct.create_string_buffer(0x80000)
    ct.memmove(ct.addressof(ram)+0x1010, vb, len(vb))
    # API enum constants from spucore.h: sound on/main enable, main voice L/R.
    for flag in (1 << 19, 1 << 18, 1 << 11, 1 << 10):
        lib.spucore_setflag(state, flag, 1)
    for register, value in ((0, 0x3FFF), (1, 0x3FFF), (18, 1), (19, 1)):
        lib.spucore_setreg(state, register, value, 0xFFFFFFFF)
    # Fast attack, maximum sustain, frozen sustain counter: isolate oscillator.
    for register, value in ((0, 0x3FFF), (1, 0x3FFF), (4, pitch), (5, 0x1010+address),
                            (6, 0x000F), (7, 0x1FC0)):
        lib.spucore_setreg_voice(state, 0, register, value, 0xFFFFFFFF)
    lib.spucore_setreg(state, 10, 1, 0xFFFFFFFF)  # key on
    out = np.empty((44100*seconds, 2), dtype=np.int16)
    lib.spucore_render(state, ram, out.ctypes.data, None, len(out), 1, 0)
    assert np.any(out)
    return out[:, 0].astype(np.float64)/32768.0


def difference(reference, model):
    # Exclude key-on transient, allow only a small startup delay and constant
    # gain. No pitch/time stretching or per-window alignment can mask drift.
    best = dict(relative_rms=2.0)
    for delay in range(-8, 9):
        a = model[100:len(model)-delay] if delay >= 0 else model[100-delay:]
        b = reference[100+delay:] if delay >= 0 else reference[100:len(reference)+delay]
        gain = float(b@a/(a@a))
        error = float(np.sqrt(np.mean((b-gain*a)**2))/np.sqrt(np.mean(b*b)))
        if error < best['relative_rms']:
            best = dict(relative_rms=error, startup_delay=delay, gain=gain)
    return best


def main():
    lib = setup()
    bank = (ds.MEMBERS / '136.bin').read_bytes()
    offset, size = ds.parse(bank)['vb'][0]
    vb = bank[offset:offset+size]
    rows = []
    for address, pitch, old_pitch in ((0xE200, 1490, 1490.5888592304), (0xE200, 1479, 1479.8651463445),
                                       (0x11BC0, 1495, 1495.979821241), (0x11BC0, 1485, 1485.217324268)):
        reference = render(lib, vb, address, pitch)
        index = np.arange(len(reference))
        sample = rs.decode_sample(vb, address)
        old = rs.cubic(sample.pcm, index*old_pitch/4096, sample.loop)
        pitch_only = rs.cubic(sample.pcm, index*pitch/4096, sample.loop)
        updated = rs.music_resample(rs.MusicSample(vb, address), index*pitch/4096)
        row = dict(sample=f'0x{address:X}', pitch=pitch, seconds=11,
                   previous=difference(reference, old), pitch_only=difference(reference, pitch_only),
                   updated=difference(reference, updated))
        assert row['updated']['relative_rms'] < row['previous']['relative_rms'] * 0.1
        assert row['updated']['relative_rms'] < 0.05
        rows.append(row)
    report = dict(oracle='Highly Experimental SPU core (standalone test, no BIOS)', commit=COMMIT,
                  scope='isolated oscillator; excludes sequencer, ADSR shape, pan and reverb',
                  tests=rows, failures=0)
    (OUT / 'spu-reference.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
