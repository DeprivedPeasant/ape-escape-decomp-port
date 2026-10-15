"""Diagnostic (not a check): finds out which results of an original monkey function depend on UNINITIALISED STACK contents, and
where the leftover value comes from.

For a scenario the original function is run on the MIPS harness twice with the whole stack window (0x801FF000..0x80200000)
filled with two different random patterns.  Every RAM byte outside the stack window (and every hook argument) that differs
between the two runs was computed from stack contents that the function chain (the function and its callees) never wrote:
its value depends on code that ran before the function (an unknown earlier caller).  Bytes that agree are determined by the
chain itself: `provenance` then names the instruction that wrote the stack word that was read (function, address, register).

Usage from a check or by hand:  from monkey_stack_probe import StackProbe;  StackProbe(h).run(name, setup, argsf)
"""
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import monkey_harness as mh  # noqa: E402
import mips_ram as mr  # noqa: E402

M32 = 0xFFFFFFFF
STACK_LO, STACK_HI = mh.STACK_LO, mh.STACK_HI


class StackProbe:
    def __init__(self, h):
        self.h = h
        self.fn_starts = None

    def _fn_name(self, pc):
        if self.fn_starts is None:
            rows = []
            for line in (mh.ROOT / "notes/inventory/functions.csv").read_text(encoding="utf-8").splitlines()[1:]:
                p = line.split(",")
                try:
                    rows.append((int(p[0], 16), p[1]))
                except ValueError:
                    pass
            self.fn_starts = sorted(rows)
        best = None
        for a, n in self.fn_starts:
            if a <= pc:
                best = (a, n)
            else:
                break
        return best

    def run_once(self, name, setup, argsf, pattern_seed, trace=False):
        h = self.h
        m = h.m
        m.restore(h.base)
        m.gte_reset()
        ctx = setup(m)
        args = argsf(ctx)
        g = random.Random(pattern_seed)
        m.write_bytes(STACK_LO, g.randbytes(STACK_HI - STACK_LO))
        model = mh.Model(1, mh.MipsMem(m), 0)
        model.ctx = ctx
        m.hook_log.clear()
        writes = []
        if trace:
            orig = mr.MipsRam.write

            def tw(self, address, size, value, _orig=orig):
                f = sys._getframe(1)
                pc = f.f_locals.get("pc")
                if STACK_LO <= (address & M32) < STACK_HI:
                    writes.append((pc, address & M32, size, value & ((1 << (8 * size)) - 1)))
                return _orig(self, address, size, value)
            mr.MipsRam.write = tw
        try:
            try:
                regs = m.call(h.funcs[name][0], args, hooks=h.mips_hooks(model))
                ret = regs[2]
            except mr.MipsError as e:
                ret = "trap " + str(e)
        finally:
            if trace:
                mr.MipsRam.write = orig
        ram = bytes(m.ram)[:STACK_LO - 0x80000000]
        return ram, ret, list(model.log), writes

    def run(self, name, setup, argsf, seeds=(0xA5A5, 0x5A5A), verbose=False):
        """returns (differing RAM addresses, return values differ?, hook logs differ?) between the two stack fills"""
        r1 = self.run_once(name, setup, argsf, seeds[0])
        r2 = self.run_once(name, setup, argsf, seeds[1])
        diffs = [0x80000000 + i for i in range(len(r1[0])) if r1[0][i] != r2[0][i]]
        return diffs, r1[1] != r2[1], r1[2] != r2[2]

    def provenance(self, name, setup, argsf, address, seed=0xA5A5):
        """the last stack write to the word at `address` before the end of the run: (pc, function, size, value) or None"""
        ram, ret, log, writes = self.run_once(name, setup, argsf, seed, trace=True)
        hits = [w for w in writes if w[1] <= address < w[1] + w[2]]
        if not hits:
            return None
        pc, a, sz, val = hits[-1]
        return pc, self._fn_name(pc), sz, val
