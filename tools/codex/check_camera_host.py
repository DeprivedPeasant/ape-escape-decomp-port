"""Original camera snapshot/predicates and real-terrain host adapter checks."""
import ctypes as C,json,os,random,shutil,struct,subprocess
from pathlib import Path
from src_paths import src
from check_native_camera import Slot
from check_camera_math import Tables
from check_camera_follow import Frame
from check_camera_collision import Queries
from mips_ground_reference import Reference,signed
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/native'
class Host(C.Structure):
    _fields_=[('slot',Slot),('frame',Frame),('math',Tables),('queries',Queries),('ground',C.c_void_p),('collision',C.c_void_p),('shadow',C.c_uint8*16),('status',C.c_uint32),('ready',C.c_int)]
class Snapshot(C.Structure):
    _fields_=[('render8',C.c_uint8*8),('facing8',C.c_uint8*8),('pos',C.c_int32*3),('vel',C.c_int32*3),('ground_y',C.c_int32)]

def main():
    OUT.mkdir(parents=True,exist_ok=True);dll=OUT/f'camera_host_{os.getpid()}.dll'
    sources=['native_camera','native_camera_collision','native_camera_math','native_camera_follow','native_camera_host','native_collision','native_ground']
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',*[str(src(name+'.c')) for name in sources],'-o',str(dll),'-lm','-static-libgcc'],check=True)
    lib=C.CDLL(str(dll));hp=C.POINTER(Host);sp=C.POINTER(Snapshot)
    lib.ae_camera_host_snapshot.argtypes=[hp,sp];lib.ae_camera_host_init.argtypes=[hp,C.c_char_p,C.c_size_t,C.c_void_p,C.c_void_p,sp]
    lib.ae_camera_host_step.argtypes=[hp,sp,C.c_int]
    lib.ae_camera_can_exit_stage.argtypes=[C.c_uint32,C.c_uint32,C.c_uint32];lib.ae_camera_player_submerged.argtypes=[C.c_int,C.c_int32,C.c_int32]
    for name in ('ground','collision'):
        getattr(lib,'ae_'+name+'_load').argtypes=[C.c_char_p];getattr(lib,'ae_'+name+'_load').restype=C.c_void_p
        getattr(lib,'ae_'+name+'_free').argtypes=[C.c_void_p]
    lib.ae_collision_math_load.argtypes=[C.c_char_p]
    blob=(ROOT/'extract/members/05B.bin').read_bytes();ref=Reference(blob);rng=random.Random(94423)
    for case in range(2000):
        h=Host();p=Snapshot.from_buffer_copy(rng.randbytes(C.sizeof(Snapshot)))
        h.frame=Frame.from_buffer_copy(rng.randbytes(C.sizeof(Frame)))
        ref.scratch[:128]=bytes(h.frame.scratch)
        player=0x800EC1E0
        for off,values in ((0x1C,bytes(p.render8)),(0x60,bytes(p.facing8))):ref.ram[player-0x80000000+off:player-0x80000000+off+8]=values
        for off,values in ((0x24,p.pos),(0x30,p.vel)):
            for i,x in enumerate(values):ref.write(player+off+i*4,4,x)
        ref.write(player+0x12C,4,p.ground_y);ref.call(0x80047DA4)
        before=bytes(h.frame.controls);lib.ae_camera_host_snapshot(C.byref(h),C.byref(p))
        assert bytes(h.frame.scratch)==bytes(ref.scratch[:128]) and bytes(h.frame.controls)==before
        assert bytes(h.shadow)==bytes(ref.ram[0xE693C:0xE694C]),case
    exit_cases=0
    for code in range(256):
        for state in (0,6,0x23,0x4A,0x2D,0xFFFF):
            for flags in (0,0x20,0x50,0xE0,0xFFFFFFFF):
                ref.write(0x800F447C,1,code);ref.write(0x800EC23E,2,state);ref.write(0x800EC250,4,flags)
                expected=ref.call(0x8005DC78)[2]
                assert lib.ae_camera_can_exit_stage(code,state,flags)==expected,(code,state,flags)
                exit_cases+=1
    for case in range(2000):
        active=rng.randrange(256);water=rng.randrange(-32768,32768);y=rng.randrange(-2147483648,2147483648)
        ref.write(0x800BAD7B,1,active);ref.write(0x800EC208,4,y);ref.write(0x800EC31C,2,water)
        assert lib.ae_camera_player_submerged(active,water,y)==ref.call(0x8006B9F8)[2],(active,water,y)
    path=str(ROOT/'extract/members/05B.bin').encode();g=lib.ae_ground_load(path);c=lib.ae_collision_load(path);assert g and c
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes();assert lib.ae_collision_math_load(str(ROOT/'extract/disc/SCUS_944.23').encode())==1
    h=Host();p=Snapshot();struct.pack_into('<hhhh',p.facing8,0,0,0,4096,0)
    p.pos[:]=[0,-100,0];p.ground_y=0;struct.pack_into('<hhhh',p.render8,0,*p.pos,0)
    assert lib.ae_camera_host_init(C.byref(h),exe,len(exe),g,c,C.byref(p))==1
    h.frame.current_area=1;h.frame.current_gadget=1;h.frame.can_exit=1;h.frame.state_flags=0x20;h.frame.projected_sxy=192|(120<<16)
    for case in range(500):
        p.pos[0]=case%100-50;struct.pack_into('<hhhh',p.render8,0,*p.pos,0)
        assert lib.ae_camera_host_step(C.byref(h),C.byref(p),0)==1 and h.status==0,case
    h.frame.current_gadget=3;h.frame.controls[0x46]=0;before=(bytes(h.slot),bytes(h.frame.scratch),bytes(h.frame.controls),bytes(h.shadow))
    assert lib.ae_camera_host_step(C.byref(h),C.byref(p),0)==0 and h.status&2
    assert before==(bytes(h.slot),bytes(h.frame.scratch),bytes(h.frame.controls),bytes(h.shadow))
    # Both horizontal components at -32768 exceed the signed SDK sum domain.
    # A callback failure must roll back the whole host update too.
    h.frame.current_gadget=1
    struct.pack_into('<h',h.slot.raw,0xA,0);struct.pack_into('<h',h.slot.raw,0xE,0)
    p.pos[:]=[-32768,-100,-32768];struct.pack_into('<hhhh',p.render8,0,*p.pos,0)
    before=(bytes(h.slot),bytes(h.frame.scratch),bytes(h.frame.controls),bytes(h.shadow))
    assert lib.ae_camera_host_step(C.byref(h),C.byref(p),0)==0 and h.status&0x100
    assert before==(bytes(h.slot),bytes(h.frame.scratch),bytes(h.frame.controls),bytes(h.shadow))
    assert lib.ae_camera_host_init(C.byref(Host()),exe,100,g,c,C.byref(p))==0
    lib.ae_ground_free(g);lib.ae_collision_free(c)
    report=dict(snapshot_cases=2000,exit_cases=exit_cases,submerged_cases=2000,terrain_frames=500,failure_cases=3,failures=0)
    (OUT/'camera-host-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
    (ROOT/'notes/port/validation/native_camera_host_validation.md').write_text(f'''# Camera host validation

Generated by tools/codex/check_camera_host.py. {report}.

Snapshots compare all scratch bytes and the 16-byte shadow with the original instructions, using distinct render/body positions and arbitrary velocity, facing and padding bits. Stage-exit and submerged predicates compare against the original instructions. Strict GCC build and 500 host frames use actual Fossil Field terrain and original SDK tables. Failure checks preserve camera state for an unsupported gadget variant and a normalization overflow, and reject a truncated EXE.

The terrain sequence is an adapter exercise, not a player gameplay capture. Projection and frame predicates remain caller inputs. Renderer integration and actual stage-camera event dispatch remain separate.
''')

if __name__=='__main__':main()
