"""Original ramp return values when the ray caller aliases output/top scratch."""
import ctypes as C,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library,collision_records,OUT
from codex.mips_ground_reference import Reference,signed

def main():
    lib=library();lib.ae_collision_probe.argtypes=[C.POINTER(C.c_int16),C.c_uint32,C.c_int,C.c_int32,C.c_int32,C.POINTER(C.c_int32),C.POINTER(C.c_int32),C.POINTER(C.c_int32)]
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes());cases=hits=0
    for area,member in sorted(da.area_members().items()):
        _,boxes=collision_records((da.MEMBERS/f'{member:03X}.bin').read_bytes())
        for raw in boxes:
            b=struct.unpack('<28h',raw);kind=b[8]&65535
            if kind&0xF000!=0x2000:continue
            for dx,dz in [(0,0),(b[3],b[5]),(-b[3],-b[5]),(b[3]//2,-b[5]//2)]:
                for y in [b[1]-b[4]//2,b[1]-32,b[1]-128]:
                    point=(b[0]+dx,y,b[2]+dz);old=(b[0],y,b[2])
                    ref.write(0x800B20B4,4,0x8013F100);ref.write(0x800B20B8,4,0x8013F120)
                    ref.ram[0x13F000:0x13F038]=raw
                    for a,values in [(0x8013F100,point),(0x8013F120,old),(0x1F800114,(64,kind,128)),(0x1F800124,(0,0,0,0,0,0))]:
                        for i,v in enumerate(values):ref.write(a+i*4,4,v)
                    expected=signed(ref.call(0x80073FD4,(0x8013F000,0x1F800114,0x1F800124))[2])
                    out=(C.c_int32*6)();actual=lib.ae_collision_probe((C.c_int16*28)(*b),kind,1,64,128,(C.c_int32*3)(*point),(C.c_int32*3)(*old),out)
                    assert expected==actual,(area,kind,point,expected,actual)
                    cases+=1;hits+=actual!=0
    report=dict(comparisons=cases,hits=hits,failures=0,scope='Ramp handler return with CollisionRayTest output scratch alias; does not compare unused output words')
    (OUT/'ray-ramp-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

if __name__=='__main__':main()
