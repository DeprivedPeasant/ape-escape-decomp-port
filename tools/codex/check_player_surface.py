"""Compare player water-volume classification with original GetSurfaceType instructions."""
import ctypes as ct
import json
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library, OUT
from codex.check_collision_floor import setup,full_reference
from codex.mips_ground_reference import signed

def main():
    lib=library();setup(lib)
    lib.ae_collision_surface_type.argtypes=[ct.c_void_p,ct.POINTER(ct.c_int32),ct.POINTER(ct.c_int16)]
    lib.ae_collision_surface_type.restype=ct.c_uint16
    total=water=0;rows=[]
    for area,member in sorted(da.area_members().items()):
        path=da.MEMBERS/f'{member:03X}.bin';c=lib.ae_collision_load(str(path).encode())
        assert c
        ref,_,_,_,boxes=full_reference(path.read_bytes())
        points=[(0,0,0)]
        for raw in boxes:
            b=struct.unpack('<28h',raw)
            if b[9]&255!=9:continue
            top=b[1]-b[4]
            for y in [top-1,top,top+1,top+63,top+64,top+191,top+192,b[1],b[1]+1]:
                for x,z in [(b[0],b[2]),(b[0]+b[3],b[2]+b[5]),(b[0]+b[3]+1,b[2])]:
                    points.append((x,y,z))
        try:
            for p in points:
                expected=ref.call(0x800797F8,p,limit=300000)[2]&65535
                height=ct.c_int16();actual=lib.ae_collision_surface_type(c,(ct.c_int32*3)(*p),ct.byref(height))
                assert actual==expected,(area,p,expected,actual)
                # Original only writes the volume scratch when a match was found.
                if signed(ref.read(0x800F45E4,2),16)>=0:
                    original_top=signed(ref.read(0x1F8003DE,2),16)-signed(ref.read(0x1F8003E4,2),16)
                    assert height.value==signed(original_top,16),(area,p,original_top,height.value)
                    water+=1
                total+=1
        finally:lib.ae_collision_free(c)
        rows.append(dict(area=area,comparisons=len(points)))
    result=dict(areas=rows,comparisons=total,volume_hits=water,failures=0)
    (OUT/'player-surface-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print({k:v for k,v in result.items() if k!='areas'})

if __name__=='__main__':main()
