# Native camera host adapter

`native_camera_host.c/.h` wrap the instruction-verified `ae_camera_follow_update`
(`native_camera_follow.h`) so a host can drive it from player state. No game
bytes are embedded; the EXE image is only read in `ae_camera_host_init` (the
math tables are copied into the host, which owns them afterwards).

## API

- `ae_camera_host_init(h, exe, size, ground, collision, snapshot)`: zeroes `h`,
  loads the tables (`ae_camera_math_load`), binds the callbacks, stores the
  borrowed ground / collision pointers, sets `ready = 1` and calls reset. Any
  null argument, missing ground or collision, math load failure or reset failure
  returns 0 with `ready = 0`.
- `ae_camera_host_reset(h, snapshot)`: needs `ready`. Computes
  `Atan2Safe(-facing.vx, -facing.vz)` first; if that fails nothing is mutated.
  Then zeroes the slot and runs `ae_camera_init_follow(kind 1, zero eye8,
  zero target8, pos, facing, yaw)`, zeroes `frame.scratch` and `frame.controls`,
  snapshots, and clears `h->status` / `frame.status`. Other frame inputs
  (`current_area`, `state`, ...) are left untouched so the caller can set them
  before or after a reset.
- `ae_camera_host_snapshot(h, snapshot)`: CameraSnapshotPlayer.
- `ae_camera_host_step(h, snapshot, mode)`: see below.
- `ae_camera_can_exit_stage`, `ae_camera_player_submerged`: pure predicates.

`h->queries.user` points at the host, and the host is rebound on reset / step,
but a host should not be copied while in use.

## Snapshot layout

Little-endian byte stores, 16 bit positions wrapped:

| Destination | Source |
|---|---|
| `scratch[0x10..0x17]` | `render8` verbatim (original: player actor + 0x1C) |
| `scratch[0x40..0x47]` | `facing8` verbatim |
| `scratch[0x18/0x1A/0x1C]` | low 16 bits of `pos[0..2]` |
| `scratch[0x68/0x6C/0x70]` | full 32 bit `vel[0..2]` |
| `scratch[0x2C..0x2F]` | full 32 bit `ground_y` |
| `snapshot_shadow[0..7]`, `[8..15]` | `render8`, `facing8` (original globals 0x800E693C..0x800E694B) |

Other scratch and control bytes are not touched. The shadow is separate because
`frame.controls` ends at 0x800E691F while the original shadow lives at 0x800E693C.

The caller must pass the original render position bytes (the 8 bytes at player
actor + 0x1C) in `render8`; they are not assumed to equal the body position. The
native player has no such field yet, so any proxy must be labelled as one by the
integration.

## Callbacks and the math-failure transaction

- normalize: `ae_camera_normalize`; failure sets `AE_CAMERA_HOST_MATH_DOMAIN`
  (0x100) in `h->status` and returns 0.
- sqrt: `ae_collision_sqrt(value as signed 32, sdk_lookup = 1)`.
- ground: `ae_collision_ground_height(c, g, x, y, z, 0, -1, 0, &hit, &solid)` with
  zeroed outputs; returns the sign-extended low 16 bits of `hit.height`. The bool
  return is ignored on purpose, since the no-floor / out-of-bounds sentinel
  height is meaningful. No status is invented for existing collision errors.

A step copies slot, frame and shadow, clears `h->status` / `frame.status`,
snapshots, clears `controls[0x47]`, and calls `ae_camera_follow_update`. If the
update fails, or succeeds after a normalize failure, slot / frame / shadow are
restored, the combined bits (`h->status | frame.status`, plus
`AE_CAMERA_FOLLOW_MATH_DOMAIN` for a callback failure) are stored in `h->status`
and ORed into the restored `frame.status`, and 0 is returned. `h->status` is
authoritative; `frame.status` may keep bits from earlier failed steps. A null
snapshot or a host that is not ready sets `AE_CAMERA_FOLLOW_MISSING_CALLBACK` on a
non-null host and returns 0.

Before each step the caller must set explicitly: `current_area`,
`current_gadget`, `state`, `state_flags`, `player_mode`, `stage_entry_flags`,
`pad_held`, `right_stick_word`, `stick_x`, `stick_y`, `can_exit`, `submerged`,
`rccar_active`, `projected_sxy`, and any `controls` overrides. The step does not
overwrite them (except `controls[0x47]`, the once-per-frame guard).

## Predicates

- `ae_camera_can_exit_stage(code, state, flags)`: `code` is the byte
  `g_StageExitCode` (low 8 bits used), `state` the halfword `g_Player.state` (low 16
  bits used), `flags` `g_Player.stateFlags`. Returns 1 only when `code - 3 < 3`
  (unsigned), state is not 0x23 / 0x4A / 0x2D, and `(flags & 0xF0) != 0xE0`.
  The instructions (delay slot `li v0,1` on the `bne`) agree with the decompile.
- `ae_camera_player_submerged(active, water_y, player_y)`: `active` is the byte at
  0x800BAD7B (low 8 bits used), `water_y` the signed halfword `waterY` (low 16 bits
  sign extended), `player_y` the 32 bit `pos[1]`. Returns 1 when active and
  `water_y < (player_y - 256)` as signed 32 bit with wrapped subtraction.
- No RcCar helper: it depends on world state that is not available, so
  `frame.rccar_active` is an explicit caller input.

## Still pending

No GL / view matrix / projection (`projected_sxy` stays a caller input). Not
compiled or run by the author: the original snapshot and predicates still need
comparing and a host-world fixture needs building. Gadget-3 variant follow
remains unsupported in `ae_camera_follow_update`.
