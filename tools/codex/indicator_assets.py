"""Decode one-bone models at known area indicator feature pointers.

The general scanner deliberately starts at two bones. This narrow decoder uses
the same ModelHeader layout and shared primitive walker without changing that
scanner. A supplied limit prevents primitives crossing into the channel stream.
"""
import struct
import export_model as em


def parse_indicator_actor(blob, offset, limit=None):
    end = len(blob) if limit is None else min(limit, len(blob))
    if offset < 0 or offset+12 > end:
        return None
    flags, bones, vertices, colors, normals = struct.unpack_from('<IHHHH', blob, offset)
    if bones != 1:
        return em.parse_actor(blob, offset)
    if not (8 <= vertices <= 4000 and 1 <= colors <= 1024 and normals <= 1):
        return None
    color_start = offset+12
    bone_start = color_start+colors*4
    vertex_start = bone_start+12
    primitive_start = vertex_start+vertices*(16 if normals else 8)
    if primitive_start > end:
        return None
    count, first, x, y, z, parent = struct.unpack_from('<HHhhhH', blob, bone_start)
    # Root parent0 is the recovered p-1 convention. Do not normalize an unknown
    # parent sentinel into a guessed hierarchy.
    if count != vertices or first != 0 or parent != 0:
        return None
    primitives, primitive_end, ok = em.walk_prims(blob[:end], primitive_start, vertices, colors)
    if not ok or not primitives:
        return None
    return dict(off=offset, flags=flags, bones=[(count, first, (x, y, z), parent)],
                vc=vertices, cc=colors, hn=normals,
                colors=[tuple(blob[color_start+i*4:color_start+i*4+3]) for i in range(colors)],
                verts=[struct.unpack_from('<hhh', blob, vertex_start+i*8) for i in range(vertices)],
                normals=[struct.unpack_from('<hhh', blob, vertex_start+vertices*8+i*8)
                         for i in range(vertices)] if normals else None,
                prims=primitives, end=primitive_end, ok=True)
