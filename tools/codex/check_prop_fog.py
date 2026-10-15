"""Ordinary-prop fog configuration and actual original F4 integer instruction checks.

Original game bytes are loaded from ignored extract/, never embedded in this tool.
GTE endpoints are excluded explicitly: this is not a rendered-pixel comparison.
"""
import ctypes as C
import json
import os
import random
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import dump_areas as da
import prop_fog
from mips_ground_reference import Reference, signed

OUT = ROOT / 'extract/codex/native'


class Fog(C.Structure):
    _fields_ = [('rgb', C.c_uint8 * 3), ('coefficient', C.c_uint16),
               ('near_distance', C.c_uint16), ('draw_distance', C.c_uint32)]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f'fog_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra',
                    '-Werror', '-shared', str(ROOT / 'src/render/native_prop_fog.c'),
                    '-o', str(dll), '-static-libgcc'], check=True)
    lib = C.CDLL(str(dll))
    lib.ae_prop_fog_config.argtypes = [C.c_void_p, C.c_size_t, C.POINTER(Fog)]
    lib.ae_prop_fog_factor.argtypes = [C.c_int32, C.c_uint16, C.c_uint16]
    lib.ae_prop_fog_factor.restype = C.c_int32
    areas = []
    for area, member in sorted(da.area_members().items()):
        blob = (da.MEMBERS / f'{member:03X}.bin').read_bytes()
        ah = struct.unpack_from('<I', blob, 0x4D0)[0]
        h = blob[ah:ah + 0x30]
        enabled, near, far = struct.unpack_from('<h2xHH', h, 0x28)
        effective = near if enabled else 0x4B0
        cfg = Fog()
        C.memset(C.byref(cfg), 0xA5, C.sizeof(cfg))
        assert lib.ae_prop_fog_config(h, len(h), C.byref(cfg)) == 1
        assert (tuple(cfg.rgb), cfg.coefficient, cfg.near_distance, cfg.draw_distance) == (
            tuple(h[0x20:0x23]), 0xF000, effective, effective * 4 + 0x400)
        parsed = prop_fog.load(member)
        assert (parsed['rgb'], parsed['fog_enabled'], parsed['near'], parsed['far'],
                parsed['effective_near'], parsed['coefficient'], parsed['draw_distance']) == (
            tuple(cfg.rgb), enabled, near, far, effective, cfg.coefficient, cfg.draw_distance)
        areas.append(dict(area=area, member=member, rgb=list(cfg.rgb), near=effective,
                          far=far, draw_distance=cfg.draw_distance))
    guards = 0
    good = bytes(0x30)
    for header, size, target in [(None, 0x30, True), (good, 0x2F, True),
                                 (good, 0x30, False)]:
        cfg = Fog()
        C.memset(C.byref(cfg), 0xA5, C.sizeof(cfg))
        before = bytes(cfg)
        assert lib.ae_prop_fog_config(header, size, C.byref(cfg) if target else None) == 0
        assert bytes(cfg) == before
        guards += 1
    blob = (da.MEMBERS / '05B.bin').read_bytes()
    malformed = bytearray(blob)
    struct.pack_into('<I', malformed, 0x4D0, len(blob))
    for bad in (b'', blob[:0x4D0], malformed):
        try:
            prop_fog._load_blob(bad, 0x5B)
        except ValueError:
            guards += 1
        else:
            raise AssertionError('malformed source accepted')
    # Config ranges and signed nonzero enable semantics, independently derived raw fields.
    rng = random.Random(94423)
    config_cases = 0
    for enabled in (0, 1, -1, -32768, 32767):
        for near in (0, 1, 0x4B0, 2000, 65535):
            h = bytearray(rng.randbytes(0x30))
            struct.pack_into('<h2xHH', h, 0x28, enabled, near, rng.randrange(65536))
            cfg = Fog()
            assert lib.ae_prop_fog_config(bytes(h), len(h), C.byref(cfg)) == 1
            assert cfg.near_distance == (near if enabled else 0x4B0)
            assert cfg.draw_distance == cfg.near_distance * 4 + 0x400
            # Explicit memset+memcpy guarantees structure padding, unlike C aggregate assignment.
            assert bytes(cfg)[3] == 0 and bytes(cfg)[8:12] == struct.pack('<I', cfg.draw_distance)
            config_cases += 1
    ref = Reference(blob)
    apply_cases = 0
    far_cases = 0
    for _ in range(3000):
        rgb = tuple(rng.randrange(256) for _ in range(3))
        coef, near = rng.randrange(65536), rng.randrange(65536)
        for i, value in enumerate(rgb):
            ref.write(0x800EBFBC + i, 1, value)
        ref.write(0x800EBFC4, 4, rng.randrange(65536) << 16 | coef)
        ref.write(0x800EBFC8, 4, rng.randrange(65536) << 16 | near)
        ref.scratch[0x3C0:0x3C8] = rng.randbytes(8)
        calls = []

        def far(cpu, regs):
            calls.append(tuple(regs[4:7]))
            return 0

        ref.call(0x800323F8, hooks={0x80031630: far})
        assert calls == [rgb]
        assert ref.scratch[0x3C0:0x3C8] == struct.pack('<4H', coef, near, 0, 0)
        apply_cases += 1
        regs = ref.call(0x80031630, rgb, hooks={0x8003163C: lambda cpu, regs: 0})
        assert tuple(regs[4:7]) == tuple(v << 4 for v in rgb)
        far_cases += 1
    inputs = [(d, n, c) for d in (-2147483648, -1, 0, 1, 1999, 2000, 2001,
                                  2273, 2274, 65535, 2147483647)
              for n in (0, 1, 2000, 65535) for c in (0, 1, 0xF000, 65535)]
    inputs += [(signed(rng.randrange(1 << 32)), rng.randrange(65536), rng.randrange(65536))
               for _ in range(20000)]
    factor_cases = 0
    paths = ((0x8001950C, 0x80019564), (0x80019628, 0x80019680),
             (0x80019728, 0x80019780))
    for root, stop in paths:
        def trace(pc, regs):
            if pc == root:
                regs[10], regs[12] = 0x1F800000, 0x80130003
        ref.trace = trace
        ref.write(0x8013000C, 4, 0x28785634)
        for depth, near, coef in inputs:
            ref.write(0x1F8003DC, 4, depth)
            ref.write(0x1F8003C2, 2, near)
            ref.write(0x1F8003C0, 2, coef)
            regs = ref.call(root, hooks={stop: lambda cpu, regs: 0})
            actual = lib.ae_prop_fog_factor(depth, near, coef)
            assert actual == ref.read(0x1F8003DC, 4) == regs[4], (hex(root), depth, near, coef)
            assert 0 <= actual <= 4096
            factor_cases += 1
    ref.trace = None
    report = dict(areas=len(areas), config_cases=config_cases, guards=guards,
                  original_apply_fog_cases=apply_cases, original_far_color_prefix_cases=far_cases,
                  original_factor_paths=len(paths), original_factor_cases=factor_cases, failures=0)
    (OUT / 'fog-validation.json').write_text(json.dumps(dict(report=report, areas=areas), indent=2) + '\n')
    (ROOT / 'notes/port/validation/prop_fog_validation.md').write_text(
        '# Ordinary-prop fog validation\n\nGenerated by check_prop_fog.py.\n\n' +
        '\n'.join(f'- {key}: {value}' for key, value in report.items()) +
        '\n\nActual area headers agree with the C and Python configuration readers. '
        'ApplyLevelFog executes completely with only SetFarColor hooked; its ordered RGB call and eight scratch bytes agree. '
        'The SDK far-color shift prefix and all three original F4 integer factor paths execute from extracted EXE bytes. '
        'GTE depth, DPCS/CC, color saturation, GPU ordering, runtime palettes and rendered-scene parity are excluded. '
        'The helper is not wired into the viewer. Original draw distance is near*4+0x400 (1024).\n')
    print(report)


if __name__ == '__main__':
    main()
