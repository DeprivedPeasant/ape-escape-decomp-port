"""Export each area as a textured Wavefront OBJ: terrain, props (the real meshes), collider boxes.

First brick of a level editor and an end-to-end check of the grid / mesh / texture formats (notes/11, 12, 20).

Terrain: per cell, 4 corner vertices + the centre vertex, 4 triangles around the centre, heights from the corner /
centre height arrays. Textured from the 16-tile atlas (TPAGE 0x18, CLUT row 0xF0+tile at x=0x370; tile = cell bits
3-6) with the per-cell UV orientation table (cell bits 7-9); world corners X-Z-, X+Z-, X-Z+, X+Z+ use UV slots
(0, 2, 1, 3) and colour bytes (0, 6, 1, 7) whatever bit 2 is. Vertex colours use the `v x y z r g b` extension.
Props: each placement instantiates its mesh (area mesh table, notes/11 primitive layout), rotated about Y by
rotY (4096 = 360 degrees, sign tentative) and moved to its position; textures come from the area's VRAM.
Colliders: boxes (pos[1] = top y, extends down by halfSize[1] - tentative).
Areas with AreaHeader.flags & 3 == 3 have no terrain (junk grid) and only get props and colliders.

Usage: uv run python tools/export_area_obj.py [area ...]
Output: extract/obj/area_XX/area_XX.obj + .mtl + tex_*.png + terrain_atlas.png
"""
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dump_areas as da  # noqa: E402
import dump_tims as dt  # noqa: E402
import export_model as em  # noqa: E402
import vram as vr  # noqa: E402

OUT = da.ROOT / "extract/obj"
SCALE = 1 / 256.0  # one cell (0x200 units) = 2.0 in the OBJ
UV_QUAD = 0x800AF060  # 8 orientations x 4 corners (u, v) bytes


def uv_table() -> list[list[tuple[int, int]]]:
    o = UV_QUAD - 0x80010000 + 0x800
    return [[(da.EXE[o + k * 8 + 2 * c], da.EXE[o + k * 8 + 2 * c + 1]) for c in range(4)] for k in range(8)]


def export(area: int, member: int, uvs) -> tuple[int, int, int]:
    blob = (da.MEMBERS / f"{member:03X}.bin").read_bytes()
    u32 = lambda o: struct.unpack_from("<I", blob, o)[0]
    ah = u32(0x4D0)
    flags = u32(ah)
    grid = ah + u32(ah + 4) * 4
    meshtab = ah + u32(ah + 8) * 4
    props = ah + u32(ah + 0xC) * 4
    coll = ah + u32(ah + 0x10) * 4
    folder = OUT / f"area_{area:02X}"
    folder.mkdir(parents=True, exist_ok=True)
    vram = vr.area_vram(area)
    mats = vr.Materials(folder, vram)
    lines = [f"# Ape Escape area 0x{area:02X} (member 0x{member:03X}), 1 unit = 256 game units",
             f"mtllib area_{area:02X}.mtl"]
    counts = {"v": 0, "vt": 0, "tri": 0}

    def v(x, y, z, col=None):
        c = f" {col[0] / 255:.3f} {col[1] / 255:.3f} {col[2] / 255:.3f}" if col else ""
        lines.append(f"v {x * SCALE:.4f} {-y * SCALE:.4f} {z * SCALE:.4f}{c}")
        counts["v"] += 1
        return counts["v"]

    def vt(u, w):
        lines.append(f"vt {u / 256:.4f} {1 - w / 256:.4f}")
        counts["vt"] += 1
        return counts["vt"]

    def tri(a, b, c, ta=None, tb=None, tc=None):
        counts["tri"] += 1
        if ta:
            lines.append(f"f {a}/{ta} {b}/{tb} {c}/{tc}")
        else:
            lines.append(f"f {a} {b} {c}")

    if flags & 3 != 3:
        w, d = struct.unpack_from("<HH", blob, grid)
        ncol = u32(grid + 4)
        cells = grid + 8
        colors = [blob[cells + w * d * 4 + i * 4:cells + w * d * 4 + i * 4 + 3] for i in range(ncol)]
        tiles = cells + w * d * 4 + ncol * 4
        heights = tiles + w * d * 8
        corner = lambda cx, cz: struct.unpack_from("<h", blob, heights + (cz * (w + 1) + cx) * 2)[0]
        centers = heights + (w + 1) * (d + 1) * 2
        atlas = bytearray(256 * 256 * 4)
        for i in range(16):
            _, _, rgba = vr.texture(vram, 0x18, (0xF0 + i) * 0x40 | 0x37)
            tx, ty = (i % 4) * 64, (i // 4) * 64
            for y in range(64):
                o = ((ty + y) * 256 + tx) * 4
                atlas[o:o + 256] = rgba[o:o + 256]
        dt.png(folder / "terrain_atlas.png", 256, 256, bytes(atlas))
        lines += ["usemtl terrain_atlas", "o terrain"]
        x0, z0 = -w * 0x100, -d * 0x100
        for cz in range(d):
            for cx in range(w):
                i = cz * w + cx
                t = blob[tiles + i * 8:tiles + i * 8 + 8]
                cw = u32(cells + i * 4)
                # world corners X-Z-, X+Z-, X-Z+, X+Z+ take UV table slots (0, 2, 1, 3) and colour bytes (0, 6, 1, 7)
                # for both states of bit 2: DrawTerrain permutes the packet geometry and the UV / colour slots together,
                # so the world-corner association does not change (from codex-inbox, validated on original instructions)
                cols = [colors[t[b]] if cw & 1 else (128, 128, 128) for b in (0, 6, 1, 7)]
                tile, orient = cw >> 3 & 15, cw >> 7 & 7
                uv = [uvs[orient][s] for s in (0, 2, 1, 3)]
                xa, za = x0 + cx * 0x200, z0 + cz * 0x200
                ids = [v(xa + dx, corner(cx + ix, cz + iz), za + dz, cols[n])
                       for n, (ix, iz, dx, dz) in enumerate(
                           ((0, 0, 0, 0), (1, 0, 0x200, 0), (0, 1, 0, 0x200), (1, 1, 0x200, 0x200)))]
                tids = [vt((tile % 4) * 64 + uv[k][0] + (0.5 if uv[k][0] == 0 else 0),
                           (tile // 4) * 64 + uv[k][1] + (0.5 if uv[k][1] == 0 else 0)) for k in range(4)]
                cc = struct.unpack_from("<h", blob, centers + i * 2)[0]
                ctr = v(xa + 0x100, cc, za + 0x100, tuple(sum(c[k] for c in cols) // 4 for k in range(3)))
                cu, cv = sum(uv[k][0] for k in range(4)) / 4, sum(uv[k][1] for k in range(4)) / 4
                ctid = vt((tile % 4) * 64 + cu, (tile // 4) * 64 + cv)
                a, b, c, e = ids
                ta, tb, tc, te = tids
                for p, q, tp, tq in ((a, b, ta, tb), (b, e, tb, te), (e, c, te, tc), (c, a, tc, ta)):
                    tri(ctr, p, q, ctid, tp, tq)

    n = struct.unpack_from("<I", blob, props)[0]
    meshes = {}
    nmesh = u32(meshtab)
    for i in range(n):
        mesh, rot, px, py, pz, _ = struct.unpack_from("<HHhhhH", blob, props + 4 + i * 12)
        if mesh >= nmesh:
            continue
        if mesh not in meshes:
            m = meshtab + struct.unpack_from("<H", blob, meshtab + 4 + mesh * 4)[0] * 4
            cc, vofs, vc, pofs = struct.unpack_from("<HHHH", blob, m + 0xE)
            prims, _, ok = em.walk_prims(blob, m + pofs * 4, vc, cc)
            meshes[mesh] = (m, cc, vc, vofs, prims)
        m, cc, vc, vofs, prims = meshes[mesh]
        cols = [tuple(blob[m + 0x18 + k * 4:m + 0x18 + k * 4 + 3]) for k in range(cc)]
        verts = [struct.unpack_from("<hhh", blob, m + vofs * 4 + k * 8) for k in range(vc)]
        ang = rot * 2 * math.pi / 4096
        ca, sa = math.cos(ang), math.sin(ang)
        lines.append(f"o prop_{i}_mesh{mesh}")
        cur = ""  # unset: an untextured first primitive (mat None) must still emit "usemtl white"
        for r in prims:
            mat = mats.get(r["tpage"], r["clut"]) if "uv" in r else None
            if mat != cur:
                lines.append(f"usemtl {mat}" if mat else "usemtl white")
                cur = mat
            ids, tids = [], []
            for k, vi in enumerate(r["v"]):
                x, y, z = verts[vi]
                col = cols[r["col"][k if len(r["col"]) > 1 else 0]]
                ids.append(v(px + x * ca + z * sa, py + y, pz - x * sa + z * ca, col))
                if "uv" in r:
                    tids.append(vt(*r["uv"][k]))
            if "uv" in r:
                tri(ids[0], ids[1], ids[2], tids[0], tids[1], tids[2])
                if len(ids) == 4:
                    tri(ids[2], ids[1], ids[3], tids[2], tids[1], tids[3])
            else:
                tri(ids[0], ids[1], ids[2])
                if len(ids) == 4:
                    tri(ids[2], ids[1], ids[3])

    cbase = coll + struct.unpack_from("<I", blob, coll + 8)[0]
    nc = struct.unpack_from("<H", blob, cbase)[0]
    lines.append("usemtl white")
    for i in range(nc):
        r = cbase + 2 + i * 0x38  # entries follow the u16 count
        px, py, pz, hx, hy, hz = struct.unpack_from("<hhhhhh", blob, r)
        typ = struct.unpack_from("<H", blob, r + 0x12)[0] & 0xFF
        lines.append(f"o collider_{i}_t{typ}")
        corners = [v(px + sx * hx, py + (hy if sy else 0), pz + sz * hz)
                   for sy in (0, 1) for sx in (-1, 1) for sz in (-1, 1)]
        for q in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 2, 6, 4), (1, 5, 7, 3), (0, 4, 5, 1), (2, 3, 7, 6)):
            tri(corners[q[0]], corners[q[1]], corners[q[3]])
            tri(corners[q[0]], corners[q[2]], corners[q[3]])

    (folder / f"area_{area:02X}.obj").write_text("\n".join(lines) + "\n")
    (folder / f"area_{area:02X}.mtl").write_text(
        mats.mtl() + "\nnewmtl terrain_atlas\nKd 1 1 1\nmap_Kd terrain_atlas.png\n\nnewmtl white\nKd 0.8 0.8 0.8\n")
    return counts["v"], counts["tri"], nc


def main() -> None:
    want = {int(a, 0) for a in sys.argv[1:]}
    uvs = uv_table()
    for area, member in sorted(da.area_members().items()):
        if want and area not in want:
            continue
        nv, ntri, nc = export(area, member, uvs)
        print(f"area 0x{area:02X}: {nv} verts, {ntri} tris, {nc} colliders", flush=True)


if __name__ == "__main__":
    main()
