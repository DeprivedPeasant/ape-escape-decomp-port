# Native camera ground clamp helpers

`native_camera_collision.c/.h` add two leaf helpers on top of the existing
`native_camera.h` slot model (`AeCameraSlot`, 0xB0 bytes, little-endian
accessors; `native_camera.c` is not modified). No dependencies beyond C99.

This is **not** a full camera or hardware query implementation. The original
`VectorNormalSS`, `SquareRoot0` and `GetGroundHeight` are supplied by the caller
through `AeCameraQueries`; this module only reproduces the two helpers' own
instruction sequences.

## Provenance

Written from the original private exports `80048f6c_CameraSlotGroundClamp.c/.asm.txt`
and `8004906c_CameraSlotGroundClampNear.c/.asm.txt` (not included in this
repository). Where the decompile and the instructions differ, the instructions
were followed. No code from external reference projects was used.

## Callbacks (`AeCameraQueries`)

| Callback | Original | Contract |
|---|---|---|
| `normalize(user, int16_t io[3])` | VectorNormalSS 0x800301E0, in place | Output any signed 16 bit values; returns the squared-length bits (`uint32_t`). |
| `sqrt(user, uint32_t)` | SquareRoot0 0x800303C0 | Signed 32 bit result. |
| `ground(user, x, y, z)` | GetGroundHeight 0x80043400 | Wrapped signed 32 bit coordinates; result truncated to signed 16 bit by the helper. |

Both functions return 1 when executed and 0 if the slot, the scratch pointer,
the query table or a required callback is missing; in that case no slot byte is
written and no callback is called. `ae_camera_ground_clamp` requires
`normalize` and `ground`; `ae_camera_ground_clamp_near` requires `normalize`,
`sqrt` and `ground`.

## `ae_camera_ground_clamp` (0x80048F6C)

`focus` = scratch halfwords 0x1F800018 / 1A / 1C.

1. Vector = `(s16)(focus[0] - eye[0])`, **current eye Y** (not 0), `(s16)(focus[2] - eye[2])`.
2. `normalize` in place; the return value is ignored.
3. Query `ground(eyeX + (vx >> 5), eyeY, eyeZ + (vz >> 5))` (floor shifts of the
   signed 16 bit outputs, 32 bit wrap); `g` = result as signed 16 bit.
4. If `g - 0x80 < eyeY < g + 0x110`: if `focusY < g + 0x100`, eye Y = `g - 0x80`;
   then if (re-read) eye Y `< focusY - 0x400`, eye Y = `focusY - 0x400`. Stores
   keep the low 16 bits.

## `ae_camera_ground_clamp_near` (0x8004906C)

`desired_eye` = scratch halfwords 0x1F800000 / 02 / 04.

1. Vector = `(s16)(desired[0] - eye[0])`, 0, `(s16)(desired[2] - eye[2])`; `normalize`
   in place returns `len_sq`.
2. `root = sqrt(len_sq)` (signed 32 bit); only an upper clamp to 0x40 is applied,
   there is no lower bound.
3. Products `v * root` are the low 32 bits (wrapped, signed). Two queries in this
   order, both at eye Y: `eye + (product >> 10)` then `eye + (product >> 11)`
   (floor shifts, 32 bit wrap); each result is truncated to signed 16 bit.
4. If either result `< desiredY - 0x100` (signed 32 bit threshold):
   in this order: if `near != 0 && clampState (0x34) == 0` then yaw (0x40) = savedYaw (0x44);
   clampFlag (0x37) = 0; clampCount (0x36) += 1 unless it is already 255 (the
   instructions use an unsigned byte compare, `sltiu 0xff`; the decompile's
   `-1` on a signed byte is a mistyping); clampTimer (0x35) = 6.
5. Otherwise: if clampTimer == 0, clampCount = 0; if stillFrames (0x30) == 0 and
   clampFlag != 255, clampFlag += 1.

## Validation

Strict GCC compilation passes. check_camera_collision.py compares4000original-instruction cases,6000orderedground queries andtwo missing-callback cases, zero failures. Full176slot bytes, normalize input vectors, square-root arguments and query coordinates match. Callbackresultsare explicitly supplied to isolate the helpers; exact math/world adapters andfullfollowremainpending. Details are generated in validation/native_camera_collision_validation.md.
