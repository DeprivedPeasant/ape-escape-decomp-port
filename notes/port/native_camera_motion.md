# native_camera_motion: CameraSlotUpdate (0x8004CA70)

`ae_camera_slot_update(slot, player, stereo, tables, queries)` ports the whole
function from the instruction listing. Not validated by this worker (no build,
no original data); see "Validation still required".

## Flow (assembly addresses)

1. `0x8004CA90` `fixedActive` (0x33) = 0, always, even if the timer is 0.
2. `lh 0x88` timer: 0 returns (no dependencies needed at all); > 0 is
   decremented (low 16 bits stored); negative is left alone.
3. `lbu 0x03` scriptActive != 0: `queries->indexed(user, tmp, id)` on the
   slot already updated by steps 1-2, then return (`0x8004CAB0`).
4. `queries->focus(user, tmp, slot[0x91], F)` once, F = sp+0x10 (8 bytes);
   E = sp+0x18 = copy of the 8 eye bytes at 0x0A..0x11 (padding included).
5. Switch on `slot[0x90] - 2` (unsigned < 8, i.e. states 2..9). Any other
   state, and states 3, 4, 6, pass through to step 6 (F = focus, E = old eye).
6. `0x8004CE28`: `stereo != 0` (byte 0x800AF131): F.y += 3, E.y -= 6
   (16 bit wrap).
7. F is stored to 0x12..0x19 (target), E to 0x0A..0x11 (eye), 8 bytes each,
   so padding bytes pass through unchanged.

## States

| state | block | result |
|---|---|---|
| 2 | 0x8004CB24 | second focus query (same mode 0x91) replaces all of E |
| 5 | 0x8004CB7C | E.x = F.x + sra12(lo32(rcos(yaw)*dist)), E.z = F.z + sra12(lo32(rsin(yaw)*dist)); both use the old yaw/dist. yaw = v - (trunc(v/4096) * 4096) with v = yaw + s16[0x8C] + 0x1000 (wrapping, truncate toward zero); E.y += u16[0x9E]; dist += s16[0x9C]. F unchanged |
| 7 | 0x8004CB3C | E.x/y/z += s16 at 0x9C / 0x9E / 0xA0 (16 bit wrap). F unchanged |
| 8 | 0x8004CC2C | see below; fixedActive stays 0 |
| 9 | 0x8004CD40 | like 8, but E = full 8 bytes at 0xA8..0xAF; fixedActive = 1 |

States 8 and 9 first compute from the OLD eye (before it is recomputed):
F = (player.lo16[0] - E.x, player.lo16[1] - (E.y + 0x80), player.lo16[2] -
E.z), 16 bit wrap, then SDK `VectorNormalSS` (0x800301E0) in place. Then:

- state 8: E.x = s16[0xA8] + trunc((player[0] - s16[0xA8]) / 4) using the FULL
  32 bit player position (wrapping subtraction); E.z likewise with s16[0xAC]
  and player[2]; E.y keeps the old eye Y.
- state 9: E = anchor bytes 0xA8..0xAF verbatim (all 8).
- both: F.axis = E.axis + trunc((F.axis + s16[0x94 + 2*axis]) / 2) with the
  normalized F and the NEW E (16 bit wrap).

## Dependencies and failure (returns 0, slot unchanged)

The slot is copied, updated, and written back only on return 1.

- `slot` null: 0.
- timer 0: needs nothing (queries / tables / player may be NULL).
- scriptActive: needs `queries->indexed` only. It returning 0 fails the call.
- everything else needs `queries->focus` (returning 0 fails the call).
- states 5, 8, 9 need `tables` loaded; 8 and 9 also need `player`.
  Normalize returning 0 (squared length above INT32_MAX, where the original's
  add traps) fails the call.
- state 2 calls focus again; other states need nothing further.

## Callback responsibilities

- `focus` is the original CameraSlotGetFocusPoint (0x8004B8F4): this module
  does not implement it, and expects all 8 output bytes initialised. It gets
  the temporary slot (timer already decremented, fixedActive cleared).
- `indexed` is the original CallIndexedHandler (0x8004D880); it may modify
  the temporary slot arbitrarily; the result is committed on nonzero return.

## Offsets used (slot)

0x03 scriptActive, 0x0A eye (8), 0x12 target (8), 0x33 fixedActive, 0x38 dist,
0x40 yaw, 0x88 timer, 0x8C scriptRate, 0x90 state, 0x91 focus mode, 0x94
scriptFocus s16[3], 0x9C scriptVel s16[3], 0xA8 anchor (8). Local enum only;
shared headers are unchanged.

## Validation still required

- The jump table at 0x800A4D90 is not available here. The state to block
  mapping (2, 5, 7, 8, 9) and "3, 4, 6 and >9 pass through" are taken from the
  decompile and the assembly layout; verify the table words.
- Differential run against the original instructions for every state,
  stereo on/off, timer < 0 / 0 / 1, negative yaw wrap, large dist / player
  coordinates in state 8, and padding preservation.
- Whether `ae_camera_rsin` / `ae_camera_rcos` accept every int32 yaw as the
  original does (inherited from native_camera_math).

## Not included

Scripts, renderer, player integration, GetFocusPoint, indexed handlers.
