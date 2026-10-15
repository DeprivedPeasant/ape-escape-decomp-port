# Collision bodies: 1:1 port on a RAM image (`src/core/`, `src/bodies/`)

The collision-body system of the original game (the body list, the 256-slot table, the standalone body pool, the
loop-sound slots, the body-vs-body tests and the per-frame `CollisionUpdate` dispatch) is ported **instruction for
instruction**, not re-designed. It runs on a byte-exact image of PSX main RAM, so the original data (absolute
pointers, struct layouts) is used unchanged and every function can be diffed against the original code.

## Architecture

* `src/core/ae_ram.{c,h}`: `AeRam` = 2 MiB main RAM (`0x80000000..0x801FFFFF`, also the KUSEG `0x00000000` and
  KSEG1 `0xA0000000` mirrors) plus the 1 KiB scratchpad (`0x1F800000`, also its KSEG0 / KSEG1 mirrors).
  Little-endian accessors `ae_rd8/16/32`, signed variants `ae_rds8/16/32`, `ae_wr8/16/32`, `ae_ptr(r, addr, size)`,
  `ae_ram_load_exe` (PS-X EXE: 0x800 byte header, text at the header's `t_addr`) and `ae_ram_load` (the area
  member goes to `AE_AREA_BASE = 0x80154000`). **Fail loudly**: an access outside RAM / scratchpad or an
  unaligned lh / lw / sh / sw sets a sticky error flag (`ae_ram_failed`, `ae_ram_error`, first message kept) and
  reads return 0, writes are dropped. No game data is in the repository: callers load `extract/disc/SCUS_944.23`
  and the members at run time.
* `src/bodies/ae_bodies.{c,h}`: the ported functions. Names are the original names with an `ae_` prefix; globals
  are used at their original addresses through named constants (`AE_G_COLLISION_BODIES = 0x800B20D8`, ...).
  Arguments and results keep the original widths (`int32_t` for `lh`-sign-extended values, `uint32_t` for
  addresses). Every function carries its original address in a comment.
* Calls to functions outside the module go through `AeBodyHooks` (function pointers + a user pointer, see below).
  A NULL hook sets the `AeRam` error flag and returns 0 (**fail closed**), except `body_stub_return_zero`
  (the original is `return 0`, NULL means "the original").
* Maths: `VecLength3 @80046618` is `ae_player_vec_length3` of `src/player/native_player.c` (so the DLL / viewer
  must link that file; it was re-validated here against the original instructions, 1500 cases including 32-bit
  extremes). `VectorNormalS @8003019C` is translated from its listing (`ae_VectorNormalS`: GTE SQR, LZCR, the
  192-entry table at `0x800A9C14`, GPF). The table is `floor(sqrt(2^30 / (i + 64)))` for i = 0..191 (identical to
  the EXE's entries). `ae_player_normalize` of `src/player/native_player.c` is the exact port of VectorNormalSS
  (`src/core/ae_vecnorm.h`), validated against the original instructions by `tools/mips/check_player_input.py`,
  and is the normalizer the player uses.

## Using it from the host

```c
AeRam *r = ae_ram_create();
ae_ram_load_exe(r, exe_bytes, exe_size);                    /* extract/disc/SCUS_944.23, read at run time */
ae_ram_load(r, AE_AREA_BASE, area_bytes, area_size);        /* the area member, g_AreaData */
AeBodyHooks hooks = {0};                                    /* fill host calls and nested port hook tables */
ae_CollisionBodiesReset(r);                                 /* AreaLoad */
ae_CollisionBodyRegister(r, &hooks, 3, owner + 0x24, owner);/* a monkey: type 3, body embedded in the owner */
ae_CollisionUpdate(r, &hooks);                              /* once per frame; types 1 / 2 run inline C ports */
if (ae_ram_failed(r)) fprintf(stderr, "%s
", ae_ram_error(r));
```

The module links `src/core/ae_ram.c`, `src/bodies/ae_bodies.c` and `src/player/native_player.c` (for `VecLength3`).
Everything the code reads (`g_Player` fields at `0x800EC1E0`, `g_LevelGrid` width / depth, colliders, ...) must be in
the RAM image at the original addresses; the host owns those writes.

## Functions

All "diffed" functions are checked against the original instructions by `tools/mips/check_bodies.py`,
`tools/mips/check_probe.py` or `tools/mips/check_bodymove.py` (byte-identical RAM + scratchpad, return value,
hook-call sequence). See *Validation* and [bodymove.md](bodymove.md) for the movement counts.

| Original (address) | C | Status |
|---|---|---|
| CollisionBodiesReset 80071d3c | `ae_CollisionBodiesReset` | ported + diffed |
| CollisionBodyRegister 80071e44 | `ae_CollisionBodyRegister` | ported + diffed (hooks `printf_error`, `sound_loop_start`) |
| CollisionBodyAllocSlot 80071fdc | `ae_CollisionBodyAllocSlot` | ported + diffed |
| CollisionBodyUnregister 80072018 | `ae_CollisionBodyUnregister` | ported + diffed (hook `sound_loop_stop`) |
| GetObjectByIndex 80072170 | `ae_GetObjectByIndex` | ported + diffed |
| CollisionBodyIndexOf 800721b4 | `ae_CollisionBodyIndexOf` | ported + diffed |
| CollisionBodyAlloc 80072220 | `ae_CollisionBodyAlloc` | ported + diffed |
| CollisionBodyClear 8007228c | `ae_CollisionBodyClear` | ported + diffed |
| CollisionBodyFree 800722d4 | `ae_CollisionBodyFree` | ported + diffed |
| LoopSlotAdd / Ref / Sweep / ClearAll 80072334 / 80072380 / 800723e8 / 80072454 | `ae_LoopSlot*` | ported + diffed |
| CollisionBodyGetCenter 80077c28 | `ae_CollisionBodyGetCenter` | ported + diffed (not in the task list; every body test calls it) |
| BodyCollideWithBodies 800777ac | `ae_BodyCollideWithBodies` | ported + diffed (hook `camera_nudge`) |
| BodyFindOverlapType1 80077d18 | `ae_BodyFindOverlapType1` | ported + diffed (hook `body_stub_return_zero`) |
| BodyFindOverlapType3to5 80077ee8 | `ae_BodyFindOverlapType3to5` | ported + diffed |
| BodyCollideBodiesDetailed 80078088 | `ae_BodyCollideBodiesDetailed` | ported + diffed (hooks `collision_ray_test`, `gimmick_type_to_sfx`, `play_sfx_at`, `body_stub_return_zero`) |
| BodyCheckPickupContact 800788b8 | `ae_BodyCheckPickupContact` | ported + diffed (hook `pickup_collect`) |
| BodyContactType 800392e4 | `ae_BodyContactType` | ported + diffed |
| BodyTypeOfActor 80078864 | `ae_BodyTypeOfActor` | ported + diffed (not in the task list; called by Detailed) |
| BodyContactSfx 80078808 | `ae_BodyContactSfx` | ported + diffed (not in the task list; hook `gimmick_type_to_sfx`) |
| VectorNormalS 8003019c (core 80030210) | `ae_VectorNormalS` | ported + diffed (SDK helper) |
| CollisionUpdate 8007247c | `ae_CollisionUpdate` | ported + diffed, including inline player / RC-car cases 1 / 2; the case callees are detailed in [bodymove.md](bodymove.md) |
| CollisionBuildProbePoints 8007304c | `ae_CollisionBuildProbePoints` | ported + diffed in `src/bodies/ae_probe.c` (`check_probe.py`); called directly by CollisionUpdate |
| BodyStub_ReturnZero 80098320 | hook `body_stub_return_zero` | the original returns 0; the port defaults to that and lets a host / test override it |

### Hooks the host must supply (`AeBodyHooks`)

| Hook | Original | Called from |
|---|---|---|
| `reserved_body_move` | retired ABI slot; `CollisionUpdate` does not call it | none |
| `build_probe_points(type, body)` | legacy ABI slot; the core calls `ae_CollisionBuildProbePoints` directly | none |
| `printf_error(fmt, type)` | printf 800a3720, only `" Collision_set Error %d\n"` | CollisionBodyRegister (table full) |
| `sound_loop_start(1, 0x16)` -> handle | SoundLoopStart 800878e0 | CollisionBodyRegister type 2 |
| `sound_loop_stop(0x16, handle)` | SoundLoopStop 8008806c | CollisionBodyUnregister type 2 |
| `collision_ray_test(arg, params, out[2])` | CollisionRayTest 80078c9c; `params` = scratchpad `0x1F800108` (five s32: x, y + radius, z, radius, 2 * radius, already written); the port reads only `out[0]` after a result >= 0 | BodyCollideBodiesDetailed |
| `gimmick_type_to_sfx(colliderType)` | GimmickTypeToSfx 8005bb98 | BodyContactSfx |
| `play_sfx_at(sfx, pos[3])` | PlaySfxAt 80087a20 (the original passes a stack pointer to three s16) | BodyCollideBodiesDetailed |
| `pickup_collect(type)` | PickupCollect 80078a18 | BodyCheckPickupContact |
| `camera_nudge(dx, dy, dz)` | CameraNudge 80047690 | BodyCollideWithBodies (type 1 pushed) |
| `body_stub_return_zero()` | BodyStub_ReturnZero 80098320 | FindOverlapType1, BodyCollideBodiesDetailed |

## Findings: pseudocode vs. instructions

The listing was followed everywhere; these are the places where the Ghidra output of `notes/decomp/obodies.c` is
misleading (found while diffing):

* **`VecLength3` has three arguments.** Ghidra shows `VecLength3(x, 0)` in BodyCollideWithBodies,
  BodyFindOverlapType1 / 3to5 and BodyCollideBodiesDetailed; the call really is `VecLength3(dx, 0, dz)` ($a2 holds
  the z difference). Using 0 for z would be wrong.
* `VectorNormalS` leaves the sum of squares in `$v0` (Ghidra: void). Its core `add`s trap on signed overflow of
  the sum (x^2 + y^2 + z^2 >= 2^31 with the three inputs truncated to s16 as GTE IR); the port flags that case
  instead of wrapping.
* `CollisionBodyClear` does **not** clear `+0x1E`, `+0x28` or `+0x34`; it clears `+0x00..0x17`, `+0x18/1A/1C`,
  `+0x20/22/24/26/2A`, `+0x2C`, `+0x30`. It returns 0. `LoopSlotClearAll` returns -1.
* `GetObjectByIndex(0x100)` is not an error: it reads `+4` of "slot 256", which is the first standalone body
  (`0x800BD1A4`); the u16 test is `< 0x101`. `CollisionBodyIndexOf` compares the sign-extended index as unsigned `< 0x101`.
* `CollisionUpdate`: the contact word of **every** list node is cleared in the delay slot of the loop branch,
  including the body that was just freed / unregistered; the next pointer is read before the node is processed;
  `DAT_800B20CC` is stored as a u16 before dispatch and dispatch uses `(s16)(type - 1) < 17` as an unsigned
  compare (type 0 and types above 0x11 take no case but still get the grid clamp); the grid clamp uses
  `-(w-1)*256 .. (w-1)*256` with the original compare order (so a width of 0 gives odd results, reproduced).
  Cases 6 / 7 / 0xC / 0xD fall through into the 8 / 0xE / 0xF code; cases 6..8, 0xC..0x11 decrement `solid`
  afterwards (`lhu - 1`).
* `BodyCollideBodiesDetailed`: the "type 6 / 7 / 12 / 13 of the other body" branch (radius test setting
  `0xB0000000` on both) can **never run**: the list filter admits only types 1..5 for the other body. The branch
  is translated anyway (the check cannot execute it, see coverage below). `/ 3` for the crouching player is
  `mult 0x55555556` / `mfhi` minus the sign, i.e. C `int16 / 3`. The ray test input words are `(s16)` truncations
  (`y + radius` is a 16-bit sum), `arg = (kind << 16) | (s32)(s16)(body + 0x24)` (the OR of a sign-extended value),
  and `PlaySfxAt` gets a pointer to the first three s16 of that block.
* `BodyFindOverlapType1` and `Detailed` call `BodyStub_ReturnZero` (the only reason the hook exists); the non-zero
  branches are dead in the shipped game, but are ported and exercised through the hook.
* `CollisionBodyUnregister` for a body that is in no slot returns -1; for type 2 it clears `DAT_800F5A78` to 0xFFFF
  whenever the slot type is 2, independent of the loop flag.

## Validation

`.venv/Scripts/python.exe tools/mips/check_bodies.py` (about 30 s; `--cases 100` runs the full case counts in about two minutes,
`--coverage` prints the executed-instruction coverage of the original code, `--trapv` builds the port with `-ftrapv`):

1. `tools/mips/mips_ram.py` is a reusable MIPS-I harness: it loads the EXE (+ optional area member) into a RAM image
   at the real addresses, runs an original function by address with arguments, and intercepts calls (`hooks`, by
   entry address) or mid-function addresses (`patches`) with Python callbacks. Load / branch delay slots are exact
   (including lwl / lwr merging with an in-flight load), div by zero and INT_MIN / -1 follow the hardware,
   add / sub / addi overflow raises. COP2 runs on the pinned PCSX-ReARMed GTE of `tools/codex/gte_oracle.py`
   (`attach_gte_oracle`); `.venv/Scripts/python.exe tools/mips/mips_ram.py <hex addr> <size>` prints an instruction listing.
   `tools/codex/mips_ground_reference.py` is untouched.
2. `check_bodies.py` builds scenarios in RAM (consistent body lists with 0..14 bodies of every type, pool bodies and
   owner-embedded bodies, the player / RC car as owners, full tables, outliers beyond the grid, random contact words,
   random level sizes, colliders, loop-slot states), copies the image to the C `AeRam`, runs the original code and
   the port with the **same deterministic hook model**, and requires: identical return value, identical main RAM
   (excluding the top-of-RAM stack window `0x801FF000+`), identical scratchpad and an identical sequence of hook
   calls with arguments. It also checks full player and RC-car `CollisionUpdate` passes with real area data. The
   standalone movement functions and the two inline instruction ranges receive deeper differential and coverage
   checks in `check_bodymove.py`; see [bodymove.md](bodymove.md).
3. Fail-closed: for a share of the cases the port runs with an all-NULL hook table and must flag "not provided"
   exactly when the original run called a hook.
4. Faults: when the original faults (unmapped address from a wild index argument) the port must flag its error flag.

### Case counts (last run)

`.venv/Scripts/python.exe tools/mips/check_bodies.py` (default, `--cases 40`): **6313 cases, 1451 hook calls compared, 4.8 million
original MIPS instructions executed, 32 s, 0 mismatches.** The same checker at `--cases 100` (111 s): 15757 cases,
0 mismatches. Default case counts per function:

| Function | Default | Function | Default |
|---|---|---|---|
| CollisionBodiesReset | 40 | BodyCollideWithBodies | 640 |
| CollisionBodyRegister | 200 | BodyFindOverlapType1 | 380 |
| CollisionBodyAllocSlot | 40 | BodyFindOverlapType3to5 | 380 |
| CollisionBodyUnregister | 240 | BodyCollideBodiesDetailed | 800 |
| GetObjectByIndex / CollisionBodyIndexOf | 100 / 100 | BodyCheckPickupContact | 380 |
| CollisionBodyAlloc / Clear / Free | 100 each | BodyContactType / TypeOfActor / ContactSfx | 200 / 120 / 120 |
| LoopSlotAdd / Ref / Sweep / ClearAll | 120 each | CollisionBodyGetCenter | 200 |
| CollisionUpdate | 360 | VectorNormalS / VecLength3 (direct) | 600 / 600 |
| fail-closed NULL-hook cases (extra, on top of the above) | 334 | AeRam self-test | 17 probes |

(for the five body-vs-body functions the totals include 200 boundary-sweep cases each; the rest are random worlds)

### Instruction coverage of the original code

`--coverage` records every executed original instruction of the ported functions. At the default scale **every
instruction is executed at least once except 19 instructions of BodyCollideBodiesDetailed
(`0x800782EC..0x80078334`) that cannot execute**: the list filter lets only types 1..5 through as the other
body, and that block handles types 6 / 7 / 12 / 13 of the other body (see Findings). The type 1 / 2 inline
`CollisionUpdate` ranges are fully covered by `check_bodymove.py`; see [bodymove.md](bodymove.md).
`VecLength3` and the `VectorNormalS` core are covered 19/19 and 48/48.

### Does the checker see mistakes? (mutation testing)

The checker itself was tested by compiling deliberately wrong copies of `ae_bodies.c` (`--bodies-src`, never the
repo file) and requiring a mismatch: 43 mutations (off-by-one on every `<` / `<=` comparison of the overlap
tests, `half_abs` vs `half`, sign extension of slot indices and `lh` vs `lhu` arguments, wrong constants in contact
words, the `/ 3` of the crouching player, tail pointer, probe-flag values, collider / grid fields, owner checks, ...):
42 were caught; the remaining one is equivalent to the original (`(31 - v1) >> 1` vs `(30 - v1) >> 1` for even `v1`). Random scenarios alone
missed the boundary mutations, which is why the checker also sweeps two bodies across every threshold (+-2) of
each overlap test (`boundary_world`). `--trapv` builds the port with `-ftrapv` (signed overflow aborts) and passes the
same scenarios.

