"""Complete original camera selection instructions versus portable control state."""
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
from mips_ground_reference import Reference, signed

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/native'


class Slot(C.Structure):
    _fields_=[('raw',C.c_uint8*0xB0)]


class Selection(C.Structure):
    _fields_=[('snapshot',C.c_uint32),('activate',C.c_uint32),('slot',C.c_int32)]


def main():
    dll=OUT/f'camera_select_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                    *[str(src(name+'.c')) for name in
                      ('native_camera_select','native_camera','native_camera_math')],
                    '-o',str(dll),'-static-libgcc'],check=True)
    lib=C.CDLL(str(dll));bp=C.POINTER(C.c_uint8)
    lib.ae_camera_select_mode.argtypes=[bp,C.POINTER(Slot),C.c_uint32,C.POINTER(C.c_int32),
                                      bp,bp,C.c_int,C.POINTER(Tables),C.POINTER(Selection)]
    lib.ae_camera_math_load.argtypes=[C.POINTER(Tables),C.c_char_p,C.c_size_t]
    lib.ae_camera_snapshot_eye.argtypes=[C.POINTER(Slot),C.POINTER(C.c_int32),bp,C.POINTER(Tables)]
    lib.ae_camera_activate_slot.argtypes=[bp,C.POINTER(Slot),C.c_uint32,C.c_uint32,C.c_uint32]
    tables=Tables();exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes()
    assert lib.ae_camera_math_load(C.byref(tables),exe,len(exe))
    ref=Reference((ROOT/'extract/members/05B.bin').read_bytes());rng=random.Random(46_94423)
    event_count=changed=normalized=0
    for case in range(12000):
        data=bytearray(rng.randbytes(0x50));slots=bytearray(rng.randbytes(12*0xB0))
        for offset in (0x18,0x19,0x1A,0x0E):data[offset]=rng.randrange(12)
        data[0x1B]=rng.choice([255,rng.randrange(12)])
        for offset in (0x42,0x38,0x32,0x46,0x41):data[offset]=rng.choice([0,0,0,1,2,255])
        if data[0x1B]==255:data[0x41]=0
        data[0x0F]=rng.choice([0,1,2,3,4,5,255])
        for i in range(12):
            slots[i*0xB0+0x8E]=rng.choice([0,1,2])
            slots[i*0xB0+0x33]=rng.choice([0,0,1,255])
            for offset in (0x0A,0x0E):struct.pack_into('<h',slots,i*0xB0+offset,rng.randrange(-8000,8000))
        if case>=10000:
            # Force the direction gate; broad random priorities rarely reach it.
            data[0x1B]=4;data[0x0E]=1;data[0x19]=2;data[0x0F]=rng.choice([0,1,4,5])
            for offset in (0x42,0x38,0x32,0x46,0x41):data[offset]=0
            slots[4*0xB0+0x8E]=1;slots[4*0xB0+0x33]=0
        player=[rng.randrange(-8000,8000) for _ in range(3)]
        # Full-word positions with the same low16 values exercise lhu semantics.
        if case%5==0:player=[signed((rng.randrange(65536)<<16)|(v&65535)) for v in player]
        eye=rng.randbytes(8);target=rng.randbytes(8);flight=rng.choice([0,0,1,-1])
        controls=(C.c_uint8*0x50).from_buffer_copy(data);native_slots=(Slot*12).from_buffer_copy(slots)
        events=Selection.from_buffer_copy(bytes([0xAA])*C.sizeof(Selection))
        ref.ram[0xE68D0:0xE6920]=data;ref.ram[0x130000:0x130000+len(slots)]=slots
        ref.write(0x800B4D40,4,0x80130000)
        for i,v in enumerate(player):ref.write(0x800EC204+i*4,4,v)
        ref.ram[0xF45A8:0xF45B0]=eye;ref.ram[0xF45B0:0xF45B8]=target
        calls=[]
        def snapshot(cpu,regs):calls.append(('snapshot',));return 0
        def activate(cpu,regs):calls.append(('activate',signed(regs[4])));return 0
        def trace(pc,regs):
            nonlocal normalized
            if pc==0x800301E0:normalized+=1
        ref.trace=trace
        ref.call(0x80046DE8,hooks={0x8004A940:lambda cpu,regs:flight,
                                0x80049514:snapshot,0x80046DAC:activate})
        assert lib.ae_camera_select_mode(controls,native_slots,12,(C.c_int32*3)(*player),
            (C.c_uint8*8).from_buffer_copy(eye),(C.c_uint8*8).from_buffer_copy(target),flight,
            C.byref(tables),C.byref(events)),case
        actual=[]
        if events.snapshot:actual.append(('snapshot',))
        if events.activate:actual.append(('activate',events.slot));changed+=1
        assert actual==calls and bytes(controls)==ref.ram[0xE68D0:0xE6920],(case,actual,calls)
        assert bytes(native_slots)==slots and ref.ram[0x130000:0x130000+len(slots)]==slots
        event_count+=len(calls)
        # Repeat with both real original service callees and SDK math executing.
        ref.trace=None
        ref.ram[0xE68D0:0xE6920]=data
        ref.ram[0x130000:0x130000+len(slots)]=slots
        ref.call(0x80046DE8,hooks={0x8004A940:lambda cpu,regs:flight})
        if events.snapshot:
            assert lib.ae_camera_snapshot_eye(C.byref(native_slots[data[0x18]]),
                (C.c_int32*3)(*player),(C.c_uint8*8).from_buffer_copy(eye),C.byref(tables)),case
        if events.activate:
            assert lib.ae_camera_activate_slot(controls,native_slots,12,events.slot,0x80130000)
        assert bytes(controls)==ref.ram[0xE68D0:0xE6920],case
        assert bytes(native_slots)==ref.ram[0x130000:0x130000+len(slots)],case
    ref.trace=None
    # Transactional validation failures; no output should be cleared on failure.
    before=bytes(controls);old=bytes(events);failures=0
    for args in ((None,native_slots,12),(controls,None,12),(controls,native_slots,0)):
        assert not lib.ae_camera_select_mode(*args,(C.c_int32*3)(0,0,0),
            (C.c_uint8*8)(),(C.c_uint8*8)(),0,C.byref(tables),C.byref(events))
        assert bytes(controls)==before and bytes(events)==old;failures+=1
    bad=(C.c_uint8*0x50)();bad[0x18]=12;bad[0x1B]=255
    old_controls=bytes(bad)
    assert not lib.ae_camera_select_mode(bad,native_slots,12,(C.c_int32*3)(),
        (C.c_uint8*8)(),(C.c_uint8*8)(),0,C.byref(tables),C.byref(events))
    assert bytes(bad)==old_controls and bytes(events)==old;failures+=1
    # Missing tables and an original normalization overflow fail closed.
    bad=(C.c_uint8*0x50)();bad[0x1B]=4;bad[0x0E]=1;bad[0x19]=2
    native_slots[4].raw[0x8E]=1;native_slots[4].raw[0x33]=0
    for offset in (0x0A,0x0E):
        native_slots[1].raw[offset]=native_slots[1].raw[offset+1]=0
    for table,position in ((None,(1,0,1)),(C.byref(tables),(32768,0,32768))):
        old_controls=bytes(bad)
        assert not lib.ae_camera_select_mode(bad,native_slots,12,(C.c_int32*3)(*position),
            (C.c_uint8*8)(),(C.c_uint8*8)(),0,table,C.byref(events))
        assert bytes(bad)==old_controls and bytes(events)==old;failures+=1
    service_failures=0
    follow=Slot.from_buffer_copy(rng.randbytes(0xB0));before=bytes(follow)
    for table,position in ((None,(0,0,0)),(C.byref(tables),(32768,0,32768))):
        assert not lib.ae_camera_snapshot_eye(C.byref(follow),(C.c_int32*3)(*position),
                                              (C.c_uint8*8)(),table)
        assert bytes(follow)==before;service_failures+=1
    for index,base in ((12,0x80130000),(1,0xFFFFFFF0)):
        before=bytes(controls)
        assert not lib.ae_camera_activate_slot(controls,native_slots,12,index,base)
        assert bytes(controls)==before;service_failures+=1
    report=dict(original_selector_cases=12000,changed_selections=changed,
                original_normalize_calls=normalized,ordered_service_requests=event_count,
                invalid_argument_checks=failures,complete_selector_service_cases=12000,
                service_failure_checks=service_failures,failures=0)
    (OUT/'camera-select-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    (ROOT/'notes/port/validation/camera_select_validation.md').write_text(
        '# Camera selection validation\n\nGenerated by `check_camera_select.py`.\n\n'+
        '\n'.join(f'- {k}: {v}' for k,v in report.items())+'\n\n'+
        'Complete original CameraSelectMode executes original VectorNormalSS where needed. '
        'Flight status is supplied; snapshot and activation callees are intercepted. '
        'All 80 control bytes and ordered requests match; slot bytes remain unchanged. '
        'A second pass executes original snapshot/activation callees and compares all controls and 12 slots '
        'with the portable selector plus service functions. Address tags remain opaque in native code. '
        'Region lifecycle, scripts and blending remain separate.\n')
    print(report)


if __name__=='__main__':main()
