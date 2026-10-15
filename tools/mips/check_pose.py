"""Differential checks for the model pose, animation and matrix ports.

The original routines execute from SCUS_944.23 with area 0x01's real member 05B.bin loaded at
0x80154000. The C side receives the same RAM/scratchpad image. Calls to the SDK GTE routines run
through the pinned external GTE oracle used by the other RAM-image checkers.

Run with .venv/Scripts/python.exe tools/mips/check_pose.py [--coverage] [--cases PERCENT].
"""
import argparse
import ctypes as C
import os
import random
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import dump_areas as da  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

OUT = ROOT / "extract/mips"
M32 = 0xFFFFFFFF
STACK_LO = 0x801FF000
AREA_BASE = 0x80154000
ACTOR = 0x80110000
BONE_BUFFER = 0x80110200
MATRIX_BUFFER = 0x80111000
TABLE_BUFFER = 0x80112000
FRAME_BUFFER = 0x80113000
STREAM_BUFFER = 0x80114000
MODEL_STUB = 0x80115000
TEST_MATRIX_A = 0x80100000
TEST_MATRIX_B = 0x80100040
TEST_MATRIX_OUT = 0x80100080
TEST_VECTOR = 0x80100100
GLOBAL_ANGLES = 0x800F4B30
GLOBAL_BONES = 0x800F4730
VIEW_MATRIX = 0x800E78C0
DEFAULT_SEED = 0x944230

FUNCS = {
    "MatrixIdentity": (0x8004637C, 40, "ae_sdk_MatrixIdentity", 1, False),
    "MatrixFromDirXZ": (0x80014804, 72, "ae_sdk_MatrixFromDirXZ", 2, False),
    "RotMatrixZYX (SDK)": (0x8002FCC0, 100, "ae_sdk_RotMatrixZYX", 2, False),
    "RotMatrixX (SDK)": (0x80030A30, 408, "ae_sdk_RotMatrixX", 2, False),
    "RotMatrixY (SDK)": (0x80030890, 408, "ae_sdk_RotMatrixY", 2, False),
    "RotMatrixZ (SDK)": (0x800306F0, 408, "ae_sdk_RotMatrixZ", 2, False),
    "ScaleMatrix (SDK)": (0x80030E60, 312, "ae_sdk_ScaleMatrix", 2, False),
    "CompMatrix (SDK)": (0x8002FF50, 352, "ae_sdk_CompMatrix", 3, False),
    "MatrixLookDir": (0x8001484C, 568, "ae_sdk_MatrixLookDir", 2, False),
    "ActorAnimDecode": (0x80014E34, 188, "ae_ActorAnimDecode", 1, False),
    "AnimDecodeLerp": (0x80014FA0, 672, "ae_AnimDecodeLerp", 4, False),
    "ModelBuildBoneMatrices": (0x80015504, 636, "ae_ModelBuildBoneMatrices", 2, True),
    "ModelPoseBones": (0x80015780, 832, "ae_ModelPoseBones", 2, True),
}

# The RotMatrixZYX entry is a 100-byte head which tail-jumps through two adjacent original blocks.
# MatrixLookDir calls VectorNormalSS and its shared GTE normalization core.
EXTRA_COVERAGE = {
    "RotMatrixZYX tail blocks": (0x8002FD24, 0x228),
    "VectorNormalSS wrapper": (0x800301E0, 0x30),
    "VectorNormalSS core": (0x80030210, 0xC0),
}
BASE_CASES = {
    "MatrixIdentity": 12,
    "MatrixFromDirXZ": 72,
    "RotMatrixZYX (SDK)": 180,
    "RotMatrixX (SDK)": 160,
    "RotMatrixY (SDK)": 160,
    "RotMatrixZ (SDK)": 160,
    "ScaleMatrix (SDK)": 120,
    "CompMatrix (SDK)": 220,
    "MatrixLookDir": 160,
    "ActorAnimDecode": 100,
    "AnimDecodeLerp": 180,
    "ModelBuildBoneMatrices": 100,
    "ModelPoseBones": 140,
}

SINE_EDGE_ANGLES = [
    -32768, -8193, -8192, -4097, -4096, -3073, -3072, -2049, -2048, -1025, -1024,
    -1, 0, 1, 1023, 1024, 1025, 2047, 2048, 2049, 3071, 3072, 3073, 4095, 4096, 8192, 32767,
]
DIR_EDGE_VECTORS = [
    (0, 0, 0), (0, 4096, 0), (0, -4096, 0), (4096, 0, 0), (-4096, 0, 0),
    (0, 0, 4096), (0, 0, -4096), (32767, 0, 0), (-32768, 0, 0),
    (32767, 32767, 0), (32767, 0, 32767), (-32768, -32768, -32768),
]


def s32(value):
    value &= M32
    return value - 0x100000000 if value & 0x80000000 else value


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def sign_bits(value, shift, bits):
    part = (value >> shift) & ((1 << bits) - 1)
    return part - (1 << bits) if part & (1 << (bits - 1)) else part


def packed_xyz(x, y, z):
    return (x & 0x3FF) | ((y & 0xFFF) << 10) | ((z & 0x3FF) << 22)


def random_s16(rng):
    return rng.choice([-32768, -8192, -4096, -1, 0, 1, 2048, 4096, 8192, 32767,
                       rng.randint(-32768, 32767)])


def make_matrix(rotation=None, translation=None, padding=0):
    if rotation is None:
        rotation = [4096, 0, 0, 0, 4096, 0, 0, 0, 4096]
    if translation is None:
        translation = [0, 0, 0]
    return struct.pack("<9hH3i", *[s16(v) for v in rotation], padding & 0xFFFF,
                       *[s32(v) for v in translation])


def write_matrix(m, address, rotation=None, translation=None, padding=0):
    m.write_bytes(address, make_matrix(rotation, translation, padding))


def raw_u32(blob, offset):
    return struct.unpack_from("<I", blob, offset)[0]


def raw_model(blob, offset):
    if offset < 0 or offset + 12 > len(blob):
        raise ValueError(f"model header outside member at {offset:X}")
    flags, bone_count, vertex_count, color_count, has_normals = struct.unpack_from("<IHHHH", blob, offset)
    colors = offset + 12
    bones = colors + color_count * 4
    vertices = bones + bone_count * 12
    if vertices > len(blob):
        raise ValueError(f"model bone array outside member at {offset:X}")
    return {"offset": offset, "flags": flags, "bone_count": bone_count, "vertex_count": vertex_count,
            "color_count": color_count, "has_normals": has_normals, "bone_offset": bones,
            "vertex_offset": vertices}


def fixed_frame_values(blob, frame_offset, bone_count, key):
    stride = (bone_count + 2) * 4
    word_offset = frame_offset + key * stride
    angles = []
    for bone in range(bone_count):
        packed = raw_u32(blob, word_offset + 8 + 4 * bone)
        angles.append((sign_bits(packed, 0, 10) * 4,
                       sign_bits(packed, 10, 12) * 4,
                       sign_bits(packed, 22, 10) * 4))
    return angles


class PoseAssets:
    def __init__(self):
        self.area_id = 0x01
        self.member_id = da.area_members()[self.area_id]
        self.blob = (da.MEMBERS / f"{self.member_id:03X}.bin").read_bytes()
        self.monkey_section = raw_u32(self.blob, 0x4E4)
        self.monkey_header = struct.unpack_from("<11I", self.blob, self.monkey_section)
        if self.monkey_header[0] != 0x11:
            raise ValueError("area 0x01 member has no recognized monkey section")

        monkey_model_offset = self.monkey_section + self.monkey_header[2]
        self.monkey_model = raw_model(self.blob, monkey_model_offset)
        self.monkey_model["address"] = AREA_BASE + monkey_model_offset
        self.monkey_model["bone_bytes"] = self.blob[self.monkey_model["bone_offset"]:
                                                    self.monkey_model["bone_offset"] + self.monkey_model["bone_count"] * 12]

        # In member 05B, the first fixed-frame table is empty; the second table and its frames are real.
        self.anim_table_offset = self.monkey_section + self.monkey_header[7]
        self.anim_table_end = self.monkey_section + self.monkey_header[8]
        self.anim_frames_offset = self.monkey_section + self.monkey_header[9]
        self.anim_table_address = AREA_BASE + self.anim_table_offset
        self.anim_frames_address = AREA_BASE + self.anim_frames_offset
        self.fixed_clips = []
        entry_count = (self.anim_table_end - self.anim_table_offset) // 8
        stride = (self.monkey_model["bone_count"] + 2) * 4
        for index in range(entry_count):
            first, last, spacing, key = struct.unpack_from("<4H", self.blob, self.anim_table_offset + index * 8)
            final_key = key + ((last - first) // spacing + 1) if spacing and last >= first else key
            if (last != 0xFFFF and spacing and last >= first and
                    self.anim_frames_offset + (final_key + 1) * stride <= len(self.blob)):
                self.fixed_clips.append({"index": index, "first": first, "last": last,
                                         "spacing": spacing, "key": key})
        if not self.fixed_clips:
            raise ValueError("area 0x01 has no usable real monkey animation clip")

        area_header = raw_u32(self.blob, 0x4D0)
        feature_words = struct.unpack_from("<8I", self.blob, area_header + 0x34)
        feature = next((area_header + (word >> 8) * 4 for word in feature_words if (word & 0xFF) == 0x10), None)
        if feature is None:
            raise ValueError("area 0x01 has no feature 0x10 actor")
        feature_model_offset = feature + struct.unpack_from("<H", self.blob, feature + 4)[0] * 4
        feature_stream_offset = feature + struct.unpack_from("<H", self.blob, feature + 8)[0] * 4
        self.feature_model = raw_model(self.blob, feature_model_offset)
        self.feature_model["address"] = AREA_BASE + feature_model_offset
        self.feature_model["bone_bytes"] = self.blob[self.feature_model["bone_offset"]:
                                                     self.feature_model["bone_offset"] + self.feature_model["bone_count"] * 12]
        self.feature_stream_offset = feature_stream_offset
        self.feature_stream_address = AREA_BASE + feature_stream_offset
        self.feature_channel_count = raw_u32(self.blob, feature_stream_offset)
        self.feature_descriptors = self.blob[feature_stream_offset + 4:
                                             feature_stream_offset + 4 + self.feature_channel_count * 4]
        self.feature_frame_start = feature_stream_offset + 4 + self.feature_channel_count * 4
        if len(self.feature_descriptors) != self.feature_channel_count * 4:
            raise ValueError("area 0x01 feature channel descriptors are truncated")

    def fixed_animation_case(self, rng, mode):
        clip = rng.choice(self.fixed_clips)
        if mode == "first":
            frame = clip["first"]
        elif mode == "last":
            frame = clip["last"]
        elif mode == "before" and clip["first"] > 0 and clip["key"] > 0:
            frame = clip["first"] - 1
        else:
            frame = rng.randint(clip["first"], clip["last"])
        return clip, frame


ASSETS = PoseAssets()


def set_actor(m, model_def, *, anim_set=0, anim_table=0, frame=0, anim_index=0, position=None,
              matrix_seed=0, matrix_count=None):
    if position is None:
        position = [0, 0, 0]
    count = model_def["bone_count"]
    matrix_count = matrix_count if matrix_count is not None else max(3, count + 1)
    m.write_bytes(ACTOR, bytes(0x1B0))
    m.write_bytes(BONE_BUFFER, model_def["bone_bytes"] if "bone_bytes" in model_def else bytes(count * 12))
    m.write_bytes(MATRIX_BUFFER, bytes(32 * matrix_count))
    m.write32(ACTOR, model_def["address"])
    m.write32(ACTOR + 4, model_def["address"] + 12)
    m.write32(ACTOR + 8, 0)
    m.write32(ACTOR + 0x0C, BONE_BUFFER)
    m.write32(ACTOR + 0x10, model_def["address"] + model_def.get("vertex_offset", 0))
    m.write32(ACTOR + 0x14, MATRIX_BUFFER)
    m.write32(ACTOR + 0x18, anim_set)
    for axis, value in enumerate(position):
        m.write32(ACTOR + 0x24 + 4 * axis, value & M32)
    m.write16(ACTOR + 0x5C, frame)
    m.write16(ACTOR + 0x5E, anim_index)
    m.write32(ACTOR + 0x16C, anim_table)
    rng = random.Random(matrix_seed)
    for matrix in range(matrix_count):
        if matrix == 0 and matrix_seed & 1:
            write_matrix(m, MATRIX_BUFFER + matrix * 32, translation=[rng.randint(-40000, 40000) for _ in range(3)])
        else:
            rotation = [rng.randint(-6000, 6000) for _ in range(9)]
            translation = [rng.randint(-80000, 80000) for _ in range(3)]
            write_matrix(m, MATRIX_BUFFER + matrix * 32, rotation, translation, rng.getrandbits(16))


def set_view_matrix(m, rng, *, identity=False):
    if identity:
        write_matrix(m, VIEW_MATRIX)
    else:
        write_matrix(m, VIEW_MATRIX, [rng.randint(-6000, 6000) for _ in range(9)],
                     [rng.randint(-30000, 30000) for _ in range(3)], rng.getrandbits(16))


def set_pose_angles_from_frame(m, frame_pointer, bone_count, key):
    stride = (bone_count + 2) * 4
    frame_address = frame_pointer + key * stride
    for bone in range(bone_count):
        packed = m.read32(frame_address + 8 + bone * 4)
        values = (sign_bits(packed, 0, 10) * 4, sign_bits(packed, 10, 12) * 4,
                  sign_bits(packed, 22, 10) * 4)
        for axis, value in enumerate(values):
            m.write16(GLOBAL_ANGLES + bone * 8 + axis * 2, value & 0xFFFF)
        m.write16(GLOBAL_ANGLES + bone * 8 + 6, 0xA000 | bone)


def write_synthetic_fixed_frames(m, base, bone_count, rng, frame_count=8):
    stride = (bone_count + 2) * 4
    for frame in range(frame_count):
        words = [rng.getrandbits(32), 0]
        root_options = [(-512, -2048, 511), (-1, -1, 0), (0, 0, 0), (511, 2047, -512)]
        root = root_options[frame % len(root_options)] if frame < 4 else tuple(
            (rng.randint(-512, 511), rng.randint(-2048, 2047), rng.randint(-512, 511)))
        words[1] = packed_xyz(*root)
        for bone in range(bone_count):
            if frame == 0 and bone == 0:
                angles = (511, 0, 0)
            elif frame == 1 and bone == 0:
                angles = (-511, 512, -512)  # wrapped crossing and exact +/-2048 ties after *4
            elif frame == 2 and bone == 0:
                angles = (0, 0, 0)
            elif frame == 3 and bone == 0:
                angles = (-512, 0, 0)  # exact -2048 interpolation tie for x
            else:
                angles = (rng.randint(-512, 511), rng.randint(-2048, 2047), rng.randint(-512, 511))
            words.append(packed_xyz(*angles))
        m.write_bytes(base + frame * stride, struct.pack("<" + "I" * len(words), *words))


def write_channel_stream(m, base, bone_count, descriptors, rng, frame_count=2, values=None):
    m.write32(base, len(descriptors))
    descriptor_bytes = bytearray()
    for bone, kind, unused, pad in descriptors:
        descriptor_bytes += bytes((bone, unused & 0xFF, kind, pad & 0xFF))
    m.write_bytes(base + 4, descriptor_bytes)
    samples = base + 4 + len(descriptor_bytes)
    for frame in range(frame_count):
        for channel, (bone, kind, _unused, _pad) in enumerate(descriptors):
            if values is not None:
                xyz = values[frame % len(values)][channel]
            elif kind == 3:
                xyz = rng.choice([(0, 0, 0), (-64, 256, -128), (256, 256, 256),
                                  (511, 2047, -512), tuple(rng.randint(-512, 511) for _ in range(3))])
            else:
                xyz = (rng.randint(-512, 511), rng.randint(-2048, 2047), rng.randint(-512, 511))
            m.write32(samples + (frame * len(descriptors) + channel) * 4, packed_xyz(*xyz))
    return samples


def case_specs(name, count, seed):
    rng = random.Random(seed ^ sum(name.encode("ascii")))
    specs = []

    def add(label, setup, args):
        specs.append((label, setup, args))

    if name == "MatrixIdentity":
        for i in range(count):
            g = random.Random(seed + i)
            add(f"#{i}", lambda m, g=g: m.write_bytes(TEST_MATRIX_OUT, g.randbytes(32)), (TEST_MATRIX_OUT,))

    elif name == "MatrixFromDirXZ":
        vectors = [(0, 0, 0), (0, -1, 0), (4096, 0, -4096), (-32768, 0, 32767)]
        for i in range(count):
            g = random.Random(seed + 0x1000 + i)
            vector = vectors[i] if i < len(vectors) else tuple(random_s16(g) for _ in range(3))
            alias = i in (4, 17)
            direction = TEST_MATRIX_OUT if alias else TEST_VECTOR
            matrix = TEST_MATRIX_OUT if not alias else direction
            def setup(m, g=g, vector=vector, direction=direction, matrix=matrix):
                m.write_bytes(direction, struct.pack("<3h", *[s16(v) for v in vector]))
                if not (direction == matrix):
                    m.write_bytes(matrix, g.randbytes(32))
            add(f"#{i} dir={vector} alias={alias}", setup, (direction, matrix))

    elif name == "RotMatrixZYX (SDK)":
        for i in range(count):
            g = random.Random(seed + 0x2000 + i)
            angles = tuple(SINE_EDGE_ANGLES[i % len(SINE_EDGE_ANGLES)] if i < len(SINE_EDGE_ANGLES)
                           else g.randint(-32768, 32767) for _ in range(3))
            def setup(m, g=g, angles=angles):
                m.write_bytes(TEST_VECTOR, struct.pack("<3h", *angles))
                m.write_bytes(TEST_MATRIX_OUT, g.randbytes(32))
            add(f"#{i} angles={angles}", setup, (TEST_VECTOR, TEST_MATRIX_OUT))

    elif name in {"RotMatrixX (SDK)", "RotMatrixY (SDK)", "RotMatrixZ (SDK)"}:
        angles = SINE_EDGE_ANGLES + [-2147483648, 2147483647]
        for i in range(count):
            angle = angles[i] if i < len(angles) else s32(rng.getrandbits(32))
            blob = rng.randbytes(32)
            add(f"#{i} angle={angle}",
                lambda m, blob=blob: m.write_bytes(TEST_MATRIX_OUT, blob),
                (angle, TEST_MATRIX_OUT))

    elif name == "ScaleMatrix (SDK)":
        for i in range(count):
            g = random.Random(seed + 0x3000 + i)
            alias = i == 5
            matrix = TEST_MATRIX_A
            scale = matrix if alias else TEST_VECTOR
            scales = (0, 4096, -4096) if i == 0 else tuple(g.choice(
                [0, 1, -1, 4096, -4096, 0x7FFFFFFF, 0x80000000, g.getrandbits(32)]) for _ in range(3))
            def setup(m, g=g, alias=alias, scales=scales):
                m.write_bytes(matrix, g.randbytes(32))
                if not alias:
                    m.write_bytes(scale, struct.pack("<3I", *[v & M32 for v in scales]))
            add(f"#{i} scales={scales} alias={alias}", setup, (matrix, scale))

    elif name == "CompMatrix (SDK)":
        for i in range(count):
            g = random.Random(seed + 0x4000 + i)
            alias = {2: "left", 3: "right"}.get(i)
            if i == 0:
                left_rot = [4096, 0, 0, 0, 4096, 0, 0, 0, 4096]
                right_rot = left_rot[:]
                left_trans, right_trans = [0, 0, 0], [0, 0, 0]
            elif i == 1:
                left_rot = [4096, 0, 0, 0, 4096, 0, 0, 0, 4096]
                right_rot = left_rot[:]
                left_trans, right_trans = [0x7FFFFFFF, 0, 0], [1, 0, 0]
            elif i == 4:
                left_rot = [4096, 0, 0, 0, 4096, 0, 0, 0, 4096]
                right_rot = left_rot[:]
                left_trans, right_trans = [0, 0, 0], [0x00018000, 0x00018001, 0x0000FFFF]
            else:
                left_rot = [random_s16(g) for _ in range(9)]
                right_rot = [random_s16(g) for _ in range(9)]
                left_trans = [g.randint(-500000, 500000) for _ in range(3)]
                right_trans = [g.choice([-32768, -1, 0, 1, 32767, g.randint(-32768, 32767)]) for _ in range(3)]
            def setup(m, g=g, left_rot=left_rot, right_rot=right_rot, left_trans=left_trans,
                      right_trans=right_trans):
                write_matrix(m, TEST_MATRIX_A, left_rot, left_trans, g.getrandbits(16))
                write_matrix(m, TEST_MATRIX_B, right_rot, right_trans, g.getrandbits(16))
                m.write_bytes(TEST_MATRIX_OUT, g.randbytes(32))
            output = TEST_MATRIX_A if alias == "left" else TEST_MATRIX_B if alias == "right" else TEST_MATRIX_OUT
            add(f"#{i} alias={alias or 'none'}", setup, (TEST_MATRIX_A, TEST_MATRIX_B, output))

    elif name == "MatrixLookDir":
        for i in range(count):
            g = random.Random(seed + 0x5000 + i)
            vector = DIR_EDGE_VECTORS[i % len(DIR_EDGE_VECTORS)] if i < len(DIR_EDGE_VECTORS) else tuple(
                g.randint(-30000, 30000) for _ in range(3))
            def setup(m, g=g, vector=vector):
                m.write_bytes(TEST_VECTOR, struct.pack("<3h", *[s16(v) for v in vector]))
                m.write_bytes(TEST_MATRIX_OUT, g.randbytes(32))
            add(f"#{i} dir={vector}", setup, (TEST_VECTOR, TEST_MATRIX_OUT))

    elif name == "ActorAnimDecode":
        clips = ASSETS.fixed_clips
        for i in range(count):
            g = random.Random(seed + 0x6000 + i)
            if i <= 3:
                table, frames, clip_index, frame, is_real = TABLE_BUFFER, FRAME_BUFFER, 0, 0, False
                clip_index = 2 if i == 2 else 0
                frame = 4 if i == 1 else 0
                entries = ([(0, 4, 1, 0), (10, 15, 2, 1), (0, 0, 0, 0)] if i != 3 else
                           [(4095, 4095, 4096, 1)])
            else:
                clip, frame = ASSETS.fixed_animation_case(g, "first" if i == 3 else "last" if i == 4 else "random")
                table, frames, clip_index, is_real = ASSETS.anim_table_address, ASSETS.anim_frames_address, clip["index"], True
                entries = None
            def setup(m, g=g, table=table, frames=frames, clip_index=clip_index, frame=frame,
                      is_real=is_real, entries=entries, i=i):
                set_actor(m, ASSETS.monkey_model, anim_set=frames, anim_table=table, frame=frame,
                          anim_index=clip_index, position=[0, 0, 0], matrix_seed=seed + i)
                if not is_real:
                    for index, entry in enumerate(entries):
                        m.write_bytes(table + index * 8, struct.pack("<4H", *entry))
                    write_synthetic_fixed_frames(m, frames, ASSETS.monkey_model["bone_count"], g)
            add(f"#{i} frame={frame} clip={clip_index} real={is_real}", setup, (ACTOR,))

    elif name == "AnimDecodeLerp":
        for i in range(count):
            g = random.Random(seed + 0x7000 + i)
            if i % 3 == 0:
                anim = FRAME_BUFFER
                frame_a, frame_b = (3, 4) if i == 0 else (2, 3) if i == 3 else (0, 1)
                fraction = [0, 1, 2048, 4095, 4096, M32][i % 6]
                real = False
            else:
                clip = g.choice(ASSETS.fixed_clips)
                max_key = clip["key"] + (clip["last"] - clip["first"]) // clip["spacing"]
                frame_a = g.randint(max(0, clip["key"] - (1 if clip["key"] else 0)), max_key)
                frame_b = frame_a + 1
                fraction = g.choice([0, 1, 1024, 2048, 3072, 4095, g.getrandbits(32)])
                anim, real = ASSETS.anim_frames_address, True
            def setup(m, g=g, anim=anim, frame_a=frame_a, frame_b=frame_b, real=real):
                set_actor(m, ASSETS.monkey_model, anim_set=anim, matrix_seed=seed + i)
                if not real:
                    write_synthetic_fixed_frames(m, anim, ASSETS.monkey_model["bone_count"], g)
            add(f"#{i} keys={frame_a},{frame_b} t={fraction} real={real}", setup,
                (ACTOR, frame_a, frame_b, fraction))

    elif name == "ModelBuildBoneMatrices":
        for i in range(count):
            g = random.Random(seed + 0x8000 + i)
            flags = [0, 1, 2, 3, 4, 5, 6, 7, 0x101, 0x102][i % 10]
            clip = g.choice(ASSETS.fixed_clips)
            key = clip["key"] + ((clip["last"] - clip["first"]) // clip["spacing"] if i & 1 else 0)
            zero_bones = i in (8, 9)
            model_def = dict(ASSETS.monkey_model)
            if zero_bones:
                model_def = {"address": MODEL_STUB, "bone_count": 0, "bone_bytes": b"", "vertex_offset": 0}
            def setup(m, g=g, flags=flags, key=key, model_def=model_def, zero_bones=zero_bones):
                if zero_bones:
                    m.write_bytes(MODEL_STUB, struct.pack("<IHHHH", 0, 0, 0, 0, 0))
                set_actor(m, model_def, position=[g.randint(-20000, 20000) for _ in range(3)],
                          matrix_seed=seed + i, matrix_count=max(3, model_def["bone_count"] + 1))
                set_pose_angles_from_frame(m, ASSETS.anim_frames_address, ASSETS.monkey_model["bone_count"], key) if not zero_bones else None
                if zero_bones:
                    for j in range(24):
                        m.write16(GLOBAL_ANGLES + j * 2, random_s16(g))
                set_view_matrix(m, g, identity=(i % 4 == 0))
                # This global is read for the zero-bone world case.
                m.write_bytes(GLOBAL_BONES, g.randbytes(3 * 32))
            add(f"#{i} flags={flags:X} key={key} zero_bones={zero_bones}", setup, (ACTOR, flags))

    elif name == "ModelPoseBones":
        for i in range(count):
            g = random.Random(seed + 0x9000 + i)
            flags = [0, 2, 1, 3, 4, 6, 0x102, 0xFFFF][i % 8]
            if i < 8:
                stream = ASSETS.feature_stream_address
                frame = [0, 1, 28, 29, 57, 58, 87, 88][i]
                model_def = ASSETS.feature_model
                real = True
            elif i == 8:
                stream, frame, model_def, real = STREAM_BUFFER, 0, ASSETS.feature_model, False
            elif i == 9:
                stream, frame, model_def, real = STREAM_BUFFER, 1, ASSETS.feature_model, False
            elif i == 10:
                stream, frame, model_def, real = STREAM_BUFFER, 0, ASSETS.feature_model, False
            else:
                stream, frame, model_def, real = STREAM_BUFFER, g.randrange(2), ASSETS.feature_model, False
            def setup(m, g=g, flags=flags, stream=stream, frame=frame, model_def=model_def,
                      real=real, i=i):
                if i == 11:
                    model_def = {"address": MODEL_STUB, "bone_count": 0, "bone_bytes": b"", "vertex_offset": 0}
                    m.write_bytes(MODEL_STUB, struct.pack("<IHHHH", 0, 0, 0, 0, 0))
                set_actor(m, model_def, anim_set=stream, frame=frame,
                          position=[g.randint(-12000, 12000) for _ in range(3)], matrix_seed=seed + i)
                set_view_matrix(m, g, identity=(i % 3 == 0))
                if not real:
                    bone_count = model_def["bone_count"]
                    if i == 8:
                        descriptors = [(1, 1, 0xA5, 0x5A), (2, 2, 0x69, 0x96)]
                    elif i == 9:
                        descriptors = [(bone, kind, (0x31 + kind), (0xC2 + bone))
                                       for bone in range(1, min(2, bone_count) + 1) for kind in (1, 2, 3)]
                    elif i == 10:
                        descriptors = []
                    elif i == 11:
                        descriptors = []
                    else:
                        descriptors = [(1, 1, 0xFF, 0xFF)] if bone_count else []
                    write_channel_stream(m, stream, bone_count, descriptors, g,
                                         frame_count=max(2, frame + 1))
            add(f"#{i} flags={flags:X} frame={frame} real={real}", setup, (ACTOR, flags))

    return specs[:count]


def build_dll(sdk_src=None, pose_src=None, trapv=False):
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f"pose_{os.getpid()}_{time.time_ns()}.dll"
    flags = ["-O1", "-ftrapv"] if trapv else ["-O2"]
    srcs = [ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c",
            Path(sdk_src) if sdk_src else ROOT / "src/core/ae_sdk_matrix.c",
            Path(pose_src) if pose_src else ROOT / "src/core/ae_pose.c"]
    subprocess.run([shutil.which("gcc"), "-std=c11", *flags, "-Wall", "-Wextra", "-Werror", "-shared",
                    "-I", str(ROOT / "src/core"),
                    "-static-libgcc", *map(str, srcs), "-o", str(dll)], check=True)
    return dll


class CSide:
    def __init__(self, dll):
        self.lib = lib = C.CDLL(str(dll))
        P, U32 = C.c_void_p, C.c_uint32
        lib.ae_ram_create.restype = P
        lib.ae_ptr.argtypes = [P, U32, U32]
        lib.ae_ptr.restype = P
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = C.c_int
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_clear_error.argtypes = [P]
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        for _, _, name, argc, is_void in FUNCS.values():
            fn = getattr(lib, name)
            fn.argtypes = [P] + [U32] * argc
            fn.restype = None if is_void else U32

    def sync(self, m):
        C.memmove(self.main, bytes(m.ram), 0x200000)
        C.memmove(self.scratch, bytes(m.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.ram)

    def call(self, name, args):
        _, _, c_name, _argc, _is_void = FUNCS[name]
        return getattr(self.lib, c_name)(self.ram, *[value & M32 for value in args])

    def failed(self):
        return bool(self.lib.ae_ram_failed(self.ram))

    def error(self):
        return self.lib.ae_ram_error(self.ram).decode("utf-8", "replace")


class PoseHarness:
    def __init__(self, dll, coverage=True, quiet=False):
        self.m = mr.MipsRam.from_extract(ASSETS.blob)
        self.m.attach_gte_oracle()
        if coverage:
            self.m.coverage = set()
        self.base = self.m.snapshot()
        self.c = CSide(dll)
        self.stack_lo = STACK_LO - 0x80000000
        self.counts = {}
        self.failures = 0
        self.traps_agreed = 0
        self.cases_seen = 0
        self.quiet = quiet
        reg.declare("stack_window", f"{STACK_LO:08X}..80200000")

    def report_failure(self, name, label, *details):
        self.failures += 1
        if self.quiet:
            return
        print(f"FAIL {name} [{label}]")
        for detail in details:
            print("   ", detail)

    def compare_memory(self, name, label):
        m_ram = bytes(self.m.ram)[:self.stack_lo]
        c_ram = bytes(self.c.main)[:self.stack_lo]
        if m_ram != c_ram:
            diffs = [i for i, (a, b) in enumerate(zip(m_ram, c_ram)) if a != b]
            self.report_failure(name, label, f"RAM differs at " + ", ".join(
                f"{0x80000000 + i:08X}(MIPS {m_ram[i]:02X}, C {c_ram[i]:02X})" for i in diffs[:10]))
            return False
        if bytes(self.m.scratch) != bytes(self.c.scratch):
            diffs = [i for i, (a, b) in enumerate(zip(self.m.scratch, self.c.scratch)) if a != b]
            self.report_failure(name, label, "scratchpad differs at " + ", ".join(
                f"{0x1F800000 + i:08X}(MIPS {self.m.scratch[i]:02X}, C {self.c.scratch[i]:02X})" for i in diffs[:10]))
            return False
        return True

    def run(self, name, label, setup, args):
        self.m.restore(self.base)
        self.m.gte_reset()
        setup(self.m)
        self.c.sync(self.m)
        self.m.hook_log.clear()
        self.counts[name] = self.counts.get(name, 0) + 1
        self.cases_seen += 1
        mips_trap = None
        try:
            regs = self.m.call(FUNCS[name][0], args)
            mips_result = regs[2]
        except mr.MipsError as exc:
            mips_trap = str(exc)
            mips_result = None
        c_result = self.c.call(name, args)
        c_failed = self.c.failed()

        if mips_trap:
            if c_failed:
                self.traps_agreed += 1
            else:
                self.report_failure(name, label, f"original trapped ({mips_trap}), C did not fail closed")
                return False
        elif c_failed:
            self.report_failure(name, label, f"C failed but original completed: {self.c.error()}")
            return False
        elif not FUNCS[name][4] and (mips_result & M32) != (c_result & M32):
            self.report_failure(name, label, f"return differs (MIPS {mips_result & M32:08X}, C {c_result & M32:08X})")
            return False

        return self.compare_memory(name, label)


def run_checker(args):
    dll = build_dll(trapv=args.trapv)
    harness = PoseHarness(dll, coverage=True)
    start = time.time()
    only = set(args.only or FUNCS.keys())
    unknown = only - set(FUNCS)
    if unknown:
        raise SystemExit(f"unknown function name(s): {', '.join(sorted(unknown))}")
    for name in FUNCS:
        if name not in only:
            continue
        count = max(1, round(BASE_CASES[name] * args.cases / 100.0))
        for label, setup, call_args in case_specs(name, count, args.seed):
            harness.run(name, label, setup, call_args)

    summary = None
    if harness.m.coverage is not None:
        items = [(name, addr, size, True) for name, (addr, size, *_rest) in FUNCS.items()]
        items += [(name, addr, size, False) for name, (addr, size) in EXTRA_COVERAGE.items()]
        summary = reg.coverage_summary(items, harness.m.coverage)
        if args.coverage:
            reg.print_coverage(summary)
    print("cases per function:")
    for name, count in sorted(harness.counts.items()):
        print(f"  {name:32s} {count}")
    print(f"original traps matched by C error flags: {harness.traps_agreed}")
    print(f"total cases {harness.cases_seen}; MIPS instructions {harness.m.steps}; {time.time() - start:.1f}s")
    default_run = (not args.only and args.cases == 100 and args.seed == DEFAULT_SEED and not args.trapv and
                   not args.mutate)
    if default_run:
        reg.write_results(harness.counts, summary, not harness.failures, function_names=list(FUNCS))
    if harness.failures:
        print(f"FAILED: {harness.failures} mismatches")
        return 1
    print("OK: the pose C ports match the original instructions on every case")
    if args.mutate:
        return run_mutations(harness)
    return 0


MUTATIONS = [
    ("identity-diagonal", "sdk", "ae_wr32(r, matrix, 4096u);", "ae_wr32(r, matrix, 4095u);", "MatrixIdentity", (0,)),
    ("identity-translation", "sdk", "ae_wr32(r, matrix + 28u, 0);", "ae_wr32(r, matrix + 28u, 1);", "MatrixIdentity", (0,)),
    ("dirxz-return", "sdk", "uint32_t return_value = 4096u;", "uint32_t return_value = 4095u;", "MatrixFromDirXZ", (0,)),
    ("dirxz-z-sign", "sdk", "uint32_t value = ae_rd16(r, direction + 4u);\n    value = 0u - value;",
     "uint32_t value = ae_rd16(r, direction + 4u);\n    value = value;", "MatrixFromDirXZ", (2, 3, 5)),
    ("lookdir-basis", "sdk", "if (dir_x != 0 || dir_z != 0) {", "if (dir_x != 0 && dir_z != 0) {",
     "MatrixLookDir", tuple(range(12))),
    ("lookdir-cross-round", "sdk", "if (value < 0) value = (int32_t)((uint32_t)value + 4095u);",
     "if (value < 0) value = (int32_t)((uint32_t)value + 4096u);", "MatrixLookDir", tuple(range(12))),
    ("rotmatrix-zx-sign", "sdk", "rotation[2][0] = negate32(sine_y);", "rotation[2][0] = sine_y;",
     "RotMatrixZYX (SDK)", tuple(range(27))),
    ("rotmatrix-store-order", "sdk", "ae_wr16(r, matrix + 12u, (uint32_t)rotation[2][0]);",
     "ae_wr16(r, matrix + 14u, (uint32_t)rotation[2][0]);", "RotMatrixZYX (SDK)", tuple(range(27))),
    ("scalematrix-x-scale", "sdk", "const uint32_t scales_low[] = {scale_x, scale_z, scale_y, scale_x};",
     "const uint32_t scales_low[] = {scale_y, scale_z, scale_y, scale_x};", "ScaleMatrix (SDK)", tuple(range(16))),
    ("scalematrix-z-scale", "sdk", "(uint32_t)arithmetic_shift_right((int32_t)((uint32_t)last * scale_z), 12));",
     "(uint32_t)arithmetic_shift_right((int32_t)((uint32_t)last * scale_y), 12));",
     "ScaleMatrix (SDK)", tuple(range(16))),
    ("compmatrix-negative-shift", "sdk", "if (value % 4096 < 0) --quotient;",
     "if (value % 4096 < 0) ++quotient;", "CompMatrix (SDK)", tuple(range(24))),
    ("compmatrix-translation-sign", "sdk", "right_translation[row] = signed16(raw);",
     "right_translation[row] = (int32_t)raw;", "CompMatrix (SDK)", tuple(range(24))),
    ("compmatrix-saturation", "sdk", "if (value > INT16_MAX) return INT16_MAX;",
     "if (value > INT16_MAX) return INT16_MAX - 1;", "CompMatrix (SDK)", tuple(range(24))),
    ("actor-key-index", "pose", "uint32_t frame_a = first_key + (uint32_t)key_delta;",
     "uint32_t frame_a = first_key + (uint32_t)key_delta + 1u;", "ActorAnimDecode", (1, 3, 4, 5)),
    ("actor-negative-fraction", "pose", "if (whole_numerator < 0) whole_numerator += 4095;",
     "if (whole_numerator < 0) whole_numerator += 4094;", "ActorAnimDecode", (3,)),
    ("lerp-wrap-boundary", "pose", "else if (difference <= -2049) second_x += 4096;",
     "else if (difference <= -2048) second_x += 4096;", "AnimDecodeLerp", (0, 3, 6)),
    ("lerp-negative-round", "pose", "(uint32_t)weighted + 4095u", "(uint32_t)weighted + 4096u",
     "AnimDecodeLerp", (0, 3, 6)),
    ("lerp-root-y-store", "pose", "ae_wr16(r, bones + 6u, (uint32_t)root_y);",
     "ae_wr16(r, bones + 8u, (uint32_t)root_y);", "AnimDecodeLerp", (0, 3, 6)),
    ("build-additive-root", "pose", "set_actor_world_position(r, actor, matrices, (flags & 4u) != 0);",
     "set_actor_world_position(r, actor, matrices, (flags & 8u) != 0);",
     "ModelBuildBoneMatrices", tuple(range(10))),
    ("build-world-flag", "pose", "if (flags & 2u) {", "if (flags & 4u) {",
     "ModelBuildBoneMatrices", tuple(range(10))),
    ("build-view-flag", "pose", "if (flags & 1u) {", "if (flags & 2u) {",
     "ModelBuildBoneMatrices", tuple(range(10))),
    ("build-parent-index", "pose", "uint32_t parent = ae_rd16(r, bone_record + 10u);",
     "uint32_t parent = ae_rd16(r, bone_record + 10u) + 1u;",
     "ModelBuildBoneMatrices", tuple(range(10))),
    ("pose-descriptor-kind", "pose", "ae_rd8(r, descriptor + 2u) == kind",
     "ae_rd8(r, descriptor + 1u) == kind", "ModelPoseBones", tuple(range(12))),
    ("pose-scale-factor", "pose", "signed_bits(packed, 0, 10) * 16", "signed_bits(packed, 0, 10) * 8",
     "ModelPoseBones", tuple(range(12))),
    ("pose-world-parent", "pose", "ae_sdk_CompMatrix(r, AE_POSE_GLOBAL_BONES + parent * 32u, local_matrix,\n                              AE_POSE_GLOBAL_BONES + bone_number * 32u);",
     "ae_sdk_CompMatrix(r, AE_POSE_GLOBAL_BONES + (parent + 1u) * 32u, local_matrix,\n                              AE_POSE_GLOBAL_BONES + bone_number * 32u);",
     "ModelPoseBones", tuple(range(12))),
    ("pose-local-parent", "pose",
     "        ae_sdk_CompMatrix(r, matrices + parent * 32u, local_matrix, local_matrix);\n"
     "        if (ae_ram_failed(r)) return;\n    }\n    copy_first_world_bone_position(r, actor);",
     "        ae_sdk_CompMatrix(r, matrices, local_matrix, local_matrix);\n"
     "        if (ae_ram_failed(r)) return;\n    }\n    copy_first_world_bone_position(r, actor);",
     "ModelPoseBones", tuple(range(12))),
]


def run_mutations(harness):
    killed = 0
    survivors = []
    harness.quiet = True
    harness.m.coverage = None
    for mutation_id, source_kind, needle, replacement, function, indexes in MUTATIONS:
        source_path = ROOT / ("src/core/ae_sdk_matrix.c" if source_kind == "sdk" else "src/core/ae_pose.c")
        source = source_path.read_text(encoding="utf-8")
        if needle not in source:
            raise SystemExit(f"mutation {mutation_id}: source text was not found")
        mutated_path = OUT / f"pose_mutation_{mutation_id.replace('-', '_')}.c"
        mutated_path.write_text(source.replace(needle, replacement, 1), encoding="utf-8")
        dll = build_dll(sdk_src=mutated_path if source_kind == "sdk" else None,
                        pose_src=mutated_path if source_kind == "pose" else None)
        harness.c = CSide(dll)
        harness.failures = 0
        harness.traps_agreed = 0
        harness.cases_seen = 0
        harness.counts = {}
        specs = case_specs(function, BASE_CASES[function], DEFAULT_SEED)
        for index in indexes:
            if harness.failures:
                break
            label, setup, call_args = specs[index]
            harness.run(function, label, setup, call_args)
        if harness.failures:
            killed += 1
            print(f"  KILLED {mutation_id} ({harness.failures} differential mismatch(es))")
        else:
            survivors.append(mutation_id)
            print(f"  SURVIVED {mutation_id}")
    print(f"mutation tests: {killed}/{len(MUTATIONS)} killed")
    if survivors:
        print("surviving mutations: " + ", ".join(survivors))
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=float, default=100, help="scale the default per-function case counts in percent")
    parser.add_argument("--seed", type=lambda x: int(x, 0), default=DEFAULT_SEED)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--trapv", action="store_true")
    parser.add_argument("--only", nargs="*", help="run only these function names (for focused development checks)")
    parser.add_argument("--mutate", action="store_true", help="run checker mutation tests")
    args = parser.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    if not ASSETS.blob:
        raise SystemExit("area member is empty")
    return run_checker(args)


if __name__ == "__main__":
    sys.exit(main())
