"""Offline glTF 2.0 actor exporter. Reuses export_model and vram unchanged.

Commands: spike, all, check PATH, audit. Outputs only under extract/gltf.
Explicit clips: --clip NAME:OFFSET:FIRST:LAST (inclusive), --fps (default 30).
No guessed animation pointers: known player state sets are resolved automatically.
"""
import argparse
import json
import math
import struct
from collections import defaultdict
from pathlib import Path

import export_model as em
import vram
from codex.area_tables import area_members
from codex.actor_sources import idle_clips,ui_clips,ui_vram
from codex.channel_animation import decode as decode_channels
from codex.scene_sources import scene_clips, scene_vram
from codex.feature_sources import feature_clips

ROOT = em.ROOT
OUT = ROOT / 'extract/gltf'
SIZE = {5121: 1, 5123: 2, 5125: 4, 5126: 4}
FMT = {5121: 'B', 5123: 'H', 5125: 'I', 5126: 'f'}
WIDTH = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def xyz(v):
    return [v[0] / 256, -v[1] / 256, v[2] / 256]


def quaternion(angles):
    # qz*qy*qx, then conjugate rotation by diag(1,-1,1).
    x, y, z = [a * math.pi / 4096 for a in angles]
    cx, cy, cz = math.cos(x), math.cos(y), math.cos(z)
    sx, sy, sz = math.sin(x), math.sin(y), math.sin(z)
    return [-(sx*cy*cz-cx*sy*sz), cx*sy*cz+sx*cy*sz,
            -(cx*cy*sz-sx*sy*cz), cx*cy*cz+sx*sy*sz]


def hierarchy(m):
    origins, owners = [], [None] * m['vc']
    for i, (count, first, offset, parent) in enumerate(m['bones']):
        if not 0 <= parent <= i:
            raise ValueError(f'bone {i}: forward/invalid matrix parent {parent}')
        base = origins[parent-1] if parent else (0, 0, 0)
        local = (0, 0, 0) if i == 0 else offset
        origins.append(tuple(base[k] + local[k] for k in range(3)))
        if first + count > m['vc']:
            raise ValueError(f'bone {i}: vertex range exceeds model')
        for vi in range(first, first + count):
            if owners[vi] is not None:
                raise ValueError(f'vertex {vi}: multiple bones')
            owners[vi] = i
    if None in owners:
        raise ValueError('unowned vertex')
    return origins, owners


def player_clips(b):
    clips = []
    labels = {0: 'stand', 1: 'stand_alt', 2: 'creep', 3: 'walk', 5: 'run'}
    for slot in range(10):
        anim, states, transitions = [struct.unpack_from('<H', b, 0x14 + slot*16+k)[0]*4 for k in (0, 4, 8)]
        if transitions < states or (transitions-states) % 8:
            raise ValueError(f'player set {slot}: malformed state table')
        for state in range((transitions-states)//8):
            first, last, flags = struct.unpack_from('<HHI', b, states+state*8)
            name = f'set{slot}_state{state:02X}'
            if slot == 0 and state in labels:
                name += '_' + labels[state]
            if first <= last:
                # State ranges end inclusively; clip data must precede state table.
                if anim + (last+1)*72 > states:
                    raise ValueError(f'{name}: frames overrun animation set')
                clips.append((name, anim, first, last, 1))
    return clips


def clip_metadata():
    """Resolve animation sets through actual loader tables, never header heuristics."""
    out = {('008', 0xBC): player_clips((em.MEMBERS/'008.bin').read_bytes())}
    for area, member in area_members().items():
        mem = f'{member:03X}'
        b = (em.MEMBERS/f'{mem}.bin').read_bytes()
        monkey = struct.unpack_from('<I', b, 0x4E4)[0]
        h = struct.unpack_from('<11I', b, monkey) if monkey+44 <= len(b) else (0,)
        if h[0] == 17:
            clips = []
            for slot, (table, end, frames) in enumerate([(h[6],h[7],h[8]),(h[7],h[8],h[9])]):
                if (end-table) % 8:
                    raise ValueError(f'area {area:02X}: monkey table alignment')
                for i in range((end-table)//8):
                    first,last,step,key = struct.unpack_from('<4H',b,monkey+table+i*8)
                    if last == 0xFFFF or last+1 == first:
                        continue  # unused alternative or zero-length entries
                    if not step or last < first:
                        raise ValueError(f'area {area:02X}: monkey clip {i} range')
                    final_key = key+(last-first)//step+1  # ActorAnimDecode always reads key+1
                    clips.append((f'monkey_set{slot}_clip{i:02X}',monkey+frames,key,final_key,step,last-first))
            for modeloff in h[2:5]:
                out.setdefault((mem,monkey+modeloff),clips)
        sec = struct.unpack_from('<I',b,0x4E0)[0]
        if sec+16 > len(b) or struct.unpack_from('<I',b,sec)[0] != 0x44455A2E:
            continue
        count = struct.unpack_from('<H',b,sec+4)[0]
        entries = sec+struct.unpack_from('<I',b,sec+8)[0]
        for i in range(count):
            entry = entries+i*0x20
            modeloff,animoff = struct.unpack_from('<II',b,entry+0x18)
            key = (mem,sec+modeloff)
            model = em.parse_actor(b,key[1])
            if not model or not model['ok'] or not animoff:
                continue
            anim = sec+animoff
            n = struct.unpack_from('<H',b,anim)[0]
            if n > 1024 or anim+4+n*4 > len(b):
                raise ValueError(f'{key}: ZED animation count {n}')
            clips = []
            for slot in range(n):
                first,last = struct.unpack_from('<HH',b,anim+4+slot*4)
                if first > last:
                    raise ValueError(f'{key}: enemy frame range {first}..{last}')
                clips.append((f'enemy_clip{slot:02X}',anim+4+n*4,first,last,1,'channels'))
            if key in out and out[key] != clips:
                raise ValueError(f'{key}: conflicting animation association')
            out[key] = clips
    out.update(idle_clips())
    for key,clips in ui_clips().items():
        if key in out:
            raise ValueError(f'{key}: conflicting UI association')
        out[key] = clips
    for key,clips in scene_clips().items():
        if key in out:
            if [c[1:] for c in out[key]] != [c[1:] for c in clips]:
                raise ValueError(f'{key}: conflicting scene association')
        else:
            out[key] = clips
    for key,clips in feature_clips().items():
        if key in out:
            raise ValueError(f'{key}: conflicting feature association')
        out[key] = clips
    return out


def clip_frame_count(clip):
    return clip[5]+1 if len(clip) == 6 and isinstance(clip[5],int) else clip[3]-clip[2]+1


def truncate_div(value, divisor):
    return value//divisor if value >= 0 else -((-value)//divisor)


def interpolate_pose(a, b, fraction):
    """AnimDecodeLerp: 12-bit weight, wrapped Euler, division toward zero."""
    root = tuple(truncate_div(x*(4096-fraction)+y*fraction,4096) for x,y in zip(a[0],b[0]))
    angles = []
    for aa,bb in zip(a[1],b[1]):
        result = []
        for x,y in zip(aa,bb):
            if y-x > 2048:
                x += 4096
            elif y-x < -2048:
                y += 4096
            result.append(truncate_div(x*(4096-fraction)+y*fraction,4096))
        angles.append(tuple(result))
    return root,angles


def sample_clip(b,nb,clip):
    if len(clip) == 6 and clip[5] == 'channels':
        raise ValueError('channel clips require sample_local_poses with a model')
    name,offset,first,last,step = clip[:5]
    if not 0 <= first <= last or offset < 0 or offset+(last+1)*(nb+2)*4 > len(b):
        raise ValueError(f'{name}: frame range outside member')
    keys = [em.decode_frame(b,offset+f*(nb+2)*4,nb) for f in range(first,last+1)]
    if len(clip) == 5:
        return [i*step for i in range(len(keys))],keys
    duration = clip[5]
    if step <= 0 or duration < 0 or duration//step+1 >= len(keys):
        raise ValueError(f'{name}: interpolation neighbor outside key range')
    frames = []
    for frame in range(duration+1):
        key = frame//step
        fraction = ((frame*4096)//step) % 4096
        frames.append(interpolate_pose(keys[key],keys[key+1],fraction))
    return list(range(duration+1)),frames


def sample_local_poses(b,m,clip):
    if len(clip) == 6 and clip[5] == 'channels':
        frames = decode_channels(b,m,clip[1],clip[2],clip[3])
        return [i*clip[4] for i in range(len(frames))],frames
    times,raw = sample_clip(b,len(m['bones']),clip)
    frames = []
    for root,angles in raw:
        translations = [root]+[tuple(bone[2]) for bone in m['bones'][1:]]
        frames.append((translations,angles,[(1,1,1)]*len(m['bones'])))
    return times,frames


class Document:
    def __init__(self):
        self.bin = bytearray()
        self.doc = {'asset': {'version': '2.0', 'generator': 'Ape Escape offline actor exporter'},
                    'buffers': [], 'bufferViews': [], 'accessors': [], 'nodes': [],
                    'meshes': [], 'materials': [], 'skins': [], 'animations': [],
                    'scene': 0, 'scenes': [{'nodes': [0, 1]}]}

    def accessor(self, rows, kind, component=5126, target=None, bounds=False):
        n = WIDTH[kind]
        assert rows and all(len(row) == n for row in rows)
        self.bin.extend(b'\0' * (-len(self.bin) % 4))
        start = len(self.bin)
        for row in rows:
            self.bin.extend(struct.pack('<' + FMT[component]*n, *row))
        view = {'buffer': 0, 'byteOffset': start, 'byteLength': len(self.bin)-start}
        if target:
            view['target'] = target
        self.doc['bufferViews'].append(view)
        a = {'bufferView': len(self.doc['bufferViews'])-1, 'componentType': component,
             'count': len(rows), 'type': kind}
        if bounds:
            a['min'] = [min(r[i] for r in rows) for i in range(n)]
            a['max'] = [max(r[i] for r in rows) for i in range(n)]
        self.doc['accessors'].append(a)
        return len(self.doc['accessors'])-1


def build(m, b, clips, fps, texture_factory=None):
    origins, owners = hierarchy(m)
    d = Document()
    g = d.doc
    g['nodes'] = [{'name': 'skeleton', 'children': []}, {'name': 'actor_mesh', 'mesh': 0, 'skin': 0}]
    for i, (_, _, offset, parent) in enumerate(m['bones']):
        g['nodes'].append({'name': f'bone_{i:02d}', 'translation': xyz((0,0,0) if i == 0 else offset)})
        g['nodes'][parent+1 if parent else 0].setdefault('children', []).append(i+2)
    ibms = []
    for origin in origins:
        x, y, z = xyz(origin)
        ibms.append([1,0,0,0, 0,1,0,0, 0,0,1,0, -x,-y,-z,1])
    g['skins'] = [{'joints': list(range(2, len(m['bones'])+2)), 'skeleton': 0,
                   'inverseBindMatrices': d.accessor(ibms, 'MAT4')}]
    grouped = defaultdict(list)
    for p in m['prims']:
        grouped[(p.get('tpage'), p.get('clut'), p['semi'])].append(p)
    primitives = []
    for (tp, clut, semi), polys in grouped.items():
        mat = {'name': f'page_{tp}_{clut}_{semi}', 'doubleSided': True,
               'pbrMetallicRoughness': {'metallicFactor': 0, 'roughnessFactor': 1},
               'extras': {'psxSemiTransparent': semi}}
        if tp is not None and texture_factory:
            uri = texture_factory(tp, clut)
            images = g.setdefault('images', [])
            textures = g.setdefault('textures', [])
            images.append({'uri': uri})
            textures.append({'source': len(images)-1, 'sampler': 0})
            g['samplers'] = [{'magFilter': 9728, 'minFilter': 9728, 'wrapS': 10497, 'wrapT': 10497}]
            mat['pbrMetallicRoughness']['baseColorTexture'] = {'index': len(textures)-1}
            mat['alphaMode'] = 'BLEND' if semi else 'MASK'
            mat['alphaCutoff'] = 0.5
        g['materials'].append(mat)
        pos, joints, weights, colors, uv, normals, indices = [], [], [], [], [], [], []
        for p in polys:
            start = len(pos)
            for k, vi in enumerate(p['v']):
                joint = owners[vi]
                pos.append(xyz(tuple(m['verts'][vi][axis] + origins[joint][axis] for axis in range(3))))
                joints.append([joint, 0, 0, 0])
                weights.append([1,0,0,0])
                color = m['colors'][p['col'][k if len(p['col']) > 1 else 0]]
                colors.append([c/255 for c in color])
                if tp is not None:
                    uv.append([u/256 for u in p['uv'][k]])
                if m['normals']:
                    normal = xyz(m['normals'][vi])
                    length = math.sqrt(sum(x*x for x in normal))
                    normals.append([x/length for x in normal] if length else [0,1,0])
            # Reflecting y reverses winding. PSX quad corner order = 0,1,2 / 2,1,3.
            indices.extend([[start], [start+2], [start+1]])
            if len(p['v']) == 4:
                indices.extend([[start+2], [start+3], [start+1]])
        attrs = {'POSITION': d.accessor(pos, 'VEC3', target=34962, bounds=True),
                 'JOINTS_0': d.accessor(joints, 'VEC4', 5123, 34962),
                 'WEIGHTS_0': d.accessor(weights, 'VEC4', target=34962),
                 'COLOR_0': d.accessor(colors, 'VEC3', target=34962)}
        if uv:
            attrs['TEXCOORD_0'] = d.accessor(uv, 'VEC2', target=34962)
        if normals:
            attrs['NORMAL'] = d.accessor(normals, 'VEC3', target=34962)
        primitives.append({'attributes': attrs, 'indices': d.accessor(indices, 'SCALAR', 5125, 34963),
                           'material': len(g['materials'])-1, 'mode': 4})
    g['meshes'] = [{'primitives': primitives}]
    for clip in clips:
        name, offset, first, last, step = clip[:5]
        nb = len(m['bones'])
        frame_times,frames = sample_local_poses(b,m,clip)
        time = d.accessor([[f/fps] for f in frame_times], 'SCALAR', bounds=True)
        anim = {'name': name, 'samplers': [], 'channels': [],
                'extras': {'memberOffset': offset, 'firstFrame': first, 'lastFrame': last, 'frameStep': step}}
        channel_format = len(clip) == 6 and clip[5] == 'channels'
        anim['extras']['sourceFormat'] = 'ModelPoseBones channels' if channel_format else 'AnimDecodeFrame fixed'
        if len(clip) == 6 and not channel_format:
            anim['extras'].update({'gameFrameDuration':clip[5], 'interpolationBaked':True})
        tracks = []
        for bone in range(nb):
            if channel_format or bone == 0:
                tracks.append((bone+2,'translation',[xyz(t[bone]) for t,_,_ in frames],'VEC3'))
            if channel_format:
                tracks.append((bone+2,'scale',[list(s[bone]) for _,_,s in frames],'VEC3'))
            qs = [quaternion(angles[bone]) for _, angles,_ in frames]
            for i in range(1,len(qs)):
                if sum(a*c for a,c in zip(qs[i-1], qs[i])) < 0:
                    qs[i] = [-x for x in qs[i]]
            tracks.append((bone+2, 'rotation', qs, 'VEC4'))
        for node, path, rows, kind in tracks:
            output = d.accessor(rows, kind)
            anim['samplers'].append({'input': time, 'output': output, 'interpolation': 'LINEAR'})
            anim['channels'].append({'sampler': len(anim['samplers'])-1, 'target': {'node': node, 'path': path}})
        g['animations'].append(anim)
    g['buffers'] = [{'byteLength': len(d.bin), 'uri': 'actor.bin'}]
    g['extras'] = {'sourceOffset': m['off'], 'psxUnitsPerExportUnit': 256, 'fps': fps}
    return g, bytes(d.bin)


def validate(g, binary):
    """Check actual payloads, not only JSON declarations; raises ValueError on corruption."""
    def require(ok, message):
        if not ok:
            raise ValueError(message)
    require(g['asset']['version'] == '2.0', 'asset version')
    require(len(g['buffers']) == 1 and g['buffers'][0]['byteLength'] == len(binary), 'buffer length')
    for view in g['bufferViews']:
        require(view['buffer'] == 0 and view.get('byteOffset',0) >= 0 and view['byteLength'] > 0,
                'buffer view invalid')
        require(view.get('byteOffset',0)+view['byteLength'] <= len(binary), 'buffer view exceeds binary')
    arrays = []
    for a in g['accessors']:
        require(0 <= a['bufferView'] < len(g['bufferViews']), 'accessor view index')
        view = g['bufferViews'][a['bufferView']]
        n, size = WIDTH[a['type']], SIZE[a['componentType']]
        offset = a.get('byteOffset',0)
        stride = view.get('byteStride',n*size)
        require(a['count'] > 0 and stride >= n*size and offset % size == 0, 'accessor count/alignment')
        require(offset+(a['count']-1)*stride+n*size <= view['byteLength'], 'accessor exceeds view')
        start = view.get('byteOffset',0)+offset
        require(start % size == 0, 'absolute accessor alignment')
        rows = [struct.unpack_from('<'+FMT[a['componentType']]*n, binary, start+i*stride) for i in range(a['count'])]
        require(all(math.isfinite(x) for row in rows for x in row), 'nonfinite accessor')
        for key, fun in [('min',min),('max',max)]:
            if key in a:
                require(len(a[key]) == n and all(math.isclose(a[key][j],fun(r[j] for r in rows),abs_tol=1e-5,rel_tol=1e-5) for j in range(n)), 'accessor bounds')
        arrays.append(rows)
    nn = len(g['nodes'])
    parents = {}
    for i,node in enumerate(g['nodes']):
        for child in node.get('children',[]):
            require(0 <= child < nn and child not in parents, 'node child/multiple parents')
            parents[child] = i
        for key in ('mesh','skin'):
            if key in node:
                require(0 <= node[key] < len(g[key+'es' if key == 'mesh' else 'skins']), 'node resource index')
    for i in range(nn):
        seen = set()
        while i in parents:
            require(i not in seen, 'node cycle')
            seen.add(i)
            i = parents[i]
    for scene in g['scenes']:
        require(all(0 <= n < nn and n not in parents for n in scene['nodes']), 'scene root indices')
    for skin in g['skins']:
        require(all(0 <= j < nn for j in skin['joints']) and len(set(skin['joints'])) == len(skin['joints']), 'skin joint indices')
        require(0 <= skin['skeleton'] < nn, 'skin skeleton')
        require(len(arrays[skin['inverseBindMatrices']]) == len(skin['joints']), 'inverse bind count')
    for mesh in g['meshes']:
        for p in mesh['primitives']:
            attrs = p['attributes']
            require(all(0 <= a < len(arrays) for a in attrs.values()), 'attribute accessor index')
            count = len(arrays[attrs['POSITION']])
            require(all(len(arrays[a]) == count for a in attrs.values()), 'attribute count mismatch')
            require(all(0 <= row[0] < count for row in arrays[p['indices']]), 'mesh index bounds')
            require(len(arrays[p['indices']]) % 3 == 0, 'triangle count')
            require(0 <= p['material'] < len(g['materials']), 'material index')
            require(all(0 <= j < len(g['skins'][0]['joints']) for row in arrays[attrs['JOINTS_0']] for j in row), 'joint accessor bounds')
            require(all(math.isclose(sum(row),1,abs_tol=1e-6) for row in arrays[attrs['WEIGHTS_0']]), 'weights not normalized')
    for anim in g['animations']:
        for channel in anim['channels']:
            require(0 <= channel['target']['node'] < nn, 'animation node')
            require(0 <= channel['sampler'] < len(anim['samplers']), 'animation sampler')
            s = anim['samplers'][channel['sampler']]
            path = channel['target']['path']
            require(path in ('translation','rotation','scale'), 'animation target path')
            require(all(0 <= s[k] < len(arrays) for k in ('input','output')), 'animation accessor index')
            require(g['accessors'][s['input']]['type'] == 'SCALAR' and g['accessors'][s['input']]['componentType'] == 5126, 'animation time accessor type')
            require(g['accessors'][s['output']]['type'] == ('VEC4' if path == 'rotation' else 'VEC3') and g['accessors'][s['output']]['componentType'] == 5126, 'animation output accessor type')
            times, values = arrays[s['input']], arrays[s['output']]
            require(len(times) == len(values) and all(times[i][0] < times[i+1][0] for i in range(len(times)-1)), 'animation times/count')
            if channel['target']['path'] == 'rotation':
                require(all(math.isclose(sum(x*x for x in row),1,abs_tol=1e-5) for row in values), 'quaternion norm')
    for texture in g.get('textures',[]):
        require(0 <= texture['source'] < len(g['images']) and 0 <= texture['sampler'] < len(g['samplers']), 'texture indices')
    return arrays


def export(member, offset, m, clips, fps):
    folder = OUT / f'{member}_{offset:06X}'
    folder.mkdir(parents=True, exist_ok=True)
    area = next((a for a,mem in area_members().items() if f'{mem:03X}' == member), None)
    ui = ui_vram(member) or scene_vram(member)
    vr = ui[0] if ui else vram.area_vram(area) if area else vram.global_vram()
    texture_dir = f'tex_{ui[1]:03X}' if ui else 'tex'
    (folder/texture_dir).mkdir(exist_ok=True)
    mats = vram.Materials(folder/texture_dir,vr)
    def texture(tp,clut):
        return texture_dir+'/tex_' + mats.get(tp,clut) + '.png'
    b = (em.MEMBERS/f'{member}.bin').read_bytes()
    g, binary = build(m,b,clips,fps,texture)
    validate(g,binary)
    (folder/'actor.bin').write_bytes(binary)
    (folder/'actor.gltf').write_text(json.dumps(g,indent=2)+'\n',encoding='utf-8')
    return folder/'actor.gltf'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['spike','all','audit','check','actor'])
    parser.add_argument('args',nargs='*')
    parser.add_argument('--fps', type=float, default=30)
    parser.add_argument('--clip', action='append', default=[],help='raw player/monkey frames NAME:OFFSET:FIRST:LAST')
    parser.add_argument('--channel-clip',action='append',default=[],help='ModelPoseBones channel stream NAME:OFFSET:FIRST:LAST')
    options = parser.parse_args()
    if not math.isfinite(options.fps) or options.fps <= 0:
        parser.error('fps must be finite and positive')
    if options.mode == 'check':
        path = Path(options.args[0])
        g = json.loads(path.read_text())
        validate(g,(path.parent/g['buffers'][0]['uri']).read_bytes())
        for image in g.get('images',[]):
            assert (path.parent/image['uri']).read_bytes()[:8] == b'\x89PNG\r\n\x1a\n'
        print('PASS',path)
        return
    if options.mode in ('spike','actor'):
        member, offset = ('008',0xBC) if options.mode == 'spike' else (f'{int(options.args[0],16):03X}',int(options.args[1],0))
        b = (em.MEMBERS/f'{member}.bin').read_bytes()
        m = em.parse_actor(b,offset)
        if not m or not m['ok']:
            raise ValueError('not a valid actor model')
        clips = clip_metadata().get((member,offset),[])
        for source,channel_format in ((options.clip,False),(options.channel_clip,True)):
            for clip in source:
                name, off, first, last = clip.split(':')
                spec = (name,int(off,0),int(first,0),int(last,0),1)
                clips.append(spec+('channels',) if channel_format else spec)
        print(export(member,offset,m,clips,options.fps),f'{len(clips)} clips')
        return
    found = em.scan_members()
    metadata = clip_metadata()
    rows, failures, exported = [], [], 0
    for member,offset,m in found:
        try:
            if not m['ok']:
                raise ValueError('primitive parsing failed')
            b = (em.MEMBERS/f'{member}.bin').read_bytes()
            clips = metadata.get((member,offset),[])
            g,binary = build(m,b,clips,options.fps)
            validate(g,binary)
            if options.mode == 'all':
                export(member,offset,m,clips,options.fps)
                exported += 1
            rows.append(f'| {member} | 0x{offset:X} | {len(m["bones"])} | {m["vc"]} | {len(clips)} | {sum(clip_frame_count(c) for c in clips)} | pass |')
        except (ValueError,IndexError,struct.error,AssertionError) as error:
            failures.append(f'{member}+0x{offset:X}: {error}')
    note = ['# glTF actor export (generated)','',
            'Generated by `uv run --offline python tools/export_gltf.py audit`.', '',
            'Commands: `spike`, `all`, `actor MEMBER OFFSET`, `check PATH`. '
            'Explicit source clips use `--clip NAME:OFFSET:FIRST:LAST` for raw player/monkey frames or '
            '`--channel-clip NAME:OFFSET:FIRST:LAST` for ModelPoseBones channel streams; '
            'OFFSET is the stream header address within the member, not the preceding clip table. '
            'Both options append to any automatically resolved clips; unassociated actors start with an empty clip list.',
            'Outputs: `extract/gltf/MEMBER_OFFSET/actor.gltf`, `actor.bin`, and PNG pages under `tex/`.', '',
            'The exporter imports export_model/vram without modifying them. Each source vertex belongs to one joint with weight 1. '
            'Corners are duplicated for UV/colour seams; quads split 0,1,2 / 2,1,3. Bone parent p refers to bone p-1; '
            'p=0 attaches to the synthetic skeleton root. For fixed player/monkey poses, bone 0 translation replaces its stored offset; channel poses retain stored offsets unless overridden. '
            'Bind rotations are identity and inverse binds undo accumulated offsets.', '',
            'Angles use the existing packed decoder (10-bit x/z, 12-bit y field, all multiplied by 4). '
            'Local rotations are Rz*Ry*Rx. Coordinates reflect y and divide by 256 (chosen export scale, not proven metres). '
            'Quaternion vectors reflect as (-x,y,-z); winding is reversed. PNG UVs use (u/256,v/256). '
            'Nearest texture sampling, vertex colours and PBR roughness=1/metallic=0 approximate PSX materials; blend modes and environment mapping are not reproduced.', '',
            'PlayerInit resolves ten state sets from member 008 header +0x14+i*0x10. '
            'States are 8 bytes (inclusive first/last frames and flags); animation stride is (bones+2)*4. '
            'Spike set 0 starts at 0x1035C; state 0 stand spans 0..28. Empty gadget sets are skipped. '
            'Frame times default to 30 fps (configurable); clip naming is numeric except established player states. '
            'LINEAR interpolation uses quaternion shortest paths between emitted samples; subframe interpolation approximates the game\'s Euler interpolation. '
            'Monkey integer-frame interpolation is baked as documented below and in notes/55-animation-timing.md.', '',
            f'Actors scanned: {len(found)}; structurally valid: {len(rows)}; failures: {len(failures)}.',
            f'Animation associations: {len(metadata)} actor instances; {sum(map(len,metadata.values()))} clips; '
            f'{sum(clip_frame_count(c) for clips in metadata.values() for c in clips)} exported pose samples (repeated area/LOD instances counted separately).',
            'All actors have their joint and vertex counts checked and an in-memory glTF built/validated. '
            'MonkeySpawnAll resolves model offsets (header words 2..4), 8-byte clip tables (words 6/7), and frame sets (words 8/9) relative to the monkey section. '
            'Monkey clips carry first/last game frames, keyframe spacing, and starting key index. The export reads the final interpolation neighbour but ends at the inclusive last game frame. '
            'AnimDecodeLerp wrapping, 12-bit blend weight and division toward zero are baked at every integer game frame before quaternion conversion. '
            'ZED +0x18 is the MODEL offset (ObjInitModel); +0x1C is the animation table offset. '
            'ZakoGetAnimTable reads a 4-byte header with u16 clip count, then count*4-byte first/last pairs followed by a ModelPoseBones channel stream. See notes/61-channel-animation.md for its descriptor-dependent stride and TRS channels. '
            'These corrected the earlier ambiguous +0x18 animation-data description in notes/22 and symbols.csv through the inbox. '
            'Zero clips means unresolved association, not zero source frames; unrelated actors in auxiliary data blobs require explicit --clip.', '',
            'Player idle member 013 contains eight independently streamed 0xA000-byte blocks. '
            'Six scanner-supported secondary actors now use the block-relative model/channel pointers and shared player state ranges. '
            'All eight blocks validate; two one-joint models are outside the existing scanner limits. See notes/56-actor-sources.md.', '',
            'Compact stage UI bundles now use loader-proven model/animation descriptor pairing and their actual TIM list. '
            'Only the slots initialized by DrawWipeA/B are associated; unused duplicate descriptors are not guessed. '
            'See notes/60-ui-actor-bundles.md. UI images are placed in tex_<TIM-member> subdirectories.', '',
            'Additional compact mode scenes use loader-backed associations in tools/codex/scene_sources.py: '
            'menu 029, stage-clear 02D/02E, special-entry 02F..035 and scene 045..049. '
            'Their actual TIM lists are used and channel streams checked against the next declared section. '
            'See notes/62-scene-actor-bundles.md.', '',
            'Area features 10/11 now associate direct channel streams through their initializer pointers '
            'and the hardcoded idle/activation ranges in IndicatorsUpdateDraw/Indicator_Activate. '
            'One-joint variants remain outside the shared scanner. See notes/63-feature-actor-bundles.md.', '',
            'Uses the complete area reader tools/codex/area_tables.py, which skips NULL load-chain holes and terminates on id=0xFFFFFFFF. '
            'This recovers areas after 0x34 that the original shared reader omitted (Claude subsequently corrected it); texture reconstruction still uses vram.area_vram directly for the selected area id.', '',
            'Checks: buffer/view lengths, alignment, accessor payload min/max and finite values, node hierarchy/cycles, skin joint bounds, '
            'index bounds and attribute counts, normalized weights/quaternions, animation frame/time counts, image/texture indices. '
            'This is a project structural check, not the external Khronos validator (offline/no new dependencies).', '',
            '| Member | Offset | Joints | Vertices | Clips | Exported pose samples | Structure |',
            '|---|---|---|---|---|---|---|'] + rows
    note += ['', '## Failures',''] + (failures or ['None.'])
    result_path = OUT/'pose-validation.json'
    if result_path.exists():
        result = json.loads(result_path.read_text())
        note += ['', '## Independent pose comparison', '',
                 'Run `uv run --offline python tools/codex/check_gltf.py` before regenerating this note.',
                 f'Compared {result["compared_vertices"]} emitted quaternion/hierarchy vertex transforms with the independent raw-pose/channel TRS references '
                 f'(first/middle/last keys of the first resolved clip per model). Maximum error: {result["max_export_unit_error"]:.9g} export units. '
                 f'Rejected {result["corruptions_rejected"]} deliberately corrupted buffers, bounds, node/index references, times and quaternions.']
    (ROOT/'notes/50-gltf-export.md').write_text('\n'.join(note)+'\n',encoding='utf-8')
    print(f'actors={len(found)} pass={len(rows)} failures={len(failures)} exported={exported}')
    if failures:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
