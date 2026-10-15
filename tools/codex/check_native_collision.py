"""Differential collision tests against original extracted MIPS instructions.

No reconstructed C is used as the oracle. Game bytes and generated logs stay in extract/.
"""
import ctypes as ct
import json
import math
import os
import random
import shutil
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.mips_ground_reference import Reference, signed

ROOT=da.ROOT
OUT=ROOT/'extract/codex/native'
class ShapeHit(ct.Structure):
    _fields_=[(name,ct.c_int32) for name in ('height','extra','slope_x','slope_z')]

def library():
    OUT.mkdir(parents=True,exist_ok=True)
    # Windows keeps loaded DLLs locked; independent validators need separate files.
    dll=OUT/f'native_collision_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                    str(ROOT/'src/collision/native_collision.c'),str(ROOT/'src/collision/native_ground.c'),'-o',str(dll),'-static-libgcc'],check=True)
    lib=ct.CDLL(str(dll))
    lib.ae_collision_shape.argtypes=[ct.c_int32,ct.POINTER(ct.c_int16),ct.POINTER(ct.c_int32),ct.c_int32,ct.POINTER(ShapeHit)]
    lib.ae_collision_sqrt.argtypes=[ct.c_int32,ct.c_int32]
    return lib

def collision_records(blob):
    ah=struct.unpack_from('<I',blob,0x4D0)[0]
    start=ah+struct.unpack_from('<I',blob,ah+16)[0]*4
    offsets=struct.unpack_from('<5I',blob,start)
    blocks=[]
    for offset,stride in zip(offsets[1:3],(20,56)):
        count=struct.unpack_from('<H',blob,start+offset)[0]
        blocks.append([blob[start+offset+2+i*stride:start+offset+2+(i+1)*stride] for i in range(count)])
    return blocks

def shape_compare(lib,ref,kind,half,point,wall):
    # Nonzero seeds check exactly which outputs a handler preserves on misses/hits.
    hit=ShapeHit(-1024,193,317,-219)
    ref.write(0x800AF2EC,4,hit.slope_x);ref.write(0x800AF2F0,4,hit.slope_z)
    ref.write(0x800F45E6,2,wall)
    for i,value in enumerate(half):ref.write(0x8013F000+i*2,2,value)
    for i,value in enumerate(point):ref.write(0x8013F010+i*4,4,value)
    ref.write(0x8013F020,4,hit.height);ref.write(0x8013F024,4,hit.extra)
    address=da.exe_u32(0x800AF180+kind*4)
    result=ref.call(address,(kind,0x8013F000,0x8013F010,0x8013F020))
    expected=[signed(result[2]),signed(ref.read(0x8013F020,4)),signed(ref.read(0x8013F024,4)),
              signed(ref.read(0x800AF2EC,4)),signed(ref.read(0x800AF2F0,4))]
    status=lib.ae_collision_shape(kind,(ct.c_int16*3)(*half),(ct.c_int32*3)(*point),wall,ct.byref(hit))
    actual=[status,hit.height,hit.extra,hit.slope_x,hit.slope_z]
    if actual!=expected:raise AssertionError((kind,half,point,wall,expected,actual))

def main():
    lib=library();rng=random.Random(94423)
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes())
    sqrt_cases=list(range(1025))+[rng.randrange(1,0x7FFFFFFF) for _ in range(4096)]
    for sdk,root in ((0,0x80046584),(1,0x800303C0)):
        for n in sqrt_cases:
            expected=ref.call(root,(n,))[2]
            assert lib.ae_collision_sqrt(n,sdk)==expected,(sdk,n,expected,lib.ae_collision_sqrt(n,sdk))
    table_checks=0
    for i in range(192):
        actual=struct.unpack_from('<h',da.EXE,0x800AE5C0-0x80010000+0x800+i*2)[0]
        assert actual==math.isqrt((64+i)*262144)
        table_checks+=1
    shapes=Counter();colliders=Counter();comparisons=0;rows=[]
    for area,member in sorted(da.area_members().items()):
        records,boxes=collision_records((da.MEMBERS/f'{member:03X}.bin').read_bytes())
        for record in records:
            kind=record[18];half=struct.unpack_from('<3h',record,6);shapes[kind]+=1
            for p,wall in (((0,-1024,0),0),((half[0],-1024,half[2]),1),
                            ((rng.randrange(-320,321),-1024,rng.randrange(-320,321)),1)):
                shape_compare(lib,ref,kind,half,p,wall);comparisons+=1
        colliders.update(struct.unpack_from('<H',b,16)[0] for b in boxes)
        rows.append(dict(area=area,member=member,shapes=len(records),colliders=len(boxes)))
        if len(rows)%10==0:print(f'{len(rows)} areas, {comparisons:,} original shape comparisons, failures=0',flush=True)
    # Additional defined handlers absent from on-disk shape grids. Types74..77 use
    # an unspecified caller t0 outside the center; do not manufacture a contract.
    for kind in range(89):
        if 74<=kind<=77:continue
        for half in ((256,512,256),(73,127,91),(0,512,256),(256,-127,256)):
            for x in (-321,-320,-257,-256,-65,-64,-1,0,1,64,65,256,257,320,321):
                for z in (-257,-64,-1,0,1,64,257):
                    for wall in (0,1):
                        shape_compare(lib,ref,kind,half,(x,-1024,z),wall);comparisons+=1
    report=dict(areas=rows,shape_records=sum(shapes.values()),collider_records=sum(colliders.values()),
                shape_types=dict(sorted(shapes.items())),collider_types={f'{k:04X}':v for k,v in sorted(colliders.items())},
                shape_comparisons=comparisons,sqrt_comparisons=len(sqrt_cases)*2,lookup_entries_checked=table_checks,failures=0)
    (OUT/'collision-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('areas','shape_types','collider_types')},indent=2))
if __name__=='__main__':main()
