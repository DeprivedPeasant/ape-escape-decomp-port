# Native camera follow (CameraFollowUpdate)

`native_camera_follow.c/.h` port `CameraFollowUpdate` (0x800495F4) and its
helpers `CameraSlotUpdateYaw` (0x80048DD0) and `CameraSlotAimAtFocus`
(0x80047360) from the original instruction listings (the MIPS is the
authority; Ghidra types, casts and uninitialised-variable artifacts are not).
It reuses the committed `native_camera` slot accessors, `stick_orbit`,
`native_camera_math` (atan / sin / cos tables) and
`native_camera_collision` (`ground_clamp`, `ground_clamp_near`) without
changing them.

**Status: written without a compiler or tests.** Nothing here has been built,
run or compared against the original yet. Codex is expected to compare the full
slot / frame bytes and the query call order against the original MIPS.

## Interface

`ae_camera_follow_update(slot, frame, tables, queries, mode)` returns 1 on
success and 0 on an explicit missing or unsupported input. There is no global
state; everything the original reads or writes outside the slot is in
`AeCameraFollowFrame`:

| Field | Original |
|---|---|
| `scratch[0x80]` | PSX scratchpad 0x1F800000..7F, bytewise little endian |
| `controls[0x50]` | globals 0x800E68D0..691F (index = address - 0x800E68D0) |
| `state_flags` / `state` / `player_mode` / `current_area` / `stage_entry_flags` | 0x800EC250 (word) / 0x800EC23E, 0x800F44A0, 0x800F4476 (halfwords, low 16 used) / 0x800F4450 (word) |
| `current_gadget` | signed byte 0x800EC2D2, low byte taken as int8 |
| `pad_held`, `right_stick_word` | g_PadHeld; word 0x800E54A0 (low 16 = g_RStickMag, full 32 tested for `== 0`) |
| `stick_x`, `stick_y` | signed bytes 0x800E53C2 / C3 |
| `projected_sxy` | RotTransPers output of the previous-view projection of (scratch 0x10, 0x1A - 0x80, 0x14) |
| `can_exit`, `submerged`, `rccar_active` | return values of PlayerCanExitStage, PlayerIsSubmerged, CameraRcCarActive |
| `status` | `AE_CAMERA_FOLLOW_*` bits ORed on failure (never cleared by the port) |

Scratch words used (offsets from 0x1F800000): 0x00/02/04 desired eye (halfword
stores only, the pad at 0x06 is kept), 0x08/0A/0C desired target, 0x10/14/1A
player position, 0x18/1A/1C ground-clamp focus (**separate** from the player
position, taken from the scratch bytes, not derived), 0x2C ground reference,
0x40/0x44 direction words, 0x60/0x64 projection (low 16 unsigned / high 16
signed), 0x68/6C/70 movement words, 0x74/0x78 AimAtFocus cross / dot results
(full 32 bits; Ghidra's PlayerScratch field names for these are misleading).

The caller clears `controls[0x47]` once per frame (the original's DAT_800E6917
guard). If it is already nonzero the call returns 1 and changes nothing;
otherwise it is set to 1.

## Callbacks and math

`queries->normalize` (VectorNormalSS), `queries->sqrt` (SquareRoot0, signed 32
bit result, the argument is the wrapped 32 bit sum and can look negative) and
`queries->ground` (GetGroundHeight, result truncated to s16 like the original)
are the original query routines supplied by the caller and are called in the
original order with the original arguments (normalization input vectors are
copied before the call and normalized in place). `AeCameraMathTables` supplies
atan2 / sin / cos. The `normalize` return value is only used where the
original uses it (inside `ground_clamp_near`).

All negation, multiply, add and subtract are wrapped 32 bit. Shifts are floor
shifts; the original's "bgez / addiu (2^n - 1) / sra n" idiom is
`trunc_shr` (division truncating toward zero), and the `% 4096` idiom is
`mod4096` (remainder keeps the sign of the dividend). Halfword stores keep the
low 16 bits. The only real divide (`yawDelta * (0x40 - n) / (0x41 - n)`, n in
1..0x30) goes through a checked divide that reports `MATH_DOMAIN`.

## Failure contract

Preflight (before any write): a null slot / tables / queries, unloaded tables or
a null `normalize` / `sqrt` / `ground` gives `MISSING_CALLBACK`; a mode other
than 0 or 1 gives `UNSUPPORTED_MODE`. Then the guard check (returns 1 if set).
Then the gadget check: `CameraFollowGadgetVariant` is not ported, so a current
gadget of 3 (signed byte) with `controls[0x46] != 1` returns 0 with
`UNSUPPORTED_GADGET`.

The coordinator's note described the unsupported case as `controls[46] == 0`;
the instructions (0x8004A25C..0x8004A26C) call the variant for every value
other than 1, so the port fails closed for all of those (a strict superset).

Runtime failures (atan outside its domain, a divide trap) restore the slot,
scratch and controls from a snapshot taken after the guard check and return 0
with `MATH_DOMAIN`. `ground_clamp*` can only fail on missing callbacks, which
the preflight already excludes.

## What is ported

Both modes 0 and 1 in full: the eye / target / distance overrides, the stage
mode 1 override (`controls[0x48]`), UpdateYaw, the mode 1 still / hold
counters, StickOrbit, the yaw-delta damping (rc-car and still-frame variants),
clamp-state decay, follow distance smoothing, yaw and mode-timer height blend,
the three-step ground probe, the held-behind-wall check, hit and still-vertical
velocity handling, desired target / eye, both `ground_clamp_near` passes, the
follow-distance recovery, target and eye smoothing (including the fixed-active
eye copy: 8 bytes of scratch 0x00..0x07 go to slot 0x0A..0x11), `ground_clamp`
and the final distance / last-position / saved-yaw update. The fixed-active
target branch uses halfword stores, as in the instructions.

`ae_camera_slot_update_yaw` and `ae_camera_slot_aim_at_focus` are public. UpdateYaw
copies the 8 scratch bytes 0x40..0x47 to slot 0x64..0x6B. The halfword sums
used for the probe positions keep only their low 16 bits, so the original's
logical-vs-arithmetic shift of negative step values does not matter.

## Not implemented / pending

- `CameraFollowGadgetVariant` (0x8004AF78): fails closed as above.
- Hardware projection (GTE RotTransPers) and the player / world query
  integration: the caller supplies `projected_sxy` and the callbacks.
- Renderer wiring.

## Validation still required

Not run. Codex should compare slot / scratch / controls bytes and the order and
arguments of normalize / sqrt / ground calls against the original MIPS for
modes 0 and 1, and exercise each branch (rc-car, hit, held, fixed-active,
states 0x2D / 0x40 / 0x50, area 7).
