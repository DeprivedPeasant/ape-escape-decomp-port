"""Independent music pitch/loop checks; system Python provides numpy.

Original instructions and PSF traces are test references only, never port code.
Run render_psf_reference.py first to produce the optional independent SPU trace.
"""
import ctypes as ct
import csv
import json
import random
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'extract/codex/audio-audit'
sys.path.insert(0, str(ROOT / 'tools'))
import dump_areas as da
import dump_sound as ds
import dump_tones as dt
import render_seq as rs
from codex.mips_ground_reference import Reference


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # Own glue exposes private functions for differential testing without
    # extending the public player API or embedding any extracted instructions.
    source = OUT / 'music_probe.c'
    source.write_text('''#include "../../../src/audio/sp_player.c"
unsigned probe_pitch(const unsigned char *exe, size_t size, int root, int note, int fine, int bend, int range) {
    SpPlayer p = {0};
    const unsigned char *table = exe_ptr(exe, size, 0x800B38D0, 1216);
    for (int i=0;i<608;i++) p.pitch_tab[i]=rd16(table+i*2);
    return music_pitch(&p,root,note,fine,bend,range);
}
void probe_sample(const unsigned char *vb, size_t size, unsigned addr, const double *positions, double *out, int n) {
    MusicDecoder d = {0}; d.address=d.repeat=addr; d.next=28; d.decoded=-1;
    for(int i=0;i<n;i++) out[i]=music_at(&d,vb,size,positions[i]);
}
''')
    dll = OUT / 'music_probe.dll'
    subprocess.run([shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-shared',
                    str(source), '-o', str(dll), '-lm', '-static-libgcc'], check=True)
    lib = ct.CDLL(str(dll))
    lib.probe_pitch.argtypes = [ct.c_char_p, ct.c_size_t] + [ct.c_int] * 5
    lib.probe_pitch.restype = ct.c_uint
    ptr = ct.POINTER(ct.c_double)
    lib.probe_sample.argtypes = [ct.c_char_p, ct.c_size_t, ct.c_uint, ptr, ptr, ct.c_int]
    exe = rs.exe_bytes()
    table = np.frombuffer(rs.exe_read(exe, 0x800B38D0, 1216), dtype='<u2').astype(np.int64)
    member = da.area_members()[1]
    original = Reference((da.MEMBERS / f'{member:03X}.bin').read_bytes())
    rng = random.Random(94423)
    cases, outside = 0, 0
    for _ in range(10000):
        root, note = rng.randrange(128), rng.randrange(128)
        fine, bend, span = rng.randrange(-128, 128), rng.randrange(128), rng.randrange(13)
        try:
            expected = int(rs.music_pitch(table, root, note, fine, bend, span))
        except ValueError:
            outside += 1
            continue  # artificial combination outside the documented guard table
        original.trace = lambda pc, r: original.write(r[29] + 16, 4, span) if pc == 0x8009E594 else None
        hardware = min(original.call(0x8009E594, (root, note, fine, bend))[2], 0x4000)
        actual = lib.probe_pitch(exe, len(exe), root, note, fine, bend, span)
        assert actual == expected == hardware, (root, note, fine, bend, span, actual, expected, hardware)
        cases += 1
    # Sustained stereo pads: reference comes from executing a separate PSF driver.
    trace = OUT / 'time_station_psf_30s.pcm.voices.csv'
    assert trace.exists(), 'run render_psf_reference.py --seconds 30 first'
    observed = set()
    with trace.open() as stream:
        for row in csv.DictReader(stream):
            if int(row['adsr1']) == 0xB9FF and int(row['envelope']) > 0:
                observed.add(int(row['pitch']))
    expected_pad = {1490, 1479, 1495, 1485}
    assert observed == expected_pad, observed
    bank = (ds.MEMBERS / '136.bin').read_bytes()
    chunks = ds.parse(bank)
    so, ss = chunks['shd'][0]; vo, vs = chunks['vb'][0]
    h, vb = bank[so:so+ss], bank[vo:vo+vs]
    progs = dt.parse_programs(h)
    pad = progs[28]
    pitches = [int(rs.music_pitch(table, layer[2], layer[0], struct.unpack_from('b', layer, 3)[0], 64, layer[13]))
               for layer in pad['layers']]
    assert set(pitches) == observed
    samples = []
    for addr in (0xE200, 0x11BC0, 0x1DD80, 0xB1B0):
        sample = rs.MusicSample(vb, addr)
        assert sample.loop is not None
        length = len(sample.pcm) + (len(sample.pcm) - sample.loop) * 3
        # Many repeat boundaries, every interpolation fraction, variable rates.
        positions = np.arange(length * 2, dtype=np.float64) * 0.5
        positions += (np.arange(len(positions)) % 256) / 65536.0
        reference = rs.music_resample(sample, positions)
        actual = np.empty_like(reference)
        lib.probe_sample(vb, len(vb), addr, positions.ctypes.data_as(ptr), actual.ctypes.data_as(ptr), len(actual))
        assert np.array_equal(reference, actual), (addr, np.abs(reference-actual).max())
        first = np.asarray(sample.decoded[:len(sample.pcm)])
        second = np.asarray(sample.decoded[len(sample.pcm):2*len(sample.pcm)-sample.loop])
        delta = second - first[sample.loop:]
        samples.append(dict(address=f'0x{addr:X}', output_checks=len(actual),
                            repeat_vs_cached_peak_lsb=int(np.abs(delta).max())))
    on, _ = rs.envelope(0xB9FF, 0x5FCF, 44100*11, hardware_hold=True)
    assert on[-1] == on[-44100], 'all-ones sustain rate must hold its level'
    report = dict(original_mips_pitch_cases=cases, artificial_out_of_guard=outside,
                  psf_pad_pitches=pitches, streaming_samples=samples,
                  sustain_hold=True, failures=0)
    (OUT / 'music-fidelity.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

