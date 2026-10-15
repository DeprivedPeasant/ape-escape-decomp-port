"""ctypes boundary for the test-only C interpreter; builds under extract/mips."""
import ctypes as C
import hashlib
import os
from pathlib import Path
import shutil
import subprocess

U32 = C.c_uint32
P32 = C.POINTER(U32)
P8 = C.POINTER(C.c_uint8)
GTE = C.CFUNCTYPE(U32, P32, U32, U32)
WRITE = C.CFUNCTYPE(None, U32, U32, U32)


class State(C.Structure):
    _fields_ = [('r', U32 * 32)] + [(n, U32) for n in
                ('pc', 'branch', 'load_reg', 'load_value', 'hi', 'lo', 'has_branch', 'has_load')] + [
                    ('steps', C.c_uint64), ('remaining', C.c_uint64), ('error', C.c_char * 160)]


class Memory(C.Structure):
    _fields_ = [('ram', P8), ('scratch', P8), ('coverage', P8), ('special', P32),
                ('special_count', U32), ('gte', GTE), ('gte_state', P32),
                ('covered', P32), ('covered_count', U32),
                ('exceptional_pc', U32), ('has_exceptional_pc', U32),
                ('code', P32), ('decoded', P32), ('code_valid', P8), ('decoded_count', U32),
                ('write_hook', WRITE)]


_lib = None
WORDS = 3*0x80000+256


def pc_index(pc):
    if not isinstance(pc, int) or not 0 <= pc <= 0xffffffff or pc & 3:
        return None  # raw Python-PC compatibility cases stay in the reference loop
    if 0x1f800000 <= pc < 0x1f800400:
        return 3*0x80000+(pc-0x1f800000)//4
    bank = {0: 0, 4: 1, 5: 2}[pc>>29]
    return bank*0x80000+(pc & 0x1fffff)//4


def index_pc(index):
    bank, offset = divmod(index, 0x80000)
    return (0, 0x80000000, 0xa0000000, 0x1f800000)[bank]+offset*4


class CodeCache:
    def __init__(self):
        self.code = (U32 * WORDS)()
        self.valid = (C.c_uint8 * (WORDS//8))()
        self.decoded = (U32 * WORDS)()
        self.generation = -1
        self.cursor = 0

    def sync(self, cache):
        if self.generation != cache.generation:
            C.memset(self.valid, 0, C.sizeof(self.valid))
            self.generation, self.cursor = cache.generation, 0
        for pc in cache.changes[self.cursor:]:
            index = pc_index(pc)
            if index is None:
                continue
            self.code[index] = cache[pc][-1]
            self.valid[index>>3] |= 1 << (index&7)
        self.cursor = len(cache.changes)

    def collect(self, cache, memory):
        from mips_ram import signed
        for i in range(memory.decoded_count):
            index = self.decoded[i]
            pc, ins = index_pc(index), self.code[index]
            cache[pc] = (ins>>26, ins>>21&31, ins>>16&31, ins>>11&31,
                         ins>>6&31, ins&63, ins&65535, signed(ins&65535, 16), ins)
        self.cursor = len(cache.changes)
        memory.decoded_count = 0


def build():
    global _lib
    if _lib is None:
        source = Path(__file__).with_suffix('.c')
        out = source.parents[2] / 'extract/mips'
        out.mkdir(parents=True, exist_ok=True)
        key = hashlib.sha256(source.read_bytes()).hexdigest()[:16]
        dll = out / f'mips_fast_{key}.dll'
        if not dll.exists():
            temporary = out / f'mips_fast_{key}_{os.getpid()}.dll'
            subprocess.run([shutil.which('gcc'), '-std=c11', '-O3', '-Wall', '-Wextra',
                            '-shared', str(source), '-o', str(temporary), '-static-libgcc'], check=True)
            try:
                os.replace(temporary, dll)
            except OSError:
                # Another process may already have loaded the identical DLL on
                # Windows, preventing replacement. Its content has the same hash.
                if not dll.exists():
                    raise
                temporary.unlink()
        _lib = C.CDLL(str(dll))
        _lib.mips_run.argtypes = [C.POINTER(State), C.POINTER(Memory)]
        _lib.mips_run.restype = C.c_int
    return _lib


def call(cpu, root, args, hooks, patches, limit, sp, initial_regs):
    from mips_ram import MipsError, MipsRam, SENTINEL
    def word(value):
        return isinstance(value, int) and 0 <= value <= 0xffffffff
    if not word(root) or not word(sp):
        return cpu._reference_call(root, args=args, hooks=hooks, patches=patches,
                                   limit=limit, sp=sp, initial_regs=initial_regs)
    s = State()
    for index, value in (initial_regs or {}).items():
        if not 0 < index < 32:
            raise ValueError('initial register index must be 1..31')
        s.r[index] = value & 0xffffffff
    for i, value in enumerate(args[:4]):
        s.r[4+i] = value & 0xffffffff
    s.r[29] = sp
    for i, value in enumerate(args[4:]):
        cpu.write32(sp+16+4*i, value)
    s.r[31] = SENTINEL
    s.pc = root
    s.remaining = max(0, limit)
    hooks, patches = hooks or {}, patches or {}
    # Non-word PCs can only occur on the Python compatibility path. Do not
    # accidentally wrap their hook keys onto an unrelated uint32 address.
    addresses = sorted(pc for pc in set(hooks) | set(patches) if word(pc))
    specials = (U32 * len(addresses))(*addresses)
    ram = (C.c_uint8 * len(cpu.ram)).from_buffer(cpu.ram)
    scratch = (C.c_uint8 * len(cpu.scratch)).from_buffer(cpu.scratch)
    cov = cpu.coverage  # the reference captures this set for the duration of call()
    bitmap = covered = None
    if cov is not None:
        if not hasattr(cpu, '_fast_coverage'):
            cpu._fast_coverage = ((C.c_uint8 * (WORDS//8))(), (U32 * WORDS)())
        bitmap, covered = cpu._fast_coverage
    gte = GTE()
    gte_state = None
    callback_errors = []
    registers = list(s.r)  # one live list shared by all hooks and the caller
    write_hook = WRITE()
    if getattr(cpu.write, '__func__', None) is not MipsRam.write:
        def write_callback(address, size, value):
            try:
                sync_observers(True)
                cpu.write(address, size, value)
            except BaseException as error:
                callback_errors.append(error)
                s.error = b'Python memory callback failed'
        write_hook = WRITE(write_callback)
    if cpu.gte_backend is not None:
        native_function = getattr(cpu.gte_backend, '_native_function', None)
        if native_function is not None:
            gte = C.cast(native_function, GTE)
            gte_state = cpu.gte_backend._native_state
        else:
            # Preserve the documented custom Python backend API as well.
            def callback(state, ins, value):
                try:
                    sync_observers(True)
                    return cpu.gte_backend(ins, value)
                except BaseException as error:
                    callback_errors.append(error)
                    s.error = b'Python GTE callback failed'
                    return 0
            gte = GTE(callback)
    if not hasattr(cpu, '_fast_code'):
        cpu._fast_code = CodeCache()
    code = cpu._fast_code
    memory = Memory(ram, scratch, bitmap, specials, len(addresses), gte, gte_state, covered, 0,
                    0, 0, code.code, code.decoded, code.valid, 0, write_hook)
    def sync_observers(publish_regs=False):
        # Hooks and overridden stores may inspect these public fields. Publish
        # the executed prefix before invoking Python, not just at final return.
        cpu.steps += s.steps
        s.steps = 0
        if publish_regs:
            registers[:] = s.r
        if bitmap is not None:
            if memory.has_exceptional_pc:
                cov.add(memory.exceptional_pc)
                memory.has_exceptional_pc = 0
            for i in range(memory.covered_count):
                index = covered[i]
                cov.add(index_pc(index))
                bitmap[index>>3] = 0
            memory.covered_count = 0
    lib = build()
    resumed = hook_exception = False
    try:
        while True:
            code.sync(cpu._decoded)
            status = lib.mips_run(C.byref(s), C.byref(memory))
            sync_observers(True)
            code.collect(cpu._decoded, memory)
            if status == -1:
                if callback_errors:
                    raise callback_errors[0]
                message = bytes(s.error).decode()
                raise MipsError(message)
            if status == 0:
                return registers
            r = registers
            pc = s.pc
            cpu.hook_log.append((pc, r[4], r[5], r[6], r[7]))
            try:
                if pc in patches:
                    next_pc = patches[pc](cpu, r) & 0xffffffff
                else:
                    result = hooks[pc](cpu, r)
                    if isinstance(result, tuple):
                        r[2], r[3] = result[0] & 0xffffffff, result[1] & 0xffffffff
                    elif result is not None:
                        r[2] = result & 0xffffffff
                    next_pc = r[31]
            except BaseException:
                hook_exception = True
                cpu._last_state = (r[:], s.hi, s.lo)
                cpu._last_machine = (s.pc, None, None)
                raise
            if not word(next_pc) or not all(word(value) for value in r):
                # The Python hook protocol permits arbitrary raw register writes.
                # They are normally uint32 words, but truncating an exceptional
                # value here would change comparisons, shifts or even the return
                # list. Continue from this exact boundary without rerunning hooks.
                resumed = True
                cpu.steps += s.steps
                s.steps = 0
                return cpu._reference_call(root, hooks=hooks, patches=patches,
                                           _resume=(r, next_pc, None, None, s.hi, s.lo, s.remaining))
            s.r[:] = r
            s.pc = next_pc
    finally:
        sync_observers()
        if not resumed and not hook_exception:
            cpu._last_state = (list(s.r), s.hi, s.lo)
            cpu._last_machine = (s.pc, s.branch if s.has_branch else None,
                                 (s.load_reg, s.load_value) if s.has_load else None)
