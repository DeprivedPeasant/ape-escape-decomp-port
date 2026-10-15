"""List the collider-scan / ray queries that native_player_volume + Codex's host reject (status != 0).

Replays the exact query set of tools/codex/check_cloud_volumes.py (same RNG seed and order) against the
CURRENT worktree module sources and prints, for every query the native side refuses, the area, the
query kind, the position/probe, the AE_VOLUME_* status bits and what the original MIPS code returned.
Read-only use of Codex's host/collision sources and reference interpreter.
"""
import argparse
import ctypes as C
import json
import random
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
PLAYER_DIR = HERE.parent
sys.path.insert(0, str(HERE))
from replay_check import find_main_root  # noqa: E402

BITS = ['NO_TABLE', 'SLOPE_HANDLER', 'SQRT_NEGATIVE', 'DIAG_ORIENTATION', 'TRENCH_TRAP', 'RAY_NOT_PREPARED',
        'RAY_MODE', 'GRID_PROBE', 'COLLIDER_PROBE', 'GRID_RANGE', 'NO_GROUND', 'WORD_3FA']


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--area', type=lambda s: int(s, 0))
    ap.add_argument('--report', type=Path)
    args = ap.parse_args()
    root = find_main_root()
    sys.path.insert(0, str(root / 'tools'))
    import dump_areas as da
    from codex.check_collision_floor import full_reference
    from codex.mips_ground_reference import signed
    tmp = tempfile.mkdtemp()
    dll = Path(tmp) / 'volume_unknowns.dll'
    cmd = [shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-shared', '-static-libgcc']
    cmd += [str(root / 'tools/codex/volume_review_bridge.c')]
    cmd += [str(root / 'src' / p) for p in ('app/native_player_host.c', 'collision/native_collision.c', 'collision/native_ground.c')]
    cmd += [str(PLAYER_DIR / p) for p in ('native_player.c', 'native_player_body.c', 'native_player_volume.c', 'native_player_pad.c', 'native_gadget_slots.c')] + [str(root / 'src/core' / p) for p in ('ae_ram.c', 'ae_pad.c')] + ['-lm', '-o', str(dll)]
    subprocess.run(cmd, check=True)
    lib = C.CDLL(str(dll))
    lib.volume_review_load.argtypes = [C.c_char_p] * 3
    lib.volume_review_load.restype = C.c_void_p
    lib.volume_review_free.argtypes = [C.c_void_p]
    lib.volume_review_scan.argtypes = [C.c_void_p, C.POINTER(C.c_int32), C.POINTER(C.c_uint32)]
    lib.volume_review_ray.argtypes = [C.c_void_p] + [C.POINTER(C.c_int32)] * 3 + [C.POINTER(C.c_uint32)]
    rng = random.Random(94423)
    totals = dict(scan=0, ray=0, unknown=0)
    unknown, failures = [], []
    for area, member in sorted(da.area_members().items()):
        if args.area is not None and area != args.area:
            continue
        path = da.MEMBERS / f'{member:03X}.bin'
        blob = path.read_bytes()
        ref, _, w, d, boxes = full_reference(blob)
        host = lib.volume_review_load(str(path).encode(), str(root / 'extract/disc/SCUS_944.23').encode(), str(da.MEMBERS / '008.bin').encode())
        assert host
        points = [struct.unpack_from('<3i', blob, i * 0x24) for i in range(16)]
        for raw in boxes:
            b = struct.unpack('<28h', raw)
            points.extend([(b[0], b[1] - b[4] // 2, b[2]), (b[0] + b[3], b[1] - b[4], b[2] + b[5])])
        try:
            for qi, pos in enumerate(points):
                body = 0x8013F200
                for i, v in enumerate(pos):
                    ref.write(body + i * 4, 4, v)
                ref.write(body + 0x34, 2, -1); ref.write(0x800F44A0, 2, 0); ref.write(0x800F45E6, 2, 1)
                ref.call(0x800792C8, (body, 2), limit=300000)
                expected = signed(ref.read(body + 0x34, 2), 16)
                status = C.c_uint32()
                actual = lib.volume_review_scan(host, (C.c_int32 * 3)(*pos), C.byref(status))
                if status.value:
                    rec = dict(area=area, member=member, query='scan', index_in_set=qi, position=pos, status=status.value,
                               status_bits=[n for k, n in enumerate(BITS) if status.value >> k & 1], original=expected, native=actual)
                    # which colliders contain / are near this point, for diagnosis
                    unknown.append(rec); totals['unknown'] += 1
                elif actual != expected:
                    failures.append(dict(area=area, query='scan', position=pos, expected=expected, actual=actual))
                else:
                    totals['scan'] += 1
                vel = (0, rng.choice([-20, 0, 20]), 0)
                for i, v in enumerate([*pos, *vel, 0, 0, (512 << 16) | 128, 0, 0, 0, 0, 0]):
                    ref.write(body + i * 4, 4, v)
                for a, v in [(0x800B20B0, 0x8013F000), (0x800B20B4, 0x8013F100), (0x800B20B8, 0x8013F120), (0x800B20BC, 0x8013F300), (0x800B20C0, 0x8013E000)]:
                    ref.write(a, 4, v)
                ref.ram[0x13F120:0x13F158] = ref.ram[0x13F200:0x13F238]
                ref.write(0x800B20CC, 2, 0)
                ref.call(0x8007304C, (1, body), limit=300000)
                probe = (*pos, 64, 128)
                for i, v in enumerate(probe):
                    ref.write(0x8013F400 + i * 4, 4, v)
                expected = signed(ref.call(0x80078C9C, (0, 0x8013F400, 0x8013F440), limit=300000)[2])
                actual = lib.volume_review_ray(host, (C.c_int32 * 3)(*pos), (C.c_int32 * 3)(*vel), (C.c_int32 * 5)(*probe), C.byref(status))
                if status.value:
                    unknown.append(dict(area=area, member=member, query='ray', index_in_set=qi, position=pos, velocity=vel, probe=probe, status=status.value,
                                        status_bits=[n for k, n in enumerate(BITS) if status.value >> k & 1], original=expected, native=actual))
                    totals['unknown'] += 1
                elif actual != expected:
                    failures.append(dict(area=area, query='ray', position=pos, expected=expected, actual=actual))
                else:
                    totals['ray'] += 1
        finally:
            lib.volume_review_free(host)
    out = dict(totals=totals, unknown=unknown, failures=failures)
    print(json.dumps(out, indent=1))
    if args.report:
        args.report.write_text(json.dumps(out, indent=1) + '\n')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
