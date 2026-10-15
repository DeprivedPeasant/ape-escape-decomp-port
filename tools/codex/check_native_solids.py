"""Validate original GetSolidHeight shape-grid traversal and contact outputs.

The oracle has its collider count set to zero to isolate the shape-grid component.
"""
import ctypes as ct
import json
import random
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from codex.check_native_collision import library, ROOT, OUT
from codex.mips_ground_reference import Reference, signed
import dump_areas as da

class SolidHit(ct.Structure):
    _fields_=[(name,ct.c_int32) for name in ('height','kind','index','type','surface','slope_x','slope_z','extra_a','extra_b')]

def reference(blob):
    ref=Reference(blob)
    ah=struct.unpack_from('<I',blob,0x4D0)[0]
    co=ah+struct.unpack_from('<I',blob,ah+16)[0]*4
    grid,shapes,boxes=struct.unpack_from('<3I',blob,co)
    width,depth=struct.unpack_from('<2H',blob,co+grid)
    for addr,value in ((0x800F4560,co+grid+4+0x80154000),(0x800F4558,co+shapes+2+0x80154000),
                       (0x800F455C,co+boxes+2+0x80154000),(0x800F45F0,width),(0x800F45F4,depth),
                       (0x800F45F8,width*256),(0x800F45FC,depth*256)):
        ref.write(addr,4,value)
    ref.write(0x800F45D4,2,0)
    ref.write(0x800F45E0,2,-1)
    ref.write(0x800AF2E4,4,0x1F8003F0)
    ref.write(0x800AF2E8,4,0x1F8003D4)
    return ref,grid+co+4,width,depth

def compare(lib,native,ref,point,wall):
    hit=SolidHit()
    ref.write(0x800F45E6,2,wall);ref.write(0x800F45E8,2,0)
    for offset in range(0,16,2):ref.write(0x1F8003F0+offset,2,0)
    expected=signed(ref.call(0x8004D8C0,point)[2])
    lib.ae_collision_grid_height(native,*point,wall,ct.byref(hit))
    if hit.height!=expected:raise AssertionError((point,wall,expected,hit.height))
    if expected!=32767:
        contact=struct.unpack('<8h',bytes(ref.scratch[0x3F0:0x400]))
        actual=(hit.kind,hit.index,signed(hit.type,16),hit.surface,hit.extra_a,hit.extra_b,hit.slope_x,hit.slope_z)
        if actual!=contact:raise AssertionError((point,wall,contact,actual))
    return expected!=32767

def main():
    lib=library()
    lib.ae_collision_load.argtypes=[ct.c_char_p];lib.ae_collision_load.restype=ct.c_void_p
    lib.ae_collision_free.argtypes=[ct.c_void_p]
    lib.ae_collision_error.restype=ct.c_char_p
    lib.ae_collision_grid_height.argtypes=[ct.c_void_p,ct.c_int32,ct.c_int32,ct.c_int32,ct.c_int32,ct.POINTER(SolidHit)]
    rng=random.Random(94423);rows=[];total=contacts=0
    for area,member in sorted(da.area_members().items()):
        path=da.MEMBERS/f'{member:03X}.bin';blob=path.read_bytes()
        native=lib.ae_collision_load(str(path).encode())
        if not native:raise AssertionError((area,lib.ae_collision_error()))
        ref,grid,w,d=reference(blob);count=0;hits=0
        try:
            for cz in range(d):
                for cx in range(w):
                    index=struct.unpack_from('<h',blob,grid+(cz*w+cx)*2)[0]
                    ys=(-8192,-1024,1) if index>=0 else (-1024,)
                    for y in ys:
                        point=(cx*512-w*256+rng.randrange(1,512),y,cz*512-d*256+rng.randrange(1,512))
                        hits+=compare(lib,native,ref,point,int(count%3==0));count+=1
            for p in ((-w*256,-1024,0),(w*256,-1024,0),(0,-1024,-d*256),(0,-1024,d*256)):
                hits+=compare(lib,native,ref,p,1);count+=1
        finally:lib.ae_collision_free(native)
        total+=count;contacts+=hits;rows.append(dict(area=area,member=member,comparisons=count,contacts=hits))
        if len(rows)%10==0:print(f'{len(rows)} areas, {total:,} grid queries, {contacts:,} contacts, failures=0',flush=True)
    result=dict(areas=rows,comparisons=total,contacts=contacts,failures=0,oracle='original GetSolidHeight; collider count zero')
    (OUT/'solid-grid-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='areas'},indent=2))
if __name__=='__main__':main()
