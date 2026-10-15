# PlayerUpdate first-half port map

This note records the address-based batch from `extract/codex-task/task.md`. The original call edges below were read from the SCUS-944.23 instruction ranges with `tools/mips/mips_ram.py`. Eighteen functions have readable RAM-image ports and independent differential checks; `PlayerUpdate` remains unfinished.

## Call tree and boundaries

The table's direct edges are:

| Original function | Direct callees in this batch | Direct already-ported callees | Remaining out-of-batch calls |
|---|---|---|---|
| `XaPlay` | `CdMgrQueueCommand` | — | `SoundSetVolumeMode`, `CdPosToInt` |
| `CdMgrQueueCommand` | `CdMgrIssueCommand` | `printf` is a libc/SDK boundary | — |
| `CdMgrIssueCommand` | — | — | `CdControl`, `CdReadyCallback`, `CdPosToInt`, `VSync` |
| `ArcGetMemberPos` | — | — | `CdIntToPos` (SDK) |
| `ActorAnimDecodeEntry` | — | `AnimDecodeLerp` | — |
| `ModelDrawWithOt` | — | — | `ModelEmitPrims` (renderer) |
| `ModelPrepCycleColors` | — | — | — |
| `ModelEmitPrimsCycleColors` | — | — | renderer packet / projection blocks |
| `TrailRingReset` | — | — | — |
| `DrawRibbonBetween` | — | `VectorNormalSS` (SDK math) | renderer projection / packet block |
| `PlayerIdleModelSetup` | — | `ModelInit` | — |
| `AimRequestClear` | — | — | — |
| `CameraFixedModeOff` | — | — | — |
| `AimModeEnter` | `IsArea53` | `PlaySfx` | — |
| `EventCamStartRandom` | — | `PlaySfx` | `EventCamCreate` |
| `IsArea53` | — | — | — |
| `HudShowHealth` | — | — | — |
| `PlayerDamage` | `HudShowHealth` | — | — |
| `PlayerUpdate` | `ArcGetMemberPos`, `CdMgrQueueCommand`, `PlayerIdleModelSetup`, `AimRequestClear`, `XaPlay`, `PlayerDamage` | `PlayerSetMode`, `HeapAlloc`, `HeapFree`, `StageRequest`, `GetGroundHeightEx`, `GetObjectByIndex`, `StageExitIsActive`, `PlayerCheckGadgetSelect`, `PlayerSetState`, `PlaySfx`, `VectorNormalSS`, `GetSurfaceType`, `rsin`, `rcos`, `MatrixFromDirXZ`, `ApplyMatrixSV`, `VecLength3`, `PlayerSetTintEffect`, `SpawnParticle`, `CollisionBodyAlloc`, `CollisionBodyRegister`, `PlayerRunAnimEvents` | `CdPosToInt`, `memset`, overlay `MCXCMD_OBJ_328`, `PlayerIsOnGround`, `PlayerUpdateClimb`, `PlayerUpdateRope`, `PlayerUpdateHeldByObject`, `PlayerUpdateIdleAnim`, `PlayerDrawSquashedWithFx`, `PlayerDrawSquashedFlattened`, `PlayerAnimateAndDraw`, `ModelDraw`, `TurnTowardsY`, `PlayerApplyGadgetStance`, `PadRumble`, `PlayerSpawnDustBehind`, `PlayerAccelXYZ`, `PlayerAccelXZ`, `PlayerOnWallBump`, `PlayerCheckWaterEntry`, `PlayerResetAir`, `PlayerEnterSwimIfInWater`, `PlayerRunTransitions`, `PlayerOnContact`, `PlayPlayerVoice`, `PlayerSelectGadget`, `PlayerSpawnDust`, and `rand` |

The renderer replaces GPU/GTE work. `ModelDrawWithOt` keeps its arguments and projection setup, then calls `ModelProjectVertices` and `ModelEmitPrims`. `ModelEmitPrimsCycleColors` delegates its full 0x640-byte renderer function through one named hook. `DrawRibbonBetween` computes its endpoint delta, calls the verified `VectorNormalSS` port, builds a named descriptor in scratchpad, and passes it to the native ribbon renderer. These replaced ranges are recorded in `tools/mips/check_exclusions.csv`; the MIPS oracle still executes them, so the coverage report labels those declarations stale while confirming the reference paths ran.

## Hook policy

Functions in the task table call one another directly. Calls to existing RAM-image ports call those C ports directly (including the already-diffed SDK math routines). PsyQ / BIOS / libc calls and unported game functions remain hooks carrying the original arguments. A missing required hook sets the `AeRam` error flag.

## Validation status

The default `tools/mips/check_player1.py --coverage` run passes 862 cases in 40.7 seconds and uses four real area members (`05B.bin`, `05E.bin`, `064.bin`, `066.bin`). Every checked case compares return values, all main RAM below the registered stack window, scratchpad, and hook arguments/call order.

| Function | Default cases | MIPS instruction coverage | Status |
|---|---:|---:|---|
| `XaPlay` | 48 | 105/105 | diffed |
| `CdMgrQueueCommand` | 5 | 129/131 | diffed; negative ring-index wrap branches proven unreachable |
| `CdMgrIssueCommand` | 13 | 127/128 | diffed; negative read-index wrap branch proven unreachable |
| `ArcGetMemberPos` | 48 | 25/25 | diffed |
| `ActorAnimDecodeEntry` | 56 | 37/44 | diffed; seven divide guards proven unreachable because the divisor is unsigned and the first zero divide traps |
| `ModelDrawWithOt` | 48 | 50/50 | diffed; the per-vertex GTE block is a renderer hook |
| `ModelPrepCycleColors` | 60 | 34/34 | diffed |
| `ModelEmitPrimsCycleColors` | 64 | 380/401 | renderer-owned; unvisited unsupported modes are registered; executed packet/GTE ranges remain in the MIPS oracle |
| `TrailRingReset` | 24 | 11/11 | diffed |
| `DrawRibbonBetween` | 64 | 346/362 | diffed; unvisited paths and delegated projection/packet ranges are registered |
| `PlayerIdleModelSetup` | 40 | 56/56 | diffed |
| `AimRequestClear` | 20 | 9/9 | diffed |
| `CameraFixedModeOff` | 20 | 5/5 | diffed |
| `AimModeEnter` | 48 | 22/22 | diffed |
| `EventCamStartRandom` | 48 | 28/28 | diffed |
| `IsArea53` | 64 | 9/9 | diffed |
| `HudShowHealth` | 64 | 31/31 | diffed |
| `PlayerDamage` | 128 | 41/41 | diffed |
| `PlayerUpdate` | — | — | not ported or validated |

The mutation runner caught all 36/36 deliberate logic changes across the 18 implemented functions. `PlayerUpdate` is still absent from the C module and ledger. `src/player/native_player.c` contains a separate host reconstruction that explicitly covers only part of mode 0 and reports approximations; it is not used as a substitute for this function's exact RAM-image port.

## Hooked behavior

For implemented functions, the platform hooks left open are `CdIntToPos`, `CdPosToInt`, `CdControl`, `CdReadyCallback`, `VSync`, `printf`, `SoundSetVolumeMode`, and `EventCamCreate`. Renderer hooks are `ModelProjectVertices`, `ModelEmitPrims`, `ModelEmitPrimsCycleColors`, and `DrawRibbonBetween`. `AnimDecodeLerp`, `ModelInit`, `PlaySfx`, and `VectorNormalSS` are called through their existing ports.

`PlayerUpdate` itself is still wholly unimplemented. Its call-tree hooks are listed in the table above; the missing function also contains substantial inline state, movement, ground-contact, animation and render coordination that must be ported before its coverage and mutation requirements can be closed.
