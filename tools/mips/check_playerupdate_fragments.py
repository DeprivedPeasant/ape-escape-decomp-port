"""Independent range checks of three internal portions of complete PlayerUpdate.

These range checks do not certify PlayerUpdate or intercept it in the game
host. Both inputs and outputs include the entire RAM image and scratchpad;
only execution outside the explicitly reported ranges is stopped. No stack
mask or shared stale words are used here; the vertical range has its registered
two-instruction unreachable proof.
"""
import argparse
import ctypes as C
import itertools
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mips'))
import mips_ram as mr
import exclusion_registry as reg

AREAS = ('05B.bin', '064.bin', '066.bin', '05E.bin')
RANGES = {
    'copy_input': (0x80060A30, 0x80060C68, 0x80060C68),
    'select_tables': (0x80060C68, 0x80060D08, 0x800433B0),
    'prepare_vertical_move': (0x800614E8, 0x80061694, 0x80067974),
}
HEADER = ROOT / 'src/player/ae_playerupdate_internal.h'


class Harness:
    def __init__(self, header=HEADER, label='base'):
        out = ROOT / 'extract/checks'
        out.mkdir(parents=True, exist_ok=True)
        shim = out / f'playerupdate_fragments_{os.getpid()}_{label}.c'
        shim.write_text(f'#include "{header.as_posix()}"\n' + '\n'.join(
            f'void test_playerupdate_{name}(AeRam *r) {{ playerupdate_{name}(r); }}'
            for name in RANGES))
        dll = shim.with_suffix('.dll')
        source = ROOT/'tools/mips/playerupdate_test_api.c' if header == HEADER else shim
        subprocess.run([shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra',
                        '-Werror', '-shared', '-static-libgcc', str(source),
                        str(ROOT/'src/core/ae_ram.c'), '-I', str(ROOT/'src/player'),
                        '-o', str(dll)], check=True)
        self.lib = C.CDLL(str(dll))
        self.lib.ae_ram_create.restype = C.c_void_p
        self.lib.ae_ram_destroy.argtypes = [C.c_void_p]
        self.lib.ae_ram_clear_error.argtypes = [C.c_void_p]
        self.lib.ae_ram_failed.argtypes = [C.c_void_p]
        self.ram = self.lib.ae_ram_create()
        self.cpu = mr.MipsRam.from_extract()
        self.cpu.coverage = set()
        self.counts = {name: 0 for name in RANGES}
        for name in RANGES:
            getattr(self.lib, f'test_playerupdate_{name}').argtypes = [C.c_void_p]
            getattr(self.lib, f'test_playerupdate_{name}').restype = None

    def close(self):
        self.lib.ae_ram_destroy(self.ram)

    def run(self, name, image, scratch):
        self.cpu.ram[:] = image
        self.cpu.scratch[:] = scratch
        C.memmove(self.ram, image, mr.RAM_SIZE)
        C.memmove(self.ram+mr.RAM_SIZE, scratch, mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)
        start, _, stop = RANGES[name]
        self.cpu.call(start, patches={stop: lambda cpu, regs: mr.SENTINEL},
                      initial_regs={20: mr.SCRATCH_BASE})
        getattr(self.lib, f'test_playerupdate_{name}')(self.ram)
        assert not self.lib.ae_ram_failed(self.ram), name
        actual = C.string_at(self.ram, mr.RAM_SIZE)
        expected = bytes(self.cpu.ram)
        if actual != expected:
            index = next(i for i in range(mr.RAM_SIZE) if actual[i] != expected[i])
            raise AssertionError(f'{name}: RAM differs at {index+0x80000000:08X}')
        actual = C.string_at(self.ram+mr.RAM_SIZE, mr.SCRATCH_SIZE)
        if actual != bytes(self.cpu.scratch):
            index = next(i for i in range(mr.SCRATCH_SIZE) if actual[i] != self.cpu.scratch[i])
            raise AssertionError(f'{name}: scratch differs at {index+mr.SCRATCH_BASE:08X}')
        self.counts[name] += 1


def scenarios(seed):
    rng = random.Random(seed)
    for member in AREAS:
        cpu = mr.MipsRam.from_extract(ROOT/'extract/members'/member)
        base = bytes(cpu.ram)
        for aim, camera, mode, health, lock in itertools.product(
                (0, 1, 2), (0, 1, 2), (0, 1, 2), (0, 1, -1, -32768), (0, 11)):
            cpu.ram[:] = base
            cpu.ram[0xE5478:0xE54E0] = rng.randbytes(0x68)
            for address, value in ((0x800E6902, aim), (0x800E6908, camera), (0x800E6916, mode)):
                cpu.write8(address, value)
            cpu.write16(0x800EC2C8, health & 0xFFFF)
            cpu.write16(0x800F44A2, lock)
            yield 'copy_input', bytes(cpu.ram), rng.randbytes(mr.SCRATCH_SIZE)
        for state, gadget in itertools.product((0, 127, 128, 129, 65535), (-128, -1, 0, 1, 9, 127)):
            cpu.ram[:] = base
            cpu.ram[0xF45F0:0xF5600] = rng.randbytes(0x1010)
            cpu.write16(0x800EC23E, state)
            cpu.write8(0x800EC2D2, gadget & 255)
            yield 'select_tables', bytes(cpu.ram), rng.randbytes(mr.SCRATCH_SIZE)
        for state, fluid, velocity in itertools.product(
                (0, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 65535),
                (0, 1, 2, 3, 15), (-0x80000000, -33, -17, -16, -1, 0, 1, 16, 17, 33, 0x7FFFFFFF)):
            cpu.ram[:] = base
            cpu.ram[0xEC1E0:0xEC330] = rng.randbytes(0x150)
            cpu.write16(0x800EC23E, state)
            cpu.write16(0x800EC318, (rng.getrandbits(12) << 4) | fluid)
            cpu.write32(0x800EC214, velocity & 0xFFFFFFFF)
            cpu.write32(0x800EC014, rng.getrandbits(32))
            # Cover both sides of buoyancy and the wrapping subtraction.
            cpu.write32(0x800EC208, rng.choice((-0x80000000, -1000, 0, 1000, 0x7FFFFFFF)) & 0xFFFFFFFF)
            cpu.write16(0x800EC31C, rng.choice((-1000, 0, 1000)) & 0xFFFF)
            yield 'prepare_vertical_move', bytes(cpu.ram), rng.randbytes(mr.SCRATCH_SIZE)


MUTATIONS = [
    ('aim mask', '& 0xFAu', '& 0xFBu'),
    ('camera mask', '& 0x2F0u', '& 0x2F1u'),
    ('health signed', 'PU_HEALTH) != 0', 'PU_HEALTH) > 0'),
    ('lock value', 'PU_LOCK) == 11', 'PU_LOCK) == 10'),
    ('right aim during death', 'aim_mode && !locked ?', 'aim_mode && !locked && alive ?'),
    ('state threshold', 'PU_STATE) < 128', 'PU_STATE) <= 128'),
    ('gadget signed', 'ae_rds8(r, PU_PLAYER + PU_GADGET)', 'ae_rd8(r, PU_PLAYER + PU_GADGET)'),
    ('event table', 'gadget_set + PU_STATE_SET_EVENTS', 'gadget_set + PU_STATE_SET_TRANSITIONS'),
    ('water dispatch', 'state == 0x49 || state == 0x4A', 'state == 0x49'),
    ('buoyancy', 'fraction += 8192u', 'fraction += 8191u'),
    ('dive impulse', 'fraction += 20480u', 'fraction += 20479u'),
    ('gravity', 'PU_GRAVITY) << 7', 'PU_GRAVITY) << 6'),
    ('velocity lower clamp', 'velocity < -16) velocity = -16', 'velocity < -16) velocity = -15'),
    ('velocity upper clamp', 'velocity > 16) velocity = 16', 'velocity > 16) velocity = 15'),
    ('half rounding', 'PU_VECTOR_Y) / 2)', 'PU_VECTOR_Y) >> 1)'),
    ('fraction consumption', 'PU_FRACTION + PU_VECTOR_Y, 0);\n    ae_wr32', 'PU_FRACTION + PU_VECTOR_Y, 1);\n    ae_wr32'),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seed', type=int, default=60780)
    parser.add_argument('--mutate', action='store_true')
    args = parser.parse_args()
    h = Harness()
    try:
        for scenario in scenarios(args.seed):
            h.run(*scenario)
        summary = reg.coverage_summary([(name, start, end-start, True)
            for name, (start, end, _) in RANGES.items()], h.cpu.coverage)
        reg.print_coverage(summary)
        for name, result in summary.items():
            print(f'{name}: {h.counts[name]} cases')
            assert result['uncovered_count'] == 0, (name, result)
            assert not result['stale_declared'], (name, result)
        if args.seed == 60780 and not args.mutate:
            reg.write_results(h.counts, summary, True, extra={'fragment_checks_only': True})
    finally:
        h.close()
    if args.mutate:
        source = HEADER.read_text()
        for index, (label, before, after) in enumerate(MUTATIONS):
            assert before in source, label
            path = ROOT/'extract/checks'/f'playerupdate_fragment_mutant_{index}.h'
            path.write_text(source.replace(before, after, 1))
            mutant = Harness(path, f'mutant_{index}')
            try:
                for scenario in scenarios(args.seed):
                    mutant.run(*scenario)
            except AssertionError:
                print(f'caught: {label}')
            else:
                raise AssertionError(f'mutation survived: {label}')
            finally:
                mutant.close()
        print(f'{len(MUTATIONS)} fragment mutations caught')
    print('Independent range checks; check_playerupdate.py validates the complete enclosing function.')


if __name__ == '__main__':
    main()
