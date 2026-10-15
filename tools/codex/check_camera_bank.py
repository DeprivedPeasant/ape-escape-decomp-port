"""Complete original camera bank services, including real SDK initialization."""
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
from check_camera_select import Slot
from mips_ground_reference import Reference, signed

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/native'
BP=C.POINTER(C.c_uint8)

def main(source_dir):
    dll=OUT/f'camera_bank_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                    str(source_dir/'native_camera_bank.c'),str(ROOT/'src/camera/native_camera.c'),
                    str(ROOT/'src/camera/native_camera_math.c'),
                    '-o',str(dll),'-static-libgcc'],check=True)
    lib=C.CDLL(str(dll))
    lib.ae_camera_slot_create.argtypes=[C.POINTER(Slot),BP,C.c_int32,BP,BP,BP,
        C.POINTER(C.c_int32),C.POINTER(C.c_int16),C.POINTER(Tables),C.POINTER(C.c_uint32)]
    lib.ae_camera_slot_remove.argtypes=[C.POINTER(Slot),BP,BP,C.c_size_t,C.c_uint16,C.c_uint32]
    lib.ae_camera_set_script.argtypes=[C.POINTER(Slot),BP,C.c_size_t,C.c_uint32,C.c_uint32,C.c_uint8]
    lib.ae_camera_math_load.argtypes=[C.POINTER(Tables),C.c_char_p,C.c_size_t]
    tables=Tables();exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes()
    assert lib.ae_camera_math_load(C.byref(tables),exe,len(exe))
    ref=Reference((ROOT/'extract/members/05B.bin').read_bytes());rng=random.Random(51_94423)
    full=created=0
    for case in range(6000):
        raw=bytearray(rng.randbytes(12*0xB0));control=rng.randbytes(80)
        for index in range(12):raw[index*0xB0]=rng.choice([0,1,2,3,4,5,255])
        if case%5==0:
            for index in range(12):raw[index*0xB0]=1
            full+=1
        else:created+=1
        kind=rng.choice([-2147483648,-1,0,1,2,3,4,5,6,255,256,257,2147483647])
        initial=rng.randbytes(8);eye=rng.randbytes(8);target=rng.randbytes(8)
        player=[signed(rng.getrandbits(32)) for _ in range(3)]
        facing=[rng.randrange(-32768,32768) for _ in range(3)]
        slots=(Slot*12).from_buffer_copy(raw);controls=(C.c_uint8*80).from_buffer_copy(control)
        vecs=[(C.c_uint8*8).from_buffer_copy(v) for v in (initial,eye,target)];output=C.c_uint32(1234567)
        assert lib.ae_camera_slot_create(slots,controls,kind,*vecs,(C.c_int32*3)(*player),
                                        (C.c_int16*3)(*facing),C.byref(tables),C.byref(output))
        ref.ram[0x130000:0x130840]=raw;ref.ram[0xE68D0:0xE6920]=control
        ref.write(0x800B4D40,4,0x80130000);ref.ram[0x131000:0x131008]=initial
        ref.ram[0xF45A8:0xF45B0]=eye;ref.ram[0xF45B0:0xF45B8]=target
        for i,v in enumerate(player):ref.write(0x800EC204+i*4,4,v)
        for i,v in enumerate(facing):ref.write(0x800EC240+i*2,2,v)
        expected_index=ref.call(0x800471E4,(kind,0x80131000))[2]
        expected=bytes(ref.ram[0x130000:0x130840])
        assert bytes(slots)==expected,(case,kind,[(hex(i),a,b) for i,(a,b) in enumerate(zip(bytes(slots),expected)) if a!=b])
        assert bytes(controls)==ref.ram[0xE68D0:0xE6920] and output.value==expected_index,case
    for case in range(6000):
        raw=bytearray(rng.randbytes(12*0xB0));control=bytearray(rng.randbytes(80));region=rng.randbytes(16*48)
        index=rng.randrange(12);raw[index*0xB0]=rng.choice([0,1,3,4,5,6,255])
        raw[index*0xB0+0x93]=rng.choice([255,rng.randrange(16)])
        control[0x1B]=rng.choice([255,index,rng.randrange(12)])
        struct.pack_into('<H',control,4,rng.choice([index,rng.randrange(12),65535]))
        struct.pack_into('<I',control,0x4C,0x80131000)
        slots=(Slot*12).from_buffer_copy(raw);controls=(C.c_uint8*80).from_buffer_copy(control)
        regions=(C.c_uint8*len(region)).from_buffer_copy(region)
        assert lib.ae_camera_slot_remove(slots,controls,regions,len(region),16,index)
        ref.ram[0x130000:0x130840]=raw;ref.ram[0xE68D0:0xE6920]=control
        ref.ram[0x131000:0x131000+len(region)]=region;ref.write(0x800B4D40,4,0x80130000)
        ref.call(0x80047290,(index,))
        assert bytes(slots)==ref.ram[0x130000:0x130840],('remove',case)
        assert bytes(controls)==ref.ram[0xE68D0:0xE6920],('remove controls',case)
        assert bytes(regions)==ref.ram[0x131000:0x131000+len(region)],('remove regions',case)
    inventory=json.loads((OUT/'camera-scripts.json').read_text());routes=0
    for script in inventory['scripts']:
        blob=(ROOT/f'extract/members/{script["member"]:03X}.bin').read_bytes()
        cam=struct.unpack_from('<I',blob,0x4D8)[0];table=cam+struct.unpack_from('<H',blob,cam+14)[0]
        window=blob[table:];base=0x80154000+table;flag=rng.randrange(256)
        raw=rng.randbytes(0xB0);slot=Slot.from_buffer_copy(raw)
        data=(C.c_uint8*len(window)).from_buffer_copy(window)
        assert lib.ae_camera_set_script(C.byref(slot),data,len(window),base,script['index'],flag)
        original=Reference(blob);original.ram[0x130000:0x1300B0]=raw
        original.write(0x800B4D40,4,0x80130000);original.write(0x800E6924,4,base)
        original.write(0x800E692C,4,base+8)
        original.call(0x8004BAD8,(0,script['index'],flag,1))
        assert bytes(slot)==original.ram[0x130000:0x1300B0],('route',script['area'],script['index'])
        routes+=1
    # Original script entries are halfwords, with no four-byte alignment rule.
    window=bytearray(52);struct.pack_into('<H',window,0,1)
    struct.pack_into('<H',window,10,50);struct.pack_into('<H',window,50,2)
    base=0x80131002;raw=rng.randbytes(0xB0);slot=Slot.from_buffer_copy(raw)
    data=(C.c_uint8*len(window)).from_buffer_copy(window)
    assert lib.ae_camera_set_script(C.byref(slot),data,len(window),base,0,1)
    ref.ram[0x131002:0x131002+len(window)]=window;ref.ram[0x130000:0x1300B0]=raw
    ref.write(0x800B4D40,4,0x80130000);ref.write(0x800E6924,4,base)
    ref.write(0x800E692C,4,base+8);ref.call(0x8004BAD8,(0,0,1,1))
    assert bytes(slot)==ref.ram[0x130000:0x1300B0]
    # Kind4 never reads player/facing/math in InitFollow.
    slots=(Slot*12)();controls=(C.c_uint8*80)();output=C.c_uint32(1234567)
    vec=(C.c_uint8*8)()
    assert lib.ae_camera_slot_create(slots,controls,4,vec,vec,vec,None,None,None,C.byref(output))
    assert output.value==0 and slots[0].raw[0]==4
    # Invalid indices/region mappings/table offsets leave all state untouched.
    raw=bytearray(12*0xB0);raw[0x93]=1
    slots=(Slot*12).from_buffer_copy(raw);controls=(C.c_uint8*80)();regions=(C.c_uint8*48)()
    before=(bytes(slots),bytes(controls),bytes(regions))
    for index in (12,0):
        assert not lib.ae_camera_slot_remove(slots,controls,regions,48,1,index)
        assert (bytes(slots),bytes(controls),bytes(regions))==before
    output=C.c_uint32(1234567)
    assert not lib.ae_camera_slot_create(slots,controls,1,None,None,None,None,None,None,C.byref(output))
    assert (bytes(slots),bytes(controls),bytes(regions))==before and output.value==1234567
    slot=Slot.from_buffer_copy(rng.randbytes(0xB0));before_slot=bytes(slot)
    for table,base,index in [(b'\0'*8,0x80131000,0),(struct.pack('<H',1)+bytes(46),0xFFFFFFFF,0),
                             (struct.pack('<H',1)+bytes(46),0x80131000,1)]:
        data=(C.c_uint8*len(table)).from_buffer_copy(table)
        assert not lib.ae_camera_set_script(C.byref(slot),data,len(table),base,index,1)
        assert bytes(slot)==before_slot
    report=dict(original_create_cases=6000,full_bank_cases=full,original_remove_cases=6000,
                actual_table_routes=routes,halfword_alignment_cases=1,unreached_dependency_cases=1,
                guard_checks=6,failures=0)
    (OUT/'camera-bank-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    (ROOT/'notes/port/validation/camera_bank_validation.md').write_text(
        '# Camera bank validation\n\nGenerated by check_camera_bank.py.\n\n'+
        '\n'.join(f'- {k}: {v}' for k,v in report.items())+'\n\n'+
        'Complete original Create/Remove/SetScript run, including InitFollow/ResetFollow and '
        'actual SDK yaw math. All12slots/80controls/region flag bytes compare. Actual252area '
        'table routes compare complete slot bytes. No runtime area lifecycle or capture claim.\n')
    print(report)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-dir',type=Path,default=ROOT/'src/camera')
    main(p.parse_args().source_dir)
