# Player animation events and hit bodies

Status: both original functions are hand-ported to the `AeRam` image and pass the differential checker
`tools/mips/check_anim_events.py`.

| Original function | C function | Default cases | Instruction coverage | Result |
|---|---|---:|---:|---|
| `PlayerRunAnimEvents` `80064454` | `ae_PlayerRunAnimEvents` | 361 | 352/353; one proven unreachable | diffed |
| `PlayerAnimEventAddHitBodies` `8007C468` | `ae_PlayerAnimEventAddHitBodies` | 8 | 108/108 | diffed |

The checker loads SCUS-94423, area 0x01 member 05B at `0x80154000`, and real player animation member 008 at
`0x80103000`. It executes the original instructions and the C implementation from the same RAM and scratchpad
images, compares all non-stack RAM, scratchpad, and hook-call sequences, and uses the GTE oracle for
`ApplyMatrixSV`. The default run has 369 total cases and takes about two seconds in the current environment.

## Event behavior

Records start with `{state, relativeFrame, kind, argument}`. Kinds 2 and 6 have 12 payload bytes; kinds 3 and 7
have four; other kinds have no payload. The event fires only when the player state matches and
`state.firstFrame + relativeFrame == player.animFrame`, with `player.animLerp == 0`.

| Kind | Original behavior |
|---:|---|
| 1 | Calls `PlaySfx(argument)`. |
| 2 | Allocates a type-6 body at a transformed bone offset, with radius and facing data from the payload. |
| 3 | Allocates a type-8 body at player position plus a transformed bone offset. The radius is the signed payload byte times eight. |
| 4 | Calls the function indexed by `argument` in `g_PlayerEventFns` at `0x800B027C`. |
| 5 | Calls the function indexed by `argument` in `g_PlayerActionFns` at `0x800B03AC`. |
| 6 | Calls `PlayerAnimEventAddHitBodies`, which attempts two type-6 bodies. |
| 7 | Allocates a type-8 body at the signed short position `0x800F5180`; its radius comes from payload byte 3, not the header argument. |
| 8 | Calls `PlayPlayerVoice(argument)`. |

Kind 3 at state `0x80`, frame 9 in the Monkey Net set allocates the catch body. The body position adds the
bone matrix transform of the three signed payload bytes multiplied by eight to the player position. Kinds 3
and 7 skip allocation while the global mode at `0x800F44A2` is 11 or player health is nonpositive. After
registration they set body parameters to `0x100` and zero.

`PlayerAnimEventAddHitBodies` interpolates the points at `0x800F5154` and `0x800F515C`: the first body uses
`(A + 7*B) >> 3`, radius `radius + 0x20`; the second uses `(5*A + 3*B) >> 3`, radius `radius`. Both copy
player facing, store the payload parameters, and register as type 6. The function also writes the two body
addresses at `0x800F516C` and `0x800F5170` before registration; those stores are the delay slots of the
original register calls.

## Hooks left open

- `PlaySfx` `800877F8` and `PlayPlayerVoice` `8006896C` remain audio hooks. The audio port owns their playback.
- Kind 4 / 5 function pointers go through `AeAnimEventHooks.event_callback` with the resolved EXE address.
  The host dispatches the associated player event or action; this batch does not port those callback bodies.
- `printf` `800A3720` is provided through `AeBodyHooks.printf_error` when the 256-entry collision-body slot table is full.
  It emits the original `Collision_set Error` diagnostic. Body allocation and registration themselves call the existing C ports directly.

## Validation

The default differential run executes every real member 008 event at its exact frame, one frame before and
after, and at nonzero interpolation; it also traverses each full real event stream. Synthetic cases cover
kinds 0 through 9, including kinds 5 and 7 absent from the real member, health/mode gates, slot-table-full,
pool-full and one-body-free states, randomized bone transforms, and signed coordinate extremes.

`PlayerRunAnimEvents` executes 352 of 353 listed instructions. `800644D4` cannot execute because the
preceding `lbu` loads the event state byte into `v1`; the negative-only instruction after `bgez v1` is
unreachable. Its proof is registered in `tools/mips/check_exclusions.csv`. The stack window
`801FF000..80200000` is excluded because the MIPS and C frames differ; the transform vector is initialized
before the C port reads it.

The mutation suite tests 42 deliberate source changes across both functions; all 42 are detected. The
per-function default case counts are appended to `tools/verified.csv`. `tools/check_verified.py` scans the
new `src/player/ae_anim_events.c` file for ledger consistency.
