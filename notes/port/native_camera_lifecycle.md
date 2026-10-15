# Native camera region lifecycle

`native_camera_lifecycle.c/.h`: `ae_camera_region_update`, a port of CameraRegionUpdate
(0x8004BF20) written from the original pseudocode and the full instruction listing.
It adds no new leaf logic. It calls the existing `ae_camera_region_test`,
`ae_camera_slot_create` / `ae_camera_slot_remove` / `ae_camera_set_script`,
`ae_camera_select_mode`, `ae_camera_snapshot_eye` and `ae_camera_activate_slot`.
Not compiled or run by the author (no shell); Codex must build and test.

## Frame layout
`AeCameraRegionFrame` = controls[0x50] (0x800E68D0..691F), focus[16] (0x800E68C0..CF),
slots[12] x 0xB0. Regions are the caller's mutable 0x30-stride records. Control 0x4C
(region pointer) is never read or written; control 0x02 (slot count) is the caller's.

## Flow (per call)
1. Query point = low 16 bits of player x, (y - 0x80), z. Player is required.
2. If control 0x0C != 0, decrement it (before the query).
3. `ae_camera_region_test` on the records and explicit `region_count`; -1 -> 0xFF,
   -2 (invalid) fails the call.
4. query == control 0x0D (previous): nothing more.
5. Otherwise if 0x0C == 0 it becomes 1; a transition happens only when 0x0C == 1, so a
   timer that is still 2..255 after the decrement blocks it (the 0x0C value stays).
6. Transition: 0x0C = 0, then:
   - **Exit (query 0xFF):** old record nibble n = flags & 0xF, mode = flags & 0x6000;
     `Remove(n)` if n != 0 and mode is 0 or 0x6000. Then 0x1B = 0xFF, 0x3F/0x40/0x42 = 0,
     then `ae_camera_select_mode` (12 slots), its snapshot request
     (`ae_camera_snapshot_eye` on slot control 0x18) and `ae_camera_activate_slot`.
     This all happens before the final 0x0D store.
   - **Change (query = new region):** if previous != 0xFF, `Remove(old nibble)` if
     n != 0 and mode == 0. Clear 0x3F/0x40/0x42 and 0x1B = 0xFF. Focus: bytes 0..5 =
     region +4..+9 (focus +6/+7 preserved); focus +8/+A/+C = s16 region +A/+C/+E
     halved, truncating toward zero. Region flags |= 0x8000 if clear. Then:
     - nibble 0: EventCamCreate(s16 region +0, param2 = (flags & 0x6000) == 0x4000,
       group 1): `slot_create(kind 4, eye8, eye8, target8)`, `set_script(index,
       region_flag 0)`, and when param2 == 0 `activate_slot` and control 0x1B = index.
       Region flags low nibble = index, slot byte 0x93 = new region index.
     - nibble != 0: 0x1B = nibble, slot 0x88 = slot 0x8A (u16), and if
       (flags & 0x6000) == 0 eye8 / target8 are copied to slot 0x0A / 0x12 (8 bytes each).
7. At the end of a transition: 0x0C = 0 and 0x0D = query, even if `Remove` wrote
   0x0C / 0x0D itself.

## Transactionality
The frame is copied by value and the records into a heap copy of exactly
`region_count * 48` bytes; both are committed only on success, so any failure leaves
all frame and region bytes unchanged. Requirements: frame, player, eye8, target8
non-null; region_count <= 255; regions non-null and regions_bytes >= count * 0x30
when count > 0. Failures: invalid query args, previous region >= count (checked only
when a transition happens), slot nibble >= 12, selector / snapshot / math failure,
negative script id, any `set_script` domain failure, allocation failure.

## Behaviour carried over from the existing services
- All 12 slots used on create: `slot_create` reports success with index 0 and no
  change (as documented in the bank notes); the script is then attached to slot 0
  and the region is linked to it, which is what the original would do with return 0.
- `original_slot_base` is only the opaque tag stored by `ae_camera_activate_slot`.
- tables: only the selector (kind 5 dot test) and the snapshot use them; facing is
  forwarded to `slot_create` and not read for kind 4.

## Open points / validation still required
- Compile (C11) and differential-test against the original function with real
  callees: timer cases (0, 1, 2..255), exit / change with nibble 0 and != 0, each
  `6000` mode, flag 0x8000 setting, focus halving of odd / negative values, create
  with param2 0 / 1, full bank, and failure snapshots of frame + regions.
- The negative script id is rejected (the original would index before the table);
  needs confirming no real region uses it.
- The script table window assumption noted in `native_camera_bank.md` applies unchanged.
- The previous-region range check on transition is a native-side safety rule; the
  original never validates it.
