# Camera update port

This batch ports the 26 functions in `extract/codex-task/task.md` as readable RAM-image C in `src/camera/ae_camupd.c` and `ae_camupd.h`. Each entry takes `AeRam *`; shared host and SDK boundaries are named in `AeCamupdHooks`. Existing camera and stage ports are called directly.

## Call map

The per-frame root is `CameraUpdate` (80047BD0):

```text
CameraUpdate
├─ CameraSnapshotPlayer -> CopySvecAt1C
├─ [when camera state allows] CameraRegionUpdate
│  ├─ CameraRegionTest
│  ├─ CameraSlotRemove (existing camera port)
│  ├─ CameraSelectMode
│  └─ EventCamCreate
├─ CameraSelectMode
│  ├─ CameraSlotSnapshotEye / CameraActivateSlot (existing camera ports)
│  ├─ IsFlightArea
│  └─ VectorNormalSS (validated SDK port)
├─ for each scripted slot: CameraScriptRun
├─ for scripted camera kinds: CameraSlotUpdate
│  ├─ CameraSlotGetFocusPoint / CameraSlotRemove / CameraSlotSnapshotEye
│  │  (existing camera ports)
│  ├─ CameraGetPlayerDir / Atan2Safe / rsin / rcos
│  └─ EventCamTrigger
├─ per-slot CameraFollowDispatch / CameraKindAimUpdate / CameraKindFixedUpdate
│  (existing camera ports)
├─ CameraApplySlot
├─ CameraShakeApply
├─ EventCamUpdate
│  ├─ EventCamLoadRecord
│  └─ StageExitRequest -> StageRequest (verified stage port)
├─ StageExitSequenceUpdate
│  ├─ CameraSlotPlaceBehindPlayer (existing camera port)
│  ├─ EventCamStart -> EventCamLoadRecord / CutsceneModeOff
│  ├─ CutsceneModeOn
│  └─ ScreenFlashDraw -> RenderScreenFlash hook
├─ CameraTransitionFrameDraw -> RenderCameraTransition hook
├─ CameraLookAt hook
└─ ScreenFadeUpdate -> ScreenFlashDraw
```

Other batch edges: `EventCamTrigger` calls `StageRequest`, `EventCamCreate`, and `PadRumble`; `EventCamCreate` calls `CameraGetEye`, `CameraSlotCreate`, `EventCamSetScript`, and, when requested, `CameraActivateSlot`. `EventCamStart` loads a record and exits cutscene mode. `EventCamUpdate` either loads the next record or requests stage exit. `EventCamLoadRecord` uses the validated `RotMatrixZYX` SDK port. The leaves `CameraGetEye`, `CameraGetPlayerDir`, `CameraApplySlot`, `CameraShakeApply`, `CameraRegionTest`, `CopySvecAt1C`, `IsFlightArea`, `CutsceneModeOn`, and `CutsceneModeOff` update RAM directly; `Atan2Safe` calls validated `ratan2` and `CameraSnapshotPlayer` uses `CopySvecAt1C`.

`CameraScriptRun` is the game's camera-script command interpreter. Its switch implements commands 1 through 26 with structured C. It directly uses the already ported slot-focus and slot-management routines and dispatches trigger command 22 through `EventCamTrigger`. It calls the verified `VectorNormalSS`, `rsin`, and `rcos` ports for the script geometry. Fixed-point angle conversion and signed shifts retain the original truncation and wrap behavior.

## Open boundaries

The native runtime must supply these callbacks; the checker records their arguments and return behavior:

| Hook | Original address | Boundary |
|---|---:|---|
| `PadGetState` | 8002B52C | Reads controller state for `PadRumble`. |
| `memset` | 800A3740 | SDK memory fill used for temporary camera and region vectors. |
| `Printf` | 800A3720 | Reports invalid camera-script commands and event-record indices. |
| `SpawnParticle` | 800698B8 | Spawns the particle requested by an event-camera trigger. |
| `CameraAreaScriptDispatch` | 8004B590 | Dispatches the area-specific camera behavior reached through camera updates. |
| `CameraLookAt` | 80046670 | Applies the eye/target look-at transform and updates camera matrix state; receives eye, target, view matrix, rotation, and roll. |
| `RenderScreenFlash` | renderer replacement | Builds and submits the screen-flash renderer packet. |
| `RenderCameraTransition` | renderer replacement | Builds and submits the selected transition image/quad. It receives the decremented timer, ordering-table cursor, image address, and display mode. The C port retains the game timer/cursor updates in RAM. |

`EventCamTrigger` and `StageExitRequest` call the verified `StageRequest` port directly. Its stage-host boundaries remain named in `AeStageHooks`: `PlayerSetMode` changes player modes, `StageIntroStart` starts the intro, `XaPlay` requests XA audio, and `StageTypeClearInit` initializes the clear-stage flow. The pure-maths calls (`ratan2`, `rsin`, `rcos`, `VectorNormalSS`, and `RotMatrixZYX`) go directly to their verified SDK ports.

## Validation

`tools/mips/check_camupd.py` compares the native C result with the original instructions on identical RAM images. Each default scenario uses a real area member, rotating among 091, 092, and 093, and compares return values, hook-call sequences, main RAM below the original stack window, and the full scratchpad. There are 32 scenarios per function (832 total). The default run completed in under a minute with zero mismatches. All 26 ledger rows are `diffed` with `min_cases=32`.

| Function | Executed / total instructions | Function | Executed / total instructions |
|---|---:|---|---:|
| PadRumble | 40/40 | CopySvecAt1C | 10/10 |
| Atan2Safe | 15/15 | ScreenFlashDraw | 0/58 |
| ScreenFadeUpdate | 21/21 | CameraSelectMode | 185/185 |
| CameraGetEye | 14/14 | CameraUpdate | 111/111 |
| CameraSnapshotPlayer | 58/58 | CameraApplySlot | 178/180 |
| CameraShakeApply | 85/85 | IsFlightArea | 14/14 |
| CameraGetPlayerDir | 29/29 | EventCamTrigger | 51/51 |
| EventCamCreate | 34/34 | CameraRegionTest | 98/98 |
| CameraRegionUpdate | 241/241 | CameraScriptRun | 470/470 |
| CameraTransitionFrameDraw | 0/142 | EventCamLoadRecord | 142/154 |
| EventCamStart | 28/28 | StageExitSequenceUpdate | 186/186 |
| StageExitRequest | 18/18 | EventCamUpdate | 53/53 |
| CutsceneModeOn | 4/4 | CutsceneModeOff | 3/3 |

The checker reports 2,088/2,302 instructions executed. The 214 remaining instruction slots are registered in `tools/mips/check_exclusions.csv`:

- `ScreenFlashDraw` (58 instructions) and `CameraTransitionFrameDraw` (142) are renderer-owned ordering-table and packet construction, replaced by the named renderer hooks.
- `CameraApplySlot` has two unreachable signed-rounding instructions (800480D0 and 80048100): the multiplier is read from an unsigned halfword timer.
- `EventCamLoadRecord` has 12 unreachable divide-trap instructions (8004D1B0, 8004D1C8, 8004D1F0, 8004D208, 8004D234, 8004D24C, 8004D2B0, 8004D2C8, 8004D2F4, 8004D30C, 8004D334, 8004D34C): duration zero is normalized to one, and signed-halfword endpoint differences cannot reach the signed-division overflow case.
- The common stack window, 801FF000..80200000, is excluded because original MIPS frames differ from host C frames; all other main RAM and the full scratchpad are compared.

Two deliberate changes per function were tested. All 52/52 mutations were caught. The checker writes `extract/checks/results/check_camupd.json` on its default run.

## Findings

- The instruction path for `CameraApplySlot` loads the interpolation timer as unsigned, so negative-rounding branches shown in some decompilation views cannot run.
- `EventCamLoadRecord` forces a zero duration to one before deriving eye and rotation deltas. This both preserves the observable timer and makes its registered signed-division trap paths unreachable.
- Script command 26 derives its direction from the selected focus point, normalizes the horizontal eye-to-focus vector, then applies signed Q12 distance scaling. It is not a direct copy of the player's facing direction.
- Renderer hooks receive game-computed state and values; the native renderer still needs to implement packet generation and matrix/render integration at those boundaries.
