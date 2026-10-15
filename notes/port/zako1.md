# Zako per-frame update batch

This batch ports all 36 functions in `extract/codex-task/task.md` to
`src/world/ae_zako1.c` with declarations and named RAM fields in
`src/world/ae_zako1.h`. The port follows the `ZakoUpdateAll` frame path, and
keeps native renderer and platform work behind named hooks.

## Call tree

```text
ZakoUpdateAll
|-- ProjectilesUpdateDraw [hook]
|-- ZakoUpdateLoop (unless frame-state bit 2 is set)
|   |-- AreaModeUpdate [indirect hook via 800AF574]
|   |-- ObjSyncBodyHitFlags
|   |-- ObjUpdateMotionState
|   |   |-- ObjSwapToReactAnim
|   |   |   |-- ObjIsOwnerType0B
|   |   |   |-- ZakoOnHitByGadget
|   |   |   |   |-- GetObjectByIndex [ported body helper]
|   |   |   |   |-- ZakoApplyHit -> ZakoSetKnockback
|   |   |   |   |-- ObjApplyDamage
|   |   |   |   `-- ObjUnregisterBody [ported init/body helper]
|   |   |   `-- ObjDeathSfx -> ObjPuffAt / ObjPlaySfx
|   |   |-- ObjApplyVelocity -> ObjGroundHeightCached -> GetSolidHeight [world hook on cache miss]
|   |   |-- ObjAnimClampLastFrame
|   |   |-- ObjBounceOnGround
|   |   `-- ObjKnockbackDecay
|   |-- ObjResetMotion
|   |   |-- ObjReenableBody [ported body and target-list helpers]
|   |   |-- ObjDeathSfx
|   |   `-- ZakoDefeat
|   |       |-- FUN_800A3740 [BIOS memset hook]
|   |       |-- ObjKill [ported init helper]
|   |       |-- SpawnParticle [ported effects helper]
|   |       |-- ObjPlaySfx
|   |       `-- ZakoDropPickups -> SpawnPickups [hook]
|   |-- FUN_8013F660 [overlay handler hook for type 0x3F]
|   |-- ZakoTypeUpdate [indirect hook via 800AF350]
|   |-- ZakoHooksRun -> registered callbacks [indirect hook]
|   `-- ZakoDebugMenuDraw
|       |-- ColorFadeSet / DebugTextPrintfWhite [hooks]
|       |-- FUN_8002FB00 / FUN_8002FB30 [projection setup hooks]
|       `-- ZakoDebugDrawPrimitive [native renderer boundary]
|-- Draw_Nop [hook]
|-- ZakoCullAndDraw
|   |-- PointOffscreen
|   |   |-- FUN_8002FB00 / FUN_8002FB30 [projection setup hooks]
|   |   `-- FUN_80031330 [projection hook]
|   |-- ZakoDraw
|   |   |-- ObjGroundHeightCached / DrawShadow [hook]
|   |   |-- ZakoBuildMatrix / ModelPoseBones [ported SDK / pose helpers]
|   |   |-- ZakoUpdateLinkPoints -> ZakoGetBonePoint [ported ApplyMatrixSV]
|   |   |-- ZakoUpdateColliderPos [Atan2Safe hook, ported sin/cos]
|   |   |-- ModelTransformSkinned [native renderer hook]
|   |   |-- ModelPrepCycleColors / ModelEmitPrimsCycleColors [renderer hooks]
|   |   `-- ModelLightColorsToScratch -> ModelEmitPrims [renderer hooks]
|   `-- ZakoPoseHidden
|       |-- ZakoBuildMatrix / ModelPoseBones
|       |-- ZakoUpdateLinkPoints
|       `-- ZakoUpdateColliderPos
`-- ZakoSpawnNearby -> ZakoAllocSlot -> ZakoSpawnFromEntry [ported init helper]
```

## Function status and coverage

All functions are ported and `diffed`. Cases are from the default checker run
across real area members 1, 2, 3, and 6. Coverage is instruction coverage of the
original function; the three percentages below are lower only because the
registered instructions are native renderer blocks or defensive divide traps.

| Address | Function | Status | Cases | Coverage |
|---|---|---:|---:|---:|
| 80050B04 | ZakoSpawnNearby | ported + diffed | 2 | 100% |
| 80050C34 | ZakoUpdateAll | ported + diffed | 1 | 100% |
| 80050CB0 | ZakoAllocSlot | ported + diffed | 3 | 100% |
| 800511A4 | ZakoUpdateLoop | ported + diffed | 11 | 100% |
| 80051410 | PointOffscreen | ported + diffed | 5 | 100% |
| 80051534 | ZakoCullAndDraw | ported + diffed | 3 | 100% |
| 80051620 | ZakoGetBonePoint | ported + diffed | 2 | 100% |
| 800516D8 | ZakoUpdateLinkPoints | ported + diffed | 1 | 100% |
| 80051740 | ZakoUpdateColliderPos | ported + diffed | 3 | 100% |
| 800518AC | ZakoBuildMatrix | ported + diffed | 7 | 100% |
| 80051A28 | ZakoPoseHidden | ported + diffed | 3 | 100% |
| 80051AA4 | ModelLightColorsToScratch | ported + diffed | 2 | 41.38%* |
| 80051C00 | ZakoDraw | ported + diffed | 5 | 100% |
| 80051DCC | ObjReenableBody | ported + diffed | 3 | 100% |
| 80051F1C | ZakoDefeat | ported + diffed | 2 | 100% |
| 8005216C | ZakoDropPickups | ported + diffed | 2 | 100% |
| 800522A8 | ZakoSetKnockback | ported + diffed | 3 | 100% |
| 8005240C | ZakoApplyHit | ported + diffed | 6 | 100% |
| 800525A0 | ObjApplyDamage | ported + diffed | 3 | 100% |
| 800525E0 | ZakoOnHitByGadget | ported + diffed | 2 | 100% |
| 80052640 | ObjIsOwnerType0B | ported + diffed | 2 | 100% |
| 80052658 | ObjAnimClampLastFrame | ported + diffed | 2 | 100% |
| 80052688 | ObjBounceOnGround | ported + diffed | 4 | 100% |
| 8005278C | ObjApplyVelocity | ported + diffed | 1 | 100% |
| 80052800 | ObjKnockbackDecay | ported + diffed | 1 | 100% |
| 80052864 | ObjDeathSfx | ported + diffed | 3 | 100% |
| 800528E0 | ObjResetMotion | ported + diffed | 5 | 100% |
| 800529A0 | ObjSwapToReactAnim | ported + diffed | 2 | 100% |
| 80052A24 | ObjUpdateMotionState | ported + diffed | 11 | 100% |
| 80052BC8 | ObjSyncBodyHitFlags | ported + diffed | 3 | 100% |
| 80055404 | ObjPlaySfx | ported + diffed | 1 | 100% |
| 8005685C | ObjTurnTowardsXZ | ported + diffed | 5 | 94.12%* |
| 80056C58 | ObjGroundHeightCached | ported + diffed | 2 | 100% |
| 800573A0 | ObjPuffAt | ported + diffed | 1 | 100% |
| 8005760C | ZakoHooksRun | ported + diffed | 1 | 100% |
| 80058954 | ZakoDebugMenuDraw | ported + diffed | 3 | 78.03%* |

`*` Registered coverage proof: `ModelLightColorsToScratch` leaves 51 GTE color
vector instructions to the renderer; `ObjTurnTowardsXZ` leaves four DIV trap
instructions unreachable for the positive step and bounded angular difference;
`ZakoDebugMenuDraw` leaves 105 projection/packet-generation instructions to the
native renderer. There are no other uncovered instructions.

The checker compared main RAM below the declared stack window, the complete
scratchpad, return values, hook order and consumed hook arguments. The OT packet
buffer (80186000:4096) is masked as renderer output. It used two deterministic
stack/data exceptions: the host stack window, and the nested SpawnPuff padding
halfword observed as 0x8000 by the MIPS oracle. `--mutate` inserts two
selector-gated wrong writes per function; all 72 mutations were caught.

## Open hooks

These are calls outside the 36-function table or platform/renderer boundaries;
all other calls in the call tree go directly to ported functions.

| Hook | Address / boundary | Original responsibility |
|---|---|---|
| ProjectilesUpdateDraw | 80059B08 | Update and draw projectiles before the enemy pass. |
| Draw_Nop | 8005E000 | Stage draw-phase callback after the object update. |
| FUN_8002FB00, FUN_8002FB30 | 8002FB00, 8002FB30 | Configure projection state. |
| FUN_80031330 | 80031330 | Project a vector and return depth/screen results. |
| Atan2Safe | 80046340 | Convert an X/Z direction to a safe 12-bit angle. |
| FUN_800A3740 | 800A3740 | BIOS memset wrapper used for local scratch records. |
| SpawnPickups | 80083AA8 | Spawn one or more configured pickup items. |
| FUN_8013F660 | 8013F660 | Handle the overlay-specific enemy type 0x3F. |
| AreaModeUpdate | indirect via 800AF574 | Run the current area-mode object callback. |
| ZakoTypeUpdate | indirect via 800AF350 | Run the enemy-type update callback. |
| ZakoFrameHook | indirect via 800B8D20 | Run registered per-frame callbacks. |
| DrawShadow | 8001F604 | Draw the object shadow at its projected ground position. |
| ModelTransformSkinned | 80016A60 | Transform a skinned model for rendering. |
| ModelPrepCycleColors | 8001D4A8 | Prepare per-vertex cycle colors. |
| ModelEmitPrims | 800180B8 | Emit model GPU primitives. |
| ModelEmitPrimsCycleColors | 8001D530 | Emit model primitives with cycle colors. |
| DebugTextPrintfWhite | 8008CBFC | Draw debug-menu status and entry labels. |
| ColorFadeSet | 8008C3C0 | Set the debug text/primitive fade color. |
| ZakoDebugDrawPrimitive | native renderer boundary | Project an enemy and emit its native debug OT packets. |
| GetSolidHeight | 8004D8C0 | Query the world heightfield on a ground-cache miss. |
| MatrixLookDir / MatrixFromDirXZ | 8001484C / 80014804 | Build spawn transforms from direction vectors. |
| SpawnHooks.PlaySfx | 800877F8 | Play the spawn/death sound through the platform audio system. |
| AeBodyHooks callbacks | see `src/bodies/ae_bodies.h` | Supply collision-body sound, ray/probe, body movement, pickup, and camera behavior where the shared collision helpers request them. |
| Sound hardware callbacks | SPU register / wait boundary | Wait for SPU state and write voice ADSR, volume, and key-off registers. |

## Findings

- `ObjUpdateMotionState` reads the gravity table index from object byte `+0x83`,
  the last-hit type, rather than the current enemy type.
- `ZakoUpdateLoop` skips the rest of an object iteration after motion returns
  status 2 and `ObjResetMotion` runs.
- `ZakoBuildMatrix` sends both nonzero orientation modes through the direction
  matrix helper; mode 2 also uses the mode-specific scaling path.
- `ZakoUpdateColliderPos` reads its height input from collider offset `+0x08`
  and stores the result at the collider Y field.
- The debug OT cursor returned by the renderer is a packet pointer, while the
  context link stores its physical low 24 bits. The frame draw count at 800EC350
  and the OT context index at 800EC35C are distinct values.
- ObjSwapToReactAnim reaches SpawnPuff through ObjDeathSfx. Its original nested
  stack leaves a 0x8000 halfword in the particle position padding field; the
  checker provides that exact residue to the C callback.
- Negative angle and fixed-point paths use MIPS arithmetic shifts and signed
  truncation behavior; those are preserved in the C helpers.

Validation command: `.venv/Scripts/python.exe tools/mips/check_zako1.py --coverage`.
The checker is under the task runtime target and writes
`extract/checks/results/check_zako1.json` on its default-scale run.
