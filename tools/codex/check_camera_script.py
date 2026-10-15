"""Local camera VM comparisons against every original area script."""
import argparse
import ctypes as C
import json
import os
import random
import shutil
import struct
import subprocess
from pathlib import Path
from check_camera_select import Slot
from mips_ground_reference import Reference

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/native'
class Result(C.Structure):
    _fields_=[('remove_slot',C.c_uint32)]

def main(source_dir):
    subprocess.run(['uv','run','python','tools/codex/camera_scripts.py'],cwd=ROOT,check=True,
                   stdout=subprocess.DEVNULL)
    inventory=json.loads((OUT/'camera-scripts.json').read_text())
    dll=OUT/f'camera_script_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                    str(source_dir/'native_camera_script.c'),str(ROOT/'src/camera/native_camera.c'),
                    '-o',str(dll),'-static-libgcc'],check=True)
    lib=C.CDLL(str(dll));bp=C.POINTER(C.c_uint8)
    lib.ae_camera_script_run.argtypes=[C.POINTER(Slot),bp,bp,C.c_size_t,C.c_uint32,C.POINTER(Result)]
    rng=random.Random(50_94423);cases=commands=removals=sequences=0;opcodes=set();refs={}
    def compare(ref,blob,base,raw,control):
        nonlocal cases,commands,removals
        slot=Slot.from_buffer_copy(raw);controls=(C.c_uint8*80).from_buffer_copy(control)
        data=(C.c_uint8*len(blob)).from_buffer_copy(blob);result=Result(1234567)
        assert lib.ae_camera_script_run(C.byref(slot),controls,data,len(blob),base,C.byref(result))
        ref.ram[0x130000:0x1300B0]=raw;ref.ram[0xE68D0:0xE6920]=control
        removed=[];fetched=[]
        def remove(cpu,regs):removed.append(regs[4]);return 0
        def trace(pc,regs):
            if pc==0x8004C37C:fetched.append(regs[16])
        ref.trace=trace
        ref.call(0x8004C318,(0x80130000,),hooks={0x80047290:remove})
        ref.trace=None
        expected=bytes(ref.ram[0x130000:0x1300B0])
        assert bytes(slot)==expected,(cases,[(hex(i),a,b) for i,(a,b) in enumerate(zip(bytes(slot),expected)) if a!=b])
        assert bytes(controls)==ref.ram[0xE68D0:0xE6920],cases
        assert result.remove_slot==(removed[0] if removed else 0xFFFFFFFF),(cases,result.remove_slot,removed)
        cases+=1;commands+=len(fetched);removals+=len(removed)
        for pc in fetched:opcodes.add(struct.unpack_from('<H',blob,pc-base)[0])
        return bytes(slot),bytes(controls),bool(removed)
    for script in inventory['scripts']:
        member=script['member']
        if member not in refs:
            blob=(ROOT/f'extract/members/{member:03X}.bin').read_bytes()
            refs[member]=(Reference(blob),blob)
        ref,blob=refs[member];base=0x80154000
        for repeat in range(3):
            raw=bytearray(rng.randbytes(0xB0));control=rng.randbytes(80)
            struct.pack_into('<I',raw,0x84,base+script['start']);struct.pack_into('<h',raw,0x88,0)
            for tick in range(20):
                raw,control,removed=compare(ref,blob,base,raw,control)
                if removed:break
                # Reach each future yield without pretending this is a gameplay tick.
                raw=bytearray(raw);struct.pack_into('<h',raw,0x88,0)
            else:raise AssertionError('real local script did not reach remove')
            sequences+=1
    # Relative operand-address branch, with forward/backward targets.
    ref=Reference((ROOT/'extract/members/05B.bin').read_bytes())
    for delta,start in [(2,0),(-2,0),(-6,4)]:
        blob=bytearray(struct.pack('<4h',2,2,2,2));struct.pack_into('<hh',blob,start,1,delta)
        base=0x80131000;ref.ram[0x131000:0x131008]=blob
        raw=bytearray(rng.randbytes(0xB0));struct.pack_into('<I',raw,0x84,base+start)
        struct.pack_into('<h',raw,0x88,0);compare(ref,bytes(blob),base,raw,rng.randbytes(80))
    guards=0
    for blob,pc in [(b'\x03\0',0),(b'\xFF\0',0),(b'\x14\0\0\0',0),
                    (b'\x01\0\xFF\x7F',0),(b'\x02\0',1),
                    (struct.pack('<3h',3,123,255),0)]:
        raw=bytearray(rng.randbytes(0xB0));struct.pack_into('<I',raw,0x84,0x80131000+pc)
        struct.pack_into('<h',raw,0x88,0);slot=Slot.from_buffer_copy(raw)
        controls=(C.c_uint8*80).from_buffer_copy(rng.randbytes(80));before=bytes(controls)
        data=(C.c_uint8*len(blob)).from_buffer_copy(blob);result=Result(1234567)
        assert not lib.ae_camera_script_run(C.byref(slot),controls,data,len(blob),0x80131000,C.byref(result))
        assert bytes(slot)==raw and bytes(controls)==before and result.remove_slot==1234567
        guards+=1
    for timer,pointer in [(0,0),(1,0xFFFFFFFF),(-1,0xFFFFFFFF)]:
        raw=bytearray(rng.randbytes(0xB0));struct.pack_into('<I',raw,0x84,pointer)
        struct.pack_into('<h',raw,0x88,timer);slot=Slot.from_buffer_copy(raw)
        controls=(C.c_uint8*80).from_buffer_copy(rng.randbytes(80));before=bytes(controls);result=Result(1234567)
        assert lib.ae_camera_script_run(C.byref(slot),controls,None,0,0,C.byref(result))
        assert bytes(slot)==raw and bytes(controls)==before and result.remove_slot==0xFFFFFFFF
        guards+=1
    report=dict(actual_scripts=len(inventory['scripts']),complete_sequences=sequences,
                original_vm_calls=cases,executed_commands=commands,remove_requests=removals,
                opcodes=[f'{v:02X}' for v in sorted(opcodes)],guard_checks=guards,failures=0)
    (OUT/'camera-script-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    (ROOT/'notes/port/validation/camera_script_validation.md').write_text(
        '# Local camera script VM validation\n\nGenerated by check_camera_script.py.\n\n'+
        '\n'.join(f'- {k}: {v}' for k,v in report.items())+'\n\n'+
        'Every local area script runs to removal in three arbitrary initial slot/control states. '
        'Complete CameraScriptRun instructions compare all176slot and80control bytes, next '
        'pointers and removal requests. Timer waits are cleared between yields to examine '
        'subsequent commands; this is not a gameplay timing trace. CameraSlotRemove is a '
        'controlled endpoint; the real slot/global lifecycle remains separate.\n')
    print(report)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-dir',type=Path,default=ROOT/'src/camera')
    main(p.parse_args().source_dir)
