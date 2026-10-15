# Missa function batch

Pass 8 integration strengthens the final PlayerInit case with an empty boot
header. This exposed cached auxiliary-count/table reads: the original reloads
them after each StaticModelInit because its outputs can alias those fields.
The port now retains those reads and the actual final loop index. All 1,280
cases pass; the other 31 PlayerInit cases still use the real member 008.
See game_module_bugs.md for instruction evidence and the old-source rejection.

The requested 48 entries split into 40 game/state functions ported as readable RAM-image C and eight pure renderer
boundaries left as named hooks. The default differential run compares 32 cases per ported function against the
original MIPS code using real EXE/area data and a 2,000,000-instruction budget per oracle call. It passed 1,280 cases
with no mismatches. The checker executed 2,145 of 2,333 reported words; the remaining words are registered renderer
blocks or proven unreachable instructions.

## Differential results

All 40 ledger rows are `diffed`, with 32 cases each. The mutation run covers each row twice. Coverage is executed
words / original span words; the registered exclusions are listed below and in `tools/mips/check_exclusions.csv`.

| Address | Function | Coverage | Source |
|---|---|---:|---|
| 80010D80 | AreaCheckExits | 204/204 | `src/world/ae_missa_area.c` |
| 800115F4 | AreaUsesAltMember | 51/51 | `src/core/ae_missa_core.c` |
| 80013128 | PadDemoPlaybackStep | 77/77 | `src/core/ae_missa_core.c` |
| 800140B4 | StickToButtons | 26/26 | `src/core/ae_missa_core.c` |
| 80015B30 | ModelInitNormals | 28/28 | `src/core/ae_missa_core.c` |
| 8001F8B0 | TrailRingPush | 41/41 | `src/player/ae_missa_player.c` |
| 8001F954 | TrailRingDraw | 94/145 | `src/ui/ae_missa_renderer.c` |
| 8003272C | PlayerInit | 236/236 | `src/player/ae_missa_player.c` |
| 80032D38 | PlayerPlaceAtEntry | 60/60 | `src/player/ae_missa_player.c` |
| 80046978 | ScreenFadeSet | 6/6 | `src/core/ae_missa_core.c` |
| 80047E8C | CameraRadarFocusSelect | 21/21 | `src/camera/ae_missa_camera.c` |
| 80047EE0 | CameraRadarFocusUpdate | 68/68 | `src/camera/ae_missa_camera.c` |
| 80048398 | AimRequestStart | 38/38 | `src/camera/ae_missa_camera.c` |
| 8004898C | BossAimSfx | 24/24 | `src/camera/ae_missa_camera.c` |
| 8004AA58 | CameraRcCarFollow | 132/139 | `src/camera/ae_missa_rc_car.c` |
| 8004B54C | CameraGetSlotTarget | 13/13 | `src/camera/ae_missa_camera.c` |
| 8004B590 | CameraAreaScriptDispatch | 27/27 | `src/camera/ae_missa_camera.c` |
| 8004BBF0 | EventCamStartVictory | 27/27 | `src/camera/ae_missa_camera.c` |
| 8004BC5C | EventCamStartDefault | 16/16 | `src/camera/ae_missa_camera.c` |
| 8004D73C | StageExitStart | 25/25 | `src/camera/ae_missa_camera.c` |
| 8004D880 | CallIndexedHandler | 14/14 | `src/camera/ae_missa_camera.c` |
| 800509CC | ZakoSaveDefeatedMask | 76/76 | `src/world/ae_missa_zako.c` |
| 80054A08 | ZakoClearDefeatedBits | 45/45 | `src/world/ae_missa_zako.c` |
| 8005DD44 | PlayTimeAdd | 13/13 | `src/core/ae_missa_core.c` |
| 8005F68C | HudDrawTimeRankIcon | 34/57 | `src/ui/ae_missa_hud.c` |
| 80060448 | HudDrawMonkeyInfo | 161/203 | `src/ui/ae_missa_hud.c` |
| 800650D8 | PlayerInput_StickGesture | 4/4 | `src/core/ae_missa_core.c` |
| 80066EEC | PlayerAttachSet | 18/18 | `src/camera/ae_missa_camera.c` |
| 80066F34 | PlayerSetBounceFlag | 7/7 | `src/camera/ae_missa_camera.c` |
| 800698E4 | SpawnEffectGroupAt | 15/15 | `src/effects/ae_missa_spawn.c` |
| 8006AA60 | BreakableFinalizeVisualUpdate | 44/44 | `src/effects/ae_missa_spawn.c` |
| 8006C58C | PlayerFxJumpBurst | 70/70 | `src/effects/ae_missa_spawn.c` |
| 8006CE9C | PlayerFxSpawn116 | 21/21 | `src/effects/ae_missa_spawn.c` |
| 8006CFBC | PlayerFxSetMatrixPos | 34/34 | `src/effects/ae_missa_spawn.c` |
| 800717B4 | Indicator_Activate | 145/145 | `src/world/ae_missa_indicator.c` |
| 80073B64 | PointInBoxXZ | 25/25 | `src/core/ae_missa_core.c` |
| 8007774C | DirIndexFromFlags | 24/24 | `src/core/ae_missa_core.c` |
| 8007A14C | FadeFrame | 10/10 | `src/core/ae_missa_core.c` |
| 8007CDFC | TargetListFindBest | 86/86 | `src/player/ae_missa_gadget.c` |
| 8007F40C | PadDirectionCode | 85/85 | `src/camera/ae_missa_camera.c` |

`TrailRingDraw` now runs its original control flow in C. Its lifetime field is decremented by 32 for each rendered
segment and cleared at the same points as the original. The checker runs the original function and replaces only the
GTE projection and packet/ordering-table link block with a per-segment renderer callback. It covers 94/145 words;
50 words in `8001FA48..8001FB0C` are renderer code, and `8001FB74` is a proven-dead tail word. The loop cases exercise
empty, short, boundary (`31/32/33`, `63/64/65`), and chained ring lifetimes.

Its store audit separates the five stack saves (`8001F964, 8001F96C, 8001F970, 8001F974, 8001F978`), ring lifetime
writes (`8001FA44, 8001FB24, 8001FB28`), packet/OT stores (`8001FA78, 8001FA90, 8001FA94, 8001FAAC, 8001FAB0,
8001FAC8, 8001FACC, 8001FAD0, 8001FAD4, 8001FAD8, 8001FAF0, 8001FAF4, 8001FAF8, 8001FB04, 8001FB08,
8001FB0C`), and final renderer cursor store (`8001FB78` to `800EC354`). Thus the ledger covers its state updates;
only GTE projection and GPU/OT packet output cross the native renderer boundary.

## Renderer hooks without ledger rows

These eight original functions have no game-state stores. Their stores target only stack scratch, GPU packet data,
ordering-table links, and renderer primitive cursors. Their `unreachable` rows in `check_exclusions.csv` record the
instruction-by-instruction store audit. No C wrapper function is retained for them; the caller-facing named hooks
remain in the HUD/menu hook tables. The menu C caller already computes the active selection, positions, and cursor
phase passed to those renderers.

| Address | Function | Store audit (original PCs) |
|---|---|---|
| 80060224 | HudDrawStatPips | `8006022C, 80060240, 800602C8, 800602CC, 800602D0, 800602D4, 800602D8, 800602DC, 800602E0, 800602E4, 800602E8, 800602EC, 800602F0, 800602FC, 80060300, 80060328` |
| 80060334 | HudDrawFrameBox | `80060394, 80060398, 8006039C, 800603A0, 800603A4, 800603C4, 800603DC, 800603E8, 800603EC, 80060420, 80060424, 80060430, 80060434, 80060444` |
| 8007D71C | DrawMenuChrome | `8007D760, 8007D794, 8007D7A8, 8007D7E8, 8007D81C, 8007D82C, 8007D854, 8007D860, 8007D870, 8007D87C, 8007D890, 8007D89C, 8007D8AC, 8007D8BC, 8007D8D0, 8007D8E0, 8007D8EC, 8007D8F8, 8007D904, 8007D910, 8007D91C, 8007D928, 8007D934, 8007D940, 8007D94C, 8007D958, 8007D964, 8007D970` |
| 8007D9F0 | DrawMenuRows | `8007D9F8, 8007DA00, 8007DA04, 8007DA10, 8007DA28, 8007DA34, 8007DA3C, 8007DA4C, 8007DA58, 8007DA5C, 8007DA68, 8007DA6C, 8007DA80, 8007DA90, 8007DA9C, 8007DAA4, 8007DAC8, 8007DACC, 8007DAD0, 8007DAD4, 8007DAD8, 8007DADC, 8007DAE0, 8007DAE4, 8007DB08, 8007DB0C, 8007DB10, 8007DB14, 8007DB18, 8007DB1C, 8007DB20, 8007DB24, 8007DB30, 8007DB3C, 8007DB60` |
| 8007E030 | DrawMenuHighlight | `8007E040, 8007E058, 8007E05C, 8007E068, 8007E0E8, 8007E11C, 8007E12C` |
| 8007E1B8 | DrawMenuItem | `8007E1C4, 8007E1D4, 8007E1E0, 8007E1EC, 8007E23C, 8007E270, 8007E280` |
| 8007E3F8 | DrawMenuSelection | `8007E444, 8007E478, 8007E488, 8007E48C, 8007E498, 8007E4C8, 8007E4D8, 8007E4E4, 8007E4F0, 8007E538, 8007E54C, 8007E558, 8007E590, 8007E5C4, 8007E5D4` |
| 8007F148 | DrawMenuCursor | `8007F150, 8007F15C, 8007F16C, 8007F17C, 8007F194, 8007F1A4, 8007F1B4, 8007F1C4, 8007F1DC, 8007F1EC, 8007F234, 8007F244, 8007F254, 8007F260, 8007F298, 8007F2CC, 8007F2DC` |

For the HUD builders, the C `HudDrawMonkeyInfo` logic supplies the pip row/count and frame-box coordinates; the native
renderer chooses packet colors and emits the packets. For the menu builders, the packet/color/geometry stores are
renderer output, while the parent menu port computes selection and cursor phase before invoking the named hooks.

## Other registered coverage ranges

- `HudDrawTimeRankIcon` has 34/57 covered words; its 23-word ordering-table/GPU packet block at `8005F70C..8005F764`
  is intercepted by the native renderer hook and registered as `HudDrawTimeRankIcon packet block`.
- `HudDrawMonkeyInfo` has 161/203 covered words; its 42-word badge packet block at `800605C8..8006066C` is replaced
  by `RendererHudMonkeyBadge` and registered as `HudDrawMonkeyInfo badge packet block`.
- `CameraRcCarFollow` has seven proven-dead words: `8004AB3C`, `8004AB4C..8004AB54`, `8004AB9C`,
  `8004ABB4..8004ABB8`. Earlier clamps ensure the projected-depth divisor is positive and the timer is 1..40.
- In `TrailRingDraw`, `8001FB74` is unreachable: any path reaching the loop test at `8001FB6C` has just loaded a
  positive previous-record lifetime at `8001FB64`, so that test branches back. Both no-predecessor exits jump directly
  to `8001FB78`.

The checker compares RAM outside the declared MIPS stack window and compares all scratchpad bytes. The checker writes
the 32-case results in `extract/checks/results/check_missa.json`. The mutation run catches two deliberately wrong RAM
stores per diffed function; the final result is 80/80.

## Unnamed-entry bounds and remaining hooks

Disassembly and call behavior identify `PlayerSetBounceFlag` (`80066F34`, 28 bytes) and
`BreakableFinalizeVisualUpdate` (`8006AA60`, 176 bytes). The six pause-menu packet entries and the two pure HUD packet
builders remain renderer hooks with no ledger rows, as audited above. `TrailRingDraw` keeps a C state port and calls
the native renderer once per active segment.

Remaining native/SDK hooks include the eight pure renderer boundaries, per-segment `TrailRingDraw` packet emission,
`HudDrawTimeRankIcon` packet emission, `RendererHudMonkeyBadge`, adjacent `ModelTransformRigid` / `ModelDrawEnvMap`
paths, and platform/audio/CD services. `BeginFrame` owns native ordering-table setup; `RenderScreenFlash` and
`RenderFadeSpiral` replace packet output in the existing fade port. `SpuWaitIdle` remains the sound-thread boundary.
`ClearWords`, diagnostics, text width, message boxes, sequence/audio operations, and CD position/control retain named
service arguments. `PlayerAuxModelInit`, `PropVisualUpdate`, and `StageTypeClearInit` remain named game hooks.

`CameraAreaScriptDispatch` and `CallIndexedHandler` preserve indirect dispatch because their targets come from
area/script tables. Already verified functions reached by this batch are called directly, including `ae_PlaySfx`,
`ae_EventCamCreate`, `ae_StageExitRequest`, `ae_CameraFixedModeOff`, `ae_BossKindForArea`, `ae_GetLevelFirstArea`,
`ae_ZakoTypeIsIgnored`, and `ae_FadeSpiralDraw`. `tools/check_verified.py` was not modified.

## Findings

- Camera aim requests use the area set's boss kind before entering aim mode. Event-camera victory/default setup
  creates its camera slot first, then applies the fixed-mode and guard changes; victory queues sounds 9 and 10 in order.
- `PadDirectionCode` scans all eight direction masks and keeps the last matching direction before comparing the
  accumulated pause-menu code to the five action entries.
- Stage-exit action 25 clears event-camera running/frame state before making the stage-clear request. The direct call
  through `ae_StageExitRequest` was differentially checked with the stage-clear callback at the host boundary.
- `PlaySfx` is game sound logic and is called directly by task functions; only its `SpuWaitIdle` and diagnostics remain
  host callbacks.
- All MIPS calls use an explicit 2,000,000-instruction limit.
