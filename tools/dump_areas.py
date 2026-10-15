"""Summarize every area data blob and export terrain heightmaps.

Area blobs are the archive members that an area's load list puts at 0x80154000 with no
postprocess (AreaLoad reads them there). Layout (notes/12-level-collision.md):
    blob +0x4D0: byte offset of the AreaHeader
    AreaHeader (word offsets, relative to the header):
        +0x04 grid, +0x08 mesh table, +0x0C props, +0x10 collision,
        +0x20 fog color, +0x2C/+0x2E fog near/far, +0x34 features[8]
    grid: width u16, depth u16, colorCount u32, cells u32[w*d], colors u32[colorCount],
          cellTiles 8 bytes [w*d] (first 4 = per-corner color indices), heights s16[(w+1)*(d+1)] + s16[w*d] centers
    collision (byte offsets from its start): [0] shape grid {w s16, d s16, s16[w*d]},
          [1] shapes {count u16, 0x14-byte records; +0x12 u16: low byte = shape type, high bits flags},
          [2] colliders {count u16, 0x38-byte entries}, [3], [4]

Outputs notes/13-areas.md and extract/areas/area_XX_heightmap.png.

Usage: uv run python tools/dump_areas.py
"""
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXE = (ROOT / "extract/disc/SCUS_944.23").read_bytes()
MEMBERS = ROOT / "extract/members"
OUT_MD = ROOT / "notes/13-areas.md"
OUT_IMG = ROOT / "extract/areas"
AREA_PTRS, AREA_DATA_DEST = 0x800A81B0, 0x80154000


def exe_u32(addr: int) -> int:
    return struct.unpack_from("<I", EXE, addr - 0x80010000 + 0x800)[0]


AREA_REC_LO, AREA_REC_HI = 0x800A71B0, 0x800A81B0  # load record region (found by tools/codex/area_tables.py)


def area_chains():
    """(area index, first load record address) for every area; the table at AREA_PTRS has 8 byte entries
    {u32 chain pointer, u32 id} ending with id 0xFFFFFFFF (index 0x6A); NULL chains (0x34, 0x59, 0x68, 0x69) are holes."""
    for area in range(1, 0x100):
        if exe_u32(AREA_PTRS + area * 8 + 4) == 0xFFFFFFFF:
            break
        ptr = exe_u32(AREA_PTRS + area * 8)
        if AREA_REC_LO <= ptr < AREA_REC_HI:
            yield area, ptr


def area_members() -> dict[int, int]:
    """area index -> archive member loaded to 0x80154000 with postprocess 0."""
    out = {}
    for area, ptr in area_chains():
        rec = ptr
        for _ in range(8):  # walk the record chain until a negative postprocess ends it
            member, post = struct.unpack_from("<Hh", EXE, rec - 0x80010000 + 0x800)
            dest = exe_u32(rec + 4)
            if post < 0:
                break
            if dest == AREA_DATA_DEST and post == 0 and member < 0x180:
                out[area] = member
            rec += 8
    return out


def write_png(path: Path, width: int, height: int, gray: bytes) -> None:
    raw = b"".join(b"\x00" + gray[y * width:(y + 1) * width] for y in range(height))
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)


SHAPE_HANDLERS = 0x800AF180
SHAPE_NAMES = {0x8004E33C: "Box", 0x8004E444: "WallThin", 0x8004E63C: "WallEdge",
               0x8004E8F8: "WallDiagonal", 0x8004EA1C: "Ramp45", 0x8004EC3C: "Cylinder",
               0x8004ED30: "Ramp2", 0x8004EE94: "Ramp4", 0x8004F000: "Wedge",
               0x8004F330: "DiagonalBox"}


def shape_types(blob: bytes, off: int, size: int) -> list[int]:
    """Type byte of every collision shape (count u16, then 0x14-byte records)."""
    n = struct.unpack_from("<H", blob, off)[0]
    assert 2 + n * 0x14 == size, (n, size)
    return [blob[off + 2 + i * 0x14 + 0x12] for i in range(n)]


def cell_stats(blob: bytes) -> dict:
    """Render attributes of the terrain cells (low 16 bits of each cell word) plus a check that
    every colour index in cellTiles is < colorCount (only meaningful when terrain is drawn)."""
    u32 = lambda o: struct.unpack_from("<I", blob, o)[0]
    ah = u32(0x4D0)
    grid = ah + u32(ah + 4) * 4
    w, d = struct.unpack_from("<HH", blob, grid)
    nc = u32(grid + 4)
    cells = grid + 8
    tiles = cells + w * d * 4 + nc * 4
    st = {"drawn": 0, "near": 0, "mirror": 0, "tile": [0] * 16, "orient": [0] * 16, "badcolor": 0,
          "terrain": (u32(ah) & 3) == 0}
    for i in range(w * d):
        c = u32(cells + i * 4)
        if not c & 1:
            continue
        st["drawn"] += 1
        st["near"] += c >> 1 & 1
        st["mirror"] += c >> 2 & 1
        st["tile"][c >> 3 & 15] += 1
        st["orient"][c >> 7 & 15] += 1
        if max(blob[tiles + i * 8:tiles + i * 8 + 8]) >= nc:
            st["badcolor"] += 1
    return st


def parse(blob: bytes) -> dict:
    u32 = lambda o: struct.unpack_from("<I", blob, o)[0]
    ah = u32(0x4D0)
    grid = ah + u32(ah + 4) * 4
    props = ah + u32(ah + 0xC) * 4
    coll = ah + u32(ah + 0x10) * 4
    w, d = struct.unpack_from("<HH", blob, grid)
    ncolors = u32(grid + 4)
    cells = grid + 8
    tiles = cells + w * d * 4 + ncolors * 4
    heights = tiles + w * d * 8
    corners = struct.unpack_from(f"<{(w + 1) * (d + 1)}h", blob, heights)
    c = [u32(coll + i * 4) for i in range(5)]
    return {
        "hdr": ah, "flags": u32(ah), "w": w, "d": d, "colors": ncolors,
        "props": u32(props),
        "colliders": struct.unpack_from("<H", blob, coll + c[2])[0],
        "shapes": shape_types(blob, coll + c[1], c[2] - c[1]),
        "collgrid": struct.unpack_from("<hh", blob, coll + c[0]),
        "fog": blob[ah + 0x20:ah + 0x23].hex(),
        "fognear": struct.unpack_from("<H", blob, ah + 0x2C)[0],
        "fogfar": struct.unpack_from("<H", blob, ah + 0x2E)[0],
        "features": [u32(ah + 0x34 + i * 4) & 0xFF for i in range(8)],
        "corners": corners,
    }


def main() -> None:
    OUT_IMG.mkdir(parents=True, exist_ok=True)
    rows = ["# Areas (generated)", "",
            "Generated by `tools/dump_areas.py` from the area records in SCUS_944.23 and the "
            "area data blobs in extract/members. Heightmaps: extract/areas/ (y is negative-up, "
            "so brighter = higher).", "",
            "| Area | Member | Grid | Colors | Props | Colliders | Height range | Fog | Near/Far | Features |",
            "|---|---|---|---|---|---|---|---|---|---|"]
    members = area_members()
    for area, member in sorted(members.items()):
        try:
            a = parse((MEMBERS / f"{member:03X}.bin").read_bytes())
        except (struct.error, IndexError) as e:
            rows.append(f"| 0x{area:02X} | 0x{member:03X} | parse failed: {e} |||||||||")
            continue
        lo, hi = min(a["corners"]), max(a["corners"])
        span = (hi - lo) or 1
        gray = bytes(255 - (h - lo) * 255 // span for h in a["corners"])
        write_png(OUT_IMG / f"area_{area:02X}_heightmap.png", a["w"] + 1, a["d"] + 1, gray)
        feats = " ".join(f"{f:02X}" for f in a["features"] if f)
        rows.append(f"| 0x{area:02X} | 0x{member:03X} | {a['w']}x{a['d']} | {a['colors']} | {a['props']} "
                    f"| {a['colliders']} | {lo}..{hi} | {a['fog']} | {a['fognear']}/{a['fogfar']} | {feats} |")
    rows += ["", "## Collision shape types (all areas)", "",
             "Handler = `g_CollShapeHandlers[type]` (89 entries, verified against the EXE table). "
             "Types >= 64 never occur in the data.", "",
             "| Type | Handler | Shapes | Areas |", "|---|---|---|---|"]
    total, where = {}, {}
    for area, member in sorted(members.items()):
        for t in parse((MEMBERS / f"{member:03X}.bin").read_bytes())["shapes"]:
            total[t] = total.get(t, 0) + 1
            where.setdefault(t, set()).add(area)
    for t in sorted(total):
        h = SHAPE_NAMES.get(exe_u32(SHAPE_HANDLERS + t * 4), "?")
        rows.append(f"| {t} (0x{t:02X}) | CollShape_{h} | {total[t]} | {len(where[t])} |")
    agg = {"drawn": 0, "near": 0, "mirror": 0, "tile": [0] * 16, "orient": [0] * 16}
    bad, off = [], []
    for area, member in sorted(members.items()):
        st = cell_stats((MEMBERS / f"{member:03X}.bin").read_bytes())
        if not st["terrain"]:
            off.append(area)
            if st["badcolor"] == 0 and st["drawn"]:
                bad.append(area)  # would mean the flag does not explain garbage grids
            continue
        if st["badcolor"]:
            bad.append(area)
        for k in ("drawn", "near", "mirror"):
            agg[k] += st[k]
        for k in ("tile", "orient"):
            agg[k] = [a + b for a, b in zip(agg[k], st[k])]
    rows += ["", "## Terrain cell render attributes (areas with terrain drawn)", "",
             "Low 16 bits of each cell word: bit0 draw, bit1 near (detailed), bit2 mirror, bits 3-6 "
             "tile index (4x4 atlas of 64x64 tiles; CLUT row 0xF0+index), bits 7-10 UV orientation. "
             f"Areas with flags&3==3 (no terrain, {len(off)} of them) have meaningless grids and are skipped. "
             f"Colour-index range check (every cellTiles byte < colorCount): "
             f"{'FAILED for ' + str(bad) if bad else 'passed for all terrain areas'}.", "",
             f"Drawn cells {agg['drawn']}, near {agg['near']}, mirrored {agg['mirror']}.", "",
             "| Index | Tile (bits 3-6) | UV orientation (bits 7-10) |", "|---|---|---|"]
    rows += [f"| {i} | {agg['tile'][i]} | {agg['orient'][i]} |" for i in range(16)]
    rows += ["", "## Area features (AreaHeader.features)", "",
             "Each entry: low byte id, upper 24 bits = word offset of its data from the header "
             "(AreaLoad passes header + offset*4). 0x08 zone boxes {count, ZoneBox[count]}; 0x10/0x11 "
             "indicator models; 0x20 VRAM animation list; 0x30-0x32 and 0x40/0x41 see notes/12.", "",
             "| Area | 0x08 zone boxes | 0x20 VRAM anim entries | other |", "|---|---|---|---|"]
    for area, member in sorted(members.items()):
        blob = (MEMBERS / f"{member:03X}.bin").read_bytes()
        u32 = lambda o: struct.unpack_from("<I", blob, o)[0]
        ah = u32(0x4D0)
        zones, vram, other = "", "", []
        for i in range(8):
            f = u32(ah + 0x34 + i * 4)
            fid, ptr = f & 0xFF, ah + (f >> 8) * 4
            if fid == 0x08:
                zones = str(u32(ptr))
            elif fid == 0x20:
                n = 0
                while blob[ptr + n * 16] != 0:
                    n += 1
                vram = str(n)
            elif fid:
                other.append(f"0x{fid:02X}")
        rows.append(f"| 0x{area:02X} | {zones} | {vram} | {' '.join(other)} |")
    OUT_MD.write_text("\n".join(rows) + "\n", encoding="utf-8")
    print(f"{len(members)} areas -> {OUT_MD}, heightmaps in {OUT_IMG}")


if __name__ == "__main__":
    main()
