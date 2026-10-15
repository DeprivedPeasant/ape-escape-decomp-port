"""Whole follow-camera update vs original instructions with explicit projection/ground inputs."""
import ctypes as C,json,os,random,shutil,struct,subprocess
from pathlib import Path
from src_paths import src
from check_native_camera import Slot
from check_camera_math import Tables
from check_camera_collision import Queries,Normalize,Sqrt,Ground
from mips_ground_reference import Reference,signed
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/native'
class Frame(C.Structure):
    _fields_=[('scratch',C.c_uint8*128),('controls',C.c_uint8*80)]+[(name,C.c_uint32) for name in
        ('state_flags','state','player_mode','current_area','current_gadget','stage_entry_flags','pad_held','right_stick_word','projected_sxy','status')]+[
        ('can_exit',C.c_int32),('submerged',C.c_int32),('rccar_active',C.c_int32),('stick_x',C.c_int8),('stick_y',C.c_int8)]

def main():
    OUT.mkdir(parents=True,exist_ok=True);dll=OUT/f'camera_follow_{os.getpid()}.dll'
    sources=['native_camera.c','native_camera_collision.c','native_camera_math.c','native_camera_follow.c']
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',*[str(src(name)) for name in sources],'-o',str(dll),'-static-libgcc'],check=True)
    lib=C.CDLL(str(dll));lib.ae_camera_follow_update.argtypes=[C.POINTER(Slot),C.POINTER(Frame),C.POINTER(Tables),C.POINTER(Queries),C.c_int]
    lib.ae_camera_math_load.argtypes=[C.POINTER(Tables),C.c_char_p,C.c_size_t]
    lib.ae_camera_normalize.argtypes=[C.POINTER(Tables),C.POINTER(C.c_int16),C.POINTER(C.c_uint32)]
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes();tables=Tables();assert lib.ae_camera_math_load(C.byref(tables),exe,len(exe))==1
    blob=(ROOT/'extract/members/05B.bin').read_bytes();ref=Reference(blob);math=Reference(blob);rng=random.Random(94423);cases=queries=0
    for case in range(5000):
        slot=Slot.from_buffer_copy(rng.randbytes(176));f=Frame();pos=[rng.randrange(-5000,5001),rng.randrange(-1000,0),rng.randrange(-5000,5001)]
        for off in (0xA,0x12,0x1A):
            for i,value in enumerate(pos):struct.pack_into('<h',slot.raw,off+i*2,value+rng.randrange(-400,401))
        for off in (0x4,0x38):struct.pack_into('<h' if off==4 else '<i',slot.raw,off,rng.randrange(500,1300))
        struct.pack_into('<hh',slot.raw,6,-370,-110)
        for off in (0x40,0x44,0x48,0x4C,0x70,0x74):struct.pack_into('<i',slot.raw,off,rng.randrange(0,4096))
        for off in (0x52,0x5A):struct.pack_into('<h',slot.raw,off,rng.randrange(-32,33))
        for off in (0x30,0x31,0x32,0x34,0x35,0x36,0x37,0x80):slot.raw[off]=rng.choice([0,1,3,6,18,25])
        slot.raw[0x33]=rng.choice([0,1]);slot.raw[0x81]=rng.choice([0,1])
        for off,value in ((0x10,pos[0]),(0x14,pos[2]),(0x18,pos[0]),(0x1A,pos[1]),(0x1C,pos[2]),(0x2C,pos[1]),(0x40,4096),(0x44,0)):struct.pack_into('<h',f.scratch,off,value)
        # Render position and body position are separate original fields.
        for off in (0x10,0x14):struct.pack_into('<h',f.scratch,off,struct.unpack_from('<h',f.scratch,off)[0]+rng.randrange(-128,129))
        for off in (0x40,0x44):struct.pack_into('<h',f.scratch,off,rng.randrange(-4096,4097))
        for off in (0x42,0x46):struct.pack_into('<H',f.scratch,off,rng.randrange(65536))
        for off in (0x68,0x6C,0x70):struct.pack_into('<i',f.scratch,off,rng.randrange(-60,61))
        f.controls[0x38]=rng.choice([0,1]);f.controls[0x48]=rng.choice([0,1]);f.controls[0x46]=0
        f.controls[0x3F]=rng.choice([0,1]);f.controls[0x40]=rng.choice([0,1])
        struct.pack_into('<hh',f.controls,0x34,-350,1100)
        f.state_flags=rng.choice([0x20,0x30,0x40,0x50,0x80,0xA0]);f.state=rng.choice([0,6,10,0x2D]);f.player_mode=rng.choice([0,10])
        f.current_area=rng.choice([1,7]);f.current_gadget=rng.choice([1,7,255]);f.stage_entry_flags=rng.choice([0,1]);f.pad_held=rng.randrange(16)<<12
        f.right_stick_word=rng.choice([0,100,101,127,0x10000]);f.stick_x=rng.choice([-127,-64,0,64,127]);f.stick_y=rng.choice([-127,0,127])
        f.can_exit=rng.choice([0,1]);f.submerged=rng.choice([0,1]);f.rccar_active=rng.choice([0,1]);f.projected_sxy=(rng.randrange(65536) | (rng.randrange(-100,341)&65535)<<16)
        mode=rng.choice([0,1]);ref.ram[0x13F000:0x13F0B0]=bytes(slot);ref.scratch[:128]=bytes(f.scratch);ref.ram[0xE68D0:0xE6920]=bytes(f.controls)
        for address,size,value in [(0x800EC250,4,f.state_flags),(0x800EC23E,2,f.state),(0x800EC2D2,1,f.current_gadget),(0x800F44A0,2,f.player_mode),
          (0x800F4476,2,f.current_area),(0x800F4450,4,f.stage_entry_flags),(0x800E5478,4,f.pad_held),(0x800E54A0,4,f.right_stick_word),(0x800E53C2,1,f.stick_x),(0x800E53C3,1,f.stick_y)]:ref.write(address,size,value)
        expected_ground=[];actual_ground=[];expected_norm=[];actual_norm=[];expected_sqrt=[];actual_sqrt=[];errors=[];expected_order=[];actual_order=[]
        def norm_hook(cpu,r):
            vector=[signed(cpu.read(r[4]+i*2,2),16) for i in range(3)];expected_norm.append(vector);expected_order.append(('norm',vector))
            for i,x in enumerate(vector):math.write(0x8013E000+i*2,2,x)
            value=math.call(0x800301E0,(0x8013E000,0x8013E010))[2]
            for i in range(3):cpu.write(r[5]+i*2,2,math.read(0x8013E010+i*2,2))
            return value
        def sqrt_hook(cpu,r):expected_sqrt.append(r[4]);expected_order.append(('sqrt',r[4]));return math.call(0x800303C0,(r[4],))[2]
        def height(x,z):return pos[1]+((x//512+z//512)%7)*16
        def ground_hook(cpu,r):
            xyz=[signed(r[i]) for i in (4,5,6)];expected_ground.append(xyz);expected_order.append(('ground',xyz));return height(xyz[0],xyz[2])
        def project_hook(cpu,r):cpu.write(r[5],4,f.projected_sxy);cpu.write(r[6],4,0);return 0
        def memset_hook(cpu,r):
            for i in range(r[6]):cpu.write(r[4]+i,1,r[5])
            return r[4]
        hooks={0x800301E0:norm_hook,0x800303C0:sqrt_hook,0x80043400:ground_hook,0x8002FB00:lambda cpu,r:0,0x8002FB30:lambda cpu,r:0,
          0x80031330:project_hook,0x800A3740:memset_hook,0x8005DC78:lambda cpu,r:f.can_exit,0x8006B9F8:lambda cpu,r:f.submerged,0x8004A978:lambda cpu,r:f.rccar_active}
        ref.call(0x800495F4,(0x8013F000,mode),hooks=hooks,limit=50000)
        @Normalize
        def normalize(user,io):
            actual_norm.append(list(io[:3]));actual_order.append(('norm',list(io[:3])));length=C.c_uint32()
            if not lib.ae_camera_normalize(C.byref(tables),io,C.byref(length)):errors.append('normalize domain')
            return length.value
        @Sqrt
        def square_root(user,n):actual_sqrt.append(n);actual_order.append(('sqrt',n));return signed(math.call(0x800303C0,(n,))[2])
        @Ground
        def ground(user,x,y,z):actual_ground.append([x,y,z]);actual_order.append(('ground',[x,y,z]));return height(x,z)
        q=Queries(None,normalize,square_root,ground)
        ok=lib.ae_camera_follow_update(C.byref(slot),C.byref(f),C.byref(tables),C.byref(q),mode)
        assert ok==1 and not errors and f.status==0,(case,'status',ok,f.status,errors)
        for name,actual,expected in [('slot',bytes(slot),bytes(ref.ram[0x13F000:0x13F0B0])),('scratch',bytes(f.scratch),bytes(ref.scratch[:128])),('controls',bytes(f.controls),bytes(ref.ram[0xE68D0:0xE6920]))]:
            assert actual==expected,(case,mode,name,[(i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:15])
        assert actual_ground==expected_ground and actual_norm==expected_norm and actual_sqrt==expected_sqrt,(case,'query order',actual_ground,expected_ground)
        assert actual_order==expected_order,(case,'interleaved query order',actual_order,expected_order)
        cases+=1;queries+=len(actual_ground)
    failure_cases=0
    for mode,gadget,aim,loaded,guard,expected in [(2,1,0,1,0,8),(0,3,0,1,0,2),(0,3,2,1,0,2),(0,1,0,0,0,1),(0,3,0,1,1,0)]:
        s=Slot.from_buffer_copy(bytes(slot));test=Frame.from_buffer_copy(bytes(f));test.controls[0x47]=guard;test.controls[0x46]=aim;test.current_gadget=gadget;test.status=0;tables.loaded=loaded
        before=(bytes(s),bytes(test.scratch),bytes(test.controls))
        ok=lib.ae_camera_follow_update(C.byref(s),C.byref(test),C.byref(tables),C.byref(q),mode)
        assert ok==(expected==0) and test.status==expected and before==(bytes(s),bytes(test.scratch),bytes(test.controls))
        failure_cases+=1
    report=dict(cases=cases,ground_queries=queries,failure_cases=failure_cases,failures=0,scope='Original whole follow-camera MIPS vs slot/scratch/control bytes and interleaved math/world query order. Projection and external state predicates explicitly supplied; bounded generated states, no full gameplay-camera capture claim.')
    (OUT/'camera-follow-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
    (ROOT/'notes/port/validation/native_camera_follow_validation.md').write_text(f'''# Follow camera validation

Generated by tools/codex/check_camera_follow.py. {cases} whole-update cases, {queries} ground queries and {failure_cases} fail-closed/guard checks pass, zero failures. Strict GCC warnings are errors. Every camera-slot, scratch and control byte matches the original MIPS instructions; the interleaved normalize, square-root and ground query arguments also match.

Both follow modes, arbitrary facing and padding, distinct render/body positions, still/hold timers, fixed camera, stick orbit, distance overrides, submerged/RC-car predicates, stage entry, area 7, airborne flags and state 0x2D are exercised. The imported memset thunk is modelled explicitly. SDK normalization executes original instructions in a separate test CPU and is compared with the independently verified native implementation.

Projection, player predicates and ground heights are controlled inputs. These checks establish bounded instruction equivalence, not continuous original-game camera replay or renderer equivalence. Unsupported gadget-3 variants, absent math tables and unsupported modes fail without changing slot/scratch/control state. Renderer integration remains separate.
''')

if __name__=='__main__':main()
