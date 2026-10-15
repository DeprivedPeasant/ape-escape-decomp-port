"""Actual area runtime packs and shared indicator-bank original-code comparisons."""
import ctypes as C
import json
import os
import shutil
import struct
import subprocess
from pathlib import Path
from src_paths import src
from pack_indicators import pack, area_members
from check_indicator_triggers import Trigger
from check_native_indicators import Indicator
from check_indicator_runtime import RuntimeEvents
from mips_ground_reference import Reference, signed

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/native'

def main():
    names=('native_indicator_world','native_indicator_runtime','native_indicator_trigger',
           'native_indicators','native_camera_math','native_actor','native_collision','native_ground')
    dll=OUT/f'indicator_proximity_world_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
        *[str(src(n+'.c')) for n in names],'-o',str(dll),
        '-lopengl32','-lm','-static-libgcc'],check=True)
    lib=C.CDLL(str(dll));bp=C.POINTER(C.c_uint8);wp=C.c_void_p
    lib.ae_indicator_world_load.argtypes=[C.POINTER(wp),C.c_char_p,C.c_char_p,C.c_size_t,C.c_uint32]
    lib.ae_indicator_world_free.argtypes=[wp]
    lib.ae_indicator_world_has_runtime.argtypes=[wp]
    lib.ae_indicator_world_runtime_record.argtypes=[wp,C.c_uint32,C.POINTER(Trigger),bp,C.POINTER(Indicator)]
    lib.ae_indicator_world_proximity_step.argtypes=[wp,C.POINTER(C.c_int32),C.c_uint32,C.c_int,
        C.POINTER(C.c_int16),C.c_uint32,C.POINTER(RuntimeEvents),C.POINTER(C.c_uint32)]
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes()
    ref=Reference((ROOT/'extract/members/05B.bin').read_bytes())
    areas=records=ticks=requests=missing_cases=0
    def snapshot(w,n):
        result=[]
        for i in range(n):
            t=Trigger();c=(C.c_uint8*56)();s=Indicator()
            assert lib.ae_indicator_world_runtime_record(w,i,C.byref(t),c,C.byref(s))
            result.append((bytes(t),bytes(c),bytes(s)))
        return result
    for area,member in sorted(area_members().items()):
        prepared=pack(area,runtime=True);data=Path(prepared['pack']).read_bytes()
        assert data[:8]==b'AEIND2\0\0' and len(data)==16+164*prepared['count']
        w=wp();assert lib.ae_indicator_world_load(C.byref(w),prepared['pack'].encode(),exe,len(exe),area)
        assert lib.ae_indicator_world_has_runtime(w)
        n=prepared['count'];blob=(ROOT/f'extract/members/{member:03X}.bin').read_bytes()
        ah=struct.unpack_from('<I',blob,0x4D0)[0];co=ah+struct.unpack_from('<I',blob,ah+16)[0]*4
        boxes=co+struct.unpack_from('<I',blob,co+8)[0];nc=struct.unpack_from('<H',blob,boxes)[0]
        ref.ram[0x130000:0x130000+nc*56]=blob[boxes+2:boxes+2+nc*56]
        ref.ram[0xBBD34:0xBBD94]=bytes(96);ref.write(0x800BBC50,4,0)
        ref.write(0x800F455C,4,0x80130000);ref.write(0x800F4476,2,area)
        for i,row in enumerate(prepared['records']):
            t=data[16+i*164+84:16+i*164+108]
            ref.ram[0x13A000+i*24:0x13A018+i*24]=t
            ref.call(0x80091060,(0x8013A000+i*24,),hooks={0x80097678:lambda cpu,regs:0})
        def compare():
            result=snapshot(w,n)
            for i,(t,c,s) in enumerate(result):
                ci=struct.unpack_from('<h',t,4)[0]
                assert t==ref.ram[0x13A000+i*24:0x13A018+i*24],('trigger',area,i)
                assert c==ref.ram[0x130000+ci*56:0x130038+ci*56],('collider',area,i)
                assert s==ref.ram[0xBBD34+i*24:0xBBD4C+i*24],('slot',area,i)
            return result
        current=compare();records+=n;areas+=1
        for tick in range(16):
            row=tick%n if n else 0
            center=list(struct.unpack_from('<3h',current[row][1])) if n else [0,0,0]
            radius=struct.unpack_from('<h',current[row][0],14)[0] if n else 0
            player=list(center);player[0]+= [0,radius,max(1,radius+1),0][tick%4]
            exit_code=7 if tick==15 else 0;busy=tick%3==0
            known=((1<<n)-1) if tick%2 else 0;touched=(C.c_int16*4)(0,0,0,0)
            expected_missing=0
            for i,(t,c,s) in enumerate(current):
                ci=struct.unpack_from('<h',t,4)[0]
                # Unknown live contacts must be explicitly absent, never disk values.
                touch=0 if known&(1<<i) else -1
                if exit_code==0:ref.write(0x80130000+ci*56+30,2,touch)
                state=struct.unpack_from('<h',t,2)[0];radius_i=struct.unpack_from('<h',t,14)[0]
                ctr=struct.unpack_from('<3h',c)
                distance=signed(ref.call(0x80046618,tuple(player[j]-ctr[j] for j in range(3)))[2])
                if not exit_code and not known&(1<<i) and ((state==1 and distance<radius_i) or (state==3 and distance<=radius_i)):
                    expected_missing|=1<<i
            calls=[]
            def record(name,argc=0,value=0):
                def hook(cpu,regs):calls.append((name,*[signed(regs[4+i]) for i in range(argc)]));return value
                return hook
            def pickup(cpu,regs):
                calls.append(('pickup',*[signed(cpu.read(regs[5]+j*2,2),16) for j in range(3)]));return 0
            noop=lambda cpu,regs:0
            hooks={0x8008DF3C:lambda cpu,regs:int(busy),0x8008DEEC:record('hint'),
                0x8008C09C:lambda cpu,regs:0x80138000,0x80025E24:lambda cpu,regs:1000,
                0x80083AA8:pickup,0x800877F8:record('sound',1),0x800698E4:record('effect',4)}
            hooks.update({a:noop for a in (0x80012C0C,0x80025D20,0x80012360,0x8005D100,0x8007B374,0x80048430,0x80048748)})
            for j,v in enumerate(player):ref.write(0x800EC204+j*4,4,v)
            ref.write(0x800F447C,1,exit_code)
            for i in range(n):ref.call(0x80091178,(0x8013A000+i*24,),hooks=hooks)
            events=(RuntimeEvents*4)();missing=C.c_uint32(0xAAAAAAAA)
            assert lib.ae_indicator_world_proximity_step(w,(C.c_int32*3)(*player),exit_code,
                busy,touched,known,events,C.byref(missing))
            assert missing.value==expected_missing,('missing mask',area,tick,missing.value,expected_missing)
            actual=[]
            for event in events:
                a=event.activation;t=event.trigger
                if a.hint:actual.append(('hint',))
                if a.pickups:actual.extend([('pickup',*a.position)]*a.pickups)
                if a.sound:actual.append(('sound',a.sound))
                if t.effect:actual.append(('effect',0x112,*t.position))
            assert actual==calls,('requests',area,tick,actual,calls)
            current=compare();ticks+=n;requests+=len(calls);missing_cases+=bool(missing.value)
        lib.ae_indicator_world_free(w)
    # All prepared duplicate fields must agree with the real source records.
    prepared=pack(1,runtime=True);original=Path(prepared['pack']).read_bytes();bad=OUT/'indicator_runtime_bad.aei'
    corruptions=[(16+84+10,'<h',2),(16+84+12,'<h',123),(16+84+18,'<h',12),(16+108,'<h',123)]
    for off,fmt,value in corruptions:
        data=bytearray(original);struct.pack_into(fmt,data,off,struct.unpack_from(fmt,data,off)[0]^1);bad.write_bytes(data)
        w=wp();assert not lib.ae_indicator_world_load(C.byref(w),str(bad).encode(),exe,len(exe),1) and not w
    w=wp();assert lib.ae_indicator_world_load(C.byref(w),prepared['pack'].encode(),exe,len(exe),1)
    before=snapshot(w,prepared['count'])
    events=(RuntimeEvents*4).from_buffer_copy(bytes([0xAA])*(C.sizeof(RuntimeEvents)*4))
    old=bytes(events);missing=C.c_uint32(0xAAAAAAAA)
    for player,mask in ((None,0),((C.c_int32*3)(0,0,0),1<<prepared['count']),
                        ((C.c_int32*3)(0,0,0),1)):
        assert not lib.ae_indicator_world_proximity_step(w,player,0,0,None,mask,events,C.byref(missing))
        assert snapshot(w,prepared['count'])==before and bytes(events)==old and missing.value==0xAAAAAAAA
    assert lib.ae_indicator_world_proximity_step(w,None,0x107,0,None,0xFFFFFFFF,events,C.byref(missing))
    assert bytes(events)==bytes(C.sizeof(events)) and not missing.value and snapshot(w,prepared['count'])==before
    lib.ae_indicator_world_free(w)
    legacy=pack(1);w=wp()
    assert lib.ae_indicator_world_load(C.byref(w),legacy['pack'].encode(),exe,len(exe),1)
    assert not lib.ae_indicator_world_has_runtime(w)
    assert not lib.ae_indicator_world_proximity_step(w,None,7,0,None,0,events,C.byref(missing))
    lib.ae_indicator_world_free(w)
    report=dict(areas=areas,actual_trigger_records=records,original_shared_bank_ticks=ticks,
                ordered_requests=requests,missing_touch_batches=missing_cases,metadata_failure_checks=len(corruptions),
                dependency_and_legacy_checks=5,failures=0)
    (OUT/'indicator-proximity-world-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    (ROOT/'notes/port/validation/indicator_proximity_world_validation.md').write_text(
        '# Proximity world validation\n\nGenerated by check_indicator_proximity_world.py.\n\n'+
        '\n'.join(f'- {k}: {v}' for k,v in report.items())+'\n\n'+
        'Actual source triggers/colliders and shared slots match original initialization and update '
        'instructions. Player positions, touch inputs and fixed per-batch message busy are controlled. '
        'Missing contacts are explicitly reported. Live collider displacement, hint/player-mode/progress '
        'protocol and console gameplay remain outside this check.\n')
    print(report)

if __name__=='__main__':main()
