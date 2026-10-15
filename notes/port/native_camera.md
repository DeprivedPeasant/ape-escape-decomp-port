# Native camera leaf routines

`native_camera.c/.h` hold portable C99 versions of the small camera slot
routines of the original game. They have no dependencies, no floating point and
no game data. The full follow camera is not part of this module.

## Slot model

`AeCameraSlot` is exactly the original 0xB0-byte slot as `uint8_t raw[0xB0]`.
All fields are read and written through little-endian helpers
(`ae_camera_u16/i16/i32`, `ae_camera_set16/set32`), so there is no packed struct
or alignment dependence. The helpers take a byte offset (`size_t`); the caller
must make sure the whole access fits inside 0xB0 bytes (not checked). Setters
store the value modulo 2^16 / 2^32. Offsets used are listed in the header
(`AE_CAM_*`) and come from the original instruction accesses.

## Routines and original sources

| Function | Original | Notes |
|---|---|---|
| `ae_camera_reset_follow` | CameraSlotResetFollow 0x800493D4 | The caller passes the already computed `Atan2Safe(-facingX, -facingZ)` as `original_yaw`. Eye = pos - facing>>1, target = pos + facing>>2, stored as the low 16 bits. The shifts are floor (arithmetic) shifts on negative values, like the original `sra`. |
| `ae_camera_clear_script` | CameraSlotClearScript 0x8004C2E4 | Clears only slot bytes. The original also zeroes two globals (0x800AF130 / 0x800AF131); those are outside the slot and the integrator must clear them. |
| `ae_camera_init_follow` | CameraSlotInitFollow 0x80047104 | Sets heights, copies 8 bytes each (padding included) from the caller's eye / target blocks to offsets 0x0A / 0x12, sets dist, clears 0x2C and orbit X. Kind 1 and 3 call reset, kind 4 and 5 clear the script, other kinds do nothing more. The caller reads the eye / target source blocks from the original globals. |
| `ae_camera_smooth_eye` / `_target` | CameraSlotSmoothEye 0x80047464, SmoothTarget 0x80047558 | Per axis `(cur * (div - 1) + desired) / div` in wrapped 32-bit arithmetic, quotient truncated toward zero, result stored as signed 16 bit modulo 2^16. |
| `ae_camera_stick_orbit` | CameraSlotStickOrbit 0x8004923C | See below. |

### Division traps

The original executes `break` for a zero divisor and for `INT32_MIN / -1`. The
C version returns 0 in both cases instead of dividing, and returns 1 otherwise.
Axes processed before the failing one keep their new value; the failing axis
and later ones are unchanged, as in the original order of stores.

### Stick orbit

Nothing is written if `can_exit` is false. The stick is considered only when
`fixed_active == 1` and `stick_mag > 100`; otherwise both stick values count as
0. `stick_x` is the byte at 0x800E53C2 and `stick_y` the byte at 0x800E53C3
(the original's two `lb` loads). Orbit Y is cleared, then it is evaluated only
if stick X is nonzero or both clamp state and mode timer are zero. Priority:
held 0x2000 or positive X gives +0x22; otherwise held 0x8000 or negative X gives
-0x22; otherwise nothing is written (orbit Y stays 0). When set, still frames
and clamp flag are cleared. Orbit X is then cleared; held 0x1000 or positive Y
sets -0x30, limited by `player_y - 0x404`; held 0x4000 or negative Y then
overrides with +0x30, limited by `player_y + 0x100`. The limit comparisons use
the 16-bit signed wrap of the sums, as the original does.

## Validation status

Reviewed and integrated by Codex after the Sonnet delivery. Strict GCC C11 build passes. check_native_camera.py compares all176slot bytes against original executable instructions in6400cases, zero failures. Coverage includes500resets/500initializations/3000orbits/2000smooth operations/300zero-divisor partial-write cases/100slot script clears. Details and limits are generated in validation/native_camera_validation.md. Original atan helper and exit-permission return values are explicitly supplied; fullfollow, camera collision, event/region modes and renderer integration are separate work.
