# Native camera handler 11 (area 05, overlay 0x18D 0x80140B0C)

`native_camera_molten.c/.h`: `ae_camera_molten_update(slot, math, queries, player, region_focus8, scratch)`.
Pure local C99 port of the overlay 0x18D camera slot handler (indexed handler 11) used by area 05.
Status: written from the original MIPS by a delegated worker without a compiler or game data;
Codex strict build and6000completeoriginalinstruction comparisons pass (see validation/camera_molten_validation.md).

## Behaviour (from the assembly, not from the decompile's parentheses)

1. Player halfwords `(s16)pos[0..2]` -> scratch `0x10 / 0x12 / 0x14`.
2. `CameraSlotGetFocusPoint(slot, 2)` = 8 byte copy of `region_focus8`; only the x (`+0`) and z
   (`+4`) halfwords are used (`fx`, `fz`). Done through `ae_camera_get_focus_point` (mode 2).
3. `v = {s16(fx - px), 0, s16(fz - pz)}`; `VectorNormalSS` in place (`ae_camera_normalize`). Result
   not checked by the original; a sum > INT32_MAX traps there, so the port fails closed.
4. `dx = s16(px - fx)`, `dz = s16(pz - fz)`; `dist = SquareRoot0(dx*dx + dz*dz)` (u32 add, via
   `queries->sqrt`). `mag = 2000`; if `dist > 1000` (signed) `mag = dist << 1` (32 bit wrap);
   then if `mag >= 0xD49` (signed) `mag = 0xD48`. A wrapped negative `mag` therefore stays negative
   (only reachable with a sqrt result >= 2^30).
5. `tx = lo32(v.x * mag)`, `tz = lo32(v.z * mag)` (`mult`/`mflo`, wraps).
   Desired target (scratch): `+8 = fx - ((tx<0 ? tx+3 : tx) >> 14)`, `+A = py - 0x82`,
   `+C = fz - ((tz<0 ? tz+3 : tz) >> 14)`.
   Desired eye: `+0 = fx - (tx >> 12)` (no +3 bias), `+2 = py - 0x35C`, `+4 = fz - (tz >> 12)`.
   All stored as halfwords (low 16 bits).
6. `CameraSlotSmoothTarget(slot, 8, 8, 8)` then `CameraSlotSmoothEye(slot, 8, 16, 8)`, using the
   desired values just written (reads signed halfwords).
7. `w = {s16(eye.x - fx), 0, s16(eye.z - fz)}` with the smoothed eye (slot `0x0A`, `0x0E`);
   `sq = VectorNormalSS(w)`. If `(s32)sq > 0x1DE83F` (i.e. `sq >= 1400^2`), stop (the smoothing and
   scratch writes stay). Otherwise, with `n` = normalized `w`:
   - `target.x (0x12) = (fx + n.x*350) >> 12` and `target.z (0x16) = (fz + n.z*350) >> 12`
   - `eye.x (0x0A) = (fx + n.x*1400) >> 12` and `eye.z (0x0E) = (fz + n.z*1400) >> 12`

   with `fx`/`fz` the signed (`lh`) focus halfwords, arithmetic shifts, halfword stores.
   The `y` halves of eye and target are only changed by the smoothing.

Writes: slot eye / target halfwords (`0x0A..0x0F`, `0x12..0x17`) only; scratch `0x00..0x05`,
`0x08..0x0D`, `0x10..0x15`. All other slot and scratch bytes (including `0x06/0x07/0x0E/0x0F`
scratch gaps and slot padding) are preserved. The stack locals (focus, both vectors) are not
visible. Facing is not needed (focus mode 2).

## Failure contract

Returns 0 with slot and scratch bit-identical (all work on local copies, committed at the end) if:
null `slot / math / queries / player / region_focus8 / scratch`, `math->loaded == 0`,
`queries->sqrt == NULL`, either normalization sum exceeds INT32_MAX (original `add` would trap), or
a smoothing division would trap (cannot happen with divisors 8 / 16 / 8). A zero vector normalizes
to zero and succeeds (final branch writes `focus >> 12`-style values with n = 0). Only
`queries->sqrt` is used; `normalize` / `ground` may be NULL. Unsupported status for areas/handlers
is the caller's concern.

## Dependencies

`native_camera.c` (slot access, smooth), `native_camera_focus.c`, `native_camera_math.c`
(normalize). No game data or tables embedded.

## Validation still required (Codex)

- Compile with the existing camera objects; `-Wall -Wextra -Wconversion` check, UBSan run.
- Differential run against the original overlay 0x18D MIPS at 0x80140B0C (with SDK
  `VectorNormalSS` 0x800301E0, `SquareRoot0` 0x800303C0, `CameraSlotGetFocusPoint` 0x8004B8F4,
  `CameraSlotSmoothTarget` 0x80047558, `CameraSlotSmoothEye` 0x80047464): compare all 176 slot bytes
  and all 128 scratch bytes, including padding, on randomized inputs and edges: focus == player
  (zero vectors), distances around 1000 / 1001 / 0x6A4, wraparound halfwords (+-32768 players and
  focus), eye at/around the 1400 length threshold (`sq` 0x1DE83F / 0x1DE840), negative `tx`/`tz`
  (+3 bias) and negative eye vectors.
- Confirm `VectorNormalSS` does not touch the real scratchpad 0x1F800000 area (the task states it
  only uses GTE registers and caller structs) and that its return value is the squared length.
- Confirm the normalize helper's failure case (`-32768, -32768` => sum 2^31) matches the original trap.
- Confirm `sqrt` callback matches SquareRoot0 for the full u32 range incl. results > 2^30 (never
  expected).

## Limitations / blockers

- Not compiled or run in the worker (no shell, no data). Evidence is only the supplied assembly.
- The stack-resident focus point `y` (`asStack_38[1]`) is not used by the handler, so it is dropped.
- Assumes the smoothing helpers read desired values from scratch `0x08` (target) and `0x00` (eye);
  this port passes those halfwords explicitly. If the original SDK helpers read other scratch
  bytes, validation will show it.


## Codex review

Delivery hashes/absent primary baselines verified, all code/notes read. StrictC11 build and6000fulloriginaloverlay18D cases compareall176slot/128scratchbytes;6000actualSDKsqrtqueries andtransactionalmissing-math checkpass. BIOSmemset is supplied; allotherhelper/math instructions execute. Reference decompile/listings remain ignored and are deliberately omitted from this committed note. Not yet wired into stage dispatcher.
