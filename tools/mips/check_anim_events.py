"""Differential checks for the player's animation-event dispatch and hit-body spawner.

The original functions run from SCUS_944.23 with real area member 05B and player member 008
loaded at their game addresses. ApplyMatrixSV uses the same GTE oracle as check_pose.py. Sound,
the two event callback tables, and printf are deterministic hooks; collision-body allocation and
registration execute the original MIPS / ported C bodies directly.

Usage: .venv/Scripts/python.exe tools/mips/check_anim_events.py [--coverage] [--mutate]
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
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import dump_areas as da  # noqa: E402
import check_bodies as cb  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

OUT = ROOT / "extract/mips"
PLAYER_MEMBER_BASE = 0x80103000
EVENT_BUFFER = 0x80140000
PLAYER = 0x800EC1E0
BONE_WORLD = 0x800F4730
ANIM_EVENT_DATA = 0x800F5120
EVENT_FIXED_POSITION = 0x800F5180
EVENT_SKIP_MODE = 0x800F44A2
BODY_SLOTS = 0x800BBDA0
BODY_POOL = 0x800BD1A0
BODY_POOL_FREE = 0x800BEDA0
BODY_HEAD = 0x800B20D8
BODY_TAIL = 0x800B20DC
BODY_SLOT_COUNT = 256
BODY_SLOT_SIZE = 0x14
BODY_COUNT = 128
BODY_SIZE = 0x38
PLAYER_POSITION = PLAYER + 0x24
PLAYER_FACING = PLAYER + 0x60
PLAYER_ANIM_FRAME = PLAYER + 0x5C
PLAYER_STATE = PLAYER + 0x5E
PLAYER_ANIM_LERP = PLAYER + 0x8A
PLAYER_HEALTH = PLAYER + 0xE8
PLAYER_EVENT_FUNCTIONS = 0x800B027C
PLAYER_ACTION_FUNCTIONS = 0x800B03AC
PLAY_SFX = 0x800877F8
PLAY_PLAYER_VOICE = 0x8006896C
PRINTF = 0x800A3720
STACK_LO = 0x801FF000
M32 = 0xFFFFFFFF
DEFAULT_SEED = 0x944230

FUNCS = {
    "PlayerRunAnimEvents": (0x80064454, 1412, "ae_PlayerRunAnimEvents", True),
    "PlayerAnimEventAddHitBodies": (0x8007C468, 432, "ae_PlayerAnimEventAddHitBodies", True),
}

EVENT_PAYLOAD = {2: 12, 3: 4, 6: 12, 7: 4}
EVENT_BYTES = {kind: 4 + payload for kind, payload in EVENT_PAYLOAD.items()}
SIMPLE_EVENT_SIZE = 4
PLAYER_DATA = (ROOT / "extract/members/008.bin").read_bytes()
PLAYER_MEMBER = ROOT / "extract/members/008.bin"
AREA_MEMBER_ID = da.area_members()[0x01]
AREA_DATA = ROOT / "extract/members" / f"{AREA_MEMBER_ID:03X}.bin"


def s32(value):
    return mr.signed(value, 32)


def s16(value):
    return mr.signed(value, 16)


def put16(mem, address, value):
    mem.write16(address, value & 0xFFFF)


def put32(mem, address, value):
    mem.write32(address, value & M32)


class PlayerAssets:
    """Set/state/event pointers and records decoded from the real archive member 008."""

    def __init__(self, blob):
        self.blob = blob
        self.sets = []
        self.events = []
        for set_index in range(10):
            header = 0x14 + set_index * 0x10
            anim, states, transitions, events = (struct.unpack_from("<H", blob, header + off)[0] * 4
                                                  for off in (0, 4, 8, 12))
            if not (states <= transitions <= events < len(blob)):
                raise ValueError(f"member 008 set {set_index}: invalid table offsets")
            self.sets.append({"index": set_index, "anim": anim, "states": states,
                              "transitions": transitions, "events": events})
            p = events
            row_index = 0
            while p < len(blob) and blob[p] != 0xFF:
                if p + 4 > len(blob):
                    raise ValueError(f"member 008 set {set_index}: truncated event header")
                state, frame, kind, argument = blob[p:p + 4]
                size = EVENT_BYTES.get(kind, SIMPLE_EVENT_SIZE)
                if p + size > len(blob):
                    raise ValueError(f"member 008 set {set_index}: truncated event payload")
                state_index = state & 0x7F
                active_state_table = states if state >= 0x80 else self.sets[0]["states"]
                first_frame = struct.unpack_from("<H", blob, active_state_table + state_index * 8)[0]
                raw = bytes(blob[p:p + size])
                self.events.append({"set": set_index, "row": row_index, "state": state,
                                    "frame_offset": frame, "frame": (first_frame + frame) & 0xFFFF,
                                    "kind": kind, "argument": argument, "raw": raw})
                row_index += 1
                p += size
            if p >= len(blob):
                raise ValueError(f"member 008 set {set_index}: missing event terminator")


ASSETS = PlayerAssets(PLAYER_DATA)


def body_world(mem, pool="free", slots="free"):
    """Recreate the empty collision-body pool/list; the original alloc/register code does the work."""
    put32(mem, BODY_HEAD, 0)
    put32(mem, BODY_TAIL, 0)
    for index in range(BODY_SLOT_COUNT):
        slot = BODY_SLOTS + index * BODY_SLOT_SIZE
        put16(mem, slot, 0 if slots == "full" else 0xFFFF)
        put16(mem, slot + 2, 0xFFFF)
        for field in (4, 8, 0xC, 0x10):
            put32(mem, slot + field, 0)
    for index in range(BODY_COUNT):
        put16(mem, BODY_POOL_FREE + index * 2, 0 if pool in ("full", "one") else 0xFFFF)
    if pool == "one":
        put16(mem, BODY_POOL_FREE + (BODY_COUNT - 1) * 2, 0xFFFF)
    if pool == "full":
        for index in range(BODY_COUNT):
            put16(mem, BODY_POOL_FREE + index * 2, 0)
    # Allocation clears a selected slot, but zeroing the pool also makes failure cases deterministic.
    mem.write_bytes(BODY_POOL, bytes(BODY_COUNT * BODY_SIZE))


def write_matrix(mem, rng, mode="mixed"):
    if mode == "identity":
        values = [0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000]
    elif mode == "quarter_turn":
        values = [0, 0, 0x1000, 0, 0x1000, 0, -0x1000, 0, 0]
    else:
        # Signed, non-overflowing matrix entries exercise GTE matrix/vector clipping and transforms.
        values = [rng.choice([-0x1000, -0x0800, -1, 0, 1, 0x0800, 0x1000]) for _ in range(9)]
    for index, value in enumerate(values):
        put16(mem, BONE_WORLD + index * 2, value)
    put16(mem, BONE_WORLD + 18, 0)
    for index in range(3):
        put32(mem, BONE_WORLD + 20 + index * 4, rng.randint(-0x20000, 0x20000))


def fill_player_world(mem, label, *, state, frame, lerp=0, pool="free", slots="free", health=5, mode=0):
    rng = random.Random(zlib.crc32(label.encode("utf-8")) ^ DEFAULT_SEED)
    body_world(mem, pool, slots)
    if "/wrap" in label:
        pos = [0x7FFFFFF0, -0x80000010, 0x7FFFFFE0]
    else:
        pos = [rng.randint(-0x100000, 0x100000) for _ in range(3)]
    for axis, value in enumerate(pos):
        put32(mem, PLAYER_POSITION + axis * 4, value)
    facing = [rng.choice([-0x1000, -0x800, 0, 0x800, 0x1000]) for _ in range(3)] + [rng.getrandbits(16)]
    for axis, value in enumerate(facing):
        put16(mem, PLAYER_FACING + axis * 2, value)
    put16(mem, PLAYER_STATE, state)
    put16(mem, PLAYER_ANIM_FRAME, frame)
    put16(mem, PLAYER_ANIM_LERP, lerp)
    put16(mem, PLAYER_HEALTH, health)
    put16(mem, EVENT_SKIP_MODE, mode)
    for offset in range(0, 0x70, 2):
        put16(mem, ANIM_EVENT_DATA + offset, rng.choice([-32768, -2049, -1, 0, 1, 2047, 32767]))
    for axis, value in enumerate([rng.choice([-32768, -1, 0, 1, 32767]) for _ in range(3)]):
        put16(mem, EVENT_FIXED_POSITION + axis * 2, value)
    matrix_mode = "identity" if rng.randrange(3) == 0 else "quarter_turn" if rng.randrange(2) == 0 else "mixed"
    write_matrix(mem, rng, matrix_mode)
    return rng


def install_event(mem, raw):
    mem.write_bytes(EVENT_BUFFER, raw + b"\xFF")
    return EVENT_BUFFER


def make_event_case(event, tag, *, frame_mode="exact", lerp=0, pool="free", slots="free", health=5, mode=0,
                    raw_override=None):
    player_set = ASSETS.sets[event["set"]]
    active_state_set = player_set if event["state"] >= 0x80 else ASSETS.sets[0]
    state_table = PLAYER_MEMBER_BASE + active_state_set["states"]
    raw = raw_override if raw_override is not None else event["raw"]
    expected_frame = event["frame"]
    if frame_mode == "before":
        player_frame = (expected_frame - 1) & 0xFFFF
    elif frame_mode == "after":
        player_frame = (expected_frame + 1) & 0xFFFF
    else:
        player_frame = expected_frame
    label = f"event/{tag}"

    def setup(mem):
        fill_player_world(mem, label, state=event["state"], frame=player_frame, lerp=lerp,
                          pool=pool, slots=slots, health=health, mode=mode)
        install_event(mem, raw)

    return {"name": "PlayerRunAnimEvents", "label": label, "tags": {tag, f"kind{event['kind']}"},
            "setup": setup, "args": (state_table, EVENT_BUFFER)}


def make_real_scan_case(player_set):
    label = f"scan/set{player_set['index']}"
    state = 0xFFFF  # No u8 event state can match; the whole real stream still traverses every record size.

    def setup(mem):
        first_events = [e for e in ASSETS.events if e["set"] == player_set["index"]]
        frame = first_events[0]["frame"] if first_events else 0
        fill_player_world(mem, label, state=state, frame=frame)

    return {"name": "PlayerRunAnimEvents", "label": label, "tags": {"scan"}, "setup": setup,
            "args": (PLAYER_MEMBER_BASE + player_set["states"], PLAYER_MEMBER_BASE + player_set["events"])}


def make_synthetic_event_case(kind, label, *, state=0, argument=1, pool="free", slots="free", health=5, mode=0,
                              payload=None, frame_mode="exact", lerp=0):
    set0 = ASSETS.sets[0]
    first = struct.unpack_from("<H", PLAYER_DATA, set0["states"] + (state & 0x7F) * 8)[0]
    size = EVENT_BYTES.get(kind, SIMPLE_EVENT_SIZE)
    if payload is None:
        payload = bytes(size - 4)
    if len(payload) != size - 4:
        raise ValueError(f"kind {kind} needs {size - 4} payload bytes")
    raw = bytes((state & 0xFF, 0, kind & 0xFF, argument & 0xFF)) + payload
    event = {"set": 0, "state": state, "frame": first,
             "raw": raw, "kind": kind}
    return make_event_case(event, label, frame_mode=frame_mode, lerp=lerp, pool=pool, slots=slots,
                           health=health, mode=mode, raw_override=raw)


def make_hit_case(label, *, pool="free", slots="free", radius=0x180, param_a=0x1234, param_b=0xABCD,
                  endpoints=None):
    if endpoints is None:
        endpoints = [(-0x1234, 0x0567, 0x2345), (0x3210, -0x4567, 0x6789)]

    def setup(mem):
        fill_player_world(mem, label, state=0, frame=0, pool=pool, slots=slots)
        for point_index, point in enumerate(endpoints):
            base = ANIM_EVENT_DATA + (0x34 if point_index == 0 else 0x3C)
            for axis, value in enumerate(point):
                put16(mem, base + axis * 2, value)

    args = (0x12345678, radius, param_a, param_b)
    return {"name": "PlayerAnimEventAddHitBodies", "label": label, "tags": {label}, "setup": setup, "args": args}


def make_compound_event_case(label, first_kind, first_payload=b"", *, first_argument=0, second_sfx=0x43):
    set0 = ASSETS.sets[0]
    state = 0
    first_frame = struct.unpack_from("<H", PLAYER_DATA, set0["states"] + state * 8)[0]
    first_size = EVENT_BYTES.get(first_kind, SIMPLE_EVENT_SIZE)
    if len(first_payload) != first_size - 4:
        raise ValueError(f"compound kind {first_kind} needs {first_size - 4} payload bytes")
    first = bytes((state, 0, first_kind, first_argument & 0xFF)) + first_payload
    second = bytes((state, 0, 1, second_sfx & 0xFF))
    raw = first + second
    label = f"synthetic/{label}"

    def setup(mem):
        fill_player_world(mem, label, state=state, frame=first_frame)
        install_event(mem, raw)

    return {"name": "PlayerRunAnimEvents", "label": label, "tags": {label, f"kind{first_kind}"},
            "setup": setup, "args": (PLAYER_MEMBER_BASE + set0["states"], EVENT_BUFFER)}


def build_cases():
    cases = []
    for event in ASSETS.events:
        event_tag = f"set{event['set']}/row{event['row']}/kind{event['kind']}"
        cases.append(make_event_case(event, event_tag + "/exact"))
        cases.append(make_event_case(event, event_tag + "/before", frame_mode="before"))
        cases.append(make_event_case(event, event_tag + "/after", frame_mode="after"))
        cases.append(make_event_case(event, event_tag + "/lerp", lerp=1))
    cases.extend(make_real_scan_case(player_set) for player_set in ASSETS.sets)

    # Kinds absent from member 008 and invalid kind bytes use real state tables with synthetic records.
    cases.extend([
        make_synthetic_event_case(5, "synthetic/kind5-action", argument=1),
        make_synthetic_event_case(7, "synthetic/kind7-fixed", argument=9, payload=b"\x12\x34\x56\x78"),
        make_synthetic_event_case(0, "synthetic/kind0-ignored"),
        make_synthetic_event_case(9, "synthetic/kind9-ignored"),
        make_synthetic_event_case(3, "synthetic/kind3-health-zero", health=0),
        make_synthetic_event_case(3, "synthetic/kind3-mode-skip", mode=11),
        make_synthetic_event_case(7, "synthetic/kind7-health-zero", argument=9, health=0,
                                  payload=b"\x00\x00\x00\x00"),
        make_synthetic_event_case(7, "synthetic/kind7-mode-skip", argument=9, mode=11,
                                  payload=b"\x00\x00\x00\x00"),
        make_synthetic_event_case(3, "synthetic/kind3-slot-table-full", slots="full"),
        make_synthetic_event_case(7, "synthetic/kind7-pool-full", argument=9, pool="full",
                                  payload=b"\x00\x00\x00\x00"),
        make_synthetic_event_case(6, "synthetic/kind6-one-body-free", argument=0x80,
                                  payload=bytes.fromhex("008000ff102030400034cdab"), pool="one"),
        make_synthetic_event_case(6, "synthetic/kind6-negative-angle-product", argument=0x80,
                                  payload=bytes.fromhex("000000ffc00000003412bc9a")),
        make_synthetic_event_case(6, "synthetic/kind6-slot-table-full", argument=0x80,
                                  payload=bytes.fromhex("008000ff102030400034cdab"), slots="full"),
        make_synthetic_event_case(2, "synthetic/kind2-negative-offsets", argument=0,
                                  payload=bytes.fromhex("80ff7f80c0007f003412bc9a")),
        make_synthetic_event_case(2, "synthetic/kind2-negative-z", argument=0,
                                  payload=bytes.fromhex("01020304000000003412bc9a")),
        make_synthetic_event_case(3, "synthetic/kind3-wrap-position", argument=0x7F,
                                  payload=bytes.fromhex("7f807f7f"), frame_mode="exact"),
        make_compound_event_case("stride-long", 6, bytes.fromhex("000000100000000000000155"),
                                 first_argument=0x80, second_sfx=0x61),
        make_compound_event_case("stride-short", 3, bytes.fromhex("00000155"),
                                 first_argument=0x30, second_sfx=0x62),
        make_compound_event_case("stride-simple", 1, second_sfx=0x63),
    ])

    cases.extend([
        make_hit_case("hit/typical"),
        make_hit_case("hit/negative-rounding", radius=-0x400,
                      endpoints=[(-1, -9, -17), (9, 17, 33)]),
        make_hit_case("hit/signed-extremes", radius=-1024, param_a=0xFFFF, param_b=0x8000,
                      endpoints=[(-32768, 32767, -1), (32767, -32768, 1)]),
        make_hit_case("hit/one-body-free", pool="one"),
        make_hit_case("hit/pool-full", pool="full"),
        make_hit_case("hit/slot-table-full", slots="full"),
        make_hit_case("hit/radius-zero", radius=0, param_a=0, param_b=0,
                      endpoints=[(0, 0, 0), (0, 0, 0)]),
        make_hit_case("hit/radius-negative-edge", radius=-1024, param_a=0x7FFF, param_b=0xFFFF,
                      endpoints=[(-1001, 0, -1), (1000, -3, 4)]),
    ])
    return cases


class EventHooks(C.Structure):
    _fields_ = [
        ("user", C.c_void_p),
        ("play_sfx", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32)),
        ("play_player_voice", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32)),
        ("event_callback", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32)),
    ]


class Model:
    def __init__(self):
        self.log = []


class CSide:
    def __init__(self, dll):
        self.lib = lib = C.CDLL(str(dll))
        P, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
        lib.ae_ram_create.restype = P
        lib.ae_ptr.argtypes = [P, U32, U32]
        lib.ae_ptr.restype = P
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = C.c_int
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_PlayerRunAnimEvents.argtypes = [P, P, P, U32, U32]
        lib.ae_PlayerRunAnimEvents.restype = None
        lib.ae_PlayerAnimEventAddHitBodies.argtypes = [P, P, U32, I32, U32, U32]
        lib.ae_PlayerAnimEventAddHitBodies.restype = None
        self.ram = lib.ae_ram_create()
        if not self.ram:
            raise MemoryError("ae_ram_create failed")
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.model = None
        self.body_hooks = cb.Hooks()
        self.event_hooks = EventHooks()
        self._keep = []
        printf_type = dict(cb.Hooks._fields_)["printf_error"]
        self._keep.append(printf_type(lambda user, ram, fmt, body_type:
                                      self.model.log.append(("printf", fmt & M32, s32(body_type)))) )
        self.body_hooks.printf_error = self._keep[-1]
        self._keep.extend([
            EventHooks._fields_[1][1](lambda user, ram, ident: self.model.log.append(("sfx", s32(ident)))),
            EventHooks._fields_[2][1](lambda user, ram, ident: self.model.log.append(("voice", s32(ident)))),
            EventHooks._fields_[3][1](lambda user, ram, target: self.model.log.append(("callback", target & M32))),
        ])
        self.event_hooks.play_sfx = self._keep[-3]
        self.event_hooks.play_player_voice = self._keep[-2]
        self.event_hooks.event_callback = self._keep[-1]

    def sync(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), 0x200000)
        C.memmove(self.scratch, bytes(cpu.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.ram)

    def failed(self):
        return bool(self.lib.ae_ram_failed(self.ram))

    def error(self):
        return self.lib.ae_ram_error(self.ram).decode("utf-8", "replace")

    def call(self, name, args):
        if name == "PlayerRunAnimEvents":
            self.lib.ae_PlayerRunAnimEvents(self.ram, C.byref(self.body_hooks), C.byref(self.event_hooks), *args)
        else:
            self.lib.ae_PlayerAnimEventAddHitBodies(self.ram, C.byref(self.body_hooks), *args)


class Harness:
    def __init__(self, dll, coverage=True, quiet=False):
        self.m = mr.MipsRam.from_extract(AREA_DATA)
        self.m.write_bytes(PLAYER_MEMBER_BASE, PLAYER_DATA)
        self.m.attach_gte_oracle()
        if coverage:
            self.m.coverage = set()
        self.base = self.m.snapshot()
        self.c = CSide(dll)
        self.counts = {}
        self.cases_seen = 0
        self.failures = 0
        self.quiet = quiet
        self.stack_offset = STACK_LO - 0x80000000
        reg.declare("stack_window", f"{STACK_LO:08X}..80200000")

    def fail(self, name, label, *details):
        self.failures += 1
        if self.quiet:
            return
        print(f"FAIL {name} [{label}]")
        for detail in details:
            print("   ", detail)

    def compare(self, name, label, model):
        problems = []
        if self.c.failed():
            problems.append("C error flag: " + self.c.error())
        if model.log != self.c.model.log:
            problems.append(f"hook sequence differs\n      MIPS {model.log}\n      C    {self.c.model.log}")
        mram = bytes(self.m.ram)[:self.stack_offset]
        cram = bytes(self.c.main)[:self.stack_offset]
        if mram != cram:
            diffs = [i for i, (a, b) in enumerate(zip(mram, cram)) if a != b]
            problems.append("RAM differs at " + ", ".join(
                f"{0x80000000 + i:08X}(MIPS {mram[i]:02X}, C {cram[i]:02X})" for i in diffs[:12]))
        if bytes(self.m.scratch) != bytes(self.c.scratch):
            diffs = [i for i, (a, b) in enumerate(zip(self.m.scratch, self.c.scratch)) if a != b]
            problems.append("scratch differs at " + ", ".join(
                f"{0x1F800000 + i:08X}(MIPS {self.m.scratch[i]:02X}, C {self.c.scratch[i]:02X})" for i in diffs[:12]))
        if problems:
            self.fail(name, label, *problems)
            return False
        return True

    def mips_hooks(self, model):
        def sound(label):
            def hook(cpu, regs):
                model.log.append((label, s32(regs[4])))
                return 0
            return hook

        def printf(cpu, regs):
            model.log.append(("printf", regs[4] & M32, s32(regs[5])))
            return 0

        def callback(address):
            def hook(cpu, regs):
                model.log.append(("callback", address & M32))
                return 0
            return hook

        hooks = {PLAY_SFX: sound("sfx"), PLAY_PLAYER_VOICE: sound("voice"), PRINTF: printf}
        for address in range(PLAYER_EVENT_FUNCTIONS, PLAYER_ACTION_FUNCTIONS, 4):
            target = self.m.read32(address)
            if target and target not in hooks:
                hooks[target] = callback(target)
        for index in range(40):
            target = self.m.read32(PLAYER_ACTION_FUNCTIONS + index * 4)
            if target and target not in hooks:
                hooks[target] = callback(target)
        return hooks

    def run(self, case, *, count=True):
        self.m.restore(self.base)
        self.m.gte_reset()
        case["setup"](self.m)
        self.c.sync(self.m)
        model = Model()
        self.c.model = Model()
        self.m.hook_log.clear()
        name = case["name"]
        if count:
            self.counts[name] = self.counts.get(name, 0) + 1
            self.cases_seen += 1
        try:
            self.m.call(FUNCS[name][0], case["args"], hooks=self.mips_hooks(model))
        except mr.MipsError as exc:
            self.fail(name, case["label"], f"original MIPS harness error: {exc}")
            return False
        self.c.call(name, case["args"])
        return self.compare(name, case["label"], model)


def build_dll(source=None):
    OUT.mkdir(parents=True, exist_ok=True)
    tag = f"anim_events_{os.getpid()}_{time.time_ns()}"
    dll = OUT / f"{tag}.dll"
    sources = [ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c", ROOT / "src/bodies/ae_bodies.c",
               ROOT / "src/player/native_player.c", Path(source) if source else ROOT / "src/player/ae_anim_events.c"]
    subprocess.run([shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared",
                    "-static-libgcc", "-I", str(ROOT / "src/core"), "-I", str(ROOT / "src/player"),
                    *map(str, sources), "-o", str(dll), "-lm"],
                   check=True)
    return dll


def default_cases():
    cases = build_cases()
    counts = {}
    for case in cases:
        counts[case["name"]] = counts.get(case["name"], 0) + 1
    return cases


def coverage_result(harness, print_coverage, only):
    items = [(name, addr, size, True) for name, (addr, size, _cname, _hooks) in FUNCS.items() if name in only]
    summary = reg.coverage_summary(items, harness.m.coverage)
    if print_coverage:
        reg.print_coverage(summary)
    for name, ent in summary.items():
        if ent["uncovered"]:
            harness.fail(name, "instruction coverage", f"undeclared instructions not executed: {ent['uncovered']}")
        if ent["stale_declared"]:
            harness.fail(name, "instruction coverage", f"declared unreachable instructions executed: {ent['stale_declared']}")
    return summary


def run_mutations(harness, cases):
    mutations = MUTATIONS
    killed = 0
    survivors = []
    harness.quiet = True
    harness.m.coverage = None
    for mutation_id, needle, replacement, probe in mutations:
        source = (ROOT / "src/player/ae_anim_events.c").read_text(encoding="utf-8")
        if needle not in source:
            raise SystemExit(f"mutation {mutation_id}: source text was not found")
        mutation_path = OUT / f"anim_events_mutation_{mutation_id.replace('-', '_')}.c"
        mutation_path.write_text(source.replace(needle, replacement, 1), encoding="utf-8")
        dll = build_dll(mutation_path)
        harness.c = CSide(dll)
        harness.failures = 0
        case = next((c for c in cases if probe in c["label"]), None)
        if case is None:
            raise SystemExit(f"mutation {mutation_id}: no test case contains {probe!r}")
        harness.run(case, count=False)
        if harness.failures:
            killed += 1
            print(f"  KILLED {mutation_id}")
        else:
            survivors.append(mutation_id)
            print(f"  SURVIVED {mutation_id}")
    print(f"mutation tests: {killed}/{len(mutations)} killed")
    if survivors:
        print("surviving mutations: " + ", ".join(survivors))
        return 1
    return 0


MUTATIONS = [
    # PlayerRunAnimEvents: matching, state lookup, event-kind routing, payload widths and hooks.
    ("event-state-compare", "if (state == player_state) {", "if (state != player_state) {", "kind1/exact"),
    ("event-state-index", "state & AE_ANIM_EVENT_FIRST_FRAME_MASK", "state & 0xFFu", "set1/row"),
    ("event-frame-compare", "relative_frame + first_frame == player_frame", "relative_frame + first_frame != player_frame", "kind1/exact"),
    ("event-lerp-gate", "anim_lerp == 0u", "anim_lerp != 0u", "kind1/exact"),
    ("event-sfx-id", "(int32_t)argument);", "(int32_t)(argument + 1u));", "kind1/exact"),
    ("event-voice-id", "play_player_voice(event_hooks->user, r, (int32_t)argument)",
     "play_player_voice(event_hooks->user, r, (int32_t)(argument + 1u))", "kind8/exact"),
    ("event-callback-table", "AE_G_PLAYER_EVENT_FUNCTIONS, argument", "AE_G_PLAYER_ACTION_FUNCTIONS, argument", "kind4/exact"),
    ("event-callback-index", "table + index * 4u", "table + index * 2u", "kind4/exact"),
    ("event-kind2-body-type", "AE_BODY_TYPE_ANIM_EVENT, body", "AE_BODY_TYPE_PLAYER_EVENT, body", "kind2/exact"),
    ("event-kind2-radius", "payload + AE_ANIM_EVENT_BONE_RADIUS));", "payload + AE_ANIM_EVENT_BONE_RADIUS + 1u));", "kind2/exact"),
    ("event-kind2-offset", "signed_byte_times_eight(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BONE_X)",
     "signed_byte_times_eight(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BONE_Y)", "synthetic/kind2-negative-offsets"),
    ("event-kind3-body-type", "    (void)body_register(r, body_hooks, AE_BODY_TYPE_PLAYER_EVENT, body);\n    if (ae_ram_failed(r)) return;\n    ae_wr16(r, body + AE_BODY_PARAM_A, AE_ANIM_EVENT_DEFAULT_BODY_HEIGHT);",
     "    (void)body_register(r, body_hooks, AE_BODY_TYPE_ANIM_EVENT, body);\n    if (ae_ram_failed(r)) return;\n    ae_wr16(r, body + AE_BODY_PARAM_A, AE_ANIM_EVENT_DEFAULT_BODY_HEIGHT);", "kind3/exact"),
    ("event-kind3-player-gate", "ae_rds16(r, AE_G_PLAYER + AE_PLAYER_HEALTH) <= 0",
     "ae_rds16(r, AE_G_PLAYER + AE_PLAYER_HEALTH) < 0", "synthetic/kind3-health-zero"),
    ("event-kind3-mode-gate", "== AE_ANIM_EVENT_SKIP_BODY_MODE", "!= AE_ANIM_EVENT_SKIP_BODY_MODE", "kind3/exact"),
    ("event-kind3-position-add", "add32(player_position, bone_offset)", "add32(player_position, -bone_offset)", "synthetic/kind3-wrap-position"),
    ("event-kind3-facing", "copy_player_facing(r, body);", "copy_player_facing(r, body + 4u);", "kind3/exact"),
    ("event-kind7-position", "AE_G_PLAYER_EVENT_FIXED_POSITION + axis * 2u", "AE_G_PLAYER_EVENT_FIXED_POSITION + axis * 4u", "synthetic/kind7-fixed"),
    ("event-kind7-origin", "#define AE_G_PLAYER_EVENT_FIXED_POSITION    0x800F5180u",
     "#define AE_G_PLAYER_EVENT_FIXED_POSITION    0x800F5182u", "synthetic/kind7-fixed"),
    ("event-kind7-body-type", "#define AE_BODY_TYPE_PLAYER_EVENT            8",
     "#define AE_BODY_TYPE_PLAYER_EVENT            6", "synthetic/kind7-fixed"),
    ("event-kind6-param", "record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BODY_PARAM_A", "record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BODY_PARAM_B", "kind6/exact"),
    ("event-kind6-radius", "int32_t radius = signed_byte_times_eight(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BONE_RADIUS);",
     "int32_t radius = ae_rds8(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BONE_RADIUS);", "kind6/exact"),
    ("event-kind6-call-parameter", "ae_PlayerAnimEventAddHitBodies(r, body_hooks, 0u, radius, param_a, param_b)", "ae_PlayerAnimEventAddHitBodies(r, body_hooks, 0u, add32(radius, 1), param_a, param_b)", "synthetic/stride-long"),
    ("event-long-stride", "#define AE_ANIM_EVENT_LONG_RECORD_SIZE      0x10u", "#define AE_ANIM_EVENT_LONG_RECORD_SIZE      0x0Cu", "synthetic/stride-long"),
    ("event-short-stride", "#define AE_ANIM_EVENT_SHORT_RECORD_SIZE     0x08u", "#define AE_ANIM_EVENT_SHORT_RECORD_SIZE     0x04u", "synthetic/stride-short"),
    ("event-simple-stride", "#define AE_ANIM_EVENT_SIMPLE_RECORD_SIZE    0x04u", "#define AE_ANIM_EVENT_SIMPLE_RECORD_SIZE    0x08u", "synthetic/stride-simple"),
    # PlayerAnimEventAddHitBodies: both interpolation positions, geometry, body fields and allocation edges.
    ("hit-first-interpolation", "interpolate_hit_body_position(r, first, 1u, 7u)", "interpolate_hit_body_position(r, first, 1u, 6u)", "hit/typical"),
    ("hit-second-interpolation", "interpolate_hit_body_position(r, second, 5u, 3u)", "interpolate_hit_body_position(r, second, 4u, 3u)", "hit/typical"),
    ("hit-interpolation-shift", "position[axis] = arithmetic_shift_right(add32(weighted_a, weighted_b), 3u);", "position[axis] = arithmetic_shift_right(add32(weighted_a, weighted_b), 2u);", "hit/negative-rounding"),
    ("hit-point-a", "AE_PLAYER_EVENT_POINT_A + axis * 2u", "AE_PLAYER_EVENT_POINT_A + axis * 2u + 2u", "hit/typical"),
    ("hit-point-b", "AE_PLAYER_EVENT_POINT_B + axis * 2u", "AE_PLAYER_EVENT_POINT_B + axis * 2u + 2u", "hit/typical"),
    ("hit-radius-bonus", "add32(radius, 0x20)", "add32(radius, 0x10)", "hit/typical"),
    ("hit-second-radius", "ae_wr16(r, second_body + AE_BODY_RADIUS, (uint32_t)radius);", "ae_wr16(r, second_body + AE_BODY_RADIUS, (uint32_t)add32(radius, 0x20));", "hit/typical"),
    ("hit-first-param-a", "ae_wr16(r, first_body + AE_BODY_PARAM_A, param_a);", "ae_wr16(r, first_body + AE_BODY_PARAM_A, param_b);", "hit/typical"),
    ("hit-second-param-b", "ae_wr16(r, second_body + AE_BODY_PARAM_B, param_b);", "ae_wr16(r, second_body + AE_BODY_PARAM_B, param_a);", "hit/typical"),
    ("hit-body-facing", "copy_player_facing(r, first_body);", "copy_player_facing(r, first_body + 4u);", "hit/typical"),
    ("hit-body-type", "#define AE_BODY_TYPE_ANIM_EVENT              6", "#define AE_BODY_TYPE_ANIM_EVENT              8", "hit/typical"),
    ("hit-owner", "ae_CollisionBodyRegister(r, hooks, type, body, AE_G_PLAYER)", "ae_CollisionBodyRegister(r, hooks, type, body, AE_G_PLAYER + 4u)", "hit/typical"),
    ("hit-first-allocation", "if (first_body != 0u && !ae_ram_failed(r)) {", "if (first_body == 0u && !ae_ram_failed(r)) {", "hit/typical"),
    ("hit-second-allocation", "if (second_body == 0u || ae_ram_failed(r)) return;", "if (second_body != 0u || ae_ram_failed(r)) return;", "hit/typical"),
    ("hit-unused-arg", "(void)unused;", "if (unused != 0u) radius = add32(radius, 1);", "hit/typical"),
    ("hit-first-position-store", "first_body + AE_BODY_POS + axis * 4u", "first_body + AE_BODY_POS + axis * 4u + 4u", "hit/typical"),
    ("hit-second-position-store", "second_body + AE_BODY_POS + axis * 4u", "second_body + AE_BODY_POS + axis * 4u + 4u", "hit/typical"),
]


def run_checker(args):
    if not (ROOT / "extract/disc/SCUS_944.23").is_file() or not PLAYER_MEMBER.exists() or not AREA_DATA.is_file():
        raise SystemExit("required real EXE, player member 008, or area 0x01 member is missing")
    cases = default_cases()
    only = set(args.only or FUNCS)
    unknown = only - set(FUNCS)
    if unknown:
        raise SystemExit("unknown function name(s): " + ", ".join(sorted(unknown)))
    if args.cases < 100:
        scale = max(0.01, args.cases / 100.0)
        mandatory = [case for case in cases if case["label"].startswith(("synthetic/", "hit/", "scan/")) or
                     case["label"].endswith(("/exact", "/before", "/after", "/lerp"))]
        target = max(len(mandatory), int(round(len(cases) * scale)))
        cases = mandatory + [case for case in cases if case not in mandatory][:max(0, target - len(mandatory))]
    dll = build_dll()
    harness = Harness(dll, coverage=True)
    started = time.time()
    for case in cases:
        if case["name"] in only:
            harness.run(case)

    summary = coverage_result(harness, args.coverage, only)
    print("cases per function:")
    for name in sorted(only):
        print(f"  {name:32s} {harness.counts.get(name, 0)}")
    print(f"total cases {harness.cases_seen}; MIPS instructions {harness.m.steps}; {time.time() - started:.1f}s")
    default_run = not args.only and args.cases == 100 and not args.mutate
    if default_run:
        reg.write_results(harness.counts, summary, not harness.failures, function_names=list(FUNCS))
    if harness.failures:
        print(f"FAILED: {harness.failures} differential mismatches or coverage gaps")
        return 1
    print("OK: animation-event ports match the original instructions")
    if args.mutate:
        return run_mutations(harness, cases)
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--only", nargs="*", help="run only these function names")
    parser.add_argument("--cases", type=float, default=100, help="percentage of differential cases to run")
    args = parser.parse_args()
    return run_checker(args)


if __name__ == "__main__":
    sys.exit(main())
