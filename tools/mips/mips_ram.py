"""Reusable MIPS-I (R3000A) harness over a byte-exact image of PSX main RAM.

Test-only. It loads the original SCUS_944.23 (and optionally an area member) into a 2 MiB RAM
image at the real addresses (extract/ is read at run time; no game data lives in the repo),
lets a test read / write RAM, runs an original function by address, and intercepts calls to
chosen addresses (or mid-function addresses) with Python hooks.

    cpu = MipsRam.from_extract(area=None)
    cpu.write32(0x800B20D8, 0)
    regs = cpu.call(0x80071d3c, hooks={0x8007228c: lambda cpu, r: 0})

Exactness: load delay slots and branch delay slots follow the R3000 (a load result becomes
visible after the next instruction; lwl/lwr merge with an in-flight load), div/divu by zero and
INT_MIN / -1 give the hardware results, add/sub/addi overflow raises MipsError (the hardware
would trap; the game compiles to addu).  GTE (COP2) is delegated to an optional backend
(`gte_backend(instruction, value) -> result`, e.g. the pinned PCSX-ReARMed oracle of
tools/codex/gte_oracle.py); without a backend any COP2 instruction raises MipsError.

Hooks (two kinds, both keyed by address):
  hooks    {entry_address: fn(cpu, regs) -> int | (v0, v1) | None}   replaces a whole function:
           when the program counter reaches the address the hook runs, v0 (and v1) are set from
           its result and execution continues at $ra.  This is how calls to functions outside
           the task are intercepted (the delay slot of the calling jal has already executed).
  patches  {address: fn(cpu, regs) -> new_pc}   intercept in the middle of a function: the hook
           runs instead of the instruction at the address and returns the pc to continue at.
Every hook invocation is appended to cpu.hook_log as (address, a0, a1, a2, a3) before it runs.
"""
import struct
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXE_PATH = ROOT / "extract/disc/SCUS_944.23"
AREA_BASE = 0x80154000
SENTINEL = 0xFFFFFFF0          # return address of a top-level call()
RAM_SIZE = 0x200000
SCRATCH_BASE = 0x1F800000
SCRATCH_SIZE = 0x400
DEFAULT_SP = 0x801FFF00        # top-level stack (callers compare RAM excluding STACK_WINDOW)
STACK_WINDOW = (0x801FF000, 0x80200000)


class MipsError(Exception):
    pass


class _DecodedCache(dict):
    """Track cache edits so the native backend shares the reference cache policy.

    Instruction RAM is normally immutable, but clearing the cache explicitly must
    also invalidate C. Only newly decoded instructions cross the ctypes boundary.
    """
    def __init__(self):
        super().__init__()
        self.generation = 0
        self.changes = []

    def __setitem__(self, pc, decoded):
        super().__setitem__(pc, decoded)
        self.changes.append(pc)

    def clear(self):
        super().clear()
        self.generation += 1
        self.changes.clear()


def signed(v, bits=32):
    v &= (1 << bits) - 1
    return v - (1 << bits) if v & (1 << (bits - 1)) else v


def _div(a, b, unsigned):
    if unsigned:
        a &= 0xFFFFFFFF
        b &= 0xFFFFFFFF
        if b == 0:
            return 0xFFFFFFFF, a
        return a // b, a % b
    a, b = signed(a), signed(b)
    if b == 0:
        return (1 if a < 0 else 0xFFFFFFFF), a & 0xFFFFFFFF
    if a == -0x80000000 and b == -1:
        return 0x80000000, 0
    q = abs(a) // abs(b)
    if (a < 0) != (b < 0):
        q = -q
    return q & 0xFFFFFFFF, (a - q * b) & 0xFFFFFFFF


class MipsRam:
    def __init__(self):
        self.ram = bytearray(RAM_SIZE)
        self.scratch = bytearray(SCRATCH_SIZE)
        self.gte_backend = None
        self.hook_log = []
        self.steps = 0
        self.trace = None
        self.coverage = None        # set() to record executed program counters
        self._decoded = _DecodedCache()

    # ---------------------------------------------------------------- loading
    @classmethod
    def from_extract(cls, area=None, exe_path=EXE_PATH):
        cpu = cls()
        cpu.load_exe(Path(exe_path).read_bytes())
        if area is not None:
            cpu.load_area(area if isinstance(area, (bytes, bytearray)) else Path(area).read_bytes())
        return cpu

    def load_exe(self, exe):
        if exe[:8] != b"PS-X EXE":
            raise MipsError("not a PS-X EXE")
        t_addr, t_size = struct.unpack_from("<II", exe, 0x18)
        off = (t_addr & 0x1FFFFF)
        n = min(t_size, len(exe) - 0x800)
        self.ram[off:off + n] = exe[0x800:0x800 + n]

    def load_area(self, blob, addr=AREA_BASE):
        off = addr & 0x1FFFFF
        if off + len(blob) > RAM_SIZE:
            raise MipsError("area member exceeds RAM")
        self.ram[off:off + len(blob)] = blob

    # ----------------------------------------------------------------- memory
    def _map(self, address, size):
        address &= 0xFFFFFFFF
        if SCRATCH_BASE <= address < SCRATCH_BASE + SCRATCH_SIZE:
            blob, offset = self.scratch, address - SCRATCH_BASE
        else:
            seg = address >> 29
            phys = address & 0x1FFFFFFF
            if seg in (0, 4, 5) and phys < RAM_SIZE:
                blob, offset = self.ram, phys
            else:
                raise MipsError(f"unmapped address {address:08X}")
        if offset + size > len(blob):
            raise MipsError(f"address {address:08X} size {size} out of bounds")
        return blob, offset

    def read(self, address, size):
        blob, offset = self._map(address, size)
        if offset % size:
            raise MipsError(f"unaligned read {address:08X}/{size}")
        return int.from_bytes(blob[offset:offset + size], "little")

    def write(self, address, size, value):
        blob, offset = self._map(address, size)
        if offset % size:
            raise MipsError(f"unaligned write {address:08X}/{size}")
        blob[offset:offset + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")

    def read8(self, a): return self.read(a, 1)
    def read16(self, a): return self.read(a, 2)
    def read32(self, a): return self.read(a, 4)
    def reads8(self, a): return signed(self.read(a, 1), 8)
    def reads16(self, a): return signed(self.read(a, 2), 16)
    def reads32(self, a): return signed(self.read(a, 4), 32)
    def write8(self, a, v): self.write(a, 1, v)
    def write16(self, a, v): self.write(a, 2, v)
    def write32(self, a, v): self.write(a, 4, v)

    def read_bytes(self, address, n):
        blob, offset = self._map(address, n)
        return bytes(blob[offset:offset + n])

    def write_bytes(self, address, data):
        blob, offset = self._map(address, len(data))
        blob[offset:offset + len(data)] = data

    def snapshot(self):
        return bytes(self.ram), bytes(self.scratch)

    def restore(self, snap):
        memoryview(self.ram)[:] = snap[0]
        memoryview(self.scratch)[:] = snap[1]

    # -------------------------------------------------------------- execution
    def _decode(self, pc):
        ins = self.read(pc, 4)
        d = (ins >> 26, ins >> 21 & 31, ins >> 16 & 31, ins >> 11 & 31, ins >> 6 & 31, ins & 63,
             ins & 0xFFFF, signed(ins & 0xFFFF, 16), ins)
        self._decoded[pc] = d
        return d

    def call(self, root, args=(), hooks=None, patches=None, limit=2_000_000, sp=DEFAULT_SP, initial_regs=None):
        backend = os.environ.get('MIPS_BACKEND', 'c')
        kw = dict(args=args, hooks=hooks, patches=patches, limit=limit, sp=sp, initial_regs=initial_regs)
        if backend == 'py':
            return self._reference_call(root, **kw)
        if backend not in ('c', 'both'):
            raise ValueError('MIPS_BACKEND must be c, py or both')
        import mips_fast
        if backend == 'c':
            return mips_fast.call(self, root, **kw)
        return self._both_call(root, kw, mips_fast)

    def _reference_call(self, root, **kw):
        try:
            return self._call_py(root, **kw)
        except BaseException as error:
            tb = error.__traceback__
            while tb:
                if tb.tb_frame.f_code is self._call_py.__func__.__code__:
                    loc = tb.tb_frame.f_locals
                    if 'hi' in loc:
                        self._last_state = (loc['r'][:], loc['hi'], loc['lo'])
                        self._last_machine = (loc['pc'], loc['branch'], loc['pending'])
                tb = tb.tb_next
            raise

    def _both_call(self, root, kw, fast):
        # write() is also an existing interception surface (the sound checker
        # intercepts SPU stores). Replay it just like hooks, once per user effect.
        original_write = self.write
        if getattr(original_write, '__func__', None) is MipsRam.write:
            return self._both_execute(root, kw, fast)
        memory_records = []
        depth = 0
        original_hooks = {kind: kw[kind] for kind in ('hooks', 'patches')}
        def in_hook(fn):
            def wrapped(cpu, regs):
                nonlocal depth
                depth += 1
                try:
                    return fn(cpu, regs)
                finally:
                    depth -= 1
            return wrapped
        wrapped_kw = dict(kw)
        for kind, hooks in original_hooks.items():
            wrapped_kw[kind] = {pc: in_hook(fn) for pc, fn in (hooks or {}).items()}
        def record_write(address, size, value):
            if depth:
                return original_write(address, size, value)
            before = self.snapshot()
            try:
                result = original_write(address, size, value)
            except BaseException as error:
                memory_records.append(((address, size, value), before, self.snapshot(), None, error))
                raise
            memory_records.append(((address, size, value), before, self.snapshot(), result, None))
            return result
        cursor = 0
        def replay_write(address, size, value):
            nonlocal cursor
            if cursor >= len(memory_records):
                raise AssertionError('C backend invoked an extra memory write callback')
            args, before, after, result, error = memory_records[cursor]
            cursor += 1
            if args != (address, size, value) or before != self.snapshot():
                raise AssertionError('backend difference at memory write callback')
            self.restore(after)
            if error is not None:
                raise error
            return result
        self.write = record_write
        try:
            result = self._both_execute(root, wrapped_kw, fast, lambda: setattr(self, 'write', replay_write))
            if cursor != len(memory_records):
                raise AssertionError('C backend missed a memory write callback')
            return result
        finally:
            self.write = original_write

    def _both_execute(self, root, kw, fast, before_c=None):
        # Execute user hooks once. Replay their exact RAM/register changes after
        # verifying the C side reaches each hook with identical observable state.
        before = self.snapshot()
        initial_steps, initial_log = self.steps, self.hook_log[:]
        initial_cov_object = self.coverage
        initial_cov = None if self.coverage is None else self.coverage.copy()
        initial_gte = bytes(self.gte_state) if hasattr(self, 'gte_state') else None
        records = []
        self._last_state = None
        def record(fn):
            def wrapped(cpu, r):
                entry = (r[:], cpu.snapshot(), bytes(cpu.gte_state) if initial_gte is not None else None,
                         cpu.steps, None if cpu.coverage is None else cpu.coverage.copy())
                def public_state():
                    return (cpu.steps, cpu.coverage, None if cpu.coverage is None else cpu.coverage.copy(),
                            cpu.hook_log[:])
                try:
                    result = fn(cpu, r)
                except BaseException as error:
                    records.append((entry, r[:], cpu.snapshot(), None, error,
                                    bytes(cpu.gte_state) if initial_gte is not None else None, public_state()))
                    raise
                records.append((entry, r[:], cpu.snapshot(), result, None,
                                bytes(cpu.gte_state) if initial_gte is not None else None, public_state()))
                return result
            return wrapped
        pykw = dict(kw)
        for kind in ('hooks', 'patches'):
            pykw[kind] = {pc: record(fn) for pc, fn in (kw[kind] or {}).items()}
        def outcome(fn):
            try:
                return fn(), None
            except BaseException as error:
                return None, error
        pyresult, pyerror = outcome(lambda: self._reference_call(root, **pykw))
        expected = (self.snapshot(), self._last_state, self.steps, self.hook_log[:],
                    None if self.coverage is None else self.coverage.copy(),
                    bytes(self.gte_state) if initial_gte is not None else None)
        self.restore(before)
        self._last_state = None
        self.steps, self.hook_log[:] = initial_steps, initial_log
        self.coverage = initial_cov_object
        if initial_cov is not None:
            self.coverage.clear(); self.coverage.update(initial_cov)
        if initial_gte is not None:
            import ctypes as C
            C.memmove(self.gte_state, initial_gte, len(initial_gte))
        cursor = 0
        def replay(cpu, r):
            nonlocal cursor
            if cursor >= len(records):
                raise AssertionError('C backend invoked an extra hook')
            entry, after_regs, after_mem, result, error, after_gte, public = records[cursor]
            cursor += 1
            if entry != (r[:], cpu.snapshot(), bytes(cpu.gte_state) if initial_gte is not None else None,
                         cpu.steps, None if cpu.coverage is None else cpu.coverage.copy()):
                raise AssertionError('backend difference at hook entry')
            r[:] = after_regs
            cpu.restore(after_mem)
            if after_gte is not None:
                C.memmove(cpu.gte_state, after_gte, len(after_gte))
            self.steps, self.coverage, cov_contents, hook_log = public
            if self.coverage is not None:
                self.coverage.clear(); self.coverage.update(cov_contents)
            self.hook_log[:] = hook_log
            if error is not None:
                raise error
            return result
        ckw = dict(kw)
        for kind in ('hooks', 'patches'):
            ckw[kind] = {pc: replay for pc in (kw[kind] or {})}
        if before_c is not None:
            before_c()
        cresult, cerror = outcome(lambda: fast.call(self, root, **ckw))
        if (type(pyerror), str(pyerror)) != (type(cerror), str(cerror)):
            raise AssertionError(f'backend exception difference: {pyerror!r} / {cerror!r}')
        actual = (self.snapshot(), self._last_state, self.steps, self.hook_log[:],
                  None if self.coverage is None else self.coverage.copy(),
                  bytes(self.gte_state) if initial_gte is not None else None)
        labels = ('memory', 'registers/HI/LO', 'steps', 'hook sequence', 'coverage', 'GTE')
        for label, want, got in zip(labels, expected, actual):
            if want != got:
                raise AssertionError(f'backend difference in {label} at root {root:08X}' +
                                     (f': {want!r} / {got!r}' if label == 'registers/HI/LO' else ''))
        if cursor != len(records):
            raise AssertionError('C backend missed a hook')
        if pyresult != cresult:
            raise AssertionError('backend difference in return value')
        if (type(pyerror), str(pyerror)) != (type(cerror), str(cerror)):
            raise AssertionError(f'backend exception difference: {pyerror!r} / {cerror!r}')
        if cerror is not None:
            raise cerror
        # User hooks ran on the reference list. Preserve aliases retained by a
        # hook, after verifying that the native return has identical contents.
        return pyresult

    def _call_py(self, root, args=(), hooks=None, patches=None, limit=2_000_000, sp=DEFAULT_SP, initial_regs=None,
                 _resume=None):
        """Run the function at `root` with up to four register arguments (extra args go on the
        stack at sp+16...).  Returns the register file (list of 32 ints) at return.
        Decoded instructions are cached per address (the code is never written by the tests)."""
        hooks = hooks or {}
        patches = patches or {}
        special = set(hooks) | set(patches)
        r = [0] * 32
        # Test callers may seed explicitly registered undefined caller values.
        for index, value in (initial_regs or {}).items():
            if not 0 < index < 32:
                raise ValueError('initial register index must be 1..31')
            r[index] = value & 0xFFFFFFFF
        for i, v in enumerate(args[:4]):
            r[4 + i] = v & 0xFFFFFFFF
        r[29] = sp
        for i, v in enumerate(args[4:]):
            self.write32(sp + 16 + 4 * i, v)
        r[31] = SENTINEL
        pc = root
        branch = None      # pending branch target (delay slot executing)
        pending = None     # (reg, value) load in flight
        lo = hi = 0
        if _resume is not None:
            r, pc, branch, pending, hi, lo, limit = _resume
        M = 0xFFFFFFFF
        decoded = self._decoded
        cov = self.coverage
        read = self.read
        for _ in range(limit):
            if branch is None:
                if pc == SENTINEL:
                    if pending is not None:
                        r[pending[0]] = pending[1]
                        r[0] = 0
                    self._last_state = (r[:], hi, lo)
                    self._last_machine = (pc, branch, None)
                    return r
                if pc in special:
                    if pending is not None:
                        r[pending[0]] = pending[1]
                        r[0] = 0
                        pending = None
                    self.hook_log.append((pc, r[4], r[5], r[6], r[7]))
                    if pc in patches:
                        pc = patches[pc](self, r) & M
                        continue
                    res = hooks[pc](self, r)
                    if isinstance(res, tuple):
                        r[2], r[3] = res[0] & M, res[1] & M
                    elif res is not None:
                        r[2] = res & M
                    pc = r[31]
                    continue
            elif pc in special:
                raise MipsError(f"hook at {pc:08X} lands in a branch delay slot")
            if cov is not None:
                cov.add(pc)
            d = decoded.get(pc)
            if d is None:
                d = self._decode(pc)
            op, rs, rt, rd, sh, fn, imm, simm, ins = d
            self.steps += 1
            a = r[rs]
            b = r[rt]
            previous_branch, previous_load = branch, pending
            branch = pending = None
            dest = 0
            val = 0
            if op == 0:
                if fn == 0: dest = rd; val = b << sh
                elif fn == 33: dest = rd; val = a + b
                elif fn == 37: dest = rd; val = a | b
                elif fn == 2: dest = rd; val = b >> sh
                elif fn == 3: dest = rd; val = signed(b) >> sh
                elif fn == 4: dest = rd; val = b << (a & 31)
                elif fn == 6: dest = rd; val = b >> (a & 31)
                elif fn == 7: dest = rd; val = signed(b) >> (a & 31)
                elif fn == 8: branch = a
                elif fn == 9:
                    branch = a
                    dest = rd; val = pc + 8
                elif fn == 16: dest = rd; val = hi
                elif fn == 17: hi = a
                elif fn == 18: dest = rd; val = lo
                elif fn == 19: lo = a
                elif fn in (24, 25):
                    p = signed(a) * signed(b) if fn == 24 else a * b
                    lo, hi = p & M, (p >> 32) & M
                elif fn in (26, 27):
                    lo, hi = _div(a, b, fn == 27)
                elif fn == 32:
                    s = signed(a) + signed(b)
                    if not -0x80000000 <= s < 0x80000000: raise MipsError(f"add overflow at {pc:08X}")
                    dest = rd; val = s
                elif fn == 34:
                    s = signed(a) - signed(b)
                    if not -0x80000000 <= s < 0x80000000: raise MipsError(f"sub overflow at {pc:08X}")
                    dest = rd; val = s
                elif fn == 35: dest = rd; val = a - b
                elif fn == 36: dest = rd; val = a & b
                elif fn == 38: dest = rd; val = a ^ b
                elif fn == 39: dest = rd; val = ~(a | b)
                elif fn == 42: dest = rd; val = int(signed(a) < signed(b))
                elif fn == 43: dest = rd; val = int(a < b)
                elif fn in (12, 13): raise MipsError(f"syscall/break at {pc:08X}")
                else: raise MipsError(f"unsupported SPECIAL {fn:#x} at {pc:08X}")
            elif op == 9: dest = rt; val = a + simm
            elif op == 35:
                pending = (rt, read((a + simm) & M, 4))
            elif op == 43:
                self.write((a + simm) & M, 4, b)
            elif op == 4:
                if a == b: branch = (pc + 4 + simm * 4) & M
            elif op == 5:
                if a != b: branch = (pc + 4 + simm * 4) & M
            elif op == 15: dest = rt; val = imm << 16
            elif op == 3:
                branch = ((pc + 4) & 0xF0000000) | ((ins & 0x3FFFFFF) << 2)
                dest = 31; val = pc + 8
            elif op == 2:
                branch = ((pc + 4) & 0xF0000000) | ((ins & 0x3FFFFFF) << 2)
            elif op == 1:
                if rt in (0, 16): take = signed(a) < 0
                elif rt in (1, 17): take = signed(a) >= 0
                else: raise MipsError(f"unsupported REGIMM {rt} at {pc:08X}")
                if rt in (16, 17):
                    dest = 31; val = pc + 8
                if take: branch = (pc + 4 + simm * 4) & M
            elif op == 6:
                if signed(a) <= 0: branch = (pc + 4 + simm * 4) & M
            elif op == 7:
                if signed(a) > 0: branch = (pc + 4 + simm * 4) & M
            elif op == 8:
                s = signed(a) + simm
                if not -0x80000000 <= s < 0x80000000: raise MipsError(f"addi overflow at {pc:08X}")
                dest = rt; val = s
            elif op == 10: dest = rt; val = int(signed(a) < simm)
            elif op == 11: dest = rt; val = int(a < (simm & M))
            elif op == 12: dest = rt; val = a & imm
            elif op == 13: dest = rt; val = a | imm
            elif op == 14: dest = rt; val = a ^ imm
            elif op == 16: raise MipsError(f"COP0 at {pc:08X}")
            elif op == 18:
                if self.gte_backend is None:
                    raise MipsError(f"COP2 {ins:08X} at {pc:08X} without gte_backend")
                if ins & 0x02000000:
                    self.gte_backend(ins, 0)
                else:
                    result = self.gte_backend(ins, b)
                    if rs in (0, 2): pending = (rt, result & M)
            elif op in (32, 33, 36, 37):
                size = 1 if op in (32, 36) else 2
                value = read((a + simm) & M, size)
                if op in (32, 33): value = signed(value, size * 8)
                pending = (rt, value & M)
            elif op in (34, 38):
                address = (a + simm) & M
                shift = (address & 3) * 8
                word = read(address & ~3, 4)
                old = previous_load[1] if previous_load and previous_load[0] == rt else b
                if op == 34:   # lwl
                    value = (old & ((1 << (24 - shift)) - 1)) | ((word << (24 - shift)) & M)
                else:          # lwr
                    keep = ~((1 << (32 - shift)) - 1) & M if shift else 0
                    value = (old & keep) | (word >> shift)
                pending = (rt, value & M)
            elif op in (40, 41):
                self.write((a + simm) & M, {40: 1, 41: 2}[op], b)
            elif op in (42, 46):
                address = (a + simm) & M
                offset = address & 3
                aligned = address & ~3
                if op == 42:   # swl
                    for i in range(offset + 1):
                        self.write(aligned + i, 1, b >> ((3 - offset + i) * 8))
                else:          # swr
                    for i in range(offset, 4):
                        self.write(aligned + i, 1, b >> ((i - offset) * 8))
            elif op == 50:     # lwc2
                if self.gte_backend is None: raise MipsError(f"lwc2 without gte_backend at {pc:08X}")
                self.gte_backend(0x48800000 | rt << 11, read((a + simm) & M, 4))
            elif op == 58:     # swc2
                if self.gte_backend is None: raise MipsError(f"swc2 without gte_backend at {pc:08X}")
                self.write((a + simm) & M, 4, self.gte_backend(0x48000000 | rt << 11, 0))
            else:
                raise MipsError(f"unsupported opcode {op:#x} at {pc:08X}")
            if dest:
                r[dest] = val & M
            if previous_load is not None and previous_load[0] != dest:
                r[previous_load[0]] = previous_load[1]
            r[0] = 0
            pc = previous_branch if previous_branch is not None else (pc + 4) & M
        raise MipsError("instruction budget exceeded")

    # ------------------------------------------------------------------- GTE
    def attach_gte_oracle(self):
        """Use the pinned PCSX-ReARMed GTE (tools/codex/gte_oracle.py); returns self."""
        import ctypes as C
        sys.path.insert(0, str(ROOT / "tools/codex"))
        import gte_oracle
        lib = gte_oracle.build()
        self._gte_lib = lib
        state = (C.c_uint32 * 64)()
        self.gte_state = state
        self.gte_backend = lambda ins, value: lib.oracle_gte_step(state, ins, value & 0xFFFFFFFF)
        # Some checkers share the callable/state across CPUs. Carry native
        # ownership on the callable so those CPUs also bypass Python for COP2.
        self.gte_backend._native_function = lib.oracle_gte_step
        self.gte_backend._native_state = state
        return self

    def gte_reset(self):
        import ctypes as C
        C.memset(self.gte_state, 0, C.sizeof(self.gte_state))


# ----------------------------------------------------------------- disassembler
_R = ["zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
      "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"]
_SPECIAL = {0: "sll", 2: "srl", 3: "sra", 4: "sllv", 6: "srlv", 7: "srav", 8: "jr", 9: "jalr", 12: "syscall",
            13: "break", 16: "mfhi", 17: "mthi", 18: "mflo", 19: "mtlo", 24: "mult", 25: "multu", 26: "div",
            27: "divu", 32: "add", 33: "addu", 34: "sub", 35: "subu", 36: "and", 37: "or", 38: "xor", 39: "nor",
            42: "slt", 43: "sltu"}
_IMM = {8: "addi", 9: "addiu", 10: "slti", 11: "sltiu", 12: "andi", 13: "ori", 14: "xori"}
_MEM = {32: "lb", 33: "lh", 34: "lwl", 35: "lw", 36: "lbu", 37: "lhu", 38: "lwr", 40: "sb", 41: "sh", 42: "swl",
        43: "sw", 46: "swr", 50: "lwc2", 58: "swc2"}


def disasm(ins, pc):
    op, rs, rt, rd, sh, fn = ins >> 26, ins >> 21 & 31, ins >> 16 & 31, ins >> 11 & 31, ins >> 6 & 31, ins & 63
    imm = ins & 0xFFFF
    simm = signed(imm, 16)
    R = lambda i: "$" + _R[i]
    if ins == 0:
        return "nop"
    if op == 0:
        n = _SPECIAL.get(fn, f"special{fn}")
        if fn in (0, 2, 3): return f"{n} {R(rd)}, {R(rt)}, {sh}"
        if fn in (4, 6, 7): return f"{n} {R(rd)}, {R(rt)}, {R(rs)}"
        if fn == 8: return f"jr {R(rs)}"
        if fn == 9: return f"jalr {R(rd)}, {R(rs)}"
        if fn in (16, 18): return f"{n} {R(rd)}"
        if fn in (17, 19): return f"{n} {R(rs)}"
        if fn in (24, 25, 26, 27): return f"{n} {R(rs)}, {R(rt)}"
        if fn in (12, 13): return n
        return f"{n} {R(rd)}, {R(rs)}, {R(rt)}"
    if op == 1:
        n = {0: "bltz", 1: "bgez", 16: "bltzal", 17: "bgezal"}.get(rt, f"regimm{rt}")
        return f"{n} {R(rs)}, {(pc + 4 + simm * 4) & 0xFFFFFFFF:08x}"
    if op in (2, 3):
        return f"{'j' if op == 2 else 'jal'} {((pc + 4) & 0xF0000000) | ((ins & 0x3FFFFFF) << 2):08x}"
    if op in (4, 5):
        return f"{'beq' if op == 4 else 'bne'} {R(rs)}, {R(rt)}, {(pc + 4 + simm * 4) & 0xFFFFFFFF:08x}"
    if op in (6, 7):
        return f"{'blez' if op == 6 else 'bgtz'} {R(rs)}, {(pc + 4 + simm * 4) & 0xFFFFFFFF:08x}"
    if op in _IMM:
        if op in (12, 13, 14): return f"{_IMM[op]} {R(rt)}, {R(rs)}, 0x{imm:x}"
        return f"{_IMM[op]} {R(rt)}, {R(rs)}, {simm}"
    if op == 15:
        return f"lui {R(rt)}, 0x{imm:x}"
    if op in _MEM:
        if op in (50, 58): return f"{_MEM[op]} cop2r{rt}, {simm}({R(rs)})"
        return f"{_MEM[op]} {R(rt)}, {simm}({R(rs)})"
    if op == 18:
        if ins & 0x02000000: return f"cop2 0x{ins & 0x1FFFFFF:07x}"
        n = {0: "mfc2", 2: "cfc2", 4: "mtc2", 6: "ctc2"}.get(rs, f"cop2op{rs}")
        return f"{n} {R(rt)}, cop2r{rd}"
    if op == 16:
        return f"cop0 0x{ins & 0x3FFFFFF:x}"
    return f".word 0x{ins:08x}"


def listing(cpu, start, size):
    out = []
    for pc in range(start, start + size, 4):
        out.append(f"{pc:08x}  {cpu.read(pc, 4):08x}  {disasm(cpu.read(pc, 4), pc)}")
    return out


if __name__ == "__main__":
    # python tools/mips/mips_ram.py <hex addr> <size in bytes>   -> instruction listing from the EXE
    cpu = MipsRam.from_extract()
    print("\n".join(listing(cpu, int(sys.argv[1], 16), int(sys.argv[2], 0))))
