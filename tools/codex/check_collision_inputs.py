"""Malformed collision-member and angle-asset rejection checks."""
import ctypes as ct
import json
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library,OUT
from codex.check_collision_floor import setup
def main():
    lib=library();setup(lib);b=(da.MEMBERS/'05B.bin').read_bytes()
    ah=struct.unpack_from('<I',b,0x4D0)[0];co=ah+struct.unpack_from('<I',b,ah+16)[0]*4
    grid,shapes,boxes=struct.unpack_from('<3I',b,co)
    edits=[('header offset',0x4D0,struct.pack('<I',0xFFFFFFFF)),('collision offset',ah+16,struct.pack('<I',0xFFFFFFFF)),
           ('grid offset',co,struct.pack('<I',0xFFFFFFFF)),('shape offset',co+4,struct.pack('<I',0xFFFFFFFF)),
           ('collider offset',co+8,struct.pack('<I',0xFFFFFFFF)),('zero width',co+grid,struct.pack('<H',0)),
           ('large depth',co+grid+2,struct.pack('<H',257)),('invalid grid index',co+grid+4,struct.pack('<h',32767)),
           ('invalid shape link',co+shapes+2+16,struct.pack('<h',32767)),('shape cycle',co+shapes+2+16,struct.pack('<h',0)),
           ('unknown shape type',co+shapes+2+18,b'\xFF'),
           ('unknown collider class',co+boxes+2+16,struct.pack('<H',0xF000)),
           ('invalid collider handler',co+boxes+2+16,struct.pack('<H',0x20FF))]
    cases=[(name,b[:off]+replace+b[off+len(replace):]) for name,off,replace in edits]
    cases += [('short header',b[:0x500]),('short collision table',b[:co+19]),('short collider records',b[:co+boxes+3])]
    path=OUT/'invalid_collision_member.bin';rows=[]
    for name,blob in cases:
        path.write_bytes(blob);c=lib.ae_collision_load(str(path).encode())
        if c:lib.ae_collision_free(c);raise AssertionError(f'malformed collision accepted: {name}')
        rows.append(dict(case=name,error=lib.ae_collision_error().decode()))
    original=(da.ROOT/'extract/disc/SCUS_944.23').read_bytes();path=OUT/'invalid_collision_math.bin'
    offset=0xB4430-0x10000+0x800
    for name,blob in [('short lookup',original[:offset+2048]),('invalid angle',original[:offset]+b'\xFF\x7F'+original[offset+2:])]:
        path.write_bytes(blob)
        assert not lib.ae_collision_math_load(str(path).encode()),name
        rows.append(dict(case=name,error=lib.ae_collision_error().decode()))
    result=dict(rejected=rows,failures=0)
    lib.ae_collision_response_supported.argtypes=[ct.c_void_p,ct.POINTER(ct.c_uint32)]
    guards=[]
    for kind in (70,71,72,73,82,83,84):
        blob=bytearray(b);struct.pack_into('<H',blob,co+shapes+2+18,0x3000|kind)
        path.write_bytes(blob);c=lib.ae_collision_load(str(path).encode())
        assert c,lib.ae_collision_error()
        try:
            missing=(ct.c_uint32*2)()
            assert not lib.ae_collision_response_supported(c,missing) and missing[0]>0
            guards.append(dict(case=f'collider-only type {kind} in grid',missing_shapes=missing[0]))
        finally:lib.ae_collision_free(c)
    result['response_guards']=guards
    (OUT/'collision-input-validation.json').write_text(json.dumps(result,indent=2)+'\n');print(f'{len(rows)} malformed inputs rejected, {len(guards)} collider-only grid guards passed, failures=0')
if __name__=='__main__':main()
