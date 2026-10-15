# Player Update batch 2

## Call tree and boundaries

`Mode_Stage` enters the existing `PlayerUpdate` dispatcher. Its mode branches use this batch as follows:

- Normal movement uses the ground/water checks, air and velocity helpers, transitions, gadget selection, and contact response.
- Modes 5, 6, and 7 dispatch to `PlayerUpdateClimb`, `PlayerUpdateRope`, and `PlayerUpdateHeldByObject`.
- Mode 8 uses `MonkeyNetCatchUpdate`; mode 10 uses `PlayerUpdateIdleAnim`; modes 12/13 use the squashed draw/update paths.
- The draw/effect cluster is `PlayerDrawSquashed calls squash/flattened particle routines and effect helpers. The renderer calls are named hooks.
- `PlayerCheckWaterEntry` calls `PlayerSplashStart`; `PlayerOnContact` can call the particle, sparkle, ground-impact, damage, state, and rumble paths.
- `PlayerSelectGadget` uses the already-ported gadget, radar, aim-table, car, effect, state, and sound helpers.

42 of the 43 task-table entries have readable C definitions in `src/player/ae_player2_leaf.c`, declared by `src/player/ae_player2.h`. This includes the executable area-overlay body at `8013F0C0`. Existing verified functions are called directly, including `ae_RcCarDespawn`, `ae_CollisionBodyRegister`, the player-action ports, and the spawn/world helpers. Rendering and hardware-facing operations remain named hooks. `80142808` remains open because the same runtime address contains different code in different overlays; see below.

## Differential results

Default run: **passed**, 807 cases, areas 1, 5, 37, plus the area 2 player overlay. The checker reported 3,674/3,701 original instructions executed. There are no undeclared coverage gaps.

| Address | Function | Status | Cases | MIPS instruction coverage |
|---|---|---|---:|---:|
| 800652E4 | `PlayerIsOnGround` | ported + diffed | 6 | 13/13 |
| 800657F0 | `PlayerCheckWaterEntry` | ported + diffed | 8 | 62/62 |
| 8006699C | `PlayerAccelXZ` | ported + diffed | 6 | 71/80 (9 proved unreachable) |
| 80066ADC | `PlayerAccelXYZ` | ported + diffed | 6 | 96/109 (13 proved unreachable) |
| 8006B70C | `PlayerSpawnDust` | ported + diffed | 7 | 52/52 |
| 8013F0C0 | `FUN_8013f0c0` (area overlay 18C) | ported + diffed | 4 | 61/61 |
| 8006C2C0 | `PlayerFxSpawn10C` | ported + diffed | 6 | 10/10 |
| 8006B570 | `EffectGroundImpact` | ported + diffed | 6 | 80/80 |
| 8006BF84 | `EffectGroundImpactAtPlayer` | ported + diffed | 6 | 13/13 |
| 80099360 | `RcCarSpawn` | ported + diffed | 6 | 67/67 |
| 80066018 | `PlayerStartHurtBlink` | ported + diffed | 3 | 4/4 |
| 80066094 | `PlayerEnterSwimIfInWater` | ported + diffed | 6 | 13/13 |
| 800668A0 | `PlayerOnWallBump` | ported + diffed | 7 | 63/63 |
| 8006896C | `PlayPlayerVoice` | ported + diffed | 14 | 33/33 |
| 80067A1C | `PlayerApplyGadgetStance` | ported + diffed | 12 | 53/53 |
| 800665E0 | `PlayerResetAir` | ported + diffed | 3 | 6/6 |
| 8006CFA4 | `PlayerFxResetBlink` | ported + diffed | 3 | 6/6 |
| 8006BF10 | `PlayerSpawnDustBehind` | ported + diffed | 3 | 29/29 |
| 8006C204 | `PlayerSparkleBurst` | ported + diffed | 3 | 47/47 |
| 8007A304 | `AimTableClear3` | ported + diffed | 3 | 10/10 |
| 8007AA64 | `PlayerFxClearDAT_800F5144` | ported + diffed | 3 | 13/13 |
| 8007F5D4 | `IsPauseMenuOpen` | ported + diffed | 3 | 4/4 |
| 80099D20 | `RadarResetState` | ported + diffed | 3 | 20/20 |
| 800A1174 | `Max16` | ported + diffed | 6 | 10/10 |
| 800A2A14 | `SpuSetMainVolume` | ported + diffed | 6 | 23/23 |
| 80086248 | `SoundSetVolumeMode` | ported + diffed | 8 | 39/39 |
| 80070B70 | `SpawnRandomParticle` | ported + diffed | 8 | 136/137 (1 proved unreachable) |
| 8006B848 | `PlayerSplashStart` | ported + diffed | 4 | 66/66 |
| 800649D8 | `PlayerRunTransitions` | ported + diffed | 7 | 70/70 |
| 80068634 | `PlayerUpdateHeldByObject` | ported + diffed | 7 | 105/105 |
| 8006C92C | `PlayerFxSquashParticles` | ported + diffed | 8 | 191/191 |
| 8006CC28 | `PlayerFxFlattenedParticles` | ported + diffed | 7 | 110/110 |
| 800689F0 | `PlayerDrawSquashed` | ported + diffed | 5 | 59/59 |
| 80068ADC | `PlayerDrawSquashedWithFx` | ported + diffed | 8 | 16/16 |
| 80068B1C | `PlayerDrawSquashedFlattened` | ported + diffed | 7 | 24/24 |
| 800687D8 | `PlayerUpdateIdleAnim` | ported + diffed | 4 | 101/101 |
| 8007AC64 | `PlayerSelectGadget` | ported + diffed | 10 | 213/213 |
| 80067CAC | `PlayerUpdateClimb` | ported + diffed | 67 | 334/334 |
| 800681E4 | `PlayerUpdateRope` | ported + diffed | 55 | 272/276 (4 proved unreachable) |
| 80066FC8 | `PlayerOnContact` | ported + diffed | 440 | 619/619 |
| 8007BD38 | `MonkeyNetCatchDraw` | ported + diffed | 11 | 346/346 |
| 8007C2A0 | `MonkeyNetCatchUpdate` | ported + diffed | 12 | 114/114 |
| 80142808 | `FUN_80142808` | overlay hook; variant unresolved | - | no single overlay body identified |

Declared coverage proofs in `tools/mips/check_exclusions.csv`:

- `PlayerAccelXZ`: 9 division/rounding instructions cannot execute because the vector length is a positive divisor in the update arm.
- `PlayerAccelXYZ`: 13 corresponding division/rounding instructions have the same positive-length proof.
- `SpawnRandomParticle`: 1 signed-negative cursor branch cannot execute; the cursor is loaded unsigned and normalized to the 0-7 ring.
- `PlayerUpdateRope`: 4 divide trap instructions cannot execute for a valid attached rope; `PlayerSetMode(6)` stores the positive integer length before entering this state.

The 42 ported functions were subjected to 113 source mutations (at least two per function); all 113 were caught.

## Functions kept as overlay hooks

`FUN_8013f0c0` is in area member `18C.bin`, loaded at `0x8013D000`; it contains 61 instructions through `0x8013F1B0` and the particle script table at `0x8013F248`. It is now ported and checked as part of `PlayerSpawnDust` for area 2 and in four direct cases. Its final position is sampled with interleaved primary and secondary seed updates before one type-200 particle is spawned.

`FUN_80142808` (`0x80142808`) is called by `PlayerUpdate` for player mode 2, but the address is overlay-dependent. The area-overlay scan found a 420-byte player animation/render function starting there in `ovl_1A2` (areas `0x4B`/`0x4C`), an 8-byte return-zero stub in `ovl_199`, and an interior block in `ovl_19D`; `ovl_19E` and `ovl_1A6` also have different instructions at the same address. Separately, `ovl_186::80142808` is `MCXCMD_OBJ_328` (60 bytes) in the save-menu overlay. The task table does not identify which loaded overlay supplies the mode-2 callback, so there is not yet a single behavior to port and validate for this address.

## Other hook boundaries

- Graphics/GTE-facing hooks (`AnimDecodeFrame`, `MatrixFromFrontUp`, `ScaleMatrixL`, `ModelTransformSkinned`, `ModelPrepCycleColors`, `ModelEmitPrimsCycleColors`, `ModelDraw`, `DrawRibbonBetween`, `MatrixLookDir`, `ModelTransformRigid`, `ModelDrawWithOt`, `ActorAnimDecodeEntry`, `SetRotMatrix`, `SetTransMatrix`, `RotTransSV`, `RotTransPers`, `RotMatrixY`) replace model setup, projection, packet construction, and ordering-table work for the native renderer.
- Audio hooks (`PlaySfx`, sound-loop start/stop, and voice output) deliver sound requests to the native audio layer. `SpuSetMainVolume` and `SoundSetVolumeMode` game logic are ported; the native layer handles the hardware write.
- Player integration hooks (`PlayerStopGadgetSounds`, `PlayerCanActivateGadget`, `AimModeEnter`, `AimModeLeave`, `TrailRingReset`, `CameraFixedModeOff`, `AimRequestClear`, `PlayerCondition`, `PlayerAction`, `TurnTowardsY`, `PlayerDamage`, `PadRumble`, `EventCamStartRandom`) bridge to the player, input, camera, area, and rumble systems outside this batch. State/mode setters, action logic in scope, and collision-body bookkeeping use their existing ports directly.
- `AeSpawnHooks.position_padding` supplies caller-owned fourth halfwords copied by legacy eight-byte position copies. `AePlayer2Hooks.position_padding` supplies the area-2 overlay's local stack value. The checker seeds each exact MIPS slot per scenario; these are explicit stack-value boundaries, not renderer hooks.

## Findings

- `PlayerOnContact` requires sign-extending normalized halfword vectors before 32-bit knockback arithmetic, and the two knockback branches use opposite facing signs.
- Contact damage can change mode before the frame counter logic; the routine reloads player state after the mode-change call.
- `PlayerSelectGadget` stores the aim request separately from aim mode and runs a nested selection effect using a caller-owned particle-position padding halfword.
- The area-2 dust overlay takes random X/Z samples with interleaved global seeds, then spawns one particle from its overlay-local script table.
- `PlayerUpdateClimb` and `PlayerUpdateRope` have state subgraphs that are reached only with the requested player state preserved by the fixture; the default matrices cover all reachable climb instructions and all reachable rope instructions.
