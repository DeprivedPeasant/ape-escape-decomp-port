"""Original ring/half-space, strip and climb-target radial response checks."""
import ctypes as ct
import json
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library,OUT,collision_records
from codex.check_collision_boxes import compare
from codex.mips_ground_reference import Reference
def main():
    lib=library()
    args=[ct.POINTER(ct.c_int16),ct.c_int32,ct.c_int32,ct.c_int32,ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32)]
    for name in ('ring','strip','radial'):getattr(lib,f'ae_collision_{name}_probe').argtypes=args
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes());total=contacts=records=0
    for area,member in sorted(da.area_members().items()):
        _,boxes=collision_records((da.MEMBERS/f'{member:03X}.bin').read_bytes())
        for record in boxes:
            b=struct.unpack('<28h',record);kind=b[8]&65535;high=kind&0xF000;low=kind&255
            probe='strip' if high==0x1000 else 'ring' if high in (0x3000,0x9000) and low in (54,68,78,79,80,81) else 'radial' if high==0x9000 and low==89 else None
            if not probe:continue
            records+=1
            for radius in (96,128,256):
                for dx,dz in ((0,0),(1,-1),(-1,1),(b[3],0),(-b[3],0),(0,b[3]),(0,-b[3]),(b[3]-64,0),(b[3]+64,0)):
                    p=(b[0]+dx,b[1]-100,b[2]+dz)
                    contacts+=bool(compare(lib,ref,b,kind,radius,512,p,(b[0],p[1],b[2]),probe=probe));total+=1
    for low in (54,68,78,79,80,81):
        b=[0,-512,0,512,512,512,0,0,0x3000|low]+[0]*19
        for x in (-600,-577,-576,-513,-512,-511,-449,-448,-447,-1,0,1,447,448,449,511,512,513,576,577,600):
            for z in (-512,-1,0,1,512):
                for y in (-961,-960,-768,-512,0,1):
                    contacts+=bool(compare(lib,ref,b,b[8],128,512,(x,y,z),(0,y,0),probe='ring'));total+=1
    result=dict(records=records,comparisons=total,contacts=contacts,failures=0)
    (OUT/'ring-strip-validation.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
if __name__=='__main__':main()
