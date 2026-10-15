"""Offline CPU preview of an exported glTF skin/animation and its actual PNG textures.

Uses installed numpy with system Python; no browser, server or downloaded libraries.
python tools/codex/preview_gltf.py extract/gltf/008_0000BC/actor.gltf --clip set0_state00_stand
Outputs a PNG contact sheet and JSON image statistics next to the glTF.
Preview covers project-generated glTF, not every possible glTF extension.
"""
import argparse
import json
import math
import re
import struct
import sys
import zlib
from pathlib import Path

import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import export_gltf as gl


def png_read(path):
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('not PNG')
    cursor,idat = 8,bytearray()
    while cursor < len(data):
        size,kind = struct.unpack_from('>I4s',data,cursor)
        payload = data[cursor+8:cursor+8+size]
        if kind == b'IHDR':
            width,height,depth,color,compression,filtering,interlace = struct.unpack('>IIBBBBB',payload)
            if (depth,color,compression,filtering,interlace) != (8,6,0,0,0):
                raise ValueError('preview expects exported 8-bit RGBA noninterlaced PNG')
        if kind == b'IDAT':
            idat.extend(payload)
        cursor += size+12
    rows = np.frombuffer(zlib.decompress(idat),np.uint8).reshape(height,width*4+1)
    if rows[:,0].any():
        raise ValueError('preview expects exported PNG filter 0')
    return rows[:,1:].reshape(height,width,4).copy()


def png_write(path,pixels):
    height,width,_ = pixels.shape
    raw = b''.join(b'\0'+row.tobytes() for row in pixels)
    def chunk(kind,payload):
        return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload)&0xFFFFFFFF)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+
                     chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))


def rotation(q):
    x,y,z,w = q
    return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                     [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                     [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])


def interpolate(times,values,time,is_rotation):
    if len(times) == 1 or time <= times[0]:
        return values[0]
    if time >= times[-1]:
        return values[-1]
    index = int(np.searchsorted(times,time,side='right'))-1
    factor = (time-times[index])/(times[index+1]-times[index])
    a,b = values[index],values[index+1]
    if is_rotation:
        dot = float(a@b)
        if dot < 0:
            b,dot = -b,-dot
        dot = min(1,max(-1,dot))
        if dot < .9995:
            angle = math.acos(dot)
            q = (math.sin((1-factor)*angle)*a+math.sin(factor*angle)*b)/math.sin(angle)
        else:
            q = a*(1-factor)+b*factor
        return q/np.linalg.norm(q)
    return a*(1-factor)+b*factor


def pose(g,arrays,clip,time):
    overrides = {}
    if clip is not None:
        for channel in clip['channels']:
            sampler = clip['samplers'][channel['sampler']]
            target = channel['target']
            values = interpolate(arrays[sampler['input']][:,0],arrays[sampler['output']],time,target['path']=='rotation')
            overrides[(target['node'],target['path'])] = values
    local,world = [],[None]*len(g['nodes'])
    parents = {}
    for index,node in enumerate(g['nodes']):
        matrix = np.eye(4)
        q = overrides.get((index,'rotation'),node.get('rotation',[0,0,0,1]))
        scale = overrides.get((index,'scale'),node.get('scale',[1,1,1]))
        matrix[:3,:3] = rotation(q)@np.diag(scale)
        matrix[:3,3] = overrides.get((index,'translation'),node.get('translation',[0,0,0]))
        if 'matrix' in node:
            matrix = np.array(node['matrix']).reshape(4,4,order='F')
        local.append(matrix)
        for child in node.get('children',[]):
            parents[child] = index
    def transform(index):
        if world[index] is None:
            world[index] = transform(parents[index])@local[index] if index in parents else local[index]
        return world[index]
    for i in range(len(world)):
        transform(i)
    primitives = []
    for ni,node in enumerate(g['nodes']):
        if 'mesh' not in node:
            continue
        matrices = None
        if 'skin' in node:
            skin = g['skins'][node['skin']]
            ibms = arrays[skin['inverseBindMatrices']]
            matrices = np.array([world[j]@ibms[i].reshape(4,4,order='F') for i,j in enumerate(skin['joints'])])
        for p in g['meshes'][node['mesh']]['primitives']:
            attrs = p['attributes']
            positions = arrays[attrs['POSITION']]
            hp = np.column_stack((positions,np.ones(len(positions))))
            if matrices is not None:
                joints = arrays[attrs['JOINTS_0']].astype(int)
                weights = arrays[attrs['WEIGHTS_0']]
                transformed = sum(np.einsum('nij,nj->ni',matrices[joints[:,k]],hp)*weights[:,k,None] for k in range(4))
            else:
                transformed = hp@world[ni].T
            primitives.append({'positions':transformed[:,:3],
                               'indices':arrays[p['indices']].astype(int).reshape(-1,3),
                               'colors':arrays[attrs['COLOR_0']][:,:3] if 'COLOR_0' in attrs else np.ones((len(hp),3)),
                               'uv':arrays[attrs['TEXCOORD_0']] if 'TEXCOORD_0' in attrs else None,
                               'material':g['materials'][p['material']]})
    return primitives


def srgb_to_linear(values):
    return np.where(values <= .04045,values/12.92,((values+.055)/1.055)**2.4)


def linear_to_srgb(values):
    return np.where(values <= .0031308,12.92*values,1.055*np.maximum(values,0)**(1/2.4)-.055)


def render(g,primitives,textures,bounds,view,width=420,height=520):
    image = np.empty((height,width,4),np.uint8)
    image[:] = [28,32,44,255]
    depth = np.full((height,width),-np.inf)
    yaw = {'front':math.pi,'side':math.pi/2,'rear':0}[view]
    camera = rotation([0,math.sin(yaw/2),0,math.cos(yaw/2)])
    center,extent = bounds
    scale = min((width-40)/max(extent[0],.01),(height-40)/max(extent[1],.01))
    covered = 0
    for primitive in primitives:
        vertices = (primitive['positions']-center)@camera.T
        screen = np.column_stack((vertices[:,0]*scale+width/2,-vertices[:,1]*scale+height/2))
        mat = primitive['material']
        pbr = mat['pbrMetallicRoughness']
        base_factor = np.array(pbr.get('baseColorFactor',[1,1,1,1]))
        tex = None
        if 'baseColorTexture' in pbr:
            texture = g['textures'][pbr['baseColorTexture']['index']]
            tex = textures[texture['source']]
        for ids in primitive['indices']:
            points = screen[ids]
            minimum,maximum = np.floor(points.min(0)).astype(int),np.ceil(points.max(0)).astype(int)
            x0,y0 = max(0,minimum[0]),max(0,minimum[1])
            x1,y1 = min(width,maximum[0]),min(height,maximum[1])
            if x0 >= x1 or y0 >= y1:
                continue
            X,Y = np.meshgrid(np.arange(x0,x1)+.5,np.arange(y0,y1)+.5)
            a,b,c = points
            denominator = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(denominator) < 1e-8:
                continue
            l1 = ((b[1]-c[1])*(X-c[0])+(c[0]-b[0])*(Y-c[1]))/denominator
            l2 = ((c[1]-a[1])*(X-c[0])+(a[0]-c[0])*(Y-c[1]))/denominator
            l3 = 1-l1-l2
            inside = (l1>=0)&(l2>=0)&(l3>=0)
            z = l1*vertices[ids[0],2]+l2*vertices[ids[1],2]+l3*vertices[ids[2],2]
            vertex_color = sum(l[...,None]*primitive['colors'][i] for l,i in zip((l1,l2,l3),ids))
            rgb = vertex_color*base_factor[:3]
            alpha = np.ones(X.shape)*base_factor[3]
            if tex is not None:
                uv = sum(l[...,None]*primitive['uv'][i] for l,i in zip((l1,l2,l3),ids))
                tx = (np.floor(uv[...,0]*tex.shape[1]).astype(int) % tex.shape[1])
                ty = (np.floor(uv[...,1]*tex.shape[0]).astype(int) % tex.shape[0])
                pixel = tex[ty,tx]/255
                rgb *= srgb_to_linear(pixel[...,:3])
                alpha *= pixel[...,3]
            # Unlit material base-color preview; avoids inventing a scene light setup.
            color = np.clip(linear_to_srgb(rgb)*255,0,255).astype(np.uint8)
            visible = inside & (z>depth[y0:y1,x0:x1]) & (alpha >= mat.get('alphaCutoff',.01))
            depth[y0:y1,x0:x1][visible] = z[visible]
            image[y0:y1,x0:x1,:3][visible] = color[visible]
            covered += int(visible.sum())
    return image,{'drawn_fragments':covered,'visible_pixels':int(np.isfinite(depth).sum())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('gltf',type=Path)
    parser.add_argument('--clip',default=None)
    parser.add_argument('--samples',type=int,default=4)
    options = parser.parse_args()
    if options.samples < 1 or options.samples > 20:
        parser.error('samples must be 1..20')
    path = options.gltf.resolve()
    if not path.is_relative_to(gl.OUT.resolve()):
        parser.error('preview input/output must stay under extract/gltf')
    g = json.loads(path.read_text())
    binary = (path.parent/g['buffers'][0]['uri']).read_bytes()
    arrays = [np.array(a) for a in gl.validate(g,binary)]
    clips = g.get('animations',[])
    clip = next((c for c in clips if c['name'] == options.clip),None) if options.clip else (clips[0] if clips else None)
    if options.clip and clip is None:
        parser.error('clip name not found')
    textures = [png_read(path.parent/i['uri']) for i in g.get('images',[])]
    duration = arrays[clip['samplers'][0]['input']][-1,0] if clip else 0
    times = np.linspace(0,duration,options.samples)
    frames = [pose(g,arrays,clip,float(t)) for t in times]
    vertices = np.concatenate([p['positions'] for primitives in frames for p in primitives])
    minimum,maximum = vertices.min(0),vertices.max(0)
    center = (minimum+maximum)/2
    # Share bounds for all views and all times so root/body motion stays visible.
    extent = np.array([max(maximum[0]-minimum[0],maximum[2]-minimum[2]),maximum[1]-minimum[1]])*1.05
    views = ['front','side','rear']
    image = np.empty((520*3,420*options.samples,4),np.uint8)
    stats = []
    for row,view in enumerate(views):
        for col,(time,primitives) in enumerate(zip(times,frames)):
            pixels,result = render(g,primitives,textures,(center,extent),view)
            image[row*520:(row+1)*520,col*420:(col+1)*420] = pixels
            posed_vertices = np.concatenate([p['positions'] for p in primitives])
            collapsed = float(np.ptp(posed_vertices,axis=0).max()) < 2e-5
            stats.append({'view':view,'seconds':float(time),'collapsed_geometry':collapsed,**result})
    name = clip['name'] if clip else 'bind'
    safe_name = re.sub(r'[^A-Za-z0-9_-]','_',name)
    output = path.parent/f'preview_{safe_name}.png'
    png_write(output,image)
    result = {'source':str(path),'clip':name,'duration_seconds':float(duration),'samples':options.samples,
              'rows':views,'dimensions':list(image.shape[:2][::-1]),'frames':stats}
    output.with_suffix('.json').write_text(json.dumps(result,indent=2)+'\n')
    print(output)
    if any(s['visible_pixels'] == 0 and not s['collapsed_geometry'] for s in stats):
        raise ValueError('blank preview frame')


if __name__ == '__main__':
    main()
