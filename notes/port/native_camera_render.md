# Native camera render adapter and viewer follow-camera prototype

`native_camera_render.c/.h` are a pure, stateless host adapter between the original
camera slot (`AeCameraSlot`, PSX units, Y negative-up) and the viewer's OpenGL orbit
camera. `native_viewer.c` uses it for the opt-in `--native-camera` mode.

This is a **prototype**, not a port of the original camera system. Two things are kept
strictly apart:

1. **Verified original follow instructions.** `AeCameraHost` / `ae_camera_follow_update`
   (`native_camera_host.md`, `native_camera_follow.md`) reproduce `CameraFollowUpdate`
   mode 0 and its helpers against the original instructions. Their eye / target slot
   values are the camera state of this prototype.
2. **Host approximations.** Everything in this file and in the viewer glue: render
   orbit conversion, the approximate projection, the frame
   input proxies. None of it is tested against original captures. No camera projection
   parity is claimed.

Still missing: the original stage / region / script camera event dispatcher, the
`CameraLookAt` GTE / root-matrix / roll path, gadget 3 follow variant, RC car camera.
The native viewer is not a complete port.

## API (`native_camera_render.h`)

```c
int ae_camera_render_orbit(const AeCameraSlot *slot, float target[3], float *yaw,
                           float *pitch, float *distance);
int ae_camera_project_approx(const AeCameraSlot *slot, const int16_t point[3], float focal,
                             float offset_x, float offset_y, uint32_t *sxy);
```

### `ae_camera_render_orbit`

With `t = (x, -y, z) / 256` of the slot target, `e` likewise for the eye, `delta = e - t`:

- `target = t`, `distance = |delta|`
- `yaw = atan2(-delta.x, delta.z)` degrees
- `pitch = atan2(delta.y, hypot(delta.x, delta.z))` degrees

The viewer modelview is `Scale(-1,1,1) * Translate(0,0,-distance) * Rx(pitch) * Ry(yaw) *
Translate(-target)`. Solving "eye maps to the view origin" gives exactly the formulas
above, so the mirrored-X handedness of the existing renderer is unchanged and no
`gluLookAt` is introduced. The original pitch can go beyond the viewer's inspection clamp
of 85 degrees; the adapter accepts the open range (-90, 90).

Fail closed: a null argument, zero distance, zero horizontal distance (straight up /
down) or a non-finite result returns 0 and writes **nothing**.

### `ae_camera_project_approx`

Float version of the original `CameraLookAt` basis, taken from the original function's
matrix rows (the decompile was used as evidence for the basis only, not copied):

```
d = target - eye              (PSX units, Y negative-up, no 256 scale)
if d.x == 0 && d.z == 0: d.z = 4096     (original sets vz = 0x1000 before both normalisations)
hx, hz = normalize(d.x, d.z)            vy = d.y / |d|     h = hypot(d.x/|d|, d.z/|d|)
rows: [hz, 0, -hx]   [-hx*vy, h, -vy*hz]   [h*hx, vy, h*hz]
view = rows * (point - eye)             depth = view[2]
X = offset_x + focal * view[0] / depth  Y = offset_y + focal * view[1] / depth
```

Depth must be greater than `1e-3`, otherwise it returns 0 without writing. X and Y are
truncated toward zero, clamped to the original screen range [-1024, 1023] and packed
`(X & 0xFFFF) | (Y & 0xFFFF) << 16`. `focal` must be positive and finite, offsets finite.

It deliberately does **not** reproduce GTE fixed point reciprocal rounding, SDK
normalisation (`VectorNormal`, `SquareRoot0`), the roll rotation, the root / `g_CameraRotMatrix`
composition or the `CompMatrix` with the global matrix. Treat results as approximate.

## Viewer integration (`--native-camera`)

Requires a build with `AE_NATIVE_PLAYER`; without it the flag is a usage error.
`build_native_viewer.py --native-camera` implies `--native-player` (and the player demo)
and forwards `--native-camera` to the check, capture and run invocations. Default builds
and runs are unchanged.

```
python tools/codex/build_native_viewer.py --native-camera --area 0x01 --run
```

`--native-camera` is rejected together with `--yaw`, `--pitch`, `--distance` or
`--target` (usage error): the follow slot owns the camera. Follow is never enabled
automatically.

### Controls with follow enabled

| Input | Effect |
|---|---|
| Arrow keys | Virtual camera stick: LEFT / RIGHT = -64 / +64 `stick_x`, UP / DOWN = -64 / +64 `stick_y` |
| Right mouse drag | Accumulated mouse delta added to the stick (integer, clamped to +-127, consumed each 30 Hz tick) |
| Gamepad right stick | Outside a 0.18 dead zone, `axis * 127` added to the stick |
| Mouse wheel | Ignored (the original update controls the follow distance) |
| V | Toggle original follow / inspection (only when the host is ready and player demo is active). Disabling leaves the last orbit state for inspection; enabling snaps back to the slot |
| HOME / F | Existing reset / focus, then the follow slot is reset from the player (not reset every frame) |
| WASD, Shift, Space | Unchanged native player controls; P toggles the player demo and disables follow when entering inspection. Press P and then V to resume follow |

Stick orientation is a natural-guess mapping. The original pad nomenclature has
ambiguities, so no input parity is claimed. Pending mouse input is cleared on focus loss,
on V and on P. While the actor is paused the camera does not advance.

With follow disabled (V) the previous inspection controls apply unchanged.

### Per-tick data flow (`original_player_tick`, 30 Hz, before the player / body step)

1. Build the snapshot (see anchor evidence) and the follow frame inputs.
2. Project with the **previous** slot eye / target to get `projected_sxy`.
3. `ae_camera_host_step(mode 0)`. Failure is a real error (`fail()` / viewer exit 1); there
   is no silent fallback.
4. `ae_camera_render_orbit` writes `camera.target / yaw / pitch / distance`, and
   `base_distance = distance`.

At startup the host is initialised after the player host (the EXE bytes are read with a
bounded `stdio` helper, up to 16 MiB, and freed after `ae_camera_host_init`; the host
keeps its own copy of the math tables). The orbit is applied once, before the `--check`
return so check mode validates the camera host, and again after `reset_camera` /
`focus_actor` and before graphics initialisation.

### Host proxies (all documented, none original)

| Field | Value |
|---|---|
| `render8` | Cached pre-collision body position from the previous draw phase, plus zero padding. Original Spike first-bone offset is zero and mode-0 ModelBuildBoneMatrices flags 3 writes that world-bone translation to actor+0x1C. This supported anchor is validated by check_camera_demo.py; it is separate from current body position |
| `facing8` | The four halfwords of `AePlayer.facing` |
| `pos`, `vel`, `ground_y` | `AePlayer` values |
| `state`, `state_flags` | `AePlayer` values |
| `current_area` | `scene.area` |
| `current_gadget` | `(uint8_t)AePlayer.current_gadget`; gadget 7 (RC car) fails closed with an error because the viewer has no RC car |
| `player_mode` | 0 (the player module is the mode 0 update) |
| `stage_entry_flags` | 0 (native host loading events are absent) |
| `can_exit` | `ae_camera_can_exit_stage(3, state, flags)`. `3` is an explicit normal-stage policy because `g_StageExitCode` is unmodelled |
| `submerged` | `ae_camera_player_submerged(active, water_y, pos.y)` with `active = (surface_type >> 12) == 1` from `ae_collision_surface_type` at the player: a proxy for the original water-active byte |
| `rccar_active` | 0 |
| `pad_held` | 0 (no verified pad-bit mapping for camera orbit; the stick values carry the input) |
| `right_stick_word` | Unsigned 16 bit stick magnitude, `round(hypot(stick_x, stick_y))` clamped to 127 |
| `stick_x`, `stick_y` | Signed, as in the controls table |
| `projected_sxy` | `ae_camera_project_approx(prev slot, (render x, body y - 0x80 mod 16 bits, render z), focal 328, offset 192 / 120)`. 328 and 192 / 120 are host defaults for a nominal 384x240 PSX frame; the OpenGL 55 degree perspective and window aspect are kept and are **not** identical |

If the approximate projection is invalid the previous valid `projected_sxy` is used
(initially `192 | 120 << 16`), viewer status bit `0x200` is set for that tick and a
warning is printed once. This is an external host bit, separate from the
`AE_CAMERA_FOLLOW_*` / `AE_CAMERA_HOST_*` bits. It is expected on the first tick when the
reset eye / target are not yet in front of the body.

### Output

Startup prints the follow mode, the body-position proxy, the approximate projection and
that event dispatch is pending. The window title lists V and the orbit controls (a UI
stage-name title still takes precedence).

With follow enabled the `--walk-test` trace adds per frame `camera_eye` (3 x s16),
`camera_target` (3 x s16), `camera_yaw` (slot s32), `camera_distance` (slot s32) and
`camera_status` (uint32). The `--capture` JSON always has `native_camera`, and with follow
enabled also raw `camera_eye`, `camera_target` and `camera_status`. This is viewer
walk-test output, not player capture tooling.

## Validation status

See generated validation/native_camera_render_validation.md and validation/native_camera_demo_validation.md for Codex independent geometry/build/frame-order/native-walk checks. Sonnet authored the initial adapter in an isolated worktree; Codex reviewed all changes, corrected camera/draw/body ordering from original instructions and ran the checks.

## Limitations

- Stage / region / script camera dispatcher missing; only follow mode 0 runs.
- Pad bits, stage exit code and water-active byte are host proxies. The cached render anchor is supported for Spike mode 0 / matrix flags 3; swimming/additive root transforms are separate.
- Projection is approximate and unverified; the OpenGL frustum is not the PSX projection.
- No roll, no root matrix, no GTE rounding.
- Gadget 3 follow variant and RC car are unsupported (the first fails inside the follow
  update, the second fails closed in the viewer).
- Actor presentation currently draws after the integrated player/body step. Complete original draw/collision phase ordering remains separate; camera snapshot ordering is preserved.

## Original snapshot ordering

Mode_Stage instructions at 0x8005C0EC, 0x8005C12C and 0x8005C26C call CameraUpdate, PlayerUpdate and CollisionUpdate in that order. The viewer camera steps before movement/body integration; afterward the pre-collision position is cached for the next snapshot. ModelBuildBoneMatrices at 0x80015688..0x800156AC copies first-world-bone translation to actor+0x1C/1E/20. The real Spike first bone is offset (0,0,0), parent 0, so for mode 0 / flags 3 the anchor equals the draw-phase body position. Initial spawn initializes the cached anchor. Frame traces include camera_body_at_update and camera_render_at_update; the integration checker verifies their distinct frame lags. This does not claim the rest of the native actor draw timing matches the console.
