"""Original triangle wall response differential validation (0x8007449C)."""
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
    lib=library();lib.ae_collision_triangle_probe.argtypes=[ct.POINTER(ct.c_int16),ct.c_int32,ct.c_int32,ct.c_int32,ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32)]
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes());total=contacts=records=0
    for area,member in sorted(da.area_members().items()):
        _,boxes=collision_records((da.MEMBERS/f'{member:03X}.bin').read_bytes())
        for record in boxes:
            kind=struct.unpack_from('<H',record,16)[0]
            if kind&0xF000!=0x4000:continue
            b=struct.unpack_from('<6h',record);records+=1
            for radius in (96,128,256):
                for dx,dz in ((0,0),(1,0),(0,1),(-1,0),(0,-1),(b[3],0),(0,b[5]),
                              (b[3],b[5]),(-b[3],-b[5]),(b[3]//2,-b[5]//2)):
                    p=(b[0]+dx,b[1]-b[4]//2,b[2]+dz)
                    contacts+=compare(lib,ref,b,kind,radius,512,p,(b[0],p[1],b[2]),True);total+=1
        if area%20==0:print(f'area {area:02X}: {total:,} triangle probes, failures=0',flush=True)
    for orientation in range(4):
        for x in (-513,-384,-256,-129,-1,0,1,129,256,384,513):
            for z in (-513,-384,-256,-129,-1,0,1,129,256,384,513):
                for y in (-1537,-1024,-608,-512,-416,0):
                    contacts+=compare(lib,ref,(0,-512,0,256,1024,256),0x4026+orientation*1024,128,512,(x,y,z),(0,y,0),True);total+=1
    result=dict(records=records,comparisons=total,contacts=contacts,failures=0)
    (OUT/'triangle-probe-validation.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
if __name__=='__main__':main()
