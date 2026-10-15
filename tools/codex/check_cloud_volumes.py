import subprocess
"""Original MIPS collider scan and mode-0 ray comparisons on real area records."""
import argparse,ctypes as C,json,os,random,shutil,struct,subprocess,sys
from pathlib import Path
from src_paths import src
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_collision_floor import full_reference
from codex.mips_ground_reference import signed
ROOT=da.ROOT;OUT=ROOT/'extract/codex/native'

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--area',type=lambda s:int(s,0))
    ap.add_argument('--regression-case',action='store_true',help='single original full-sweep ray discrepancy')
    args=ap.parse_args()
    source=ROOT/'src/player';dll=OUT/f'volume_review_{os.getpid()}.dll'
    files=['native_player_host.c','native_collision.c','native_ground.c']
    command=[shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared','-static-libgcc']
    command += [str(ROOT/'tools/codex/volume_review_bridge.c')]+[src(p) for p in files]
    command += [str(source/p) for p in ['native_player.c','native_player_body.c','native_player_volume.c','native_player_pad.c','native_gadget_slots.c']]+[str(ROOT/'src/core'/p) for p in ['ae_ram.c','ae_pad.c']]+['-lm','-o',str(dll)]
    subprocess.run(command,check=True);lib=C.CDLL(str(dll))
    lib.volume_review_load.argtypes=[C.c_char_p]*3;lib.volume_review_load.restype=C.c_void_p
    lib.volume_review_free.argtypes=[C.c_void_p]
    lib.volume_review_scan.argtypes=[C.c_void_p,C.POINTER(C.c_int32),C.POINTER(C.c_uint32)]
    lib.volume_review_ray.argtypes=[C.c_void_p]+[C.POINTER(C.c_int32)]*3+[C.POINTER(C.c_uint32)]
    rng=random.Random(94423);rows=[];failures=[];totals=dict(scan=0,ray=0,unknown=0)
    for area,member in sorted(da.area_members().items()):
        if args.area is not None and area!=args.area:continue
        path=da.MEMBERS/f'{member:03X}.bin';blob=path.read_bytes();ref,_,w,d,boxes=full_reference(blob)
        host=lib.volume_review_load(str(path).encode(),str(ROOT/'extract/disc/SCUS_944.23').encode(),str(da.MEMBERS/'008.bin').encode());assert host
        points=[struct.unpack_from('<3i',blob,i*0x24) for i in range(16)]
        for raw in boxes:
            b=struct.unpack('<28h',raw)
            points.extend([(b[0],b[1]-b[4]//2,b[2]),(b[0]+b[3],b[1]-b[4],b[2]+b[5])])
        if args.regression_case:points=[(-7936,-2048,2368)]
        counts=dict(scan=0,ray=0,unknown=0)
        try:
            for pos in points:
                body=0x8013F200
                for i,v in enumerate(pos):ref.write(body+i*4,4,v)
                ref.write(body+0x34,2,-1);ref.write(0x800F44A0,2,0);ref.write(0x800F45E6,2,1)
                ref.call(0x800792C8,(body,2),limit=300000)
                expected=signed(ref.read(body+0x34,2),16);status=C.c_uint32()
                actual=lib.volume_review_scan(host,(C.c_int32*3)(*pos),C.byref(status))
                if status.value:counts['unknown']+=1
                elif actual!=expected:failures.append(dict(area=area,query='scan',position=pos,expected=expected,actual=actual))
                else:counts['scan']+=1
                vel=(0,rng.choice([-20,0,20]),0)
                if args.regression_case:vel=(0,-20,0)
                for i,v in enumerate([*pos,*vel,0,0,(512<<16)|128,0,0,0,0,0]):ref.write(body+i*4,4,v)
                for a,v in [(0x800B20B0,0x8013F000),(0x800B20B4,0x8013F100),(0x800B20B8,0x8013F120),(0x800B20BC,0x8013F300),(0x800B20C0,0x8013E000)]:ref.write(a,4,v)
                # Original response helpers read the body snapshot when evaluating crossings.
                ref.ram[0x13F120:0x13F158]=ref.ram[0x13F200:0x13F238]
                ref.write(0x800B20CC,2,0);ref.call(0x8007304C,(1,body),limit=300000)
                probe=(*pos,64,128)
                for i,v in enumerate(probe):ref.write(0x8013F400+i*4,4,v)
                expected=signed(ref.call(0x80078C9C,(0,0x8013F400,0x8013F440),limit=300000)[2])
                actual=lib.volume_review_ray(host,(C.c_int32*3)(*pos),(C.c_int32*3)(*vel),(C.c_int32*5)(*probe),C.byref(status))
                if status.value:counts['unknown']+=1
                elif actual!=expected:failures.append(dict(area=area,query='ray',position=pos,velocity=vel,probe=probe,expected=expected,actual=actual))
                else:counts['ray']+=1
                if len(failures)>=20:break
        finally:lib.volume_review_free(host)
        for k,v in counts.items():totals[k]+=v
        rows.append(dict(area=area,**counts))
        if len(rows)%10==0:print(len(rows),'areas',totals,'failures',len(failures),flush=True)
        if len(failures)>=20:break
    report=dict(cloud_revision=subprocess.run(['git','-C',str(ROOT),'rev-parse','HEAD'],capture_output=True,text=True).stdout.strip(),areas=rows,totals=totals,failures=failures)
    filename='cloud-volume-validation.json' if args.area is None else f'cloud-volume-validation-{args.area:02X}.json'
    (OUT/filename).write_text(json.dumps(report,indent=2)+'\n')
    print(totals,'failures',len(failures));assert not failures,failures[:3]

if __name__=='__main__':main()
