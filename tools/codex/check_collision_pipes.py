"""Original half-pipe/trench response comparison, ordinary body mode."""
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
    lib.ae_collision_radial_probe.argtypes=args;lib.ae_collision_trench_probe.argtypes=args
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes());total=contacts=records=0
    for area,member in sorted(da.area_members().items()):
        _,boxes=collision_records((da.MEMBERS/f'{member:03X}.bin').read_bytes())
        for record in boxes:
            b=struct.unpack('<28h',record);kind=b[8]&65535;low=kind&255
            if kind&0xF000 not in (0x3000,0x9000) or low not in (36,37,83,84):continue
            records+=1;probe='trench' if low>=83 else 'radial';axis=2 if low in (36,83) else 0;r=b[axis+3]
            for radius in (96,128,256):
                for d in (-r-radius-257,-r-256,-r-1,-r,-r+256,-1,0,1,r-256,r,r+1,r+256,r+radius+257):
                    for dy in (-r-1,-r,-512,-100,0,100,512,r):
                        p=[b[0],b[1]+dy,b[2]];p[axis]+=d
                        for old in ((b[0],p[1],b[2]),tuple(p)):
                            contacts+=bool(compare(lib,ref,b,kind,radius,512,p,old,probe=probe));total+=1
        if area%20==0:print(f'area {area:02X}: {total:,} pipe probes, failures=0',flush=True)
    result=dict(records=records,comparisons=total,contacts=contacts,failures=0)
    (OUT/'pipe-probe-validation.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
if __name__=='__main__':main()
