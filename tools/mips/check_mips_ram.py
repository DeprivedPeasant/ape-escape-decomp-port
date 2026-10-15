"""Self-test of the MIPS-I harness tools/mips/mips_ram.py with hand-assembled programs (no game data needed).

Covers the points the differential checks depend on: load delay slots (old value visible in the slot, a write to the
same register in the slot wins), branch delay slots, jal / jalr / jr, lwl / lwr merging (also with an in-flight load),
swl / swr, mult / div edge cases (divide by zero, INT_MIN / -1), overflow traps of add / sub / addi, signed and
unsigned compares, function hooks, mid-function patches, and the memory map (mirrors, scratchpad, faults).
"""
import sys
import os
import random
import ctypes as C
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mips_ram as mr  # noqa: E402
import exclusion_registry as excl  # noqa: E402

CODE = 0x80100000
DATA = 0x80110000
reg = {n: i for i, n in enumerate(mr._R)}


def R(op, rs, rt, rd, sh, fn): return (op << 26) | (reg[rs] << 21) | (reg[rt] << 16) | (reg[rd] << 11) | (sh << 6) | fn
def I(op, rs, rt, imm): return (op << 26) | (reg[rs] << 21) | (reg[rt] << 16) | (imm & 0xFFFF)
def addu(d, s, t): return R(0, s, t, d, 0, 33)
def add(d, s, t): return R(0, s, t, d, 0, 32)
def sub(d, s, t): return R(0, s, t, d, 0, 34)
def subu(d, s, t): return R(0, s, t, d, 0, 35)
def jr(s): return R(0, s, "zero", "zero", 0, 8)
def jalr(d, s): return R(0, s, "zero", d, 0, 9)
def mult(s, t): return R(0, s, t, "zero", 0, 24)
def multu(s, t): return R(0, s, t, "zero", 0, 25)
def div(s, t): return R(0, s, t, "zero", 0, 26)
def divu(s, t): return R(0, s, t, "zero", 0, 27)
def mflo(d): return R(0, "zero", "zero", d, 0, 18)
def mfhi(d): return R(0, "zero", "zero", d, 0, 16)
def slt(d, s, t): return R(0, s, t, d, 0, 42)
def sltu(d, s, t): return R(0, s, t, d, 0, 43)
def sra(d, t, sh): return R(0, "zero", t, d, sh, 3)
def addiu(t, s, imm): return I(9, s, t, imm)
def addi(t, s, imm): return I(8, s, t, imm)
def lw(t, off, s): return I(35, s, t, off)
def lh(t, off, s): return I(33, s, t, off)
def lbu(t, off, s): return I(36, s, t, off)
def lwl(t, off, s): return I(34, s, t, off)
def lwr(t, off, s): return I(38, s, t, off)
def sw(t, off, s): return I(43, s, t, off)
def swl(t, off, s): return I(42, s, t, off)
def swr(t, off, s): return I(46, s, t, off)
def beq(s, t, off): return I(4, s, t, off)
def bne(s, t, off): return I(5, s, t, off)
def jal(target): return (3 << 26) | ((target >> 2) & 0x3FFFFFF)
NOP = 0


def run(words, args=(), setup=None, **kw):
    cpu = mr.MipsRam()
    for i, w in enumerate(words):
        cpu.write32(CODE + 4 * i, w)
    if setup:
        setup(cpu)
    return cpu, cpu.call(CODE, args, **kw)


def expect(name, got, want):
    if got != want:
        print(f"FAIL {name}: got {got:#x} want {want:#x}")
        return 1
    return 0


def main():
    bad = 0
    S = lambda c: c.write32(DATA, 0x11223344) or c.write32(DATA + 4, 0x55667788)  # noqa: E731
    # load delay: the slot sees the OLD value, the instruction after sees the new one
    cpu, r = run([addu("t0", "a1", "zero"), lw("t0", 0, "a0"), addu("v0", "t0", "zero"), jr("ra"), addu("v1", "t0", "zero")],
                 (DATA, 0xAAAA), S)
    bad += expect("load delay slot sees old", r[2], 0xAAAA) + expect("load result visible after", r[3], 0x11223344)
    # a write to the same register in the delay slot wins over the load
    cpu, r = run([lw("t0", 0, "a0"), addiu("t0", "zero", 5), jr("ra"), addu("v0", "t0", "zero")], (DATA,), S)
    bad += expect("same-register write beats load", r[2], 5)
    # branch delay slot executes, taken branch skips the next instruction
    cpu, r = run([beq("zero", "zero", 2), addiu("v0", "zero", 1), addiu("v0", "zero", 2), jr("ra"), addiu("v1", "zero", 7)])
    bad += expect("branch delay slot", r[2], 1) + expect("jr delay slot", r[3], 7)
    # not-taken branch falls through (and still runs the delay slot)
    cpu, r = run([bne("zero", "zero", 5), addiu("v0", "zero", 1), addiu("v0", "v0", 10), jr("ra"), NOP])
    bad += expect("not taken", r[2], 11)
    # jal / ra
    cpu, r = run([addiu("sp", "sp", -8), sw("ra", 0, "sp"), jal(CODE + 0x40), addiu("a0", "zero", 6), lw("ra", 0, "sp"), addiu("sp", "sp", 8),
                  jr("ra"), NOP] + [NOP] * 8 + [addu("v0", "a0", "a0"), jr("ra"), NOP])
    bad += expect("jal", r[2], 12)
    # lwr / lwl unaligned load of bytes 1..4 (also lwl directly in the lwr delay slot)
    cpu, r = run([lwr("t0", 1, "a0"), lwl("t0", 4, "a0"), NOP, jr("ra"), addu("v0", "t0", "zero")], (DATA,), S)
    bad += expect("lwr/lwl", r[2], int.from_bytes(bytes(cpu.read_bytes(DATA + 1, 4)), "little"))
    # swl / swr unaligned store
    def S2(c):
        c.write32(DATA + 16, 0xFFFFFFFF)
        c.write32(DATA + 20, 0xFFFFFFFF)
    cpu, r = run([addiu("t0", "zero", 0x1234), lui_(0x5678, "t1"), swr("t1", 1, "a0"), swl("t1", 4, "a0"), jr("ra"), NOP], (DATA + 16,), S2)
    # expected: little-endian bytes of t1 stored at +1..+4
    t1 = 0x56780000
    want = bytearray(b"\xff" * 8)
    want[1:5] = t1.to_bytes(4, "little")
    bad += expect("swr/swl", int.from_bytes(cpu.read_bytes(DATA + 16, 8), "little"), int.from_bytes(bytes(want), "little"))
    # mult / div
    cpu, r = run([mult("a0", "a1"), mflo("v0"), mfhi("v1"), jr("ra"), NOP], (0xFFFFFFFE, 0x40000000))
    bad += expect("mult lo", r[2], (-2 * 0x40000000) & 0xFFFFFFFF) + expect("mult hi", r[3], 0xFFFFFFFF)
    cpu, r = run([multu("a0", "a1"), mflo("v0"), mfhi("v1"), jr("ra"), NOP], (0xFFFFFFFE, 0x40000000))
    bad += expect("multu hi", r[3], (0xFFFFFFFE * 0x40000000) >> 32)
    cpu, r = run([div("a0", "a1"), mflo("v0"), mfhi("v1"), jr("ra"), NOP], (7, 0))
    bad += expect("div by zero lo (positive)", r[2], 0xFFFFFFFF) + expect("div by zero hi", r[3], 7)
    cpu, r = run([div("a0", "a1"), mflo("v0"), mfhi("v1"), jr("ra"), NOP], (0xFFFFFFF9, 0))
    bad += expect("div by zero lo (negative)", r[2], 1)
    cpu, r = run([div("a0", "a1"), mflo("v0"), mfhi("v1"), jr("ra"), NOP], (0x80000000, 0xFFFFFFFF))
    bad += expect("INT_MIN / -1 lo", r[2], 0x80000000) + expect("INT_MIN / -1 hi", r[3], 0)
    cpu, r = run([div("a0", "a1"), mflo("v0"), mfhi("v1"), jr("ra"), NOP], (0xFFFFFFF9, 2))      # -7 / 2 = -3 r -1
    bad += expect("div truncates toward zero", r[2], 0xFFFFFFFD) + expect("div remainder sign", r[3], 0xFFFFFFFF)
    # compares and shifts
    cpu, r = run([slt("v0", "a0", "a1"), sltu("v1", "a0", "a1"), jr("ra"), NOP], (0xFFFFFFFF, 1))
    bad += expect("slt", r[2], 1) + expect("sltu", r[3], 0)
    cpu, r = run([sra("v0", "a0", 4), jr("ra"), NOP], (0x80000010,))
    bad += expect("sra", r[2], 0xF8000001)
    # overflow traps
    for nm, prog in (("add", [add("v0", "a0", "a1")]), ("sub", [sub("v0", "a0", "a1")]), ("addi", [addi("v0", "a0", 1)])):
        try:
            run(prog + [jr("ra"), NOP], (0x7FFFFFFF if nm != "sub" else 0x80000000, 1))
            print(f"FAIL {nm} overflow did not trap")
            bad += 1
        except mr.MipsError:
            pass
    cpu, r = run([addu("v0", "a0", "a1"), jr("ra"), NOP], (0x7FFFFFFF, 1))
    bad += expect("addu wraps", r[2], 0x80000000)
    # function hook (replaces a callee) and mid-function patch
    prog = [addiu("sp", "sp", -8), sw("ra", 0, "sp"), jal(0x80100200), addiu("a0", "zero", 3), lw("ra", 0, "sp"), addiu("sp", "sp", 8),
            jr("ra"), addiu("v1", "v0", 1)]
    cpu, r = run(prog, hooks={0x80100200: lambda c, regs: regs[4] * 10})
    bad += expect("hook result in v0", r[2], 30) + expect("code after hook", r[3], 31)
    bad += expect("hook logged", cpu.hook_log[0][0], 0x80100200) + expect("hook arg logged", cpu.hook_log[0][1], 3)
    cpu, r = run([addiu("v0", "zero", 1), addiu("v0", "v0", 1), addiu("v0", "v0", 1), jr("ra"), NOP],
                 patches={CODE + 4: lambda c, regs: CODE + 12})
    bad += expect("patch skips an instruction", r[2], 1)
    # memory map
    cpu = mr.MipsRam()
    cpu.write32(0x80123450, 0xCAFEBABE)
    bad += expect("KUSEG mirror", cpu.read32(0x00123450), 0xCAFEBABE) + expect("KSEG1 mirror", cpu.read32(0xA0123450), 0xCAFEBABE)
    cpu.write32(0x1F800010, 0x12345678)
    bad += expect("scratchpad", cpu.read32(0x1F800010), 0x12345678)
    for addr, size in ((0x80200000, 4), (0x1F800400, 4), (0x80000001, 4), (0x80000001, 2)):
        try:
            cpu.read(addr, size)
            print(f"FAIL read {addr:08X}/{size} should fault")
            bad += 1
        except mr.MipsError:
            pass
    # instruction listing
    bad += expect("disasm", int(mr.disasm(addu("v0", "a0", "a1"), 0).startswith("addu $v0, $a0, $a1")), 1)
    print("mips_ram self-test:", "FAILED" if bad else "ok")
    if not bad:
        backend_equivalence()
    excl.write_results({}, None, not bad)        # no ported functions: the summary only records pass / fail for the ledger
    return 1 if bad else 0


def lui_(imm, t): return I(15, "zero", t, imm)


def backend_equivalence():
    """Compare each executed prefix, including stopped branch/load state.

    Fixed seeds make failures reproducible. Every implemented decoder arm appears
    explicitly, then random streams combine arms and edge operands. Mutated DLLs
    are isolated build outputs and never replace the production source/library.
    """
    import mips_fast as fast
    import subprocess
    import shutil
    rng = random.Random(0x3000a)
    cpu = mr.MipsRam().attach_gte_oracle()
    cpu.coverage = set()
    edges = [0, 1, 0xffffffff, 0x80000000, 0x7fffffff, 0xffff8000, 31, 32]
    functions = sorted(mr._SPECIAL)
    ops = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
           32, 33, 34, 35, 36, 37, 38, 40, 41, 42, 43, 46, 50, 58]
    instructions = [(8<<21)|(9<<16)|(10<<11)|(rng.randrange(32)<<6)|fn for fn in functions]
    instructions += [(op<<26)|(4<<21)|(8<<16)|rng.choice([0, 1, 2, 3, 0xffff, 0x8000]) for op in ops if op != 1]
    instructions += [(1<<26)|(8<<21)|(rt<<16)|1 for rt in [0, 1, 16, 17, 3]]
    instructions += [(18<<26)|(rs<<21)|(8<<16)|(rd<<11) for rs in [0, 2, 4, 6] for rd in [0, 15, 28, 31]]
    instructions += [0x4a180001, 0x4a780010, 0x4a58002d, 0x4a68002e,
                     0x0000003f, 0xfc000000]
    count = 0
    old_backend = os.environ.get('MIPS_BACKEND')
    os.environ['MIPS_BACKEND'] = 'both'
    def check(words, args=(DATA,), initial=None, hooks=None, patches=None, prefixes=True, root=CODE):
        nonlocal count
        cpu.write_bytes(CODE, b'\0'*256)
        cpu._decoded.clear()  # each generated stream is a new code image
        for i, word in enumerate(words):
            cpu.write32(CODE+4*i, word)
        baseline = cpu.snapshot()
        for budget in range(1, len(words)+3) if prefixes else [len(words)+3]:
            cpu.restore(baseline)
            cpu.gte_reset()
            cpu.coverage.clear()
            try:
                cpu.call(root, args, hooks=hooks, patches=patches, limit=budget, initial_regs=initial)
            except mr.MipsError as error:
                if str(error) == 'instruction budget exceeded':
                    # _both_call compared visible state; independently compare
                    # pending state at this exact instruction boundary as well.
                    c_machine = cpu._last_machine
                    snapshot = cpu.snapshot()
                    saved_steps = cpu.steps
                    cpu.restore(baseline)
                    cpu.gte_reset()
                    try:
                        cpu._reference_call(root, args=args, hooks=hooks, patches=patches,
                                            limit=budget, initial_regs=initial)
                    except mr.MipsError:
                        assert cpu._last_machine == c_machine, 'pending branch/load difference'
                    cpu.restore(snapshot)
                    cpu.steps = saved_steps
            except AssertionError as error:
                raise AssertionError(f'words={[hex(w) for w in words]} budget={budget} initial={initial}: {error}') from error
            count += 1
    try:
        cpu.write_bytes(DATA, bytes(range(128, 192)))
        for ins in instructions:
            for edge in [0, 0x80000000, 0xffffffff]:
                check([ins, mflo('v0'), mfhi('v1'), jr('ra'), NOP],
                      initial={8: edge, 9: rng.choice(edges)})
        safe = [ins for ins in instructions if ins>>26 not in (1, 2, 3, 16, 18) and
                not (ins>>26 == 0 and ins&63 in (8, 9, 12, 13, 63))]
        for _ in range(120):
            words = rng.choices(safe, k=10)+[jr('ra'), NOP]
            check(words, initial={8: rng.choice(edges), 9: rng.choice(edges)})
        for op in (32, 33, 35, 36, 37, 40, 41, 43, 34, 38, 42, 46):
            for addr in [0x110000, 0xa0110000, 0x1f800000, 0x1f8003ff,
                         0x801fffff, 0x80200000, 0x1f800400, DATA+1, DATA+2, DATA+3]:
                check([(op<<26)|(4<<21)|(8<<16), NOP, jr('ra'), NOP], (addr,))
        # Adjacent merge loads, overwritten pending loads and branch-in-delay behavior.
        for offset in range(4):
            check([lwl('t0', offset, 'a0'), lwr('t0', offset, 'a0'),
                   addu('v0', 't0', 'zero'), jr('ra'), NOP])
        check([beq('zero', 'zero', 2), beq('zero', 'zero', 3), NOP,
               addiu('v0', 'zero', 7), jr('ra'), NOP, jr('ra'), NOP])
        def hook(c, r):
            c.write32(DATA+32, r[8])
            r[0] = 123  # hooks may write zero; it resets only on instruction execution
            r[9] = 0x87654321
            return (17, 19)
        check([lw('t0', 0, 'a0'), NOP, jr('ra'), NOP],
              hooks={CODE+8: hook}, prefixes=False)
        check([lw('t0', 0, 'a0'), NOP, jr('ra'), NOP],
              hooks={CODE+4: hook}, prefixes=False)
        check([jr('ra'), lw('v0', 0, 'a0')])  # sentinel commits pending load
        check([lw('t0', 0, 'a0'), NOP, jr('ra'), NOP],
              patches={CODE+8: lambda c, r: hook(c, r) and CODE+12}, prefixes=False)
        check([beq('zero', 'zero', 1), NOP, jr('ra'), NOP], hooks={CODE+4: hook})
        check([jr('ra'), NOP], (1, 2, 3, 4, 5, 6), {2: 99})
        for root in [0x00100000, 0xa0100000]:
            check([addiu('v0', 'zero', 17), jr('ra'), NOP], root=root)
        for i, word in enumerate([addiu('v0', 'zero', 23), jr('ra'), NOP]):
            cpu.write32(mr.SCRATCH_BASE+4*i, word)
        cpu._decoded.clear()
        assert cpu.call(mr.SCRATCH_BASE)[2] == 23
        count += 1
        def failing_hook(c, r):
            c.write32(DATA+40, 0x12345678)
            r[8] = 19
            raise mr.MipsError('deliberate hook exception')
        check([NOP, jr('ra'), NOP], hooks={CODE: failing_hook}, prefixes=False)
        def raw_register_hook(c, r):
            r[8], r[9] = -1, 1 << 40
            return None
        check([jr('ra'), NOP], hooks={CODE: raw_register_hook}, prefixes=False)
        check([NOP, addu('v0', 't0', 'zero'), jr('ra'), NOP],
              patches={CODE: lambda c, r: raw_register_hook(c, r) or CODE+4})
        def raw_failing_hook(c, r):
            raw_register_hook(c, r)
            raise mr.MipsError('raw register hook exception')
        check([jr('ra'), NOP], hooks={CODE: raw_failing_hook}, prefixes=False)
        check([NOP, jr('ra'), NOP],
              hooks={CODE+4: lambda c, r: c.steps+len(c.coverage)}, prefixes=False)
        def reset_observers(c, r):
            c.steps = 0
            c.coverage.clear()
            return CODE+8
        check([NOP, NOP, jr('ra'), NOP], patches={CODE+4: reset_observers})
        # Hooks receive the same live register list across interceptions and at
        # return, including when an earlier hook retained an alias to that list.
        retained = []
        def retain_regs(c, r):
            retained.append(r)
            return CODE+4
        def edit_retained(c, r):
            retained[0][8] = 123
            return CODE+8
        cpu._decoded.clear()
        for i, word in enumerate([NOP, NOP, jr('ra'), NOP]):
            cpu.write32(CODE+4*i, word)
        os.environ['MIPS_BACKEND'] = 'c'
        returned = cpu.call(CODE, patches={CODE: retain_regs, CODE+4: edit_retained})
        assert returned is retained[0] and returned[8] == 123
        count += 1
        retained.clear()
        os.environ['MIPS_BACKEND'] = 'both'
        returned = cpu.call(CODE, patches={CODE: retain_regs, CODE+4: edit_retained})
        assert returned is retained[0] and returned[8] == 123
        count += 1
        # No oracle must produce the same fail-closed errors.
        saved = cpu.gte_backend
        cpu.gte_backend = None
        for ins in [0x48081000, 0x4a180001, I(50, 'a0', 't0', 0), I(58, 'a0', 't0', 0)]:
            check([ins, jr('ra'), NOP])
        cpu.gte_backend = saved
        cpu.gte_backend = lambda ins, value: value ^ 0x12345678
        check([0x48081000, NOP, jr('ra'), NOP])
        def failing_gte(ins, value):
            raise mr.MipsError('deliberate custom GTE exception')
        cpu.gte_backend = failing_gte
        for ins in [0x48081000, 0x4a180001, I(50, 'a0', 't0', 0), I(58, 'a0', 't0', 0)]:
            check([ins, jr('ra'), NOP])
        cpu.gte_backend = saved
        original_write = cpu.write
        memory_log = []
        def intercepted_write(address, size, value):
            memory_log.append((address, size, value))
            if address == 0x1f801c00:
                return None
            return original_write(address, size, value)
        # Initialize before installing the override, then verify instruction and
        # hook writes both preserve the single-call external callback behavior.
        cpu._decoded.clear()
        for i, word in enumerate([sw('a1', 0, 'a0'), jr('ra'), NOP]):
            cpu.write32(CODE+4*i, word)
        cpu.write = intercepted_write
        try:
            cpu.call(CODE, (0x1f801c00, 0x12345678))
            assert memory_log == [(0x1f801c00, 4, 0x12345678)]
            memory_log.clear()
            cpu.call(CODE, (DATA, 0x87654321))
            assert memory_log == [(DATA, 4, 0x87654321)]
            memory_log.clear()
            cpu.call(CODE, hooks={CODE: lambda c, r: c.write32(DATA, 123)})
            assert memory_log == [(DATA, 4, 123)]
            memory_log.clear()
            try:
                cpu.call(CODE, (0x1f900000, 1))
            except mr.MipsError as error:
                assert 'unmapped address' in str(error)
            else:
                raise AssertionError('overridden write error did not propagate')
            assert memory_log == [(0x1f900000, 4, 1)]
            count += 4
        finally:
            cpu.write = original_write
        # Cache policy survives RAM writes and backend changes; explicit clear
        # causes the next invocation to fetch the replacement code.
        cpu._decoded.clear()
        for i, word in enumerate([addiu('v0', 'zero', 7), jr('ra'), NOP]):
            cpu.write32(CODE+4*i, word)
        for first, second in [('c', 'py'), ('py', 'c'), ('both', 'c')]:
            cpu._decoded.clear()
            cpu.write32(CODE, addiu('v0', 'zero', 7))
            os.environ['MIPS_BACKEND'] = first
            assert cpu.call(CODE)[2] == 7
            cpu.write32(CODE, addiu('v0', 'zero', 19))
            os.environ['MIPS_BACKEND'] = second
            assert cpu.call(CODE)[2] == 7
            cpu._decoded.clear()
            assert cpu.call(CODE)[2] == 19
            count += 3
        os.environ['MIPS_BACKEND'] = 'both'
        print(f'backend equivalence: {count} instruction-prefix cases passed')
        production = fast.build()
        source = Path(fast.__file__).with_suffix('.c').read_text()
        mutations = {
            'load_delay': ('s->load_value=(v);', 's->load_value=(v); s->r[rt]=s->load_value;'),
            'lh_sign': ('(uint32_t)(int32_t)(int16_t)readmem', '(uint32_t)(uint16_t)readmem'),
            'overflow': ('wide > INT32_MAX', 'wide > INT64_MAX'),
        }
        probes = {
            'load_delay': ([lw('t0', 0, 'a0'), addu('v0', 't0', 'zero'), jr('ra'), addu('v1', 't0', 'zero')], (DATA,)),
            'lh_sign': ([lh('t0', 0, 'a0'), NOP, jr('ra'), addu('v0', 't0', 'zero')], (DATA,)),
            'overflow': ([add('v0', 'a0', 'a1'), jr('ra'), NOP], (0x7fffffff, 1)),
        }
        out = mr.ROOT / 'extract/mips'
        for name, (old, new) in mutations.items():
            assert old in source
            path = out / f'mutation_{name}.c'
            path.write_text(source.replace(old, new))
            dll = out / f'mutation_{name}_{os.getpid()}.dll'
            subprocess.run([shutil.which('gcc'), '-O2', '-shared', str(path), '-o', str(dll)], check=True)
            mutant = C.CDLL(str(dll))
            mutant.mips_run.argtypes = production.mips_run.argtypes
            mutant.mips_run.restype = C.c_int
            fast._lib = mutant
            try:
                cpu.write32(DATA, 0x81828384)
                check(*probes[name], prefixes=False)
            except AssertionError:
                print(f'backend mutation caught: {name}')
            else:
                raise AssertionError(f'uncaught C mutation: {name}')
            finally:
                fast._lib = production
    finally:
        if old_backend is None:
            os.environ.pop('MIPS_BACKEND', None)
        else:
            os.environ['MIPS_BACKEND'] = old_backend


if __name__ == "__main__":
    sys.exit(main())
