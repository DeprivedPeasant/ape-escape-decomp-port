"""Original thin, corner, edge and diagonal wall-band response comparisons."""
import ctypes as ct
import json
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library,OUT,collision_records
from codex.check_collision_boxes import compare
from codex.mips_ground_reference import Reference,signed
def main():
    lib=library();lib.ae_collision_wall_band_probe.argtypes=[ct.POINTER(ct.c_int16),ct.c_int32,ct.c_int32,ct.c_int32,ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int32)]
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes());total=contacts=records=0
    for area,member in sorted(da.area_members().items()):
        blob=(da.MEMBERS/f'{member:03X}.bin').read_bytes();shapes,_=collision_records(blob)
        ah=struct.unpack_from('<I',blob,0x4D0)[0];co=ah+struct.unpack_from('<I',blob,ah+16)[0]*4
        grid=co+struct.unpack_from('<I',blob,co)[0];w,d=struct.unpack_from('<2H',blob,grid)
        for record in shapes:
            raw=struct.unpack('<10h',record);kind=raw[9]&65535;high=kind&0xF000
            if high not in (0x1000,0x5000,0x6000,0x7000,0x8000):continue
            b=(signed(raw[0]*512-w*256+256,16),signed(-raw[1]*512,16),signed(raw[2]*512-d*256+256,16),*raw[3:6]);records+=1
            for radius in (96,128,256):
                for dx,dz in ((0,0),(1,-1),(-1,1),(-256,-256),(256,256),(-256,256),(256,-256),(-128,0),(0,128)):
                    p=(b[0]+dx,b[1]-100,b[2]+dz)
                    contacts+=bool(compare(lib,ref,b,kind,radius,512,p,(b[0],p[1],b[2]),probe='band'));total+=1
        if area%20==0:print(f'area {area:02X}: {total:,} wall-band probes, failures=0',flush=True)
    for high in (0x1000,0x5000,0x6000,0x7000,0x8000):
        for orientation in range(4):
            for x in (-397,-396,-257,-256,-141,-140,-1,0,1,140,141,256,257,396,397):
                for z in (-397,-256,-140,-1,0,1,140,256,397):
                    contacts+=bool(compare(lib,ref,(0,-512,0,256,512,256),high|orientation*1024|3,128,512,(x,-768,z),(0,-768,0),probe='band'));total+=1
    result=dict(records=records,comparisons=total,contacts=contacts,failures=0)
    (OUT/'wall-band-validation.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
if __name__=='__main__':main()
