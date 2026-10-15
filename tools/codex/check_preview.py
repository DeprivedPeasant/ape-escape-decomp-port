"""Check actual glTF inverse-bind/joint/weight skinning using the preview consumer.

System Python (installed numpy). Tests every scanner model's bind pose, and first,
middle, last animation keys for every resolved clip. The reference is the existing
PSX matrix pose implementation; the consumer reads glTF accessors only.
"""
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import export_gltf as gl
import export_model as em
import preview_gltf as preview
from check_channels import reference as channel_reference, positions as channel_positions


def compare(m,root,angles,primitives,reference=None):
    expected = np.array([gl.xyz(v) for v in (reference if reference is not None else em.posed_positions(m,root,angles))])
    # The writer groups source polygons by material in insertion order.
    keys = {}
    for p in m['prims']:
        key = (p.get('tpage'),p.get('clut'),p['semi'])
        keys.setdefault(key,[]).extend(p['v'])
    worst,vertices = 0,0
    for indices,p in zip(keys.values(),primitives):
        error = float(np.abs(p['positions']-expected[indices]).max())
        worst = max(worst,error)
        vertices += len(indices)
    if worst > 2e-5:
        raise AssertionError(f'{m["off"]:X}: actual glTF skin error {worst}')
    return worst,vertices


def main():
    metadata = gl.clip_metadata()
    models,keys,vertices,worst = 0,0,0,0
    for member,offset,m in em.scan_members():
        b = (em.MEMBERS/f'{member}.bin').read_bytes()
        clips = metadata.get((member,offset),[])
        g,binary = gl.build(m,b,clips,30)
        arrays = [np.array(a) for a in gl.validate(g,binary)]
        error,n = compare(m,(0,0,0),[(0,0,0)]*len(m['bones']),preview.pose(g,arrays,None,0))
        worst,vertices = max(worst,error),vertices+n
        models += 1
        for source,clip in zip(clips,g['animations']):
            _,poses = gl.sample_local_poses(b,m,source)
            for frame in sorted({0,(len(poses)-1)//2,len(poses)-1}):
                translations,angles,scales = poses[frame]
                reference = channel_positions(m,channel_reference(b,m,source,source[2]+frame)) if len(source)==6 and source[5]=='channels' else None
                time = arrays[clip['samplers'][0]['input']][frame,0]
                error,n = compare(m,translations[0],angles,preview.pose(g,arrays,clip,float(time)),reference)
                worst,vertices = max(worst,error),vertices+n
                keys += 1
        if models % 100 == 0:
            print(f'checked models={models}, clip keys={keys}',flush=True)
    result = {'models':models,'clip_keys':keys,'corner_positions_compared':vertices,'max_error':worst}
    (gl.OUT/'skin-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(result)


if __name__ == '__main__':
    main()
