# Player actions and control-mode setters

This batch ports the 42 entries listed in `extract/codex-task/task.md` onto the `AeRam` image. The call map below was
rebuilt from the original EXE instructions because this note was absent from the interrupted worktree.

## Call tree

### Leaf actions and setters

These entries have no out-of-batch calls in their instruction ranges:

`PlayerAct_JumpPct3`, `PlayerAct_VerticalOnly`, `PlayerAct_JumpPct2`, `PlayerAct_DriftForward`,
`PlayerAct_NegateInputXZ`, `PlayerAct_TurnAround`, `PlayerAct_Nop`, `PlayerAct_AnimSpeedNormal`,
`PlayerAct_AnimSpeedHalf`, `PlayerAct_TurnFacingLeft90`, `PlayerAct_TurnFacingRight90`, `PlayerAct_Rebound`,
`PlayerAct_JumpFromLStick` (its `0x800662B0` branch is an internal label), `PlayerAct_StepBackDrop`,
`PlayerAct_FaceStickOnRotation`, `PlayerSetClimbTarget`, `PlayerSetRope`, and `PlayerSetTintEffect`.

`PlayerSetState` calls `AnimFramePtr` only on the animation-blend path. `PlayerSetEyeFrame` calls `DR_MOVE`.

### Action-to-action calls

* `PlayerAct_TurnAroundAndJump` calls `PlayerAct_TurnAround`, then `PlayerAct_ScaleMoveVel`.
* `PlayerAct_FaceStickMaybeHalfSpeed` calls `PlayerAct_FaceStick`, then `PlayerAct_AnimSpeedHalf` when gesture bit 2 is set.
* `PlayerAct_FaceStickRotateA/B` call `PlayerAct_FaceStick`, then the validated `ApplyMatrixSV` port.
* `PlayerAct_FaceRightPerp/FaceLeftPerp` call `PlayerAct_GadgetSpinFromStick` after writing `desiredDir`.
* `PlayerAct_BounceBack` and `PlayerAct_ExitToMode0Forward` call `PlayerSetMode(0)`.
* `PlayerAct_Land` may call `PlayerSetState(0x18)` or `PlayerSetState(0x4B)`.

The remaining action hooks are `AimRequestClear`, `PlayerCond_JumpHeld`, `PadRumble`, `PlayerInput_StickGesture`,
`Gadget3AimUpdateAmmo`, `AimModeEnter`, `AimModeLeave`, `PlayPlayerVoice`, `EffectGroundImpactAtPlayer`,
`PlayerDamage`, and `PlaySfx`.

### `PlayerSetMode` dispatch

`PlayerSetMode` first handles requested mode 11 / 14 directly. Otherwise it reads `g_PlayerMode`:

* Current modes 0, 5, and 6 dispatch the requested mode through the 14-entry table at `0x800A6070` (requests 14 and
  above go to the common mode write).
* Current mode 1 exits the actor-controlled mode, updates the actor model, requests state 8, and may invoke the
  function pointer stored at `0x800B4DD0`.
* Current mode 3 exits the mounted/attached mode, updates the actor model, requests state 8, and restores gadget input.
* Current modes 2, 4, 7, and 9 use the common mode write, with a special cleanup for requested mode 13.
* Current mode 8 handles stage-exit / goal checks and can request stage transitions.
* Current mode 10 frees the streamed idle-animation model and returns to state 0.
* Current modes 11 and above use the common mode write, with the same request-13 cleanup.

The requested-mode table at `0x800A6070` maps modes 0..13 to code at `0x8007B8A0`, `0x8007B480`, `0x8007BC70`,
`0x8007B53C`, `0x8007BC70`, `0x8007B608`, `0x8007B664`, `0x8007B87C`, `0x8007B5E0`, `0x8007B7EC`,
`0x8007B750`, `0x8007BC70`, `0x8007B83C`, and `0x8007BC50`. Those blocks respectively write mode 0; enter actor
mode 1; write mode 2; enter actor mode 3; write mode 4; enter climb mode; initialize rope mode; enter swim mode; set
mode 8; enter catch mode; enter idle-animation mode; write mode 11; enter squash/fall mode; and clear the mode-13
cleanup flag.

The old-mode table at `0x800A6040` maps current modes 0..10 to `0x8007B458`, `0x8007B8C0`, `0x8007BC44`,
`0x8007BA28`, `0x8007BC44`, `0x8007B458`, `0x8007B458`, `0x8007BC44`, `0x8007BB14`, `0x8007BC44`, and
`0x8007BBE0`.

### Hooks and direct ports

Calls outside this batch remain named hooks: `AnimFramePtr`, `DR_MOVE`, `PadRumble`, `AimRequestClear`,
`PlayerCond_JumpHeld`, `PlayerInput_StickGesture`, `Gadget3AimUpdateAmmo`, `AimModeEnter`, `AimModeLeave`,
`PlayPlayerVoice`, `EffectGroundImpactAtPlayer`, `PlayerDamage`, `PlaySfx`, `StageRequest`, `EffectBigBurst`,
`StageGoalReached`, `HudPanelA_Start`, `HeapFree`, `CameraAreaScriptDispatch`, `PlayerSelectGadget`,
`PlayerIdleModelSetup`, and the indirect callback from `0x800B4DD0`.

`ApplyMatrixSV`, `VectorNormalSS`, `VecLength3`, and `IntSqrt` already have instruction-diffed ports and are called
directly. `PlayerSetState` and other entries in this batch are direct calls, not hooks.

## Validation record

The differential checker and per-function case counts are recorded in `tools/mips/check_player_act.py`,
`tools/verified.csv`, and the final section added here after validation. Gaps and their rationale are recorded in
`tools/mips/check_exclusions.csv` and below.

## Completed validation

`tools/mips/check_player_act.py` compares the 42 C functions against the original EXE on an `AeRam` image loaded from SCUS_944.23, real area member 091, and the real player state tables in member 008. The default run passes 161 cases in about 4.5 seconds. All target instructions are covered except `PlayerSetMode` 0x8007B6F0..0x8007B6F8, registered unreachable: both rope lengths are `VecLength3` results from SQR12 of signed 16 bit inputs, so each is at most 886 and their sum cannot be -1. The zero-length divide break at 0x8007B6E0 is tested as a matching MIPS trap / C failure. The direct helper coverage items `VectorNormalSS core`, `IntSqrt`, `ApplyMatrixSV`, `VectorNormalSS`, and `VecLength3` are fully covered.

Checker mutation mode injected two wrong RAM writes into each function and caught all 84 mutations. The checker registers the original stack window as excluded because the MIPS and C stack frames differ, and seeds `PlayerSetMode` caller-saved `$a1/$a2/$a3` equally because mode-1 entry passes `$a2` through to `CameraAreaScriptDispatch`.

`.venv/Scripts/python.exe tools/check_verified.py` passes. It reports missing result JSON files for other checkers in this worktree; the player action results and all 42 ledger rows are present and consistent.

### Function status and default cases

| Original function | Status | Cases |
|---|---:|---:|
| `PlayerSetState` | diffed | 14 |
| `PlayerAct_ScaleMoveVel` | diffed | 2 |
| `PlayerAct_JumpHoldBoost` | diffed | 4 |
| `PlayerAct_JumpPct3` | diffed | 2 |
| `PlayerAct_VerticalOnly` | diffed | 2 |
| `PlayerAct_JumpPct2` | diffed | 2 |
| `PlayerAct_SquashReset` | diffed | 2 |
| `PlayerAct_FlyerThrust` | diffed | 4 |
| `PlayerAct_FlyerLaunchA` | diffed | 2 |
| `PlayerAct_FlyerLaunchB` | diffed | 2 |
| `PlayerAct_GadgetSpinFromStick` | diffed | 4 |
| `PlayerAct_Gadget3Aim` | diffed | 3 |
| `PlayerAct_DriftForward` | diffed | 2 |
| `PlayerAct_NegateInputXZ` | diffed | 2 |
| `PlayerAct_TurnAround` | diffed | 2 |
| `PlayerAct_TurnAroundAndJump` | diffed | 2 |
| `PlayerAct_Nop` | diffed | 2 |
| `PlayerAct_FaceStick` | diffed | 3 |
| `PlayerAct_JumpFromLStick` | diffed | 4 |
| `PlayerAct_StepBackDrop` | diffed | 2 |
| `PlayerAct_BounceBack` | diffed | 2 |
| `PlayerAct_ExitToMode0Forward` | diffed | 2 |
| `PlayerAct_AnimSpeedNormal` | diffed | 2 |
| `PlayerAct_AnimSpeedHalf` | diffed | 2 |
| `PlayerAct_FaceStickMaybeHalfSpeed` | diffed | 2 |
| `PlayerAct_FaceStickRotateA` | diffed | 2 |
| `PlayerAct_FaceStickRotateB` | diffed | 2 |
| `PlayerAct_AimModeEnter` | diffed | 2 |
| `PlayerAct_AimModeLeave` | diffed | 2 |
| `PlayerAct_FaceRightPerp` | diffed | 3 |
| `PlayerAct_FaceLeftPerp` | diffed | 3 |
| `PlayerAct_TurnFacingLeft90` | diffed | 2 |
| `PlayerAct_TurnFacingRight90` | diffed | 2 |
| `PlayerAct_FaceStickOnRotation` | diffed | 4 |
| `PlayerAct_Rebound` | diffed | 2 |
| `PlayerAct_HurtVoiceFlag` | diffed | 2 |
| `PlayerAct_Land` | diffed | 5 |
| `PlayerSetClimbTarget` | diffed | 2 |
| `PlayerSetRope` | diffed | 1 |
| `PlayerSetEyeFrame` | diffed | 3 |
| `PlayerSetTintEffect` | diffed | 6 |
| `PlayerSetMode` | diffed | 46 |

### Remaining external hooks

| Hooked original | Role at these call sites |
|---|---|
| `AnimFramePtr` | Find the animation frame address used as the blend source in PlayerSetState. |
| `DR_MOVE` | Queue a GPU rectangle move for PlayerSetEyeFrame. |
| `PadRumble` | Request controller rumble during landing. |
| `AimRequestClear` | Clear an outstanding aim request during catch-mode entry. |
| `PlayerCond_JumpHeld` | Return the jump-held condition used by PlayerAct_JumpHoldBoost. |
| `PlayerInput_StickGesture` | Return the stick gesture bits consumed by spin and land actions. |
| `Gadget3AimUpdateAmmo` | Update gadget-3 aim ammunition state. |
| `AimModeEnter / AimModeLeave` | Enter or leave the aiming subsystem. |
| `PlayPlayerVoice` | Request the hurt voice. |
| `EffectGroundImpactAtPlayer` | Spawn the ground-impact effect. |
| `PlayerDamage` | Apply the landing fall-damage amount. |
| `PlaySfx` | Play actor / scripted-mode sounds. |
| `StageRequest` | Request a stage or area transition. |
| `EffectBigBurst` | Spawn the actor burst at its stored position. |
| `StageGoalReached` | Query whether the stage goal has been reached. |
| `HudPanelA_Start` | Start the stage-complete HUD panel. |
| `HeapFree` | Release idle-animation model buffers. |
| `CameraAreaScriptDispatch` | Dispatch area camera scripting during actor-mode entry. |
| `PlayerSelectGadget` | Run the external gadget selection state update. |
| `PlayerIdleModelSetup` | Build or clear the idle-animation model from its streamed blob. |
| `PlayerModeCallback` | Call the function pointer stored at 0x800B4DD0 when an actor exits. |

`ApplyMatrixSV`, `VectorNormalSS`, `VecLength3`, and `IntSqrt` are called through existing instruction-diffed C ports. No PlayerCond function beyond `PlayerCond_JumpHeld` is part of this batch.

### Instruction findings

- PlayerSetState reads the existing player state as the state-change discriminator. It writes only the latest state-table pointer to player scratch +0x48, resets animation lerp and speed on every call, and uses a signed frame-table remainder for negative requested states.
- PlayerAct_JumpFromLStick uses the exact sequence of shifts: movement direction times magnitude, then move speed, then jump speed percentage. PlayerAct_BounceBack and PlayerAct_ExitToMode0Forward copy the full gadget direction words into desired direction and facing before calling PlayerSetMode(0).
- PlayerSetMode entry mode 7 selects state 0x2E. The squash path subtracts 0x200 from player position Y before setting state 0x24. Actor and scripted exit paths differ in their rotation-copy sizes and update player velocity Y separately from movement velocity.
- PlayerSetEyeFrame uses the queue pointer at 0x800EC354 and writes its final partial command word through that queue pointer. The eye queue index at 0x800EC35C only selects the command buffer.
- PlayerSetMode current-mode dispatch uses a table for current modes 0, 5, and 6; current modes 11 and above follow the common mode write and request-13 cleanup. Mode 8 request 9 enters catch mode directly and skips stage-goal checks.
