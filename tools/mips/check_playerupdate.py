"""Differential checker for complete PlayerUpdate and TurnTowardsY.

Runs original instructions with only the unported SDK RotMatrixY intercepted.
Real EXE and area images, all non-stack RAM and scratchpad, and SDK arguments
are compared. playerupdate_whole adds complete enclosing-function cases with
named dependency doubles; check_game_area validates real-callee stage frames.
"""
import argparse
import ctypes as C
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

FUNCTIONS = {'TurnTowardsY': (0x80066C90, 284)}
AREAS = ('05B.bin', '064.bin', '066.bin', '05E.bin')
HOOKS = [('RotMatrixY', 0x80030890, 2)]
HOOKFN = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32)


class Hooks(C.Structure):
    _fields_ = [('user', C.c_void_p), ('RotMatrixY', HOOKFN)]


def build(source=None, label='base'):
    out = ROOT / 'extract/checks'
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f'playerupdate_{os.getpid()}_{label}.dll'
    subprocess.run([shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-shared', '-static-libgcc', str(ROOT/'src/core/ae_ram.c'),
                    str(ROOT/'src/core/ae_sdk.c'), str(ROOT/'src/core/ae_sdk_matrix.c'),
                    str(source or ROOT/'src/player/ae_playerupdate.c'),
                    '-I', str(ROOT/'src/player'), '-o', str(dll)], check=True)
    return dll


class Harness:
    def __init__(self, source=None, label='base', gte=None):
        self.lib = C.CDLL(str(build(source, label)))
        self.lib.ae_ram_create.restype = C.c_void_p
        self.ram = self.lib.ae_ram_create()
        self.lib.ae_ram_destroy.argtypes = [C.c_void_p]
        self.lib.ae_ram_failed.argtypes = [C.c_void_p]
        self.lib.ae_ram_clear_error.argtypes = [C.c_void_p]
        self.lib.ae_TurnTowardsY.argtypes = [C.c_void_p, C.POINTER(Hooks),
                                           C.c_uint32, C.c_uint32, C.c_int32]
        self.lib.ae_TurnTowardsY.restype = None
        self.cpu = mr.MipsRam.from_extract()
        if gte is None:
            self.cpu.attach_gte_oracle()
        else:
            self.cpu.gte_backend, self.cpu.gte_state = gte
        self.cpu.coverage = set()
        self.callback = HOOKFN(self.rotate_c)
        self.hooks = Hooks(None, self.callback)
        self.calls = []
        self.counts = {'TurnTowardsY': 0}

    def rotate_c(self, user, ram, angle, matrix):
        # SDK is a named boundary. Execute its original maths in a separate
        # oracle image rather than approximating sine or matrix rounding.
        self.calls.append((angle, matrix))
        oracle = mr.MipsRam.from_extract()
        offset = matrix - mr.SCRATCH_BASE
        oracle.scratch[:] = C.string_at(ram + mr.RAM_SIZE, mr.SCRATCH_SIZE)
        result = oracle.call(0x80030890, (angle, matrix))[2]
        C.memmove(ram + mr.RAM_SIZE + offset, bytes(oracle.scratch[offset:offset+32]), 32)
        return mr.signed(result)

    def rotate_mips(self, cpu, regs):
        self.mcalls.append((regs[4], regs[5]))
        oracle = mr.MipsRam.from_extract()
        oracle.scratch[:] = cpu.scratch
        result = oracle.call(0x80030890, (regs[4], regs[5]))[2]
        cpu.scratch[:] = oracle.scratch
        return result

    def run(self, image, scratch, args):
        self.cpu.ram[:] = image
        self.cpu.scratch[:] = scratch
        C.memmove(self.ram, bytes(image), mr.RAM_SIZE)
        C.memmove(self.ram + mr.RAM_SIZE, bytes(scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)
        self.calls, self.mcalls = [], []
        self.cpu.call(0x80066C90, args, hooks={0x80030890: self.rotate_mips})
        self.lib.ae_TurnTowardsY(self.ram, C.byref(self.hooks), *args)
        if self.lib.ae_ram_failed(self.ram):
            raise AssertionError('C port set error flag')
        assert self.calls == self.mcalls, (args, self.calls, self.mcalls)
        stack = mr.STACK_WINDOW[0] - 0x80000000
        actual = C.string_at(self.ram, mr.RAM_SIZE)
        expected = bytes(self.cpu.ram)
        if actual[:stack] != expected[:stack]:
            index = next(i for i in range(stack) if actual[i] != expected[i])
            raise AssertionError(f'RAM mismatch at {index+0x80000000:08X}; args={args}')
        assert actual[mr.STACK_WINDOW[1]-0x80000000:] == expected[mr.STACK_WINDOW[1]-0x80000000:]
        assert C.string_at(self.ram+mr.RAM_SIZE, mr.SCRATCH_SIZE) == bytes(self.cpu.scratch), args
        self.counts['TurnTowardsY'] += 1

    def close(self):
        self.lib.ae_ram_destroy(self.ram)


def cases(seed, scale):
    rng = random.Random(seed)
    edges = [((4096, 0, 0), (4096, 0, 0), 0),
             ((4096, 0, 0), (0, 0, 4096), 113),
             ((4096, 0, 0), (0, 0, -4096), 341),
             ((0, 0, 4096), (0, 0, -4096), 56),
             ((4096, 0, 0), (1, 0, 0), 0),
             ((4096, 0, 0), (1, 0, 1), 0),
             ((4096, 0, 0), (1, 0, -1), 0),
             ((-32768, -32768, -32768), (-32768, -32768, -32768), 0),
             ((1, 0, 0), (-1, 0, 0), 1024)]
    for member in AREAS:
        cpu = mr.MipsRam.from_extract(ROOT/'extract/members'/member)
        for index in range(len(edges) + max(1, 100*scale//100)):
            if index < len(edges): target, direction, angle = edges[index]
            else:
                target = tuple(rng.randint(-32768, 32767) for _ in range(3))
                direction = tuple(rng.randint(-32768, 32767) for _ in range(3))
                angle = rng.choice([0, 1, 56, 113, 341, 1024, 2048, -1, -341,
                                    0x7FFFFFFF, -0x80000000, rng.randint(-8192, 8192)])
            cpu.ram[:] = mr.MipsRam.from_extract(ROOT/'extract/members'/member).ram
            cpu.scratch[:] = rng.randbytes(mr.SCRATCH_SIZE)
            target_address = 0x80120000
            direction_address = 0x80120010
            # Aliasing includes the scratch matrix reset and partial vector overlap.
            if index >= len(edges):
                target_address = rng.choice([target_address, 0x1F800028, 0x1F800030])
                direction_address = rng.choice([direction_address, target_address,
                                                 target_address+4, 0x1F800028])
            for base, vector in ((target_address, target), (direction_address, direction)):
                for axis, value in enumerate((*vector, rng.getrandbits(16))):
                    cpu.write16(base+axis*2, value & 0xFFFF)
            yield bytes(cpu.ram), bytes(cpu.scratch), (target_address, direction_address, angle)


MUTATIONS = [
    ('threshold', '(dot >> 12) < ae_sdk_rcos', '(dot >> 12) <= ae_sdk_rcos'),
    ('dot rounding', '(dot >> 12)', '(dot / 4096)'),
    ('dot sign', '(uint32_t)(target_z * direction_z)', '(uint32_t)(-target_z * direction_z)'),
    ('cross direction', 'cross / 4096 > 0', 'cross / 4096 < 0'),
    ('cross zero', 'cross / 4096 > 0', 'cross / 4096 >= 0'),
    ('angle sign', '(int32_t)(0u - (uint32_t)max_angle)', 'max_angle'),
    ('matrix init', 'ae_sdk_MatrixIdentity(r, TURN_MATRIX);', '/* omitted matrix initialization */'),
    ('matrix address', '#define TURN_MATRIX 0x1F800028u', '#define TURN_MATRIX 0x1F800048u'),
    ('copy padding', 'i < SVECTOR_SIZE; ++i)\n            ae_wr8', 'i < SVECTOR_SIZE - 2; ++i)\n            ae_wr8'),
    ('copy source', 'copy[i] = (uint8_t)ae_rd8(r, target + i)', 'copy[i] = (uint8_t)ae_rd8(r, direction + i)'),
    ('rotate target', 'TURN_MATRIX, direction, direction);', 'TURN_MATRIX, target, direction);'),
    ('matrix alias reload', 'target_x = ae_rds16(r, target + VECTOR_X);\n        target_z', 'target_x = target_x;\n        target_z'),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cases', type=int, default=100)
    parser.add_argument('--seed', type=int, default=60780)
    parser.add_argument('--coverage', action='store_true')
    parser.add_argument('--mutate', action='store_true')
    args = parser.parse_args()
    reg.declare('stack_window', '801FF000..80200000')
    reg.declare('shared_random', 'PlayerUpdate unwritten local normal and position padding')
    reg.declare('shared_random', 'PlayerUpdate PlayPlayerVoice fallback argument')
    reg.declare('shared_random', 'PlayerUpdate actor camera-script table')
    scenarios = list(cases(args.seed, args.cases))
    harness = Harness()
    try:
        for scenario in scenarios: harness.run(*scenario)
        summary = reg.coverage_summary([(n, a, s, True) for n, (a, s) in FUNCTIONS.items()],
                                       harness.cpu.coverage)
        reg.print_coverage(summary)
        print(f"TurnTowardsY: {harness.counts['TurnTowardsY']} cases passed")
        import playerupdate_whole as whole
        whole_harness = whole.Harness(gte=(harness.cpu.gte_backend,harness.cpu.gte_state))
        try:
            import itertools
            whole_cases = list(itertools.chain(whole.scenarios(args.seed,args.cases),whole.focused_scenarios()))
            for scenario in whole_cases: whole_harness.run(*scenario)
            summary.update(reg.coverage_summary([('PlayerUpdate',0x80060780,11944,True)],whole_harness.cpu.coverage))
            assert not summary['PlayerUpdate']['uncovered_count'],summary['PlayerUpdate']
            assert not summary['PlayerUpdate']['stale_declared'],summary['PlayerUpdate']
            harness.counts['PlayerUpdate']=whole_harness.count
            reg.print_coverage({'PlayerUpdate':summary['PlayerUpdate']})
            print(f'PlayerUpdate: {whole_harness.count} whole-function cases passed')
        finally:whole_harness.close()
        if args.cases == 100 and args.seed == 60780 and not args.mutate:
            reg.write_results(harness.counts, summary, True)
    finally:
        harness.close()
    if args.mutate:
        source = (ROOT/'src/player/ae_playerupdate.c').read_text()
        for index, (label, before, after) in enumerate(MUTATIONS):
            assert before in source, label
            path = ROOT/'extract/checks'/f'playerupdate_mutant_{index}.c'
            path.write_text(source.replace(before, after, 1))
            mutant = Harness(path, f'mutant_{index}',
                             (harness.cpu.gte_backend, harness.cpu.gte_state))
            try:
                for scenario in scenarios: mutant.run(*scenario)
            except AssertionError:
                print(f'caught: {label}')
            else:
                raise AssertionError(f'mutation survived: {label}')
            finally:
                mutant.close()
        print(f'{len(MUTATIONS)} mutations caught')
        whole.mutate(whole_cases,(harness.cpu.gte_backend,harness.cpu.gte_state))


if __name__ == '__main__':
    main()
