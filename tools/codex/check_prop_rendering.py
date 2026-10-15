"""Source-material pack checks; all generated assets and frozen baselines remain ignored."""
import hashlib,json,os,struct,subprocess,sys,types
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import dump_areas as da
import vram
from codex.pack_area import pack,OUT,prop_flags
from codex.prop_materials import triangles
from codex.build_native_viewer import build

def parse(data):
    h=struct.unpack_from('<8s8I6f',data);nv,nb,nt=h[4:7];stride=24 if h[1]==2 else 16
    batches=[struct.unpack_from('<6I' if stride==24 else '<4I',data,64+i*stride) for i in range(nb)]
    off=64+nb*stride;verts=data[off:off+nv*32];off+=nv*32;textures=[];offsets=[]
    for _ in range(nt):
        w,height,n=struct.unpack_from('<3I',data,off);off+=12
        assert (w,height,n)==(256,256,256*256*4)
        offsets.append(off);textures.append(data[off:off+n]);off+=n
    assert off==len(data)
    return batches,verts,textures,offsets

def run(exe,*args):
    r=subprocess.run([str(exe),*map(str,args)],cwd=ROOT,capture_output=True,text=True,timeout=30)
    return r

def main():
    # Frozen owned pre-change packer, read from Git; no original game/reference code copied.
    old=types.ModuleType('frozen_pack');old.__file__=str(ROOT/'tools/codex/pack_area.py')
    source=subprocess.check_output(['git','show','0bec607:tools/codex/pack_area.py'],cwd=ROOT).decode()
    exec(compile(source,old.__file__,'exec'),old.__dict__)
    exe=build();rows=[];pixels=pairs=total=semi=unknown=0;first=None
    for area,member in sorted(da.area_members().items()):
        baseline=Path(old.pack(area)['pack']).read_bytes()
        legacy=Path(pack(area)['pack']).read_bytes();assert baseline==legacy,(area,'legacy bytes')
        result=pack(area,prop_materials=True);data=Path(result['pack']).read_bytes()
        batches,vertices,textures,offsets=parse(data);_,oldvertices,_,_=parse(legacy)
        assert len(vertices)==len(oldvertices)
        for i in range(0,len(vertices),32):assert vertices[i:i+20]==oldvertices[i:i+20]
        raw=triangles(member);cursor=0;checked={};vr=None
        for b in batches:
            _,count,texture,kind,other,flags=b
            if kind!=1:assert other==0xFFFFFFFF and flags==0;continue
            for _ in range(count//3):
                r=raw[cursor];cursor+=1
                assert flags==prop_flags(r,r['tpage'] is not None)
                if r['tpage'] is None:assert texture==other==0xFFFFFFFF
                elif r['semi']:
                    assert other!=0xFFFFFFFF
                    key=r['material_key']
                    if key in checked:assert checked[key]==(texture,other)
                    else:checked[key]=(texture,other)
            if other!=0xFFFFFFFF:
                semi+=count//3
            if flags&8:unknown+=count//3
        assert cursor==len(raw)
        if checked:vr=vram.area_vram(area)
        for (page,clut),(a,b) in checked.items():
            _,_,reference=vram.texture(vr,page,clut)
            expected=b''.join(reference[y*1024:(y+1)*1024] for y in reversed(range(256)))
            o,m=textures[a],textures[b]
            # Check original palette/raw-texel STP independently at every pixel.
            px=(page&15)*64;py=(page>>4&1)*256;mode=page>>7&3;cx=(clut&63)*16;cy=clut>>6&511
            for y in range(256):
                for x in range(256):
                    def half(xx,yy):return struct.unpack_from('<H',vr,(yy*1024+xx)*2)[0]
                    if mode==2:word=half(px+x,py+y)
                    else:
                        bits=8 if mode==1 else 4
                        packed=half(px+x//(16//bits),py+y)
                        index=packed>>(bits*(x%(16//bits)))&((1<<bits)-1)
                        word=half(cx+index,cy)
                    p=((255-y)*256+x)*4
                    assert o[p+3]==(255 if word and not word&0x8000 else 0)
                    assert m[p+3]==(255 if word&0x8000 else 0)
                    combined=bytes(o[p+k]|m[p+k] for k in range(4))
                    assert combined==expected[p:p+4]
            pairs+=1;pixels+=65536
        r=run(exe,result['pack'],'--check');assert not r.returncode,(area,r.stderr)
        total+=len(raw);rows.append(dict(area=f'{area:02X}',triangles=len(raw),layers=len(checked)))
        if first is None:first=data
        print(f'area {area:02X} validated, cumulative pixels {pixels}',flush=True)
    bad=OUT/'prop_bad.aev';guards=0
    for offset,value in ((8,3),(64+12,3),(64+20,16),(64+16,4096)):
        damaged=bytearray(first);struct.pack_into('<I',damaged,offset,value);bad.write_bytes(damaged)
        assert run(exe,bad,'--check').returncode;guards+=1
    report=dict(areas=len(rows),prop_triangles=total,textured_semi_triangles=semi,untextured_semi_triangles=unknown,
                legacy_byte_equal_areas=len(rows),split_pairs=pairs,source_texel_checks=pixels,malformed_guards=guards,failures=0)
    (OUT/'prop-render-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    (ROOT/'notes/port/validation/prop_render_validation.md').write_text('# Prop material pack validation\n\nGenerated by check_prop_rendering.py.\n\n'+
        '\n'.join(f'- {k}: {v}' for k,v in report.items())+'\n\nOriginal page/CLUT texels independently verify opaque and semi coverage, zero transparency, STP and RGB at every split-layer pixel. Default packs match the frozen pre-change packer exactly; positions/UVs remain equal. This checks static asset conversion and loading, not console rendering parity.\n')
    print(report)
if __name__=='__main__':main()
