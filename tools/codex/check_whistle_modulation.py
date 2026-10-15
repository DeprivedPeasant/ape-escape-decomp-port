"""Check normal-music additive modulation against original SeqUpdateVoices.

Uses extracted instructions only in the test oracle. No game code is port code.
Run with installed system Python (numpy). Outputs remain ignored in extract/.
"""
import ctypes as ct
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
    source = OUT / 'whistle_probe.c'
    source.write_text('''#include "../../../src/audio/sp_player.c"
unsigned probe(const unsigned char *exe, size_t size, int root, int note, int fine, int bend, int range, int wave, int depth) {
    SpPlayer p = {0}; const unsigned char *t=exe_ptr(exe,size,0x800B38D0,1216);
    for(int i=0;i<608;i++) p.pitch_tab[i]=rd16(t+i*2);
    return music_mod_pitch(&p,root,note,fine,bend,range,wave,depth);
}
''')
    dll = OUT / 'whistle_probe.dll'
    subprocess.run([shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-shared',
                    str(source), '-o', str(dll), '-lm', '-static-libgcc'], check=True)
    lib = ct.CDLL(str(dll))
    lib.probe.argtypes = [ct.c_char_p, ct.c_size_t]+[ct.c_int]*7
    lib.probe.restype = ct.c_uint
    exe = rs.exe_bytes()
    table = np.frombuffer(rs.exe_read(exe, 0x800B38D0, 1216), dtype='<u2').astype(np.int64)
    bank = (ds.MEMBERS / '136.bin').read_bytes()
    chunks = ds.parse(bank)
    off, length = chunks['shd'][0]
    header = bank[off:off+length]
    off, length = chunks['ssq']
    notes, channels, _ = rs.build(bank[off:off+length], 1)
    program = dt.parse_programs(header)[3]
    layer = program['layers'][0]
    lfo_offset = struct.unpack_from('<I', header, 24)[0]
    ref = Reference((da.MEMBERS / f'{da.area_members()[1]:03X}.bin').read_bytes())
    ref.ram[0xE07F0:0xE07F0+24*0x66] = bytes(24*0x66)
    ref.ram[0xF5AC0:0xF5AC0+0x120] = bytes(0x120)
    ref.ram[0x120000:0x120000+len(header)] = header
    for address, value in ((0x800E1864, 0x80120000+lfo_offset), (0x800E1868, 0x80120000+lfo_offset),
                            (0x800E1874, 0x80110000)):
        ref.write(address, 4, value)
    ref.write(0x800F75FE, 1, 60)
    rng = random.Random(94423)
    cases = 0
    for _ in range(1500):
        root, note = rng.randrange(40, 100), rng.randrange(40, 100)
        fine, bend, span = rng.randrange(-32, 33), rng.randrange(128), rng.randrange(1, 13)
        depth, curve = rng.randrange(128), rng.randrange(7)
        phase = rng.randrange(224)  # one original update advances by 16
        for offset, value in ((0, 1), (2, note), (6, 0), (0x10, curve), (0x12, phase),
                              (0x14, 1), (0x16, depth), (0x1C, 0), (0x36, fine & 255),
                              (0x38, bend), (0x3A, span), (0x3C, 16), (0x40, root), (0x4E, 120)):
            ref.write(0x800E07F0+offset, 2, value)
        ref.call(0x8009CE18, limit=20000)
        updated_phase = ref.read(0x800E0802, 2)
        wave = int(rs.lfo_wave(header, curve)[updated_phase >> 2])
        expected = min(ref.read(0x80110004, 2), 0x4000)
        c = lib.probe(exe, len(exe), root, note, fine, bend, span, wave, depth)
        py = int(rs.music_mod_pitch(table, root, note, fine, bend, span, wave, depth))
        assert c == py == expected, (root, note, fine, bend, span, wave, depth, c, py, expected)
        cases += 1
    # Real whistle notes and all their written channel-bend/depth values.
    events = []
    for on, off, channel, key, velocity in notes:
        if channel != 6 or not 40 < on/44100 < 78:
            continue
        if int(channels[channel]['prog'].at(on, 1)) != 3:
            continue
        end = max(off-on, 1)
        bends = np.asarray(channels[channel]['bend'].at(on, end))
        depths = np.asarray(channels[channel]['mod'].at(on, end))
        changes = set(zip(np.broadcast_to(bends, (end,)).tolist(), np.broadcast_to(depths, (end,)).tolist()))
        root, fine, span = layer[2], struct.unpack_from('b', layer, 3)[0], layer[13]
        for bend, depth in changes:
            wave = rs.lfo_wave(header, layer[14]).astype(np.int64)
            pitches = rs.music_mod_pitch(table, root, key, fine, bend, span, wave, depth)
            base = int(rs.music_pitch(table, root, key, fine, bend, span))
            old = rs.music_pitch(table, root, key, fine, wave*depth//255-((depth+1)>>1)+64, span)
            if depth:
                events.append(dict(on_seconds=on/44100, bend=int(bend), depth=int(depth), base=base,
                                   previous_span=int(np.ptp(old)), corrected_span=int(np.ptp(pitches))))
    assert events and all(row['corrected_span'] < row['previous_span'] for row in events)
    report = dict(original_update_cases=cases, real_whistle_settings=len(events), failures=0,
                  corrected='preserve written bend, add modulation with one-semitone range',
                  maximum_previous_span=max(row['previous_span'] for row in events),
                  maximum_corrected_span=max(row['corrected_span'] for row in events), events=events)
    (OUT / 'whistle-modulation.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'PASS {cases} original SeqUpdateVoices cases; {len(events)} real whistle settings; '
          f"maximum pitch span {report['maximum_previous_span']} -> {report['maximum_corrected_span']}")


if __name__ == '__main__':
    main()
