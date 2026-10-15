# World queries and events on AeRam

`src/world/ae_world.{h,c}` ports the eleven functions requested in
`extract/codex-task/world_task.md`, plus the leaf setter called by GimmickEvent.
All twelve are **ported and diffed**. Link `src/core/ae_ram.c` and
`src/world/ae_world.c`; no monkey, player, body or viewer source was changed.
Hosts still need to connect the monkey hooks to this module and supply the
world hooks. This task does not connect the native viewer.

## Functions and direct callees

Counts are from the default checker run (seed 94423, scale 1).

| Original | Address | Cases | Instructions executed | Direct callees |
|---|---|---:|---:|---|
| ClearWords | 80032708 | 9 | 9/9 | none |
| StageExitIsActive | 8005DCE4 | 256 | 13/13 | none |
| TargetListAdd | 8007CD88 | 33 | 15/15 | none |
| TargetListRemove | 8007CDC4 | 3 | 14/14 | none |
| ObjSlotGetAnimIndex | 80052C4C | 40 | 21/21 | none |
| ObjSlotSetField_CF | 80052CA0 | 40 | 19/19 | none |
| ObjSlotGetTarget | 80052D04 | 44 | 26/26 | none |
| GimmickGetInfo | 8005B7BC | 60 | 49/49 | none |
| GimmickEvent | 8005B880 | 714 | 183/183 | GimmickActivate, PlaySfxAt, MeshInitFromAreaTable, FUN_8013d358, CameraShake, SetDAT_800E77E2, EventCamStart |
| GetGroundHeightEx | 800433B0 | 453 | 20/20 | GetGroundHeight (direct port) |
| GetGroundHeight | 80043400 | 459 | 316/318 | GetSolidHeight |
| SetDAT_800E77E2 | 80052CEC | 6 | 3/3 | none; direct port used by GimmickEvent |

## Hooks left open

Every hook has the original function's name (the overlay function retains its
inventory name because symbols.csv has no name for it). The callbacks receive
`user`, `AeRam *`, and four argument words; only the original's actual arguments
are meaningful. Addresses passed to callbacks are PSX RAM addresses, not host
pointers. NULL sets the sticky RAM error and stops dependent work. Check
`ae_ram_failed()` after calls.

| Hook | Original | Arguments | Original work behind the boundary |
|---|---|---|---|
| GetSolidHeight | 8004D8C0 | x, min(y, terrain height), z | Queries stacked collision shapes and collider boxes; updates contact/surface/slope scratchpad. Calls CollProbeCounterInc, an indirect shape handler, GetColliderQuadrantMaskXZ, ColliderHeightAt, GetSurfaceType. The 1,656-byte function and its shape dispatch remain in the collision system. |
| PlaySfxAt | 80087A20 | sound ID, collider position address | Positional sound playback. GimmickEvent uses 0x75, 0xB0, 0xAC or 0xAF depending on event/type/area. |
| GimmickActivate | 8008FC30 | signed linked index, source gimmick address | Recursive type-specific activation, doors, water and sounds; 1,536 bytes with recursion, progress and overlay calls. |
| MeshInitFromAreaTable | 80015CC8 | prop instance address, signed mesh index | Resolves the area's mesh-table entry then calls MeshInit; rendering-owned wrapper. |
| FUN_8013d358 | ovl_197::8013D358 | signed collider x, y, z | Used only in area 0x29: forms position (x, y-128, z), calls EffectSpawnFromDef with definition 0x80141B54, arguments 1/4 and vector (0,0,-4096), then SpawnParticle type 0x112 at that position. Confirmed from member 197.bin instructions; effect system remains hooked. |
| CameraShake | 80048A00 | level (here 0) | Starts camera shake, honors existing shake priority, advances the game's 16-bit random seed three times and writes camera offsets. Camera-owned leaf, kept behind the camera boundary. |
| EventCamStart | 8004D39C | event camera ID (here 0) | Validates ID, starts camera record, clears aim mode and updates camera/exit state. Calls Printf, the camera record initializer and a player-state helper. |

## Validation

Run `.venv/Scripts/python.exe tools/mips/check_world.py --coverage`.
The checker compiles the C module with warnings as errors, then compares original
EXE instructions with C on identical complete RAM images. It compares defined
return values, all main RAM below the registered stack window, all 1 KiB of
scratchpad and ordered hook calls (including positional sound vector contents).
Hooks have deterministic RAM side effects and random return words; GetSolidHeight
also changes scratchpad surface and slopes, exercising both contact selection
paths. Each required callback is independently removed on paths that call it,
checking fail-closed behavior. Before each first hook call, the original run
records RAM, scratchpad and the preceding hook log. Missing-hook runs must match
that checkpoint, including the wrapper's unconditional output writes, so merely
setting the error flag while continuing dependent work is also rejected.

The default run passes **2,117 cases**, under **one minute** on this machine,
with **688/690 instructions executed**, covering **every reachable instruction**.
The baseline includes the real EXE and Fossil Field area 1, member 05B.bin.
Real grid pointers, heights and cells are fixed up as AreaLoad does, as are its
three gimmicks, group count and collider table. Randomized and edge scenarios
cover all event dispatches, signed/empty object slots, target duplicates/full list,
all byte exit codes, signed group counts, no-heightfield and out-of-grid paths,
all terrain selector classes, diagonals, full-range s16 heights, negative rounding
and 32-bit coordinate wrap. Two wide-grid boundary scenarios per ground function
exercise the signed x-index shift with a width of 65535, including the transition
from index 32767 to a negative signed index. The original alternate-seed run with
`--trapv` also passed before this checker extension.

`--mutate` catches **61/61 deliberately wrong variants**, with **zero survivors**,
covering all twelve functions, all twelve GimmickEvent dispatch destinations,
all four terrain regions, bounds, signed casts/shifts, division toward zero,
swapped operands, scratchpad writes, solid-height ties, every hook's arguments,
and skipped fail-closed checks. Mutations are confined to the named function;
repeated instances of a wrong rule within it change together. Each variant runs
that function's scenarios (and the direct wrapper for GetGroundHeight), fails on
the first mismatch, and cannot overwrite default reports. Compilation errors or
harness errors do not count as caught mutations.

The initial unsigned x-shift mutation survived the existing narrow grids. The
new wide-grid scenarios catch it because the wrong copy calls GetSolidHeight
where the original returns immediately. No mutation was dropped and no exclusion
was added. Ledger minima were raised to 459 for GetGroundHeight and 453 for its
wrapper; all other counts remain unchanged. Production C was not changed.
The checker fails on undeclared coverage
gaps and writes default-run results through exclusion_registry; nondefault,
overflow-trapping and mutation runs do not overwrite the default report.
`tools/check_verified.py` now includes world sources in its mandatory scan.

Exactly three exclusions were appended for owner review:

* Stack window 801FF000..80200000: original call-frame writes differ from the
  host C stack. No other RAM masks exist.
* Shared stale slope halfwords: when AreaHeader bit 1 disables terrain, the
  original skips initializing two stack locals. If solid height exceeds 16384,
  they are copied into scratchpad. `AeWorldHooks.ground_stale_x/z` explicitly
  supply those values. The checker seeds the original locals at the correct
  direct/wrapped frame offsets and compares their resulting scratchpad writes.
* GetGroundHeight 800435FC..80043600: impossible default jump and its delay slot.
  The preceding unsigned comparison restricts the selector to 0..2, and separate
  branches exhaust all three values. There are no other coverage gaps.

The hooked systems themselves are outside this differential proof. Their callbacks
must reproduce all original side effects when integrated into the native port.

## Instruction findings

* ClearWords takes a **signed word count**, not a byte count. Zero and negative
  counts do nothing. It, TargetListRemove and the byte setters have no defined
  return value; the C declarations use void.
* TargetListRemove clears **every** duplicate, rather than just the first match.
  TargetListAdd accepts zero, returning the first free index while leaving it free.
* Object slot indices are signed bytes; only -1 is the empty sentinel. The two
  animation/field helpers narrow the slot argument to s16, whereas
  ObjSlotGetTarget retains all 32 bits. Its target output sign-extends three s16s.
* GimmickGetInfo sums signed group counts, narrows its index to s16 and checks
  only the upper bound. Negative indices therefore read preceding records;
  validation intentionally tests backed negative indices, rather than adding a
  lower-bound check absent from the instructions. It writes three halfwords.
* GimmickEvent checks a signed 32-bit index only for negativity, before computing
  the record and collider addresses. Event 8 unconditionally writes state 1;
  event 9 maps arguments 0..3 to states 1..4. State/type distinctions and call
  order come from the actual jump table, including the special type 0x2B close.
* Terrain variant bits are cell bits **12..14**, combined with bit 11 and the
  local-coordinate quadrant. This agrees with notes/67 and corrects the earlier
  bits-8..10 description in notes/12 and the LevelGrid struct comment.
* Out-of-grid terrain returns zero immediately without calling GetSolidHeight.
  Unsupported split selectors give terrain 16384 and zero slopes. With terrain
  disabled, the stale-slope case above is preserved explicitly.
* Interpolation uses signed halfword differences and division toward zero, then
  narrows the height to s16. GetSolidHeight's return is also narrowed to s16.
  A tie selects solid metadata; terrain wins only when strictly lower. Clearing
  surface bits 0..4 compares the original full-width y with the selected height.

No decompiler output was available in this worktree; all decisions were checked
against mips_ram listings, symbols, structs, inventory calls and existing notes.
Listings are saved locally in extract/codex-task/world_disassembly.txt and are
not committed. No game data, third-party tools or generated disassembly is committed.
