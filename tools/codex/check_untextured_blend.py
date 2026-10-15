"""Original untextured packet paths, actual area metadata and real host GL checks."""
import collections,json,struct,subprocess,sys,types
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
sys.path.insert(0,str(ROOT/'tools/codex'))
import dump_areas as da
from mips_ground_reference import Reference
from codex.prop_materials import triangles
from codex.pack_area import pack,OUT
from codex.check_prop_rendering import parse,run
from codex.check_prop_blending import scene
from codex.check_native_viewer import ppm
from codex.build_native_viewer import build

def packet_paths():
    m=Reference((ROOT/'extract/members/05B.bin').read_bytes());m.opcodes=set()
    out,src=0x80120000,0x80110000;count=0
    paths=((0x800194E8,0x80019500,'F4'),(0x80018A24,0x80018A3C,'G4'),
           (0x8001915C,0x8001917C,'F3'),(0x80018588,0x800185A8,'G3'))
    for start,end,kind in paths:
        for value in range(256):
            m.write(src+2,1,value);m.write(0x1F8003EA,1,value)
            def trace(pc,r):
                if pc!=start:return
                if kind=='F4':r[12]=src+3;r[11]=out+3;r[15]=0xE1000200
                elif kind=='G4':r[10]=0x1F800000;r[11]=out+3;r[14]=0xE1000200
                elif kind=='F3':r[10]=src+3;r[11]=out+3;r[12]=out;r[4]=0xE1000000
                else:r[9]=0x1F800000;r[10]=out+3;r[11]=out;r[4]=0xE1000000
            m.trace=trace;m.call(start,hooks={end:lambda m,r:0},limit=20)
            assert m.read(out,4)==0xE1000200|((value<<2)&0x9FF),(kind,value)
            count+=1
    return count

def main():
    paths=packet_paths();exe=build();modes=collections.Counter();total=0
    old=types.ModuleType('frozen_pack');old.__file__=str(ROOT/'tools/codex/pack_area.py')
    source=subprocess.check_output(['git','show','0bec607:tools/codex/pack_area.py'],cwd=ROOT).decode()
    exec(compile(source,old.__file__,'exec'),old.__dict__)
    areas=0
    for area,member in sorted(da.area_members().items()):
        legacy=Path(pack(area)['pack']).read_bytes()
        assert legacy==Path(old.pack(area)['pack']).read_bytes(),(area,'legacy bytes')
        raw=triangles(member);blob=(ROOT/f'extract/members/{member:03X}.bin').read_bytes()
        result=pack(area,prop_materials=True);data=Path(result['pack']).read_bytes()
        batches,_,_,_=parse(data);cursor=0
        for _,count,texture,kind,other,flags in batches:
            if kind!=1:continue
            for record in raw[cursor:cursor+count//3]:
                if record['tpage'] is None:
                    byte=blob[record['primitive_offset']+2];word=0xE1000200|((byte<<2)&0x9FF)
                    assert record['draw_mode_byte']==byte and record['draw_mode_word']==word
                    assert record['blend_mode']==(byte>>3)&3
                    expected=25|(((byte>>3)&3)<<1) if record['semi'] else 0
                    assert flags==expected and texture==other==0xFFFFFFFF,(area,record,flags)
                    if record['semi']:total+=1;modes[(byte>>3)&3]+=1
                else:
                    assert record['draw_mode_byte'] is None and record['draw_mode_word'] is None
                    expected=1|(((record['tpage']>>5)&3)<<1) if record['semi'] else 0
                    assert flags==expected,(area,'textured flags')
            cursor+=count//3
        assert cursor==len(raw)
        assert not run(exe,result['pack'],'--check').returncode
        areas+=1;print(f'area {area:02X}: verified',flush=True)
    gl=[];base=bytearray()
    for mode in range(4):
        b,f=(80,120,160),(40,64,96)
        for fallback,debug in ((False,False),(True,False),(False,True)):
            data=scene(mode,b,f,'semi')
            struct.pack_into('<6I',data,88,6,6,0xFFFFFFFF,1,0xFFFFFFFF,9 if fallback else 25|(mode<<1))
            for i in range(6,12):struct.pack_into('<3f',data,112+i*32+20,*(x/255 for x in f))
            path=OUT/'untext_probe.aev';path.write_bytes(data);image=OUT/f'untext_{mode}_{fallback}_{debug}.ppm'
            args=['--keyboard-only','--frames',1,'--target',0,0,0,'--yaw',0,'--pitch',0,'--distance',6,'--capture',image]
            if debug:args.append('--untextured')
            r=run(exe,path,*args);assert not r.returncode,r.stderr
            if fallback:assert 'unknown' in r.stderr.lower(),r.stderr
            pixels=ppm(image)
            with image.open('rb') as file:file.readline();w,h=map(int,file.readline().split())
            actual=tuple(pixels[((h//2)*w+w//2)*3:((h//2)*w+w//2)*3+3])
            funcs=(lambda B,F:(B+F)/2,lambda B,F:B+F,lambda B,F:B-F,lambda B,F:B+F/4)
            expected=f if fallback or debug else tuple(max(0,min(255,round(funcs[mode](B,F)))) for B,F in zip(b,f))
            assert all(abs(a-e)<=1 for a,e in zip(actual,expected)),(mode,fallback,debug,actual,expected)
            gl.append(dict(mode=mode,legacy=fallback,debug=debug,actual=actual,expected=expected));base=data
    # The new branch continues the batch loop. Verify the following opaque draw
    # sees blending disabled instead of inheriting the preceding semi state.
    source=scene(0,(80,120,160),(40,64,96),'semi')
    struct.pack_into('<6I',source,88,6,6,0xFFFFFFFF,1,0xFFFFFFFF,25)
    for i in range(6,12):struct.pack_into('<3f',source,112+i*32+20,40/255,64/255,96/255)
    header=bytearray(source[:64]);struct.pack_into('<II',header,20,18,3);struct.pack_into('<I',header,36,6)
    struct.pack_into('<f',header,60,.1)
    extra=bytearray(source[112+6*32:112+12*32])
    for i in range(6):
        struct.pack_into('<f',extra,i*32+8,.1)
        struct.pack_into('<3f',extra,i*32+20,24/255,48/255,72/255)
    state=header+source[64:112]+struct.pack('<6I',12,6,0xFFFFFFFF,1,0xFFFFFFFF,0)+source[112:496]+extra+source[496:]
    path=OUT/'untext_state.aev';path.write_bytes(state);image=path.with_suffix('.ppm')
    r=run(exe,path,'--keyboard-only','--frames',1,'--target',0,0,0,'--yaw',0,'--pitch',0,'--distance',6,'--capture',image)
    assert not r.returncode,r.stderr
    pixels=ppm(image)
    with image.open('rb') as file:file.readline();w,h=map(int,file.readline().split())
    offset=((h//2)*w+w//2)*3;actual=tuple(pixels[offset:offset+3])
    assert all(abs(a-e)<=1 for a,e in zip(actual,(24,48,72))),('following opaque batch',actual)
    gl.append(dict(following_opaque_batch=True,actual=actual,expected=(24,48,72)))
    guards=0
    for flags,texture,other in ((16,0xFFFFFFFF,0xFFFFFFFF),(17,1,2),(24,0xFFFFFFFF,0xFFFFFFFF),(25,1,0xFFFFFFFF)):
        damaged=bytearray(base);struct.pack_into('<6I',damaged,88,6,6,texture,1,other,flags)
        path=OUT/'untext_bad.aev';path.write_bytes(damaged)
        assert run(exe,path,'--check').returncode,(flags,texture,other);guards+=1
    report=dict(original_packet_path_cases=paths,areas=areas,legacy_byte_equal_areas=areas,
                actual_untextured_semi_triangles=total,source_modes=dict(modes),actual_gl_cases=len(gl),
                malformed_guards=guards,failures=0)
    (OUT/'untextured-blend-validation.json').write_text(json.dumps(dict(report,cases=gl),indent=2)+'\n')
    (ROOT/'notes/port/validation/untextured_blend_validation.md').write_text('# Untextured blend validation\n\nGenerated by check_untextured_blend.py.\n\n'+
        '\n'.join(f'- {k}: {v}' for k,v in report.items())+'\n\nOriginal instruction checks cover only the four packet draw-mode branches, not complete emitters, GTE, fog or ordering. Actual source byte +2 independently verifies metadata and pack modes. Real OpenGL draws test all four host formulas, legacy opaque fallback and opaque debug views within one byte. Default legacy packs remain byte identical. Console pixel parity is not established.\n')
    print(report)
if __name__=='__main__':main()
