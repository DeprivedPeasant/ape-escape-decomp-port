"""Check native actor poses against the independent glTF consumer, then GPU captures.

Run with system Python (installed numpy): python tools/codex/check_native_actor.py
No installation/download. Native binaries and generated assets stay in extract/.
"""
import ctypes as ct
import json
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import export_gltf as gl
from codex.pack_actor import pack_actor
from codex.pack_area import OUT, ROOT
from codex.build_native_viewer import build
from codex.check_native_viewer import run, ppm
from codex.preview_gltf import pose


def library():
    compiler = shutil.which('gcc')
    if not compiler:
        raise RuntimeError('installed compiler unavailable')
    dll = OUT/'native_actor.dll'
    subprocess.run([compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-shared',
                    str(ROOT/'src/render/native_actor.c'), '-o', str(dll), '-lopengl32', '-static-libgcc'], check=True)
    lib = ct.CDLL(str(dll))
    lib.ae_actor_load.argtypes = [ct.c_char_p]; lib.ae_actor_load.restype = ct.c_void_p
    lib.ae_actor_error.restype = ct.c_char_p
    lib.ae_actor_free.argtypes = [ct.c_void_p]
    lib.ae_actor_info.argtypes = [ct.c_void_p, ct.POINTER(ct.c_uint32)]
    lib.ae_actor_pose.argtypes = [ct.c_void_p, ct.c_uint32, ct.c_uint32]; lib.ae_actor_pose.restype = ct.c_int
    lib.ae_actor_positions.argtypes = [ct.c_void_p, ct.POINTER(ct.c_float), ct.c_uint32]
    lib.ae_actor_positions.restype = ct.c_int
    return lib


def corruptions(lib, path):
    blob = path.read_bytes()
    nn, nj, nv, nb, nt = struct.unpack_from('<5I', blob, 12)
    joint, batch = 64+nn*44, 64+nn*44+nj*68
    vertex, clip = batch+nb*16, batch+nb*16+nv*36
    fields = [('signature', 0, b'BADACTOR'), ('version', 8, struct.pack('<I', 2)),
              ('node count', 12, struct.pack('<I', 257)), ('joint count', 16, struct.pack('<I', nn+1)),
              ('vertex count', 20, struct.pack('<I', 0xFFFFFFFF)), ('clip count', 32, struct.pack('<I', 0)),
              ('parent index', 64, struct.pack('<i', nn)), ('self cycle', 64, struct.pack('<i', 0)),
              ('invalid quaternion', 64+4+12, struct.pack('<f', 1e5)),
              ('joint node index', joint, struct.pack('<I', nn)),
              ('inverse bind NaN', joint+4, struct.pack('<f', float('nan'))),
              ('batch gap', batch, struct.pack('<I', 3)), ('batch texture', batch+8, struct.pack('<I', nt)),
              ('vertex joint', vertex+32, struct.pack('<I', nj)),
              ('vertex NaN', vertex, struct.pack('<f', float('nan'))),
              ('zero frames', clip+64, struct.pack('<I', 0)), ('invalid fps', clip+68, struct.pack('<f', 0)),
              ('clip sample NaN', clip+72, struct.pack('<f', float('nan')))]
    cases = [(name, blob[:offset]+replacement+blob[offset+len(replacement):]) for name,offset,replacement in fields]
    cases += [('unterminated clip name', blob[:clip]+b'X'*64+blob[clip+64:]),
              ('header truncation', blob[:32]), ('pixel truncation', blob[:-1]), ('trailing bytes', blob+b'\0')]
    outcomes = []
    fixture = OUT/'invalid_actor.aea'
    for name, data in cases:
        fixture.write_bytes(data)
        actor = lib.ae_actor_load(str(fixture).encode())
        if actor:
            lib.ae_actor_free(actor)
            raise AssertionError(f'corrupt actor accepted: {name}')
        outcomes.append(dict(case=name, error=lib.ae_actor_error().decode()))
    return outcomes


def main():
    packed = pack_actor()
    lib = library()
    actor = lib.ae_actor_load(packed['path'].encode())
    if not actor:
        raise AssertionError(lib.ae_actor_error().decode())
    info = (ct.c_uint32*4)(); lib.ae_actor_info(actor, info)
    assert list(info) == [packed['nodes'], packed['joints'], packed['vertices'], len(packed['clips'])]
    gpath = Path(packed['gltf']); g = json.loads(gpath.read_text())
    arrays = [np.array(a) for a in gl.validate(g, (gpath.parent/g['buffers'][0]['uri']).read_bytes())]
    output = (ct.c_float*(info[2]*3))()
    max_error, compared, rows = 0., 0, []
    try:
        for clip, metadata in enumerate(packed['clips']):
            error = 0.
            for frame in range(metadata['frames']):
                assert lib.ae_actor_pose(actor, clip, frame) == 1
                assert lib.ae_actor_positions(actor, output, len(output)) == 1
                native = np.ctypeslib.as_array(output).reshape(-1, 3)
                # Use accessor time rather than recomputing float32 endpoints.
                time = arrays[g['animations'][clip]['samplers'][0]['input']][frame, 0]
                primitives = pose(g, arrays, g['animations'][clip], float(time))
                expected = np.concatenate([p['positions'][p['indices'].reshape(-1)] for p in primitives])
                delta = float(np.abs(native-expected).max())
                assert delta < 5e-6, (metadata['name'], frame, delta)
                error = max(error, delta); compared += 1
            max_error = max(max_error, error)
            rows.append(dict(**metadata, max_error=error))
            if (clip+1)%20 == 0:
                print(f'Native pose checks: {clip+1} clips / {compared} frames; max error {max_error:.3g}', flush=True)
        assert lib.ae_actor_pose(actor, len(packed['clips']), 0) == 0
        assert lib.ae_actor_pose(actor, 0, packed['clips'][0]['frames']) == 0
        assert lib.ae_actor_positions(actor, output, len(output)-1) == 0
    finally:
        lib.ae_actor_free(actor)
    invalid = corruptions(lib, Path(packed['path']))
    executable = build()
    scene = OUT/'area_01.aev'
    baseline = OUT/'spike_absent.ppm'
    camera = ['--target', '0', '8.6', '0', '--distance', '4', '--yaw', '0', '--pitch', '10']
    run(executable, scene, '--capture', baseline, *camera)
    baseline_pixels = ppm(baseline)
    captures = []
    images = []
    for name, clip, frame in [('stand0',0,0), ('stand14',0,14), ('walk0',3,0), ('walk5',3,5), ('jump0',8,0)]:
        path = OUT/f'spike_{name}.ppm'
        fps = packed['clips'][clip]['fps']
        run(executable, scene, '--actor', packed['path'], '--actor-pos', '0', '8', '0', '--clip', clip,
            '--time', frame/fps+1e-7, '--capture', path, *camera)
        pixels = ppm(path); images.append(pixels)
        changed = sum(any(pixels[i+k] != baseline_pixels[i+k] for k in range(3)) for i in range(0,len(pixels),3))
        assert changed > 1000, f'actor not visible: {name}/{changed}'
        captures.append(dict(name=name, clip=clip, frame=frame, actor_pixels=changed))
    changed = sum(any(images[0][i+k] != images[1][i+k] for k in range(3)) for i in range(0,len(images[0]),3))
    assert changed > 50, 'stand animation not visible'
    run(executable, scene, '--actor', packed['path'], '--actor-pos', '0', '8', '0', '--frames', '120')
    result = dict(packed=packed, clips=rows, frames=compared, vertex_positions=compared*info[2], max_error=max_error,
                  invalid_actors=invalid, captures=captures, stand_changed_pixels=changed, failures=[])
    (OUT/'actor-validation.json').write_text(json.dumps(result, indent=2)+'\n')
    lines = ['# Native animated Spike integration (generated)', '',
             'Generated by `python tools/codex/check_native_actor.py` using existing system numpy.', '',
             '## Result', '',
             'The native Win32 viewer now renders Spike with a rigid-weight skeletal skin and all '
             '134 exported state/gadget clips. CPU evaluation reconstructs node TRS matrices, follows '
             'the parent hierarchy, multiplies inverse bind matrices and transforms mesh vertices. '
             'Animation plays at the stored sampled rate (30 Hz for Spike), independent of rendering rate. '
             'No Python or glTF parser runs inside the native executable.', '',
             '```powershell',
             'uv run --offline python tools/codex/build_native_viewer.py --spike --run',
             'uv run --offline python tools/codex/build_native_viewer.py --spike --capture',
             'python tools/codex/check_native_actor.py',
             '```', '',
             'F focuses the camera on Spike, N cycles clips, Space pauses/resumes animation. '
             'Existing camera controls remain available. The build runner now reads original area entry '
             'and terrain placement (notes/67-68); the independent actor captures below deliberately '
             'use fixed diagnostic placement. CLI options: `--actor path.aea`, `--clip index`, '
             '`--time seconds` (fixed capture pose), `--actor-pos x y z`, `--focus-actor`, and '
             '`--target x y z`. These diagnostic captures use explicit placement (0,8,0) in area 01; this is not '
             'an asserted original spawn location. Do not use that placement unmodified in other areas.', '',
             '## Independent validation', '',
             f'- Joints: {packed["joints"]}; hierarchy nodes: {packed["nodes"]}; mesh triangles: {packed["triangles"]}.',
             f'- Clips checked: {len(rows)}; sampled poses: {compared}; expanded vertex positions compared: {compared*info[2]:,}.',
             f'- Maximum native-versus-independent-consumer error: {max_error:.9g} exported units (threshold 0.000005).',
             f'- Malformed actor packs rejected: {len(invalid)}; API boundary errors rejected: 3.',
             '- Animated 120-frame window/event-loop smoke: passed.',
             f'- GPU actor captures: {len(captures)}; every capture differs from an identical camera/scene without the actor.',
             f'- Stand frame 0 versus 14 changed pixels: {changed:,}.',
             '- Failures: 0.', '',
             '| Capture | Clip | Frame | Pixels changed from scene without Spike |',
             '|---|---:|---:|---:|']
    lines += [f"| {r['name']} | {r['clip']} | {r['frame']} | {r['actor_pixels']} |" for r in captures]
    lines += ['', '## Prepared format and limits', '',
              'AEACTR1 is a new little-endian asset adapter: 64-byte header (magic, version, node/joint/'
              'vertex/batch/texture/clip counts, zero flags and bind-pose bounds); nodes are parent s32 '
              'and 10 TRS floats; joints are node u32 and 16 column-major inverse-bind floats; batches '
              'are first/count/texture/kind u32; expanded vertices contain 8 xyz/uv/rgb floats plus joint '
              'u32; clips have a 64-byte NUL-terminated name, frameCount u32, fps float and '
              'frame-major/node-major 10-float TRS samples; textures are width/height/bytes u32 plus RGBA8.', '',
              'The loader bounds allocations, rejects hierarchy cycles, checks transforms, quaternions, '
              'joints, batch coverage, vertices, timing and texture lengths. Current adapter requires '
              'the exporter\'s single rigid-weight skin, uniform samples and TRS nodes. It renders exact '
              'stored sampled poses; PSX fixed-point differences and source interpolation limitations '
              'remain those documented in notes/50 and 55. Material lighting/blending remains approximate.', '',
              'This integrates the animated model, not the original PlayerUpdate/state machine, '
              'ground/collider contacts, gadget logic, enemies or gameplay. Source-backed ground probing '
              'and spawn placement are the next steps. Full results/captures are ignored under '
              '`extract/codex/native/actor-validation.json`.']
    (ROOT/'notes/66-native-animated-actor.md').write_text('\n'.join(lines)+'\n')
    print(f'PASS: {len(rows)} clips/{compared} frames, {len(invalid)} corruption cases, {len(captures)} GPU captures', flush=True)


if __name__ == '__main__':
    main()
