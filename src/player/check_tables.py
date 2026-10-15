#!/usr/bin/env python3
"""Local-only verification of the two EXE tables the portable player needs.

Run on the workstation that has the original SCUS_944.23 (never in the cloud checkout):

    python src/player/check_tables.py extract/disc/SCUS_944.23 [--dump surface.json]

It reads, with the same address arithmetic the C loader uses (file offset = addr - 0x80010000 + 0x800):
  * the SDK sine quarter table at 0x800A9DB0 (1025 s16) that rsin / rcos index, and reports how far
    it is from round(4096 * sin(i * 2pi / 4096)), i.e. whether the built-in approximation is exact;
  * g_SurfaceTable at 0x800AEF60 (8 x 0x10 bytes) with the fields PlayerUpdate reads.
Nothing is written unless --dump is given.
"""
import argparse
import json
import math
import struct
import sys

SINE, SURFACE = 0x800A9DB0, 0x800AEF60


def off(addr: int) -> int:
    return addr - 0x80010000 + 0x800


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("exe")
    ap.add_argument("--dump", help="write the decoded surface table to this JSON path")
    a = ap.parse_args()
    exe = open(a.exe, "rb").read()
    table = struct.unpack_from("<1025h", exe, off(SINE))
    ok = table[0] == 0 and table[0x400] == 0x1000 and all(table[i] >= table[i - 1] for i in range(1, 1025))
    approx = [round(4096 * math.sin(i * math.pi / 2048)) for i in range(1025)]
    diffs = [(i, table[i], approx[i]) for i in range(1025) if table[i] != approx[i]]
    print(f"sine table: endpoints/monotonic ok={ok}; mismatches against round(4096*sin): {len(diffs)}")
    for i, real, calc in diffs[:12]:
        print(f"  index {i:#x}: original {real} approx {calc}")
    entries = []
    for k in range(8):
        raw = exe[off(SURFACE) + k * 16: off(SURFACE) + k * 16 + 16]
        w0, w1 = struct.unpack_from("<ii", raw, 0)
        h = struct.unpack_from("<8h", raw, 0)
        entries.append({"kind": k, "raw": raw.hex(), "accel_idle_+0": w0, "accel_move_+4": w1,
                        "slope_wall_+0xA": h[5], "slope_jump_+0xC": h[6], "slope_limit_+0xE": h[7]})
        print(f"surface {k}: raw={raw.hex()} +0={w0:#x} +4={w1:#x} +A={h[5]:#x} +C={h[6]:#x} +E={h[7]:#x}")
    if a.dump:
        json.dump({"sine_mismatches": len(diffs), "surface": entries}, open(a.dump, "w"), indent=1)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
