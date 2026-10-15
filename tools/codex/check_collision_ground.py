"""Original complete GetGroundHeight (terrain + solids + collider/water) validation."""
import ctypes as ct
import json
import random
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library,OUT
from codex.check_native_ground import Hit
from codex.check_native_solids import SolidHit
from codex.check_collision_floor import setup,full_reference
from codex.mips_ground_reference import signed
def main():
    lib=library();setup(lib)
    lib.ae_ground_load.argtypes=[ct.c_char_p];lib.ae_ground_load.restype=ct.c_void_p
    lib.ae_ground_free.argtypes=[ct.c_void_p]
    lib.ae_collision_ground_height.argtypes=[ct.c_void_p,ct.c_void_p,ct.c_int32,ct.c_int32,ct.c_int32,ct.c_int32,ct.c_int32,ct.c_int32,ct.POINTER(Hit),ct.POINTER(SolidHit)]
    rng=random.Random(94423);rows=[];total=contacts=0
    for area,member in sorted(da.area_members().items()):
        path=da.MEMBERS/f'{member:03X}.bin';blob=path.read_bytes();c=lib.ae_collision_load(str(path).encode());g=lib.ae_ground_load(str(path).encode())
        assert c and g
        ref,_,w,d,boxes=full_reference(blob)
        points=[struct.unpack_from('<3i',blob,i*0x24) for i in range(16)]
        points += [(rng.randrange(-w*256+1,w*256),rng.choice((-16384,-8192,-1024,0,1)),rng.randrange(-d*256+1,d*256)) for _ in range(32)]
        points += [(struct.unpack_from('<h',b)[0],struct.unpack_from('<h',b,2)[0]-struct.unpack_from('<h',b,8)[0]//2,struct.unpack_from('<h',b,4)[0]) for b in boxes]
        count=0
        try:
            for p in points:
                ref.write(0x800F45E6,2,count%3);ref.write(0x800F45E8,2,int(count%7==0));ref.write(0x800F45E0,2,-1)
                regs=ref.call(0x80043400,p,limit=300000)
                expected=[signed(regs[2]),signed(ref.read(0x1F8003FC,2),16),signed(ref.read(0x1F8003FE,2),16),ref.read(0x1F8003F8,2)]
                hit=Hit();contact=SolidHit()
                lib.ae_collision_ground_height(c,g,*p,count%3,-1,int(count%7==0),ct.byref(hit),ct.byref(contact))
                actual=[hit.height,hit.slope_x,hit.slope_z,hit.surface]
                if actual!=expected:raise AssertionError((p,count%3,expected,actual))
                count+=1;contacts+=contact.kind>=0
        except Exception as e:raise AssertionError((f'area {area:02X}',str(e))) from e
        finally:lib.ae_collision_free(c);lib.ae_ground_free(g)
        total+=count;rows.append(dict(area=area,comparisons=count))
        if len(rows)%10==0:print(f'{len(rows)} areas: {total:,} complete ground queries, failures=0',flush=True)
    result=dict(areas=rows,comparisons=total,contacts=contacts,failures=0)
    (OUT/'complete-ground-validation.json').write_text(json.dumps(result,indent=2)+'\n');print({k:v for k,v in result.items() if k!='areas'})
if __name__=='__main__':main()
