"""Cross-check tools/overlay_map.json against the game's own overlay load tables.

Sources in SCUS_944.23:
  - mode table 0x800AFA30: 8-byte {entry:u32, member:u32}; member 0xFFFFFFFF = resident code.
    Loader 0x8005BDC0 loads the member to 0x80136000.
  - area record pointer table 0x800A81B0 -> chains of 8-byte
    {member:u16, postprocess:i16, destination:u32} records in 0x800A71E0..0x800A8190.

Usage: uv run python tools/verify_overlay_map.py
"""
import collections
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXE = (ROOT / "extract" / "disc" / "SCUS_944.23").read_bytes()
BASE, HDR = 0x80010000, 0x800
OVL_FIRST, OVL_LAST = 0x180, 0x1B0
MODE_TABLE, MODE_DEST = 0x800AFA30, 0x80136000
AREA_LO, AREA_HI, AREA_PTRS = 0x800A71B0, 0x800A81B0, 0x800A81B0


def rd(addr: int, n: int) -> bytes:
    off = addr - BASE + HDR
    return EXE[off:off + n]


def u32(addr: int) -> int:
    return struct.unpack("<I", rd(addr, 4))[0]


def mode_table() -> dict[int, set[int]]:
    """member -> set of entry addresses"""
    out = collections.defaultdict(set)
    addr = MODE_TABLE
    while True:
        entry, member = struct.unpack("<II", rd(addr, 8))
        if not 0x80000000 <= entry < 0x80200000:
            break
        if member != 0xFFFFFFFF:
            out[member].add(entry)
        addr += 8
    print(f"mode table: {(addr - MODE_TABLE) // 8} modes, {len(out)} overlay members")
    return out


def area_records() -> tuple[dict[int, set[int]], dict[int, set[int]]]:
    """member -> destinations, member -> postprocess values (overlay members only)"""
    # Pointer table: 8-byte {chain_ptr:u32, id:u32}; entry 0 is null.
    ptrs = []
    addr = AREA_PTRS + 8
    while 0x800A7000 <= (p := u32(addr)) < AREA_HI:
        ptrs.append((p, u32(addr + 4)))
        addr += 8
    print(f"area pointer table: {len(ptrs)} entries, chains {hex(min(p for p, _ in ptrs))}.."
          f"{hex(max(p for p, _ in ptrs))}, ids {hex(ptrs[0][1])}..{hex(ptrs[-1][1])}")

    dest, post = collections.defaultdict(set), collections.defaultdict(set)
    kinds = collections.Counter()
    for rec in range(AREA_LO, AREA_HI, 8):
        member, postproc, d = struct.unpack("<HhI", rd(rec, 8))
        if OVL_FIRST <= member <= OVL_LAST:
            dest[member].add(d)
            post[member].add(postproc)
            kinds["overlay"] += 1
        elif member < OVL_FIRST:
            kinds["data"] += 1
        else:
            kinds[f"other:{member:#x}"] += 1
    print(f"area records: {dict(kinds)}")
    return dest, post


def main() -> None:
    modes = mode_table()
    area_dest, area_post = area_records()
    ovl_map = {int(o["index"], 16): o for o in
               json.loads((ROOT / "tools" / "overlay_map.json").read_text())["overlays"]}

    problems = 0
    for idx in range(OVL_FIRST, OVL_LAST + 1):
        seen = set()
        if idx in modes:
            seen.add(MODE_DEST)
        seen |= area_dest.get(idx, set())
        mapped = ovl_map.get(idx)
        mapped_addr = int(mapped["load_addr"], 16) if mapped else None
        status = mapped["status"] if mapped else "absent"
        if not seen:
            note = "no table reference"
        elif len(seen) > 1:
            note = "CONFLICT: multiple destinations"
            problems += 1
        elif mapped_addr not in seen:
            note = "MISMATCH"
            problems += 1
        else:
            note = "ok"
        src = "+".join(s for s, hit in (("mode", idx in modes), ("area", idx in area_dest)) if hit) or "-"
        tables = ",".join(hex(x) for x in sorted(seen)) or "-"
        print(f"{idx:#05x}  map={mapped_addr and hex(mapped_addr)}  tables={tables:<12} via={src:<9} "
              f"post={sorted(area_post.get(idx, []))}  [{status}]  {note}")
        # mode entries must fall inside the member
        for e in modes.get(idx, ()):
            size = (ROOT / "extract" / "members" / f"{idx:03X}.bin").stat().st_size
            if not MODE_DEST <= e < MODE_DEST + size:
                print(f"       entry {e:#x} outside member ({size:#x} bytes)")
                problems += 1
    print(f"\nproblems: {problems}")


if __name__ == "__main__":
    main()
