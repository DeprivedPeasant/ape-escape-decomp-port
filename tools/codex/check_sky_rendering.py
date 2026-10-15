"""Real sky packs, frozen legacy bytes and actual GL background state checks."""
import json,struct,subprocess,sys,types
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import dump_areas as da
from codex.pack_area import pack,OUT,HEADER
from codex.sky_props import load
from codex.prop_materials import triangles
from codex.build_native_viewer import build
from codex.check_prop_rendering import run
from codex.check_native_viewer import ppm
from sky_reference import SkyReference

def parse(data):
    h=HEADER.unpack_from(data);nv,nb=h[4:6];stride={1:16,2:24,3:28}[h[1]]
    batches=[struct.unpack_from('<'+str(stride//4)+'I',data,64+i*stride) for i in range(nb)]
    start=64+nb*stride;end=start+nv*32
    return batches,data[start:end],data[end:]

def synthetic(kind,world=False,semi=False):
    verts=[];batches=[]
    # Source order deliberately puts the near opaque world surface before a farther one.
    # Both must be drawn after the sky and retain correct depth/array/matrix state.
    if world:
        for z,rgb in ((.2,(96,64,32)),(0,(24,48,72))):
            first=len(verts)
            for i in (0,1,2,2,1,3):
                x,y=((-1,-1),(1,-1),(-1,1),(1,1))[i]
                verts.append((x,y,z,0,0,*(v/255 for v in rgb)))
            batches.append((first,6,0xFFFFFFFF,1,0xFFFFFFFF,0,0))
    first=len(verts)
    for i in (0,1,2,2,1,3):
        x,y=((-4,-4),(4,-4),(-4,4),(4,4))[i]
        verts.append((x,y,-4,0,0,88/255,120/255,160/255))
    batches.append((first,6,0xFFFFFFFF,1,0xFFFFFFFF,25 if semi else 0,kind))
    data=bytearray(HEADER.pack(b'AEVIEW3\0',3,1,0x5B,len(verts),len(batches),0,0,len(verts)//3,-4,-4,-4,4,4,.2))
    for b in batches:data.extend(struct.pack('<7I',*b))
    for v in verts:data.extend(struct.pack('<8f',*v))
    return data

def gl_checks(exe):
    gl=[]
    for kind in (4,5):
        for world,semi in ((False,False),(False,True),(True,False)):
            path=OUT/'sky_probe.aev';path.write_bytes(synthetic(kind,world,semi));images=[]
            for target,distance in ((0,6),(80,100)):
                image=OUT/f'sky_probe_{kind}_{world}_{semi}_{target}.ppm'
                r=run(exe,path,'--keyboard-only','--frames',1,'--target',target,0,0,'--yaw',0,'--pitch',0,
                      '--distance',distance,'--capture',image)
                if r.returncode:
                    # Sky writes no depth; capture's legacy depth/variance qualification
                    # reports failure even though it saves the valid GL image.
                    assert 'capture contains no meaningful geometry' in r.stderr,r.stderr
                    assert json.loads(Path(str(image)+'.json').read_text())['covered_pixels']==0
                pixels=ppm(image);images.append(pixels)
                with image.open('rb') as f:f.readline();w,h=map(int,f.readline().split())
                offset=((h//2)*w+w//2)*3;actual=tuple(pixels[offset:offset+3])
                if world and not target:expected=(96,64,32)
                elif semi:expected=tuple(round((b+f)/2) for b,f in zip((31,46,66),(88,120,160)))
                else:expected=(88,120,160)
                assert all(abs(a-e)<=1 for a,e in zip(actual,expected)),(kind,world,semi,target,actual,expected)
                gl.append(dict(type=kind,world=world,semi=semi,target=target,actual=actual,expected=expected))
            if not world:assert images[0]==images[1],('sky inherits camera translation',kind,semi)
    guards=0
    base=synthetic(4)
    damaged=[]
    for offset,value in ((8,2),(64+24,3),(64+12,2),(64+4,9)):
        d=bytearray(base);struct.pack_into('<I',d,offset,value);damaged.append(d)
    for coordinate in (129,1/512):
        d=bytearray(base);struct.pack_into('<f',d,92,coordinate);struct.pack_into('<f',d,52,150);damaged.append(d)
    damaged.append(base[:-1])
    for d in damaged:
        path=OUT/'sky_bad.aev';path.write_bytes(d);assert run(exe,path,'--check').returncode;guards+=1
    return gl,guards

def main():
    exe=build();old=types.ModuleType('frozen_pack');old.__file__=str(ROOT/'tools/codex/pack_area.py')
    source=subprocess.check_output(['git','show','6624d9a:tools/codex/pack_area.py'],cwd=ROOT).decode()
    exec(compile(source,old.__file__,'exec'),old.__dict__)
    reference=SkyReference();coords={};count=areas=placements=0;first=None
    for area,member in sorted(da.area_members().items()):
        for material in (False,True):
            before=Path(old.pack(area,prop_materials=material)['pack']).read_bytes()
            after=Path(pack(area,prop_materials=material)['pack']).read_bytes()
            assert before==after,(area,'legacy bytes',material)
            if material:legacy=after
        result=pack(area,sky_props=True);data=Path(result['pack']).read_bytes()
        batches,verts,textures=parse(data);_,oldverts,oldtextures=parse(legacy)
        assert len(verts)==len(oldverts) and textures==oldtextures,(area,'texture/vertex layout changed')
        raw=triangles(member);records={r['prop_index']:r for r in load(member)};cursor=0;sky_cursor={i:0 for i in records}
        for start,n,texture,kind,other,flags,sky_type in batches:
            for v in range(start,start+n):
                assert verts[v*32+12:v*32+32]==oldverts[v*32+12:v*32+32]
                if not sky_type:assert verts[v*32:v*32+12]==oldverts[v*32:v*32+12]
            if kind!=1:assert not sky_type;continue
            for tri in range(n//3):
                r=raw[cursor];cursor+=1;s=records.get(r['prop_index'])
                assert sky_type==(s['type'] if s else 0)
                if not s:continue
                ids=s['triangles'][sky_cursor[r['prop_index']]];sky_cursor[r['prop_index']]+=1
                for corner,vi in enumerate(ids):
                    original=s['vertices'][vi];key=(s['rotation'],original)
                    if key not in coords:coords[key]=reference.vertices(s['rotation'],(0,0,0),[original]*3)[0]
                    x,y,z=coords[key];expected=(x/256,-y/256,z/256)
                    actual=struct.unpack_from('<3f',verts,(start+tri*3+corner)*32)
                    assert actual==expected,(area,r,actual,expected)
                count+=1
        assert cursor==len(raw) and all(sky_cursor[i]==len(records[i]['triangles']) for i in records)
        assert result['sky_triangles']==sum(len(r['triangles']) for r in records.values())
        assert not run(exe,result['pack'],'--check').returncode
        areas+=1;placements+=len(records)
        if first is None:first=data
        print(f'area {area:02X}: sky/legacy verified',flush=True)
    gl,guards=gl_checks(exe)
    report=dict(areas=areas,sky_placements=placements,sky_triangles=count,original_coordinate_keys=len(coords),
                legacy_byte_equal_packs=areas*2,actual_gl_cases=len(gl),malformed_guards=guards,failures=0)
    (OUT/'sky-render-validation.json').write_text(json.dumps(dict(report,cases=gl),indent=2)+'\n')
    (ROOT/'notes/port/validation/sky_render_validation.md').write_text('# Sky renderer validation\n\nGenerated by check_sky_rendering.py.\n\n'+
        '\n'.join(f'- {k}: {v}' for k,v in report.items())+'\n\nSky pack positions match original integer pre-GTE paths; UV/RGB/textures and ordinary geometry match v2. Frozen v1/v2 packs remain byte identical. Real GL probes verify zero camera translation, semi blend and restoration of world matrix, pointers and depth testing. This does not establish original projection, OT/GPU arithmetic or scene pixel parity.\n')
    print(report)
if __name__=='__main__':main()
