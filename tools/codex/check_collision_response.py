"""Original ramp and ordinary radial wall/ceiling responses."""
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
    lib.ae_collision_ramp_probe.argtypes=[ct.POINTER(ct.c_int16),ct.c_int32,ct.c_int32,ct.c_int32,ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32)]
    lib.ae_collision_radial_probe.argtypes=lib.ae_collision_ramp_probe.argtypes
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes());total=contacts=records=0
    for area,member in sorted(da.area_members().items()):
        _,boxes=collision_records((da.MEMBERS/f'{member:03X}.bin').read_bytes())
        for record in boxes:
            kind=struct.unpack_from('<H',record,16)[0];high=kind&0xF000
            probe='ramp' if high==0x2000 else 'radial' if high in (0x3000,0x9000) and kind&255 in (0,35,59) else None
            if not probe:continue
            b=struct.unpack_from('<6h',record);records+=1
            for radius in (96,128,256):
                for dx,dz in ((0,0),(1,0),(0,1),(-1,0),(0,-1),(b[3],0),(0,b[5]),(b[3],b[5]),(-b[3],-b[5]),(b[3]//2,-b[5]//2)):
                    p=(b[0]+dx,b[1]-b[4]//2,b[2]+dz)
                    contacts+=compare(lib,ref,b,kind,radius,512,p,(b[0],p[1],b[2]),probe=probe);total+=1
        if area%20==0:print(f'area {area:02X}: {total:,} ramp/radial probes, failures=0',flush=True)
    for kind in list(range(7,35))+[64,65,66,67,85,86,87,88]:
        for x,z in ((0,0),(1,0),(0,1),(-1,0),(0,-1),(-256,-256),(256,256),(257,-257)):
            for y in (-1537,-1024,-608,-512,-416,0):
                contacts+=compare(lib,ref,(0,-512,0,256,512,256),0x2000|kind|(((kind-7)%4)*1024),128,512,(x,y,z),(0,y,0),probe='ramp');total+=1
    result=dict(records=records,comparisons=total,contacts=contacts,failures=0)
    (OUT/'ramp-radial-validation.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
if __name__=='__main__':main()
