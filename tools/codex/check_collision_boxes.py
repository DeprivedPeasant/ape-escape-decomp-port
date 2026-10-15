"""Compare native box wall/ceiling responses with original handler 0x80073BC8."""
import ctypes as ct
import json
import random
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library,OUT,collision_records
from codex.mips_ground_reference import Reference,signed

def compare(lib,ref,box,kind,radius,height,point,previous,triangle=False,probe=None):
    b=(ct.c_int16*len(box))(*box);out=(ct.c_int32*6)(11,22,33,44,55,66)
    ref.write(0x800B20B4,4,0x8013F100);ref.write(0x800B20B8,4,0x8013F120)
    for i,value in enumerate(box):ref.write(0x8013F000+i*2,2,value)
    for address,values in ((0x8013F100,point),(0x8013F120,previous),(0x8013F040,(11,22,33,44,55,66))):
        for i,value in enumerate(values):ref.write(address+i*4,4,value)
    ref.write(0x8013F080,4,radius);ref.write(0x8013F084,4,kind);ref.write(0x8013F088,4,height)
    root=(0x80076488 if kind&0xF000==0x8000 else 0x800761AC if kind&0xF000 in (0x6000,0x7000) else 0x80075F48) if probe=='band' else 0x8007734C if probe=='strip' else 0x80074BBC if probe in ('radial','ring') else 0x80073FD4 if probe=='ramp' else 0x8007449C if triangle else 0x80073BC8
    if probe=='curved':root=0x80076AC8
    if probe=='trench':root=0x80074BBC
    expected=signed(ref.call(root,(0x8013F000,0x8013F080,0x8013F040))[2])
    function=lib.ae_collision_strip_probe if probe=='strip' else lib.ae_collision_ring_probe if probe=='ring' else lib.ae_collision_wall_band_probe if probe=='band' else lib.ae_collision_radial_probe if probe=='radial' else lib.ae_collision_ramp_probe if probe=='ramp' else lib.ae_collision_triangle_probe if triangle else lib.ae_collision_box_probe
    if probe=='curved':function=lib.ae_collision_curved_probe
    if probe=='trench':function=lib.ae_collision_trench_probe
    args=(b,kind,radius,height)
    result=function(*args,(ct.c_int32*3)(*point),(ct.c_int32*3)(*previous),out)
    eo=[signed(ref.read(0x8013F040+i*4,4)) for i in range(6)]
    eb=[signed(ref.read(0x8013F000+i*2,2),16) for i in range(len(box))]
    if result!=expected or list(out)!=eo or list(b)!=eb:
        raise AssertionError((box,kind,radius,height,point,previous,expected,result,eo,list(out),eb,list(b)))
    return result

def main():
    lib=library();lib.ae_collision_box_probe.argtypes=[ct.POINTER(ct.c_int16),ct.c_int32,ct.c_int32,ct.c_int32,ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32)]
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes());rng=random.Random(94423)
    total=contacts=records=0
    for area,member in sorted(da.area_members().items()):
        _,boxes=collision_records((da.MEMBERS/f'{member:03X}.bin').read_bytes())
        for record in boxes:
            kind=struct.unpack_from('<H',record,16)[0]
            if kind&0xF000:continue
            box=struct.unpack_from('<6h',record);records+=1
            for radius in (96,128,256):
                for dx,dz in ((0,0),(box[3],0),(-box[3]-radius,0),(0,box[5]+radius),(box[3]+radius+1,box[5]),
                              (rng.randrange(-512,513),rng.randrange(-512,513))):
                    y=box[1]-rng.choice((0,95,96,box[4]//2,box[4],box[4]+1))
                    p=(box[0]+dx,y,box[2]+dz);old=(box[0],y,box[2])
                    contacts+=compare(lib,ref,box,kind,radius,512,p,old);total+=1
        if area%10==0:print(f'area {area:02X}: {total:,} box probes, failures=0',flush=True)
    for kind in (0,60,61,62,63):
        for radius,height in ((0,0),(96,95),(128,96),(256,512)):
            for x in (-513,-384,-256,-1,0,1,256,384,513):
                for y in (-1537,-1024,-609,-608,-513,-512,-511,-417,-416,-1,0,1):
                    for z in (-513,-256,0,256,513):
                        contacts+=compare(lib,ref,(0,-512,0,256,1024,256),kind,radius,height,(x,y,z),(0,y,0));total+=1
    result=dict(box_records=records,comparisons=total,contacts=contacts,failures=0,oracle='original wall handler 0x80073BC8')
    (OUT/'box-probe-validation.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
if __name__=='__main__':main()
