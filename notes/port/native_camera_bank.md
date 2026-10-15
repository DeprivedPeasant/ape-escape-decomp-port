# Native camera bank services

`native_camera_bank.c/.h`: portable C99 ports of CameraSlotCreate (0x800471E4),
CameraSlotRemove (0x80047290) and EventCamSetScript (0x8004BAD8), built on
`native_camera` (slot accessors, `ae_camera_init_follow`) and `native_camera_math`
(`ae_camera_atan2_safe`). Not built or run by the author (no shell); Codex must
compile and differential-test.

## Create
- Scans the 12 slots for the first with kind byte 0. None free: returns 1 with
  `*index = 0`, nothing changed (original returns 0 without touching anything).
  Only `slots` and `index` are required up front.
- Free slot: all inputs are validated, then a temp copy of that slot gets: kind
  byte (low byte of `kind`), initial eye 8 bytes at 0x0A, linked index at 0x02,
  0x93 = 0xFF, then `ae_camera_init_follow` with the full int32 kind, current
  eye/target, player, facing and yaw. The initial eye is therefore overwritten by
  the current eye copy (original does the same).
- yaw (kind 1 / 3 only): `Atan2Safe(-facing[0], -facing[2])` via the supplied
  tables; failure returns 0 with nothing changed. Other kinds need no tables.
- Controls u16 at 0x02 incremented with 16-bit wrap. Slot and controls are
  committed only after everything succeeded; `*index` is written only on success.
- A kind whose low byte is 0 (0, 256, ...) leaves the slot looking free, as in the
  original; the counter still increments.

## Remove
- Preflight (no writes on failure): index < 12; if slot byte 0x93 != 0xFF then
  non-null `regions`, 0x93 < `region_count`, and the 0x30 byte record fully inside
  `regions_bytes`. Control 0x4C (region pointer) is not used.
- Then: control 0x41 cleared; region u16 at +2 `&= 0xFFF0`; controls u16 0x02
  decremented (wraps from 0); if kind is 4 or 5 and control 0x1B == index:
  0x1B = 0xFF, 0x0C = 0, 0x0D = 0xFF; if u16 control 0x04 == index: control 0x0E =
  low byte of index; finally slot kind byte = 0 only.

## SetScript (local group 1)
- Table window starts at the entry table: u16 count at 0, entries at +8, stride
  0x28, u16 relative script offset at entry + 2. Validations: aligned base, window
  >= 8 bytes, index < count (and < 0x80000000, the original's signed param),
  entry fully inside the window, relative offset 2-aligned and at least 2 bytes
  inside the window, base + offset not overflowing 32 bits.
- Writes slot 0x8E = region flag, 0x88 = 0x8A = 0, 0x84 = base + offset.
- No script decoding. Only the group 1 adapter (DAT_800e6924 / 0x800E692C); group
  0 (DAT_800e6920 / 0x800E6928) uses the same format but is not proven by the
  inventory, so the caller picks the base/table.

## Assumptions / blockers for review
- The window check uses the relative offset as an offset into the provided window,
  i.e. it assumes the script data lies inside the same byte window the host hands
  over (host passes table-offset-to-area-end). In the original, the script base
  (0x800E6924) and the entry table (0x800E692C) are separate pointers; if real
  areas have scripts before the table, the `rel + 2 <= table_bytes` bound is
  wrong for them and needs the window defined from the script base instead.
  Needs checking against all real area tables.
- Region record alignment: the +2 flag byte clear touches only the low byte
  (nibble lives in byte +2); byte +3 untouched.

## Validation still required
Compile, differential test against the original bank calls with the actual SDK
services, all real area tables, full-bank/controls/regions snapshots on failure.
