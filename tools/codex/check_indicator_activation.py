"""Compare local activation bytes and requests with complete original MIPS."""
import ctypes as C
import json
import os
import random
import shutil
import struct
import subprocess
from pathlib import Path
from check_native_indicators import Indicator
from mips_ground_reference import Reference, signed

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'extract/codex/native'


class Activation(C.Structure):
    _fields_ = [('hint', C.c_uint32), ('pickups', C.c_uint32),
                ('sound', C.c_uint32), ('sector', C.c_int32),
                ('position', C.c_int32 * 3)]


def main():
    dll = OUT / f'indicator_activation_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra',
                    '-Werror', '-shared', str(ROOT/'src/indicators/native_indicators.c'),
                    str(ROOT/'src/camera/native_camera_math.c'), '-o', str(dll),
                    '-static-libgcc'], check=True)
    lib = C.CDLL(str(dll))
    lib.ae_indicator_activate.argtypes = [C.POINTER(Indicator), C.c_int32,
                                        C.c_int, C.POINTER(Activation)]
    ref = Reference((ROOT/'extract/members/05B.bin').read_bytes())
    rng = random.Random(717_94423)
    accepted = hints = pickups = calls_total = 0
    for case in range(8000):
        data = bytearray(rng.randbytes(96)); index = case % 4
        kind = rng.choice([-32768, -1, 0, 1, 2, 3, 32767])
        state = rng.choice([0, 0, 1, 2, 65535])
        timer = rng.choice([-32768, -1, 0, 1, 90, 32767])
        sector = rng.randrange(-2147483648, 2147483648)
        busy = rng.choice([0, 0, 1, -1])
        struct.pack_into('<h', data, index*24, kind)
        struct.pack_into('<h', data, index*24+10, timer)
        struct.pack_into('<H', data, index*24+22, state)
        ref.ram[0xBBD34:0xBBD94] = data
        slot = Indicator.from_buffer_copy(data[index*24:(index+1)*24])
        events = Activation.from_buffer_copy(bytes([0xAA])*C.sizeof(Activation))
        calls = []

        def record(name, argc=0, value=0):
            def hook(cpu, regs):
                calls.append((name, *[signed(regs[4+i]) for i in range(argc)]))
                return value
            return hook

        def spawn(cpu, regs):
            calls.append(('pickup', signed(regs[4]),
                          *[signed(cpu.read(regs[5]+i*2, 2), 16) for i in range(3)],
                          signed(regs[6]), signed(regs[7])))
            return 0

        hooks = {0x8008DF3C: lambda cpu, regs: busy,
                 0x8008DEEC: record('hint'),
                 0x8008C09C: record('heap', 1, 0x80130000),
                 0x80012C0C: record('member', 2),
                 0x80025E24: lambda cpu, regs: 1000,
                 0x80025D20: record('cd_sector', 1),
                 0x80012360: record('cd_command', 1),
                 0x8005D100: record('stage', 3),
                 0x8007B374: record('player_mode', 1),
                 0x80048430: record('aim_clear'),
                 0x80048748: record('camera_fixed_off'),
                 0x80083AA8: spawn, 0x800877F8: record('sound', 1)}
        ref.call(0x800717B4, (index, sector), hooks=hooks)
        assert lib.ae_indicator_activate(C.byref(slot), sector, busy, C.byref(events)) == 1
        expected = bytes(ref.ram[0xBBD34+index*24:0xBBD34+(index+1)*24])
        assert bytes(slot) == expected, (case, kind, state, timer, busy)
        # Original must also leave every other slot untouched.
        for other in range(4):
            if other != index:
                assert ref.ram[0xBBD34+other*24:0xBBD34+(other+1)*24] == data[other*24:(other+1)*24]
        actual = []
        if events.hint:
            assert events.sector == sector
            actual.append(('hint',))
            assert [x[0] for x in calls] == ['hint', 'heap', 'member', 'cd_sector',
                'cd_command', 'cd_command', 'cd_command', 'stage', 'player_mode',
                'aim_clear', 'camera_fixed_off', 'sound']
            assert calls[1] == ('heap', 0x800)
            assert calls[2][2] == 0x14
            assert calls[3] == ('cd_sector', signed(1000+sector))
            assert calls[4:7] == [('cd_command', 0xE), ('cd_command', 2), ('cd_command', 0x20)]
            assert calls[7:9] == [('stage', 3, 0, 0), ('player_mode', 0xB)]
            hints += 1
        if events.pickups:
            assert events.pickups == 3
            actual.extend([('pickup', 1, *events.position, 3, 0)]*3)
            pickups += 1
        if events.sound:
            actual.append(('sound', events.sound)); accepted += 1
        relevant = [x for x in calls if x[0] in ('hint', 'pickup', 'sound')]
        assert actual == relevant, (case, actual, calls)
        calls_total += len(calls)
    slot = Indicator.from_buffer_copy(rng.randbytes(24)); before = bytes(slot)
    event = Activation.from_buffer_copy(rng.randbytes(C.sizeof(Activation))); old = bytes(event)
    assert not lib.ae_indicator_activate(None, 1, 0, C.byref(event)) and bytes(event) == old
    assert not lib.ae_indicator_activate(C.byref(slot), 1, 0, None) and bytes(slot) == before
    report = dict(original_activation_cases=8000, accepted=accepted, hints=hints,
                  pickup_releases=pickups, ordered_callee_calls=calls_total,
                  invalid_argument_checks=2, failures=0)
    (OUT/'indicator-activation-validation.json').write_text(json.dumps(report, indent=2)+'\n')
    (ROOT/'notes/port/validation/indicator_activation_validation.md').write_text(
        '# Indicator activation validation\n\nGenerated by `check_indicator_activation.py`.\n\n'+
        '\n'.join(f'- {key}: {value}' for key, value in report.items())+'\n\n'+
        'Complete original Indicator_Activate instructions execute with controlled external callees. '
        'All slot bytes and ordered hint/pickup/sound requests match. The hint branch also checks '
        'the original CD sector calculation, queue command order, stage request and player mode. '
        'The port returns requests; it does not implement those external services or pickup physics.\n')
    print(report)


if __name__ == '__main__':
    main()
