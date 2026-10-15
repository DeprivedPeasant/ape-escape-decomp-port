"""Complete original region lifecycle and callees versus native state."""
import argparse
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
from check_camera_select import Slot
from mips_ground_reference import Reference, signed

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/native'
BP=C.POINTER(C.c_uint8)
class Frame(C.Structure):
    _fields_=[('controls',C.c_uint8*80),('focus',C.c_uint8*16),('slots',Slot*12)]

def main(source_dir):
    dll=OUT/f'camera_lifecycle_{os.getpid()}.dll'
    names=['native_camera','native_camera_bank','native_camera_select','native_camera_math','native_camera_regions']
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                    str(source_dir/'native_camera_lifecycle.c'),
                    *[str(src(name+'.c')) for name in names],
                    '-o',str(dll),'-static-libgcc'],check=True)
    lib=C.CDLL(str(dll))
    lib.ae_camera_region_update.argtypes=[C.POINTER(Frame),BP,C.c_size_t,C.c_uint16,
        C.POINTER(C.c_int32),C.POINTER(C.c_int16),BP,BP,C.POINTER(Tables),BP,C.c_size_t,
        C.c_uint32,C.c_int,C.c_uint32]
    lib.ae_camera_math_load.argtypes=[C.POINTER(Tables),C.c_char_p,C.c_size_t]
    tables=Tables();exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes()
    assert lib.ae_camera_math_load(C.byref(tables),exe,len(exe))
    ref=Reference((ROOT/'extract/members/05B.bin').read_bytes());rng=random.Random(52_94423)
    table=bytearray(8+40*8+16);struct.pack_into('<H',table,0,8)
    for index in range(8):struct.pack_into('<H',table,8+40*index+2,328)
    struct.pack_into('<8h',table,328,20,100,1,2,3,4,5,6)
    native_table=(C.c_uint8*len(table)).from_buffer_copy(table)
    ref.ram[0x131400:0x131400+len(table)]=table
    counts={};service_counts={};ticks=0
    for case in range(8000):
        controls=bytearray(rng.randbytes(80));focus=rng.randbytes(16)
        bank=bytearray(rng.randbytes(12*0xB0));records=bytearray(rng.randbytes(4*48))
        prev=rng.choice([255,0,1,2,3]);desired=rng.choice([255,0,1,2,3])
        for index in range(12):
            bank[index*0xB0]=[1,2,3][index] if index<3 else rng.choice([0,0,4,5])
            bank[index*0xB0+2]=index;bank[index*0xB0+0x93]=255
            for offset in (0x0A,0x0C,0x0E):struct.pack_into('<h',bank,index*0xB0+offset,rng.randrange(-5000,5001))
        for index in range(4):
            mode=rng.choice([0,0x2000,0x4000,0x6000])
            linked=rng.choice([0,index+3]);flags=mode|linked|rng.choice([0,0x8000])
            struct.pack_into('<8h',records,index*48,index, signed(flags,16),index*3000,0,0,2000,2000,2000)
            if linked:bank[linked*0xB0+0x93]=index
        controls[0x18]=0;controls[0x19]=1;controls[0x1A]=2
        controls[0x1B]=255 if prev==255 else prev+3
        controls[0x0D]=prev;controls[0x0C]=rng.choice([0,1,2,3,4,255])
        controls[0x0E]=rng.randrange(12);controls[0x0F]=rng.choice([0,1,4,5])
        for offset in (0x38,0x32,0x46,0x41,0x42):controls[offset]=rng.choice([0,0,1])
        controls[0x41]=0 # lifecycle may remove selected region before the selector.
        struct.pack_into('<H',controls,0,4);struct.pack_into('<H',controls,2,sum(bank[i*0xB0]!=0 for i in range(12)))
        struct.pack_into('<H',controls,4,rng.randrange(12));struct.pack_into('<I',controls,0x4C,0x80131000)
        # Points either inside one region, or outside all four.
        player=[15000 if desired==255 else desired*3000,128,0]
        if case%4==0:player=[signed((rng.randrange(65536)<<16)|(v&65535)) for v in player]
        facing=[rng.randrange(-4096,4097) for _ in range(3)]
        eye=struct.pack('<4h',rng.randrange(-5000,5001),rng.randrange(-5000,5001),rng.randrange(-5000,5001),1234)
        target=rng.randbytes(8);flight=rng.choice([0,0,1])
        frame=Frame.from_buffer_copy(bytes(controls)+focus+bytes(bank))
        regions=(C.c_uint8*len(records)).from_buffer_copy(records)
        eyes=[(C.c_uint8*8).from_buffer_copy(v) for v in (eye,target)]
        assert lib.ae_camera_region_update(C.byref(frame),regions,len(records),4,(C.c_int32*3)(*player),
            (C.c_int16*3)(*facing),*eyes,C.byref(tables),native_table,len(table),0x80131400,flight,0x80130000),case
        ref.ram[0xE68C0:0xE68D0]=focus;ref.ram[0xE68D0:0xE6920]=controls
        ref.ram[0x130000:0x130840]=bank;ref.ram[0x131000:0x131000+len(records)]=records
        ref.write(0x800B4D40,4,0x80130000);ref.write(0x800E6924,4,0x80131400)
        ref.write(0x800E692C,4,0x80131408)
        for i,v in enumerate(player):ref.write(0x800EC204+i*4,4,v)
        for i,v in enumerate(facing):ref.write(0x800EC240+i*2,2,v)
        ref.ram[0xF45A8:0xF45B0]=eye;ref.ram[0xF45B0:0xF45B8]=target
        history=[]
        def trace(pc,regs):
            history.append((hex(pc),hex(regs[29]),hex(regs[31])))
            if len(history)>32:history.pop(0)
            if pc in (0x800471E4,0x80047290,0x8004BAD8,0x80046DAC,0x80046DE8,0x80049514):
                key=f'{pc:08X}';service_counts[key]=service_counts.get(key,0)+1
        ref.trace=trace
        def memset(cpu,regs):
            for i in range(regs[6]):cpu.write(regs[4]+i,1,regs[5])
            return regs[4]
        try:ref.call(0x8004BF20,hooks={0x8004A940:lambda cpu,regs:flight,0x800A3740:memset})
        except Exception as error:raise AssertionError((case,prev,desired,history)) from error
        ref.trace=None
        assert bytes(frame.controls)==ref.ram[0xE68D0:0xE6920],('controls',case,prev,desired,
            [(hex(i),a,b) for i,(a,b) in enumerate(zip(bytes(frame.controls),ref.ram[0xE68D0:0xE6920])) if a!=b])
        assert bytes(frame.focus)==ref.ram[0xE68C0:0xE68D0],('focus',case)
        assert bytes(frame.slots)==ref.ram[0x130000:0x130840],('slots',case)
        assert bytes(regions)==ref.ram[0x131000:0x131000+len(records)],('regions',case)
        counts[str(frame.controls[0x0D])]=counts.get(str(frame.controls[0x0D]),0)+1;ticks+=1
    # Rejected old-region mappings and missing frame are transactional.
    frame=Frame();frame.controls[0x0D]=4;regions=(C.c_uint8*48)()
    before=bytes(frame);before_regions=bytes(regions)
    assert not lib.ae_camera_region_update(C.byref(frame),regions,48,1,(C.c_int32*3)(15000,128,0),
        None,None,None,None,None,0,0,0,0)
    assert bytes(frame)==before and bytes(regions)==before_regions
    assert not lib.ae_camera_region_update(None,None,0,0,None,None,None,None,None,None,0,0,0,0)
    report=dict(original_lifecycle_cases=ticks,final_regions=counts,original_service_calls=service_counts,
                guard_checks=2,failures=0)
    (OUT/'camera-lifecycle-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    (ROOT/'notes/port/validation/camera_lifecycle_validation.md').write_text(
        '# Camera region lifecycle validation\n\nGenerated by check_camera_lifecycle.py.\n\n'+
        '\n'.join(f'- {k}: {v}' for k,v in report.items())+'\n\n'+
        'Complete CameraRegionUpdate executes including actual region query, slot create/remove, '
        'script attachment, mode selection, SDK snapshot yaw and activation services. Only '
        'IsFlightArea and the BIOS memset trampoline are supplied endpoints. '
        'All12slots/80controls/16focus/regionbytes compare. '
        'This is synthetic lifecycle evidence, not a console gameplay capture.\n')
    print(report)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-dir',type=Path,default=ROOT/'src/camera')
    main(p.parse_args().source_dir)
