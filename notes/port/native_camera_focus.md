# Native camera player direction and focus point

`native_camera_focus.c/.h` implement two leaf helpers on top of the existing
`native_camera.h` slot type (`AeCameraSlot`, 0xB0 bytes). They read the slot
bytes directly, so `native_camera.c` is not required. No dependencies beyond C99.
They enable a motion-callback host without any host or player change.

## Provenance

Written from the original exports `8004b880_CameraGetPlayerDir.c/.asm.txt` and
`8004b8f4_CameraSlotGetFocusPoint.c/.asm.txt` (private, not in this repository).
Where decompile and instructions differ, the instructions were followed. No code
from external reference projects was used.

## API

```c
int ae_camera_get_player_dir(int32_t mode, const uint8_t facing8[8], uint8_t out8[8]);
int ae_camera_get_focus_point(const AeCameraSlot *slot, int32_t mode,
                              const int32_t player[3], const uint8_t facing8[8],
                              const uint8_t region_focus8[8], uint8_t out8[8]);
```

Return 1 on success; 0 with no write if a required pointer is NULL. Only inputs
the selected mode reads are required (and `out8` always).

## `ae_camera_get_player_dir` (0x8004B880)

`facing8` = `g_Player.facing` (8 bytes at player +0x60). The mode is compared as
a signed 32 bit value (`beq 1`, `slti 2`, `beq 0`, `beq 2`).

| mode | effect | requires |
|---|---|---|
| 0 | `out8[0..7] = facing8[0..7]` (lwl/lwr/swl/swr pairs, full 8 bytes) | `facing8`, `out8` |
| 1, 2 | `out8[0..5] = 0` (three `sh zero`); bytes 6, 7 **preserved** | `out8` |
| any other (incl. negative, >= 3) | no write, success | `out8` |

## `ae_camera_get_focus_point` (0x8004B8F4)

Signed mode dispatch as above (`beq 1`, `slti 2`, `beq 0`, `beq 2`).

| mode | effect | requires |
|---|---|---|
| 0 | writes `out8[0..5]` only (below); bytes 6, 7 **preserved** | `slot`, `player`, `facing8`, `out8` |
| 2 | `out8[0..7] = region_focus8[0..7]` (scratch 0x800E68C0, full 8 bytes) | `region_focus8`, `out8` |
| 1, any other | no write, success; all 8 bytes preserved | `out8` |

Mode 0, with `dist` = s16 slot+0xA4 (focusDist), `height` = u16 slot+0xA6
(focusHeight), `dir` = player dir mode 0 (full copy of `facing8`):

- `out.x = low16(player[0]) + low16(sra(s16(dir[0..1]) * dist, 12))`
- `out.y = low16(player[1] - 0x80) + height`, low 16 bits
- `out.z = low16(player[2]) + low16(sra(s16(dir[4..5]) * dist, 12))`

The product of two s16 values is at most 2^30 in magnitude, so the 32 bit `mult`
cannot overflow; the shift is arithmetic (floor), implemented without relying on
implementation-defined signed shifts. Sums and stores keep the low 16 bits
(unsigned arithmetic, no UB). The internal dir call writes a local buffer and
does not touch `out8` (the original does not reset the output padding either).

All inputs (slot fields, player, facing, region focus) are snapshotted before the
first write, so `out8` may alias any input.

## Padding and initialization

The original never initializes all 8 bytes of the output: mode 0 leaves bytes
6, 7 alone and mode 1 writes nothing. This module therefore does **not** claim
the original initializes them. In the original, the output is a stack buffer
whose padding is whatever was there; for the reference tests an adapter should
preinitialize the focus buffer with zero padding (original stack zero means
padding 0 in reference tests), but that is **not runtime-proven**. Codex's
full-original diff test must compare all 8 bytes, with the output preseeded with
a known pattern to prove the preservation.

## Validation

Not built or run in this worktree (shell unavailable). Still required: strict
compile, and an original-instruction differential covering all modes (negative,
0, 1, 2, 3, INT32_MIN/MAX), extreme `player`/`facing`/`focusDist` values
(negative products, floor shift of negative values), preseeded output padding,
and aliasing (`out8 == facing8`, `out8 == region_focus8`, output overlapping the
slot).
