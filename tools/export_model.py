"""Find and export skinned actor models and static meshes as OBJ (notes/11, notes/16).

Actor model (ModelHeader, 0xC bytes): flags u32, boneCount, vertCount, colorCount, hasNormals (u16 each),
  colors u32[colorCount] (CVECTOR), bones[boneCount] {vertCount u16, firstVert u16, offset s16[3],
  parent u16 (12 bytes)}, verts (8-byte SVECTORs; with hasNormals a second array of vc normals (4096 = 1.0) follows), then primitive runs.
Static mesh (area mesh table / props): word0 flags, +0xE colorCount, +0x10 vertex offset (words),
  +0x12 vertCount, +0x14 primitive offset (words), colors at +0x18, verts 8 bytes.
Primitive runs come sorted by GPU code in this order (ModelEmitPrims): FT3 0x24, FT4 0x2C, GT3 0x34,
  GT4 0x3C, F3 0x20, F4 0x28, G3 0x30, G4 0x38; bit 1 of the code byte is the semi-transparent flag.
  Record: +0 word (byte 3 = code), vertex indices u16 at +4,+6,+8,(+0xA);
  textured (stride 0x1C): uv0 +0xC, clut +0xE, uv1 +0x10, tpage +0x12, uv2 +0x14, uv3 +0x16, colour
  indices at +0x18 (Gouraud: one per corner; flat textured: one);
  untextured (stride 0x10): colour indices at +0xC (Gouraud: one per corner; flat: one).
Actor models are found by scanning archive members for a header whose bone table is consistent
(firstVert/vertCount chain adds up to vertCount); the pose is the zero-angle rest pose (bone offsets
accumulated down the parent chain), which is only an approximation of the bind pose.

Usage: uv run python tools/export_model.py scan       -> notes/16-models.md (generated)
       uv run python tools/export_model.py export    -> extract/obj/models/*.obj (all that validate)
"""
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dump_areas as da  # noqa: E402
import vram as vr  # noqa: E402

ROOT = da.ROOT
MEMBERS = da.MEMBERS
OUT = ROOT / "extract/obj/models"
STRIDE = {0x24: 0x1C, 0x2C: 0x1C, 0x34: 0x1C, 0x3C: 0x1C, 0x20: 0x10, 0x28: 0x10, 0x30: 0x10, 0x38: 0x10}
NVERT = {0x24: 3, 0x2C: 4, 0x34: 3, 0x3C: 4, 0x20: 3, 0x28: 4, 0x30: 3, 0x38: 4}
NAME = {0x24: "FT3", 0x2C: "FT4", 0x34: "GT3", 0x3C: "GT4", 0x20: "F3", 0x28: "F4", 0x30: "G3", 0x38: "G4"}


def walk_prims(b: bytes, p: int, nvert: int, ncol: int):
    """Parse the primitive runs starting at p. Returns (prims, end offset, ok)."""
    prims, ok = [], True
    while p + 4 <= len(b) and (b[p + 3] & 0xFC) in STRIDE:
        code = b[p + 3] & 0xFC
        n = NVERT[code]
        if p + STRIDE[code] > len(b):
            return prims, p, False
        vi = struct.unpack_from("<4H", b, p + 4)[:n]
        if max(vi) >= nvert:
            ok = False
        rec = {"code": code, "semi": bool(b[p + 3] & 2), "v": vi}
        if code in (0x24, 0x2C, 0x34, 0x3C):
            rec["uv"] = [tuple(b[p + o:p + o + 2]) for o in (0xC, 0x10, 0x14, 0x16)][:n]
            rec["clut"], rec["tpage"] = struct.unpack_from("<H", b, p + 0xE)[0], struct.unpack_from("<H", b, p + 0x12)[0]
            ncolcorner = n if code in (0x34, 0x3C) else 1
            rec["col"] = tuple(b[p + 0x18:p + 0x18 + ncolcorner])
        else:
            ncolcorner = n if code in (0x30, 0x38) else 1
            rec["col"] = tuple(b[p + 0xC:p + 0xC + ncolcorner])
        if rec["col"] and max(rec["col"]) >= ncol:
            ok = False
        prims.append(rec)
        p += STRIDE[code]
    return prims, p, ok


def parse_actor(b: bytes, o: int):
    """Parse a ModelHeader at o; returns a model dict or None if inconsistent."""
    if o < 0 or o + 0x20 > len(b):
        return None
    flags, bc, vc, cc, hn = struct.unpack_from("<IHHHH", b, o)
    if not (2 <= bc <= 64 and 8 <= vc <= 4000 and 1 <= cc <= 1024 and hn <= 1):
        return None
    colors = o + 0xC
    bones = colors + cc * 4
    verts = bones + bc * 12
    vsz = 16 if hn else 8
    prim = verts + vc * vsz
    if prim > len(b):
        return None
    first, blist = 0, []
    for i in range(bc):
        vcnt, fv, x, y, z, par = struct.unpack_from("<HHhhhH", b, bones + i * 12)
        if vcnt and fv != first:
            return None
        if vcnt:
            first = fv + vcnt
        if par != 0xFFFF and i and par >= bc:
            return None
        blist.append((vcnt, fv, (x, y, z), par))
    if first != vc:
        return None
    prims, end, ok = walk_prims(b, prim, vc, cc)
    return {"off": o, "flags": flags, "bones": blist, "vc": vc, "cc": cc, "hn": hn,
            "colors": [tuple(b[colors + i * 4:colors + i * 4 + 3]) for i in range(cc)],
            "verts": [struct.unpack_from("<hhh", b, verts + i * 8) for i in range(vc)],
            "normals": [struct.unpack_from("<hhh", b, verts + vc * 8 + i * 8) for i in range(vc)] if hn else None,
            "prims": prims, "end": end, "ok": ok and bool(prims)}


def rest_positions(m) -> list[tuple[int, int, int]]:
    """Vertices moved by the accumulated bone offsets (zero-angle pose)."""
    n = len(m["bones"])
    acc = [None] * n

    def off(i, depth=0):
        if acc[i] is not None:
            return acc[i]
        _, _, o, par = m["bones"][i]
        base = (0, 0, 0) if par == 0xFFFF or par >= n or par == i or depth > 64 else off(par, depth + 1)
        acc[i] = (base[0] + o[0], base[1] + o[1], base[2] + o[2])
        return acc[i]

    out = [(0, 0, 0)] * m["vc"]
    for i, (vcnt, fv, _, _) in enumerate(m["bones"]):
        ox, oy, oz = off(i)
        for k in range(fv, fv + vcnt):
            x, y, z = m["verts"][k]
            out[k] = (x + ox, y + oy, z + oz)
    return out


def roles() -> dict[tuple[str, int], str]:
    """Known model roles: the player model and each area's monkey LOD models (monkey section header, notes/15)."""
    out = {("008", 0xBC): "player (Spike) - rendered, red hair / blue trousers"}
    for area, member in sorted(da.area_members().items()):
        b = (MEMBERS / f"{member:03X}.bin").read_bytes()
        m = struct.unpack_from("<I", b, 0x4E4)[0]
        hdr = struct.unpack_from("<11i", b, m)
        if hdr[0] == 0x11:
            for lod in range(3):
                out.setdefault((f"{member:03X}", m + hdr[2 + lod]), f"monkey LOD{lod} (area 0x{area:02X})")
    return out


def decode_frame(b: bytes, off: int, nbones: int):
    """One animation frame (nbones + 2 words, AnimDecodeFrame): root offset (x, y, z) and per-bone angles."""
    w = struct.unpack_from(f"<{nbones + 2}I", b, off)
    sx = lambda v, bits: v - (1 << bits) if v >> (bits - 1) else v
    root = (sx(w[1] & 0x3FF, 10), sx((w[1] >> 10) & 0xFFF, 12), sx(w[1] >> 22, 10))
    ang = []
    for i in range(nbones):
        v = w[2 + i]
        ang.append((sx(v & 0x3FF, 10) * 4, sx((v >> 10) & 0xFFF, 12) * 4, sx(v >> 22, 10) * 4))
    return root, ang


def _rot_zyx(ax: int, ay: int, az: int):
    """Rz * Ry * Rx for PSX angles (4096 = 360 degrees)."""
    import math
    cx, sx_ = math.cos(ax * math.pi / 2048), math.sin(ax * math.pi / 2048)
    cy, sy = math.cos(ay * math.pi / 2048), math.sin(ay * math.pi / 2048)
    cz, sz = math.cos(az * math.pi / 2048), math.sin(az * math.pi / 2048)
    rx = ((1, 0, 0), (0, cx, -sx_), (0, sx_, cx))
    ry = ((cy, 0, sy), (0, 1, 0), (-sy, 0, cy))
    rz = ((cz, -sz, 0), (sz, cz, 0), (0, 0, 1))
    mul = lambda a, b: tuple(tuple(sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)) for i in range(3))
    return mul(rz, mul(ry, rx))


def posed_positions(m, root, ang):
    """Skinned vertex positions for a frame: matrix[i] = parent * RotZYX(angle[i]), origin chained through the
    bone offsets (bone parent value p refers to bone p-1; 0 = the root)."""
    mul = lambda a, b: tuple(tuple(sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)) for i in range(3))
    app = lambda r, v: tuple(sum(r[i][k] * v[k] for k in range(3)) for i in range(3))
    ident = ((1, 0, 0), (0, 1, 0), (0, 0, 1))
    mats, orig = [(ident, (0, 0, 0))], []
    pos = [(0, 0, 0)] * m["vc"]
    for i, (vcnt, fv, off, par) in enumerate(m["bones"]):
        o = root if i == 0 else off
        pr, po = mats[par] if par < len(mats) else mats[0]
        r = mul(pr, _rot_zyx(*ang[i]))
        t = tuple(po[k] + app(pr, o)[k] for k in range(3))
        mats.append((r, t))
        for k in range(fv, fv + vcnt):
            q = app(r, m["verts"][k])
            pos[k] = (q[0] + t[0], q[1] + t[1], q[2] + t[2])
    return pos


def parse_static(b: bytes, o: int):
    """Static (bone-less) model, StaticModelInit: u32 flags, u16 ?, u16 vertCount, u16 colorCount, u16 ?,
    colors, 8-byte verts, primitive runs."""
    a4, vc, cc, a10 = struct.unpack_from("<4H", b, o + 4)
    verts = o + 0xC + cc * 4
    prims, end, ok = walk_prims(b, verts + vc * 8, vc, cc)
    return {"off": o, "vc": vc, "cc": cc, "prims": prims, "ok": ok and bool(prims), "end": end,
            "colors": [tuple(b[o + 0xC + i * 4:o + 0xC + i * 4 + 3]) for i in range(cc)],
            "verts": [struct.unpack_from("<hhh", b, verts + i * 8) for i in range(vc)],
            "bones": [(vc, 0, (0, 0, 0), 0xFFFF)], "hn": 0}


def player_statics():
    """The static models of the player data blob (member 0x008): header u16 words at +8/+0xC/+0x10 give word offsets of
    three tables {u32 count, {u16 wordOffset, u16}[count]} (PlayerInit): A = gadget models (slot array 0x800EC060;
    the last entry is the RC car actor model), B = 0x800F4C30, C = 0x800F4630."""
    b = (MEMBERS / "008.bin").read_bytes()
    out = []
    for name, hdr in (("A", 8), ("B", 0xC), ("C", 0x10)):
        base = struct.unpack_from("<H", b, hdr)[0] * 4
        cnt = struct.unpack_from("<I", b, base)[0]
        for i in range(cnt):
            o = base + struct.unpack_from("<H", b, base + 4 + 4 * i)[0] * 4
            if name == "A" and i == cnt - 1:
                continue  # RC car: a skinned model (found by the scan, 008 +0x82F8)
            out.append((f"{name}{i:02d}", parse_static(b, o)))
    return b, out


def scan_members():
    found = []
    for f in sorted(MEMBERS.glob("*.bin")):
        b = f.read_bytes()
        for o in range(0, len(b) - 0x40, 4):
            m = parse_actor(b, o)
            if m:
                found.append((f.stem, o, m))
    return found


def write_obj(path: Path, m, name: str, mats=None, pos=None) -> None:
    pos = pos or rest_positions(m)
    lines = [f"# {name}: {len(m['bones'])} bones, {m['vc']} verts, {len(m['prims'])} prims (rest pose)"]
    if mats:
        lines.append(f"mtllib {path.stem}.mtl")
    nv = 0
    for r in m["prims"]:
        n = len(r["v"])
        if "uv" in r:
            tp = mats.get(r["tpage"], r["clut"]) if mats else f"tp{r['tpage']:04X}_c{r['clut']:04X}"
        else:
            tp = "untextured"
        lines.append(f"usemtl {tp}")
        ids = []
        for k, vi in enumerate(r["v"]):
            x, y, z = pos[vi]
            c = m["colors"][r["col"][k if len(r["col"]) > 1 else 0]]
            lines.append(f"v {x / 256:.4f} {-y / 256:.4f} {z / 256:.4f} {c[0] / 255:.3f} {c[1] / 255:.3f} {c[2] / 255:.3f}")
            if "uv" in r:
                lines.append(f"vt {r['uv'][k][0] / 256:.4f} {1 - r['uv'][k][1] / 256:.4f}")
            nv += 1
            ids.append(nv)
        f = (lambda i: f"{i}/{i}") if "uv" in r else str
        lines.append("f " + " ".join(f(i) for i in ids[:3]))
        if n == 4:
            lines.append("f " + " ".join(f(i) for i in (ids[2], ids[1], ids[3])))
    path.write_text("\n".join(lines) + "\n")


def mesh_models():
    """Static meshes from every area's mesh table (member, index, parsed like a model body)."""
    for area, member in sorted(da.area_members().items()):
        b = (MEMBERS / f"{member:03X}.bin").read_bytes()
        u32 = lambda o: struct.unpack_from("<I", b, o)[0]
        ah = u32(0x4D0)
        mt = ah + u32(ah + 8) * 4
        for i in range(u32(mt)):
            m = mt + struct.unpack_from("<H", b, mt + 4 + i * 4)[0] * 4
            cc, vofs, vc, pofs = struct.unpack_from("<HHHH", b, m + 0xE)
            prims, end, ok = walk_prims(b, m + pofs * 4, vc, cc)
            yield area, member, i, m, cc, vc, prims, ok


def main() -> None:
    mode = sys.argv[1] if len(sys.argv) > 1 else "scan"
    found = scan_members()
    good = [(f, o, m) for f, o, m in found if m["ok"]]
    if mode == "pose":  # export_model.py pose <member> <hdr offset> <anim offset> <frame> : posed OBJ
        mem, hdr, anim, frame = sys.argv[2], int(sys.argv[3], 0), int(sys.argv[4], 0), int(sys.argv[5])
        b = (MEMBERS / f"{mem.upper()}.bin").read_bytes()
        m = parse_actor(b, hdr)
        nb = len(m["bones"])
        root, ang = decode_frame(b, anim + frame * (nb + 2) * 4, nb)
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "tex").mkdir(exist_ok=True)
        write_obj(OUT / f"{mem.upper()}_{hdr:06X}_pose{frame}.obj", m, "posed", vr.Materials(OUT / "tex", vr.global_vram(), "tex/"),
                  posed_positions(m, root, ang))
        print("root", root, "angles", ang[:4])
        return
    if mode == "statics":
        b, models = player_statics()
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "tex").mkdir(exist_ok=True)
        gv = vr.global_vram()
        for name, m in models:
            assert m["ok"], name
            write_obj(OUT / f"008_static_{name}.obj", m, f"player data static model {name}", vr.Materials(OUT / "tex", gv, "tex/"))
        slot = {"A": 0x800EC060, "B": 0x800F4C30, "C": 0x800F4630}
        rows = ["# Player data static models (generated)", "",
                "Generated by `tools/export_model.py statics`. `PlayerInit` copies the three tables of member 0x008 "
                "(header u16 words +8 / +0xC / +0x10) into slot arrays of 0x10-byte static-model instances: "
                "A (19 models + the skinned RC car at 008 +0x82F8) at 0x800EC060, B (20) at 0x800F4C30, C (9) at "
                "0x800F4630. Rendered with `export_model.py statics` + `tools/preview_obj.py`: A00 stun club, A01 net "
                "frame (`FUN_8007BD38` draws it, A14 is the swim variant), A07 slingshot-like yellow Y frame, A18 hex hoop "
                "rings, A11 is drawn by `RcCarDraw` (controller). Slot index = (address - base) / 0x10; the gadget id of "
                "each model is not decoded yet.", "",
                "| Model | Member offset | Slot address | Verts | Colours | Prims |", "|---|---|---|---|---|---|"]
        for name, m in models:
            rows.append(f"| {name} | 0x{m['off']:X} | 0x{slot[name[0]] + 0x10 * int(name[1:]):08X} | {m['vc']} | {m['cc']} | {len(m['prims'])} |")
        (ROOT / "notes/21-player-models.md").write_text("\n".join(rows) + "\n", encoding="utf-8")
        print(f"exported {len(models)} static models")
        return
    if mode == "export":
        OUT.mkdir(parents=True, exist_ok=True)
        gv = vr.global_vram()  # system / player / gadget textures from the kind-0 load lists
        area_of = {}
        for area, member in sorted(da.area_members().items()):
            area_of.setdefault(f"{member:03X}", area)
        area_vram = {}
        for f, o, m in good:
            if f in area_of:  # model inside an area blob: textures of that area's load lists
                if f not in area_vram:
                    area_vram[f] = vr.area_vram(area_of[f])
                mats = vr.Materials(OUT / f"tex_area_{area_of[f]:02X}", area_vram[f], f"tex_area_{area_of[f]:02X}/")
                (OUT / f"tex_area_{area_of[f]:02X}").mkdir(exist_ok=True)
            else:
                mats = vr.Materials(OUT / "tex", gv, "tex/")
            write_obj(OUT / f"{f}_{o:06X}.obj", m, f"member {f} +0x{o:X}", mats)
        print(f"exported {len(good)} actor models to {OUT}")
        return
    codes = Counter()
    rows = ["# Models found in the archive members (generated)", "",
            "Generated by `tools/export_model.py scan`. Actor models = ModelHeader candidates whose bone table is "
            "consistent and whose primitive runs reference only valid vertices/colours (notes/11).", "",
            f"- Actor models found: {len(found)}; fully valid: {len(good)}",
            ]
    for _, _, m in good:
        for r in m["prims"]:
            codes[NAME[r["code"]]] += 1
    bad_meshes = tot_meshes = 0
    mesh_codes = Counter()
    for area, member, i, m, cc, vc, prims, ok in mesh_models():
        tot_meshes += 1
        bad_meshes += not ok
        for r in prims:
            mesh_codes[NAME[r["code"]]] += 1
    rows += [f"- Actor primitives by type: {dict(codes)}",
             f"- Area static meshes: {tot_meshes} (primitive walk valid for {tot_meshes - bad_meshes}); "
             f"primitives {dict(mesh_codes)}", "",
             "| Member | Offset | Bones | Verts | Colours | Normals | Prims | Valid | Role |", "|---|---|---|---|---|---|---|---|---|"]
    rl = roles()
    for f, o, m in found:
        rows.append(f"| {f} | 0x{o:X} | {len(m['bones'])} | {m['vc']} | {m['cc']} | {m['hn']} | {len(m['prims'])} | {'yes' if m['ok'] else 'no'} | {rl.get((f, o), '')} |")
    (ROOT / "notes/16-models.md").write_text("\n".join(rows) + "\n", encoding="utf-8")
    print(rows[4], rows[5], rows[6], sep="\n")


if __name__ == "__main__":
    main()
