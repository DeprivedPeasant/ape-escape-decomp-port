# Collision body probes

`src/bodies/ae_probe.c` ports five original EXE functions as structured C over the byte-exact `AeRam` image. The module does
not change `ae_bodies.c`; the owner can connect the functions from the existing body hooks.

## Integration

Include `src/bodies/ae_probe.h` where the host supplies `AeBodyHooks`. In the `build_probe_points(user, ram, type, body)`
callback, call `ae_CollisionBuildProbePoints(ram, type, body)`. This is the original path used by collision-body types 6,
7, 0xC, and 0xD. It expects the area loader to have installed the collision grid, shape, collider, count, and probe-buffer
globals.

The `body_move` callback owns the original player / RC-car movement cases. Where that callback implements the original
`BodyProbeGround` step, call `ae_BodyProbeGround(ram, world_hooks, body)`. It calls the existing
`ae_GetGroundHeight(ram, world_hooks, x + velocity.x, y + step, z + velocity.z)` directly and writes the found probe height
back to `body->velocity.y` when the ground lies below the probe. The function returns zero, matching the original.

`GetSolidHeight` at 8004D8C0 remains a host-provided `AeWorldHooks.GetSolidHeight` callback inside `ae_GetGroundHeight`.
The probe functions have no other external calls. The differential checker supplies a deterministic `GetSolidHeight` hook
with ordered call logging and RAM / scratchpad side effects.

## Differential status

`tools/mips/check_probe.py` loads the real EXE and collision plus terrain members for areas 0x01, 0x05, 0x17, 0x48, and
0x65. It checks all main RAM below the stack window, every scratchpad byte, return values, and the `GetSolidHeight` call
sequence. Default cases include every probe body type, real populated and empty grid columns, signed grid edges, real
collider centers and expanded box borders, bodies outside the grids, and ground probes around terrain boundaries and
velocity thresholds. One grid case changes a selected real shape's half-height in the RAM fixture to exercise the
negative-height rounding arm.

| Original function | Address | Cases | Original instructions executed |
|---|---:|---:|---:|
| `CollisionBuildProbePoints` | 8007304C | 195 | 224/228 (4 declared unreachable) |
| `CollisionPointInGrid` | 8004F6E8 | 193 | 100/103 (3 declared unreachable) |
| `CollisionShiftProbePoints` | 800733DC | 48 | 29/29 |
| `PointInBoxXYZ` | 80073AA0 | 150 | 49/49 |
| `BodyProbeGround` | 80072D28 | 150 | 39/39 |

The seven excluded PCs are recorded in `tools/mips/check_exclusions.csv`. The two builder default jumps at 800731AC..800731B0
and 800731C4..800731C8 handle quadrant values outside 0..3; the quadrant is formed from two sign bits and the code handles
each possible value directly. The grid adjustments at 8004F778, 8004F790, and 8004F7A8 add 511 only for negative
coordinates: the strict grid bounds make `x + halfX` and `z + halfZ` positive, and positive `y` returns before `-y` is
computed. The `--coverage` run reports 441/448 instructions executed; all other target instructions ran.

The checker passed 736 default differential cases in 31.8 seconds. Its `--mutate` suite changes eight behaviors in each of
`CollisionShiftProbePoints`, `PointInBoxXYZ`, `CollisionPointInGrid`, and `BodyProbeGround`, plus nine in
`CollisionBuildProbePoints`; all 41 variants were caught. Results are recorded in `extract/checks/results/check_probe.json`,
and all five functions are in `tools/verified.csv` as `diffed`.

## Findings

- `CollisionBuildProbePoints` writes `1` to the probe-status byte when a collider is disabled or does not overlap the body;
  an overlapping active collider leaves the byte at zero. This is the inverse of the first natural reading of the helper's
  zero-on-overlap return value.
- The body-type 1 path writes `0xFFFF` at collider offset `+0x32`. `tools/structs.txt` names the 0x38-byte collider fields
  only through `+0x2A`; the new header calls this descriptive field `AE_PROBE_COLLIDER_BODY_MARKER` pending a project-wide
  name decision.
- The checker registers no external call beyond the existing `GetSolidHeight` world hook. The body and build hooks remain
  the integration points in `AeBodyHooks`; this task leaves their wiring to the project owner as requested.
