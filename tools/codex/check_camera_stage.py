"""Real area stage-camera composition with explicitly scoped original endpoints."""
import argparse
import ctypes as C
import json
import os
import shutil
import struct
import subprocess
from pathlib import Path
from src_paths import src
from area_tables import area_members
from check_camera_host import Host, Snapshot
from check_camera_regions import Regions
from check_camera_lifecycle import Frame
from check_camera_collision import Queries, Normalize, Sqrt, Ground
from mips_ground_reference import Reference, signed

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/native'
BP=C.POINTER(C.c_uint8)
class Stage(C.Structure):
    _fields_=[('state',Frame),('regions',Regions),('blob',BP),('blob_size',C.c_size_t),
              ('script_table',BP),('script_table_bytes',C.c_size_t),('script_table_base',C.c_uint32),
              ('eye8',C.c_uint8*8),('target8',C.c_uint8*8),('status',C.c_uint32),('ready',C.c_int)]

def main(source_dir):
    names=['native_camera','native_camera_math','native_camera_collision','native_camera_follow',
           'native_camera_host','native_camera_regions','native_camera_bank','native_camera_lifecycle','native_camera_select',
           'native_camera_focus','native_camera_script','native_camera_motion','native_camera_molten','native_camera_blend',
           'native_ground','native_collision']
    dll=OUT/f'camera_stage_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                    str(source_dir/'native_camera_stage.c'),
                    *[str(src(n+'.c')) for n in names],
                    '-o',str(dll),'-static-libgcc','-lm'],check=True)
    lib=C.CDLL(str(dll));hp=C.POINTER(Host);sp=C.POINTER(Snapshot);stp=C.POINTER(Stage)
    lib.ae_camera_stage_init.argtypes=[stp,hp,BP,C.c_size_t,sp]
    lib.ae_camera_stage_step.argtypes=[stp,hp,sp,C.c_int]
    lib.ae_camera_stage_free.argtypes=[stp]
    lib.ae_camera_math_load.argtypes=[C.c_void_p,C.c_char_p,C.c_size_t]
    lib.ae_camera_host_reset.argtypes=[hp,sp]
    lib.ae_camera_normalize.argtypes=[C.c_void_p,C.POINTER(C.c_int16),C.POINTER(C.c_uint32)]
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes()
    math=Reference((ROOT/'extract/members/05B.bin').read_bytes())
    rows=[];initial=steps=blocked=0
    molten_calls=0
    def memset(cpu,regs):
        for i in range(regs[6]):cpu.write(regs[4]+i,1,regs[5])
        return regs[4]
    no_op=lambda cpu,regs:0
    reset_hooks={address:no_op for address in (0x80046D80,0x8004B510,0x80046670,0x800489EC,0x8004D0F0)}
    reset_hooks[0x8008C1B8]=lambda cpu,regs:0x80130000
    reset_hooks[0x800A3720]=no_op
    cases=[(a,m,0) for a,m in sorted(area_members().items())]
    cases.append((5,area_members()[5],1))
    for area,member,region_index in cases:
        blob=(ROOT/f'extract/members/{member:03X}.bin').read_bytes()
        cam=struct.unpack_from('<I',blob,0x4D8)[0]
        h=Host();h.ready=1;assert lib.ae_camera_math_load(C.byref(h.math),exe,len(exe))
        p=Snapshot();p.pos[:]=[0,-100,0];p.ground_y=1000
        if cam and struct.unpack_from('<H',blob,cam+4)[0]:
            x,y,z=struct.unpack_from('<3h',blob,cam+20+48*region_index);p.pos[:]=[x,y+128,z]
        struct.pack_into('<4h',p.render8,0,*p.pos,0);struct.pack_into('<4h',p.facing8,0,0,0,4096,0)
        assert lib.ae_camera_host_reset(C.byref(h),C.byref(p))
        @Normalize
        def normalize(user,io):
            length=C.c_uint32()
            if not lib.ae_camera_normalize(C.byref(h.math),io,C.byref(length)):h.status|=0x100
            return length.value
        @Sqrt
        def sqrt(user,n):return signed(math.call(0x800303C0,(n,))[2])
        @Ground
        def ground(user,x,y,z):return 1000
        h.queries=Queries(C.cast(C.byref(h),C.c_void_p),normalize,sqrt,ground)
        h.frame.current_area=area;h.frame.current_gadget=1;h.frame.state_flags=0x20
        h.frame.can_exit=1;h.frame.projected_sxy=192|(120<<16)
        seed_eye=bytes(h.slot.raw[0x0A:0x12]);seed_target=bytes(h.slot.raw[0x12:0x1A])
        native_blob=(C.c_uint8*len(blob)).from_buffer_copy(blob);stage=Stage()
        assert lib.ae_camera_stage_init(C.byref(stage),C.byref(h),native_blob,len(blob),C.byref(p)),('init',area,stage.status)
        ref=Reference(blob)
        if area == 5:
            overlay=(ROOT/'extract/members/18D.bin').read_bytes()
            ref.ram[0x13D000:0x13D000+len(overlay)]=overlay
        ref.write(0x800F45A0,4,0x80154000+cam);ref.write(0x800F45A4,4,0)
        ref.ram[0xF45A8:0xF45B0]=seed_eye;ref.ram[0xF45B0:0xF45B8]=seed_target
        for i,v in enumerate(p.pos):ref.write(0x800EC204+i*4,4,v)
        ref.ram[0xEC240:0xEC248]=bytes(p.facing8)
        ref.call(0x80047754,hooks=reset_hooks,limit=200000)
        expected_controls=bytearray(ref.ram[0xE68D0:0xE6920]);expected_controls[0x4C:0x50]=bytes(4)
        assert bytes(stage.state.controls)==expected_controls,('reset controls',area,
            [(hex(i),a,b) for i,(a,b) in enumerate(zip(bytes(stage.state.controls),expected_controls)) if a!=b])
        assert bytes(stage.state.slots)==ref.ram[0x130000:0x130840],('reset bank',area)
        if stage.regions.count:
            actual=C.string_at(stage.regions.records,stage.regions.count*48)
            assert actual==ref.ram[0x154000+cam+16:0x154000+cam+16+len(actual)],('reset records',area)
        initial+=1;area_steps=0;area_status=0
        def project(cpu,regs):cpu.write(regs[5],4,h.frame.projected_sxy);cpu.write(regs[6],4,0);return 0
        hooks={address:no_op for address in (0x80048454,0x8004875C,0x80048BBC,0x8004D7A0,
            0x8004D40C,0x8004CEB4,0x80046670,0x80046A78,0x8002FB00,0x8002FB30,0x8005AC28)}
        hooks.update({0x800A3740:memset,0x8004A940:no_op,0x8004A978:no_op,
            0x8005DC78:lambda cpu,regs:1,0x8006B9F8:no_op,0x80043400:lambda cpu,regs:1000,
            0x80031330:project})
        for tick in range(30):
            before=(bytes(stage.state),bytes(stage.eye8),bytes(stage.target8),bytes(h.slot),
                    bytes(h.frame.scratch),bytes(h.frame.controls),bytes(h.shadow),
                    C.string_at(stage.regions.records,stage.regions.count*48) if stage.regions.count else b'')
            ok=lib.ae_camera_stage_step(C.byref(stage),C.byref(h),C.byref(p),0)
            if not ok:
                after=(bytes(stage.state),bytes(stage.eye8),bytes(stage.target8),bytes(h.slot),
                       bytes(h.frame.scratch),bytes(h.frame.controls),bytes(h.shadow),
                       C.string_at(stage.regions.records,stage.regions.count*48) if stage.regions.count else b'')
                assert before==after,('rollback',area,tick)
                assert stage.status,('missing status',area,tick)
                blocked+=1;area_status=stage.status;break
            # Original state persists across ticks; only actual input fields are refreshed.
            ref.ram[0xEC1FC:0xEC204]=bytes(p.render8);ref.ram[0xEC240:0xEC248]=bytes(p.facing8)
            for i,v in enumerate(p.pos):ref.write(0x800EC204+i*4,4,v)
            for i,v in enumerate(p.vel):ref.write(0x800EC210+i*4,4,v)
            ref.write(0x800EC30C,4,p.ground_y)
            for address,size,value in [(0x800EC250,4,0x20),(0x800EC23E,2,0),(0x800EC2D2,1,1),
                (0x800F44A0,2,0),(0x800F4476,2,area),(0x800F4450,4,0),(0x800E5478,4,0),
                (0x800E54A0,4,0),(0x800E53C2,1,0),(0x800E53C3,1,0)]:ref.write(address,size,value)
            def trace(pc,regs):
                nonlocal molten_calls
                if pc == 0x80140B0C:molten_calls+=1
            ref.trace=trace
            ref.call(0x80047BD0,hooks=hooks,limit=200000)
            expected_controls=bytearray(ref.ram[0xE68D0:0xE6920]);expected_controls[0x4C:0x50]=bytes(4)
            assert bytes(stage.state.controls)==expected_controls,('step controls',area,tick)
            assert bytes(stage.state.slots)==ref.ram[0x130000:0x130840],('step bank',area,tick,
                [(hex(i),a,b) for i,(a,b) in enumerate(zip(bytes(stage.state.slots),ref.ram[0x130000:0x130840])) if a!=b][:15])
            assert bytes(stage.eye8)==ref.ram[0xF45A8:0xF45B0] and bytes(stage.target8)==ref.ram[0xF45B0:0xF45B8],('eye',area,tick)
            assert bytes(h.frame.scratch)==bytes(ref.scratch[:128]),('scratch',area,tick)
            steps+=1;area_steps+=1
        rows.append(dict(area=f'{area:02X}',test_region=region_index,regions=stage.regions.count,steps=area_steps,status=area_status))
        lib.ae_camera_stage_free(C.byref(stage));assert not stage.ready and not stage.regions.records
    assert molten_calls > 0,'Molten Lava handler not exercised'
    report=dict(unique_areas=len(area_members()),actual_area_inits=initial,molten_handler_calls=molten_calls,original_scoped_camera_ticks=steps,
                explicit_unsupported_areas=blocked,failures=0,areas=rows)
    (OUT/'camera-stage-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    brief={key:value for key,value in report.items() if key!='areas'};print(brief)
    (ROOT/'notes/port/validation/camera_stage_validation.md').write_text(
        '# Stage camera composition validation\n\nGenerated by check_camera_stage.py.\n\n'+
        '\n'.join(f'- {key}: {value}' for key,value in brief.items())+'\n\n'+
        'Original ResetAll runs with native allocation, projection/aim-exit/view/shake/exit endpoints '
        'supplied. CameraUpdate comparisons omit inactive aim/fixed updates, shake, global events, '
        'exit effects, framebuffer/fade/view generation and boss/RC/flight dispatch. '
        'Projection, predicates and flat ground are controlled inputs; SDK math executes. '
        'Region pointer tags are normalized to the native inert zero tag. Focus stack padding '
        'is zero in the test CPU; native zero initialization is not console-proven. '
        'Unsupported areas preserve state and report status. This is scoped composition evidence, '
        'not full CameraUpdate or console gameplay parity.\n')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-dir',type=Path,default=ROOT/'src/camera')
    main(p.parse_args().source_dir)
