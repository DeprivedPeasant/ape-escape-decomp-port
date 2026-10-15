"""Compare native eight-probe movement pass with original build/resolve routines.

Player state side effects are excluded: this validates the physical movement bridge.
Unsupported advanced response classes are reported and never treated as empty space.
"""
import ctypes as ct
import json
import random
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library,OUT,collision_records
from codex.check_collision_floor import setup,full_reference
from codex.mips_ground_reference import signed
class Move(ct.Structure):
    _fields_=[('position',ct.c_int32*3),('velocity',ct.c_int32*3)]+[(n,ct.c_int32) for n in ('push_x','push_z','ceiling','contact')]

def compare(lib,c,ref,pos,vel,radius=128,height=512):
    body=[*pos,*vel,0,0,(height<<16)|radius,0,0,0,0,0]
    for base in (0x8013F120,0x8013F200):
        for i,v in enumerate(body):ref.write(base+i*4,4,v)
    for a,v in ((0x800B20B0,0x8013F000),(0x800B20B4,0x8013F100),(0x800B20B8,0x8013F120),(0x800B20C0,0x8013E000)):
        ref.write(a,4,v)
    ref.write(0x800B20CC,2,0) # No PlayerSetClimbTarget side effects in this physical test.
    ref.write(0x800B20C8,2,-1);ref.write(0x800B20CA,2,-1)
    ref.ram[0x13F000:0x13F080]=b'\0'*128
    ref.call(0x8007304C,(1,0x8013F200),limit=300000)
    result=ref.call(0x80073450,(1,0x8013F200),limit=300000)
    expected=[signed(ref.read(0x8013F200+i*4,4)) for i in range(6)]
    expected += [signed(ref.read(0x1F800118,4)),signed(ref.read(0x1F800120,4)),signed(result[2]),signed(ref.read(0x8013F22C,4))]
    hit=Move();status=lib.ae_collision_resolve(c,(ct.c_int32*3)(*pos),(ct.c_int32*3)(*vel),radius,height,ct.byref(hit))
    if not status:raise AssertionError(lib.ae_collision_error())
    actual=[*hit.position,*hit.velocity,hit.push_x,hit.push_z,hit.ceiling,hit.contact]
    if actual!=expected:raise AssertionError((pos,vel,expected,actual))
    return bool(hit.contact)

def main():
    lib=library();setup(lib)
    lib.ae_collision_response_supported.argtypes=[ct.c_void_p,ct.POINTER(ct.c_uint32)]
    lib.ae_collision_resolve.argtypes=[ct.c_void_p,ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32),ct.c_int32,ct.c_int32,ct.POINTER(Move)]
    rng=random.Random(94423);rows=[];total=contacts=supported=0
    for area,member in sorted(da.area_members().items()):
        path=da.MEMBERS/f'{member:03X}.bin';blob=path.read_bytes();c=lib.ae_collision_load(str(path).encode())
        if not c:raise AssertionError(lib.ae_collision_error())
        missing=(ct.c_uint32*2)();ok=lib.ae_collision_response_supported(c,missing)
        row=dict(area=area,member=member,supported=bool(ok),unsupported_shapes=missing[0],unsupported_colliders=missing[1],comparisons=0)
        try:
            if ok:
                supported+=1;ref,_,w,d,boxes=full_reference(blob);shapes,_=collision_records(blob)
                points=[struct.unpack_from('<3i',blob,i*0x24) for i in range(16)]
                for record in shapes[::max(1,len(shapes)//64)]:
                    b=struct.unpack('<10h',record)
                    points.append((b[0]*512-w*256+256,-b[1]*512-100,b[2]*512-d*256+256))
                for record in boxes:
                    b=struct.unpack('<28h',record);points.extend(((b[0],b[1]-b[4]//2,b[2]),(b[0]+b[3],b[1]-100,b[2]+b[5])))
                for p in points:
                    vel=(rng.choice((-128,-13,0,13,128)),rng.choice((-20,0,20)),rng.choice((-128,-13,0,13,128)))
                    contacts+=compare(lib,c,ref,p,vel);total+=1;row['comparisons']+=1
        except Exception as e:raise AssertionError((f'area {area:02X}',str(e))) from e
        finally:lib.ae_collision_free(c)
        rows.append(row)
        if len(rows)%10==0:print(f'{len(rows)} areas: {supported} supported, {total:,} movement comparisons, failures=0',flush=True)
    result=dict(areas=rows,supported_areas=supported,comparisons=total,contacts=contacts,failures=0)
    (OUT/'movement-collision-validation.json').write_text(json.dumps(result,indent=2)+'\n');print({k:v for k,v in result.items() if k!='areas'})
if __name__=='__main__':main()
