# Native viewer indicator proximity (event/animation prototype)

Phase: native viewer integration, reviewed and tested by Codex.
This is an **event/animation prototype**, not the complete gimmick protocol.

## Usage

```
uv run --offline python tools/codex/build_native_viewer.py --indicator-proximity [--area 0x01] [--run]
native_viewer.exe area.aev ... --indicators-pack indicators_runtime_AA.aei --indicator-proximity
```

`--indicator-proximity` implies `--native-player` (and `--world-indicators` in the build script, which then
packs with `runtime=True`). The viewer requires an `AE_NATIVE_PLAYER` build, native player mode and an AEIND2
runtime indicator world; this is validated before `--check` returns. Without the flag every launch is unchanged,
including the manual `H` hint preview and the independent 30 Hz indicator accumulator.

## Evidence for ordering

The complete Mode_Stage listing orders: PlayerUpdate (call 8005C12C), IndicatorsUpdateDraw (8005C200),
GimmickUpdateAll (8005C230), CollisionUpdate (8005C26C; conditions and player modes can suppress it).
So proximity sees the player after PlayerUpdate but before body integration, and the indicator animation
advances before proximity. The renderer uses the model-frame outputs cached by that animation tick,
so a subsequent activation does not change the pose already selected for this frame.

## Wiring

- `ae_player_host_step_input(h,pad,callback,user)` (`pad` is an `AePlayerHostPad { held, rx, ry, lx, ly }`, see src/app/native_player_host.h) runs the former step body and calls the
  observer right after `ae_player_update` and before `ae_player_body_step`. A callback returning 0 stops the
  body step and frame counter and the step returns 0 (the player update has already happened; not a rollback).
  Pass NULL / NULL for no callback. The callback must not mutate the player.
- Callback: `ae_indicator_world_tick`, then `ae_indicator_world_proximity_step(world,pos,0,ae_ui_state()!=0,NULL,0,...)`.
- Exactly one callback per 30 Hz `original_player_tick` (walk test and input catch-up included), not per render.
  When the debug actor pause returns early, the callback still runs once with the current position. The
  viewer's own indicator accumulator is disabled in this mode, so the world is never ticked twice.

## Host proxies and unknowns

- Stage exit code 0 and message-busy (`ae_ui_state()!=0`) are explicit host proxies.
- `touchedBy` is unknown and never inferred from grid or player contact; the library reports a
  missing-touch mask instead. Colliders are static (no collider motion or contact).
- Position fed is the pre-body-integration player position of that tick.

## Requests

Executed: the local activation animation (applied inside the proximity step) and `PlaySfx` via `ae_audio_play_effect`.
Not executed (counted only): hint display (bit 1), pickup spawn (bit 2), effect 0x112 (bit 4). No player mode
change, stage/CD/progress. The missing hint protocol is therefore explicit.

Logs (stderr): request detail only when a tick returns requests; missing-touch mask only when it changes
(initial 0 is silent); the unexecuted-request mask warning once per newly set bit.

## Trace fields (only with the flag, `--walk-test`)

`indicator_proximity_ticks`, `indicator_tick_position[3]`, `indicator_missing_touch_mask`,
`indicator_request_mask` (cumulative OR), `indicator_activation_requests`, `indicator_hint_requests`,
`indicator_pickup_requests`, `indicator_effect_requests`, `indicator_trigger_states` (trigger state s16 per
entry via `ae_indicator_world_runtime_record`). `indicator_frames` reflects animation and event changes; `indicator_render_frames` records the
cached pre-activation model frames used by ae_indicator_world_draw_stepped.

## Validation

Strict C11 player build and540repeated SDL walking frames across areas01/05/15 pass.
Each logical tick runs once; its world position equals the preceding integrated position
in these supported walking scenarios. A12frame diagnostic at an actual marker center
emits the correct sector14 hint request once, reports unknown repeat-contact dependence,
and keeps model frame1 for drawing while the activation sets the raw slot frame29.
Legacy/missing-runtime launches are rejected before --check succeeds. See
validation/indicator_proximity_demo_validation.md and check_indicator_proximity_demo.py.

Hint display/player-mode/stage/CD/progress/pickup/effect protocols remain unexecuted.
No console capture parity. The marker-center start is a controlled diagnostic, not an
original gameplay capture or route.


Later legacy world/follow regressions stalled during graphics initialization, including a
serial retry and previously committed default/player binaries. Closing our emulator did
not restore startup. The new integration suite passed before this failure; subsequent full
regression remains incomplete.
