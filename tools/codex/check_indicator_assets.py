"""Known one-bone indicator parsing and native poses against packed channels."""
import ctypes as C
import json
import math
import struct
import numpy as np
from pathlib import Path
from check_native_actor import library
from codex.indicator_assets import parse_indicator_actor
from codex.feature_sources import inventory
from codex.pack_actor import pack_actor
from codex.pack_area import ROOT, OUT
import export_gltf as gl


def signed(value, bits):
    return (value & ((1 << bits)-1))-(1 << bits) if value & (1 << (bits-1)) else value & ((1 << bits)-1)


def source_pose(blob, model, stream, frame):
    count = struct.unpack_from('<I', blob, stream)[0]
    translation = np.array(model['bones'][0][2], dtype=float)
    angles = [0, 0, 0]; scale = np.ones(3)
    for i in range(count):
        bone, _, kind, _ = struct.unpack_from('<4B', blob, stream+4+i*4)
        assert bone == 1
        word = struct.unpack_from('<I', blob, stream+4+count*4+(frame*count+i)*4)[0]
        vector = [signed(word >> shift, bits) for shift, bits in ((0, 10), (10, 12), (22, 10))]
        if kind == 1:
            angles = [v*4*2*math.pi/4096 for v in vector]
        elif kind == 2:
            translation = np.array(vector, dtype=float)
        elif kind == 3:
            scale = np.array(vector, dtype=float)/256
        else:
            raise AssertionError(kind)
    x, y, z = angles; sx, cx = math.sin(x), math.cos(x); sy, cy = math.sin(y), math.cos(y); sz, cz = math.sin(z), math.cos(z)
    rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    vertices = np.array(model['verts'], dtype=float)
    result = (vertices*scale)@(rz@ry@rx).T+translation
    return result*np.array([1, -1, 1])/256


def main():
    _, rows = inventory(); lib = library(); models = poses = failures = 0; worst = 0
    fixture = None
    for row in rows:
        if row['kind'] not in (0x10, 0x11):
            continue
        blob = (ROOT/f'extract/members/{row["member"]}.bin').read_bytes()
        model = parse_indicator_actor(blob, row['model'], row['stream'])
        assert model and model['ok']
        path = ROOT/f'extract/gltf/{row["member"]}_{row["model"]:06X}/actor.gltf'
        g = json.loads(path.read_text()); arrays = gl.validate(g, (path.parent/'actor.bin').read_bytes())
        assert len(g['skins'][0]['joints']) == row['joints'] and len(g['animations']) == 2
        models += 1
        if row['joints'] != 1:
            continue
        fixture = blob, row
        packed = pack_actor(path); actor = lib.ae_actor_load(packed['path'].encode()); assert actor
        try:
            info = (C.c_uint32*4)(); lib.ae_actor_info(actor, info); assert info[1] == 1
            output = (C.c_float*(info[2]*3))()
            for clip, metadata in enumerate(packed['clips']):
                first = 0 if clip == 0 else (29 if row['kind'] == 0x10 else 58)
                for frame in range(metadata['frames']):
                    assert lib.ae_actor_pose(actor, clip, frame) and lib.ae_actor_positions(actor, output, len(output))
                    expected = source_pose(blob, model, row['stream'], first+frame)
                    # Match triangle expansion to the validated glTF's vertex order.
                    ordered = []
                    source_vertices = np.array(model['verts'], dtype=float)*[1, -1, 1]/256
                    for primitive in g['meshes'][0]['primitives']:
                        positions = arrays[primitive['attributes']['POSITION']]
                        for index, in arrays[primitive['indices']]:
                            position = np.array(positions[index])
                            distances = np.max(np.abs(source_vertices-position), axis=1)
                            matches = np.where(distances < 1e-6)[0]; assert len(matches)
                            ordered.append(expected[matches[0]])
                    native = np.ctypeslib.as_array(output).reshape(-1, 3)
                    delta = float(np.max(np.abs(native-np.array(ordered))))
                    assert delta < 5e-6, (row['area'], clip, frame, delta)
                    worst = max(worst, delta); poses += 1
        finally:
            lib.ae_actor_free(actor)
    blob, row = fixture; offset = row['model']
    bone = offset+12+struct.unpack_from('<H', blob, offset+8)[0]*4
    for field, value in ((offset+6, 0), (offset+8, 0), (offset+10, 2), (bone, 33), (bone+2, 1), (bone+10, 1)):
        malformed = bytearray(blob); struct.pack_into('<H', malformed, field, value)
        assert parse_indicator_actor(malformed, offset, row['stream']) is None, field
        failures += 1
    for end in (offset+11, bone+11, bone+12+33*8):
        assert parse_indicator_actor(blob, offset, end) is None
        failures += 1
    report = dict(structural_models=models, known_one_bone_models=19, source_channel_native_poses=poses,
                  maximum_position_error=worst, malformed_checks=failures, failures=0)
    (OUT/'indicator-assets-validation.json').write_text(json.dumps(report, indent=2)+'\n')
    (ROOT/'notes/port/validation/indicator_assets_validation.md').write_text(
        '# Indicator asset validation\n\nGenerated by `check_indicator_assets.py`.\n\n'+
        '\n'.join(f'- {k}: {v}' for k, v in report.items())+'\n\n'+
        'All known feature exports pass structural glTF checks. Native one-bone poses agree with '
        'independently read packed channel words using continuous Rz*Ry*Rx transforms and original '
        'positions/scales. This validates decoded asset geometry, not original integer/GTE rendering rounding.\n')
    print(report)


if __name__ == '__main__':
    main()
