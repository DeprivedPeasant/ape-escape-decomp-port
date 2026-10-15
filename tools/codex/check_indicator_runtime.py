"""Whole-original proximity initialization and activation composition checks."""
import ctypes as C
import json
import os
import random
import shutil
import struct
import subprocess
from pathlib import Path
from src_paths import src
from check_camera_math import Tables
from check_native_indicators import Indicator
from check_indicator_triggers import Trigger, Events
from check_indicator_activation import Activation
from mips_ground_reference import Reference, signed
from pack_indicators import registrations, area_members
import dump_objects as objects

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'extract/codex/native'
class RuntimeEvents(C.Structure):
    _fields_ = [('trigger', Events), ('activation', Activation)]

def main():
    dll = OUT/f'indicator_runtime_{os.getpid()}.dll'
    names = ('native_indicator_runtime','native_indicator_trigger','native_indicators',
             'native_camera_math','native_collision','native_ground')
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',
        '-shared',*[str(src(n+'.c')) for n in names],'-o',str(dll),
        '-static-libgcc','-lm'],check=True)
    lib=C.CDLL(str(dll));bp=C.POINTER(C.c_uint8);ip=C.POINTER(Indicator)
    lib.ae_indicator_runtime_init.argtypes=[C.POINTER(Trigger),bp,ip,C.POINTER(C.c_uint32),
                                         C.c_uint32,C.c_int,C.POINTER(Tables)]
    lib.ae_indicator_runtime_step.argtypes=[C.POINTER(Trigger),bp,ip,C.c_uint32,
        C.POINTER(C.c_int32),C.c_uint32,C.c_uint32,C.c_int,C.POINTER(RuntimeEvents)]
    lib.ae_camera_math_load.argtypes=[C.POINTER(Tables),C.c_char_p,C.c_size_t]
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes();math=Tables()
    assert lib.ae_camera_math_load(C.byref(math),exe,len(exe))
    ref=Reference((ROOT/'extract/members/05B.bin').read_bytes());rng=random.Random(9106094423)
    actual=[]
    for area,member in sorted(area_members().items()):
        blob=(ROOT/f'extract/members/{member:03X}.bin').read_bytes()
        _,instances=objects.parse(blob)
        ah=struct.unpack_from('<I',blob,0x4D0)[0]
        co=ah+struct.unpack_from('<I',blob,ah+16)[0]*4
        boxes=co+struct.unpack_from('<I',blob,co+8)[0]
        for row in registrations(area,blob):
            data=bytearray(struct.pack('<HHh9h',*instances[row['object']]))
            # Caller has already resolved the collider pointer; reference index zero.
            struct.pack_into('<h',data,4,0)
            collider=blob[boxes+2+row['collider']*56:boxes+2+(row['collider']+1)*56]
            actual.append((area,data,collider))
    assert len(actual)==115
    init_cases=0
    for case in range(4230):
        if case<230:
            area,data,col=actual[case//2];data=bytearray(data);attack=case%2
        else:
            data=bytearray(rng.randbytes(24));struct.pack_into('<h',data,4,0)
            col=rng.randbytes(56);area=rng.choice([1,0x5B,0x5C,0x62,0x63,65535])
            attack=rng.randrange(2)
        old=rng.randbytes(96);slots=(Indicator*4).from_buffer_copy(old)
        count=C.c_uint32(case%4);trigger=Trigger.from_buffer_copy(data)
        collider=(C.c_uint8*56).from_buffer_copy(col)
        ref.ram[0x130000:0x130038]=col;ref.ram[0x13A000:0x13A018]=data
        ref.ram[0xBBD34:0xBBD94]=old;ref.write(0x800BBC50,4,count.value)
        ref.write(0x800F455C,4,0x80130000);ref.write(0x800F4476,2,area)
        ref.call(0x80091060,(0x8013A000,),hooks={0x80097678:lambda cpu,regs:attack})
        assert lib.ae_indicator_runtime_init(C.byref(trigger),collider,slots,C.byref(count),
                                            area,attack,C.byref(math))
        assert bytes(trigger)==ref.ram[0x13A000:0x13A018],('init trigger',case)
        assert bytes(collider)==ref.ram[0x130000:0x130038],('init collider',case)
        assert bytes(slots)==ref.ram[0xBBD34:0xBBD94],('init bank',case)
        assert count.value==ref.read(0x800BBC50,4),('init count',case)
        init_cases+=1
    calls_total=activation_calls=0
    for case in range(6000):
        data=bytearray(rng.randbytes(24));col=bytearray(rng.randbytes(56));old=bytearray(rng.randbytes(96))
        index=case%4;state=rng.choice([0,0,1,2,3,-1]);radius=rng.choice([0,1,512,32767,-1])
        struct.pack_into('<hh',data,2,state,0);struct.pack_into('<h',data,14,radius)
        struct.pack_into('<h',data,16,rng.choice([0,0,1,-1]));struct.pack_into('<h',data,20,index)
        center=[rng.randrange(-15000,15000) for _ in range(3)]
        struct.pack_into('<3h',col,0,*center);struct.pack_into('<h',col,30,rng.choice([-1,0,2]))
        player=[v+rng.choice([0,1,511,512,513,-1024]) for v in center]
        if case%20==0:player=[rng.randrange(-2147483648,2147483648) for _ in range(3)]
        struct.pack_into('<h',old,index*24,rng.choice([0,1,2,3,-1]))
        struct.pack_into('<h',old,index*24+10,rng.choice([-1,0,1,90]))
        struct.pack_into('<H',old,index*24+22,rng.choice([0,0,1,2]))
        area=rng.choice([1,0x5C,0x62,0x63]);exit_code=rng.choice([0,0,0,7,0x15,0x17]);busy=rng.randrange(2)
        trigger=Trigger.from_buffer_copy(data);collider=(C.c_uint8*56).from_buffer_copy(col)
        slots=(Indicator*4).from_buffer_copy(old);events=RuntimeEvents()
        ref.ram[0x130000:0x130038]=col;ref.ram[0x13A000:0x13A018]=data
        ref.ram[0xBBD34:0xBBD94]=old;ref.write(0x800F455C,4,0x80130000)
        ref.write(0x800F4476,2,area);ref.write(0x800F447C,1,exit_code)
        for i,v in enumerate(player):ref.write(0x800EC204+i*4,4,v)
        calls=[];activations=[]
        def trace(pc,regs):
            if pc==0x800717B4:activations.append((signed(regs[4]),signed(regs[5])))
        ref.trace=trace
        def record(name,argc=0,value=0):
            def hook(cpu,regs):
                calls.append((name,*[signed(regs[4+i]) for i in range(argc)]));return value
            return hook
        def pickup(cpu,regs):
            calls.append(('pickup',*[signed(cpu.read(regs[5]+i*2,2),16) for i in range(3)]));return 0
        hooks={0x8008DF3C:lambda cpu,regs:busy,0x8008DEEC:record('hint'),
            0x8008C09C:record('heap',1,0x80138000),0x80012C0C:record('member',2),
            0x80025E24:lambda cpu,regs:1000,0x80025D20:record('cd_sector',1),
            0x80012360:record('cd_command',1),0x8005D100:record('stage',3),
            0x8007B374:record('mode',1),0x80048430:record('aim'),0x80048748:record('fixed'),
            0x80083AA8:pickup,0x800877F8:record('sound',1),0x800698E4:record('effect',4)}
        ref.call(0x80091178,(0x8013A000,),hooks=hooks)
        assert lib.ae_indicator_runtime_step(C.byref(trigger),collider,slots,4,
            (C.c_int32*3)(*player),area,exit_code,busy,C.byref(events))
        assert bytes(trigger)==ref.ram[0x13A000:0x13A018],('step trigger',case)
        assert bytes(slots)==ref.ram[0xBBD34:0xBBD94],('step bank',case)
        assert bytes(collider)==col,('immutable collider',case)
        assert bool(events.trigger.activate)==bool(activations),('activate calls',case)
        if activations:assert activations==[(events.trigger.index,events.trigger.sector)]
        actual_calls=[];a=events.activation;t=events.trigger
        if a.hint:actual_calls.append(('hint',));assert a.sector==t.sector
        if a.pickups:assert a.pickups==3;actual_calls.extend([('pickup',*a.position)]*3)
        if a.sound:actual_calls.append(('sound',a.sound))
        if t.effect:actual_calls.append(('effect',0x112,*t.position))
        assert actual_calls==[x for x in calls if x[0] in ('hint','pickup','sound','effect')],('call order',case)
        calls_total+=len(calls);activation_calls+=len(activations)
    # Failure paths preserve outputs; exits must avoid unread dependencies.
    before=(bytes(trigger),bytes(collider),bytes(slots));previous=bytes(events)
    count=C.c_uint32(4)
    assert not lib.ae_indicator_runtime_init(C.byref(trigger),collider,slots,C.byref(count),1,0,C.byref(math))
    assert before==(bytes(trigger),bytes(collider),bytes(slots)) and count.value==4
    assert not lib.ae_indicator_runtime_step(C.byref(trigger),collider,slots,4,None,1,0,0,C.byref(events))
    assert bytes(events)==previous and before==(bytes(trigger),bytes(collider),bytes(slots))
    assert lib.ae_indicator_runtime_step(C.byref(trigger),collider,None,0,None,1,7,0,C.byref(events))
    assert bytes(events)==bytes(C.sizeof(events)) and bytes(trigger)==before[0]
    assert lib.ae_indicator_runtime_init(C.byref(trigger),collider,None,None,1,1,None)
    # A requested bad slot must roll back the trigger transition and events.
    trigger=Trigger();collider=(C.c_uint8*56)()
    struct.pack_into('<h',trigger.raw,14,512)
    struct.pack_into('<h',trigger.raw,20,-1)
    events=RuntimeEvents.from_buffer_copy(bytes([0xAA])*C.sizeof(RuntimeEvents))
    before=(bytes(trigger),bytes(slots),bytes(events))
    assert not lib.ae_indicator_runtime_step(C.byref(trigger),collider,slots,4,
        (C.c_int32*3)(0,0,0),1,0,0,C.byref(events))
    assert before==(bytes(trigger),bytes(slots),bytes(events))
    struct.pack_into('<h',trigger.raw,2,2)
    assert lib.ae_indicator_runtime_step(C.byref(trigger),collider,None,99,
        (C.c_int32*3)(0,0,0),1,0,0,C.byref(events))
    assert bytes(events)==bytes(C.sizeof(events))
    report=dict(actual_area_placements=115,original_init_cases=init_cases,original_composed_ticks=6000,
                original_activation_calls=activation_calls,ordered_engine_requests=calls_total,
                dependency_guard_checks=6,failures=0)
    (OUT/'indicator-runtime-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    (ROOT/'notes/port/validation/indicator_runtime_validation.md').write_text(
        '# Indicator runtime validation\n\nGenerated by check_indicator_runtime.py.\n\n'+
        '\n'.join(f'- {k}: {v}' for k,v in report.items())+'\n\n'+
        'Full original Init executes its real registration and SDK trig. Full original Update executes '
        'VecLength3/IntSqrt and Indicator_Activate; controlled external hint/CD/stage/player-mode/pickup/'
        'sound/effect endpoints only return or record requests. Complete trigger/collider/bank bytes '
        'and relevant ordered requests match. Runtime contacts, save progress and continuous console '
        'playback remain unproven.\n')
    print(report)

if __name__=='__main__':main()
