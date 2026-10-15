# Native viewer: stage camera integration (`--stage-camera`)

Opt-in wiring of the validated `native_camera_stage` (see `native_camera_stage.md`,
`validation/camera_stage_validation.md`) into `native_viewer.c` / `build_native_viewer.py`.
The worker had no shell, build or game data. Codex independently reviews and validates below.

## Scope labels (shown at startup, in the title and in usage)
Stage camera **prototype**: local regions / scripts / blend. Still **incomplete**: global camera
events, indexed handlers except id3 and area05/id11, selected aim / fixed slots (kind 2 / 3), flight / RC car /
boss dispatch, full GTE projection. The screen projection used for `projected_sxy` stays the
explicit float approximation (focal 328, offset 192/120, previous-value fallback).

## Flags and build
- `--stage-camera` implies `--native-camera` (and so `--native-player`). In
  `build_native_viewer.py` the implication is applied before the player-source selection, and the
  flag is forwarded to the viewer. The viewer itself sets all of the same flags when parsing.
- `--native-camera` alone is unchanged: follow-only, `ae_camera_host_step`.
- `CAMERA_MODULES` now also links `native_camera_bank, _regions, _select, _blend, _focus, _script,
  _motion, _molten, _lifecycle, _stage` (`native_camera_math` stays in `ALWAYS_MODULES`). These sources are
  part of the build digest only in player builds, as before; default builds are unchanged.
- Without `AE_NATIVE_PLAYER` the flag is an unknown option (usage), like `--native-camera`.
- The existing `--yaw/--pitch/--distance/--target` rejection applies (the stage camera owns the view).

## Runtime wiring
- `camera_stage` is a zero-initialised global next to the global `follow_host`
  (`queries.user` stays `&follow_host`).
- Init (`native_camera_init`): host first (`ae_camera_host_init`), then the area member given as
  `--ground` (the actual extracted member) is read once with `camera_read_file` into a
  viewer-owned buffer (`stage_blob`, <= 64 MiB), then `ae_camera_stage_init(blob, size, snapshot)`.
  Failure is a clear error (`stage camera initialization failed (status 0x..)`); stage and blob are freed.
- Reset (Home / F while following): `ae_camera_host_reset`, then `ae_camera_stage_free` and
  `ae_camera_stage_init` on the same blob (seeded from the freshly reset host slot). The warning
  state and the stage status are cleared.
- Step: `native_camera_step` is unchanged up to the host call (frame inputs, previous-view
  projection, camera before player, prior render cache). With the stage requested it calls
  `ae_camera_stage_step(..., is_flight_area = 0)` instead of `ae_camera_host_step`. On success
  `camera_apply_orbit` uses `follow_host.slot` as before.
- Cleanup (`graphics_free`): stage free, then the blob, before the player host, ground and collision.
- `V` inspect toggle semantics are unchanged.

## Failure policy
`stage->status` is kept in the separate `camera_stage_status` (not folded into the legacy
`camera_status`, which keeps the projection-fallback bit and `follow_host.status`).
- Only `UNSUPPORTED_HANDLER` (0x2) and/or `UNSUPPORTED_MODE` (0x8): the stage already rolled
  everything back, so the prior view is preserved. The status is recorded, one stderr warning is
  printed per **changed** status value, the run continues. No fallback camera is invented.
- Anything else (`MISSING`, `MATH_DOMAIN`, `INVALID`, or status 0): fatal error, like the legacy
  follow failure.
- A successful step or a reset clears the status and the warning state (so a later recurrence warns again).

## Flight areas
The only flight classification in the repo is the `tools/symbols.csv` comment for `IsFlightArea`
(0x8004A940): "True in player mode 2 in areas 0x48-0x4C". The original instructions at8004A940..8004A974 confirm this mode2 predicate; the
viewer stays in mode0. Rejecting these area ids is a conservative prototype policy, not an exact port of that predicate. The viewer therefore **rejects areas 0x48-0x4C** at stage init
with an error and always passes `is_flight_area = 0`. Flight, RC car and boss camera dispatch are
**not supported**; no flight support is claimed.

## Diagnostics
- Capture report (`<capture>.ppm.json`): `"stage_camera": bool` always; when the stage is active
  also `stage_camera_status`, `stage_slot` (selected bank slot, u16 control 0x04),
  `stage_slot_kind` (slot byte 0) and `stage_region` (control 0x0D, the current region index).
  Each of the three is -1 when unavailable; `stage_region` is -1 for 0xFF / out of the region count;
  `stage_slot` is range-checked against the 12-slot bank.
- Walk trace frames: same fields, only while the follow camera is active.
- No per-frame console output; only the once-per-changed-status warning above.

## Validation still required (Codex)
- Strict `-Wall -Wextra -Werror` build, default and player builds (`--player-source-dir` / `--native-player`).
- Existing `--native-camera`, `--native-player`, default `--check`, `--capture`, `--walk-test` runs unchanged
  (the new JSON key `stage_camera:false` is the only addition to their output).
- `--stage-camera --check` on all areas; flight areas 0x48-0x4C must report the rejection.
- Real camera-area captures / walk traces: the selected slot, kind and region change as expected;
  confirm whether the shipped areas ever hit `UNSUPPORTED_*` (warning appears once).
- Walk-test determinism and reset (Home) with regions / persistent kind 5 slots.

## Blockers / caveats
- The original flight predicate was instruction-checked; area rejection is an explicit conservative host policy.
- On an `UNSUPPORTED_*` non-fatal failure the player keeps running with a frozen camera view.

## Codex validation

Strict default/player builds pass. check_stage_camera_demo.py covers1080 repeated
deterministic SDL walk frames at actual entries in areas01/02/03/05/15, zero failures.
Area05 now enters regions0/1 and selects script slots of kinds4/5, without held-view
frames or missing-handler warnings. Other sampled areas select kind1; area02 enters
region1. Native traces are integration checks, not original gameplay comparisons.
The scoped original stage checker separately compares2940 full bank/control ticks,
including30actual overlay18D handler calls. Follow-only540-frame/frame-order/manual-
override regression passes. Approximate projection and omitted global dispatch remain.


Launch: `python tools/codex/build_native_viewer.py --stage-camera --world-indicators --area 0x01 --run`. Existing follow-only launch remains `--native-camera`.
