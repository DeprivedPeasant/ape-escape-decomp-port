"""Compare native terrain probes with original GetGroundHeight MIPS instructions.

uv run --offline python tools/codex/check_native_ground.py
Solid-height call is stubbed only in the test oracle: this validates terrain, not solids.
"""
import ctypes as ct
import json
import random
import shutil
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.pack_area import ROOT, OUT
from codex.mips_ground_reference import Reference


class Hit(ct.Structure):
    _fields_ = [(name, ct.c_int32) for name in ('height', 'slope_x', 'slope_z', 'surface', 'selector')]


def library():
    compiler = shutil.which('gcc')
    if not compiler:
        raise RuntimeError('installed compiler unavailable')
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT/'native_ground.dll'
    subprocess.run([compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-shared',
                    str(ROOT/'src/collision/native_ground.c'), '-o', str(dll), '-static-libgcc'], check=True)
    lib = ct.CDLL(str(dll))
    lib.ae_ground_load.argtypes = [ct.c_char_p]; lib.ae_ground_load.restype = ct.c_void_p
    lib.ae_ground_free.argtypes = [ct.c_void_p]
    lib.ae_ground_error.restype = ct.c_char_p
    lib.ae_ground_info.argtypes = [ct.c_void_p, ct.POINTER(ct.c_uint32)]
    lib.ae_ground_entry.argtypes = [ct.c_void_p, ct.c_uint32, ct.POINTER(ct.c_int32)]
    lib.ae_ground_entry.restype = ct.c_int
    lib.ae_ground_query.argtypes = [ct.c_void_p, ct.c_int32, ct.c_int32, ct.c_int32, ct.POINTER(Hit)]
    lib.ae_ground_query.restype = ct.c_int
    return lib


def compare(lib, native, reference, x, y, z):
    hit = Hit()
    status = lib.ae_ground_query(native, x, y, z, ct.byref(hit))
    expected = reference.query(x, y, z)
    actual = {name: getattr(hit, name) for name in expected}
    if expected != actual:
        raise AssertionError((x, y, z, status, hit.selector, expected, actual))
    return hit.selector


def main():
    lib = library()
    rng = random.Random(94423)
    rows, total, selectors, opcodes = [], 0, Counter(), set()
    for area, member in sorted(da.area_members().items()):
        path = da.MEMBERS/f'{member:03X}.bin'
        blob = path.read_bytes()
        native = lib.ae_ground_load(str(path).encode())
        if not native:
            raise AssertionError((area, lib.ae_ground_error().decode()))
        reference = Reference(blob)
        count = 0
        entry = (ct.c_int32*9)()
        try:
            for index in range(16):
                assert lib.ae_ground_entry(native, index, entry) == 1
                assert list(entry) == list(struct.unpack_from('<9i', blob, index*0x24))
            assert lib.ae_ground_entry(native, 16, entry) == 0
            if reference.disabled:
                points = [(0, -32768, 0)]
            else:
                w, d = reference.width, reference.depth
                points = [(-w*256-1, 0, 0), (w*256, 0, 0), (0, 0, -d*256-1), (0, 0, d*256)]
                for cz in range(d):
                    for cx in range(w):
                        for ux, uz in ((128,64),(448,256),(64,128),(256,448)):
                            points.append((cx*512-w*256+ux, rng.choice((-32768,0,32767)), cz*512-d*256+uz))
                points += [(rng.randrange(-w*256,w*256), rng.randrange(-32768,32768), rng.randrange(-d*256,d*256)) for _ in range(48)]
            for point in points:
                selectors[compare(lib, native, reference, *point)] += 1
                count += 1
        finally:
            lib.ae_ground_free(native)
        opcodes |= reference.opcodes
        total += count
        rows.append(dict(area=area, member=member, disabled=reference.disabled, width=reference.width,
                         depth=reference.depth, comparisons=count, entry0=list(struct.unpack_from('<9i',blob))))
        if len(rows)%10 == 0:
            print(f'Original-instruction comparisons: {len(rows)} areas / {total:,} probes, failures=0', flush=True)
    # Force every split variant with extreme/odd heights to exercise signed narrowing and truncation.
    template = bytearray((da.MEMBERS/'05B.bin').read_bytes())
    ah = struct.unpack_from('<I', template, 0x4D0)[0]
    grid = ah+struct.unpack_from('<I', template, ah+4)[0]*4
    w,d,colors = struct.unpack_from('<HHI', template, grid)
    heights = grid+8+w*d*12+colors*4
    fixture = OUT/'ground_fixture.bin'
    synthetic = 0
    for split in range(9):
        for values in [(0,1,-1,3,1), (-32768,32767,0,-1,12345),
                       *[tuple(rng.randrange(-32768,32768) for _ in range(5)) for _ in range(3)]]:
            modified = bytearray(template)
            word = 0xABCDEF01 if split == 8 else 0xABCD0801 | (split << 12)
            struct.pack_into('<I', modified, grid+8, word)
            for index, value in zip((0,1,w+1,w+2,(w+1)*(d+1)),values):
                struct.pack_into('<h', modified, heights+index*2, value)
            fixture.write_bytes(modified)
            native = lib.ae_ground_load(str(fixture).encode()); reference = Reference(modified)
            assert native
            try:
                for ux,uz in ((0,0),(511,0),(0,511),(511,511),(256,256),(128,64),(448,256),(64,128),(256,448),
                              (255,256),(256,255),(257,256),(256,257)):
                    for y in (-32768,0,32767):
                        compare(lib,native,reference,ux-w*256,y,uz-d*256)
                        synthetic += 1
            finally:
                lib.ae_ground_free(native)
    result = dict(areas=rows, real_probes=total, synthetic_probes=synthetic, selectors=dict(selectors),
                  executed_opcodes=sorted(opcodes), entries_checked=len(rows)*16, failures=[])
    (OUT/'ground-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    note = ['# Native terrain probes and area entries (generated)', '',
            'Generated by `uv run --offline python tools/codex/check_native_ground.py`.', '',
            '## Scope and evidence', '',
            '`native_ground.c` reads original extracted area members directly. It reconstructs the terrain '
            'height portion of GetGroundHeight (0x80043400), preserving signed 16-bit differences, '
            'division toward zero, triangle selectors, slopes, surface attributes and sentinel results. '
            'Source exports from private read-only Ghidra are ignored under notes/decomp/codex/native-ground*.c.', '',
            'The test oracle executes the actual original MIPS instructions from extract/disc/SCUS_944.23 '
            'using a small test-only integer interpreter with branch and load delay slots. It supplies '
            'the real LevelGrid/header data and hooks only GetSolidHeight to return 0x7FFF (no solids). '
            'No original code/data is embedded into committed files or used by the native renderer as '
            'emulated execution. This establishes terrain equivalence, not full collision parity.', '',
            f'- Original area members accepted: {len(rows)}.',
            f'- Real-data height/slope/surface probes compared: {total:,}.',
            f'- Synthetic split/boundary/extreme-height probes compared: {synthetic:,}.',
            f'- Entry records checked: {len(rows)*16:,}.', '- Mismatches: 0.', '',
            '## Recovered details', '',
            'Area entries are the first 16 fixed 0x24-byte records of the area blob. Each contains '
            'nine 32-bit words: player xyz at +0, camera xyz at +0x0C, facing xyz at +0x18. '
            'AreaLoad sets the entry table pointer to the blob base; AreaGetEntryPoint indexes entry*0x24; '
            'PlayerPlaceAtEntry reads player xyz and the low halves of facing words. Area 01 entry 0 '
            'is player (1280,-1536,-3072), camera (1280,-2048,-4000), facing (0,0,4096). '
            'Y remains negative-up in original data, so rendering maps xyz to (x,-y,z)/256.', '',
            'The terrain selector starts with `(localX < localZ)*2 | (512-localX < localZ)`. '
            'When cell bit11 is set it ORs 0x80 and `(cellWord >> 8) & 0x70`: the variant bits '
            'are original word bits12..14. Accepted selectors are 0/90/A0 (top), 1/A1/B1 (right), '
            '2/92/C2 (left), 3/B3/C3 (bottom), written in hex. Others produce 0x4000 with zero slopes. '
            'This corrects the earlier notes/12 description of variant bits8..10; proposal is in the inbox.', '',
            'Source bit0 controls terrain visibility, not probing. Terrain-disabled header bit1 yields '
            '0x4000; ordinary points outside the half-open terrain rectangle return zero and cleared '
            'scratch data. Some disabled-grid slopes are uninitialized in the original; the oracle uses '
            'zero stack values for deterministic comparison, and the native API reports zero there.', '',
            '## Remaining collision work', '',
            'GetSolidHeight, stacked shape handlers, collider volumes, zone masks, water, wall pushes '
            'and dynamic props are not included in this terrain API. Returning terrain-only height '
            'must not be described as complete gameplay collision. Entry facing/render orientation '
            'and original player movement/state behavior still require integration validation.', '',
            'Full results: extract/codex/native/ground-validation.json.']
    (ROOT/'notes/67-native-ground.md').write_text('\n'.join(note)+'\n')
    print(f'PASS: {len(rows)} areas, {total:,} real + {synthetic:,} synthetic probes, {len(rows)*16} entries',flush=True)


if __name__ == '__main__':
    main()
