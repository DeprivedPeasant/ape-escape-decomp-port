"""Validate collider heights, original SDK normalization and angle lookup behavior."""
import ctypes as ct
import json
import struct
import random
import math
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library,OUT,collision_records,ShapeHit
from codex.mips_ground_reference import Reference,signed

def main():
    lib=library();rng=random.Random(94423)
    lib.ae_collision_math_load.argtypes=[ct.c_char_p]
    assert lib.ae_collision_math_load(str(da.ROOT/'extract/disc/SCUS_944.23').encode())
    lib.ae_collision_atan.argtypes=[ct.c_int32,ct.c_int32]
    lib.ae_collision_normalize.argtypes=[ct.c_int32,ct.c_int32,ct.POINTER(ct.c_int32)]
    lib.ae_collision_collider.argtypes=[ct.POINTER(ct.c_int16),ct.POINTER(ct.c_int32),ct.c_int32,ct.POINTER(ShapeHit)]
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes())
    for i in range(192):
        value=struct.unpack_from('<h',da.EXE,0x800A9C14-0x80010000+0x800+i*2)[0]
        assert value==math.isqrt(1073741824//(64+i))
    math_count=0
    for x,z in [(0,0),(1,0),(0,1),(-1,-1),(32767,32767),(65535,0)]+[(rng.randrange(-32768,32768),rng.randrange(-32768,32768)) for _ in range(4096)]:
        out=(ct.c_int32*2)()
        for i,v in enumerate((x,0,z)):ref.write(0x8013F000+i*4,4,v)
        ref.call(0x8003019C,(0x8013F000,0x8013F020))
        lib.ae_collision_normalize(x,z,out)
        expected=[signed(ref.read(0x8013F020+i*2,2),16) for i in (0,2)]
        if list(out)!=expected:raise AssertionError(('normalize',x,z,expected,list(out)))
        expected=signed(ref.call(0x800145D0,(z,x))[2])
        actual=lib.ae_collision_atan(z,x)
        if actual!=expected:raise AssertionError(('atan',x,z,expected,actual))
        math_count+=2
    total=records=contacts=0
    for area,member in sorted(da.area_members().items()):
        _,boxes=collision_records((da.MEMBERS/f'{member:03X}.bin').read_bytes())
        for record in boxes:
            b=struct.unpack('<28h',record);records+=1
            for i,v in enumerate(b):ref.write(0x8013F000+i*2,2,v)
            points=[(b[0],b[1]-b[4]//2,b[2]),(b[0]+b[3],b[1]-b[4]//2,b[2]+b[5]),
                    (b[0]-b[3],b[1],b[2]-b[5]),(b[0]+b[3]+1,b[1]-1,b[2]),(b[0],b[1]+1,b[2]),
                    (b[0]+b[3]//2,b[1]-b[4],b[2]-b[5]//2),
                    (b[0]+rng.randrange(-512,513),b[1]-rng.randrange(1,1025),b[2]+rng.randrange(-512,513))]
            for p in points:
                for wall in (0,1,2):
                    ref.write(0x800F45E6,2,wall);ref.write(0x800AF2EC,4,317);ref.write(0x800AF2F0,4,-219)
                    expected=signed(ref.call(0x8004DF38,(0x8013F000,*p))[2])
                    es=[signed(ref.read(a,4)) for a in (0x800AF2EC,0x800AF2F0)]
                    hit=ShapeHit(-1024,193,317,-219)
                    actual=lib.ae_collision_collider((ct.c_int16*28)(*b),(ct.c_int32*3)(*p),wall,ct.byref(hit))
                    if expected!=actual or es!=[hit.slope_x,hit.slope_z]:raise AssertionError((area,b,p,wall,expected,actual,es,[hit.slope_x,hit.slope_z]))
                    total+=1;contacts+=bool(expected)
        if area%10==0:print(f'area {area:02X}: {total:,} collider comparisons, failures=0',flush=True)
    result=dict(records=records,comparisons=total,contacts=contacts,math_comparisons=math_count,normalization_entries_checked=192,failures=0)
    (OUT/'collider-validation.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
if __name__=='__main__':main()
