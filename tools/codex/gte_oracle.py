"""Build an unchanged pinned PCSX-ReARMed GTE backend as an ignored test oracle.

No emulator implementation is copied into the port. Downloads and binaries remain
under extract/. The small wrapper only initializes registers and dispatches commands.
"""
import ctypes as C
import hashlib
import json
import os
import shutil
import subprocess
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'extract/codex/deps/gte-reference'
COMMIT = '8b6c163575e6ce9a0537553e0d4ed4225315955e'
FILES = ['libpcsxcore/' + n for n in ('gte.c', 'gte.h', 'gte_arm.h', 'psxcommon.h',
         'r3000a.h', 'system.h', 'psxmem.h', 'gte_divider.h', 'gte_divider.c')]
FILES += ['include/compiler_features.h', 'include/arm_features.h']
WRAPPER = r'''
#include "libpcsxcore/gte.h"
#include <string.h>
psxRegisters psxRegs;
uint16_t oracle_otz(const uint16_t *sz, int count) {
    psxCP2Regs regs = {0};
    for (int i=0;i<count;i++) regs.CP2D.r[16+(count==3?1:0)+i]=sz[i];
    regs.CP2C.r[count==3?29:30]=count==3?341:256;
    gteDispatch(&regs,count==3?0x0158002D:0x0168002E);
    return (uint16_t)regs.CP2D.r[7];
}
uint32_t oracle_dpcs(const uint8_t *rgb, const uint8_t *far, uint16_t factor) {
    psxCP2Regs regs = {0};
    MTC2(&regs,(uint32_t)rgb[0]|(uint32_t)rgb[1]<<8|(uint32_t)rgb[2]<<16|(uint32_t)rgb[3]<<24,6);
    MTC2(&regs,factor,8);
    for (int i=0;i<3;i++) CTC2(&regs,(uint32_t)far[i]<<4,21+i);
    gteDispatch(&regs,0x00780010);
    return regs.CP2D.r[22];
}
uint16_t oracle_sz(const int16_t *row, int32_t tz, const int16_t *vertex) {
    psxCP2Regs regs = {0};
    CTC2(&regs,(uint16_t)row[0]|(uint32_t)(uint16_t)row[1]<<16,3);
    CTC2(&regs,(uint16_t)row[2],4);
    CTC2(&regs,(uint32_t)tz,7);
    CTC2(&regs,328,26);
    MTC2(&regs,(uint16_t)vertex[0]|(uint32_t)(uint16_t)vertex[1]<<16,0);
    MTC2(&regs,(uint16_t)vertex[2],1);
    gteDispatch(&regs,0x00180001);
    return (uint16_t)regs.CP2D.r[19];
}
uint32_t oracle_gte_step(uint32_t *state, uint32_t instruction, uint32_t value) {
    psxCP2Regs regs;
    _Static_assert(sizeof(regs)==256,"GTE register layout");
    memcpy(&regs,state,sizeof(regs));
    unsigned kind=(instruction>>21)&31, reg=(instruction>>11)&31;
    uint32_t result=0;
    if (kind==0) result=MFC2(&regs,reg);
    else if (kind==2) result=regs.CP2C.r[reg];
    else if (kind==4) MTC2(&regs,value,reg);
    else if (kind==6) CTC2(&regs,value,reg);
    else gteDispatch(&regs,instruction);
    memcpy(state,&regs,sizeof(regs));
    return result;
}
'''


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    records = []
    for name in FILES:
        path = OUT / name
        url = f'https://raw.githubusercontent.com/notaz/pcsx_rearmed/{COMMIT}/{name}'
        if not path.exists():
            data = urllib.request.urlopen(url, timeout=30).read()
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        data = path.read_bytes()
        records.append(dict(path=name, url=url, sha256=hashlib.sha256(data).hexdigest()))
    manifest = OUT / 'oracle-source.json'
    if manifest.exists():
        previous = json.loads(manifest.read_text())
        assert previous['commit'] == COMMIT, 'oracle revision changed'
        assert all(row in records for row in previous['files']), 'oracle source changed'
    manifest.write_text(json.dumps(dict(project='notaz/pcsx_rearmed', commit=COMMIT,
                                       license='GPL-2.0-or-later; external test oracle only', files=records), indent=2) + '\n')
    (OUT / 'libpcsxcore/config.h').write_text('/* Standalone native little-endian C backend. */\n#define MAXPATHLEN 4096\n')
    (OUT / 'oracle_wrapper.c').write_text(WRAPPER)
    dll = OUT / f'oracle_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'), '-std=gnu11', '-O2', '-shared', '-fwrapv', '-I' + str(OUT / 'include'),
                    str(OUT / 'libpcsxcore/gte.c'), str(OUT / 'libpcsxcore/gte_divider.c'), str(OUT / 'oracle_wrapper.c'),
                    '-o', str(dll), '-static-libgcc'], check=True)
    lib = C.CDLL(str(dll))
    lib.oracle_otz.argtypes = [C.POINTER(C.c_uint16), C.c_int]
    lib.oracle_otz.restype = C.c_uint16
    lib.oracle_dpcs.argtypes = [C.POINTER(C.c_uint8), C.POINTER(C.c_uint8), C.c_uint16]
    lib.oracle_dpcs.restype = C.c_uint32
    lib.oracle_sz.argtypes = [C.POINTER(C.c_int16), C.c_int32, C.POINTER(C.c_int16)]
    lib.oracle_sz.restype = C.c_uint16
    lib.oracle_gte_step.argtypes = [C.POINTER(C.c_uint32), C.c_uint32, C.c_uint32]
    lib.oracle_gte_step.restype = C.c_uint32
    return lib


if __name__ == '__main__':
    lib = build()
    print('Built pinned external GTE oracle', COMMIT)
