"""Camera slot leaf routines compared with original executable instructions."""
import ctypes as C,json,os,random,shutil,struct,subprocess
from pathlib import Path
from mips_ground_reference import Reference
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/native'
class Slot(C.Structure):
    _fields_=[('raw',C.c_uint8*176)]

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    dll=OUT/f'camera_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',str(ROOT/'src/camera/native_camera.c'),'-o',str(dll),'-static-libgcc'],check=True)
    lib=C.CDLL(str(dll));sp=C.POINTER(Slot);ip=C.POINTER(C.c_int32);hp=C.POINTER(C.c_int16);bp=C.POINTER(C.c_uint8)
    lib.ae_camera_reset_follow.argtypes=[sp,ip,hp,C.c_int32]
    lib.ae_camera_init_follow.argtypes=[sp,C.c_int32,bp,bp,ip,hp,C.c_int32]
    lib.ae_camera_clear_script.argtypes=[sp]
    lib.ae_camera_stick_orbit.argtypes=[sp,C.c_int,C.c_int,C.c_uint16,C.c_int8,C.c_int8,C.c_uint32,C.c_int16]
    for name in ('eye','target'):getattr(lib,'ae_camera_smooth_'+name).argtypes=[sp,hp,ip]
    ref=Reference((ROOT/'extract/members/05B.bin').read_bytes());rng=random.Random(94423);counts={}
    addr=0x8013F000
    def seed():
        raw=rng.randbytes(176);ref.ram[0x13F000:0x13F0B0]=raw;return Slot.from_buffer_copy(raw)
    def compare(slot,topic,case):
        expected=bytes(ref.ram[0x13F000:0x13F0B0]);actual=bytes(slot)
        assert actual==expected,(topic,case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b])
        counts[topic]=counts.get(topic,0)+1
    def player(pos,facing):
        for i,v in enumerate(pos):ref.write(0x800EC1E0+0x24+i*4,4,v)
        for i,v in enumerate(facing):ref.write(0x800EC1E0+0x60+i*2,2,v)
    for i in range(500):
        slot=seed();pos=[rng.randrange(-100000,100001) for _ in range(3)];facing=[rng.randrange(-32768,32768) for _ in range(3)];yaw=rng.randrange(-8192,8192)
        player(pos,facing)
        def atan(cpu,r):
            assert r[4]==(-facing[0]&0xFFFFFFFF) and r[5]==(-facing[2]&0xFFFFFFFF)
            return yaw
        ref.call(0x800493D4,(addr,),hooks={0x80046340:atan})
        lib.ae_camera_reset_follow(C.byref(slot),(C.c_int32*3)(*pos),(C.c_int16*3)(*facing),yaw)
        compare(slot,'reset',i)
        slot=seed();kind=rng.choice([-1,0,1,2,3,4,5,6,7]);eye=rng.randbytes(8);target=rng.randbytes(8)
        ref.ram[0xF45A8:0xF45B0]=eye;ref.ram[0xF45B0:0xF45B8]=target
        ref.call(0x80047104,(addr,kind),hooks={0x80046340:atan})
        lib.ae_camera_init_follow(C.byref(slot),kind,(C.c_uint8*8).from_buffer_copy(eye),(C.c_uint8*8).from_buffer_copy(target),(C.c_int32*3)(*pos),(C.c_int16*3)(*facing),yaw)
        compare(slot,'init',i)
    for i in range(3000):
        slot=seed();can_exit=rng.choice([0,1]);fixed=rng.choice([0,1,2]);mag=rng.choice([0,100,101,127,65535]);sx=rng.randrange(-128,128);sy=rng.randrange(-128,128);held=rng.randrange(16)<<12;py=rng.randrange(-32768,32768)
        ref.write(0x800E6908,1,fixed);ref.write(0x800E54A0,2,mag);ref.write(0x800E53C2,1,sx);ref.write(0x800E53C3,1,sy);ref.write(0x800E5478,4,held);ref.write(0x1F80001A,2,py)
        ref.call(0x8004923C,(addr,),hooks={0x8005DC78:lambda cpu,r:can_exit})
        lib.ae_camera_stick_orbit(C.byref(slot),can_exit,fixed,mag,sx,sy,held,py)
        compare(slot,'orbit',i)
    for name,root,offset in (('eye',0x80047464,0),('target',0x80047558,8)):
        for i in range(1000):
            slot=seed();desired=[rng.randrange(-32768,32768) for _ in range(3)];div=[rng.choice([1,2,4,8,16,32,64,-1,-7,2147483647,-2147483648]) for _ in range(3)]
            for j,v in enumerate(desired):ref.write(0x1F800000+offset+j*2,2,v)
            ref.call(root,(addr,*div))
            assert getattr(lib,'ae_camera_smooth_'+name)(C.byref(slot),(C.c_int16*3)(*desired),(C.c_int32*3)(*div))==1
            compare(slot,'smooth_'+name,i)
        for i in range(150):
            slot=seed();desired=[rng.randrange(-32768,32768) for _ in range(3)];div=[2,8,32];div[i%3]=0
            for j,v in enumerate(desired):ref.write(0x1F800000+offset+j*2,2,v)
            try:ref.call(root,(addr,*div))
            except ValueError as error:assert 'divide by zero' in str(error)
            else:raise AssertionError('Original zero-divisor case did not trap')
            assert getattr(lib,'ae_camera_smooth_'+name)(C.byref(slot),(C.c_int16*3)(*desired),(C.c_int32*3)(*div))==0
            compare(slot,'trap_'+name,i)
    for i in range(100):
        slot=seed();ref.call(0x8004C2E4,(addr,));lib.ae_camera_clear_script(C.byref(slot));compare(slot,'clear_slot_script',i)
    report=dict(cases=sum(counts.values()),counts=counts,failures=0,scope='Whole176byte slot compared against original MIPS. Atan2Safe and PlayerCanExitStage return values supplied explicitly. Script-clear global flags not represented by slot-only API; no full follow/collision/region camera claim.')
    (OUT/'camera-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
    (ROOT/'notes/port/validation/native_camera_validation.md').write_text(f'''# Native camera leaf validation

Generated by tools/codex/check_native_camera.py. Sonnet implemented the bounded module; Codex reviewed it and compared it with the original executable instructions. {report['cases']} cases pass, zero failures. Counts: {counts}.

All176slot bytes are compared, including untouched fields and copied padding. Cases cover signed coordinates/facing, original initialization branches, button priority, orbit height bounds, stick magnitude100/101 gating, disabled exit conditions, wrapped32smoothing and signed division. Atan2Safe and PlayerCanExitStage return values are supplied explicitly, so this does not validate those helpers. Slot script clearing excludes two global flags, to be handled by a future camera-world adapter. Full follow, region/event modes, collision and renderer integration remain pending.
''')

if __name__=='__main__':main()
