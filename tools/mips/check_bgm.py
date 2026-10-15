"""Differential checks for the BGM / sequencer ports on real EXE and area RAM images."""
import argparse
import ctypes as C
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas
import dump_sound
import exclusion_registry as reg
import mips_ram as mr

FUNCS = {
    "SoundLoopsStopAll": (0x80086540, 584, True, "", "s"),
    "SoundAreaKeepsMusic": (0x80086F00, 208, False, "ss", "s"),
    "BgmStart": (0x80087388, 132, True, "", "s"),
    "BgmModeWantsMusic": (0x8008740C, 80, False, "", "s"),
    "BgmUpdateVolume": (0x8008745C, 432, True, "", "s"),
    "SoundLoopStart": (0x800878E0, 320, True, "ss", "s"),
    "SoundDelayedQueueReset": (0x80088578, 48, False, "", "s"),
    "ProgressGetByte": (0x800963AC, 48, False, "u", "u"),
    "SeqUpdateVoiceVolume": (0x800A0900, 356, True, "u", "v"),
    "SeqVoiceVolumeSide": (0x800A0A64, 168, False, "uu", "s"),
    "SeqFadeInit": (0x800A0DB0, 212, False, "ssss", "s"),
    "BgmSeqOpen": (0x800A1DB0, 396, False, "su", "s"),
    "BgmSeqSetVolume": (0x800A2428, 420, True, "ss", "s"),
    "SeqTrackRearm": (0x800A2ACC, 160, False, "s", "v"),
    "SeqFadeTrackIn": (0x800A2E3C, 280, True, "sss", "s"),
}

AREA_IDS = (1, 2, 5)
SEQ_ADDR = 0x801E0000
COMMON_ADDR = 0x801A0000
HEADER_ADDR = 0x801C0000
SPU_ADDR = 0x1F801C00
STACK_LO = 0x1FF000


def build(source=None, trapv=False):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"bgm_{os.getpid()}_{time.time_ns()}.dll"
    command = [shutil.which("gcc"), "-std=c11", "-O1" if trapv else "-O2",
               *(["-ftrapv"] if trapv else []), "-Wall", "-Wextra", "-Werror",
               "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
               "-shared", "-static-libgcc", "-I" + str(ROOT / "src/sound"),
               str(ROOT / "src/core/ae_ram.c"), str(ROOT / "src/core/ae_sdk.c"),
               str(ROOT / "src/sound/ae_sound.c"), str(source or ROOT / "src/sound/ae_bgm.c"),
               "-o", str(dll)]
    subprocess.run(command, check=True)
    return dll


Idle = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p)
Wait = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32)
Adsr = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint16, C.c_uint16)
Off = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint16)
Print = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32,
                     C.c_int32, C.c_int32, C.c_int32)
Volume = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint16)


class Hooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("wait_idle", Idle), ("wait_loop", Wait),
                ("set_voice_adsr", Adsr), ("voice_key_off", Off), ("printf_error", Print),
                ("set_voice_volume", Volume)]


class Harness:
    def __init__(self, source=None, trapv=False):
        reg.declare("stack_window", "801FF000..80200000")
        area_members = dump_areas.area_members()
        missing = [area for area in AREA_IDS if area not in area_members]
        if missing:
            raise AssertionError(f"real area table has no members for {missing}")
        self.area_bases = {}
        self.m = mr.MipsRam.from_extract()
        self.m.coverage = set()
        exe_base = self.m.snapshot()
        for area in AREA_IDS:
            member = area_members[area]
            blob = (ROOT / f"extract/members/{member:03X}.bin").read_bytes()
            self.m.restore(exe_base)
            self.m.load_area(blob)
            self.area_bases[area] = self.m.snapshot()
        self.m.restore(self.area_bases[AREA_IDS[0]])

        container = (ROOT / "extract/members/01B.bin").read_bytes()
        chunks = dump_sound.parse(container)
        if not chunks or not chunks["ssq"]:
            raise AssertionError("real 01B member has no SSsq sequence")
        seq_off, seq_size = chunks["ssq"]
        self.sequence = container[seq_off:seq_off + seq_size]
        shd_off, shd_size = next((off, size) for off, size in chunks["shd"]
                                 if struct.unpack_from("<I", container, off + 0x20)[0] != 0xFFFFFFFF)
        self.header = container[shd_off:shd_off + shd_size]
        self.common = (ROOT / "extract/members/00B.bin").read_bytes()
        self.effect_rows = bytes(self.m.ram[0xB2D08:0xB2D08 + 198 * 8])

        self.lib = C.CDLL(str(build(source, trapv)))
        P, U, S = C.c_void_p, C.c_uint32, C.c_int32
        self.lib.ae_ram_create.restype = P
        self.lib.ae_ram_destroy.argtypes = [P]
        self.lib.ae_ptr.argtypes = [P, U, U]
        self.lib.ae_ptr.restype = P
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.lib.ae_ram_failed.argtypes = [P]
        self.lib.ae_ram_failed.restype = C.c_int
        self.lib.ae_ram_error.argtypes = [P]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.ram = self.lib.ae_ram_create()
        self.main = self.lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE)
        self.scratch = self.lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE)
        if not self.ram or not self.main or not self.scratch:
            raise MemoryError("could not allocate the C RAM image")
        self.libfuncs = {}
        for name, (_address, _size, has_hw, arg_types, ret) in FUNCS.items():
            fn = getattr(self.lib, "ae_" + name)
            fn.argtypes = [P] + ([P] if has_hw else []) + [S if kind == "s" else U for kind in arg_types]
            fn.restype = None if ret == "v" else (U if ret == "u" else S)
            self.libfuncs[name] = fn

        self.events = []
        self.original_events = []
        self.idle = 0
        self.counts = {name: 0 for name in FUNCS}
        self.target = None
        self.null_volume_checks = 0
        self.hooks = Hooks()
        self.keep = [
            Idle(lambda _u, _r: self.record("wait_idle", c_side=True, result=self.idle)),
            Wait(lambda _u, _r, units: self.record("wait_loop", units, c_side=True)),
            Adsr(lambda _u, _r, channel, a1, a2:
                 self.record("set_voice_adsr", channel, a1, a2, c_side=True)),
            Off(lambda _u, _r, half, mask:
                self.record("voice_key_off", half, mask, c_side=True)),
            Print(lambda _u, _r, fmt, argc, a, b, c:
                  self.record("printf_error", fmt, argc, *[a, b, c][:argc], c_side=True)),
            Volume(lambda _u, _r, channel, side, volume:
                   self.record("set_voice_volume", channel, side, volume, c_side=True)),
        ]
        for (field, _type), callback in zip(Hooks._fields_[1:], self.keep):
            setattr(self.hooks, field, callback)

    def record(self, name, *args, c_side=False, result=None):
        (self.events if c_side else self.original_events).append((name, *args))
        return result

    def setup_base(self, m):
        """Load the real common and area SShd data used by the original SFX table."""
        m.write_bytes(COMMON_ADDR, self.common)
        m.write_bytes(HEADER_ADDR, self.header)
        m.write_bytes(SEQ_ADDR, self.sequence)
        m.write_bytes(0x800E1180, bytes(256 * 12))
        for bank, header in ((0, COMMON_ADDR), (2, HEADER_ADDR)):
            record = 0x800E1180 + bank * 12
            m.write32(record, 1)
            m.write32(record + 4, header)
        m.write16(0x800BF670, 0)
        m.write16(0x800BF674, 2)
        m.write_bytes(0x800F75C0, bytes(40))
        m.write8(0x800F75C3, 1)
        m.write32(0x800E1874, SPU_ADDR)
        m.write_bytes(0x800F5AC0, bytes(24 * 0x120))
        m.write_bytes(0x800E07F0, bytes(24 * 0x66))
        for i in range(24):
            loop = 0x800BF370 + 0x1C * i
            m.write16(loop, 0xFFFF)
            m.write16(loop + 2, 0xFFFF)
            m.write16(loop + 0x12, 0)
            m.write16(loop + 0x14, 0xFFFF)
            m.write16(loop + 0x16, 0)
        for i in range(12):
            m.write_bytes(0x800BF610 + 8 * i, bytes((i + 1,)) * 8)
            m.write16(0x800BF610 + 8 * i, -1)

    def setup_track(self, m, track=0, bank=2, active=1, paused=0, volume=100):
        base = 0x800F5AC0 + 0x120 * track
        m.write8(base + 0x20, bank)
        m.write8(base + 0x24, active)
        m.write8(base + 0x25, paused)
        m.write32(base + 0x0C, 0x110)
        m.write32(base + 0x10, SEQ_ADDR)
        m.write16(base + 0x104, 170)
        m.write16(base + 0x106, 480)
        m.write8(SEQ_ADDR, volume)
        m.write32(0x800E1874, SPU_ADDR)
        m.write32(HEADER_ADDR + 0x10, 0x100)
        m.write32(HEADER_ADDR + 0x20, 0x100)
        m.write8(HEADER_ADDR + 0x100 + 0x10 + 0x0E, 0x7F)
        return base

    def setup_voice(self, m, channel=0, track=0, bank=2, program=1, effect=0,
                    active=1, phase=0, factors=(127, 256, 128, 64), pans=(0x80, 0xC0)):
        voice = 0x800E07F0 + 0x66 * channel
        m.write16(voice + 0x00, active)
        m.write16(voice + 0x04, program)
        m.write16(voice + 0x06, track)
        m.write16(voice + 0x1A, effect)
        m.write16(voice + 0x22, bank)
        for offset, factor in zip((0x28, 0x2A, 0x2C, 0x2E), factors):
            m.write16(voice + offset, factor)
        m.write16(voice + 0x30, pans[0])
        m.write16(voice + 0x32, pans[1])
        m.write16(voice + 0x4A, phase)
        return voice

    def setup_bgm(self, m, track=0, bank=2, paused=0, bgm_voice=None, add_voice=False,
                  effect=0, phase=0, current_volume=100):
        self.setup_base(m)
        self.setup_track(m, track, bank, 1, paused, current_volume)
        m.write16(0x800BF676, -1 if bgm_voice is None else bgm_voice)
        m.write16(0x800BF672, bank)
        m.write16(0x800BF67E, 75)
        m.write32(0x800F4580, SEQ_ADDR)
        if add_voice:
            self.setup_voice(m, track=track, bank=bank, effect=effect, phase=phase)

    def run(self, name, args=(), setup=None, area=1, idle=0, missing_volume=False):
        if self.target and self.target != name:
            return
        self.m.restore(self.area_bases[area])
        if setup:
            setup(self.m)
        initial = self.m.snapshot()
        C.memmove(self.main, initial[0], mr.RAM_SIZE)
        C.memmove(self.scratch, initial[1], mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)
        self.events = []
        self.original_events = []
        self.idle = idle

        write = self.m.write
        adsr_half = {}

        def spu_write(address, size, value):
            if SPU_ADDR <= address < SPU_ADDR + 0x200:
                offset = address - SPU_ADDR
                value &= 0xFFFF
                if offset in (0x18C, 0x18E):
                    self.record("voice_key_off", (offset - 0x18C) // 2, value)
                elif offset < 24 * 16 and offset % 16 in (0, 2):
                    self.record("set_voice_volume", offset // 16, offset % 16 // 2, value)
                elif offset < 24 * 16 and offset % 16 == 8:
                    adsr_half[offset // 16] = value
                elif offset < 24 * 16 and offset % 16 == 10:
                    self.record("set_voice_adsr", offset // 16, adsr_half.pop(offset // 16, 0), value)
                else:
                    raise AssertionError(f"unexpected SPU write {address:08X}/{size}")
            else:
                write(address, size, value)

        self.m.write = spu_write

        def wait_idle(_cpu, _regs):
            self.record("wait_idle")
            return idle

        def wait_loop(_cpu, regs):
            self.record("wait_loop", regs[4])
            return 0

        def print_error(_cpu, regs):
            fmt = regs[4]
            argc = {0x800A6200: 1, 0x800A6240: 3, 0x800A625C: 0}.get(fmt)
            if argc is None:
                raise AssertionError(f"unregistered diagnostic call {fmt:08X}")
            values = [mr.signed(x) for x in regs[5:5 + argc]]
            self.record("printf_error", fmt, argc, *values)
            return 0

        hooks = {0x800A1398: wait_idle, 0x8009D79C: wait_loop, 0x800A3720: print_error}
        try:
            original = self.m.call(FUNCS[name][0], args, hooks=hooks)
        finally:
            self.m.write = write

        self.lib.ae_ram_clear_error(self.ram)
        if missing_volume:
            missing_hooks = Hooks.from_buffer_copy(self.hooks)
            missing_hooks.set_voice_volume = Volume()
            fn = self.libfuncs[name]
            fn(self.ram, C.byref(missing_hooks), *args)
            if not self.lib.ae_ram_failed(self.ram):
                raise AssertionError("missing set_voice_volume callback did not fail closed")
            self.null_volume_checks += 1
            self.lib.ae_ram_clear_error(self.ram)
        else:
            fn = self.libfuncs[name]
            c_args = [self.ram] + ([C.byref(self.hooks)] if FUNCS[name][2] else []) + list(args)
            actual = fn(*c_args)
            ret = FUNCS[name][4]
            if ret != "v" and (int(actual) & 0xFFFFFFFF) != original[2]:
                raise AssertionError(f"{name}{tuple(args)}: return C={int(actual)&0xFFFFFFFF:08X} MIPS={original[2]:08X}")
            if self.lib.ae_ram_failed(self.ram):
                raise AssertionError(f"{name}{tuple(args)}: C RAM error {self.lib.ae_ram_error(self.ram)!r}")
            if self.events != self.original_events:
                raise AssertionError(f"{name}{tuple(args)}: hooks C={self.events!r} MIPS={self.original_events!r}")

        expected = bytes(self.m.ram[:STACK_LO])
        actual_ram = C.string_at(self.main, STACK_LO)
        if actual_ram != expected:
            offset = next(i for i, (a, b) in enumerate(zip(actual_ram, expected)) if a != b)
            raise AssertionError(f"{name}{tuple(args)}: RAM differs at {0x80000000+offset:08X}: C={actual_ram[offset]:02X} MIPS={expected[offset]:02X}")
        if C.string_at(self.scratch, mr.SCRATCH_SIZE) != bytes(self.m.scratch):
            raise AssertionError(f"{name}{tuple(args)}: scratchpad differs")
        self.counts[name] += 1

    def finish(self, coverage=False, default_run=False):
        items = [(name, addr, size, True) for name, (addr, size, *_rest) in FUNCS.items()]
        summary = reg.coverage_summary(items, self.m.coverage)
        if coverage:
            reg.print_coverage(summary)
        gaps = any(row["uncovered_count"] or row["stale_declared"] for row in summary.values())
        if default_run:
            reg.write_results(self.counts, summary, not gaps,
                              extra={"missing_set_voice_volume_checks": self.null_volume_checks},
                              function_names=list(FUNCS))
        if gaps:
            raise AssertionError("undeclared BGM instruction coverage gaps")
        return summary


def scenarios(h, scale=1.0, seed=94423):
    rng = random.Random(seed)

    # Leaf lookups and simple queue reset, repeated over three real area members.
    area_pairs = [(75, 0), (72, 76), (4, 0), (3, 4), (34, 7), (33, 34),
                  (32, 30), (30, 32), (8, 6), (6, 8), (9, 10), (10, 9),
                  (0, 75), (-1, 76), (0x10000 + 75, 0)]
    for area in AREA_IDS:
        for pair in area_pairs:
            h.run("SoundAreaKeepsMusic", pair, area=area)
        for offset in (0, 1, 2, 3, 4, 7, 8, 0x3FFFC, 0x3FFFF, 0x40000, 0xFFFFFFFF):
            def progress_setup(m, offset=offset):
                m.write32(0x800DFDD8 + (offset & 0x3FFFC), rng.getrandbits(32))
                m.write32(0x800DFDD8, 0x44332211)
            h.run("ProgressGetByte", (offset,), progress_setup, area=area)
        h.run("SoundDelayedQueueReset", (), lambda m: [
            m.write_bytes(0x800BF610 + 8 * i, rng.randbytes(8)) for i in range(12)], area=area)
        for mode in range(0, 32):
            h.run("BgmModeWantsMusic", (), lambda m, mode=mode: m.write16(0x800F4470, mode), area=area)
        h.run("BgmModeWantsMusic", (), lambda m: m.write16(0x800F4470, 0xFFFF), area=area)

    # Read every real SFX table entry through SoundLoopStart, with its real common/stage banks.
    for effect_id in range(198):
        slot, program, note = struct.unpack_from("<3h", h.effect_rows, effect_id * 8)
        assert slot in (0, 2), (effect_id, slot)
        for mode in (effect_id & 1,):
            def effect_setup(m, effect_id=effect_id, slot=slot, program=program, note=note):
                h.setup_base(m)
                row = 0x800B2D08 + effect_id * 8
                m.write16(row, slot)
                m.write16(row + 2, program)
                m.write16(row + 4, note)
            h.run("SoundLoopStart", (mode, effect_id), effect_setup, area=1)

    def sound_request_setup(m, loop_full=False, enabled=True):
        h.setup_base(m)
        row = 0x800B2D08 + 9 * 8
        m.write16(row, 0)
        m.write16(row + 2, 1)
        m.write16(row + 4, 1)
        m.write16(0x800BF670, 2)
        if not enabled:
            m.write8(0x800F75C3, 0)
        if loop_full:
            for i in range(24):
                m.write16(0x800BF370 + 0x1C * i, 0x100 + i)
                m.write16(0x800BF370 + 0x1C * i + 2, -1)

    for mode in (0, 1, -1, 0x10001):
        h.run("SoundLoopStart", (mode, 9), lambda m, mode=mode: sound_request_setup(m), area=1)
    def signed_sound_request(m):
        sound_request_setup(m)
        row = 0x800B2D08 + 9 * 8
        m.write16(row + 2, -1)
        m.write16(row + 4, -1)
    h.run("SoundLoopStart", (0, 9), signed_sound_request, area=1)
    h.run("SoundLoopStart", (1, 0x1009), lambda m: sound_request_setup(m), area=1)
    h.run("SoundLoopStart", (0, 9), lambda m: sound_request_setup(m, loop_full=True), area=1)
    h.run("SoundLoopStart", (0, 0), lambda m: sound_request_setup(m, loop_full=True), area=1)
    h.run("SoundLoopStart", (1, 9), lambda m: sound_request_setup(m, enabled=False), area=1)
    h.run("SoundLoopStart", (0, 9), lambda m: (sound_request_setup(m), m.write16(0x800BF670, -1)), area=1)

    # SoundLoopsStopAll: the real area-1 definition list, its sentinel, preserved 5xxx loops,
    # directly-reset 7xxx loops, and the ordinary SoundLoopFree path.
    def stop_all_setup(m, full=False, area_index=1):
        sound_request_setup(m)
        m.write16(0x800F4476, area_index)
        for i in range(24):
            row = 0x800BF370 + 0x1C * i
            m.write16(row, -1)
            m.write16(row + 2, -1)
        if full:
            for i in range(24):
                m.write16(0x800BF370 + 0x1C * i, 0x5000 + i)
        else:
            for i, (loop_id, voice) in enumerate(((0x5001, 0), (0x7002, -1), (9, 0), (10, -1))):
                row = 0x800BF370 + 0x1C * i
                m.write16(row, loop_id)
                m.write16(row + 2, voice)
    h.run("SoundLoopsStopAll", (), lambda m: stop_all_setup(m), area=1)
    h.run("SoundLoopsStopAll", (), lambda m: stop_all_setup(m, full=True), area=1)
    h.run("SoundLoopsStopAll", (), lambda m: stop_all_setup(m, area_index=5), area=5)
    def stop_all_failed_voice(m):
        stop_all_setup(m, area_index=1)
        m.write16(0x800BF66E, -1)
        m.write16(0x800B2D08 + 172 * 8, -1)
    h.run("SoundLoopsStopAll", (), stop_all_failed_voice, area=1)

    # ProgressGetByte's high offset bits are masked; exercise randomized table bytes as well.
    for _ in range(max(1, round(20 * scale))):
        offset = rng.getrandbits(32)
        def random_progress(m, offset=offset):
            m.write32(0x800DFDD8 + (offset & 0x3FFFC), rng.getrandbits(32))
        h.run("ProgressGetByte", (offset,), random_progress, area=rng.choice(AREA_IDS))

    # Sequencer open and BGM start use a genuine SSsq chunk from sound bank 01B.
    for area in AREA_IDS:
        for bank in (0, 2, 127, 128, -1):
            def open_setup(m, bank=bank):
                h.setup_base(m)
                m.write32(0x800E1180 + (bank & 0xFF) * 12, int(bank not in (128, -1)))
            h.run("BgmSeqOpen", (bank, SEQ_ADDR), open_setup, area=area)
        def start_setup(m, bank=2):
            h.setup_bgm(m, bgm_voice=None)
            m.write16(0x800BF672, bank)
        h.run("BgmStart", (), start_setup, area=area)
        h.run("BgmStart", (), lambda m: start_setup(m, bank=0), area=area)
        h.run("BgmStart", (), lambda m: (h.setup_bgm(m), m.write32(0x800F4580, 0)), area=area)
        h.run("BgmStart", (), lambda m: (h.setup_bgm(m), m.write16(0x800BF672, -1)), area=area)

    # BgmSeqOpen slot selection, rejection checks, and the full 24-track pool.
    def open_valid(m, busy=0):
        h.setup_base(m)
        for i in range(busy):
            m.write32(0x800F5AC0 + 0x120 * i + 0x24, 1)
    for busy in (0, 1, 7, 23, 24):
        h.run("BgmSeqOpen", (2, SEQ_ADDR), lambda m, busy=busy: open_valid(m, busy), area=2)
    h.run("BgmSeqOpen", (128, SEQ_ADDR), lambda m: open_valid(m), area=1)
    h.run("BgmSeqOpen", (2, SEQ_ADDR + 1), lambda m: open_valid(m), area=1)
    h.run("BgmSeqOpen", (2, SEQ_ADDR), lambda m: (open_valid(m), m.write32(SEQ_ADDR + 0x0C, 0)), area=1)
    h.run("BgmSeqOpen", (2, SEQ_ADDR), lambda m: (open_valid(m), m.write32(0x800E1180 + 2 * 12, 0)), area=1)

    # Mixer math: both phase modes, normal/effect tone paths, and first/last sequencer voices.
    for channel, phase, effect in ((0, 0, 0), (0, 0x80, 0), (23, 0, 1), (23, 3, 1)):
        def update_setup(m, channel=channel, phase=phase, effect=effect):
            h.setup_bgm(m)
            h.setup_voice(m, channel=channel, effect=effect, phase=phase,
                          factors=(0x7F, 0x123, 0x234, 0x345), pans=(0x7FFF, 0x8123))
            if effect:
                m.write32(HEADER_ADDR + 0x20, 0x100)
                m.write8(HEADER_ADDR + 0x100 + 0x10 + 0x10 + 0x0E, 0x6D)
        h.run("SeqUpdateVoiceVolume", (channel,), update_setup, area=1)
    h.run("SeqUpdateVoiceVolume", (0,), lambda m: h.setup_bgm(m, add_voice=True), area=2,
          missing_volume=True)

    for channel in (0, 23):
        for side in (0, 1):
            for phase in (0, 1, 0x80):
                def math_setup(m, channel=channel, phase=phase):
                    h.setup_bgm(m)
                    h.setup_voice(m, channel=channel, phase=phase,
                                  factors=tuple(rng.choice((0, 1, 0x7F, 0x400, 0x7FFF)) for _ in range(4)),
                                  pans=(rng.getrandbits(16), rng.getrandbits(16)))
                h.run("SeqVoiceVolumeSide", (channel, side), math_setup, area=2)

    # BgmSeqSetVolume direct update and channel volume writes.
    for volume in (-32768, -1, 0, 1, 75, 127, 128, 32767):
        def volume_setup(m, volume=volume):
            h.setup_bgm(m, add_voice=(volume in (0, 75, 127)))
            if volume in (0, 75, 127):
                h.setup_voice(m, active=1, bank=2, track=0, effect=0,
                              factors=(0x100, 0x100, 0x100, 0x100), pans=(4096, 4096))
        h.run("BgmSeqSetVolume", (0, volume), volume_setup, area=1)
    h.run("BgmSeqSetVolume", (24, 75), lambda m: h.setup_bgm(m), area=1)
    h.run("BgmSeqSetVolume", (-1, 75), lambda m: h.setup_bgm(m), area=1)
    h.run("BgmSeqSetVolume", (0, 75), lambda m: (h.setup_bgm(m), m.write8(0x800F5AC0 + 0x24, 0)), area=1)

    # Fade initialization has separate whole-track and per-program record layouts.
    for program in (-1, 0, 1, 15, 16, 255, 0xFFFF):
        h.run("SeqFadeInit", (0, 45, program, 75), lambda m: h.setup_bgm(m), area=1)

    # Rearm status combinations: active gate, header sentinel, loaded-record equality.
    def rearm_setup(m, active=1, loaded=1, sentinel=0x100):
        h.setup_bgm(m)
        m.write8(0x800F5AC0 + 0x24, active)
        m.write32(0x800F5AC0 + 0x1C, 0x12345678)
        m.write32(0x800E1180 + 2 * 12, loaded)
        m.write32(HEADER_ADDR + 0x10, sentinel)
    for active, loaded, sentinel in ((0, 1, 0x100), (1, 1, 0x100), (1, 0, 0x100),
                                     (1, 1, 0xFFFFFFFF)):
        h.run("SeqTrackRearm", (0,), lambda m, a=active, l=loaded, s=sentinel: rearm_setup(m, a, l, s), area=2)

    # Fade branch tests cover invalid frame/volume, paused-state values and successful setup.
    for frames, volume, paused, current in ((256, 75, 0, 100), (90, 128, 0, 100),
                                            (90, 75, 0, 100), (90, 75, 1, 10),
                                            (90, 75, 1, 100), (90, 75, 2, 10),
                                            (-1, -1, 0, 100)):
        def fade_setup(m, frames=frames, volume=volume, paused=paused, current=current):
            h.setup_bgm(m, paused=paused, current_volume=current, add_voice=(paused == 0))
            if paused == 0:
                h.setup_voice(m, active=1, bank=2, track=0, effect=0,
                              factors=(0x100, 0x100, 0x100, 0x100), pans=(4096, 4096))
        h.run("SeqFadeTrackIn", (0, frames, volume), fade_setup, area=1)

    # BgmUpdateVolume's special progress path and all mode groups.
    for game_mode in (3, 8, 9, 10, 11, 12, 13, 16, 17, 18, 19, 20, 23, 24, 25, 29, 30, 31):
        def update_bgm(m, game_mode=game_mode):
            h.setup_bgm(m, bgm_voice=0, paused=1, current_volume=10)
            m.write16(0x800F4470, game_mode)
        h.run("BgmUpdateVolume", (), update_bgm, area=5)
    for progress_value in (0, 5, 6):
        def special_bgm(m, progress_value=progress_value):
            h.setup_bgm(m, bgm_voice=0, paused=1, current_volume=10)
            m.write16(0x800F4470, 0x11)
            m.write32(0x800E38B4, 70)
            m.write8(0x800DFDD8 + 8, progress_value)
        h.run("BgmUpdateVolume", (), special_bgm, area=2)
    h.run("BgmUpdateVolume", (), lambda m: h.setup_bgm(m, bgm_voice=-1), area=1)


def mutation_sources(original):
    specs = [
        ("SoundLoopsStopAll", "preserved loop type", "AE_BGM_LOOP_TYPE_KEEP", "0x4000"),
        ("SoundLoopsStopAll", "special list area index", "AE_BGM_LOOP_POINTER_STRIDE * area_index", "AE_BGM_LOOP_POINTER_STRIDE * (area_index + 1u)"),
        ("SoundAreaKeepsMusic", "area 75 unconditional rule", "from_area == 75", "from_area == 74"),
        ("SoundAreaKeepsMusic", "destination 76 rule", "to_area == 76", "to_area == 75"),
        ("BgmStart", "null sequence guard", "if (sequence == 0) return -1;", "if (sequence != 0) return -1;"),
        ("BgmStart", "loaded bank zero accepted", "if (bank < 0) return -1;", "if (bank <= 0) return -1;"),
        ("BgmModeWantsMusic", "mode 13", "mode == 0x0Du", "mode == 0x0Cu"),
        ("BgmModeWantsMusic", "mode 24", "mode == 0x18u", "mode == 0x17u"),
        ("BgmUpdateVolume", "station volume divisor", "bgm_s16(target) / 3", "bgm_s16(target) / 2"),
        ("BgmUpdateVolume", "fixed reentry volume", "ae_BgmSeqSetVolume(r, hw, voice, 75);", "ae_BgmSeqSetVolume(r, hw, voice, 76);"),
        ("SoundLoopStart", "mode selects extended voice", "if (mode == 0)", "if (mode != 0)"),
        ("SoundLoopStart", "typed loop skips registration", "if (((uint32_t)loop_id & AE_BGM_LOOP_TYPE_MASK) != 0)", "if (((uint32_t)loop_id & AE_BGM_LOOP_TYPE_MASK) == 0)"),
        ("SoundDelayedQueueReset", "queue sentinel", "ae_wr16(r, entry + AE_SQ_TYPE, 0xFFFFu);", "ae_wr16(r, entry + AE_SQ_TYPE, 0);"),
        ("SoundDelayedQueueReset", "queue id clear", "ae_wr16(r, entry + AE_SQ_ID, 0);", "ae_wr16(r, entry + AE_SQ_ID, 1);"),
        ("ProgressGetByte", "progress table mask", "offset & AE_BGM_PROGRESS_MASK", "offset & 0x0000FFFCu"),
        ("ProgressGetByte", "byte shift index", "(offset & 3u) * 8u", "(offset & 1u) * 8u"),
        ("SeqUpdateVoiceVolume", "left register side", "channel, 0, (uint16_t)left", "channel, 1, (uint16_t)left"),
        ("SeqUpdateVoiceVolume", "effect program source", "uint32_t program = ae_rd16(r, voice + AE_BV_PROGRAM);", "uint32_t program = ae_rd16(r, voice + AE_BV_TRACK);"),
        ("SeqVoiceVolumeSide", "first volume factor", "voice + AE_BV_VOLUME_A", "voice + AE_BV_VOLUME_B"),
        ("SeqVoiceVolumeSide", "phase packing guard", "if (phase != 0)", "if (phase == 0)"),
        ("SeqFadeInit", "whole track selector", "program_id == 0xFFFFu", "program_id != 0xFFFFu"),
        ("SeqFadeInit", "track fade target field", "track + AE_BT_FADE_TRACK_TARGET", "track + AE_BT_FADE_TRACK_START"),
        ("BgmSeqOpen", "sequence alignment check", "(sequence & 3u) != 0", "(sequence & 3u) == 0"),
        ("BgmSeqOpen", "initial read position", "track + AE_BT_READ_POSITION, AE_BGM_SSQ_EVENT_OFFSET", "track + AE_BT_READ_POSITION, AE_BGM_SSQ_EVENT_OFFSET + 1u"),
        ("BgmSeqSetVolume", "volume upper bound", "target_volume >= 0x80", "target_volume > 0x80"),
        ("BgmSeqSetVolume", "channel scale precision", "* (uint32_t)target_volume, 7)", "* (uint32_t)target_volume, 8)"),
        ("SeqTrackRearm", "header word test", "!= 0xFFFFFFFFu", "== 0xFFFFFFFFu"),
        ("SeqTrackRearm", "rearm counter offset", "track + AE_BGM_TRACK_CLEAR_COUNTER, 0", "track + AE_BGM_TRACK_CLEAR_COUNTER + 4u, 0"),
        ("SeqFadeTrackIn", "frame boundary", "frame_count >= 256", "frame_count > 256"),
        ("SeqFadeTrackIn", "unpaused path", "if (paused == 0)", "if (paused != 0)"),
    ]
    for function, label, before, after in specs:
        assert before in original, (function, label, before)
        yield function, label, original.replace(before, after, 1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--cases", type=float, default=1.0)
    parser.add_argument("--seed", type=int, default=94423)
    parser.add_argument("--trapv", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    start = time.time()
    if args.mutate:
        original = (ROOT / "src/sound/ae_bgm.c").read_text(encoding="utf-8")
        variants = list(mutation_sources(original))
        caught, survivors = [], []
        out = ROOT / "extract/mips"
        out.mkdir(parents=True, exist_ok=True)
        for index, (function, label, text) in enumerate(variants):
            source = out / f"bgm_mutation_{index}.c"
            source.write_text(text, encoding="utf-8")
            h = Harness(source, args.trapv)
            h.target = function
            try:
                scenarios(h, args.cases, args.seed)
            except (AssertionError, mr.MipsError) as error:
                caught.append((function, label))
                print(f"caught {function}: {label}: {str(error)[:130]}", flush=True)
            else:
                survivors.append((function, label))
                print(f"SURVIVED {function}: {label}", flush=True)
        print(f"Mutations: {len(variants)}; caught: {len(caught)}; survivors: {len(survivors)}")
        return 1 if survivors or len(variants) < 30 else 0

    h = Harness(trapv=args.trapv)
    scenarios(h, args.cases, args.seed)
    summary = h.finish(coverage=args.coverage,
                       default_run=args.cases == 1.0 and args.seed == 94423 and not args.trapv)
    for name, count in h.counts.items():
        print(f"{name:28s} {count}")
    gaps = any(row["uncovered_count"] or row["stale_declared"] for row in summary.values())
    if gaps:
        raise AssertionError("BGM instruction coverage is incomplete")
    print(f"PASS {sum(h.counts.values())} cases + {h.null_volume_checks} missing-hook checks in {time.time()-start:.2f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
