"""Special sky-prop (mesh type 4 / 5) placement metadata with raw PSX signed-16 vertices and source-order triangles.

Re-walks the same area header / mesh table / prop placement table as prop_materials.py (which owns the primitive
validation) and returns only placements whose mesh type (low byte of the mesh header flags word) is 4 or 5. Placement
x/y/z are kept as metadata only: the original special draw path ignores them (see native_sky.md). No transform is
applied here; native_sky.c owns the integer vertex math.

Usage: uv run python tools/codex/sky_props.py <member hex> [...]   -> per-member counts
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import prop_materials as pm  # noqa: E402

SKY_TYPES = (4, 5)
PLACEMENT_SIZE = 12


def _load_blob(blob: bytes) -> list[dict]:
    pm._need(blob, 0x4D0, 4, "area header pointer")
    ah = struct.unpack_from("<I", blob, 0x4D0)[0]
    pm._need(blob, ah, 0x14, "area header")
    u32 = lambda o: struct.unpack_from("<I", blob, o)[0]
    meshtab = ah + u32(ah + 8) * 4
    props = ah + u32(ah + 0xC) * 4
    pm._need(blob, meshtab, 4, "mesh table")
    nmesh = u32(meshtab)
    pm._need(blob, meshtab + 4, nmesh * 4, "mesh table entries")
    pm._need(blob, props, 4, "prop table")
    n = u32(props)
    pm._need(blob, props + 4, n * PLACEMENT_SIZE, "prop placements")

    meshes: dict[int, dict] = {}
    out = []
    for i in range(n):
        mesh, rot, px, py, pz, _ = struct.unpack_from("<HHhhhH", blob, props + 4 + i * PLACEMENT_SIZE)
        if mesh >= nmesh:
            continue  # prop_materials / the OBJ exporter skip these placements
        if mesh not in meshes:
            parsed = pm.parse_mesh(blob, meshtab, mesh)  # validates the whole mesh, sky or not
            m = parsed["offset"]
            flags = u32(m)
            vofs, vc = struct.unpack_from("<HH", blob, m + 0x10)
            verts = [struct.unpack_from("<hhh", blob, m + vofs * 4 + k * 8) for k in range(vc)]
            tris = []
            for r in parsed["prims"]:
                v = r["v"]
                tris.append(tuple(v[0:3]))
                if len(v) == 4:
                    tris.append((v[2], v[1], v[3]))
            meshes[mesh] = {"flags": flags, "verts": verts, "tris": tris}
        d = meshes[mesh]
        typ = d["flags"] & 0xFF
        if typ not in SKY_TYPES:
            continue
        out.append({
            "prop_index": i,
            "mesh_index": mesh,
            "mesh_flags": d["flags"],
            "type": typ,
            "rotation": rot,
            "original_quarter_turn": (rot >> 10) & 3,
            "placement": (px, py, pz),
            "placement_ignored": True,
            "vertex_count": len(d["verts"]),
            "vertices": list(d["verts"]),
            "triangles": list(d["tris"]),
        })
    return out


def load(member: int) -> list[dict]:
    """Type 4/5 placements of an area member in prop order. Raises ValueError on any malformed source."""
    return _load_blob((pm.MEMBERS / f"{member:03X}.bin").read_bytes())


def main() -> None:
    for a in sys.argv[1:]:
        member = int(a, 16)
        recs = load(member)
        t4 = sum(1 for r in recs if r["type"] == 4)
        print(f"member 0x{member:03X}: {len(recs)} sky placements (type4 {t4}, type5 {len(recs) - t4}), "
              f"{sum(len(r['triangles']) for r in recs)} tris")


if __name__ == "__main__":
    main()
