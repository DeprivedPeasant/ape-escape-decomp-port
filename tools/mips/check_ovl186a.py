"""Differential checks for the readable port of save-menu overlay 186a.

Every original MIPS entry is run from the SCUS 944.23 RAM image with a real
area member and member 186 loaded at their runtime addresses.  Hardware and
later-range calls are intercepted by name; instruction budgets bound every
oracle execution.
"""
import argparse
import ctypes as C
import csv
import hashlib
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import check_game as cg
import exclusion_registry as reg
import mips_ram as mr

CHECKER = "check_ovl186a"
M32 = 0xFFFFFFFF
AREA_BASE = 0x80154000
OVERLAY_BASE = 0x80136000
OVERLAY = (ROOT / "extract/members/186.bin").read_bytes()
AREAS = {1: 0x05B, 2: 0x05E, 3: 0x064, 4: 0x066}
ORACLE_BUDGET = 250_000
MUTATION_COUNT = 0
RUN_MENU_C_STATES = (2, 5, 11, 11, 11, 41, 41, 0, 0, 0, 5)
TEXT_INPUT = 0x801FE800
TABLE_INPUT = 0x801FE900
OUTPUT = 0x801FEA00
HEAP_FIXTURE = 0x801F0000

# name, entry, instruction bytes, shim index
FUNCS = [
    ("SaveUi_DrawText", 0x80136000, 288, 0),
    ("SaveUi_TextWidth", 0x80136120, 64, 1),
    ("SaveUi_LookAt", 0x801368E8, 416, 4),
    ("SaveUi_StringTableIndex", 0x80136A88, 48, 5),
    ("Mode_SaveMenu", 0x80136AB8, 672, 6),
    ("SaveUi_FrameReset", 0x80136D58, 56, 7),
    ("SaveUi_SceneInit", 0x80136D90, 328, 8),
    ("SaveUi_RunMenuA", 0x80136ED8, 556, 9),
    ("SaveUi_RunMenuB", 0x80137104, 520, 10),
    ("SaveUi_FileSelectInit", 0x8013730C, 280, 11),
    ("SaveUi_TotalMonkeyCount", 0x80137424, 48, 12),
    ("SaveUi_MenuUpdate", 0x80137454, 312, 13),
    ("SaveUi_RunMenuC", 0x8013758C, 620, 14),
    ("SaveUi_CountEntries", 0x801377F8, 84, 15),
    ("SaveUi_SetState", 0x8013784C, 1080, 16),
    ("SaveUi_FormatNumber3", 0x80137C84, 292, 17),
    ("SaveUi_MemCardStart", 0x80137DA8, 116, 18),
    ("SaveUi_StateTick", 0x80137E1C, 48, 19),
    ("SaveUi_StateMachine", 0x80137E4C, 1104, 20),
    ("SaveUi_OnCardResult_0", 0x801383E0, 220, 21),
    ("SaveUi_OnCardResult_1", 0x801384BC, 104, 22),
    ("SaveUi_OnCardResult_2", 0x80138524, 200, 23),
    ("SaveUi_OnCardResult_3", 0x801385EC, 224, 24),
    ("SaveUi_OnCardResult_4", 0x801386CC, 120, 25),
    ("SaveUi_OnCardResult_5", 0x80138744, 184, 26),
    ("SaveUi_OnCardResult_6", 0x801387FC, 144, 27),
    ("SaveUi_DrawFileSelect", 0x8013888C, 1332, 28),
    ("SaveUi_DrawSlot", 0x80138DC0, 1008, 29),
    # The two indirect worker entries are inside the requested address span.
    ("SaveUi_CardStateThread", 0x8013829C, 212, 30),
    ("SaveUi_CardResultThread", 0x80138370, 112, 31),
]
BY_NAME = {row[0]: row for row in FUNCS}
VOID = {"SaveUi_LookAt", "SaveUi_FrameReset", "SaveUi_FileSelectInit",
        "SaveUi_DrawFileSelect", "SaveUi_DrawSlot", "SaveUi_CardStateThread",
        "SaveUi_CardResultThread"}

PACKET_PATCH_RANGES = {
    "SaveUi_DrawText 80136090..801360EC",
    "SaveUi_DrawFileSelect 8013897C..801389E4",
    "SaveUi_DrawFileSelect 80138A70..80138BD4",
    "SaveUi_DrawFileSelect 80138C28..80138D7C",
    "SaveUi_DrawSlot 80138E50..80138EA8",
    "SaveUi_DrawSlot 80139044..801390A8",
}

PROVEN_UNREACHABLE = {
    "SaveUi_RunMenuA 80136FC0..80136FC4",
    "SaveUi_RunMenuC 801376A4..801376A8",
    "SaveUi_RunMenuC 801376C0..801376C4",
}

# Calls made by already-ported modules reached from this overlay.  These are
# platform leaves and retain their public API arguments at the hook boundary.
MODULE_HOOKS = {
    0x8002B52C: ("PadGetState", 1),
    0x8002B5F8: ("PadInfoMode", 3),
    0x80012CA0: ("PadSetupDualShock", 1),
    0x80014208: ("PadRumbleReset", 1),
    0x800A1CCC: ("SoundBankFree", 1),
    0x800A1F3C: ("SeqTrackClose", 1),
    0x800A2B6C: ("SeqTrackSetPlayState", 2),
    0x800A342C: ("SoundStopAllVoices", 1),
    0x800A1398: ("SpuWaitIdle", 0),
    0x8009D79C: ("SpuWaitLoop", 1),
    0x800315E0: ("SetBackColor", 3),
    0x80031600: ("SetColorMatrix", 1),
    0x80031630: ("SetFarColor", 3),
    0x800180B8: ("ModelEmitPrims", 4),
    0x8001D530: ("ModelEmitPrimsCycleColors", 4),
    0x800140B4: ("StickToButtons", 1),
    0x8008CBFC: ("DebugTextPrintfWhite", 2),
    0x8008CC60: ("DebugTextSetColorWrap", 2),
    0x8005EF28: ("DebugDrawStickGauge", 0),
    0x8005EC9C: ("DebugDrawStickHistory", 0),
}
NATIVE_RENDER_ROOTS = {
    0x80136160: ("SaveUi_DrawWindow", 4),
    0x80136554: ("SaveUi_DrawWindowBorder", 4),
}
FORCED_NATIVE_HOOKS = {
    0x80014208,  # PadRumbleReset drives the controller motor.
    0x8002E7C0,  # ResetGraphOrDisplay is part of the native display backend.
}

TRACE = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_char_p, C.c_uint32,
                    C.c_uint32, C.POINTER(C.c_uint32))
P, U, I = C.c_void_p, C.c_uint32, C.c_int32


def signed16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def parse_constants():
    values = {}
    files = (ROOT / "src/modes/ae_ovl186a.h", ROOT / "src/modes/ae_ovl186a.c")
    expressions = {}
    for path in files:
        source = path.read_text(encoding="utf-8")
        for match in __import__("re").finditer(
                r"^\s*#\s*define\s+([A-Z][A-Z0-9_]*)\s+([^\r\n]+)", source, __import__("re").M):
            name, expr = match.groups()
            expr = __import__("re").sub(r"/\*.*?\*/|//.*", "", expr, flags=__import__("re").S).strip()
            if expr and not expr.endswith("\\"):
                expressions[name] = expr
        for match in __import__("re").finditer(r"enum\s+\w+\s*\{([^}]*)\}", source, __import__("re").S):
            for item in match.group(1).split(","):
                item = __import__("re").sub(r"/\*.*?\*/|//.*", "", item, flags=__import__("re").S).strip()
                if "=" in item:
                    name, expr = (part.strip() for part in item.split("=", 1))
                    expressions[name] = expr
    for _ in range(len(expressions) + 1):
        changed = False
        for name, expr in expressions.items():
            if name in values:
                continue
            expr = __import__("re").sub(r"(?<=[0-9A-Fa-f])(?:u|ul|ull|l|ll)\b", "", expr, flags=__import__("re").I)
            if not __import__("re").fullmatch(r"[A-Za-z0-9_\s()+*/|&<>~-]+", expr):
                continue
            try:
                values[name] = int(eval(expr, {"__builtins__": {}}, values))
            except (NameError, SyntaxError, TypeError, ValueError, ZeroDivisionError):
                continue
            changed = True
        if not changed:
            break
    return values


def build_overlay_dll(source_overrides=None):
    overrides = {
        "tools/mips/game_integration_shim.c": ROOT / "tools/mips/ovl186a_shim.c",
        "src/game/ae_game_hooks.c": ROOT / "tools/mips/ovl_game_stub.c",
        "src/modes/ae_ovl186a.c": ROOT / "tools/mips/ovl186a_test_adapter.c",
    }
    overrides.update(source_overrides or {})
    out = cg.OUT
    out.mkdir(parents=True, exist_ok=True)
    with (ROOT / "tools/verified.csv").open(newline="", encoding="utf-8") as stream:
        modules = {row["source"] for row in csv.DictReader(stream)
                   if row["status"] == "diffed"}
    rels = sorted(set(cg.SOURCES) | modules | {"src/modes/ae_ovl186a.c"})
    dependencies = list((ROOT / "src").rglob("*.h"))
    dependencies += list((ROOT / "tools/mips").glob("game_integration_*.inc"))
    header_digest = hashlib.sha1()
    for header in sorted(dependencies):
        header_digest.update(str(header.relative_to(ROOT)).encode())
        header_digest.update(header.read_bytes())
    gcc = shutil.which("gcc")
    if gcc is None:
        raise SystemExit("gcc is required to build the overlay checker DLL")
    cache = out / "game_obj"
    cache.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
    if overrides:
        flags += ["-Wno-unused-function", "-Wno-unused-parameter"]
    objects = []
    for rel in rels:
        source = Path(overrides.get(rel, ROOT / rel))
        included_digest = b""
        if rel == "src/modes/ae_ovl186a.c" and "src/modes/ae_ovl186a.c" in overrides:
            included_digest = hashlib.sha1((ROOT / rel).read_bytes()).digest()
        key = hashlib.sha1(source.read_bytes() + header_digest.digest() + included_digest +
                           " ".join(flags).encode() + rel.encode()).hexdigest()[:16]
        obj = cache / f"{Path(rel).stem}_{key}.o"
        if not obj.exists():
            tmp = obj.with_suffix(f".{os.getpid()}.tmp")
            result = subprocess.run([gcc, *flags, "-iquote", str(ROOT / Path(rel).parent),
                                     "-c", str(source), "-o", str(tmp)],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    check=False)
            if result.returncode:
                raise SystemExit("compile error:\n" + result.stdout.decode(errors="replace"))
            os.replace(tmp, obj)
        objects.append(obj)
    dll = out / f"ovl186a_{os.getpid()}_{time.time_ns()}.dll"
    subprocess.run([gcc, "-shared", "-static-libgcc", *map(str, objects),
                    "-o", str(dll), "-lm"], check=True)
    return dll


class Harness:
    def __init__(self, dll=None, coverage=True):
        reg.declare("stack_window", "801FF000..80200000", checker=CHECKER)
        for where in PACKET_PATCH_RANGES:
            name, span = where.split(" ", 1)
            reg.declare("unreachable", where, checker=CHECKER)
        for where in PROVEN_UNREACHABLE:
            reg.declare("unreachable", where, checker=CHECKER)
        self.trace = TRACE(self._trace)
        self.shim = None
        self._bind_library(dll if dll is not None else build_overlay_dll())
        self.cpu = mr.MipsRam.from_extract()
        self.cpu.attach_gte_oracle()
        self.base = self.cpu.snapshot()
        self.cpu.coverage = set() if coverage else None
        self.c_events, self.m_events = [], []
        self.callback_error = None
        self.counters = {False: {}, True: {}}
        self.current_name = ""
        self.current_index = 0
        self.case_args = ()
        self.area_index = 1
        self.worker_loop_count = 0
        self.coverage_by_function = {name: set() for name, *_ in FUNCS}
        self.counts = {name: 0 for name, *_ in FUNCS}
        self.tested = 0
        self.constants = parse_constants()
        self.sites = self._parse_sites()
        self.hooks = {}
        for address, (name, argc) in self.sites.items():
            # A verified EXE port executes as original MIPS here. Unported
            # calls and later-overlay entries cross the named hook boundary.
            if (address >= 0x80100000 or address in FORCED_NATIVE_HOOKS or
                    not self.lib.integration_overlay_has_port(address)):
                self.hooks[address] = self._make_hook(address, name, argc)
        self.patches = {
            0x80136090: self._patch_draw_text,
            0x8013897C: self._patch_file_panel,
            0x80138A70: self._patch_file_slot,
            0x80138C28: self._patch_monkey_list,
            0x80138E50: self._patch_slot_header,
            0x80139044: self._patch_slot_detail,
            0x801382B8: self._patch_card_state_loop,
            0x801382C4: self._patch_card_state_busy,
            0x801383D8: self._patch_card_result_loop,
        }
        for address, (name, argc) in NATIVE_RENDER_ROOTS.items():
            self.hooks[address] = self._make_hook(address, name, argc)

    def _bind_library(self, dll):
        if self.shim:
            self.lib.integration_destroy(self.shim)
            self.shim = None
        self.lib = C.CDLL(str(dll))
        self.lib.integration_create.argtypes = [TRACE, P]
        self.lib.integration_create.restype = P
        self.lib.integration_destroy.argtypes = [P]
        self.lib.integration_destroy.restype = None
        self.lib.integration_ram.argtypes = [P]
        self.lib.integration_ram.restype = P
        self.lib.integration_overlay_call.argtypes = [P, U] + [U] * 7
        self.lib.integration_overlay_call.restype = I
        self.lib.integration_overlay_has_port.argtypes = [U]
        self.lib.integration_overlay_has_port.restype = I
        self.lib.integration_get_error.argtypes = [P, P, U]
        self.lib.integration_get_error.restype = I
        self.lib.integration_clear_error.argtypes = [P]
        self.lib.integration_clear_error.restype = None
        self.shim = self.lib.integration_create(self.trace, None)
        self.ram = self.lib.integration_ram(self.shim)
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(self.ram)
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(self.ram + mr.RAM_SIZE)

    def _parse_sites(self):
        import re
        source = (ROOT / "src/modes/ae_ovl186a.c").read_text(encoding="utf-8")
        pattern = re.compile(
            r"ae_ovl186a_(?:call|later)\(ram,\s*hooks,\s*\"([^\"]+)\"\s*,\s*([^,\s]+)\s*,\s*(\d+)u",
            re.S)
        sites = {}
        for name, token, argc in pattern.findall(source):
            token = re.sub(r"(?<=[0-9A-Fa-f])u\b", "", token, flags=re.I)
            try:
                address = int(token, 0) if token.lower().startswith("0x") else self.constants[token]
            except (KeyError, ValueError):
                continue
            old = sites.get(address)
            if old is not None and old[1] != int(argc):
                raise SystemExit(f"inconsistent arg count for hook {address:08X}: {old} / {(name, argc)}")
            sites.setdefault(address, (name, int(argc)))
        sites.update({address: spec for address, spec in MODULE_HOOKS.items()})
        return sites

    def _event(self, mips, name, address, args):
        target = self.m_events if mips else self.c_events
        words = [value & M32 for value in args]
        local_bytes = None
        # The renderer consumes ClearImage's 8-byte RECT by value. The original
        # passes a PS1 stack address; C stores the same value in its declared
        # stack window, whose host address is unrelated to the MIPS frame.
        if (name == "ClearImage" and words and
                0x801FF000 <= words[0] < 0x80200000):
            address_word = words[0]
            if mips:
                local_bytes = self.cpu.read_bytes(address_word, 8)
            else:
                offset = address_word - 0x80000000
                local_bytes = bytes(self.main[offset:offset + 8])
            words[0] = 0x801FF000
        target.append((name, address & M32, tuple(words), local_bytes))

    def _write_test_u16(self, mips, address, value):
        if mips:
            self.cpu.write16(address, value)
        else:
            offset = address - 0x80000000
            self.main[offset] = value & 0xFF
            self.main[offset + 1] = (value >> 8) & 0xFF

    def _answer(self, name, address, args, mips):
        counter = self.counters[mips]
        n = counter.get((name, address), 0)
        counter[(name, address)] = n + 1
        if (name == "SaveUi_ThreadSetHook" and
                self.current_name == "SaveUi_RunMenuC" and
                self.current_index >= 4):
            state = RUN_MENU_C_STATES[self.current_index - 4]
            if mips:
                self.cpu.write16(0x8013AF56, state)
            else:
                offset = 0x8013AF56 - 0x80000000
                self.main[offset] = state & 0xFF
                self.main[offset + 1] = (state >> 8) & 0xFF
            return 0
        if (name == "SaveUi_ThreadSetHook" and
                self.current_name == "Mode_SaveMenu" and
                self.current_index == 5 and address == 0x80141A10):
            self._write_test_u16(mips, 0x8013AF56, 2)
            return 0
        if name == "HeapAlloc":
            return HEAP_FIXTURE
        if name == "BgmGetVoice":
            return -1 if (self.current_name == "Mode_SaveMenu" and
                          self.current_index == 4) else 0
        if name in ("ResetGraph", "ResetGraphOrDisplay"):
            return 1
        if (name == "SaveUi_ModeStep" and
                self.current_name == "Mode_SaveMenu" and
                self.current_index in (6, 7, 8)):
            next_mode = ({6: (21 if n == 0 else 28),
                          7: 35, 8: 28}[self.current_index])
            self._write_test_u16(mips, 0x800F4470, next_mode)
            return 0
        if name == "LoadListStep":
            seq = (0, -1) if self.current_name == "Mode_SaveMenu" else (-1,)
            return seq[min(n, len(seq) - 1)]
        if name == "SaveUi_FadeTick":
            if (self.current_name == "Mode_SaveMenu" and
                    self.current_index in (5, 6, 7)):
                if n == 0 and self.current_index == 6:
                    self._write_test_u16(mips, 0x8013AF66, 1)
                    self._write_test_u16(mips, 0x8013AF52, 0)
                return 0 if n == 0 else -1
            if (self.current_name == "SaveUi_RunMenuA" and
                    self.current_index >= 4 and n == 0):
                self._write_test_u16(mips, 0x8013AF66, 1)
                self._write_test_u16(
                    mips, 0x8013AF52,
                    {4: 0, 5: 1, 6: 2, 7: 3, 8: 4}[self.current_index])
                return 0
            if (self.current_name in ("SaveUi_RunMenuA", "SaveUi_RunMenuB", "SaveUi_RunMenuC") and
                    (self.current_index >= 4 or self.current_index % 2)):
                return 0 if n == 0 else -1
            return -1
        if name == "SaveUi_ConfirmPressed":
            if self.current_name == "SaveUi_StateMachine":
                variant, state = divmod(self.current_index, 46)
                return int((state == 2 and variant in (0, 2)) or
                           (state in (9, 41, 42, 43, 44, 45) and variant in (1, 2)))
            if self.current_name == "SaveUi_RunMenuA":
                return int(self.current_index >= 4)
            if self.current_name == "SaveUi_MenuUpdate":
                return int(self.current_index == 5)
            if self.current_name == "SaveUi_RunMenuC":
                return int(self.current_index in (4, 9, 10))
            if self.current_name == "Mode_SaveMenu" and self.current_index in (5, 6):
                return 1
            return 0
        if name == "SaveUi_CancelPressed":
            return int((self.current_name == "SaveUi_MenuUpdate" and
                        self.current_index == 1) or
                       (self.current_name == "SaveUi_RunMenuA" and
                        self.current_index == 7))
        if name == "SaveUi_YesNoUpdate":
            if self.current_name == "SaveUi_StateMachine":
                return (0, 1, 2, -1)[self.current_index // 46]
            if self.current_name == "SaveUi_RunMenuB":
                return (0, 1, 2)[self.current_index % 3]
            if self.current_name == "Mode_SaveMenu" and self.current_index == 7:
                return 2
            if self.current_name == "SaveUi_RunMenuC":
                return {5: 1, 7: 2, 8: 1, 14: 1}.get(self.current_index, 0)
            return 0
        if name == "LevelProgressGet":
            return int(self.current_name == "SaveUi_FileSelectInit" and
                       self.current_index >= 2)
        if name == "SaveUi_NameEntryCheck":
            return int(self.current_name == "SaveUi_StateMachine" and
                       self.current_index // 46 == 3 and
                       self.current_index % 46 == 15)
        if name == "SaveUi_GetColorBuffer":
            return 0x80141554
        if name == "SaveUi_GetBuffer2":
            return 0x80141800
        if name == "SaveScore":
            return (0, 97, 1000, 204)[self.current_index % 4]
        if name == "SaveUi_LastUsedEntry":
            return self.current_index % 25
        if name == "VSync":
            return n + 1
        if name in ("PadGetState", "PadInfoMode"):
            return 0
        return 0

    def _trace(self, _user, raw_name, address, argc, raw_args):
        try:
            name = raw_name.decode("ascii")
            args = tuple(int(raw_args[i]) for i in range(argc))
            address = int(address) & M32
            self._event(False, name, address, args)
            return self._answer(name, address, args, False)
        except Exception as exc:  # surfaced after the C call, not through ctypes
            self.callback_error = str(exc)
            return 0

    def _make_hook(self, address, name, argc):
        def invoke(cpu, regs):
            args = [regs[4 + i] for i in range(min(argc, 4))]
            for i in range(4, argc):
                args.append(cpu.read32(regs[29] + i * 4))
            self._event(True, name, address, args)
            return self._answer(name, address, args, True)
        return invoke

    def _render_patch(self, name, args):
        self._event(True, name, 0, args)

    def _patch_draw_text(self, cpu, regs):
        glyph = regs[6] & 0xFF
        x = signed16(regs[5])
        y = signed16(regs[20])
        row = regs[19] & 0xFFFF
        self._render_patch("SaveUi_DrawGlyph", (glyph, x, y, row))
        width = cpu.read8(0x8013B024 + glyph)
        regs[17] = (regs[17] + 24) & M32
        regs[16] = (regs[16] + 1) & M32
        regs[6] = cpu.read8(regs[16])
        regs[2] = width
        return 0x801360F0

    def _patch_file_panel(self, cpu, regs):
        cursor = cpu.read32(0x800EC354)
        ot_index = cpu.read32(0x800EC35C)
        self._render_patch("SaveUi_FilePanelPacket", (cursor, ot_index))
        cpu.write32(0x800EC354, (cursor + 8) & M32)
        return 0x801389E8

    def _patch_file_slot(self, cpu, regs):
        cursor = cpu.read32(0x800EC354)
        slot = regs[17] & M32
        entry = regs[8] & M32
        save = regs[9] & M32
        self._render_patch("SaveUi_FileSlotPacket", (cursor, slot, entry, save))
        cpu.write32(0x800EC354, (cursor + 40) & M32)
        return 0x80138BD8

    def _patch_monkey_list(self, cpu, regs):
        cursor = cpu.read32(0x800EC354)
        index = regs[17] & M32
        selected = cpu.read32(0x800EC35C)
        self._render_patch("SaveUi_MonkeyListPacket", (cursor, index, selected))
        # The patch skips the load at 80138D7C; resume with its loaded cursor
        # in v0 so the game's own add/store pair can run after the patch.
        regs[2] = cursor
        return 0x80138D80

    def _patch_slot_header(self, cpu, regs):
        cursor = cpu.read32(0x800EC354)
        self._render_patch("SaveUi_SlotHeaderPacket", (cursor, regs[17], regs[21], regs[20]))
        # Resume at the final non-packet instruction in the replaced block;
        # the game's own cursor increment follows the block at 80138ED8.
        return 0x80138EAC

    def _patch_slot_detail(self, cpu, regs):
        cursor = cpu.read32(0x800EC354)
        self._render_patch("SaveUi_SlotDetailPacket", (cursor, regs[21], regs[20]))
        # The skipped packet prologue initializes t0 to the frame block; later
        # game logic uses it to advance the primitive cursor before DrawText.
        regs[8] = 0x800EC350
        return 0x801390AC

    def _unwind_worker(self, cpu, regs, frame_size, ra_offset):
        stack = regs[29] & M32
        regs[31] = cpu.read32(stack + ra_offset)
        regs[29] = (stack + frame_size) & M32
        regs[2] = 0
        return regs[31]

    def _patch_card_state_loop(self, cpu, regs):
        if self.worker_loop_count == 0:
            self.worker_loop_count = 1
            regs[3] = signed16(cpu.read16(0x8013AF56)) & M32
            return 0x801382C0
        return self._unwind_worker(cpu, regs, 32, 24)

    def _patch_card_state_busy(self, cpu, regs):
        # The branch is also the valid-state gate.  The previous patch always
        # unwound here, so the oracle never dispatched any worker state.
        if regs[2] != 0:
            return 0x801382CC
        return self._unwind_worker(cpu, regs, 32, 24)

    def _patch_card_result_loop(self, cpu, regs):
        return self._unwind_worker(cpu, regs, 32, 28)

    def _load(self, area):
        self.cpu.restore(self.base)
        self.cpu.load_area((ROOT / f"extract/members/{AREAS[area]:03X}.bin").read_bytes(), AREA_BASE)
        self.cpu.load_area(OVERLAY, OVERLAY_BASE)
        self.cpu._decoded.clear()

    def _setup_case(self, name, index):
        self.current_name, self.current_index = name, index
        self.area_index = index % len(AREAS) + 1
        self._load(self.area_index)
        cpu = self.cpu
        rng = random.Random(0x944230 + BY_NAME[name][1] + index * 257)
        cpu.write32(0x800F4470, 28)
        cpu.write16(0x8013AF56, 0)
        cpu.write16(0x8013AF50, 0)
        cpu.write16(0x8013AF52, 0)
        cpu.write16(0x8013AF66, 1)
        cpu.write16(0x8013AF68, 30)
        cpu.write16(0x800E3880, 0)
        cpu.write16(0x800E3882, 0)
        cpu.write16(0x800E3884, 4)
        cpu.write16(0x800E3886, index % 4)
        cpu.write32(0x800EC354, 0x801F1000)
        cpu.write32(0x800EC35C, index % 8)
        cpu.write_bytes(TEXT_INPUT, b"Ape Escape\0")
        cpu.write_bytes(TABLE_INPUT, b"\0first\0second\0last\0")
        cpu.write_bytes(OUTPUT, bytes(32))
        if name in ("SaveUi_DrawText", "SaveUi_TextWidth"):
            self.case_args = (TEXT_INPUT, -1 if index % 2 == 0 else 24,
                              48 + index, index % 3) if name == "SaveUi_DrawText" else (TEXT_INPUT,)
        elif name == "SaveUi_LookAt":
            eye, target = 0x801FEB00, 0x801FEB20
            vectors = (((0, 0, 0), (0, 0, 0)), ((1, 2, 3), (9, 8, 7)),
                       ((-100, 30, 200), (100, -30, -200)),
                       ((0, 0, 10), (0, 100, 10)),
                       ((32760, -32760, 32760), (-32760, 32760, -32760)))
            a, b = vectors[index % len(vectors)]
            for ptr, xyz in ((eye, a), (target, b)):
                for n, value in enumerate(xyz): cpu.write16(ptr + n * 2, value)
            self.case_args = (eye, target)
        elif name == "SaveUi_StringTableIndex":
            self.case_args = (TABLE_INPUT, index % 7)
        elif name == "Mode_SaveMenu":
            initial_modes = (21, 35, 28, 0, 21, 14, 22, 22, 22)
            cpu.write16(0x800F4470, initial_modes[index])
            self.case_args = (index * 17,)
        elif name == "SaveUi_SceneInit":
            if index >= 4:
                mesh_count = max(1, cpu.read8(AREA_BASE))
                cpu.write8(AREA_BASE, mesh_count)
                cpu.write16(AREA_BASE + 4, 0)
                cpu.write8(AREA_BASE + 6, 2)
            self.case_args = ()
        elif name == "SaveUi_FileSelectInit":
            self.case_args = ()
            if index >= 4:
                cpu.write8(0x800DFC88, index - 3)
        elif name == "SaveUi_RunMenuC":
            self.case_args = ((-1, 21, 35, 0)[index % 4],)
        elif name == "SaveUi_CountEntries":
            list_addr = 0x801FEC00
            entries = [1, 2, 4, 8, 0xFF] if index % 2 == 0 else [1, 2, 3, 4, 5, 0xFF]
            for pos, value in enumerate(entries): cpu.write8(list_addr + pos * 2, value)
            self.case_args = (list_addr, (0, 0x10, 0x1000, 0x1010)[index % 4])
        elif name == "SaveUi_FormatNumber3":
            self.case_args = ((0, 1, 9, 10, 99, 100, 999, 1000, -1, -100, 32767, -32768)[index % 12], OUTPUT)
        elif name == "SaveUi_SetState":
            self.case_args = (index % 46, (0, 2, 5, 35)[index % 4])
        elif name in ("SaveUi_CardStateThread", "SaveUi_CardResultThread"):
            self.worker_loop_count = 0
            if name == "SaveUi_CardStateThread":
                cpu.write16(0x8013AF56, index if index < 46 else -1)
            else:
                cpu.write16(0x800E3880, (-8 + index) & 0xFFFF)
        elif name.startswith("SaveUi_OnCardResult_"):
            cpu.write16(0x800E3880, (-8 + index) & 0xFFFF)
            self.case_args = ()
        elif name == "SaveUi_StateMachine":
            cpu.write16(0x8013AF56, index % 46)
            cpu.write16(0x800E3880, (index % 9))
            cpu.write16(0x800E3884, (0, 1, 4, 5)[index % 4])
            self.case_args = ()
        elif name == "SaveUi_MenuUpdate":
            cpu.write16(0x8013AF66, 1)
            cpu.write16(0x8013AF52, index % 3)
            cpu.write16(0x8013AF6C, index % 2)
            cpu.write32(0x800E5488, (0, 0x8000, 0x2000)[index % 3])
            self.case_args = ()
        elif name == "SaveUi_DrawSlot":
            view_mode = index // 8
            draw_mode = (index // 4) % 2
            slot = index % 4
            cpu.write16(0x8013AF5E, view_mode)
            entry = 0x8013B218 + slot * 120
            save = 0x800E1880 + slot * 0x7A0
            cpu.write16(entry + 0x0A, slot)
            cpu.write32(entry + 0x0C, (0xFF if view_mode == 2 else 0xA5) ^ index)
            cpu.write_bytes(entry + 0x30, bytes((ord('0') + slot, ord('1') + slot, 0, 0)))
            cpu.write_bytes(entry + 0x40, bytes((ord('A') + slot, ord('B') + slot, 0, 0)))
            cpu.write_bytes(entry + 0x70, b"123\0" + bytes(8))
            cpu.write_bytes(save + 0x198, bytes((ord('S'), ord('l'), ord('o'), ord('t'),
                                                 ord('0') + slot, 0, 0, 0)))
            self.case_args = (draw_mode, slot)
        elif name == "SaveUi_DrawFileSelect":
            cpu.write16(0x8013AF54, index % 2)
            cpu.write16(0x8013AF5E, index % 3)
            cpu.write16(0x800E3886, index % 4)
            for slot in range(4):
                save = 0x800E1880 + slot * 0x7A0
                entry = 0x8013B218 + slot * 120
                slot_present = (slot + index) % 2 == 0
                cpu.write16(save + 0x180, 0 if slot_present else 0xFFFF)
                cpu.write16(entry + 0x0A, slot if slot_present else 0xFFFF)
                cpu.write32(entry + 0x0C, (0xFF >> slot) ^ index)
                cpu.write_bytes(entry + 0x30, bytes((ord('0') + slot, ord('1') + slot, 0, 0)))
                cpu.write_bytes(entry + 0x40, bytes((ord('A') + slot, ord('B') + slot, 0, 0)))
                cpu.write_bytes(entry + 0x70, b"123\0" + bytes(8))
                cpu.write_bytes(save + 0x198, bytes((ord('S'), ord('l'), ord('o'), ord('t'),
                                                     ord('0') + slot, 0, 0, 0)))
            self.case_args = ()
        elif name == "SaveUi_MemCardStart":
            self.case_args = (index % 2,)
        else:
            self.case_args = ()
        if name in ("SaveUi_RunMenuA", "SaveUi_RunMenuB", "SaveUi_RunMenuC"):
            cpu.write16(0x8013AF66, 1)
        if name == "SaveUi_StateMachine":
            variant, state = divmod(index, 46)
            card_mode = (0, 1, 4, 5)[variant]
            card_result = (0, 1, 0, 0)[variant]
            cpu.write16(0x8013AF56, state)
            cpu.write16(0x800E3880, card_result)
            cpu.write16(0x800E3884, card_mode)
            cpu.write16(0x8013AF6A, 35 if state == 11 and variant == 2 else 0)
        if name == "SaveUi_RunMenuC" and index >= 4:
            scenarios = ((4, 0), (4, 0), (4, 0),
                         (4, 0), (4, 35), (4, 0),
                         (4, 0), (4, 0), (4, 0), (4, 0), (4, 0))
            card_mode, mode_argument = scenarios[index - 4]
            state = RUN_MENU_C_STATES[index - 4]
            cpu.write16(0x8013AF56, state)
            cpu.write16(0x800E3884, card_mode)
            cpu.write16(0x8013AF6A, mode_argument)
        if name == "SaveUi_SetState":
            # Seed valid save images and progress bytes for the state setup cases.
            for slot in range(4):
                save = 0x800E1880 + slot * 0x7A0
                cpu.write16(save + 0x180, 0 if slot % 2 == index % 2 else 0xFFFF)
                cpu.write8(save + 0x1C0, slot & 1)
                cpu.write32(save + 0x7E8, rng.getrandbits(32))
                cpu.write_bytes(save + 0x1C8, bytes((slot + j) & 0xFF for j in range(25)))
                cpu.write8(save + 0x1E8, 0xFF)
                if index == 48 and slot == 0:
                    # SaveScore is now a port, so its former constant hook
                    # result no longer reaches SetState's score==1000 glyphs.
                    image = save + 0x180
                    score = cpu.call(0x8009816C, (image,), limit=2_000_000)[2]
                    remaining = 1000 - score
                    for lane in range(25):
                        address = image + 0x48 + lane
                        before = cpu.read8(address)
                        after = min(255, max(0, before + remaining))
                        cpu.write8(address, after)
                        remaining -= after - before
                    assert remaining == 0, 'score-1000 save fixture cannot be represented'
            if 46 <= index < 49:
                selection_mode = (-1, 35, 0)[index - 46]
                self.case_args = (1, (2, 5, 0)[index - 46])
                cpu.write16(0x8013AF6A, selection_mode)
            if index == 49:
                self.case_args = (46, 0)
        if name == "SaveUi_RunMenuA" and index >= 4:
            cpu.write16(0x8013AF52, index - 4)
        if name == "SaveUi_MenuUpdate" and index >= 6:
            cpu.write16(0x8013AF52, 2 if index == 6 else 0)
            cpu.write16(0x8013AF6C, 0)
            cpu.write32(0x800E5488, 0x8000 if index == 6 else 0x2000)
        if name == "SaveUi_MenuUpdate" and index == 8:
            cpu.write16(0x8013AF66, 0)
        if name == "SaveUi_OnCardResult_0" and index == 10:
            cpu.write16(0x800E3880, 0)
            cpu.write16(0x800E3884, 0)
        if name == "SaveUi_OnCardResult_3" and index == 10:
            cpu.write16(0x800E3880, 0)
            cpu.write16(0x800E3884, 5)
        if name == "SaveUi_OnCardResult_6" and index >= 10:
            results = (0, 0, 0xFFFE, 1)
            timers = (1, 2, 2, 2)
            cpu.write16(0x800E3880, results[index - 10])
            cpu.write16(0x8013AF5C, timers[index - 10])
        self.counters = {False: {}, True: {}}
        self.c_events.clear()
        self.m_events.clear()
        self.callback_error = None
        self.lib.integration_clear_error(self.shim)

    def _mips_args(self, name, args):
        if name == "SaveUi_DrawText": return args
        if name == "SaveUi_TextWidth": return args
        if name == "SaveUi_LookAt": return args
        if name == "SaveUi_StringTableIndex": return args
        if name in ("Mode_SaveMenu", "SaveUi_RunMenuC", "SaveUi_CountEntries",
                    "SaveUi_FormatNumber3", "SaveUi_SetState", "SaveUi_MemCardStart",
                    "SaveUi_DrawSlot"):
            return args
        return ()

    def run_one(self, name, index):
        row = BY_NAME[name]
        self._setup_case(name, index)
        args = self.case_args
        c_args = list(args[:7]) + [0] * max(0, 7 - len(args))
        C.memmove(self.main, bytes(self.cpu.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(self.cpu.scratch), mr.SCRATCH_SIZE)
        c_result = self.lib.integration_overlay_call(self.shim, row[3], *c_args)
        all_coverage = self.cpu.coverage
        case_coverage = set() if all_coverage is not None else None
        if case_coverage is not None:
            self.cpu.coverage = case_coverage
        try:
            m_result = self.cpu.call(row[1], self._mips_args(name, args),
                                     hooks=self.hooks, patches=self.patches,
                                     limit=ORACLE_BUDGET)[2]
        except mr.MipsError as exc:
            machine = getattr(self.cpu, "_last_machine", None)
            at = "unknown" if machine is None else f"pc={machine[0]:08X}"
            raise AssertionError(f"{name} case {index}: {exc} ({at}), budget={ORACLE_BUDGET}") from exc
        finally:
            if case_coverage is not None:
                if self.cpu.coverage is not None: case_coverage.update(self.cpu.coverage)
                self.cpu.coverage = all_coverage
        if case_coverage is not None:
            self.coverage_by_function[name].update(case_coverage)
        error_buffer = C.create_string_buffer(512)
        if self.lib.integration_get_error(self.shim, error_buffer, len(error_buffer)):
            raise AssertionError(f"{name} case {index}: AeRam error: {error_buffer.value.decode(errors='replace')}")
        if self.callback_error:
            raise AssertionError(f"{name} case {index}: callback error: {self.callback_error}")
        if name not in VOID and (int(c_result) & M32) != (int(m_result) & M32):
            raise AssertionError(f"{name} case {index}: return {int(c_result)&M32:08X} != {int(m_result)&M32:08X}")
        a, b = bytearray(self.cpu.ram), bytearray(self.main)
        a[0x1FF000:] = b"\0" * (mr.RAM_SIZE - 0x1FF000)
        b[0x1FF000:] = b"\0" * (mr.RAM_SIZE - 0x1FF000)
        if a != b:
            offsets = [i for i, (x, y) in enumerate(zip(a, b)) if x != y]
            raise AssertionError(f"{name} case {index}: RAM differs at " +
                                 str([(f"{0x80000000+i:08X}", a[i], b[i]) for i in offsets[:20]]) +
                                 f"; hooks equal={self.c_events == self.m_events}")
        if bytes(self.cpu.scratch) != bytes(self.scratch):
            raise AssertionError(f"{name} case {index}: scratchpad differs")
        if self.c_events != self.m_events:
            ix = next((i for i, pair in enumerate(zip(self.c_events, self.m_events)) if pair[0] != pair[1]),
                      min(len(self.c_events), len(self.m_events)))
            raise AssertionError(f"{name} case {index}: hook sequence differs at {ix}: "
                                 f"C={self.c_events[max(0,ix-2):ix+3]} M={self.m_events[max(0,ix-2):ix+3]}")
        self.counts[name] += 1
        self.tested += 1

    def finish(self, default_run=False, show_coverage=False):
        items = [(name, address, size, True) for name, address, size, _ in FUNCS]
        summary = reg.coverage_summary(items, set().union(*self.coverage_by_function.values()))
        incomplete = [(name, ent["uncovered"]) for name, ent in summary.items()
                      if ent["own"] and ent["uncovered_count"]]
        stale = [(name, ent["stale_declared"]) for name, ent in summary.items()
                 if ent["own"] and ent["stale_declared"]]
        if incomplete or stale:
            raise AssertionError(f"coverage gaps: uncovered={incomplete}; stale={stale}")
        if show_coverage:
            reg.print_coverage(summary)
        result = None
        if default_run:
            result = reg.write_results(self.counts, summary, True,
                                       extra={"areas": list(AREAS.values()),
                                              "comparisons": self.tested,
                                              "instruction_budget": ORACLE_BUDGET,
                                              "mutations": MUTATION_COUNT},
                                       function_names=[name for name, *_ in FUNCS])
        return summary, result


def case_count(name):
    if name == "SaveUi_DrawText": return 5
    if name == "SaveUi_TextWidth": return 5
    if name == "SaveUi_LookAt": return 5
    if name == "SaveUi_StringTableIndex": return 7
    if name == "Mode_SaveMenu": return 9
    if name == "SaveUi_SceneInit": return 8
    if name == "SaveUi_RunMenuA": return 9
    if name == "SaveUi_RunMenuB": return 8
    if name == "SaveUi_FileSelectInit": return 6
    if name == "SaveUi_RunMenuC": return 15
    if name == "SaveUi_CountEntries": return 4
    if name == "SaveUi_FormatNumber3": return 12
    if name == "SaveUi_SetState": return 50
    if name == "SaveUi_StateMachine": return 184
    if name in ("SaveUi_OnCardResult_0", "SaveUi_OnCardResult_3"): return 11
    if name == "SaveUi_OnCardResult_6": return 14
    if name.startswith("SaveUi_OnCardResult_"): return 10
    if name == "SaveUi_CardStateThread": return 48
    if name == "SaveUi_CardResultThread": return 10
    if name == "SaveUi_MenuUpdate": return 9
    if name == "SaveUi_DrawSlot": return 24
    if name == "SaveUi_DrawFileSelect": return 8
    return 4


def function_span(source, name):
    signature = re.compile(
        r"(?m)^[ \t]*(?:static[ \t]+)?[A-Za-z_][A-Za-z0-9_ \t*]*?\b" +
        re.escape(name) + r"\s*\(")
    match = signature.search(source)
    if match is None:
        raise ValueError(f"function {name} not found")
    opening = source.find("{", match.end())
    if opening < 0:
        raise ValueError(f"function {name} has no body")
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return match.start(), offset + 1, opening
    raise ValueError(f"function {name} has an unterminated body")


def mutate_return(source, implementation, ordinal):
    start, end, _ = function_span(source, implementation)
    body = source[start:end]
    returns = list(re.finditer(r"\breturn\s+([^;]+);", body))
    if ordinal >= len(returns):
        raise ValueError(f"{implementation} has no return expression {ordinal + 1}")
    match = returns[ordinal]
    expression = match.group(1).strip()
    replacement = f"return (({expression}) ^ 1u);"
    return source[:start + match.start()] + replacement + source[start + match.end():]


def mutate_spurious_store(source, implementation, ordinal):
    _, _, opening = function_span(source, implementation)
    address = 0x801FEA80 + ordinal
    value = 0x5A + ordinal
    mutation = f"\n    ae_wr8(ram, 0x{address:08X}u, 0x{value:02X}u);"
    return source[:opening + 1] + mutation + source[opening + 1:]


def mutation_specs(source):
    worker_steps = {
        "SaveUi_CardStateThread": "ae_ovl186a_card_state_thread_step",
        "SaveUi_CardResultThread": "ae_ovl186a_card_result_thread_step",
    }
    specs = []
    for name, *_ in FUNCS:
        implementation = worker_steps.get(name, f"ae_{name}")
        if name in worker_steps:
            specs.append((name, implementation, "store", 0, 0))
            specs.append((name, implementation, "store", 1, 0))
            continue
        start, end, _ = function_span(source, implementation)
        body = source[start:end]
        return_count = len(re.findall(r"\breturn\s+([^;]+);", body))
        if return_count:
            specs.append((name, implementation, "return", 0, 0))
            if return_count > 1:
                specs.append((name, implementation, "return", 1, 0))
            else:
                specs.append((name, implementation, "store", 0, 0))
        else:
            specs.append((name, implementation, "store", 0, 0))
            specs.append((name, implementation, "store", 1, 0))
    if len(specs) != len(FUNCS) * 2:
        raise AssertionError(f"expected two mutations per function, got {len(specs)}")
    return specs


def run_mutations():
    source_path = ROOT / "src/modes/ae_ovl186a.c"
    source = source_path.read_text(encoding="utf-8")
    adapter_template = (ROOT / "tools/mips/ovl186a_test_adapter.c").read_text(encoding="utf-8")
    include_line = '#include "../../src/modes/ae_ovl186a.c"'
    if adapter_template.count(include_line) != 1:
        raise SystemExit("ovl186a test adapter include anchor changed")
    cg.OUT.mkdir(parents=True, exist_ok=True)
    mutation_dir = Path(tempfile.mkdtemp(prefix="ovl186a_mut_", dir=cg.OUT))
    specs = mutation_specs(source)
    base_dll = build_overlay_dll()
    harness = Harness(dll=base_dll, coverage=False)
    survivors = []
    started = time.time()
    for index, (name, implementation, kind, ordinal, target_case) in enumerate(specs):
        label = (f"return site {ordinal + 1} low bit" if kind == "return"
                 else f"unexpected RAM write {ordinal + 1}")
        try:
            if kind == "return":
                mutated = mutate_return(source, implementation, ordinal)
            else:
                mutated = mutate_spurious_store(source, implementation, ordinal)
        except ValueError as exc:
            print(f"mutation {name} / {label}: SURVIVED ({exc})", flush=True)
            survivors.append(f"{name}: {kind} {ordinal + 1}")
            continue

        mutant_source = mutation_dir / f"mutant_{index:02d}.c"
        mutant_adapter = mutation_dir / f"adapter_{index:02d}.c"
        mutant_source.write_text(mutated, encoding="utf-8")
        adapter = adapter_template.replace(include_line, f'#include "{mutant_source.name}"')
        mutant_adapter.write_text(adapter, encoding="utf-8")
        try:
            dll = build_overlay_dll({"src/modes/ae_ovl186a.c": mutant_adapter})
        except (OSError, SystemExit, subprocess.CalledProcessError) as exc:
            print(f"mutation {name} / {label}: SURVIVED (build failed: {exc})", flush=True)
            survivors.append(f"{name}: {label}")
            continue

        caught_at = None
        try:
            harness._bind_library(dll)
            cases = [target_case] + [case for case in range(case_count(name))
                                     if case != target_case]
            for case in cases:
                try:
                    harness.run_one(name, case)
                except AssertionError:
                    caught_at = case
                    break
        except Exception as exc:  # build and oracle failures are reported as a surviving mutant
            print(f"mutation {name} / {label}: SURVIVED (harness error: {exc})", flush=True)
        if caught_at is None:
            print(f"mutation {name} / {label}: SURVIVED (checker too weak)", flush=True)
            survivors.append(f"{name}: {label}")
        else:
            print(f"mutation {name} / {label}: caught in case {caught_at}", flush=True)
    harness._bind_library(base_dll)
    print(f"{len(specs) - len(survivors)}/{len(specs)} mutations caught in {time.time() - started:.1f}s")
    return len(specs) - len(survivors), survivors, harness


def run_checker(args, harness=None):
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        raise SystemExit("missing extract/disc/SCUS_944.23")
    if harness is None:
        harness = Harness()
    else:
        harness.cpu.coverage = set() if args.coverage else None
        harness.coverage_by_function = {name: set() for name, *_ in FUNCS}
        harness.counts = {name: 0 for name, *_ in FUNCS}
        harness.tested = 0
    for name, *_ in FUNCS:
        for index in range(case_count(name)):
            harness.run_one(name, index)
        print(f"{name}: {harness.counts[name]} cases", flush=True)
    _, result = harness.finish(default_run=True, show_coverage=args.coverage)
    print(f"Overlay comparisons: {harness.tested}; result: {result}")
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    harness = None
    if args.mutate:
        count, survivors, harness = run_mutations()
        if survivors:
            print("SURVIVORS: " + ", ".join(survivors))
            if harness.shim:
                harness.lib.integration_destroy(harness.shim)
                harness.shim = None
            return 1
        global MUTATION_COUNT
        MUTATION_COUNT = count
    return run_checker(args, harness)


if __name__ == "__main__":
    sys.exit(main())
