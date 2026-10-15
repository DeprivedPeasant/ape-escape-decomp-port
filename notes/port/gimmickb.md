# Gimmick update batch B

## Scope and call map

The task table names 40 functions at `0x80090D6C..0x800962A4` (10,020 bytes). Its scope paragraph also says “including GimmickSwing*”; those are five earlier functions at `0x8008AD14..0x8008B3A4` (2,788 bytes) omitted from the table. This batch follows the explicit scope and includes those five as well. The first-half gimmick functions remain hooks unless already ported.

The direct calls below were read from the SCUS-94423 MIPS listings with `tools/mips/mips_ram.py`, before translating the C. `FUN_...` means the target has no stronger name in the available symbol inventory. Calls to functions outside this batch are hooks unless that original function already has a validated RAM-image port.

| Function | Direct callees in listing |
| --- | --- |
| `Gimmick_Init_MonkeyQuota` | `Gimmick_MonkeyQuota_Update` |
| `Gimmick_MonkeyQuota_Update` | `AreaMonkeyQuotaMet`, `FUN_80097504`, `GimmickActivate` |
| `Gimmick_Init_ZoneMask` | `SpecialActorPlaceAtCollider` |
| `Gimmick_PlayerInCylinder_Update` | `FUN_8009B3B0` |
| `Gimmick_ProxTrigger_Init` | `IsTimeAttack`, `Indicator_Register` |
| `Gimmick_ProxTrigger_Update` | `VecLength3`, `Indicator_Activate`, `SpawnEffectGroupAt` |
| `Gimmick_Breakable_Update` | `CameraShake`, `PadRumble`, `FUN_8006AA60`, `MeshInitFromAreaTable`, `FUN_80091598` |
| `Gimmick_Resize_Update` | `PlaySfxAt` (two call sites) |
| `Gimmick_Init_Closed3`, `Gimmick_Slider_Update`, `Gimmick_SliderQuiet_Update` | `FUN_80091AF0` |
| `Gimmick_Slider_Update` | also `PlaySfxAt` (two call sites) |
| `Gimmick_PlayerBox_Update` | `GimmickPlayerInBox` |
| `Gimmick_Init_RecordOrigin` | `FUN_80092324` |
| `Gimmick_LinkedLift_Update` | `SoundLoopStart`, `SoundEmitterPan`, `SoundLoopStop`, `FUN_80092324`, `FUN_80091C04` |
| `Gimmick_Init_PropTilt` | `RotMatrixZYX` |
| `Gimmick_Oscillate_Update` | `rcos` (two sites), `RotMatrixZYX`, `ApplyMatrixSV`, `GimmickPlayerInBox`, `PlaySfxAt` |
| `Gimmick_Init_PropMeshSet` | `MeshInitFromAreaTable` |
| `Gimmick_Counter_EventCam_Update` | `EventCamStart` |
| `Gimmick_PlayerBlast_Update` | `VecLength3`, `VectorNormalSS`, `CollisionBodyAlloc`, `CollisionBodyRegister` |
| `Gimmick_Grab_Update` | `FUN_80093848`, `PlayerAttachSet` |
| `Gimmick_Bounce_Update` | `PadRumble`, `PlaySfx`, `FUN_80066F34`, `VectorNormalSS` (three sites), `PlayerAttachSet`, `rsin` (three sites), `SvecToDiagMatrix` |
| `Gimmick_Rotate_Update` | `FUN_80094330`, `DiagonalSignedDistance` (two sites) |
| `Gimmick_Falling_Update` | `FUN_80094A48` (two sites), `FUN_80094E5C`, `PlaySfxAt` (two sites), `GimmickMoveColliderTo` |
| `GimmickMoveColliderTo` | `GimmickMoveCollider` |
| `GimmickMoveCollider` | `ColliderBoxContainsPoint` (two sites), `CameraNudge`, recursive `GimmickMoveCollider` |
| `GimmickSwingCarryPoint` | `GimmickSwingAngle`, `rcos`, `rsin` |
| `GimmickSwingAngle` | no direct calls |
| `GimmickSwingFirstStep` | `RotMatrixZYX` |
| `GimmickSwingAdvance` | no direct calls |
| `GimmickSwingProbeWall` | `GetGroundHeight` |

All other functions in the batch have no direct calls. `Gimmick_NopInit`, the four `Gimmick_NopUpdate*` entries, and `GimmickStateTablePtr` are leaves. Calls to the already ported `GimmickActivate` and the validated SDK trig routines use those C ports directly. `CollisionBodyAlloc` and `CollisionBodyRegister` also have validated RAM-image ports and are direct calls. Rendering, audio, player, indicator, and remaining unnamed helpers stay as named hooks.

## Findings

## Port and validation

All 45 functions in the expanded scope are implemented as readable C in `src/world/ae_gimmick_b.c`, with named RAM fields and signatures in `src/world/ae_gimmick_b.h`. The 40 table entries are joined by `GimmickSwingCarryPoint`, `GimmickSwingAngle`, `GimmickSwingFirstStep`, `GimmickSwingAdvance`, and `GimmickSwingProbeWall`. Existing validated ports are called directly for `GimmickActivate`, collision-body operations, SDK matrix/trig routines, `VecLength3`, `CameraShake`, and `PlaySfxAt`; the last three are checked here through their real MIPS bodies and their existing native C ports. `GimmickSwingProbeWall` calls the existing `ae_GetGroundHeight` port.

The default checker uses the real executable and an extracted area member, then compares return values, main RAM outside the stack window, scratchpad, and ordered external-call effects. It passes all 45 functions. Instruction coverage is 3,358/3,375; the remaining 17 instructions have the proofs registered below. The full mutation run caught 90/90 variants, two per function.

| Function | Status | Default cases | Instruction coverage |
| --- | --- | ---: | ---: |
| `Gimmick_Init_MonkeyQuota` | diffed | 4 | 10/10 |
| `Gimmick_MonkeyQuota_Update` | diffed | 7 | 48/48 |
| `Gimmick_NopInit` | diffed | 8 | 2/2 |
| `Gimmick_NopUpdate` | diffed | 8 | 2/2 |
| `Gimmick_Init_Closed` | diffed | 6 | 19/19 |
| `Gimmick_Init_ZoneMask` | diffed | 2 | 29/29 |
| `Gimmick_PlayerInCylinder_Update` | diffed | 4 | 56/56 |
| `Gimmick_ProxTrigger_Init` | diffed | 3 | 70/70 |
| `Gimmick_ProxTrigger_Update` | diffed | 14 | 120/120 |
| `Gimmick_Breakable_Init` | diffed | 3 | 37/37 |
| `Gimmick_Breakable_Update` | diffed | 7 | 107/107 |
| `Gimmick_Init_Closed2` | diffed | 5 | 29/29 |
| `Gimmick_NopUpdate2` | diffed | 8 | 2/2 |
| `Gimmick_Init_PropHidden` | diffed | 4 | 26/26 |
| `Gimmick_Resize_Update` | diffed | 5 | 65/65 |
| `Gimmick_Init_Closed3` | diffed | 4 | 35/35 |
| `Gimmick_Slider_Update` | diffed | 7 | 71/71 |
| `Gimmick_SliderQuiet_Update` | diffed | 7 | 54/54 |
| `Gimmick_PlayerBox_Update` | diffed | 3 | 11/11 |
| `GimmickPlayerInBox` | diffed | 10 | 163/163 |
| `Gimmick_Init_RecordOrigin` | diffed | 3 | 63/63 |
| `Gimmick_LinkedLift_Update` | diffed | 8 | 158/158 |
| `Gimmick_Init_PropTilt` | diffed | 3 | 41/41 |
| `Gimmick_Oscillate_Update` | diffed | 12 | 212/218; 6 proven unreachable |
| `Gimmick_Init_PropMeshSet` | diffed | 3 | 55/55 |
| `Gimmick_Init_CounterGate` | diffed | 5 | 27/27 |
| `Gimmick_Counter_EventCam_Update` | diffed | 7 | 58/58 |
| `Gimmick_PlayerBlast_Update` | diffed | 5 | 83/83 |
| `Gimmick_Init_Disabled` | diffed | 4 | 18/18 |
| `Gimmick_Grab_Update` | diffed | 5 | 48/48 |
| `Gimmick_Init_PropFlag2000` | diffed | 6 | 28/28 |
| `Gimmick_Bounce_Update` | diffed | 18 | 314/314 |
| `Gimmick_Init_Hidden` | diffed | 6 | 36/36 |
| `Gimmick_Rotate_Update` | diffed | 26 | 267/267 |
| `Gimmick_Falling_Update` | diffed | 7 | 113/113 |
| `GimmickMoveColliderTo` | diffed | 2 | 41/41 |
| `GimmickMoveCollider` | diffed | 3 | 147/147 |
| `Gimmick_NopUpdate3` | diffed | 8 | 2/2 |
| `Gimmick_NopUpdate4` | diffed | 8 | 2/2 |
| `GimmickSwingCarryPoint` | diffed | 12 | 161/161 |
| `GimmickSwingAngle` | diffed | 6 | 47/50; 3 proven unreachable |
| `GimmickSwingFirstStep` | diffed | 9 | 108/114; 6 proven unreachable |
| `GimmickSwingAdvance` | diffed | 12 | 95/95 |
| `GimmickSwingProbeWall` | diffed | 8 | 275/277; 2 proven unreachable |
| `GimmickStateTablePtr` | diffed | 1 | 3/3 |

The unreachable instructions are signed division overflow checks whose numerators cannot be `INT_MIN`, divide-by-zero checks dominated by a nonzero-axis guard, and a repeated fourth-corner wall comparison after that sample was already proven above the collider. Oscillate's six guards are signed-16 timer divisions whose shifted numerator cannot be `INT_MIN`. Each span has an append-only proof row in `tools/mips/check_exclusions.csv`; there are no other coverage gaps.

`GimmickPlayerInBox` is 652 bytes through its actual epilogue at `0x80091FAC`; the inventory size ends at `0x80091EDC`. `Gimmick_Rotate_Update` includes its out-of-line jump-table arms through `0x8009432C`; `FUN_80094330` starts after them. These listing boundaries are reflected in the checker and are not separate functions.

## Remaining external hooks

The following original callees are still outside this batch and remain explicit hooks. Names beginning `FUN_` are provisional because the symbol inventory does not identify them more precisely.

| Hook | Original role observed at call sites |
| --- | --- |
| `AreaMonkeyQuotaMet` | Counts eligible area monkeys and compares the configured quota. |
| `FUN_80097504` | Supplies the alternate count used by the secondary monkey-quota subtype; exact semantics remain unnamed. |
| `SpecialActorPlaceAtCollider` | Places the special actor at a collider, copies its position, and sets facing/scale and area flags. |
| `FUN_8009B3B0` | Player-in-cylinder trigger action; exact side effects remain unnamed. |
| `IsTimeAttack` | Tests the game mode/time-attack flag. |
| `Indicator_Register`, `Indicator_Activate` | Register a proximity indicator and start its hint/effect action. |
| `SpawnEffectGroupAt` | Spawns an effect group using default effect tables. |
| `PadRumble` | Sends a DualShock motor-strength/duration request. |
| `FUN_8006AA60` | Breakable-prop finalization/visual update; exact behavior remains unnamed. |
| `MeshInitFromAreaTable` | Initializes a mesh from the area mesh table. |
| `FUN_80091598` | Breakable-object update called after hit/animation state changes; exact behavior remains unnamed. |
| `FUN_80091AF0` | Shared closed-prop/slider helper; exact behavior remains unnamed. |
| `SoundLoopStart`, `SoundEmitterPan`, `SoundLoopStop` | Start, position, and stop the linked-lift looping sound. |
| `FUN_80092324` | Supplies a saved origin/mesh-state value for record-origin and linked-lift logic; exact behavior remains unnamed. |
| `FUN_80091C04` | Applies a linked-lift secondary update from subtype, link, and progress fields. |
| `EventCamStart` | Starts the selected event-camera sequence. |
| `FUN_80093848` | Tests whether the player can be grabbed and supplies the attachment point. |
| `PlayerAttachSet` | Copies attachment data into the player and sets its attached state. |
| `PlaySfx` | Starts a non-positional sound effect. |
| `FUN_80066F34` | Bounce-triggered player response; exact behavior remains unnamed. |
| `SvecToDiagMatrix` | Builds the diagonal scale matrix used by bounce visuals. |
| `FUN_80094330` | Tests whether the player/collider rotation interaction is accepted. |
| `FUN_80094A48`, `FUN_80094E5C` | Falling-gimmick probe and collider-contact tests; exact helper behavior remains unnamed. |
| `ColliderBoxContainsPoint` | Tests whether a point lies inside a collider box. |
| `CameraNudge` | Offsets camera eye and target positions when camera mode allows it. |
| `GetSolidHeight` | Solid collision-grid/box query used transitively by the existing `ae_GetGroundHeight` port through `AeWorldHooks`. |
| `PropSetMesh`, `FUN_801414a0` | Renderer mesh replacement and the type-0x29 overlay callback remain hooks beneath the existing `GimmickActivate` port. |

`GimmickActivate`, `GimmickDeactivateChain`, `SwingDoorSetPivot`, `GimmickEvent`, and `GimmickGetInfo` are called through their existing C ports. Collision-body and SDK math helpers also use their existing ports. The checker build links the existing spawn and sound sources to exercise `CameraShake` and positional sound directly, with only their hardware endpoints controlled.

The validator automatically scans new `src/**/ae_*.c` files, so no scan-list entry was needed. `tools/check_verified.py` now accepts the task's append-only status history where a previous `wip` row is followed by an identical-identity `diffed` row; all other duplicates still fail.
