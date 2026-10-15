"""Compare curved wall responses against original instructions and external angle asset."""
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
    lib=library();lib.ae_collision_math_load.argtypes=[ct.c_char_p]
    assert lib.ae_collision_math_load(str(da.ROOT/'extract/disc/SCUS_944.23').encode())
    lib.ae_collision_curved_probe.argtypes=[ct.POINTER(ct.c_int16),ct.c_int32,ct.c_int32,ct.c_int32,ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32)]
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes());total=contacts=records=0
    for area,member in sorted(da.area_members().items()):
        _,boxes=collision_records((da.MEMBERS/f'{member:03X}.bin').read_bytes())
        for record in boxes:
            b=struct.unpack('<28h',record);kind=b[8]&65535
            if kind&0xF000 not in (0x3000,0x9000) or kind&255 not in (70,71,72,73):continue
            records+=1
            for radius in (96,128,256):
                for dx,dz in ((0,0),(1,-1),(-1,1),(b[3],0),(-b[3],0),(0,b[5]),(0,-b[5]),(b[3]+radius,0),(-b[3]-radius,0),(b[3],b[5]),(-b[3],-b[5]),(b[3]//2,b[5]//2)):
                    for dy in (-b[4]-1,-b[4],-100,0,512):
                        p=(b[0]+dx,b[1]+dy,b[2]+dz)
                        contacts+=bool(compare(lib,ref,b,kind,radius,512,p,(b[0],p[1],b[2]),probe='curved'));total+=1
        if area%20==0:print(f'area {area:02X}: {total:,} curved probes, failures=0',flush=True)
    result=dict(records=records,comparisons=total,contacts=contacts,failures=0)
    (OUT/'curved-probe-validation.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
if __name__=='__main__':main()
