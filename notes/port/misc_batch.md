# Miscellaneous frame helpers and collider scripts

Batch 113 ports the gameplay functions from `extract/codex-task/task.md` into
`src/world/ae_misc_batch.c`. It adds 19 direct C ports. `ScreenBandDraw` remains
a named renderer hook because its work is GPU packet construction. The three
unnamed collider-table targets are bounded from their return instructions and
named by behavior: `ColliderScriptFollowPath` (812 bytes),
`ColliderScriptPlayerMove` (1,132 bytes), and `ColliderScriptCarry` (328 bytes).
`PadRumbleReset` was also ported: it clears one of the two game-owned rumble
state bytes and performs no device I/O.

## Call paths

* `VramScrollUpdate` walks the active area's 16-byte animation records, updates
  the destination and area-link words, and submits rectangles through the PsyQ
  `SetDrawMove` boundary.
* `AreaFeature30Update` drives the stage fade and overlay-load state machine.
  It calls the existing `PlayerSetMode`, `ArcGetMemberPos`, and
  `CdMgrQueueCommand` ports; CD and renderer operations remain hooks.
* `BossUpdate` uses `BossKindForArea` and dispatches to the area's overlay entry.
* `ColliderScriptsUpdate` reaches the three newly named movement table entries.
  `ColliderScriptRun` reaches `ColliderScriptCommand`, whose jump table handles
  C, M, S, T, and Z. The command and movement functions call the existing
  collider, gimmick, zone, player, camera, and ground-height ports directly.

## Port status and validation

The default `.venv/Scripts/python.exe tools/mips/check_misc.py --coverage` run
passes 297 cases in about 7.5 seconds. It compares the original instructions
against C on real area members and checks RAM, scratchpad, return values, and
ordered hook calls. The `min_cases` values below match that run.

| Function | Cases | Original instructions covered |
|---|---:|---:|
| `PadRumbleReset` | 3 | 11/11 |
| `GetZoneMask` | 4 | 52/52 |
| `CameraRcCarActive` | 4 | 43/43 |
| `StageExitPending` | 2 | 4/4 |
| `BossUpdate` | 6 | 48/48 |
| `VramScrollUpdate` | 8 | 182/192; 10 declared unreachable |
| `PlayerIsSubmerged` | 3 | 13/13 |
| `Indicator_Register` | 3 | 50/50 |
| `MenuItemSetRange` | 1 | 3/3 |
| `MenuItemSetStep` | 1 | 2/2 |
| `HudFindGadgetIconPos` | 2 | 27/27 |
| `ColorFadeReset` | 1 | 12/12 |
| `ProgressFlagGet` | 2 | 13/13 |
| `AreaFeature30Update` | 5 | 115/115 |
| `ColliderScriptFollowPath` | 4 | 191/203; 12 declared unreachable |
| `ColliderScriptPlayerMove` | 11 | 283/283 |
| `ColliderScriptCarry` | 3 | 82/82 |
| `ColliderScriptCommand` | 19 | 261/269; 8 declared unreachable |
| `ColliderResetToStart` | 2 | 38/38 |

The remaining instructions are registered in `tools/mips/check_exclusions.csv`:

* The VRAM type-zero dispatch cannot run because that same zero byte terminates
  the list. Valid active type-1 and type-2 rows have nonzero divisors. The
  signed-overflow checks also require a divisor of -1, while the divisor is an
  unsigned byte.
* A route's interpolation divisor is twice its positive duration, so its
  divide-by-zero and `INT32_MIN / -1` trap paths do not occur on valid routes.
* The Z-command interpolation divisor is half of a valid duration greater than
  one, so its divide-by-zero and signed-overflow trap paths do not occur.

`--mutate` catches 108/108 RAM-marker mutations (two for every one of the 54
configured checker functions) and three independent mutations to the six-word
`PlayerSetClimbTarget` callback contract. No mutation survived.

`tools/check_verified.py` exits successfully. It reports warnings that the three
new table functions are not yet in generated `notes/inventory/functions.csv`;
their names and bounds are recorded in `tools/symbols.csv` and this note. The
task forbids opening Ghidra, so the generated inventory was not edited by hand.

## Findings

* `GetZoneMask`'s negative-Y branch negates the X delta instead of the Y delta.
  The port preserves this observable behavior.
* Type-2 VRAM scroll uses signed `frame * speed`, adds three before an
  arithmetic shift when negative, divides by the entry's height byte, and
  reverses the remainder when the phase byte is zero. It can submit two moves
  for one record. Type 1 is gated by a zero active byte and submits only when
  `(frame + entry[14]) % entry[15] == 0`.
* `CameraRcCarActive` reads the current-area tag at `0x800EC2D2`; the collider
  player-move area-29 check uses the separate area ID at `0x800F4476`.
* Collider route speed (`+0x0C`) and command animation speed (`+0x1C`) are
  separate fields. A completed route keeps the incremented swing timer when
  its next segment has positive duration, and resets it at a zero-duration
  terminator.
* In Z commands, geometry `0x0600` or a zero command radius uses the raw travel
  speed. Geometry `0x0300` uses a two-part interpolation around half duration;
  other geometries also use the raw travel speed.

## Hooks left at native boundaries

| Hook | Original work behind the boundary |
|---|---|
| `ScreenBandDraw` (`8009B9C8`) | Builds the translucent screen-band GPU packets used by the stage fade. |
| `SetDrawMove` (`8002B380`) | PsyQ renderer operation that builds the VRAM `DR_MOVE` command. |
| `CdIntToPos`, `CdControl`, `CdReadyCallback`, `VSync` | PsyQ CD and frame-synchronization operations used by the archive queue. |
| `exit` (`800A3700`) | Process termination on the full-indicator-table error path. |
| `GetSolidHeight` (`8004D8C0`) | Queries collision shapes and updates collision/surface state beneath the already-ported `GetGroundHeight`. |
| `GetColliderQuadrantMaskXZ` (`800889B4`) | Computes the collider visibility quadrant from X/Z signs. |
| `FUN_801402d8` / `Area53_BossUpdate` / `FUN_80143854` / `Area46_BossUpdate` / `Area57_BossUpdate` | Per-area boss frame controllers in separately loaded overlays. |
| `FUN_80137248` | Area overlay callback invoked when the stage feature finishes loading its member. |
| `FUN_8013fc18` | Area overlay player-motion callback used by the 0x800 collider mode and area 0x1D. |

No game-host wiring was changed. The new C declarations and direct collider-table
dispatch are in the world module; renderer, CD, and overlay callbacks retain
their original names and arguments.
