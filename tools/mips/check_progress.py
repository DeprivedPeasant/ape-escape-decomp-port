"""Differential checks for the progress/save/sound/UI helper batch.

The original functions run in the MIPS-I oracle over the retail EXE and real area
members.  Their RAM-image C ports run over the same image with matching named
hooks at the native sound, SDK, overlay, and progress boundaries.

Usage: .venv/Scripts/python.exe tools/mips/check_progress.py [--coverage | --mutate]
"""
from __future__ import annotations

import argparse
import ctypes as C
import json
import os
import random
import re
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / "tools"))
import mips_ram as mr  # noqa: E402
import dump_areas  # noqa: E402
import check_game as cg  # noqa: E402
import exclusion_registry as reg  # noqa: E402

M32 = 0xFFFFFFFF
STACK_LO = 0x801FF000
STACK_HI = 0x80200000

# (original address, byte size, return type, C name, args, C hook-table kind)
# Argument kinds: s = signed 32-bit; u = unsigned/address; p = pointer out argument.
FUNCS = {
    "SaveSlotInfoPtr": (0x800119F4, 56, "u", "ae_SaveSlotInfoPtr", "u", ""),
    "MeshAnimAdvance": (0x80016E68, 84, "v", "ae_MeshAnimAdvance", "u", ""),
    "SpriteEffectQueue0": (0x8006D548, 32, "v", "ae_SpriteEffectQueue0", "u", ""),
    "SoundLoopsPause": (0x8007292C, 160, "s", "ae_SoundLoopsPause", "", "progress"),
    "ClearDAT_800BEF70": (0x8007F400, 12, "v", "ae_ClearDAT_800BEF70", "", ""),
    "OpenPauseMenu": (0x8007F56C, 104, "v", "ae_OpenPauseMenu", "s", ""),
    "DrawTextSprites": (0x80082288, 272, "v", "ae_DrawTextSprites", "usss", "progress"),
    "SoundSetVolumes": (0x800862E4, 204, "s", "ae_SoundSetVolumes", "ss", "progress"),
    "SoundVolumesDefault": (0x80086428, 140, "s", "ae_SoundVolumesDefault", "s", "progress"),
    "SoundGetVolumes": (0x800864B4, 40, "s", "ae_SoundGetVolumes", "u", ""),
    "SoundSaveVolumes": (0x800864DC, 32, "s", "ae_SoundSaveVolumes", "", ""),
    "SoundRestoreVolumes": (0x800864FC, 68, "s", "ae_SoundRestoreVolumes", "", "progress"),
    "SaveBufferFill": (0x80095B3C, 432, "s", "ae_SaveBufferFill", "", "progress"),
    "SaveCaptureState": (0x80095CEC, 88, "s", "ae_SaveCaptureState", "", "progress"),
    "SaveHeaderFill": (0x80095D44, 264, "s", "ae_SaveHeaderFill", "", ""),
    "RecordBestTime": (0x800961D0, 188, "s", "ae_RecordBestTime", "", ""),
    "SaveBlockPtr": (0x8009628C, 12, "u", "ae_SaveBlockPtr", "", ""),
    "ProgressMarkStageResult": (0x800963DC, 212, "s", "ae_ProgressMarkStageResult", "ss", ""),
    "AreaKindVisitedGet": (0x80096514, 44, "s", "ae_AreaKindVisitedGet", "area_s", ""),
    "LevelProgressGet": (0x80096580, 52, "s", "ae_LevelProgressGet", "u", ""),
    "HubDestinationState": (0x800976B8, 260, "s", "ae_HubDestinationState", "ss", "progress"),
    "HubGateLocked": (0x800977BC, 100, "s", "ae_HubGateLocked", "s", ""),
    "UnlockSpecialArea": (0x80097820, 192, "s", "ae_UnlockSpecialArea", "", ""),
    "HubGateBitTest": (0x800978E0, 28, "s", "ae_HubGateBitTest", "u", ""),
    "ExitSpecialArea": (0x800978FC, 356, "s", "ae_ExitSpecialArea", "", "progress"),
}

DEFAULT_CASES = {name: 8 for name in FUNCS}
DEFAULT_CASES.update({
    "DrawTextSprites": 10,
    "SoundSetVolumes": 10,
    "SaveCaptureState": 6,
    "RecordBestTime": 10,
    "ProgressMarkStageResult": 12,
    "HubDestinationState": 12,
    "HubGateLocked": 10,
    "UnlockSpecialArea": 7,
    "ExitSpecialArea": 10,
    "OpenPauseMenu": 6,
})

# Stable RAM addresses and fields mirrored from the named C constants.  They are
# test setup addresses only; the port itself uses named constants in ae_progress.h.
G = {
    "current_level": 0x800F4474, "current_area": 0x800F4476,
    "game_mode": 0x800F4470, "pickup": 0x800F44BA,
    "stage_mode": 0x800F4470, "stage_kind": 0x800F4472,
    "pause_loop_a": 0x800B20D0, "pause_loop_b": 0x800B20D4,
    "sfx_bank": 0x800BF670, "bgm_bank": 0x800BF674,
    "bgm_voice": 0x800BF676, "bgm_volume": 0x800BF67E,
    "sfx_volume": 0x800BF680, "saved_bgm": 0x800BF682,
    "saved_sfx": 0x800BF684, "pause_clear": 0x800BEF70,
    "pause_flag": 0x800BEF75, "pause_state": 0x800BEF76,
    "pause_request": 0x800BEF74, "pause_value": 0x800BEF80,
    "text_cursor": 0x800EC354, "text_ot_index": 0x800EC35C,
    "text_ot": 0x800F03D0, "text_advances": 0x800B2734,
    "area_records": 0x800DFB70, "level_play": 0x800DFC70,
    "ta_count": 0x800DFC88, "level_collected": 0x800DFC98,
    "time_records": 0x800DFD40, "gimmick_states": 0x800DF820,
    "flags": 0x800DFCC0, "gate_flags": 0x800DFDCC,
    "stage_result": 0x800DFDD0, "stage_words": 0x800DFDD8,
    "area_kind_visited": 0x800DFDF8, "stage_aux": 0x800DFDFC,
    "save": 0x800E0018, "save_table": 0x800B9030,
    "group_count": 0x800F45D8,
    "gadgets": 0x800F51C4, "gadget_slots": 0x800F51A8,
    "lives": 0x800F448C, "option_a": 0x800F4488,
    "option_b": 0x800F448A, "play_time": 0x800F4490,
    "previous_time": 0x800F4494, "header_word_154": 0x800F44A4,
    "header_byte_156": 0x800F44A6, "header_word_158": 0x800F44A8,
    "header_byte_168": 0x800F44B8, "saved_option_byte": 0x800EC2D2,
    "sling_ammo_1": 0x800F51C1, "sling_ammo_2": 0x800F51C2,
    "flags_progress_bytes": 0x800DFDD8,
}
SAVE_SLOT_SIZE = 0x84
SAVE_SLOT_TABLE = 0x168
SAVE_ARRAY_WORD = 4
SAVE_ENTRY_SIZE = 0x1E
GIMMICK_TYPE = 0x00

SPU_WAIT_IDLE = 0x800A1398
SPU_WAIT_LOOP = 0x8009D79C
SOUND_LOOPS = 0x800BF370
SOUND_LOOP_SIZE = 0x1C
HOOK_ADDRS = {
    0x80086788: ("SoundLoopsSetPan", 1),
    0x8013FB20: ("FUN_8013FB20", 1),
    0x800A2640: ("SoundBankSetVolume", 2),
    0x8002B410: ("SetDrawTPage", 4),
    0x800962F4: ("ProgressFlagSet", 1),
    0x80096360: ("ProgressSetByte", 2),
}


def u32(x):
    return int(x) & M32


def s32(x):
    x &= M32
    return x - 0x100000000 if x & 0x80000000 else x


def put_save_progress_entries(cpu):
    # Existing GimmickSaveApplyProgress executes its original code and reaches
    # these two named native-system hooks, just as the stage does.
    first = G["save_table"]
    cpu.write16(first + 0x00, 1)
    cpu.write16(first + 0x06 + GIMMICK_TYPE, 0x22)
    cpu.write16(first + 0x10, 1)
    cpu.write16(first + 0x18, 35)
    second = first + SAVE_ENTRY_SIZE
    cpu.write16(second + 0x00, 1)
    cpu.write16(second + 0x06 + GIMMICK_TYPE, 0x32)
    cpu.write16(second + 0x10, 0x7A)
    cpu.write16(second + 0x0E, 9)


def init_save_globals(cpu, seed):
    rng = random.Random(seed)
    cpu.write16(G["current_level"], 5)
    cpu.write16(G["group_count"], 0)
    cpu.write16(G["option_a"], rng.choice([0, 1, 2]))
    cpu.write8(G["option_b"], rng.randrange(2))
    cpu.write16(G["lives"], rng.randrange(0x100))
    cpu.write16(G["header_byte_168"], rng.randrange(0x100))
    cpu.write32(G["gadgets"], rng.getrandbits(32))
    cpu.write32(G["gadget_slots"], rng.getrandbits(32))
    cpu.write8(G["saved_option_byte"], rng.randrange(0x100))
    cpu.write8(G["sling_ammo_1"], rng.randrange(0x100))
    cpu.write8(G["sling_ammo_2"], rng.randrange(0x100))
    cpu.write16(G["header_word_154"], rng.getrandbits(16))
    cpu.write8(G["header_byte_156"], rng.randrange(0x100))
    cpu.write16(G["header_word_158"], rng.getrandbits(16))
    cpu.write16(G["sfx_volume"], rng.randrange(0x80))
    cpu.write16(G["bgm_volume"], rng.randrange(0x80))
    for i in range(128):
        cpu.write16(G["area_records"] + i * 2, rng.getrandbits(16))
    for i in range(32):
        cpu.write8(G["level_play"] + i, rng.randrange(0x100))
        cpu.write8(G["level_collected"] + i, rng.randrange(0x100))
        cpu.write32(G["time_records"] + i * 4, rng.getrandbits(32))
        cpu.write32(G["flags"] + i * 4, rng.getrandbits(32))
    for i in range(752):
        cpu.write8(G["gimmick_states"] + i, rng.randrange(0x100))
    for i in range(8):
        cpu.write32(G["stage_words"] + i * 4, rng.getrandbits(32))
    cpu.write32(G["stage_result"], rng.getrandbits(32))
    cpu.write32(G["gate_flags"], rng.getrandbits(32))
    cpu.write32(G["area_kind_visited"], rng.getrandbits(32))
    cpu.write32(G["stage_aux"], rng.getrandbits(32))
    put_save_progress_entries(cpu)


class Scenario:
    """Build one targeted state in the current MIPS image and return MIPS args."""
    def __init__(self, cpu, name, index, area, seed):
        self.cpu, self.name, self.index, self.area = cpu, name, index, area
        self.seed = seed
        self.rng = random.Random(seed ^ (index * 0x9E3779B1))

    def make(self):
        m, i, n, rng = self.cpu, self.index, self.name, self.rng
        if n == "SaveSlotInfoPtr":
            return [(0,), (1,), (2,), (7,), (0x100,), (0xFFFF,), (0xFFFFFFFF,), (0x12345678,)][i % 8]
        if n == "MeshAnimAdvance":
            mesh, data = 0x801C0000, 0x801C1000
            frame = [0, 1, 2, 3, 0xFFFF, 0x7FFF, 4, 0][i % 8]
            flags = [0, 0, 2, 2, 1, 3, 0, 2][i % 8]
            count, end = 4, [4, 4, 2, 3, 0, 0xFFFF, 4, 5][i % 8]
            m.write32(mesh, data)
            m.write16(mesh + 0x18, frame)
            m.write16(mesh + 0x1A, flags)
            m.write16(data + 0x0C, count)
            m.write16(data + count * 4, end)
            return (mesh,)
        if n == "SpriteEffectQueue0":
            spec = 0x801C2000
            m.write8(0x800B1B40, [0, 1, 31, 32, 7, 30, 15, 29][i % 8])
            for k in range(8): m.write8(spec + k, (i * 37 + k * 29) & 0xFF)
            return (spec,)
        if n == "SoundLoopsPause":
            loop_a, loop_b = G["pause_loop_a"], G["pause_loop_b"]
            ids = [(22, 0xAD), (0, 0xAD), (22, 0), (22, 0xAD), (22, 0xAD), (22, 0), (0, 0), (22, 0xAD)]
            voices = [(1, 3), (1, 3), (1, 3), (0, 0), (0x8001, 2), (2, 1), (0, 0), (3, 4)]
            a, b = ids[i % 8], voices[i % 8]
            m.write16(loop_a, a[0]); m.write16(loop_a + 2, b[0])
            m.write16(loop_b, a[1]); m.write16(loop_b + 2, b[1])
            m.write16(G["current_level"], 0x1B if i % 3 == 0 else 5)
            # Active sequencer channels make voice-stop register writes observable.
            for channel in range(24):
                addr = 0x800E07F0 + channel * 0x66
                m.write16(addr + 0x1A, 0)
                m.write16(addr + 6, 0x7FFF)
            for voice in b:
                if 0 < voice < 24:
                    addr = 0x800E07F0 + (voice % 24) * 0x66
                    m.write16(addr + 0x1A, 1)
                    m.write16(addr + 6, voice)
            # SoundLoopStop frees the matching (id, voice) record after it
            # stops the hardware voice; seed one so a wrong id is observable.
            loop_record = SOUND_LOOPS + (i % 24) * SOUND_LOOP_SIZE
            m.write16(loop_record, 0x16)
            m.write16(loop_record + 2, 22)
            m.write16(loop_record + 0x12, 1)
            m.write16(loop_record + 0x14, 0)
            return ()
        if n == "ClearDAT_800BEF70":
            m.write32(G["pause_clear"], 0xABCDEF01 + i)
            return ()
        if n == "OpenPauseMenu":
            m.write8(G["pause_flag"], i & 1)
            m.write16(G["pause_state"], 0x1234 + i)
            m.write32(G["pause_clear"], 0xCAFEBABE)
            return ([(1,), (0,), (-1,), (2,), (1,), (0,)][i % 6])
        if n == "DrawTextSprites":
            texts = [b"", b"\x01\x02\x0f\x00", b"\x10\x00", b"\x11\x24\x00",
                     b"\x0f\x80\xff\x00", b"\x20\x21\x22\x23\x00", b"\x7f\x00", b"\xfe\x05\x10\x00",
                     b"\x30\x0e\x40\x00", b"\x10\x10\x10\x10\x00"]
            text = 0x801C3000
            m.write_bytes(text, texts[i % len(texts)])
            cursor = 0x801C5000 + (i % 3) * 0x100
            ot_index = [0, 1, 2, 5, 3][i % 5]
            ot = G["text_ot"] + ot_index * 0x4078
            m.write32(G["text_cursor"], cursor)
            m.write32(G["text_ot_index"], ot_index)
            m.write_bytes(ot, (0x801C8000 + i * 0x10).to_bytes(4, "little"))
            return (text, [-32768, -5, 0, 10, 320][i % 5], [0, 12, -30, 240][i % 4], [0, 1, 4, 8, 15][i % 5])
        if n in ("SoundSetVolumes", "SoundVolumesDefault", "SoundRestoreVolumes", "SoundGetVolumes", "SoundSaveVolumes"):
            m.write16(G["sfx_bank"], [-1, 2, 3, -1, 7][i % 5])
            m.write16(G["bgm_bank"], [4, -1, 2, 3, -1][i % 5])
            m.write16(G["bgm_voice"], [-1, 0, 1, 3, -1][i % 5])
            m.write16(G["sfx_volume"], [0, 127, 0xFFFF, 32, 80][i % 5])
            m.write16(G["bgm_volume"], [75, 127, 0xFFFF, 60, 90][i % 5])
            m.write16(G["saved_bgm"], [75, 127, 0xFFFF, 0, 200][i % 5])
            m.write16(G["saved_sfx"], [127, 80, 0xFFFF, 10, 200][i % 5])
            if n == "SoundSetVolumes":
                return [(0, 75), (127, 127), (128, 200), (32, -1), (-1, 80), (75, 0), (0x100, 0x100), (48, 75), (10, 90), (90, 10)][i % 10]
            if n == "SoundVolumesDefault": return (i & 1,)
            if n == "SoundGetVolumes": return (0x801C4000,)
            return ()
        if n == "SaveBufferFill":
            init_save_globals(m, self.seed + i)
            # Both retail conditions for IsTimeAttack; other cases enter the
            # regular save path and exercise capture plus both progress hooks.
            m.write16(G["current_level"], [5, 0, 24, 29, 3, 19, 1, 31][i % 8])
            m.write16(G["stage_mode"], 0x13 if i in (0, 2) else 0)
            m.write16(G["stage_kind"], 3 if i == 1 else 0)
            return ()
        if n == "SaveCaptureState":
            init_save_globals(m, self.seed + i)
            m.write16(G["current_level"], 32 if i == 0 else (5 if i & 1 else 1))
            m.write16(G["group_count"], 0)
            if i & 1: put_save_progress_entries(m)
            return ()
        if n == "SaveHeaderFill":
            init_save_globals(m, self.seed + i)
            return ()
        if n == "RecordBestTime":
            init_save_globals(m, self.seed + i)
            levels = [24, 29, 23, 30, 18, 31, 25, 17, 28, 0]
            level = levels[i % len(levels)]
            m.write16(G["current_level"], level)
            m.write16(G["stage_mode"], 0x13 if i % 3 != 0 else 0)
            m.write16(G["stage_kind"], 3 if i % 3 == 0 and i % 2 else 0)
            index = 24 if 24 <= level <= 29 else level
            old = [0xFFFFFFFF, 500, 0x7FFFFFFF, 0x1000][i % 4]
            m.write32(G["time_records"] + index * 4, old)
            m.write32(G["play_time"], [100, 500, 0xFFFFFFFF, 400][i % 4])
            return ()
        if n == "SaveBlockPtr": return ()
        if n == "ProgressMarkStageResult":
            special = i % 4 == 0
            area = 19 if special else [1, 5, 19, 30, 88, 2][i % 6]
            exit_code = [8, 8, 3, 4, 0, 8, 5, 9, 8, 1, 8, 2][i % 12]
            initial = [0, 0x10, 0x08, 0x1000, 0x800, 0x900, 0x1010, 0xFFFF][i % 8]
            m.write32(G["stage_result"], initial)
            m.write16(G["pickup"], [0, 0xFFFC, 23, 0x7FFF][i % 4])
            return (area, exit_code)
        if n == "AreaKindVisitedGet":
            area = [1, 2, 5, 19, 88, 99, 103, 53][i % 8]
            kind = m.reads16(0x800A81B0 + area * 8 + 6)
            m.write32(G["area_kind_visited"], [0, 1 << (kind & 31), 0xFFFFFFFF, 1 << ((kind + 1) & 31)][i % 4])
            return (area,)
        if n == "LevelProgressGet":
            level = [0, 1, 23, 24, 25, 29, 30, 31][i % 8]
            m.write8(G["ta_count"], i & 0xFF)
            m.write8(G["level_play"] + level, [0, 1, 2, 3, 0xFF][i % 5])
            return (level,)
        if n == "HubDestinationState":
            if i == 11:
                # A ready destination must satisfy every part of the second
                # progress test while avoiding the earlier area-transition
                # branch and keeping the gate open.
                area = 6
                level = m.reads16(0x800A81B0 + area * 8 + 4)
                m.write32(G["gate_flags"], 0xFFFFFFFF)
                m.write8(G["ta_count"], 0)
                m.write8(G["level_play"] + level, 2)
                m.write32(G["area_kind_visited"], 0)
                return (area, 0x800)
            area = [1, 2, 5, 6, 15, 19, 30, 53, 88, 99, 103, 20][i % 12]
            kind = m.reads16(0x800A81B0 + area * 8 + 6)
            level = m.reads16(0x800A81B0 + area * 8 + 4)
            exit_code = [0x800, 0x800, 5, 3, 0x800, 1][i % 6]
            m.write32(G["gate_flags"], [0, 0xFFFFFFFF, 0x40, 0xFFFFFFFF][i % 4])
            m.write8(G["ta_count"], [1, 0, 2, 0][i % 4])
            m.write8(G["level_play"] + (level & 0xFF), [0, 1, 2, 0][i % 4])
            m.write32(G["area_kind_visited"], [0, 0xFFFFFFFF, 1 << (kind & 31), 0][i % 4])
            return (area, exit_code)
        if n == "HubGateLocked":
            gate_flags = [0, 0x2, 0x1, 0x8, 0x10,
                          0x20, 0xFFFFFFFF, 0, 0, 0][i % 10]
            m.write32(G["gate_flags"], gate_flags)
            return ([2, 6, 15, 20, 30, 53, 1, 88, 103, 2][i % 10],)
        if n == "UnlockSpecialArea":
            m.write16(G["current_area"], 0x5C + i % 7)
            m.write32(G["gate_flags"], [0, 0xFFFFFFFF, 0x100, 0x80000000][i % 4])
            m.write32(G["gadgets"], 0x100 + i)
            m.write32(G["gadget_slots"], 0x10203040 + i)
            return ()
        if n == "HubGateBitTest":
            bits = [0, 1, 6, 7, 15, 31, 32, 0xFFFFFFFF]
            flags = [1, 2, 0x40, 0x80, 0x8000,
                     0x80000000, 1, 0x80000000]
            m.write32(G["gate_flags"], flags[i % 8])
            return (bits[i % 8],)
        if n == "ExitSpecialArea":
            area = [0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 2, 0x5F, 0x5F][i % 10]
            m.write16(G["current_area"], area)
            m.write16(0x800B38BA, [2, 6, 15, 20, 30, 53, 99][i % 7])
            m.write16(0x800B38BC, [1, 2, 6, 15, 19, 30, 88][i % 7])
            m.write32(G["gate_flags"], 0xFFFFFFFF if i & 1 else 0)
            m.write8(G["ta_count"], i & 1)
            m.write32(G["area_kind_visited"], 0xFFFFFFFF if i & 1 else 0)
            return ()
        raise AssertionError(n)


PROGRESS_CB = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                           C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
GIMMICK_CB = PROGRESS_CB
GIMMICK_HANDLER_CB = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                                 C.c_uint32, C.c_uint32, C.c_uint32)
IDLE_CB = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p)
WAIT_CB = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32)
ADSR_CB = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint16, C.c_uint16)
OFF_CB = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint16)
PRINT_CB = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32,
                       C.c_int32, C.c_int32, C.c_int32)
VOLUME_CB = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint16)


class AeSoundHw(C.Structure):
    _fields_ = [
        ("user", C.c_void_p), ("wait_idle", IDLE_CB), ("wait_loop", WAIT_CB),
        ("set_voice_adsr", ADSR_CB), ("voice_key_off", OFF_CB),
        ("printf_error", PRINT_CB), ("set_voice_volume", VOLUME_CB),
    ]


_GA_FIELDS = ["CameraNudge", "BodyInsideBox", "GimmickTouchCheck", "MeshInitFromAreaTable",
              "PropSetMesh", "PlaySfx", "PlaySfxAt", "SoundLoopStart", "SoundEmitterPan",
              "SoundLoopStop", "ProgressFlagGet", "ProgressFlagSet", "ProgressGetByte",
              "ProgressSetByte", "GetSolidHeight", "SwingDoorBodyDelta", "SlideDoorMove"]


class AeGimmickAHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(n, GIMMICK_CB) for n in _GA_FIELDS] + [
        ("GimmickHandler", GIMMICK_HANDLER_CB), ("ground_stale_x", C.c_uint16),
        ("ground_stale_z", C.c_uint16)]


class AeProgressHooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("sound_hw", C.POINTER(AeSoundHw)),
                ("gimmick_a", C.POINTER(AeGimmickAHooks)), ("area_set", C.c_void_p),
                ("SoundLoopsSetPan", PROGRESS_CB), ("FUN_8013FB20", PROGRESS_CB),
                ("SoundBankSetVolume", PROGRESS_CB), ("SetDrawTPage", PROGRESS_CB)]


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        lib = self.lib
        lib.ae_ram_create.restype = P
        lib.ae_ptr.argtypes = [P, U, U]
        lib.ae_ptr.restype = P
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = I
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_rd8.argtypes = [P, U]
        lib.ae_rd16.argtypes = [P, U]
        lib.ae_rd32.argtypes = [P, U]
        lib.ae_wr8.argtypes = [P, U, U]
        lib.ae_wr16.argtypes = [P, U, U]
        lib.ae_wr32.argtypes = [P, U, U]
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scr = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.events = []
        self._keep = []
        self._current = None
        self.sound = AeSoundHw()
        self.gimmick = AeGimmickAHooks()
        self.hooks = AeProgressHooks()
        self._install_callbacks()
        for name, (_, _, ret, cname, kinds, hookkind) in FUNCS.items():
            f = getattr(lib, cname)
            args = [I if k == "s" else U for k in kinds if k not in ("area_s",)]
            if "area_s" in kinds:
                args = [P, I]
            if hookkind:
                f.argtypes = [P, C.POINTER(AeProgressHooks)] + args
            else:
                f.argtypes = [P] + args
            f.restype = None if ret == "v" else U if ret == "u" else I
        self.functions = {name: getattr(lib, spec[3]) for name, spec in FUNCS.items()}

    def _callback(self, type_, fn):
        cb = type_(fn)
        self._keep.append(cb)
        return cb

    def _c_event(self, name, args):
        self.events.append((name, tuple(u32(a) for a in args)))

    def _progress_callback(self, name, argc, effect=None):
        def run(_user, ram, a0, a1, a2, a3):
            values = (a0, a1, a2, a3)[:argc]
            self._c_event(name, values)
            if effect == "draw_tpage":
                self._set_draw_tpage(ram, a0, a1, a2, a3)
            elif effect == "flag_set":
                flag = u32(a0)
                index = ((flag << 11) & M32)
                index = (index >> 16) - (0x10000 if index & 0x80000000 else 0)
                address = 0x800DFCC0 + index * 4
                value = C.c_uint32(self.lib.ae_rd32(ram, address)).value | (1 << (flag & 31))
                self.lib.ae_wr32(ram, address, value)
            elif effect == "set_byte":
                offset = u32(a0)
                value = u32(a1) & 0xFF
                address = 0x800DFDD8 + (offset & 0x3FFFC)
                shift = (offset & 3) * 8
                word = C.c_uint32(self.lib.ae_rd32(ram, address)).value
                word = (word & ~(0xFF << shift)) | (value << shift)
                self.lib.ae_wr32(ram, address, word)
            return 0
        return self._callback(PROGRESS_CB, run)

    @staticmethod
    def _set_draw_tpage(ram, packet, semi_trans, draw_page, tpage):
        # SetDrawTPage @8002B410: SDK helper writes its 1-byte opcode and tag
        # word; the values here are exactly DrawTextSprites' arguments.
        self = CSide._active
        self.lib.ae_wr8(ram, u32(packet) + 3, 1)
        word = 0xE1000200 if u32(draw_page) else 0
        word |= u32(tpage) & 0x9FF
        if u32(semi_trans): word |= 0x400
        self.lib.ae_wr32(ram, u32(packet) + 4, word)

    _active = None

    def _install_callbacks(self):
        self.sound.wait_idle = self._callback(IDLE_CB, lambda _u, _r: self._sound_event("SpuWaitIdle", ()))
        self.sound.wait_loop = self._callback(WAIT_CB, lambda _u, _r, units: self._sound_event("SpuWaitLoop", (units,)))
        self.sound.set_voice_adsr = self._callback(ADSR_CB, lambda _u, _r, ch, a, b: self._sound_event("set_voice_adsr", (ch, a, b)))
        self.sound.voice_key_off = self._callback(OFF_CB, lambda _u, _r, half, mask: self._sound_event("voice_key_off", (half, mask)))
        self.sound.printf_error = self._callback(PRINT_CB, lambda _u, _r, fmt, argc, a, b, c: self._sound_event("printf_error", (fmt, argc, a, b, c)[:2 + argc]))
        self.sound.set_voice_volume = self._callback(VOLUME_CB, lambda _u, _r, ch, side, vol: self._sound_event("set_voice_volume", (ch, side, vol)))
        self.gimmick.ProgressFlagSet = self._progress_callback("ProgressFlagSet", 1, "flag_set")
        self.gimmick.ProgressSetByte = self._progress_callback("ProgressSetByte", 2, "set_byte")
        for name, argc in (("SoundLoopsSetPan", 1), ("FUN_8013FB20", 1),
                           ("SoundBankSetVolume", 2), ("SetDrawTPage", 4)):
            effect = "draw_tpage" if name == "SetDrawTPage" else None
            setattr(self.hooks, name, self._progress_callback(name, argc, effect))
        self.hooks.sound_hw = C.pointer(self.sound)
        self.hooks.gimmick_a = C.pointer(self.gimmick)
        self.hooks.area_set = None

    def _sound_event(self, name, values):
        self.events.append((name, tuple(u32(a) for a in values)))
        return 0

    def sync(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), mr.RAM_SIZE)
        C.memmove(self.scr, bytes(cpu.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def call(self, name, args):
        _, _, ret, _, kinds, hookkind = FUNCS[name]
        fn = self.functions[name]
        passed = [self.ram]
        if hookkind == "progress": passed.append(C.byref(self.hooks))
        elif "area_s" in kinds: passed.append(None)
        for value, kind in zip(args, kinds.replace("area_s", "s")):
            passed.append(s32(value) if kind == "s" else u32(value))
        return fn(*passed)


def build_port_dll(source_override=None, tag="progress"):
    if "src/world/ae_progress.c" not in cg.SOURCES:
        cg.SOURCES.append("src/world/ae_progress.c")
    # check_game's host glue aggregates newer hook tables and is deliberately
    # outside this function-level checker.  Link the verified RAM-image modules
    # with their core dependencies but omit the AeGame and integration shims.
    original = cg.SOURCES
    cg.SOURCES = [s for s in original if not s.startswith("src/game/")
                  and s not in ("tools/mips/game_shim.c", "tools/mips/game_integration_shim.c")]
    overrides = {"src/world/ae_progress.c": source_override} if source_override else None
    try:
        return cg.build_dll(overrides=overrides, tag=tag)
    finally:
        cg.SOURCES = original


def mips_hooks(cpu, events):
    hooks = {}
    for address, (name, argc) in HOOK_ADDRS.items():
        def callback(machine, regs, event_name=name, count=argc):
            values = tuple(regs[4:4 + count])
            events.append((event_name, tuple(u32(v) for v in values)))
            if event_name == "SetDrawTPage":
                packet, semi, draw_page, tpage = values
                machine.write8(packet + 3, 1)
                word = (0xE1000200 if draw_page else 0) | (tpage & 0x9FF)
                if semi: word |= 0x400
                machine.write32(packet + 4, word)
            elif event_name == "ProgressFlagSet":
                flag = u32(values[0])
                shifted = (flag << 11) & M32
                index = mr.signed(shifted, 32) >> 16
                addr = 0x800DFCC0 + index * 4
                machine.write32(addr, machine.read32(addr) | (1 << (flag & 31)))
            elif event_name == "ProgressSetByte":
                offset, value = values
                address = 0x800DFDD8 + (offset & 0x3FFFC)
                shift = (offset & 3) * 8
                word = machine.read32(address)
                machine.write32(address, (word & ~(0xFF << shift)) | ((value & 0xFF) << shift))
            return 0
        hooks[address] = callback
    hooks[SPU_WAIT_IDLE] = lambda machine, regs: events.append(("SpuWaitIdle", ())) or 0
    hooks[SPU_WAIT_LOOP] = lambda machine, regs: events.append(("SpuWaitLoop", (u32(regs[4]),))) or 0
    return hooks


def run_mips(cpu, name, args, events):
    write = cpu.write
    adsr = {}

    def hw_write(address, size, value):
        if 0x1F801C00 <= address < 0x1F801E00:
            offset = address - 0x1F801C00
            if size != 2: raise AssertionError(f"unexpected SPU write width {size} at {address:08X}")
            value &= 0xFFFF
            if offset in (0x18C, 0x18E):
                events.append(("voice_key_off", ((offset - 0x18C) // 2, value)))
            elif offset % 16 == 8:
                adsr[offset // 16] = value
            elif offset % 16 == 10:
                events.append(("set_voice_adsr", (offset // 16, adsr.pop(offset // 16), value)))
            elif offset % 16 in (0, 2):
                events.append(("set_voice_volume", (offset // 16, (offset % 16) // 2, value)))
            else:
                raise AssertionError(f"unexpected SPU halfword write {address:08X}={value:04X}")
            return
        write(address, size, value)

    cpu.write = hw_write
    old_backend = None
    try:
        if name == "DrawTextSprites":
            old_backend = os.environ.get("MIPS_BACKEND")
            os.environ["MIPS_BACKEND"] = "py"
        return cpu.call(FUNCS[name][0], args, hooks=mips_hooks(cpu, events))[2] & M32
    finally:
        cpu.write = write
        if old_backend is not None:
            os.environ["MIPS_BACKEND"] = old_backend
        elif name == "DrawTextSprites":
            os.environ.pop("MIPS_BACKEND", None)


def call_c(side, name, args):
    CSide._active = side
    side.events.clear()
    result = side.call(name, args)
    return None if FUNCS[name][2] == "v" else u32(result)


def compare(cpu, side, name, mret, cret, mevents, label):
    problems = []
    if side.lib.ae_ram_failed(side.ram):
        problems.append("C AeRam error: " + side.lib.ae_ram_error(side.ram).decode(errors="replace"))
    if FUNCS[name][2] != "v" and mret != cret:
        problems.append(f"return differs: MIPS={mret:08X}, C={cret:08X}")
    if mevents != side.events:
        problems.append(f"hook/hardware events differ: MIPS={mevents!r}, C={side.events!r}")
    lo = STACK_LO - 0x80000000
    main_c = bytes(side.main)
    if bytes(cpu.ram[:lo]) != main_c[:lo]:
        diffs = [i for i, (a, b) in enumerate(zip(cpu.ram[:lo], main_c[:lo])) if a != b]
        problems.append("RAM differs: " + ", ".join(
            f"{0x80000000 + off:08X} MIPS={cpu.ram[off]:02X} C={main_c[off]:02X}" for off in diffs[:8]))
    if bytes(cpu.scratch) != bytes(side.scr):
        diffs = [i for i, (a, b) in enumerate(zip(cpu.scratch, side.scr)) if a != b]
        problems.append("scratchpad differs: " + ", ".join(
            f"{mr.SCRATCH_BASE + off:08X} MIPS={cpu.scratch[off]:02X} C={side.scr[off]:02X}" for off in diffs[:8]))
    return problems


def scenario_case(cpu, side, base, blobs, name, index, seed):
    cpu.restore(base)
    area = sorted(blobs)[index % len(blobs)]
    cpu.load_area(blobs[area])
    scenario = Scenario(cpu, name, index, area, seed)
    args = scenario.make()
    side.sync(cpu)
    return args, area


def run_case(cpu, side, base, blobs, name, index, seed):
    args, area = scenario_case(cpu, side, base, blobs, name, index, seed)
    initial = cpu.snapshot()
    mevents = []
    mret = run_mips(cpu, name, args, mevents)
    mips_after = cpu.snapshot()
    cpu.restore(initial)
    side.sync(cpu)
    cret = call_c(side, name, args)
    cpu.restore(mips_after)
    return compare(cpu, side, name, mret, cret, mevents,
                   f"{name}[{index}] area {area:02X}")
MUTATIONS = {
    "SaveSlotInfoPtr": [
        ("wrong slot stride", "slot * AE_PROGRESS_SAVE_SLOT_SIZE",
         "slot * (AE_PROGRESS_SAVE_SLOT_SIZE + 1u)"),
        ("one-byte slot base offset", "slot * AE_PROGRESS_SAVE_SLOT_SIZE",
         "slot * AE_PROGRESS_SAVE_SLOT_SIZE + 1u"),
    ],
    "MeshAnimAdvance": [
        ("invert frozen-bit test", "if ((flags & 1u) != 0u) return;",
         "if ((flags & 1u) == 0u) return;"),
        ("use frozen bit for loop behavior", "(flags & 2u) == 0u ? 1u : frame",
         "(flags & 1u) == 0u ? 1u : frame"),
    ],
    "SpriteEffectQueue0": [
        ("queue effect kind 1", "ae_SpriteEffectQueue(r, spec, 0);",
         "ae_SpriteEffectQueue(r, spec, 1);"),
        ("queue effect kind 2", "ae_SpriteEffectQueue(r, spec, 0);",
         "ae_SpriteEffectQueue(r, spec, 2);"),
    ],
    "SoundLoopsPause": [
        ("invert loop A active test",
         "if (ae_rds16(r, loop_a + AE_PROGRESS_SOUND_LOOP_VOICE) != 0)",
         "if (ae_rds16(r, loop_a + AE_PROGRESS_SOUND_LOOP_VOICE) == 0)"),
        ("wrong loop A sound id", "0x16, ae_rds16(r, loop_a)",
         "0x17, ae_rds16(r, loop_a)"),
    ],
    "ClearDAT_800BEF70": [
        ("leave clear flag set", "ae_wr32(r, AE_PROGRESS_G_PAUSE_MENU_CLEAR, 0u);",
         "ae_wr32(r, AE_PROGRESS_G_PAUSE_MENU_CLEAR, 1u);"),
        ("clear only low halfword", "ae_wr32(r, AE_PROGRESS_G_PAUSE_MENU_CLEAR, 0u);",
         "ae_wr16(r, AE_PROGRESS_G_PAUSE_MENU_CLEAR, 0u);"),
    ],
    "OpenPauseMenu": [
        ("invert request branch", "if (request == 1)", "if (request != 1)"),
        ("wrong open pause state", "ae_wr16(r, pause_state, 0x0Fu);",
         "ae_wr16(r, pause_state, 0x0Eu);"),
    ],
    "DrawTextSprites": [
        ("skip glyph 0x10", "if (glyph >= 0x10u)", "if (glyph > 0x10u)"),
        ("wrong page shift", "AE_PROGRESS_TEXT_SPRITE_PAGE_SHIFT", "5u"),
    ],
    "SoundSetVolumes": [
        ("allow SFX volume 128", "if (sfx_volume >= 0x80)",
         "if (sfx_volume > 0x80)"),
        ("clamp BGM volume one step low",
         "if (bgm_volume >= 0x80) bgm_volume = 0x7F;",
         "if (bgm_volume >= 0x80) bgm_volume = 0x7E;"),
    ],
    "SoundVolumesDefault": [
        ("wrong default BGM volume", "ae_wr16(r, AE_PROGRESS_G_BGM_VOLUME, 75u);",
         "ae_wr16(r, AE_PROGRESS_G_BGM_VOLUME, 76u);"),
        ("wrong default SFX volume", "ae_wr16(r, AE_PROGRESS_G_SFX_VOLUME, 0x7Fu);",
         "ae_wr16(r, AE_PROGRESS_G_SFX_VOLUME, 0x7Eu);"),
    ],
    "SoundGetVolumes": [
        ("overlap output volume words",
         "ae_wr32(r, out + AE_PROGRESS_HEADER_VOLUME_WORD_SIZE, (uint32_t)bgm_volume);",
         "ae_wr32(r, out + 2u, (uint32_t)bgm_volume);"),
        ("return BGM in both output fields",
         "ae_wr32(r, out, (uint32_t)sfx_volume);",
         "ae_wr32(r, out, (uint32_t)(bgm_volume + (sfx_volume & 0)));"),
    ],
    "SoundSaveVolumes": [
        ("read SFX for saved BGM", "uint32_t bgm_volume = ae_rd16(r, AE_PROGRESS_G_BGM_VOLUME);",
         "uint32_t bgm_volume = ae_rd16(r, AE_PROGRESS_G_SFX_VOLUME);"),
        ("save BGM into SFX slot", "ae_wr16(r, AE_PROGRESS_G_SAVED_BGM_VOLUME, bgm_volume);",
         "ae_wr16(r, AE_PROGRESS_G_SAVED_SFX_VOLUME, bgm_volume);"),
    ],
    "SoundRestoreVolumes": [
        ("swap restored volume arguments",
         "return ae_SoundSetVolumes(r, h, sfx_volume, bgm_volume);",
         "return ae_SoundSetVolumes(r, h, bgm_volume, sfx_volume);"),
        ("restore BGM from saved SFX",
         "ae_rd16(r, AE_PROGRESS_G_SAVED_BGM_VOLUME)",
         "ae_rd16(r, AE_PROGRESS_G_SAVED_SFX_VOLUME)"),
    ],
    "SaveBufferFill": [
        ("invert time-attack save branch", "if (ae_IsTimeAttack(r) != 0)",
         "if (ae_IsTimeAttack(r) == 0)"),
        ("use halfword stride for time records",
         "ae_rd32(r, AE_PROGRESS_G_TIME_RECORDS +\n                               i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE)",
         "ae_rd32(r, AE_PROGRESS_G_TIME_RECORDS +\n                               i * AE_PROGRESS_SAVE_ARRAY_HALFWORD_SIZE)"),
    ],
    "SaveCaptureState": [
        ("shift saved progress destination by one word",
         "AE_PROGRESS_SAVE_BLOCK_PROGRESS_FLAGS +\n                   i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE",
         "AE_PROGRESS_SAVE_BLOCK_PROGRESS_FLAGS + 4u +\n                   i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE"),
        ("shift progress source by one word",
         "AE_PROGRESS_G_FLAGS +\n                           i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE",
         "AE_PROGRESS_G_FLAGS + 4u +\n                           i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE"),
    ],
    "SaveHeaderFill": [
        ("invert first option bit test",
         "ae_rd16(r, AE_PROGRESS_G_OPTION_FLAG_B) != 0u",
         "ae_rd16(r, AE_PROGRESS_G_OPTION_FLAG_B) == 0u"),
        ("shift saved ammo byte into wrong field",
         "ae_rd8(r, AE_PROGRESS_G_SAVED_OPTION_BYTE) << 16",
         "ae_rd8(r, AE_PROGRESS_G_SAVED_OPTION_BYTE) << 8"),
    ],
    "RecordBestTime": [
        ("exclude last time-attack level from shared record",
         "AE_PROGRESS_STAGE_RESULT_LEVELS)",
         "(AE_PROGRESS_STAGE_RESULT_LEVELS - 1u))"),
        ("index time-attack record by current level",
         "record_index = AE_PROGRESS_STAGE_RESULT_TIME_ATTACK_FIRST;",
         "record_index = level;"),
    ],
    "SaveBlockPtr": [
        ("return save block plus one word", "return AE_PROGRESS_SAVE_BLOCK;",
         "return AE_PROGRESS_SAVE_BLOCK + 4u;"),
        ("return save block minus one word", "return AE_PROGRESS_SAVE_BLOCK;",
         "return AE_PROGRESS_SAVE_BLOCK - 4u;"),
    ],
    "ProgressMarkStageResult": [
        ("invert special-area classification",
         "if (area == AE_PROGRESS_STAGE_RESULT_SPECIAL_AREA)",
         "if (area != AE_PROGRESS_STAGE_RESULT_SPECIAL_AREA)"),
        ("award one extra pickup",
         "count + AE_PROGRESS_STAGE_RESULT_BONUS",
         "count + AE_PROGRESS_STAGE_RESULT_BONUS + 1u", 2),
    ],
    "AreaKindVisitedGet": [
        ("read next kind bit",
         "visited >> ((uint32_t)kind & 31u)",
         "visited >> (((uint32_t)kind + 1u) & 31u)"),
        ("clear visited result bit", "& 1u);", "& 0u);"),
    ],
    "LevelProgressGet": [
        ("exclude final time-attack progress slot",
         "AE_PROGRESS_STAGE_RESULT_LEVELS)",
         "(AE_PROGRESS_STAGE_RESULT_LEVELS - 1u))"),
        ("return level instead of time-attack progress",
         "return (int32_t)ae_rd8(r, AE_PROGRESS_G_TIME_ATTACK_LEVEL_COUNT);",
         "return (int32_t)level;"),
    ],
    "HubDestinationState": [
        ("wrong special destination state", "        return 2;", "        return 1;"),
        ("accept progress one as ready", "ae_LevelProgressGet(r, (uint32_t)level) == 2",
         "ae_LevelProgressGet(r, (uint32_t)level) == 1"),
    ],
    "HubGateLocked": [
        ("invert gate-area match", "if (listed_area == area &&",
         "if (listed_area != area &&"),
        ("invert gate bit lock test",
         "(progress & (1u << (index & 31u))) == 0u",
         "(progress & (1u << (index & 31u))) != 0u"),
    ],
    "UnlockSpecialArea": [
        ("set next progress bit", "1u << (progress_index & 31u)",
         "1u << ((progress_index + 1u) & 31u)"),
        ("unlock wrong gadget for first special area",
         "AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_0,\n            AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_1",
         "AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_1,\n            AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_1"),
    ],
    "HubGateBitTest": [
        ("test following gate bit", "1u << (bit & 31u)",
         "1u << ((bit + 1u) & 31u)"),
        ("test preceding gate bit", "1u << (bit & 31u)",
         "1u << ((bit + 31u) & 31u)"),
    ],
    "ExitSpecialArea": [
        ("map special area 2 to wrong destination",
         "case 2u:\n        ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 20u);",
         "case 2u:\n        ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 21u);"),
        ("map special area 5 to wrong destination",
         "ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 53u);",
         "ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 52u);"),
    ],
}


def mutate_function(source, name, change):
    _, _, _, cname, _, _ = FUNCS[name]
    signature = re.search(
        r"\b(?:uint32_t|int32_t|void)\s+" + re.escape(cname) +
        r"\s*\([^;]*?\)\s*\{", source, re.S)
    if signature is None:
        raise AssertionError(f"cannot locate {cname} for mutation")
    open_brace = signature.end() - 1
    depth = 0
    close_brace = None
    for pos in range(open_brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                close_brace = pos
                break
    if close_brace is None:
        raise AssertionError(f"unclosed body for {cname}")

    label, old, new = change[:3]
    expected_count = change[3] if len(change) > 3 else 1
    body = source[open_brace + 1:close_brace]
    count = body.count(old)
    if count != expected_count:
        raise AssertionError(f"{name} mutation {label!r}: expected {expected_count} target(s), found {count}")
    body = body.replace(old, new, expected_count)
    return source[:open_brace + 1] + body + source[close_brace:]


def make_mutant_source(destination, variant):
    source = (ROOT / "src/world/ae_progress.c").read_text(encoding="utf-8")
    labels = {}
    for name in FUNCS:
        changes = MUTATIONS[name]
        if len(changes) != 2:
            raise AssertionError(f"{name} must have two semantic mutations")
        change = changes[variant]
        source = mutate_function(source, name, change)
        labels[name] = change[0]
    destination.write_text(source, encoding="utf-8")
    return labels


def prepare_oracle():
    reg.declare("stack_window", f"{STACK_LO:08X}..{STACK_HI:08X}")
    cpu = mr.MipsRam.from_extract()
    cpu.coverage = set()
    base = cpu.snapshot()
    member_map = dump_areas.area_members()
    area_ids = [a for a in (1, 2, 5, 19, 88, 99, 103) if a in member_map]
    if len(area_ids) < 4:
        raise SystemExit(f"expected several real area members, got {area_ids}")
    blobs = {a: (ROOT / f"extract/members/{member_map[a]:03X}.bin").read_bytes()
             for a in area_ids}
    print(f"loaded real area members: {', '.join(f'{a:02X}->{member_map[a]:03X}' for a in area_ids)}")
    return cpu, base, blobs, area_ids


def run_default(show_coverage=False):
    start = time.time()
    cg.SOURCES.append("src/world/ae_progress.c") if "src/world/ae_progress.c" not in cg.SOURCES else None
    dll = build_port_dll()
    side = CSide(dll)
    cpu, base, blobs, area_ids = prepare_oracle()

    counts = {name: 0 for name in FUNCS}
    failures = 0
    seed = 0xA317E5C
    for name in FUNCS:
        for i in range(DEFAULT_CASES[name]):
            try:
                problems = run_case(cpu, side, base, blobs, name, i, seed)
            except Exception as e:  # keep collecting evidence for the other entries
                failures += 1
                print(f"FAIL {name}[{i}]: {type(e).__name__}: {e}", flush=True)
                continue
            counts[name] += 1
            if problems:
                failures += 1
                print(f"FAIL {name}[{i}]: " + "\n  ".join(problems), flush=True)
        print(f"{name}: {counts[name]} cases", flush=True)

    summary = reg.coverage_summary([(n, spec[0], spec[1], True) for n, spec in FUNCS.items()], cpu.coverage)
    if show_coverage:
        reg.print_coverage(summary)
    uncovered = [n for n, s in summary.items() if s["uncovered"]]
    if uncovered:
        failures += 1
        print("coverage gaps remain in: " + ", ".join(uncovered), flush=True)

    passed = failures == 0 and not uncovered
    result = reg.write_results(counts, summary, passed,
        extra={"real_areas": area_ids, "elapsed_seconds": round(time.time() - start, 2)},
        function_names=list(FUNCS))
    print(f"result: {'PASS' if passed else 'FAIL'}; {sum(counts.values())} differential cases; "
          f"{time.time() - start:.1f}s")
    if result: print(f"wrote {result.relative_to(ROOT)}")
    return 0 if passed else 1


def run_mutations():
    start = time.time()
    cg.SOURCES.append("src/world/ae_progress.c") if "src/world/ae_progress.c" not in cg.SOURCES else None
    cpu, base, blobs, _ = prepare_oracle()
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    caught = 0
    attempted = sum(len(changes) for changes in MUTATIONS.values())
    seed = 0xA317E5C

    with tempfile.TemporaryDirectory(prefix="progress_mut_", dir=out) as temp_dir:
        temp = Path(temp_dir)
        for variant in range(2):
            mutant_source = temp / f"variant_{variant + 1}.c"
            labels = make_mutant_source(mutant_source, variant)
            mutant_dll = build_port_dll(mutant_source,
                                        tag=f"progress_mutant_{variant + 1}")
            side = CSide(mutant_dll)
            for name in FUNCS:
                witness = None
                for index in range(DEFAULT_CASES[name]):
                    try:
                        problems = run_case(cpu, side, base, blobs,
                                            name, index, seed)
                    except Exception as error:  # noqa: BLE001
                        print(f"MUTATION ERROR {name} variant {variant + 1}: "
                              f"{type(error).__name__}: {error}", flush=True)
                        break
                    if problems:
                        witness = index
                        break
                if witness is None:
                    print(f"MUTATION SURVIVED {name} variant {variant + 1}: "
                          f"{labels[name]}", flush=True)
                else:
                    caught += 1
                    print(f"caught {name} variant {variant + 1} ({labels[name]}) "
                          f"with case {witness}", flush=True)

    passed = caught == attempted
    result_path = reg.RESULTS_DIR / "check_progress.json"
    if result_path.exists():
        result = json.loads(result_path.read_text(encoding="utf-8"))
        result["mutations"] = {"attempted": attempted, "caught": caught}
        result["mutation_elapsed_seconds"] = round(time.time() - start, 2)
        result["passed"] = bool(result.get("passed")) and passed
        result_path.write_text(json.dumps(result, indent=1), encoding="utf-8")
    print(f"mutations: {caught}/{attempted} caught; {time.time() - start:.1f}s")
    return 0 if passed else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coverage", action="store_true",
                        help="print executed instruction coverage")
    parser.add_argument("--mutate", action="store_true",
                        help="run the 52 behavior-level differential mutants")
    args = parser.parse_args(argv)
    if args.coverage and args.mutate:
        parser.error("--coverage and --mutate are separate runs")
    if args.mutate:
        return run_mutations()
    return run_default(show_coverage=args.coverage)


if __name__ == "__main__":
    raise SystemExit(main())
