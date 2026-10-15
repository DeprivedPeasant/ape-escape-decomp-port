"""Compare exported skin transforms against the established PSX pose implementation.
Also exercise rejection of corrupt payloads and references. No external dependencies.
"""
import copy
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import export_gltf as gl
import export_model as em
from check_channels import reference as channel_reference, positions as channel_positions


def rotate(q,v):
    x,y,z,w = q
    # Quaternion vector rotation: v + 2w(q x v) + 2(q x (q x v)).
    cross = lambda a,b: [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
    c = cross([x,y,z],v)
    cc = cross([x,y,z],c)
    return [v[i]+2*w*c[i]+2*cc[i] for i in range(3)]


def main():
    metadata = gl.clip_metadata()
    checked, worst = 0,0
    spike_g = spike_b = None
    for mem,off,m in em.scan_members():
        b = (em.MEMBERS/f'{mem}.bin').read_bytes()
        clips = metadata.get((mem,off),[])
        if not clips:
            continue
        # Build one actual animation per model, then read its emitted accessors.
        clip = clips[0]
        g,binary = gl.build(m,b,[clip],30)
        arrays = gl.validate(g,binary)
        if (mem,off) == ('008',0xBC):
            spike_g,spike_b = g,binary
        anim = g['animations'][0]
        tracks = {(c['target']['node'],c['target']['path']): arrays[anim['samplers'][c['sampler']]['output']]
                  for c in anim['channels']}
        origins,owners = gl.hierarchy(m)
        _,poses = gl.sample_local_poses(b,m,clip)
        for frame in sorted({0,(len(poses)-1)//2,len(poses)-1}):
            translations,angles,scales = poses[frame]
            reference = channel_positions(m,channel_reference(b,m,clip,clip[2]+frame)) if len(clip)==6 and clip[5]=='channels' else em.posed_positions(m,translations[0],angles)
            for vi,local in enumerate(m['verts']):
                bone = owners[vi]
                point = gl.xyz(local)
                while True:
                    node = bone+2
                    point = [point[k]*tracks[(node,'scale')][frame][k] for k in range(3)] if (node,'scale') in tracks else point
                    point = rotate(tracks[(node,'rotation')][frame],point)
                    translation = tracks[(node,'translation')][frame] if (node,'translation') in tracks else g['nodes'][node]['translation']
                    point = [point[k]+translation[k] for k in range(3)]
                    parent = m['bones'][bone][3]
                    if parent == 0:
                        break
                    bone = parent-1
                error = max(abs(a-c) for a,c in zip(point,gl.xyz(reference[vi])))
                worst = max(worst,error)
                if error > 1e-5:
                    raise AssertionError((mem,off,frame,vi,error))
                checked += 1
    rejected = 0
    for kind in ('length','bounds','node','index','time','quat'):
        g,binary = copy.deepcopy(spike_g),bytearray(spike_b)
        if kind == 'length':
            binary.pop()
        elif kind == 'bounds':
            g['accessors'][1]['min'][0] -= 1
        elif kind == 'node':
            g['nodes'][0]['children'].append(len(g['nodes']))
        else:
            aidx = g['meshes'][0]['primitives'][0]['indices'] if kind == 'index' else g['animations'][0]['samplers'][0 if kind == 'time' else 1]['input' if kind == 'time' else 'output']
            a = g['accessors'][aidx]
            o = g['bufferViews'][a['bufferView']]['byteOffset']
            if kind == 'index':
                struct.pack_into('<I',binary,o,0xFFFFFFFF)
            elif kind == 'time':
                struct.pack_into('<f',binary,o+4,0)
            else:
                struct.pack_into('<4f',binary,o,0,0,0,0)
        try:
            gl.validate(g,bytes(binary))
        except (ValueError,IndexError):
            rejected += 1
        else:
            raise AssertionError(f'failed to reject {kind}')
    result = {'compared_vertices':checked,'max_export_unit_error':worst,'corruptions_rejected':rejected}
    (gl.OUT/'pose-validation.json').write_text(__import__('json').dumps(result,indent=2)+'\n')
    print(result)


if __name__ == '__main__':
    main()
