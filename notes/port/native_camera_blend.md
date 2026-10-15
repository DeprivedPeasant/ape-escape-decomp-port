# Native camera blend (CameraApplySlot, 0x80047FF0)

`int ae_camera_apply_slot(uint8_t controls[0x50], const AeCameraSlot *active, uint8_t eye8[8], uint8_t target8[8])`

Implemented from the original MIPS instruction listing (the decompile was not
used where it differs; its pseudocode omits nothing material). Returns 1, or 0
with nothing written if any pointer is NULL. No validation is claimed here;
whole-function differential testing against the original is still required.

## Inputs

| Argument | Original | Use |
|---|---|---|
| `controls` | 0x800E68D0..0x800E691F | `+0x1C` s16[3] saved eye, `+0x24` s16[3] saved target (`+0x22`, `+0x2A..` unused), `+0x2E` u8 blend style, `+0x30` u16 counter. `+0x08` (active slot pointer) is not read; `active` replaces it. |
| `active` | slot at control `+0x08` | read-only; bytes `+0x0A..0x11` eye, `+0x12..0x19` target. |
| `eye8` / `target8` | g_CameraEye 0x800F45A8 / g_CameraTarget 0x800F45B0 | exact 8 SVECTOR bytes each, little-endian s16 x, y, z then 2 padding bytes. |

## Behaviour

1. Counter `c` = u16 at `controls+0x30`.
2. `c == 0`: copy all 8 bytes of slot `+0x0A` to `eye8` and `+0x12` to `target8`
   (padding included). Nothing else is written.
3. Otherwise step `c` by style (`controls+0x2E`), store it back as u16:
   - 1: `c < 0x30 ? (c*7)>>3 : c-8`
   - 2: `(c*3)>>2`
   - 3: `0`
   - any other value (including 0): `c < 0x20 ? c-1 : c-3`
4. With the stepped `c` and `inv = 0x80 - c` (32-bit wrap), for each axis
   (eye x,y,z then target x,y,z), saved = sign-extended s16 from controls,
   cur = sign-extended s16 from the slot:
   `sum = saved*c + cur*inv` (low 32 bits, wrapping), `if (sum < 0) sum += 0x7F`,
   `out = (int32)sum >> 7` (arithmetic), stored as the low 16 bits at the
   axis' halfword in `eye8` / `target8`.
   Bytes +6 / +7 of each output vector are left unchanged.
   Style 3 therefore ends with `c = 0` and outputs the slot values exactly,
   but padding is preserved (unlike the counter-0 copy).

All arithmetic is done in `uint32_t` with explicit sign handling, so there is no
signed overflow and no implementation-defined shift or conversion. `active` is
snapshotted before any write, matching the original's stack copy.

## Scope / not covered

- Only this function. The caller, slot selection, snapshot, activation and
  rendering are separate; no viewer integration.
- Writes limited to the counter (when nonzero on entry) and the `eye8` /
  `target8` bytes described above. Slots are never written.
- Counter values above 0x80 are not special-cased (as in the original); the
  weights then go negative via wrap.

## Blockers / validation required

- No original data or captures were available here; the control field `+0x08`
  is assumed to be the same slot as the caller supplies in `active`.
- Needs the whole-function original-MIPS differential test (counter 0, every
  style, boundaries 0x1F/0x20/0x2F/0x30, counter > 0x80, extreme s16 values,
  padding preservation).
