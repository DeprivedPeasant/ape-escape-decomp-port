# MissB batch port

This note records the 69 entries in `extract/codex-task/task.md`: 66 readable C ports with ledger rows and three named pure-renderer hooks without ledger rows. The new C lives in eight `ae_missb` modules under `src/ui`, `src/world`, and `src/sound`; it runs on `AeRam`, uses named field offsets, and keeps platform, renderer, and out-of-batch calls behind their named interfaces. The task checker discovers all eight modules directly; `tools/check_verified.py` already scans `src/**/ae_*.c`, so its scan list did not need a change.

## Validation

- Default checker: **PASS**, 434 differential cases, 25.5 seconds, with a 2,000,000-instruction budget passed to every MIPS oracle call.
- Real area members exercised: 01->05B, 02->05E, 05->066, 13->120, 58->138, 63->129, and 67->135.
- Required MIPS instruction coverage for the 66 retained C ports: **5,096 / 5,160 executed**. All 64 remaining slots are declared in `tools/mips/check_exclusions.csv`: packet-building stores, the capture-grid negative-index rounding guard, invalid model-ratio division guards, or signed division traps outside valid script inputs. The three pure renderer hooks have separate full-span store-audit declarations. There are no undeclared coverage gaps.
- Mutation checker: **132 / 132 mutations caught** (two per C port).
- `tools/verified.csv` contains a `diffed` row for each function, with `min_cases` taken from the default run below.
- `tools/check_verified.py`: **PASS, 0 errors, 122 warnings**. Warnings are for unrelated checkers without result files and task entries absent from `notes/inventory/functions.csv`; this task explicitly keeps that inventory file unchanged.

## Per-function status

Every function listed here is ported and differentially diffed. Coverage is shown as executed instruction slots; registered exclusions are identified by their count and reasons in `tools/mips/check_exclusions.csv`.

| Address | Original function | C function | Source | Status | Default cases | MIPS coverage |
|---|---|---|---|---:|---:|
| 80082230 | HudTextWidth | ae_HudTextWidth | `src/ui/ae_missb_ui.c` | diffed | 3 | 19/19 |
| 80082398 | PauseDrawCaptureGrid | ae_PauseDrawCaptureGrid | `src/ui/ae_missb_ui.c` | diffed | 6 | 321/344; 23 packet/unreachable instructions declared |
| 800828F8 | HudSetGeomOffsetThenDraw | ae_HudSetGeomOffsetThenDraw | `src/ui/ae_missb_ui.c` | diffed | 3 | 67/67 |
| 80082A04 | PauseDrawCounter | ae_PauseDrawCounter | `src/ui/ae_missb_ui.c` | diffed | 9 | 207/207 |
| 80082D40 | PauseMenuInitVolumes | ae_PauseMenuInitVolumes | `src/ui/ae_missb_ui.c` | diffed | 3 | 11/11 |
| 80082D6C | PauseMenuDrawSubScreen | ae_PauseMenuDrawSubScreen | `src/ui/ae_missb_ui.c` | diffed | 12 | 335/356; 21 packet instructions declared |
| 800832FC | StringTableIndex_Main | ae_StringTableIndex_Main | `src/ui/ae_missb_ui.c` | diffed | 3 | 12/12 |
| 800847C4 | ItemsClearFlags | ae_ItemsClearFlags | `src/world/ae_missb_items.c` | diffed | 3 | 10/10 |
| 800847EC | ItemSaveCollectedMask | ae_ItemSaveCollectedMask | `src/world/ae_missb_items.c` | diffed | 3 | 49/49 |
| 80086148 | SoundSilence | ae_SoundSilence | `src/sound/ae_missb_sound.c` | diffed | 5 | 10/10 |
| 80086170 | SoundStopAndRestoreVolume | ae_SoundStopAndRestoreVolume | `src/sound/ae_missb_sound.c` | diffed | 5 | 54/54 |
| 8008760C | BgmFadeOut | ae_BgmFadeOut | `src/sound/ae_missb_sound.c` | diffed | 3 | 18/18 |
| 80087BA4 | SoundEmitterVolume | ae_SoundEmitterVolume | `src/sound/ae_missb_sound.c` | diffed | 3 | 31/33; 2 registered unreachable |
| 80087C64 | SoundSetStereo | ae_SoundSetStereo | `src/sound/ae_missb_sound.c` | diffed | 3 | 10/10 |
| 80088D78 | ColliderScriptRiseToGround | ae_ColliderScriptRiseToGround | `src/world/ae_missb_colliders.c` | diffed | 8 | 109/109 |
| 80088F2C | ColliderScriptLiftUpdate | ae_ColliderScriptLiftUpdate | `src/world/ae_missb_colliders.c` | diffed | 13 | 230/230 |
| 800895F0 | ColliderScriptRotateUpdate | ae_ColliderScriptRotateUpdate | `src/world/ae_missb_colliders.c` | diffed | 13 | 324/328; 4 registered unreachable |
| 80089B10 | ColliderScriptOrbitPlayer | ae_ColliderScriptOrbitPlayer | `src/world/ae_missb_colliders.c` | diffed | 12 | 147/149; 2 registered unreachable |
| 80089D64 | ColliderScriptNoOpA | ae_ColliderScriptNoOpA | `src/world/ae_missb_colliders.c` | diffed | 3 | 2/2 |
| 80089D6C | ColliderScriptPropTransformUpdate | ae_ColliderScriptPropTransformUpdate | `src/world/ae_missb_colliders.c` | diffed | 18 | 453/465; 12 registered unreachable |
| 8008AA64 | ColliderScriptNoOpB | ae_ColliderScriptNoOpB | `src/world/ae_missb_colliders.c` | diffed | 3 | 2/2 |
| 8008DEEC | MessageBoxOpen | ae_MessageBoxOpen | `src/ui/ae_missb_ui.c` | diffed | 3 | 13/13 |
| 8008EC70 | BodyInsideBox | ae_BodyInsideBox | `src/world/ae_missb_boxes.c` | diffed | 5 | 35/35 |
| 8008F308 | SwingDoorBodyDelta | ae_SwingDoorBodyDelta | `src/world/ae_missb_boxes.c` | diffed | 8 | 88/88 |
| 8008FAD0 | SlideDoorMove | ae_SlideDoorMove | `src/world/ae_missb_boxes.c` | diffed | 19 | 86/86 |
| 80090754 | GimmickTouchCheck | ae_GimmickTouchCheck | `src/world/ae_missb_boxes.c` | diffed | 8 | 114/114 |
| 8009091C | ColliderBoxContainsPoint | ae_ColliderBoxContainsPoint | `src/world/ae_missb_boxes.c` | diffed | 3 | 35/35 |
| 80091598 | GimmickBreakableFloorAdjust | ae_GimmickBreakableFloorAdjust | `src/world/ae_missb_gimmicks.c` | diffed | 7 | 60/60 |
| 80091AF0 | GimmickApplyPropScale | ae_GimmickApplyPropScale | `src/world/ae_missb_gimmicks.c` | diffed | 8 | 69/69 |
| 80091C04 | GimmickSetPropRotation | ae_GimmickSetPropRotation | `src/world/ae_missb_gimmicks.c` | diffed | 9 | 61/61 |
| 80092324 | GimmickColliderContactUpdate | ae_GimmickColliderContactUpdate | `src/world/ae_missb_colliders.c` | diffed | 64 | 465/465 |
| 80093848 | GimmickGetGrabOffset | ae_GimmickGetGrabOffset | `src/world/ae_missb_gimmicks.c` | diffed | 6 | 53/53 |
| 80094330 | GimmickPointWithinExpandedBox | ae_GimmickPointWithinExpandedBox | `src/world/ae_missb_gimmicks.c` | diffed | 3 | 43/43 |
| 80094A48 | GimmickLinkedAreaActive | ae_GimmickLinkedAreaActive | `src/world/ae_missb_gimmicks.c` | diffed | 6 | 38/38 |
| 80094E5C | GimmickMonkeyTouchingBox | ae_GimmickMonkeyTouchingBox | `src/world/ae_missb_gimmicks.c` | diffed | 3 | 42/42 |
| 80094F48 | StageType_Clear_Init | ae_StageType_Clear_Init | `src/world/ae_missb_game.c` | diffed | 4 | 58/58 |
| 80095030 | StageType_Clear_Update | ae_StageType_Clear_Update | `src/world/ae_missb_game.c` | diffed | 8 | 133/133 |
| 8009543C | StageVictoryStart | ae_StageVictoryStart | `src/world/ae_missb_game.c` | diffed | 3 | 10/10 |
| 8009546C | StageIntroStart | ae_StageIntroStart | `src/world/ae_missb_game.c` | diffed | 3 | 22/22 |
| 800954C4 | StageIntroUpdate | ae_StageIntroUpdate | `src/world/ae_missb_game.c` | diffed | 8 | 49/49 |
| 80095588 | PlayerEnterCutsceneMode | ae_PlayerEnterCutsceneMode | `src/world/ae_missb_game.c` | diffed | 3 | 16/16 |
| 800962B0 | ProgressChecksum | ae_ProgressChecksum | `src/world/ae_missb_progress.c` | diffed | 3 | 14/14 |
| 800962F4 | ProgressFlagSet | ae_ProgressFlagSet | `src/world/ae_missb_progress.c` | diffed | 3 | 14/14 |
| 80096360 | ProgressSetByte | ae_ProgressSetByte | `src/world/ae_missb_progress.c` | diffed | 3 | 19/19 |
| 800964B0 | ProgressGetStageResult | ae_ProgressGetStageResult | `src/world/ae_missb_progress.c` | diffed | 3 | 11/11 |
| 800965EC | LevelMonkeyCountGet | ae_LevelMonkeyCountGet | `src/world/ae_missb_progress.c` | diffed | 6 | 12/12 |
| 80096638 | LevelProgressRecordExit | ae_LevelProgressRecordExit | `src/world/ae_missb_progress.c` | diffed | 3 | 31/31 |
| 80097454 | AreaMonkeyQuotaMet | ae_AreaMonkeyQuotaMet | `src/world/ae_missb_game.c` | diffed | 6 | 44/44 |
| 80097504 | AreaAggregateActiveMonkeyCount | ae_AreaAggregateActiveMonkeyCount | `src/world/ae_missb_gimmicks.c` | diffed | 6 | 71/71 |
| 80097620 | GetDAT_800B38B8 | ae_GetDAT_800B38B8 | `src/world/ae_missb_progress.c` | diffed | 3 | 4/4 |
| 80097A60 | StageGoalReached | ae_StageGoalReached | `src/world/ae_missb_game.c` | diffed | 8 | 103/103 |
| 80097BFC | StageGradeUpdate | ae_StageGradeUpdate | `src/world/ae_missb_game.c` | diffed | 7 | 68/68 |
| 80097E60 | LevelTableGet | ae_LevelTableGet | `src/world/ae_missb_progress.c` | diffed | 6 | 10/10 |
| 80097FB0 | LevelCaughtCount | ae_LevelCaughtCount | `src/world/ae_missb_progress.c` | diffed | 6 | 54/54 |
| 80098088 | SaveBuildImage | ae_SaveBuildImage | `src/world/ae_missb_progress.c` | diffed | 3 | 57/57 |
| 8009816C | SaveComputeChecksum | ae_SaveComputeChecksum | `src/world/ae_missb_progress.c` | diffed | 6 | 109/109 |
| 80098330 | HudPopupStart | ae_HudPopupStart | `src/world/ae_missb_game.c` | diffed | 3 | 26/26 |
| 80098FB0 | PlayerAuxModelInit | ae_PlayerAuxModelInit | `src/world/ae_missb_game.c` | diffed | 3 | 15/15 |
| 8009B3B0 | GimmickCylinderSetPlayerFlag | ae_GimmickCylinderSetPlayerFlag | `src/world/ae_missb_gimmicks.c` | diffed | 3 | 7/7 |
| 8009B578 | SpecialActorPlaceAtCollider | ae_SpecialActorPlaceAtCollider | `src/world/ae_missb_game.c` | diffed | 7 | 84/84 |
| 800A2294 | SeqSetMonoFlag | ae_SeqSetMonoFlag | `src/sound/ae_missb_sound.c` | diffed | 3 | 3/3 |
| 800A2640 | SoundBankSetVolume | ae_SoundBankSetVolume | `src/sound/ae_missb_sound.c` | diffed | 6 | 108/108 |
| 800A2B6C | SeqTrackSetPlayState | ae_SeqTrackSetPlayState | `src/sound/ae_missb_sound.c` | diffed | 8 | 180/180 |
| 800A2F54 | SeqFadeTrackOut | ae_SeqFadeTrackOut | `src/sound/ae_missb_sound.c` | diffed | 6 | 36/36 |
| 800A342C | SoundStopAllVoices | ae_SoundStopAllVoices | `src/sound/ae_missb_sound.c` | diffed | 5 | 81/81 |
| 800A35FC | SfxVoiceSetPitch | ae_SfxVoiceSetPitch | `src/sound/ae_missb_sound.c` | diffed | 3 | 24/24 |

## Renderer hooks and store audit

Three functions have no gameplay stores or calculations to port. Their original bodies only write packets, ordering-table links, stack temporaries, and the renderer cursor, so they are named native renderer hooks and have no `tools/verified.csv` rows. The full spans are registered in `tools/mips/check_exclusions.csv`.

| Original function | Named renderer hook | Store audit |
|---|---|---|
| `DrawMenuArrows` (8008211C) | `RendererDrawMenuArrows` | Packet/OT stores: 80082144, 80082148, 8008214C, 80082150, 80082154, 80082160, 80082164, 8008217C, 80082180, 80082184, 80082188, 8008218C, 80082198, 8008219C, 800821B0, 800821B4, 800821C0, 800821C4. Cursor store: 800821D0. No stack or game-state stores. |
| `PauseMenuDrawArrows` (8008332C) | `RendererPauseMenuDrawArrows` | Packet/OT stores: 80083358, 8008335C, 80083368, 8008336C, 80083370, 8008337C, 80083380, 80083388, 8008338C, 80083398, 8008339C, 800833A0, 800833AC, 800833B0, 800833F0, 800833F4, 800833FC, 80083400, 80083404, 80083408, 8008340C, 80083410, 80083414, 80083418, 8008341C, 80083420, 80083424, 80083428, 8008342C, 80083434, 80083438. Cursor store: 80083464. No stack or game-state stores. |
| `AimReticleDrawCircle` (80085E08) | `RendererAimReticleDrawCircle` | Stack stores: 80085E10, 80085E18, 80085E28, 80085E2C, 80085E30, 80085E34, 80085E38, 80085E3C, 80085E40, 80085E44, 80085E70. Temporary packet/OT stores: 80085EF0, 80085F14, 80085F34, 80085F4C, 80085F64, 80085F7C, 80085F80, 80085F94, 80085F9C, 80085FA4, 80085FD0, 80085FF4. Cursor store: 80086020. No game-state stores. |

The other three functions keep their ledger rows because they compute or update game-visible renderer inputs. Their MIPS bodies continue to run in `check_missb.py`; only packet/OT stores are intercepted. `PauseDrawCaptureGrid` keeps the captured/best counts, level type, times, text, and renderer cursor live. `PauseDrawCounter` keeps the count/checksum formatting, save-image writes, scratch text, and calls live. `PauseMenuDrawSubScreen` keeps selection, toggles, volume adjustment, sound calls, button state, and cursor stores live; only its inline background/icon packets and nested renderer packet stores are intercepted. Exact packet-store PCs and the untouched stack, scratch, cursor, and state stores are listed in the corresponding exclusion rows.

The separate out-of-batch interfaces remain named hooks as required by the task boundary. In particular, `ModelTransformRigid` (800161C0) performs native model/GTE transforms, `ModelDrawEnvMap` (8001CE54) draws an environment-mapped model, and `SpuWaitIdle` (800A1398) waits for the native SPU to become idle. Other named external boundaries reached by these functions include `SetDrawTPage` (GPU draw-page state), `XaPlay` (CD/XA playback), `VSync` (frame synchronization), `SpuWaitLoop` and `SeqTrackChannelReset` (SPU/sequencer control), `SoundLoopsSetPan` (loop panning), `ColliderGroundProbe` (collider ground query), `PlayerPlaceAtEntry` (player spawn placement), `EventCamStartVictory` / `EventCamStartDefault` (camera presets), `HudDrawTimeRankIcon` (HUD rank icon drawing), `AreaGetMonkeyRecord` (stored monkey-record lookup), `MeshCalcEnvUVs` (renderer environment-map UV calculation), and `FUN_8013FB20` (out-of-scope overlay target). These names and arguments are preserved through the existing call boundary.

## Instruction findings that changed the port

- `ColliderScriptLiftUpdate` uses the current rising offset as the vertical delta while the probe remains above ground; it substitutes the ground difference only when the probe reaches the surface.
- `ColliderScriptRotateUpdate` uses geometry bits `0x0F00` for the rotation plane and bits `0xF000` for forward/reverse progress. The first C version reused the lower mode for both; the port now follows the two independent masks.
- `GimmickGetGrabOffset` chooses the signed 4096-unit offset from the original collider/player comparison branches; the C sign follows those branch results.
- `StageType_Clear_Init` leaves the quotient high-word value in `$a2` for `XaPlay`; C reproduces the multiply-high value instead of passing a constant. The caller's `$a3` value is also preserved.
- `StageType_Clear_Update` uses 30-frame countdown intervals. On the zero-delay path it writes the stage-state timer field and wraps the delay to `0xFFFF`.
- `StageGoalReached` exit modes 1 and 2 succeed when the caught count is at least the alternate goal.
- `StageGradeUpdate` has a distinct level-30 jump target that writes grade 2 directly; it does not share the level 18-29 threshold path.
- `SaveComputeChecksum` awards only the highest medal tier whose time threshold is met. The platinum, gold, and silver checks are ordered alternatives in the instructions.
- `PauseDrawCaptureGrid` always stores `quotient + 106` for each tens glyph, including zero; only the ones glyph uses the `+116` branch. Its level-type label uses X=124. Both values are compared through the text-sprite inputs.
- `PauseMenuDrawSubScreen` places menu string 7 at Y=78. The previous C port used Y=180, which produced different renderer packet coordinates.

## Registered exclusions

The three pure renderer hooks above are excluded as complete bodies after their stores were audited. For the retained capture-grid, counter, and subscreen rows, packet instructions alone are intercepted; all reachable game-state and renderer-input logic executes and compares. The capture-grid's only non-packet coverage gap is its negative row-index rounding guard at 80082410: its index starts at zero and increases to the positive total, so it cannot be negative. `ColliderScriptPropTransformUpdate` excludes ratio-division zero/overflow guards because valid model dimensions and prop half sizes are positive. `SoundEmitterVolume`, `ColliderScriptRotateUpdate`, and `ColliderScriptOrbitPlayer` exclude only their signed-division break guards; the exact addresses and input-domain reasons are registered beside the checker in `tools/mips/check_exclusions.csv`.
