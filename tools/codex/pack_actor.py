"""Adapt validated glTF rigid skins and sampled TRS clips for the native runtime.

Uses standard library only. Default input is the existing Spike export.
AEACTR1 is a prepared asset format, not a recovered PSX format.
"""
import argparse
import copy
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import export_gltf as gl
from codex.pack_area import ROOT, OUT, HEADER, png_rgba


def pack_actor(path=None):
    path = path or gl.OUT/'008_0000BC/actor.gltf'
    path = path.resolve()
    if not path.is_relative_to(gl.OUT.resolve()):
        raise ValueError('actor input must be under extract/gltf')
    g = json.loads(path.read_text())
    arrays = gl.validate(g, (path.parent/g['buffers'][0]['uri']).read_bytes())
    if len(g['skins']) != 1 or len(g['meshes']) != 1:
        raise ValueError('native adapter expects one rigid-weight skin/mesh')
    parents = [-1]*len(g['nodes'])
    rest = []
    for i, node in enumerate(g['nodes']):
        if 'matrix' in node:
            raise ValueError('native adapter expects TRS nodes')
        rest.append([*node.get('translation', [0, 0, 0]), *node.get('rotation', [0, 0, 0, 1]),
                     *node.get('scale', [1, 1, 1])])
        for child in node.get('children', []):
            parents[child] = i
    vertices, batches, textures, texture_ids = [], [], [], {}
    bounds = [[math.inf]*3, [-math.inf]*3]
    for primitive in g['meshes'][0]['primitives']:
        attrs = primitive['attributes']
        pbr = g['materials'][primitive['material']]['pbrMetallicRoughness']
        texture = 0xFFFFFFFF
        if 'baseColorTexture' in pbr:
            source = g['textures'][pbr['baseColorTexture']['index']]['source']
            if source not in texture_ids:
                texture_ids[source] = len(textures)
                texture_path = (path.parent/g['images'][source]['uri']).resolve()
                if not texture_path.is_relative_to(path.parent):
                    raise ValueError('image escapes actor folder')
                textures.append(png_rgba(texture_path))
            texture = texture_ids[source]
        first = len(vertices)
        for index, in arrays[primitive['indices']]:
            position = arrays[attrs['POSITION']][index]
            joints = arrays[attrs['JOINTS_0']][index]
            weights = arrays[attrs['WEIGHTS_0']][index]
            if weights != (1., 0., 0., 0.) and list(weights) != [1., 0., 0., 0.]:
                raise ValueError('native adapter currently supports exact rigid bone weights')
            color = arrays[attrs['COLOR_0']][index] if 'COLOR_0' in attrs else (1., 1., 1.)
            uv = arrays[attrs['TEXCOORD_0']][index] if 'TEXCOORD_0' in attrs else (0., 0.)
            vertices.append((*position, uv[0], 1-uv[1], *[min(1., c*2) for c in color], joints[0]))
            for axis, value in enumerate(position):
                bounds[0][axis] = min(bounds[0][axis], value)
                bounds[1][axis] = max(bounds[1][axis], value)
        batches.append((first, len(vertices)-first, texture, 1))
    clips = []
    for animation in g['animations']:
        times = [row[0] for row in arrays[animation['samplers'][0]['input']]]
        fps = g['extras']['fps']
        if not all(abs(t-i/fps) < 1e-5 for i, t in enumerate(times)):
            raise ValueError('native adapter expects uniform zero-origin sampled clips')
        samples = [copy.deepcopy(rest) for _ in times]
        for channel in animation['channels']:
            sampler = animation['samplers'][channel['sampler']]
            channel_times = [row[0] for row in arrays[sampler['input']]]
            if channel_times != times:
                raise ValueError('native adapter expects common channel times')
            start, count = {'translation': (0, 3), 'rotation': (3, 4), 'scale': (7, 3)}[channel['target']['path']]
            for frame, value in enumerate(arrays[sampler['output']]):
                samples[frame][channel['target']['node']][start:start+count] = value
        clips.append((animation['name'], fps, samples))
    skin = g['skins'][0]
    payload = bytearray(HEADER.pack(b'AEACTR1\0', 1, len(rest), len(skin['joints']), len(vertices),
                                   len(batches), len(textures), len(clips), 0, *bounds[0], *bounds[1]))
    for parent, trs in zip(parents, rest):
        payload.extend(struct.pack('<i10f', parent, *trs))
    for node, inverse in zip(skin['joints'], arrays[skin['inverseBindMatrices']]):
        payload.extend(struct.pack('<I16f', node, *inverse))
    for batch in batches:
        payload.extend(struct.pack('<4I', *batch))
    for vertex in vertices:
        payload.extend(struct.pack('<8fI', *vertex))
    for name, fps, samples in clips:
        encoded = name.encode('ascii')
        if len(encoded) >= 64:
            raise ValueError('clip name too long')
        payload.extend(struct.pack('<64sIf', encoded, len(samples), fps))
        for sample in samples:
            for trs in sample:
                payload.extend(struct.pack('<10f', *trs))
    for pixels in textures:
        payload.extend(struct.pack('<3I', 256, 256, len(pixels)))
        payload.extend(pixels)
    OUT.mkdir(parents=True, exist_ok=True)
    output = OUT/f'{path.parent.name}.aea'
    output.write_bytes(payload)
    result = dict(path=str(output), gltf=str(path), nodes=len(rest), joints=len(skin['joints']),
                  vertices=len(vertices), triangles=len(vertices)//3, textures=len(textures),
                  clips=[dict(name=n, frames=len(s), fps=f) for n,f,s in clips], bytes=len(payload))
    output.with_suffix('.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('gltf', nargs='?', type=Path)
    args = parser.parse_args()
    result = pack_actor(args.gltf)
    print(f"Packed {result['joints']} joints, {len(result['clips'])} clips, {result['triangles']} triangles: {result['path']}")
