# Player and RC-car body movement

The player and RC-car arms of `CollisionUpdate` at `8007247C` now execute as structured C on `AeRam`. The two cases
are inline in the original function; `ae_CollisionUpdate` calls the original callees directly. The old
`reserved_body_move` hook slot remains in `AeBodyHooks` for layout compatibility, but is not called.

## Call path

```text
CollisionUpdate type 1 (player)
  snapshot body; handle player modes 2 / 5 / 6
  PlayerMoveRumbleSoundUpdate
  BodyProbeGround
  CollisionStub_ReturnZero
  CollisionBuildProbePoints
  CollisionResolveProbes
  BodyLandOnGround
  PlayerTryLedgeGrab
  BodyRecordColliderContact

CollisionUpdate type 2 (RC car)
  snapshot body
  PlayerVehicleSoundUpdate
  BodyProbeGround
  CollisionStub_ReturnZero
  CollisionBuildProbePoints
  CollisionResolveProbes
  BodyLandOnGround
  BodyRecordColliderContact

CollisionResolveProbes -> CollProbe_* -> PointInBoxXZ / HalfPipeRangeTest /
  DirIndexFromFlags / PlayerSetClimbTarget / PlayerSetRope as needed

BodyRecordColliderContact -> collider-shape tests -> ColliderSlopeContactDir
PlayerVehicleSoundUpdate -> SoundLoopStart / SoundLoopStop / SoundEmitterPan / SoundEmitterPitch
PlayerMoveRumbleSoundUpdate -> SoundLoopStart / SoundLoopStop / PadRumble
```

The 12 functions in the assigned table are in [ae_bodymove.c](../../src/bodies/ae_bodymove.c) and
[ae_bodymove.h](../../src/bodies/ae_bodymove.h). `CollisionBuildProbePoints` and the other in-repository collision,
world, player, effects and sound callees were already ported and are called directly. Named hooks remain at the
native-system and unported-callee boundaries listed below. The checker also runs complete player and RC-car
`CollisionUpdate` passes with real area members 05B, 05E, 064 and 066 (areas 1, 2, 4 and 5).

## Port and differential results

`tools/mips/check_bodymove.py` runs each original function from the EXE and its C counterpart over identical RAM,
scratchpad and deterministic hook models. Its default run uses 12 cases per listed function and 8 player plus 8 RC-car
inline `CollisionUpdate` cases. The `--coverage` run uses 28 cases per function.

| Address | Original function | C function | Default cases | Original instructions covered |
|---|---|---|---:|---:|
| `8004F884` | ColliderSlopeContactDir | `ae_ColliderSlopeContactDir` | 12 | 64 / 64 |
| `80063EC4` | PlayerTryLedgeGrab | `ae_PlayerTryLedgeGrab` | 12 | 209 / 209 |
| `80072A14` | PlayerVehicleSoundUpdate | `ae_PlayerVehicleSoundUpdate` | 12 | 117 / 117 |
| `80072BE8` | PlayerMoveRumbleSoundUpdate | `ae_PlayerMoveRumbleSoundUpdate` | 12 | 80 / 80 |
| `80072DC4` | CollisionStub_ReturnZero | `ae_CollisionStub_ReturnZero` | 12 | 2 / 2 |
| `80072DCC` | BodyLandOnGround | `ae_BodyLandOnGround` | 12 | 160 / 160 |
| `80073450` | CollisionResolveProbes | `ae_CollisionResolveProbes` | 12 | 363 / 363 |
| `800739FC` | SignedRangeOverlap | `ae_SignedRangeOverlap` | 12 | 41 / 41 |
| `800792C8` | BodyRecordColliderContact | `ae_BodyRecordColliderContact` | 12 | 332 / 332 |
| `80087B34` | SoundEmitterPan | `ae_SoundEmitterPan` | 12 | 28 / 28 |
| `80087C28` | SoundEmitterPitch | `ae_SoundEmitterPitch` | 12 | 15 / 15 |
| `8008806C` | SoundLoopStop | `ae_SoundLoopStop` | 12 | 25 / 25 |
| `80072638` | CollisionUpdate player inline case | `ae_CollisionUpdate` | 8 | 64 / 64 |
| `80072738` | CollisionUpdate RC inline case | `ae_CollisionUpdate` | 8 | 40 / 40 |

Coverage totals **1540 / 1540 instructions** across the 12 functions and both inline ranges. There are no uncovered
instructions at the extended coverage scale or unreachable ranges to exclude. The 12-case default result (and the
separate `check_bodies.py` default report) does show branch-level coverage notices for `PlayerVehicleSoundUpdate`,
`CollisionResolveProbes`, `SoundEmitterPan` and parts of inline `CollisionUpdate`; the `--coverage` seed set executes
those paths. The standalone functions have 12 cases each in the default checker results and ledger; `CollisionUpdate`
has 16 cases in this checker (the separate `check_bodies.py` integration suite also runs one player and one RC pass
in each of the four real areas).

The mutation suite compiles 24 deliberately wrong function copies, two for each listed function. All 24 must produce
a differential mismatch; the passing run reports `mutation results: 24/24 caught`.

## Remaining named boundaries

These callees stay hooks because they are outside the assigned table and are native-system operations or unported game
functions. The normal host wiring is in `src/game/ae_game_hooks.c` and `src/game/ae_game.c`.

| Original hook | Purpose in this path | Arguments / boundary |
|---|---|---|
| `SfxVoiceSetPitch` `800A35FC` | Writes a pitch to an SPU voice; native sound hardware boundary. | `voice, pitch` |
| `XaPlay` `80011F6C` | Starts XA playback through the CD manager; native CD / XA boundary. | XA table index |
| `GetSolidHeight` `8004D8C0` | Queries the solid grid before `GetGroundHeight` chooses the final floor height. | `x, min(requested y, terrain y), z`. |
| `PointInBoxXZ` `80073B64` | Inclusive XZ point-in-collider bounds check in box probes. | Position pointer, shape pointer; the original does not use incoming `$a2/$a3`. |
| `HalfPipeRangeTest` `80050490` | Tests vertical overlap between a probe and the shape range. | `y, body height, lower/top, extent/height` as supplied by each probe. |
| `DirIndexFromFlags` `8007774C` | Maps a triangle collider's orientation flags to its direction index. | Collision type in `$a0`; unused original argument words are preserved. |
| `PlayerSetClimbTarget` `80066E28` | Attaches the player to a climbable radial collider. | Original six o32 words, including stack arguments. |
| `PlayerSetRope` `80066E8C` | Binds the player to a rope segment. | Original `$a0..$a3` words. |
| `Printf` `800A3720` | Collision-probe diagnostic output. | Original format pointer and values. |
| `PadGetState` `8002B52C` | Reads controller state before `PadRumble` updates rumble bytes in RAM. | Controller port. |
| SPU callbacks in `AeSoundHw` | Wait / key-off / ADSR operations used by the already-ported sound-loop functions. | `SpuWaitIdle`, `SpuWaitLoop`, voice ADSR, key-off and volume writes. |

`ColliderSlopeContactDir` is no longer an open boundary: collision-probe wiring routes it to the new direct port. The
same applies to `CollisionBuildProbePoints`; the old callback members are retained only for ABI layout and are not
used by the core's `CollisionUpdate` path.

## Findings from the listing and differential checks

* `PlayerTryLedgeGrab` does not produce a meaningful `$v0`; the caller ignores it. Its C API is `void`, and the checker
  compares all RAM, scratchpad and hook effects rather than an undefined return register.
* `CollisionResolveProbes` chooses its result from the vertical output on one path and the contact marker on another.
  The port was corrected after the listing showed that using the vertical output for both paths was wrong.
* `BodyRecordColliderContact` reads its query point through global pointer `0x800B20B4` for the `0x4000` and `0x2000`
  geometry branches. The `0x3000` branch instead uses scratch address `0x1F8000FC`.
* `PlayerMoveRumbleSoundUpdate` still stops an active rumble loop and clears strength and timer when the player mode is
  not 1; that branch is not a no-op.
* The original `CollisionRayTest` is already ported. A proven wiring bug in `src/world/ae_misc.c` passed the hook
  table itself as the nested probe callbacks' `user` pointer; it now passes `h->user`. Real-area ledge-grab scenarios
  exposed the invalid pointer and the corrected adapter matches the original.

## Ledger and exclusions

The 12 assigned functions are `diffed` in `tools/verified.csv` at 12 default cases each. The checker exclusions are
limited to the original stack window, the unused caller-register residue of `PointInBoxXZ`, and one never-initialized
particle padding byte copied from caller stack. That particle byte is seeded on the MIPS side and provided explicitly
to the C spawn hook; all other RAM and scratchpad bytes are compared.
