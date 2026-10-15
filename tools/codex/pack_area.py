"""Prepare local area assets for the native viewer; no game bytes enter source control.

Imports the existing OBJ exporter without modifying it. The runtime pack is deliberately
simple: LE header, draw batches, expanded xyz/uv/rgb vertices, then raw RGBA textures.
The native executable owns loading, validation, texture upload and rendering.
"""
import argparse
import json
import math
import re
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import dump_areas as da
import export_area_obj as obj
from codex.check_exports import png_check
from codex.prop_texture_layers import split
from codex.terrain_materials import samples as terrain_samples

ROOT = da.ROOT
OUT = ROOT / 'extract/codex/native'
HEADER = struct.Struct('<8s8I6f')
MAGIC = b'AEVIEW1\0'
MAGIC2 = b'AEVIEW2\0'
MAGIC3 = b'AEVIEW3\0'


def png_rgba(path):
    data = path.read_bytes()
    png_check(data)
    cursor, compressed = 8, bytearray()
    while cursor < len(data):
        size = struct.unpack_from('>I', data, cursor)[0]
        if data[cursor+4:cursor+8] == b'IDAT':
            compressed.extend(data[cursor+8:cursor+8+size])
        cursor += size+12
    rows = zlib.decompress(compressed)
    # OBJ UVs have already been flipped into bottom-left origin; upload matching rows.
    return b''.join(rows[y*1025+1:y*1025+1025] for y in reversed(range(256)))


def terrain_mask(member):
    blob = (da.MEMBERS / f'{member:03X}.bin').read_bytes()
    ah = struct.unpack_from('<I', blob, 0x4D0)[0]
    flags, offset = struct.unpack_from('<II', blob, ah)
    if flags & 3 == 3:
        return []
    grid = ah + offset*4
    width, depth = struct.unpack_from('<HH', blob, grid)
    cells = struct.unpack_from(f'<{width*depth}I', blob, grid+8)
    return [bool(word & 1) and not bool(flags & 1) for word in cells]


NONE = 0xFFFFFFFF
PROP_GROUP = re.compile(r'prop_(\d+)_mesh(\d+)$')


def prop_flags(meta, textured):
    """Batch flags of one prop triangle: bit0 raw semi, bits1-2 blend mode (tpage for textured, source draw-mode word
    for untextured), bit3 untextured semi, bit4 untextured draw mode resolved from descriptor byte +2. A bit3 batch
    without bit4 is the legacy unknown-mode fallback in older packs."""
    if not meta['semi']:
        return 0
    return 1 | (meta['blend_mode'] << 1) | (0 if textured else 8 | 16)


def sky_vertex(vertex, quarter):
    """Original MeshTransformRot90 quarter turn of one raw signed-16 vertex, each result stored as 16 bits with wrap.
    The type 4 player-Y offset is NOT applied here: the viewer applies it at run time (native_sky.c)."""
    x, y, z = vertex
    x, y, z = ((x, y, z), (z, y, -x), (-x, y, -z), (-z, y, x))[quarter]
    return tuple(((c+32768) & 0xFFFF)-32768 for c in (x, y, z))


def pack(area, refresh=False, prop_materials=False, sky_props=False):
    """Write the viewer pack. prop_materials=False is the legacy AEVIEW1 pack, byte for byte; True writes the
    distinct AEVIEW2 pack with per-prop source material (semi bit / blend mode / split texture layers).
    sky_props=True (implies prop_materials) writes the distinct AEVIEW3 pack (area_XX_sky.aev): the same plus a
    per-batch sky_type, with type 4/5 sky prop vertices taken from the raw source mesh (original quarter turn and
    16-bit wrap applied once here, no placement translation, stored x/256, -y/256, z/256)."""
    prop_materials = prop_materials or sky_props
    members = da.area_members()
    if area not in members:
        raise ValueError(f'area 0x{area:X} has no active data chain')
    member = members[area]
    folder = obj.OUT / f'area_{area:02X}'
    path = folder / f'area_{area:02X}.obj'
    if refresh or not path.exists():
        obj.export(area, member, obj.uv_table())
    obj_lines = path.read_text().splitlines()
    used_materials = {line.split()[1] for line in obj_lines if line.startswith('usemtl ')}
    materials, textures, texture_data = {}, {}, []
    material = None
    for line in (folder / f'area_{area:02X}.mtl').read_text().splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == 'newmtl':
            material = fields[1]
            materials[material] = 0xFFFFFFFF
        elif fields[0] == 'map_Kd':
            if material not in used_materials:
                continue  # Exporter declares a terrain material even in terrainless areas.
            texture_path = (folder / fields[1]).resolve()
            if not texture_path.is_relative_to(folder.resolve()):
                raise ValueError('texture path escapes area folder')
            if texture_path not in textures:
                textures[texture_path] = len(texture_data)
                texture_data.append(png_rgba(texture_path))
            materials[material] = textures[texture_path]
    positions, uvs, vertices, batches = [], [], [], []
    mask, terrain_face, suppressed = terrain_mask(member), 0, 0
    terrain_materials = terrain_samples(member)
    kind, material = 1, 'white'
    counts = [0, 0, 0]
    meta, meta_cursor, group, layers, layer_vram = [], 0, None, {}, None
    if prop_materials:
        from codex.prop_materials import triangles as prop_triangles
        meta = prop_triangles(member)
    sky_records, sky_cursor, sky_triangles = {}, {}, 0
    if sky_props:
        from codex import sky_props as sky_source
        for record in sky_source.load(member):
            sky_records[record['prop_index']] = record
            sky_cursor[record['prop_index']] = 0
    bounds = [[math.inf]*3, [-math.inf]*3]
    focus = [[math.inf]*3, [-math.inf]*3]
    for line in obj_lines:
        fields = line.split()
        if not fields or fields[0].startswith('#'):
            continue
        if fields[0] == 'v':
            values = tuple(map(float, fields[1:]))
            if len(values) not in (3, 6) or not all(map(math.isfinite, values)):
                raise ValueError('invalid OBJ vertex')
            positions.append(values if len(values) == 6 else values+(1., 1., 1.))
        elif fields[0] == 'vt':
            uvs.append(tuple(map(float, fields[1:3])))
        elif fields[0] == 'o':
            kind = 0 if fields[1] == 'terrain' else 2 if fields[1].startswith('collider_') else 1
            group = PROP_GROUP.match(fields[1]) if kind == 1 else None
            if prop_materials and kind == 1 and not group:
                raise ValueError(f'unrecognised prop group {fields[1]}')
        elif fields[0] == 'usemtl':
            material = fields[1]
        elif fields[0] == 'f':
            if len(fields) != 4:
                raise ValueError('viewer pack expects triangular exporter output')
            if kind == 0:
                cell = terrain_face//4
                terrain_face += 1
                if cell >= len(mask):
                    raise ValueError('terrain face count exceeds source grid')
                if not mask[cell]:
                    suppressed += 1
                    continue
            texture = materials[material]
            semi_texture, flags = NONE, 0
            sky_type, sky_triangle, sky_record = 0, None, None
            if prop_materials and kind == 1:
                if meta_cursor >= len(meta):
                    raise ValueError('OBJ prop triangles exceed raw primitive metadata')
                raw, meta_cursor = meta[meta_cursor], meta_cursor+1
                if (raw['prop_index'], raw['mesh_index']) != (int(group[1]), int(group[2])):
                    raise ValueError(f'prop triangle {meta_cursor-1} belongs to {group[0]}, raw metadata says '
                                     f'prop_{raw["prop_index"]}_mesh{raw["mesh_index"]}')
                uv_flags = [len(f.split('/')) > 1 and bool(f.split('/')[1]) for f in fields[1:]]
                if len(set(uv_flags)) != 1 or uv_flags[0] != (raw['tpage'] is not None):
                    raise ValueError(f'prop triangle {meta_cursor-1}: OBJ texturing differs from raw metadata')
                if raw['tpage'] is None:
                    texture = NONE  # the exporter leaves the previous usemtl active; untextured is never textured
                else:
                    key = raw['material_key']
                    if material != f'tp{key[0]:03X}_c{key[1]:04X}' or texture == NONE:
                        raise ValueError(f'prop triangle {meta_cursor-1}: OBJ material {material} differs from raw '
                                         f'tpage 0x{raw["tpage"]:04X} clut 0x{raw["clut"]:04X}')
                flags = prop_flags(raw, raw['tpage'] is not None)
                if raw['semi'] and raw['tpage'] is not None:
                    if key not in layers:
                        if layer_vram is None:
                            import vram as vr
                            layer_vram = bytes(vr.area_vram(area))
                        opaque, semi = split(layer_vram, *key)
                        layers[key] = len(texture_data)
                        texture_data.extend((opaque, semi))
                    texture, semi_texture = layers[key], layers[key]+1
                sky_record = sky_records.get(raw['prop_index'])
                if sky_record is not None:
                    if sky_record['mesh_index'] != raw['mesh_index']:
                        raise ValueError(f'sky placement {raw["prop_index"]} mesh differs from raw primitive metadata')
                    index = sky_cursor[raw['prop_index']]
                    if index >= len(sky_record['triangles']):
                        raise ValueError(f'prop {raw["prop_index"]}: OBJ sky triangles exceed source triangles')
                    sky_triangle = sky_record['triangles'][index]
                    sky_cursor[raw['prop_index']] = index+1
                    sky_type = sky_record['type']
                    sky_triangles += 1
            if sky_props:
                batch_key = [texture, kind, semi_texture, flags, sky_type]
            elif prop_materials:
                batch_key = [texture, kind, semi_texture, flags]
            else:
                batch_key = [texture, kind]
            if not batches or batches[-1][2:] != batch_key:
                batches.append([len(vertices), 0, *batch_key])
            for corner_index, face in enumerate(fields[1:]):
                indices = face.split('/')
                vi = int(indices[0])-1
                if not 0 <= vi < len(positions):
                    raise ValueError('OBJ vertex index out of bounds')
                x, y, z, r, g, b = positions[vi]
                if sky_triangle is not None:
                    # Sky geometry comes straight from the raw source vertices (OBJ keeps only UV / colour).
                    source = sky_triangle[corner_index]
                    if not 0 <= source < sky_record['vertex_count']:
                        raise ValueError('sky source vertex index out of bounds')
                    sx, sy, sz = sky_vertex(sky_record['vertices'][source], sky_record['original_quarter_turn'])
                    x, y, z = sx/256, -sy/256, sz/256
                uv = (0., 0.)
                if len(indices) > 1 and indices[1]:
                    ti = int(indices[1])-1
                    if not 0 <= ti < len(uvs):
                        raise ValueError('OBJ UV index out of bounds')
                    uv = uvs[ti]
                if kind == 0:
                    corner=vi-cell*5
                    if not 0<=corner<5:
                        raise ValueError('terrain exporter vertex order differs from native adapter')
                    u,v,r,g,b=terrain_materials[cell][corner]
                    uv=(u,v)
                # PSX texture modulation uses 128 as neutral; OpenGL uses 1.0.
                rgb = tuple(min(1., max(0., c*(2 if texture != 0xFFFFFFFF else 1))) for c in (r, g, b))
                vertices.append((x, y, z, *uv, *rgb))
                if kind != 2:
                    for axis, value in enumerate((x, y, z)):
                        bounds[0][axis] = min(bounds[0][axis], value)
                        bounds[1][axis] = max(bounds[1][axis], value)
                        if kind == 0:
                            focus[0][axis] = min(focus[0][axis], value)
                            focus[1][axis] = max(focus[1][axis], value)
            batches[-1][1] += 3
            counts[kind] += 1
    if terrain_face != len(mask)*4:
        raise ValueError('exported terrain face count differs from source grid')
    if not counts[0]+counts[1]:
        raise ValueError('empty visible geometry')
    if prop_materials and meta_cursor != len(meta):
        raise ValueError(f'raw primitive metadata has {len(meta)} triangles, OBJ props use {meta_cursor}')
    for index, record in sky_records.items():
        if sky_cursor[index] != len(record['triangles']):
            raise ValueError(f'sky placement {index}: source has {len(record["triangles"])} triangles, '
                             f'OBJ props use {sky_cursor[index]}')
    version = 3 if sky_props else 2 if prop_materials else 1
    payload = bytearray(HEADER.pack((MAGIC, MAGIC2, MAGIC3)[version-1], version, area, member,
                                    len(vertices), len(batches), len(texture_data), counts[0], counts[1],
                                    *bounds[0], *bounds[1]))
    for batch in batches:
        payload.extend(struct.pack(('<4I', '<6I', '<7I')[version-1], *batch))
    for vertex in vertices:
        payload.extend(struct.pack('<8f', *vertex))
    for pixels in texture_data:
        payload.extend(struct.pack('<3I', 256, 256, len(pixels)))
        payload.extend(pixels)
    OUT.mkdir(parents=True, exist_ok=True)
    packed = OUT / (f'area_{area:02X}_sky.aev' if sky_props else
                    f'area_{area:02X}_materials.aev' if prop_materials else f'area_{area:02X}.aev')
    packed.write_bytes(payload)
    report = dict(area=area, member=member, vertices=len(vertices), batches=len(batches),
                  textures=len(texture_data), terrain_triangles=counts[0], prop_triangles=counts[1],
                  collider_triangles=counts[2], hidden_terrain_triangles=suppressed,
                  bytes=len(payload), bounds=bounds, focus_bounds=focus if counts[0] else bounds,
                  pack=str(packed))
    if prop_materials:
        report.update(version=version, split_layer_pairs=len(layers),
                      semi_textured_triangles=sum(b[1]//3 for b in batches if b[4] != NONE),
                      untextured_semi_triangles=sum(b[1]//3 for b in batches if b[5] & 8),
                      untextured_resolved_triangles=sum(b[1]//3 for b in batches if b[5] & 16),
                      semi_triangles=sum(b[1]//3 for b in batches if b[5] & 1))
    if sky_props:
        report.update(sky_placements=len(sky_records), sky_triangles=sky_triangles)
    packed.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('area', nargs='?', default='0x01')
    parser.add_argument('--all', action='store_true')
    parser.add_argument('--refresh', action='store_true')
    parser.add_argument('--prop-materials', action='store_true',
                        help='write the distinct AEVIEW2 pack with source semi/blend prop materials')
    parser.add_argument('--sky-props', action='store_true',
                        help='write the distinct AEVIEW3 pack (area_XX_sky.aev) with source type 4/5 sky props drawn '
                             'as a background pass (opt-in, host approximation; implies --prop-materials)')
    args = parser.parse_args()
    areas = sorted(da.area_members()) if args.all else [int(args.area, 0)]
    for area in areas:
        result = pack(area, args.refresh, args.prop_materials, args.sky_props)
        sky = (f", {result['sky_placements']} sky placements / {result['sky_triangles']} sky triangles"
               if args.sky_props else '')
        print(f"area {area:02X}: {result['terrain_triangles']} terrain / {result['prop_triangles']} prop triangles{sky}, "
              f"{result['textures']} textures, {result['bytes']:,} bytes", flush=True)


if __name__ == '__main__':
    main()
