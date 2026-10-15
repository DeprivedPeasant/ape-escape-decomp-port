"""Whole CameraSlotUpdate instructions compared with the portable motion adapter."""
import argparse
import ctypes as C
import json
import os
import random
import shutil
import struct
import subprocess
from pathlib import Path
from check_camera_math import Tables
from mips_ground_reference import Reference, signed

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/native'
class Slot(C.Structure):
    _fields_=[('raw',C.c_uint8*0xB0)]
BP=C.POINTER(C.c_uint8)
FOCUS=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Slot),C.c_uint8,BP)
INDEXED=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Slot),C.c_uint8)
class Queries(C.Structure):
    _fields_=[('user',C.c_void_p),('focus',FOCUS),('indexed',INDEXED)]

def main(source_dir):
    OUT.mkdir(parents=True,exist_ok=True)
    dll=OUT/f'camera_motion_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',
                    '-shared',str(source_dir/'native_camera_motion.c'),
                    str(ROOT/'src/camera/native_camera.c'),str(ROOT/'src/camera/native_camera_math.c'),
                    '-o',str(dll),'-static-libgcc'],check=True)
    lib=C.CDLL(str(dll))
    lib.ae_camera_slot_update.argtypes=[C.POINTER(Slot),C.POINTER(C.c_int32),C.c_uint8,
                                       C.POINTER(Tables),C.POINTER(Queries)]
    lib.ae_camera_math_load.argtypes=[C.POINTER(Tables),C.c_char_p,C.c_size_t]
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes();tables=Tables()
    assert lib.ae_camera_math_load(C.byref(tables),exe,len(exe))
    ref=Reference((ROOT/'extract/members/05B.bin').read_bytes())
    rng=random.Random(494423);counts={};calls=normalizations=0
    for case in range(12000):
        raw=bytearray(rng.randbytes(0xB0));state=rng.choice([0,1,2,3,4,5,6,7,8,9,10,255])
        raw[0x90]=state;raw[3]=rng.choice([0,0,0,0,1,17,255])
        timer=rng.choice([0,0,1,2,32767,-1,-32768,rng.randrange(-32768,32768)])
        struct.pack_into('<h',raw,0x88,timer)
        # Keep SDK normalization in its original nonoverflow domain.
        for off in (0x0A,0x0C,0x0E):struct.pack_into('<h',raw,off,rng.randrange(-8000,8001))
        player=[rng.randrange(-8000,8001) for _ in range(3)]
        if case%3==0:player=[signed((rng.randrange(65536)<<16)|(v&65535)) for v in player]
        stereo=rng.choice([0,0,1,255]);focus_bytes=rng.randbytes(8)
        native_calls=[];original_calls=[]
        @FOCUS
        def focus(user,slot,mode,out):
            native_calls.append(('focus',mode,bytes(slot.contents)))
            C.memmove(out,focus_bytes,8);return 1
        @INDEXED
        def indexed(user,slot,index):
            native_calls.append(('indexed',index,bytes(slot.contents)))
            slot.contents.raw[0x33]=index
            slot.contents.raw[0x80]^=0x55
            return 1
        queries=Queries(None,focus,indexed);slot=Slot.from_buffer_copy(raw)
        assert lib.ae_camera_slot_update(C.byref(slot),(C.c_int32*3)(*player),stereo,
                                        C.byref(tables),C.byref(queries)),case
        ref.ram[0x130000:0x1300B0]=raw
        for axis,v in enumerate(player):ref.write(0x800EC204+axis*4,4,v)
        ref.write(0x800AF131,1,stereo)
        def original_focus(cpu,regs):
            p=regs[4]&0x1FFFFF
            original_calls.append(('focus',regs[5]&255,bytes(cpu.ram[p:p+0xB0])))
            dest=regs[6]&0x1FFFFF;cpu.ram[dest:dest+8]=focus_bytes;return 0
        def original_indexed(cpu,regs):
            p=regs[4]&0x1FFFFF;index=regs[5]&255
            original_calls.append(('indexed',index,bytes(cpu.ram[p:p+0xB0])))
            cpu.ram[p+0x33]=index;cpu.ram[p+0x80]^=0x55;return 0
        def trace(pc,regs):
            nonlocal normalizations
            if pc==0x800301E0:normalizations+=1
        ref.trace=trace
        ref.call(0x8004CA70,(0x80130000,),hooks={0x8004B8F4:original_focus,0x8004D880:original_indexed})
        expected=bytes(ref.ram[0x130000:0x1300B0])
        assert bytes(slot)==expected,(case,state,timer,[(hex(i),a,b) for i,(a,b) in enumerate(zip(bytes(slot),expected)) if a!=b])
        assert native_calls==original_calls,(case,native_calls,original_calls)
        counts[str(state)]=counts.get(str(state),0)+1;calls+=len(native_calls)
    # Fail closed on reached dependencies; timer-zero does not require them.
    raw=bytearray(0xB0);raw[0x90]=8;raw[0x33]=77
    struct.pack_into('<h',raw,0x88,1);slot=Slot.from_buffer_copy(raw)
    assert not lib.ae_camera_slot_update(C.byref(slot),(C.c_int32*3)(),0,None,None)
    assert bytes(slot)==raw
    struct.pack_into('<h',raw,0x88,0);slot=Slot.from_buffer_copy(raw)
    assert lib.ae_camera_slot_update(C.byref(slot),None,0,None,None)
    raw[0x33]=0;assert bytes(slot)==raw
    assert not lib.ae_camera_slot_update(None,None,0,None,None)
    report=dict(original_motion_cases=12000,states=counts,ordered_callbacks=calls,
                real_sdk_normalizations=normalizations,guard_checks=3,failures=0)
    (OUT/'camera-motion-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    (ROOT/'notes/port/validation/camera_motion_validation.md').write_text(
        '# Camera motion validation\n\nGenerated by check_camera_motion.py.\n\n'+
        '\n'.join(f'- {key}: {value}' for key,value in report.items())+'\n\n'+
        'Complete CameraSlotUpdate instructions and real SDK trig/normalization execute. '
        'All176slot bytes and ordered callback inputs match. Focus/overlay dispatch callbacks '
        'are controlled test services, not reconstructed dependencies. No gameplay capture claim.\n')
    print(report)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-dir',type=Path,default=ROOT/'src/camera')
    main(p.parse_args().source_dir)
