"""Validate combined shape/collider/water GetSolidHeight against original instructions."""
import ctypes as ct
import json
import random
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library,OUT,collision_records
from codex.check_native_solids import reference,SolidHit
from codex.mips_ground_reference import signed

def setup(lib):
    lib.ae_collision_load.argtypes=[ct.c_char_p];lib.ae_collision_load.restype=ct.c_void_p
    lib.ae_collision_free.argtypes=[ct.c_void_p];lib.ae_collision_error.restype=ct.c_char_p
    lib.ae_collision_math_load.argtypes=[ct.c_char_p]
    assert lib.ae_collision_math_load(str(da.ROOT/'extract/disc/SCUS_944.23').encode())
    lib.ae_collision_solid_height.argtypes=[ct.c_void_p,ct.c_int32,ct.c_int32,ct.c_int32,ct.c_int32,ct.c_int32,ct.c_int32,ct.POINTER(SolidHit)]

def full_reference(blob):
    ref,grid,w,d=reference(blob)
    _,boxes=collision_records(blob)
    ref.write(0x800F45D4,2,len(boxes))
    base=ref.read(0x800F455C,4)
    for i,b in enumerate(boxes):
        ref.call(0x800889EC,(base+i*56,))
    return ref,grid,w,d,boxes

def compare(lib,native,ref,p,wall,ignore,skip):
    hit=SolidHit()
    ref.write(0x800F45E6,2,wall);ref.write(0x800F45E8,2,skip);ref.write(0x800F45E0,2,ignore)
    ref.scratch[0x3F0:]=b'\0'*16
    expected=signed(ref.call(0x8004D8C0,p,limit=300000)[2])
    lib.ae_collision_solid_height(native,*p,wall,ignore,skip,ct.byref(hit))
    if expected!=hit.height:raise AssertionError((p,wall,ignore,skip,expected,hit.height))
    if expected!=32767:
        contact=struct.unpack('<8h',bytes(ref.scratch[0x3F0:]))
        actual=(hit.kind,hit.index,signed(hit.type,16),signed(hit.surface,16),hit.extra_a,hit.extra_b,hit.slope_x,hit.slope_z)
        if actual!=contact:raise AssertionError((p,wall,ignore,skip,contact,actual))
    return hit.kind if expected!=32767 else -1

def main():
    lib=library();setup(lib);rng=random.Random(94423);total=0;contacts=[0,0];rows=[]
    for area,member in sorted(da.area_members().items()):
        path=da.MEMBERS/f'{member:03X}.bin';blob=path.read_bytes();native=lib.ae_collision_load(str(path).encode())
        if not native:raise AssertionError((area,lib.ae_collision_error()))
        ref,grid,w,d,boxes=full_reference(blob)
        points=[(rng.randrange(-w*256+1,w*256),rng.choice((-16384,-8192,-1024,0,1)),rng.randrange(-d*256+1,d*256)) for _ in range(64)]
        for record in boxes:
            b=struct.unpack('<28h',record)
            points.extend(((b[0],b[1]-b[4]//2,b[2]),(b[0]+b[3]//2,b[1]-b[4],b[2]-b[5]//2)))
        points.extend(((-w*256,0,0),(w*256,0,0),(0,0,-d*256),(0,0,d*256)))
        count=0
        try:
            for p in points:
                wall=count%3;skip=int(count%7==0);ignore=rng.randrange(len(boxes)) if boxes and count%5==0 else -1
                kind=compare(lib,native,ref,p,wall,ignore,skip)
                if kind>=0:contacts[kind]+=1
                count+=1
        except Exception as e:raise AssertionError((f'area {area:02X}',str(e))) from e
        finally:lib.ae_collision_free(native)
        total+=count;rows.append(dict(area=area,member=member,comparisons=count))
        if len(rows)%10==0:print(f'{len(rows)} areas: {total:,} combined floor queries, failures=0',flush=True)
    result=dict(areas=rows,comparisons=total,shape_contacts=contacts[0],collider_contacts=contacts[1],failures=0)
    (OUT/'combined-floor-validation.json').write_text(json.dumps(result,indent=2)+'\n');print({k:v for k,v in result.items() if k!='areas'})
if __name__=='__main__':main()
