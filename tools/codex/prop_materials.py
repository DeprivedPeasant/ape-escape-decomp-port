"""Raw primitive metadata for area props, in the exact triangle order of tools/export_area_obj.py.

The OBJ exporter keeps only (tpage & 0x19F, clut) as the material and drops the semi-transparency bit of the primitive
code and tpage bits 5-6 (blend mode). This helper re-walks the same area header / mesh table / prop placement /
primitive traversal and returns those fields per emitted triangle. It only preserves metadata; it decides nothing about
alpha or blending (see prop_materials.md).

Usage: uv run python tools/codex/prop_materials.py <member hex> [...]   -> per-member summary
"""
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import dump_areas as da  # noqa: E402
import export_model as em  # noqa: E402

ROOT = da.ROOT
MEMBERS = da.MEMBERS
TEXTURED = (0x24, 0x2C, 0x34, 0x3C)
TPAGE_KEY_MASK = 0x19F  # what export_area_obj's material cache keeps
DRAW_MODE_BASE = 0xE1000200  # untextured semi: GP0 draw-mode word = base | ((descriptor byte +2) << 2 & 0x9FF)


class PropMaterialError(ValueError):
    pass


def _need(blob: bytes, off: int, size: int, what: str) -> None:
    if off < 0 or off + size > len(blob):
        raise PropMaterialError(f"{what}: bytes 0x{off:X}+0x{size:X} outside source of 0x{len(blob):X}")


def parse_mesh(blob: bytes, meshtab: int, mesh: int) -> dict:
    """One mesh-table entry: header fields plus source primitives {primitive_index, primitive_offset, ...}."""
    _need(blob, meshtab + 4 + mesh * 4, 2, f"mesh {mesh} table entry")
    m = meshtab + struct.unpack_from("<H", blob, meshtab + 4 + mesh * 4)[0] * 4
    _need(blob, m, 0x18, f"mesh {mesh} header")
    cc, vofs, vc, pofs = struct.unpack_from("<HHHH", blob, m + 0xE)
    _need(blob, m + 0x18, cc * 4, f"mesh {mesh} colours")
    _need(blob, m + vofs * 4, vc * 8, f"mesh {mesh} vertices")
    start = m + pofs * 4
    prims, end, ok = em.walk_prims(blob, start, vc, cc)
    if not ok:
        raise PropMaterialError(f"mesh {mesh}: primitive walk invalid (bad index, colour or truncation) at 0x{end:X}")
    if end + 4 > len(blob):
        raise PropMaterialError(f"mesh {mesh}: primitive run reaches end of source without a terminator")
    out, p = [], start
    for i, r in enumerate(prims):
        r = dict(r)
        r["primitive_index"] = i
        r["primitive_offset"] = p
        if "uv" not in r:
            r["draw_mode_byte"] = blob[p + 2]  # untextured: descriptor byte +2 feeds the draw-mode word
        p +=em.STRIDE[r["code"]]
        out.append(r)
    if p != end:
        raise PropMaterialError(f"mesh {mesh}: primitive stride bookkeeping mismatch")
    return {"offset": m, "colors": cc, "verts": vc, "start": start, "end": end, "prims": out}


def load(member: int) -> dict:
    """Parse a member. Returns {"triangles": [...], "skipped": [{prop_index, mesh_index}], "meshes": {idx: mesh}}."""
    blob = (MEMBERS / f"{member:03X}.bin").read_bytes()
    _need(blob, 0x4D0, 4, "area header pointer")
    ah = struct.unpack_from("<I", blob, 0x4D0)[0]
    _need(blob, ah, 0x14, "area header")
    u32 = lambda o: struct.unpack_from("<I", blob, o)[0]
    meshtab = ah + u32(ah + 8) * 4
    props = ah + u32(ah + 0xC) * 4
    _need(blob, meshtab, 4, "mesh table")
    nmesh = u32(meshtab)
    _need(blob, meshtab + 4, nmesh * 4, "mesh table entries")
    _need(blob, props, 4, "prop table")
    n = u32(props)
    _need(blob, props + 4, n * 12, "prop placements")

    meshes: dict[int, dict] = {}
    tris, skipped = [], []
    for i in range(n):
        mesh = struct.unpack_from("<H", blob, props + 4 + i * 12)[0]
        if mesh >= nmesh:
            skipped.append({"prop_index": i, "mesh_index": mesh})
            continue
        if mesh not in meshes:
            meshes[mesh] = parse_mesh(blob, meshtab, mesh)
        for r in meshes[mesh]["prims"]:
            textured = "uv" in r
            tpage = r["tpage"] if textured else None
            clut = r["clut"] if textured else None
            mode_byte = None if textured else r["draw_mode_byte"]
            mode_word = None if textured else DRAW_MODE_BASE | ((mode_byte << 2) & 0x9FF)
            for t in range(2 if len(r["v"]) == 4 else 1):
                tris.append({
                    "prop_index": i, "mesh_index": mesh,
                    "primitive_index": r["primitive_index"], "triangle_index": t,
                    "primitive_offset": r["primitive_offset"],
                    "code": r["code"], "semi": r["semi"],
                    "tpage": tpage, "clut": clut,
                    "draw_mode_byte": mode_byte, "draw_mode_word": mode_word,
                    "blend_mode": (tpage >> 5) & 3 if textured else (mode_word >> 5) & 3,
                    "material_key": (tpage & TPAGE_KEY_MASK, clut) if textured else None,
                })
    return {"triangles": tris, "skipped": skipped, "meshes": meshes}


def triangles(member: int) -> list[dict]:
    """Flat per-triangle metadata in OBJ emission order for the props of an area member."""
    return load(member)["triangles"]


def main() -> None:
    for a in sys.argv[1:]:
        member = int(a, 16)
        d = load(member)
        t = d["triangles"]
        semi = sum(1 for x in t if x["semi"])
        blend = Counter(x["blend_mode"] for x in t if x["semi"])
        print(f"member 0x{member:03X}: {len(t)} prop tris, {semi} semi, skipped placements {len(d['skipped'])}, "
              f"semi blend modes {dict(blend)}")


if __name__ == "__main__":
    main()
