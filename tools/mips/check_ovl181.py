"""Differential checks for the readable ports of overlay members 181 and 182.

Run with .venv/Scripts/python.exe tools/mips/check_ovl181.py. The MIPS side loads
each member at 0x80136000 on top of SCUS_944.23 and one of four real area blobs.
Native GPU packet blocks are intercepted as named renderer events; game state
and packet cursors remain in the RAM comparison.
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

CHECKER = "check_ovl181"
M32 = 0xFFFFFFFF
AREAS = {1: 0x05B, 2: 0x05E, 4: 0x064, 5: 0x066}
PACKET_MASK = (0x801FA000, 0x5000)
ORDERING_TABLE_BASE = 0x800EC3D0
DISPLAY_STRIDE = 16504
AREA_BASE = 0x80154000
PACKET_START = 0x801FC000
OT_HEAD_POINTER = 0x801FA000
GAME_GLOBALS = 0x800F4350
DISPLAY_GLOBALS = 0x800EC350
BOOT_DISPLAY_STATE = 0x800E38B0
PAD_STATE = 0x800E5320
OVERLAY_STATE = 0x80137810
OVERLAY_SELECTION = 0x80137814
OVERLAY_FADE_STATE = 0x80137818
OVERLAY_FADE_TIMER = 0x8013781A
OVERLAY_FRAME = 0x8013781C
OVERLAY_DRAW_INDEX = 0x8013784C
OVERLAY_SCROLL = 0x80137848
WARNING_COLOR = 0x80137270
WARNING_DISPLAY_MODE_OFFSET = 10

FUNCS = [
    ("Mode_Boot", 0x181, 0x80136000, 676, 0),
    ("Boot_ClearScreen", 0x181, 0x801362A4, 384, 1),
    ("Boot_SetupDisplay", 0x181, 0x80136424, 260, 2),
    ("Boot_DrawLogoA", 0x181, 0x80136528, 452, 3),
    ("Boot_DrawLogoB", 0x181, 0x801366EC, 424, 4),
    ("Mode_02", 0x181, 0x80136894, 124, 5),
    ("Mode_03", 0x182, 0x80136000, 1356, 6),
    ("Ovl182_DrawScene3D", 0x182, 0x8013654C, 512, 7),
    ("Ovl182_UpdateCamera", 0x182, 0x8013674C, 884, 8),
    ("Ovl182_Init", 0x182, 0x80136AC0, 412, 9),
    ("Ovl182_StateMachine", 0x182, 0x80136C5C, 164, 10),
    ("Ovl182_DrawIntroState", 0x182, 0x80136D00, 596, 11),
    ("Ovl182_MenuInput", 0x182, 0x80136F54, 1424, 12),
    ("Ovl182_FadeTick", 0x182, 0x801374E4, 224, 13),
    ("Ovl182_DrawFade", 0x182, 0x801375C4, 300, 14),
    ("Ovl181_CheckCdHardwareIntegrity", 0x181, 0x80136940, 0x1AC, 15),
    ("Ovl181_DrawIntegrityWarning", 0x181, 0x80136D18, 0x148, 17),
]
BY_NAME = {row[0]: row for row in FUNCS}
RETURNS = {"Ovl182_StateMachine", "Ovl182_DrawIntroState", "Ovl182_FadeTick",
           *(row[0] for row in FUNCS if row[0].startswith("Ovl181_"))}
INLINE_PATCHES = [
    ("Mode_03", 0x801361F4, 0x801362A8),
    ("Ovl182_DrawScene3D", 0x80136648, 0x80136718),
    ("Ovl182_DrawIntroState", 0x80136D00, 0x80136E78),
    ("Ovl181_DrawIntegrityWarning", 0x80136DB4, 0x80136DD0),
    ("Ovl182_DrawFade", 0x80137634, 0x80137660),
    ("Ovl182_DrawFade", 0x80137698, 0x801376EC),
]
EXTRA_COVERAGE = {}

NATIVE_ENTRIES = {
    "Ovl181_CdCommand": ("AE_OVL181_CD_COMMAND", 1),
    "Ovl181_DrawEncodedWarningText": ("AE_OVL181_DRAW_WARNING_TEXT", 2),
    "Ovl181_BuildWarningGlyphPacket": ("AE_OVL181_BUILD_WARNING_GLYPH", 4),
    "Ovl181_BuildWarningBackdrop": ("AE_OVL181_BUILD_WARNING_BACKDROP", 0),
    "Ovl181_InstallCdExceptionVector": ("AE_OVL181_CD_EXCEPTION_VECTOR", 0),
    "Ovl181_RegisterInitialCdEvent": ("AE_OVL181_CD_EVENT_INSTALL", 0),
    "Ovl181_RegisterCdReadEvent": ("AE_OVL181_CD_EVENT_READ", 0),
    "Ovl181_RegisterCdRetryEvent": ("AE_OVL181_CD_EVENT_RETRY", 0),
    "Ovl181_RegisterCdCompleteEvent": ("AE_OVL181_CD_EVENT_COMPLETE", 0),
    "Ovl181_HandleCdStatusEvent": ("AE_OVL181_CD_EVENT_STATUS", 2),
    "Ovl181_HandleCdPositionEvent": ("AE_OVL181_CD_EVENT_POSITION", 2),
    "Ovl181_HandleCdModeEvent": ("AE_OVL181_CD_EVENT_MODE", 2),
    "Ovl181_HandleCdSectorEvent": ("AE_OVL181_CD_EVENT_SECTOR", 3),
}
NATIVE_ENTRY_EXCLUSIONS = (
    "Ovl181_CdCommand 80136AEC..80136D14",
    "Ovl181_DrawEncodedWarningText 80136E60..80136F9C",
    "Ovl181_BuildWarningGlyphPacket 80136FA0..80137010",
    "Ovl181_BuildWarningBackdrop 80137014..80137198",
    "Ovl181_InstallCdExceptionVector 80137280..801372AC",
    "Ovl181_RegisterInitialCdEvent 801372F0..80137334",
    "Ovl181_RegisterCdReadEvent 80137338..80137358",
    "Ovl181_RegisterCdRetryEvent 8013735C..8013737C",
    "Ovl181_RegisterCdCompleteEvent 80137380..801373A0",
    "Ovl181_HandleCdStatusEvent 801373A4..80137420",
    "Ovl181_HandleCdPositionEvent 80137424..8013748C",
    "Ovl181_HandleCdModeEvent 80137490..8013753C",
    "Ovl181_HandleCdSectorEvent 80137540..80137588",
)
RENDER_HOOKS = {
    "Ovl181_IntegrityWarningClearPacket",
    "Ovl181_DrawEncodedWarningText",
    "Ovl181_BuildWarningGlyphPacket",
    "Ovl181_BuildWarningBackdrop",
    "OverlayFadeQuad",
}

# Two semantic mutations per original function.  Each one is run against its
# target case first; the remaining default cases for that function are tried if
# the target case does not expose it.
MUTATIONS = [
    ("Mode_Boot", "boot field 164", "ae_wr16(ram, AE_GAME_BOOT_FIELD_164, 0u);",
     "ae_wr16(ram, AE_GAME_BOOT_FIELD_164, 1u);", 0),
    ("Mode_Boot", "initial VSync mode",
     'CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);\n'
     '    int32_t start_vblank = CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0xFFFFFFFFu);',
     'CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 1u);\n'
     '    int32_t start_vblank = CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0xFFFFFFFFu);', 0),
    ("Boot_ClearScreen", "display index", "ae_wr32(ram, AE_DISPLAY_INDEX, 1u);",
     "ae_wr32(ram, AE_DISPLAY_INDEX, 0u);", 0),
    ("Boot_ClearScreen", "load-list id", 'CALL2(ram, hooks, "LoadListStart", AE_ADDR_LOAD_LIST_START, 0u, 0u);',
     'CALL2(ram, hooks, "LoadListStart", AE_ADDR_LOAD_LIST_START, 0u, 1u);', 0),
    ("Boot_SetupDisplay", "graphics flag", "ae_wr8(ram, AE_BOOT_GFX_FLAG_A, 1u);",
     "ae_wr8(ram, AE_BOOT_GFX_FLAG_A, 0u);", 0),
    ("Boot_SetupDisplay", "draw environment width",
     'CALL5(ram, hooks, "SetDefDrawEnv", AE_ADDR_SET_DEF_DRAW_ENV,\n'
     '              draw_env, 0u, 0u, 640u, 480u);',
     'CALL5(ram, hooks, "SetDefDrawEnv", AE_ADDR_SET_DEF_DRAW_ENV,\n'
     '              draw_env, 0u, 0u, 639u, 480u);', 0),
    ("Boot_DrawLogoA", "logo frame limit", "if (frame >= AE_LOGO_A_FRAMES) break;",
     "if (frame >= AE_LOGO_A_FRAMES - 1) break;", 0),
    ("Boot_DrawLogoA", "frame step", "++frame;", "frame += 2;", 1),
    ("Boot_DrawLogoB", "TIM archive member",
     'CALL2(ram, hooks, "ArcLoadMember", AE_ADDR_ARC_LOAD_MEMBER, 34u,',
     'CALL2(ram, hooks, "ArcLoadMember", AE_ADDR_ARC_LOAD_MEMBER, 35u,', 0),
    ("Boot_DrawLogoB", "logo frame limit", "if (frame >= AE_LOGO_B_FRAMES) break;",
     "if (frame >= AE_LOGO_B_FRAMES - 1) break;", 0),
    ("Mode_02", "minigame argument",
     'CALL1(ram, hooks, "RunMinigameExe", AE_ADDR_RUN_MINIGAME, 1u);',
     'CALL1(ram, hooks, "RunMinigameExe", AE_ADDR_RUN_MINIGAME, 0u);', 0),
    ("Mode_02", "next game mode", "ae_wr16(ram, AE_GAME_MODE, 3u);",
     "ae_wr16(ram, AE_GAME_MODE, 4u);", 0),
    ("Mode_03", "initial overlay state", "ae_wr32(ram, AE_OVL182_STATE, 0u);",
     "ae_wr32(ram, AE_OVL182_STATE, 1u);", 0),
    ("Mode_03", "initial frame", "ae_wr16(ram, AE_OVL182_FRAME, 0u);",
     "ae_wr16(ram, AE_OVL182_FRAME, 1u);", 0),
    ("Ovl182_DrawScene3D", "scroll advance", "uint32_t scroll = old_scroll + 64u;",
     "uint32_t scroll = old_scroll + 65u;", 0),
    ("Ovl182_DrawScene3D", "quad packet size",
     "AE_OVL182_WATER_QUAD_PACKET_SIZE);",
     "(AE_OVL182_WATER_QUAD_PACKET_SIZE + 1u));", 0),
    ("Ovl182_UpdateCamera", "camera sine phase",
     "int32_t first_sine = ae_sdk_rsin(ram, (int32_t)(phase << 3));",
     "int32_t first_sine = ae_sdk_rsin(ram, (int32_t)(phase << 2));", 2),
    ("Ovl182_UpdateCamera", "camera position",
     "ae_wr32(ram, camera + AE_OVL182_MODEL_POSITION_Z_OFFSET, 512u);",
     "ae_wr32(ram, camera + AE_OVL182_MODEL_POSITION_Z_OFFSET, 513u);", 2),
    ("Ovl182_Init", "animation allocation end",
     "animation_count * AE_OVL182_ANIMATION_RECORD_STRIDE +",
     "(animation_count + 1u) * AE_OVL182_ANIMATION_RECORD_STRIDE +", 0),
    ("Ovl182_Init", "model bounds",
     "ae_wr16(ram, output + AE_OVL182_MODEL_INIT_BOUNDS_OFFSET, 0xF000u);",
     "ae_wr16(ram, output + AE_OVL182_MODEL_INIT_BOUNDS_OFFSET, 0xE000u);", 0),
    ("Ovl182_StateMachine", "state one result", "case 1: return -3;",
     "case 1: return -4;", 1),
    ("Ovl182_StateMachine", "state two result", "case 2: return -6;",
     "case 2: return -7;", 2),
    ("Ovl182_DrawIntroState", "fade gate", "if (fade_state != 1) return 0;",
     "if (fade_state == 1) return 0;", 2),
    ("Ovl182_DrawIntroState", "confirm input",
     "if ((input & AE_OVL182_INPUT_CONFIRM) != 0u) {",
     "if ((input & AE_OVL182_INPUT_CONFIRM) == 0u) {", 2),
    ("Ovl182_MenuInput", "right cursor input",
     "if ((input & AE_OVL182_INPUT_RIGHT) != 0u) {",
     "if ((input & AE_OVL182_INPUT_LEFT) != 0u) {", 7),
    ("Ovl182_MenuInput", "stick vibration option",
     "uint32_t value = ae_rd8(ram, AE_GAME_VIBRATION_STICK_MODE) ^ 1u;",
     "uint32_t value = ae_rd8(ram, AE_GAME_VIBRATION_STICK_MODE) ^ 0u;", 5),
    ("Ovl182_FadeTick", "initial fade state",
     "if (state == 1) return 0;", "if (state == 0) return 0;", 0),
    ("Ovl182_FadeTick", "fade completion edge", "if (timer < 0) {",
     "if (timer <= 0) {", 0),
    ("Ovl182_DrawFade", "fade alpha",
     "int32_t alpha = (timer * 255) / AE_OVL182_INTRO_FADE;",
     "int32_t alpha = (timer * 254) / AE_OVL182_INTRO_FADE;", 4),
    ("Ovl182_DrawFade", "fade packet size",
     "draw_environment + AE_OVL182_FADE_PACKET_SIZE);",
     "draw_environment + (AE_OVL182_FADE_PACKET_SIZE + 1u));", 0),
    ("Ovl182_DrawFade", "fade packet mode",
     "uint32_t fade_mode = (fade_state == 0 || fade_state == 3) ?\n"
     "                         0xE1000220u : 0xE1000240u;",
     "uint32_t fade_mode = (fade_state == 0 || fade_state == 3) ?\n"
     "                         0xE1000240u : 0xE1000220u;", 0),
]
MUTATIONS.extend([
    ("Ovl181_CheckCdHardwareIntegrity", "saved command argument",
     "ae_wr8(ram, AE_OVL181_CD_ARGUMENT0, (uint8_t)arg0);",
     "ae_wr8(ram, AE_OVL181_CD_ARGUMENT0, (uint8_t)(arg0 + 1u));", 0),
    ("Ovl181_CheckCdHardwareIntegrity", "CD status error mask",
     "(status & 0xF0u) != 0u", "(status & 0xE0u) != 0u", 4),
    ("Ovl181_DrawIntegrityWarning", "GetVideoMode display flag",
     "NATIVE0(ram, hooks, \"GetVideoMode\", AE_ADDR_GET_VIDEO_MODE) == 1",
     "NATIVE0(ram, hooks, \"GetVideoMode\", AE_ADDR_GET_VIDEO_MODE) == 0", 1),
    ("Ovl181_DrawIntegrityWarning", "warning color state",
     "ae_wr32(ram, AE_OVL181_WARNING_COLOR, 0x8000u);",
     "ae_wr32(ram, AE_OVL181_WARNING_COLOR, 0x8001u);", 0),
])

TRACE = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_char_p, C.c_uint32,
                    C.c_uint32, C.POINTER(C.c_uint32))
P, U, I = C.c_void_p, C.c_uint32, C.c_int32


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def norm(value):
    value &= M32
    return "stack" if 0x801FF000 <= value < 0x80200000 else value


def parse_constants():
    expressions = {}
    for rel in ("src/modes/ae_overlay_layout.h", "src/modes/ae_ovl181.c",
                "src/modes/ae_ovl182.c"):
        source = (ROOT / rel).read_text(encoding="utf-8")
        for enum in re.finditer(r"enum\s*\{([^}]*)\}", source, re.S):
            for item in enum.group(1).split(","):
                item = re.sub(r"/\*.*?\*/|//.*", "", item, flags=re.S).strip()
                if "=" not in item:
                    continue
                name, expr = (part.strip() for part in item.split("=", 1))
                if re.fullmatch(r"[A-Z][A-Z0-9_]*", name):
                    expressions[name] = expr
        for define in re.finditer(
                r"^\s*#\s*define\s+([A-Z][A-Z0-9_]*)\s+([^\r\n]+)",
                source, re.M):
            name, expr = define.groups()
            expr = re.sub(r"/\*.*?\*/|//.*", "", expr).strip()
            if expr and not expr.endswith("\\"):
                expressions[name] = expr

    values = {}
    unresolved = True
    while unresolved:
        unresolved = False
        for name, expr in expressions.items():
            if name in values:
                continue
            expr = re.sub(r"(?<=[0-9A-Fa-f])(?:u|ul|ull|l|ll)\b", "", expr,
                          flags=re.I)
            if not re.fullmatch(r"[A-Za-z0-9_\s()+*/|&<>~-]+", expr):
                continue
            try:
                value = int(eval(expr, {"__builtins__": {}}, values))
            except (NameError, SyntaxError, TypeError, ValueError, ZeroDivisionError):
                continue
            values[name] = value
            unresolved = True
    return values


def parse_hook_sites():
    constants = parse_constants()
    sites = {}
    pattern = re.compile(
        r"\b(?:CALL|OVL|NATIVE)([0-7])\s*\(\s*ram\s*,\s*hooks\s*,\s*\"([^\"]+)\"\s*,\s*([^,\s)]+)",
        re.S)
    for rel in ("src/modes/ae_ovl181.c", "src/modes/ae_ovl182.c"):
        source = (ROOT / rel).read_text(encoding="utf-8")
        for argc, name, token in pattern.findall(source):
            token = re.sub(r"(?<=[0-9A-Fa-f])u\b", "", token, flags=re.I)
            try:
                address = int(token, 0) if token.lower().startswith("0x") else int(constants[token])
            except (KeyError, ValueError):
                continue
            if address == 0 or address >= 0x80100000:
                continue
            record = (name, int(argc))
            old = sites.get(address)
            if old is not None and old != record:
                raise SystemExit(f"hook site collision {address:08X}: {old} / {record}")
            sites[address] = record
    return sites


def parse_native_hook_addresses():
    """SDK/native calls are hooks even when the EXE has a callable entry there."""
    constants = parse_constants()
    addresses = set()
    pattern = re.compile(
        r"\bNATIVE[0-7]\s*\(\s*ram\s*,\s*hooks\s*,\s*\"[^\"]+\"\s*,\s*([^,\s)]+)",
        re.S)
    for rel in ("src/modes/ae_ovl181.c", "src/modes/ae_ovl182.c"):
        source = (ROOT / rel).read_text(encoding="utf-8")
        for token in pattern.findall(source):
            token = re.sub(r"(?<=[0-9A-Fa-f])u\b", "", token, flags=re.I)
            try:
                address = int(token, 0) if token.lower().startswith("0x") else int(constants[token])
            except (KeyError, ValueError):
                continue
            if 0 < address < 0x80100000:
                addresses.add(address)
    return addresses


def build_overlay_dll(source_overrides=None):
    """Build the checker DLL with one compiler process at a time."""
    overrides = {
        "tools/mips/game_integration_shim.c": ROOT / "tools/mips/ovl_shim.c",
        "src/game/ae_game_hooks.c": ROOT / "tools/mips/ovl_game_stub.c",
    }
    overrides.update(source_overrides or {})
    out = cg.OUT
    out.mkdir(parents=True, exist_ok=True)
    with (ROOT / "tools/verified.csv").open(newline="", encoding="utf-8") as stream:
        modules = {row["source"] for row in csv.DictReader(stream)
                   if row["status"] == "diffed"}
    rels = sorted(set(cg.SOURCES) | modules)
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
        key = hashlib.sha1(source.read_bytes() + header_digest.digest() +
                           " ".join(flags).encode() + rel.encode()).hexdigest()[:16]
        obj = cache / f"{Path(rel).stem}_{key}.o"
        if not obj.exists():
            tmp = obj.with_suffix(f".{os.getpid()}.tmp")
            command = [gcc, *flags, "-iquote", str(ROOT / Path(rel).parent),
                       "-c", str(source), "-o", str(tmp)]
            result = subprocess.run(command, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, check=False)
            if result.returncode:
                raise SystemExit("compile error:\n" + result.stdout.decode(errors="replace"))
            os.replace(tmp, obj)
        objects.append(obj)
    dll = out / f"ovl181_{os.getpid()}_{time.time_ns()}.dll"
    subprocess.run([gcc, "-shared", "-static-libgcc", *map(str, objects),
                    "-o", str(dll), "-lm"], check=True)
    return dll


def install_descriptor(cpu, descriptor=AREA_BASE):
    """A valid runtime model descriptor over the real area's loaded data."""
    entries = {4: 0x100, 8: 0x180, 12: 0x120, 16: 0x1A0,
               20: 0x140, 24: 0x160, 28: 0x1C0}
    for offset, value in entries.items():
        cpu.write16(descriptor + offset, value)
    headers = (descriptor + 0x400, descriptor + 0x480,
               descriptor + 0x500, descriptor + 0x580)
    for header in headers:
        cpu.write_bytes(header, bytes(12))
    for table in (descriptor + 0x600, descriptor + 0x680):
        cpu.write32(table, 2)
        cpu.write_bytes(table + 4, bytes(12))
    cpu.write_bytes(descriptor + 0x700, bytes(32))


def seed_camera_actor(cpu, actor, header, animation):
    cpu.write32(actor + 0x00, header)
    cpu.write32(actor + 0x04, header + 0x0C)
    cpu.write32(actor + 0x08, header + 0x0C)
    cpu.write32(actor + 0x0C, header + 0x0C)
    cpu.write32(actor + 0x10, header + 0x0C)
    cpu.write32(actor + 0x14, 0x800E9C60)
    cpu.write32(actor + 0x18, animation + 12)
    cpu.write32(actor + 0x74, animation + 4)
    cpu.write16(actor + 0x5C, 0)
    cpu.write16(actor + 0x5E, 0)
    cpu.write16(actor + 0x70, 0)
    cpu.write16(actor + 0x72, 0)


class Harness:
    def _bind_library(self, dll):
        self.lib = C.CDLL(str(dll))
        self.lib.integration_create.argtypes = [TRACE, P]
        self.lib.integration_create.restype = P
        self.lib.integration_ram.argtypes = [P]
        self.lib.integration_ram.restype = P
        self.lib.integration_overlay_call.argtypes = [P, U] + [U] * 7
        self.lib.integration_overlay_call.restype = I
        self.lib.integration_overlay_has_port.argtypes = [U]
        self.lib.integration_overlay_has_port.restype = I
        self.lib.ae_ram_error.argtypes = [P]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.lib.ae_pad_CameraRotMatrix.argtypes = [P, P, P, U]
        self.lib.ae_pad_CameraRotMatrix.restype = None

    def __init__(self, coverage=True, dll=None):
        reg.declare("stack_window", "801FF000..80200000", checker=CHECKER)
        reg.declare("ram_mask", "801FA000:20480", checker=CHECKER)
        reg.declare("ram_mask", "GPU ordering-table tags from inline packet builders",
                    checker=CHECKER)
        reg.declare("ram_mask",
                    "CameraLookAt native outputs: skip M3 800E78C0:32, camera matrices 800E9C60:32/800EBFC0:36/800FBFC0:32, camera state 800F4470:2/800F45CC:4/800F45E4:2/800F4742:2 and scratch 1F8003C1/1F8003C3; compare 18-byte yaw matrix 800EC330",
                    checker=CHECKER)
        self._bind_library(dll if dll is not None else build_overlay_dll())
        self.c_events, self.m_events = [], []
        self.callback_error = None
        self.c_vblank = self.m_vblank = 0
        self.c_mode03_vsync = self.m_mode03_vsync = 0
        self.c_load_step = self.m_load_step = 0
        self.coverage_by_member = {0x181: set(), 0x182: set()}
        self.mode03_button = 0
        self.mode03_force_idle = False
        self.mode03_forced_state = None
        self.current_index = 0
        self.trace = TRACE(self._trace)
        self.shim = self.lib.integration_create(self.trace, None)
        self.ram = self.lib.integration_ram(self.shim)
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(self.ram)
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(self.ram + mr.RAM_SIZE)
        self.cpu = mr.MipsRam.from_extract()
        self.cpu.attach_gte_oracle()
        self.camera_oracle = mr.MipsRam.from_extract()
        self.camera_gte_state = (C.c_uint32 * 64)()
        camera_gte_step = self.cpu.gte_backend._native_function
        self.camera_oracle.gte_state = self.camera_gte_state
        self.camera_oracle.gte_backend = lambda ins, value: camera_gte_step(
            self.camera_gte_state, ins, value & M32)
        self.camera_oracle.coverage = None
        self.base = self.cpu.snapshot()
        self.cpu.coverage = set() if coverage else None
        self.site_specs = parse_hook_sites()
        native_boundaries = parse_native_hook_addresses()
        self.site_specs.update({
            0x8002B52C: ("PadGetState", 1),
            0x8002B5F8: ("PadInfoMode", 3),
            0x80012CA0: ("PadSetupDualShock", 1),
            0x80014208: ("PadRumbleReset", 1),
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
        })
        constants = parse_constants()
        self.site_specs.update({
            constants[address_name]: (function, argc)
            for function, (address_name, argc) in NATIVE_ENTRIES.items()
        })
        for exclusion in NATIVE_ENTRY_EXCLUSIONS:
            reg.declare("unreachable", exclusion, checker=CHECKER)
        modeled_boundaries = {0x8002B52C, 0x8002B5F8, 0x80012CA0,
                              0x80014208, 0x800315E0,
                              0x80031600, 0x80031630, 0x800180B8,
                              0x8001D530, 0x8008CBFC,
                              0x8008CC60, 0x8005EF28, 0x8005EC9C}
        # StickToButtons now has a verified port. Let its original instructions
        # execute too, rather than retaining the former synthetic callback.
        self.site_specs = {address: spec for address, spec in self.site_specs.items()
                           if address in modeled_boundaries or
                           address in native_boundaries or
                           not self.lib.integration_overlay_has_port(address)}
        self.hooks = {address: self._make_hook(address, *spec)
                      for address, spec in self.site_specs.items()}
        self.patches = {
            0x801361F4: self._patch_mode_frame,
            0x80136648: self._patch_scene_quad,
            0x80136D00: self._patch_intro_title,
            0x80137634: self._patch_fade_ot_link,
            0x80137698: self._patch_fade_quad,
            0x80136DB4: self._patch_warning_packet,
        }
        self.counts = {name: 0 for name, *_ in FUNCS}
        self.tested = 0

    def _record(self, destination, name, address, args):
        destination.append((name, address & M32, tuple(norm(x) if isinstance(x, int) else x for x in args)))

    def _model(self, name, address, args, mips):
        if name == "CameraLookAt":
            if mips:
                raise AssertionError("CameraLookAt must execute as original MIPS")
            eye = C.cast(C.byref(self.main, args[0] - 0x80000000), P)
            target = C.cast(C.byref(self.main, args[1] - 0x80000000), P)
            self.lib.ae_pad_CameraRotMatrix(self.ram, eye, target, args[3])
            oracle = self.camera_oracle
            oracle.ram[:] = bytes(self.main)
            oracle.scratch[:] = bytes(self.scratch)
            oracle.call(address, args, limit=2_000_000)
            view_angle = oracle.read16(args[2] + 0x0E)
            angle_offset = args[2] + 0x0E - 0x80000000
            self.main[angle_offset:angle_offset + 2] = view_angle.to_bytes(2, "little")
            return 0
        target = self.m_events if mips else self.c_events
        observed_args = args
        if name in ("ClearImage", "ClearImage2"):
            rectangle = (self.cpu.read_bytes(args[0], 8) if mips else
                         bytes(self.main[args[0] - 0x80000000:args[0] - 0x80000000 + 8]))
            observed_args += (rectangle,)
        if name in RENDER_HOOKS:
            address = 0
        if name in ("Ovl181_DrawEncodedWarningText",
                    "Ovl181_BuildWarningBackdrop"):
            if mips:
                warning_color = self.cpu.read32(WARNING_COLOR)
            else:
                color_offset = WARNING_COLOR - 0x80000000
                warning_color = int.from_bytes(
                    self.main[color_offset:color_offset + 4], "little")
            observed_args = tuple(args) + (warning_color,)
        if (name == "PutDispEnv" and args and
                self.current_name in ("Ovl181_DrawIntegrityWarning",
                                      "Ovl181_CheckCdHardwareIntegrity")):
            if mips:
                video_mode_field = self.cpu.read16(
                    args[0] + WARNING_DISPLAY_MODE_OFFSET)
            else:
                field_offset = args[0] - 0x80000000 + WARNING_DISPLAY_MODE_OFFSET
                video_mode_field = int.from_bytes(
                    self.main[field_offset:field_offset + 2], "little")
            observed_args = tuple(args) + (video_mode_field,)
        if name == "Ovl181_CdCommand" and args and args[0] == 2:
            if mips:
                command_data = tuple(self.cpu.read_bytes(0x80137230, 3))
            else:
                start = 0x80137230 - 0x80000000
                command_data = tuple(self.main[start:start + 3])
            observed_args += command_data
        self._record(target, name, address, observed_args)
        if name == "Ovl181_CdCommand":
            if self.current_name == "Ovl181_CheckCdHardwareIntegrity":
                command = args[0] if args else 0
                if self.current_index == 5 and command == 5:
                    return -1
                if self.current_index == 6 and command == 0:
                    return -1
                if self.current_index == 7 and command == 9:
                    return -1
            return 0
        if name == "GetVideoMode":
            if self.current_name in ("Ovl181_DrawIntegrityWarning",
                                     "Ovl181_CheckCdHardwareIntegrity"):
                return (0, 1, 2, 1)[self.current_index % 4]
            return 0
        if name == "StickToButtons":
            if mips:
                pos = self.cpu.read32(PAD_STATE + 0x170)
                read8 = self.cpu.read8
            else:
                offset = PAD_STATE + 0x170 - 0x80000000
                pos = int.from_bytes(self.main[offset:offset + 4], "little")
                read8 = lambda a: self.main[a - 0x80000000]
            sample = PAD_STATE + pos * 4
            x, y = read8(sample + 2), read8(sample + 3)
            x = x - 0x100 if x & 0x80 else x
            y = y - 0x100 if y & 0x80 else y
            buttons = args[0] & M32
            if x >= 97: buttons |= 0x2000
            elif x < -96: buttons |= 0x8000
            if y >= 97: buttons |= 0x1000
            elif y < -96: buttons |= 0x4000
            return buttons
        if name == "LoadListStep":
            member = BY_NAME[self.current_name][1]
            step = self.m_load_step if mips else self.c_load_step
            if mips:
                self.m_load_step += 1
            else:
                self.c_load_step += 1
            scripted = {
                "Mode_Boot": (0, -1, -1, -1, 0, -1),
                "Mode_03": (0, -1),
            }
            if member == 0x181 and self.current_name == "Mode_Boot":
                values = scripted["Mode_Boot"]
            elif member == 0x182 and self.current_name == "Mode_03":
                values = ((0, -1, 0, -1) if self.mode03_force_idle
                          else scripted["Mode_03"])
            else:
                values = ()
            return values[step] if step < len(values) else -1
        if name == "VSync":
            if (self.current_name == "Mode_03" and args and
                    (args[0] & M32) == 2):
                if mips:
                    self.m_mode03_vsync += 1
                    frame = self.m_mode03_vsync
                else:
                    self.c_mode03_vsync += 1
                    frame = self.c_mode03_vsync
                if frame == 31:
                    raw = (~self.mode03_button) & 0xFFFF
                    if mips:
                        self.cpu.write8(0x800E52D0, 0)
                        self.cpu.write8(0x800E52D1, 0x41)
                        self.cpu.write8(0x800E52D2, raw >> 8)
                        self.cpu.write8(0x800E52D3, raw)
                    else:
                        base = 0x800E52D0 - 0x80000000
                        self.main[base:base + 4] = bytes((0, 0x41,
                                                        raw >> 8, raw & 0xFF))
                    if self.mode03_force_idle:
                        if mips:
                            self.cpu.write16(OVERLAY_FRAME, 1800)
                        else:
                            offset = OVERLAY_FRAME - 0x80000000
                            self.main[offset:offset + 2] = (1800).to_bytes(2, "little")
                    if self.mode03_forced_state is not None:
                        if mips:
                            self.cpu.write32(OVERLAY_STATE, self.mode03_forced_state)
                        else:
                            offset = OVERLAY_STATE - 0x80000000
                            self.main[offset:offset + 4] = self.mode03_forced_state.to_bytes(4, "little")
            if args and (args[0] & M32) == M32:
                if mips:
                    self.m_vblank += 1
                    return self.m_vblank
                self.c_vblank += 1
                return self.c_vblank
        return 0

    def _trace(self, _user, raw_name, address, argc, raw_args):
        try:
            name = raw_name.decode("ascii")
            args = tuple(int(raw_args[i]) for i in range(argc))
            return self._model(name, int(address), args, False)
        except Exception as exc:  # surfaced after the C call, not through ctypes
            self.callback_error = str(exc)
            return 0

    def _make_hook(self, address, name, argc):
        def invoke(cpu, regs):
            args = [regs[4 + i] for i in range(min(argc, 4))]
            for i in range(4, argc):
                args.append(cpu.read32(regs[29] + i * 4))
            return self._model(name, address, tuple(args), True)
        return invoke

    def _render(self, cpu, name, args):
        self._record(self.m_events, name, 0, args)

    def _patch_fade_quad(self, cpu, regs):
        timer = s16(cpu.read16(OVERLAY_FADE_TIMER))
        state = s16(cpu.read16(OVERLAY_FADE_STATE))
        pointer = cpu.read32(DISPLAY_GLOBALS + 4)
        alpha = cpu.read8(pointer + 6)
        self._render(cpu, "OverlayFadeQuad",
                     (pointer, state, timer, alpha, regs[3] & M32))
        cpu.write32(DISPLAY_GLOBALS + 4, pointer + 24)
        regs[2] = 0
        return regs[31]

    def _patch_fade_ot_link(self, _cpu, _regs):
        return 0x80137664

    def _patch_warning_packet(self, cpu, regs):
        packet = (regs[29] + 144) & M32
        self._render(cpu, "Ovl181_IntegrityWarningClearPacket", (packet,))
        return 0x80136DD4

    def _patch_intro_title(self, cpu, regs):
        draw = cpu.read32(OVERLAY_DRAW_INDEX)
        pointer = cpu.read32(DISPLAY_GLOBALS + 4)
        ot_head = cpu.read32(DISPLAY_GLOBALS + 8)
        phase = cpu.read32(DISPLAY_GLOBALS)
        state = s16(cpu.read16(OVERLAY_FADE_STATE))
        self._render(cpu, "OverlayIntroTitle", (draw, pointer, ot_head, phase, state))
        stack = (regs[29] - 32) & M32
        cpu.write32(stack + 24, regs[31])
        cpu.write32(stack + 20, regs[17])
        cpu.write32(stack + 16, regs[16])
        regs[29] = stack
        regs[17] = 0x80130000
        regs[6] = (pointer + 72) & M32
        regs[13] = DISPLAY_GLOBALS
        return 0x80136E68

    def _patch_scene_quad(self, cpu, regs):
        pointer = cpu.read32(DISPLAY_GLOBALS + 4)
        scroll = cpu.read32(OVERLAY_SCROLL)
        angle = s16(cpu.read16(0x800E78CE))
        scaled = int(angle / 72)
        overlay_scroll = max(-128, min(0, 8 - scaled))
        tint = 255 - cpu.read8(DISPLAY_GLOBALS)
        self._render(cpu, "OverlayWaterQuad",
                     (pointer, scroll, angle, overlay_scroll, tint))
        cpu.write32(DISPLAY_GLOBALS + 4, pointer + 24)
        return 0x8013671C

    def _patch_mode_frame(self, cpu, regs):
        pointer = cpu.read32(DISPLAY_GLOBALS + 4)
        display_index = cpu.read32(DISPLAY_GLOBALS + 12)
        state = cpu.read32(OVERLAY_STATE)
        fade = s16(cpu.read16(OVERLAY_FADE_STATE))
        frame = cpu.read16(OVERLAY_FRAME)
        self._render(cpu, "OverlayModeFrame",
                     (pointer, display_index, state, fade, frame))
        regs[3] = (pointer + 48) & M32  # the original jal's delay slot stores v1
        return 0x801362AC

    def _seed_overlay_globals(self, cpu, member):
        if member != 0x182:
            return
        cpu.write32(DISPLAY_GLOBALS, 0)
        cpu.write32(DISPLAY_GLOBALS + 4, PACKET_START)
        cpu.write32(DISPLAY_GLOBALS + 8, OT_HEAD_POINTER)
        cpu.write32(DISPLAY_GLOBALS + 12, 0)
        cpu.write32(OVERLAY_SCROLL, 0)
        cpu.write32(OVERLAY_DRAW_INDEX, 0)

    def _install_camera_fixture(self, cpu):
        install_descriptor(cpu)
        # ModelInitNormals is deliberately hooked, so initialize its result record
        # to the same valid fixture a game-side implementation leaves behind.
        seed_camera_actor(cpu, 0x80137850, AREA_BASE + 0x400, AREA_BASE + 0x600)
        seed_camera_actor(cpu, 0x801378D8, AREA_BASE + 0x480, AREA_BASE + 0x680)
        cpu.write16(0x80137850 + 0x60, 0)
        cpu.write16(0x80137850 + 0x62, 0)
        cpu.write16(0x80137850 + 0x64, 4096)
        cpu.write16(0x801378D8 + 0x60, 4096)
        cpu.write16(0x801378D8 + 0x62, 0)
        cpu.write16(0x801378D8 + 0x64, 0)

    def _load(self, area, member):
        self.cpu.restore(self.base)
        area_bytes = (ROOT / f"extract/members/{AREAS[area]:03X}.bin").read_bytes()
        overlay = (ROOT / f"extract/members/{member:03X}.bin").read_bytes()
        self.cpu.load_area(area_bytes, AREA_BASE)
        self.cpu.load_area(overlay, 0x80136000)
        self.cpu._decoded.clear()
        self._seed_overlay_globals(self.cpu, member)
        if member == 0x182:
            install_descriptor(self.cpu)

    def _setup_case(self, name, index):
        row = BY_NAME[name]
        self._load((1, 2, 4, 5)[index % 4], row[1])
        cpu = self.cpu
        rng = random.Random(0x944230 + row[1] * 0x1000 + row[2] + index * 257)
        if name == "Mode_Boot":
            cpu.write32(0x800E38B0, 1)
            cpu.write8(0x800E52D0, 0)
            cpu.write8(0x800E52D1, 0x70 if index == 0 else 0)
            for offset in (2, 4, 6):
                cpu.write16(0x800BF670 + offset, 0xFFFF)
            cpu.write16(0x800BF670 + 0x16, 0xFFFF)
        elif name == "Boot_ClearScreen":
            cpu.write32(0x800E38B0, (1, 6, 0xFFFFFFFF, 0x80000000)[index % 4])
        elif name in ("Boot_DrawLogoA", "Boot_DrawLogoB"):
            cpu.write32(0x800E38B0, index & 1)
        elif name == "Mode_02":
            cpu.write16(GAME_GLOBALS + 0x120, rng.randrange(0x10000))
            if index:
                # Original copies this embedded rectangle, rather than fixed
                # framebuffer dimensions. Keep case zero on the real data.
                for field,value in enumerate((4+index,8+index,320+index,240+index)):
                    cpu.write16(0x80136918+field*2,value)
        elif name == "Mode_03":
            self._install_camera_fixture(cpu)
            cpu.write16(OVERLAY_SELECTION, 0)
            cpu.write16(OVERLAY_FADE_TIMER, 30)
            cpu.write16(OVERLAY_FADE_STATE, 0)
            cpu.write32(OVERLAY_STATE, 0)
            cpu.write32(0x800E54E0, 0)
            self.mode03_button = 0 if index == 2 else 0x0860
            self.mode03_force_idle = index == 2
            self.mode03_forced_state = {3: 5, 4: 6, 5: 8, 6: 9}.get(index)
            cpu.write_bytes(0x800E52D0, bytes((0, 0x41, 0xFF, 0xFF)))
            for offset in (2, 4, 6):
                cpu.write16(0x800BF670 + offset, 0xFFFF)
            cpu.write16(0x800BF670 + 0x16, 0xFFFF)
            if index % 2:
                cpu.write32(OVERLAY_DRAW_INDEX, 1)
        elif name == "Ovl182_DrawScene3D":
            cpu.write32(OVERLAY_SCROLL, (0, 960, 1024, 2000, M32, 32768, 0x10000, 896)[index % 8])
            cpu.write16(0x800E78CE, (-32768, -9216, -576, 0, 576, 9216, 32767, rng.randint(-32768, 32767))[index % 8])
        elif name == "Ovl182_UpdateCamera":
            self._install_camera_fixture(cpu)
            self.cpu.call(0x80136AC0, (AREA_BASE,),
                          hooks=self._hooks_for_member(0x182),
                          patches=self._patches_for_member(0x182))
            self._install_camera_fixture_after_init(cpu)
            cpu.write32(DISPLAY_GLOBALS, (0, 1, 120, 255, 359, 4096, 0x12345678, M32)[index % 8])
        elif name == "Ovl182_Init":
            self._install_camera_fixture(cpu)
        elif name == "Ovl182_StateMachine":
            state = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, M32)[index % 12]
            cpu.write32(OVERLAY_STATE, state)
            cpu.write16(OVERLAY_FADE_STATE, 1)
            cpu.write32(OVERLAY_DRAW_INDEX, index % 2)
            set_input(cpu, (0, 0x860, 0, 0, 0xA000, 0, 0, 0, 0, 0, 0, 0)[index % 12])
        elif name == "Ovl182_DrawIntroState":
            fade, button = ((0, 0), (1, 0), (1, 0x860), (1, 0x4000), (1, 0x1000), (2, 0))[index % 6]
            cpu.write16(OVERLAY_FADE_STATE, fade)
            cpu.write16(OVERLAY_FRAME, 77)
            cpu.write32(OVERLAY_DRAW_INDEX, index & 1)
            set_input(cpu, button)
        elif name == "Ovl182_MenuInput":
            selection, button = ((0, 0), (0, 0xA000), (1, 0xA000),
                                 (2, 0x8000), (3, 0x2000), (4, 0xA000),
                                 (0, 0x1000), (4, 0x4000), (2, 0x0900),
                                 (3, 0xA000), (4, 0), (1, 0),
                                 (2, 0x2000))[index % 13]
            cpu.write32(OVERLAY_SELECTION, selection)
            cpu.write32(OVERLAY_STATE, 4)
            cpu.write32(0x800E54E0, index & 1)
            cpu.write16(GAME_GLOBALS + 0x13A, index & 1)
            cpu.write8(GAME_GLOBALS + 0x138, index & 1)
            cpu.write8(GAME_GLOBALS + 0x139, (index >> 1) & 1)
            cpu.write32(PAD_STATE + 0x170, 0)
            cpu.write8(PAD_STATE + 3, 48 if index & 1 else 0)
            cpu.write8(PAD_STATE + 1, 12 if index & 1 else 0)
            set_input(cpu, button)
        elif name == "Ovl182_FadeTick":
            state, timer = ((0, 1), (0, 0), (1, 30), (2, 0), (2, 30),
                            (3, 30), (4, 10), (0xFFFF, 0))[index % 8]
            cpu.write16(OVERLAY_FADE_STATE, state)
            cpu.write16(OVERLAY_FADE_TIMER, timer)
        elif name == "Ovl182_DrawFade":
            cpu.write16(OVERLAY_FADE_STATE, (0, 1, 2, 3, 4, 0, 2, 3)[index % 8])
            cpu.write16(OVERLAY_FADE_TIMER, (-1, 0, 1, 15, 30, 31, 7, 24)[index % 8])
        elif name == "Ovl181_CheckCdHardwareIntegrity":
            status, gate = ((0, 0), (2, 0), (0x10, 1), (0, 1),
                            (0xF0, 1), (0, 0), (0, 0), (0, 0))[index % 8]
            cpu.write8(0x80137238, status)
            cpu.write8(0x80137239, gate)

    def _install_camera_fixture_after_init(self, cpu):
        seed_camera_actor(cpu, 0x80137850, AREA_BASE + 0x400, AREA_BASE + 0x600)
        cpu.write16(0x80137850 + 0x60, 0)
        cpu.write16(0x80137850 + 0x62, 0)
        cpu.write16(0x80137850 + 0x64, 4096)

    def _hooks_for_member(self, member):
        return {address: hook for address, hook in self.hooks.items()
                if address < 0x80100000 or member == 0x181}

    def _patches_for_member(self, member):
        return {address: patch for address, patch in self.patches.items()
                if (member == 0x181 and address == 0x80136DB4) or
                (member == 0x182 and address != 0x80136DB4)}

    def _compare(self, name, label, c_result, m_result):
        error = self.lib.ae_ram_error(self.ram).decode(errors="replace")
        if error:
            raise AssertionError(f"{name} {label}: AeRam failure: {error}")
        if self.callback_error:
            raise AssertionError(f"{name} {label}: C callback failed: {self.callback_error}")
        if name in RETURNS and (int(c_result) & M32) != (int(m_result) & M32):
            raise AssertionError(f"{name} {label}: return {int(c_result)&M32:08X} != {int(m_result)&M32:08X}")
        a, b = bytearray(self.cpu.ram), bytearray(self.main)
        a[0x1FF000:] = b"\0" * (mr.RAM_SIZE - 0x1FF000)
        b[0x1FF000:] = b"\0" * (mr.RAM_SIZE - 0x1FF000)
        a[PACKET_MASK[0] - 0x80000000:PACKET_MASK[0] - 0x80000000 + PACKET_MASK[1]] = b"\0" * PACKET_MASK[1]
        b[PACKET_MASK[0] - 0x80000000:PACKET_MASK[0] - 0x80000000 + PACKET_MASK[1]] = b"\0" * PACKET_MASK[1]
        camera_output_used = name in ("Mode_03", "Ovl182_UpdateCamera")
        if camera_output_used:
            a[0xE78C0:0xE78E0] = b"\0" * 0x20
            b[0xE78C0:0xE78E0] = b"\0" * 0x20
            a[0xE9C60:0xE9C80] = b"\0" * 0x20
            b[0xE9C60:0xE9C80] = b"\0" * 0x20
            a[0xEBFC0:0xEBFE4] = b"\0" * 0x24
            b[0xEBFC0:0xEBFE4] = b"\0" * 0x24
            a[0xFBFC0:0xFBFE0] = b"\0" * 0x20
            b[0xFBFC0:0xFBFE0] = b"\0" * 0x20
            for start, length in ((0xF4470, 2), (0xF45E4, 2), (0xF4742, 2)):
                a[start:start + length] = b"\0" * length
                b[start:start + length] = b"\0" * length
            a[0xF45CC:0xF45D0] = b"\0" * 4
            b[0xF45CC:0xF45D0] = b"\0" * 4
        mode_events = [e for e in self.m_events if e[0] == "OverlayModeFrame"]
        if mode_events:
            start = mode_events[0][2][1]
            if isinstance(start, int):
                for frame in range(len(mode_events)):
                    offset = ORDERING_TABLE_BASE + (start + frame) * DISPLAY_STRIDE + 2 - 0x80000000
                    if 0 <= offset <= mr.RAM_SIZE - 2:
                        a[offset:offset + 2] = b"\0\0"
                        b[offset:offset + 2] = b"\0\0"
        if a != b:
            offsets = [i for i, (x, y) in enumerate(zip(a, b)) if x != y]
            event_index = next((i for i, pair in enumerate(zip(self.c_events, self.m_events))
                                if pair[0] != pair[1]), min(len(self.c_events), len(self.m_events)))
            raise AssertionError(f"{name} {label}: {len(offsets)} RAM bytes differ: " +
                                 str([(f"{0x80000000+i:08X}", a[i], b[i]) for i in offsets[:16]]) +
                                 f" events={len(self.c_events)}/{len(self.m_events)} equal={self.c_events == self.m_events} " +
                                 f"returns={int(c_result)&M32:08X}/{int(m_result)&M32:08X} " +
                                 f"first_event={event_index}: C={self.c_events[max(0,event_index-2):event_index+3]} " +
                                 f"M={self.m_events[max(0,event_index-2):event_index+3]}")
        scratch_a, scratch_b = bytearray(self.cpu.scratch), bytearray(self.scratch)
        if camera_output_used:
            for offset in (0x3C1, 0x3C3):
                scratch_a[offset] = scratch_b[offset] = 0
        if scratch_a != scratch_b:
            scratch_diffs = [i for i, (x, y) in enumerate(zip(scratch_a, scratch_b)) if x != y]
            raise AssertionError(f"{name} {label}: scratchpad differs: " +
                                 str([(f"{0x1F800000+i:08X}", scratch_a[i], scratch_b[i])
                                      for i in scratch_diffs[:24]]))
        if self.c_events != self.m_events:
            index = next((i for i, pair in enumerate(zip(self.c_events, self.m_events)) if pair[0] != pair[1]),
                         min(len(self.c_events), len(self.m_events)))
            raise AssertionError(f"{name} {label}: callback sequence differs at {index}: "
                                 f"C={self.c_events[max(0,index-2):index+3]} "
                                 f"M={self.m_events[max(0,index-2):index+3]}")
        self.tested += 1

    def _reset_logs(self):
        self.c_events.clear()
        self.m_events.clear()
        self.callback_error = None
        self.c_vblank = self.m_vblank = 0
        self.c_mode03_vsync = self.m_mode03_vsync = 0
        self.c_load_step = self.m_load_step = 0
        self.lib.ae_ram_clear_error(self.ram)

    def run_one(self, name, index):
        self.current_name = name
        self.current_index = index
        row = BY_NAME[name]
        self._setup_case(name, index)
        C.memmove(self.camera_gte_state, self.cpu.gte_state,
                  C.sizeof(self.camera_gte_state))
        self._reset_logs()
        C.memmove(self.main, bytes(self.cpu.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(self.cpu.scratch), mr.SCRATCH_SIZE)
        c_args = [0] * 7
        m_args = ()
        if name == "Ovl182_Init":
            c_args[0] = AREA_BASE
            m_args = (AREA_BASE,)
        elif name == "Ovl181_CheckCdHardwareIntegrity":
            m_args = ((0x11, 0x42, 0xC3), (0, 1, 2), (0xFF, 0x80, 0x7F),
                      (0x12345678, 0x87654321, 0xA5A5A5A5), (7, 8, 9),
                      (0xFE, 0xDC, 0xBA), (3, 5, 7), (0x55, 0xAA, 0xFF))[index]
        c_args[:len(m_args)] = m_args
        c_result = self.lib.integration_overlay_call(self.shim, row[4], *c_args)
        all_coverage = self.cpu.coverage
        case_coverage = set() if all_coverage is not None else None
        if case_coverage is not None:
            self.cpu.coverage = case_coverage
        try:
            active_patches = self._patches_for_member(row[1])
            if row[1] == 0x181 and name not in (
                    "Ovl181_CheckCdHardwareIntegrity",
                    "Ovl181_DrawIntegrityWarning"):
                active_patches = {}
            active_hooks = self._hooks_for_member(row[1])
            m_result = self.cpu.call(row[2], m_args, hooks=active_hooks, patches=active_patches,
                                     limit=20_000_000)[2]
        except mr.MipsError as exc:
            state = getattr(self.cpu, "_last_state", None)
            machine = getattr(self.cpu, "_last_machine", None)
            location = "unknown" if machine is None else f"pc={machine[0]:08X}"
            if state is not None:
                location += f" ra={state[0][31]:08X} a0={state[0][4]:08X} a1={state[0][5]:08X} v1={state[0][3]:08X} s5={state[0][21]:08X} s4={state[0][20]:08X}"
            counts = {key: sum(event[0] == key for event in self.m_events)
                      for key in sorted({event[0] for event in self.m_events})}
            raise AssertionError(f"{name} case {index}: {exc}; {location}; hook_count={len(self.m_events)} counts={counts}; last={self.m_events[-4:]}") from exc
        finally:
            if case_coverage is not None:
                if self.cpu.coverage is not None:
                    case_coverage.update(self.cpu.coverage)
                self.cpu.coverage = all_coverage
        if case_coverage is not None:
            self.coverage_by_member[row[1]].update(case_coverage)
        self._compare(name, f"case {index}", c_result, m_result)
        self.counts[name] += 1

    def finish(self, extra_coverage, default_run=False, show_coverage=False):
        """Summarize function coverage and write the default-scale result record."""
        items = [(name, address, size, True)
                 for name, _, address, size, _ in FUNCS]
        summary = {}
        all_coverage = set()
        for member in (0x181, 0x182):
            member_items = [item for item in items
                            if BY_NAME[item[0]][1] == member]
            coverage = self.coverage_by_member[member]
            all_coverage.update(coverage)
            summary.update(reg.coverage_summary(member_items, coverage))
        if extra_coverage:
            extra_items = [(name, address, size, False)
                           for name, (address, size) in extra_coverage.items()]
            summary.update(reg.coverage_summary(extra_items, all_coverage))

        incomplete = [(name, entry["uncovered"])
                      for name, entry in summary.items()
                      if entry["own"] and entry["uncovered_count"]]
        stale = [(name, entry["stale_declared"])
                 for name, entry in summary.items()
                 if entry["own"] and entry["stale_declared"]]
        if incomplete or stale:
            raise AssertionError(f"instruction coverage has undeclared gaps: "
                                 f"uncovered={incomplete}; stale exclusions={stale}")
        if show_coverage:
            reg.print_coverage(summary)

        result = None
        if default_run:
            result = reg.write_results(
                self.counts, summary, True,
                extra={"areas": list(AREAS), "comparisons": self.tested,
                       "mutations": 0},
                function_names=[name for name, *_ in FUNCS])
        return summary, result


def set_input(cpu, mask):
    cpu.write32(PAD_STATE + 0x158, mask)
    cpu.write32(PAD_STATE + 0x160, mask)


def case_count(name):
    return {
        "Mode_Boot": 2, "Boot_ClearScreen": 4, "Boot_SetupDisplay": 4,
        "Boot_DrawLogoA": 2, "Boot_DrawLogoB": 2, "Mode_02": 4,
        "Mode_03": 7, "Ovl182_DrawScene3D": 8, "Ovl182_UpdateCamera": 8,
        "Ovl182_Init": 8, "Ovl182_StateMachine": 12, "Ovl182_DrawIntroState": 6,
        "Ovl182_MenuInput": 13, "Ovl182_FadeTick": 8, "Ovl182_DrawFade": 8,
        "Ovl181_CheckCdHardwareIntegrity": 8,
        "Ovl181_DrawIntegrityWarning": 4,
    }[name]


def function_span(source, name):
    signature = re.compile(
        rf"(?m)^int32_t\s+ae_{re.escape(name)}\s*\([^)]*\)\s*\{{")
    match = signature.search(source)
    if match is None:
        raise ValueError(f"function ae_{name} not found")
    depth = 0
    for offset in range(match.end() - 1, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return match.start(), offset + 1
    raise ValueError(f"function ae_{name} has an unterminated body")


def mutate_function(source, name, old, new):
    start, end = function_span(source, name)
    function = source[start:end]
    matches = function.count(old)
    if matches != 1:
        raise ValueError(f"ae_{name}: expected one mutation anchor, found {matches}: {old!r}")
    return source[:start] + function.replace(old, new, 1) + source[end:]


def run_mutations():
    source_by_path = {
        "src/modes/ae_ovl181.c": (ROOT / "src/modes/ae_ovl181.c").read_text(encoding="utf-8"),
        "src/modes/ae_ovl182.c": (ROOT / "src/modes/ae_ovl182.c").read_text(encoding="utf-8"),
    }
    mutation_dir = Path(tempfile.mkdtemp(prefix="ovl181_mut_", dir=cg.OUT))
    survivors = []
    tested = 0
    started = time.time()
    harness = Harness(coverage=False)
    base_library = harness.lib
    try:
        for mutation_index, (name, label, old, new, target_case) in enumerate(MUTATIONS):
            member = BY_NAME[name][1]
            rel = "src/modes/ae_ovl181.c" if member == 0x181 else "src/modes/ae_ovl182.c"
            try:
                mutated = mutate_function(source_by_path[rel], name, old, new)
            except ValueError as exc:
                print(f"mutation {name} / {label}: SURVIVED (pattern error: {exc})", flush=True)
                survivors.append(f"{name}: {label}")
                continue

            mutated_path = mutation_dir / f"{mutation_index:02d}_{Path(rel).name}"
            mutated_path.write_text(mutated, encoding="utf-8")
            try:
                dll = build_overlay_dll({rel: mutated_path})
            except (OSError, SystemExit, subprocess.CalledProcessError) as exc:
                print(f"mutation {name} / {label}: SURVIVED (build failed: {exc})", flush=True)
                survivors.append(f"{name}: {label}")
                continue

            harness._bind_library(dll)
            candidates = [target_case] + [case for case in range(case_count(name))
                                          if case != target_case]
            caught_at = None
            for case in candidates:
                try:
                    harness.run_one(name, case)
                except AssertionError:
                    caught_at = case
                    break
            tested += 1
            if caught_at is None:
                print(f"mutation {name} / {label}: SURVIVED (checker too weak)", flush=True)
                survivors.append(f"{name}: {label}")
            else:
                print(f"mutation {name} / {label}: caught in case {caught_at}", flush=True)
    finally:
        resolved = mutation_dir.resolve()
        build_root = cg.OUT.resolve()
        if resolved.parent == build_root and resolved.name.startswith("ovl181_mut_"):
            shutil.rmtree(resolved)

    print(f"{tested}/{len(MUTATIONS)} mutations tested in {time.time() - started:.1f}s")
    return survivors


def run_checker(args):
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        raise SystemExit("missing extract/disc/SCUS_944.23")
    for member in (0x181, 0x182):
        if not (ROOT / f"extract/members/{member:03X}.bin").exists():
            raise SystemExit(f"missing overlay member {member:03X}.bin")
    harness = Harness()
    for name, *_ in FUNCS:
        for index in range(case_count(name)):
            harness.run_one(name, index)
        print(f"{name}: {harness.counts[name]} cases")
    summary, results = harness.finish(
        EXTRA_COVERAGE, default_run=True, show_coverage=args.coverage)
    print(f"Overlay comparisons: {harness.tested}; result: {results}")
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    if args.mutate:
        survivors = run_mutations()
        print("all mutations caught" if not survivors else f"SURVIVORS: {survivors}")
        return 1 if survivors else 0
    return run_checker(args)


if __name__ == "__main__":
    sys.exit(main())
