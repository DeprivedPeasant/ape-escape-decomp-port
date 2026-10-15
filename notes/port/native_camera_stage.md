# Native camera stage composition

`native_camera_stage.c/.h`: `ae_camera_stage_init` (a local subset of CameraResetAll,
0x80047754), `ae_camera_stage_step` (the supported subset of CameraUpdate, 0x80047BD0) and
`ae_camera_stage_free`. It adds no new leaf logic: it composes `native_camera_regions`,
`_bank`, `_lifecycle`, `_select`, `_script`, `_motion`, `_focus`, `_molten`, `_follow` and `_blend`
through the `AeCameraHost` (math tables, query bindings, follow frame).
Worker had no shell or game data. Codex subsequently reviewed, compiled and validated the delivery; see validation/camera_stage_validation.md.

**This is not a full CameraUpdate / CameraResetAll claim.** See "Omitted".

## Ownership
- `AeCameraStage` holds the slot bank / controls / region focus (`AeCameraRegionFrame`),
  the owned region records (`AeCameraRegions`), and **borrowed** blob / script table
  pointers. The blob must outlive the stage and not change.
- The host must be ready with `queries.user == host` (stable address). The stage stores no
  query binding and never copies, moves or rebinds the host; `host_usable` re-checks the
  binding on every call.
- Init requires a zero-initialized or freed stage (a ready stage is rejected, so records
  are never leaked). `ae_camera_stage_free` frees the records and zeroes the stage.

## Init (local ResetAll subset)
1. Eye / target seed = host `slot` bytes 0x0A / 0x12 (8 bytes each).
2. `ae_camera_regions_load` on the blob. count > 255 fails.
3. Controls start zero: 0x0D = 0x1B = 0xFF, u16 0x2C = 0x148, u16 0x00 = region count.
   The 12 bank slots start all-zero. The original only zeroed each slot's kind byte;
   zeroing every unused byte is an explicit native deterministic policy.
4. Non-empty regions: CAM header u16 at +0x0E gives the local script table offset
   (`cam_offset + u16`), window = table start .. blob end (needs >= 8 bytes), opaque
   address tag `0x80154000 + offset`. Empty regions skip this and leave the table NULL.
5. `slot_create` kinds 1, 2, 3 (initial eye = seed eye, current eye / target = seed,
   player = snapshot pos / facing8) -> controls 0x18 / 0x19 / 0x1A.
6. Per region: flags low nibble cleared. Where `flags & 0x6000 == 0x2000`: persistent
   kind 5 via `slot_create` with the region center (record +4, 8 bytes) as initial eye
   and the global seed as current eye / target; flags low nibble = index; the script id
   (signed s16 at +0, negative fails) is attached with `ae_camera_set_script(...,
   region_flag = (flags >> 13) & 3)`. Slot byte 0x93 is NOT set for these (the original
   does not either).
7. Control 0x0E = control 0x18, then `ae_camera_activate_slot` (tag `0x80130000`).
8. Everything is built on locals and committed only on success. Failures (bad CAM data,
   script table out of the blob, negative / out-of-range script id, a full bank
   (more than 9 persistent regions; the bank service would report "success, index 0"),
   math failure) free the records and leave the stage untouched except `status`.

## Step
Backup: stage frame, region records (heap copy), eye / target, host slot / frame /
snapshot_shadow / status. Order:
1. `ae_camera_host_snapshot` (the real one; the caller supplies the other frame inputs,
   including `projected_sxy` from the previous view).
2. `ae_camera_region_update` unless control 0x3E != 0.
3. `ae_camera_select_mode`; its snapshot request (`snapshot_eye` on the follow slot,
   control 0x18) then the activate request, in that order.
4. Shared control 0x47 cleared once.
5. Slots 0..11, kind 4 / 5 (kind tested once): `script_run`, apply its remove request
   (`slot_remove`), then `slot_update` even if the script removed the slot's own kind.
   Motion focus callback: all 8 output bytes zeroed (native policy, not proof of the
   original stack), then `ae_camera_get_focus_point(slot, mode, snapshot pos, facing8,
   stage focus)`. Indexed callback: id 3 (resident force-follow) sets control 0x42 = 1;
   id11 in area05 calls the validated overlay18D Molten Lava handler, using the same host
   scratchpad/math/queries and region focus; other ids fail `UNSUPPORTED_HANDLER`. Stereo (0x800AF131) is not an input: 0.
6. Slots 0..11: kind 0 / 4 / 5 skipped; kind 1 -> `ae_camera_follow_update(mode 0)` with
   host math / queries on the host frame; kind 2 / 3 skipped unless it is the selected
   slot (control u16 0x04) -> `UNSUPPORTED_MODE`; any other kind fails `UNSUPPORTED_MODE`.
   `ae_camera_host_step` is not called (it would snapshot and clear the guard again); its
   MATH_DOMAIN rule is mirrored. The 0x50 control block is copied stage -> host frame
   before and host frame -> stage after each follow call (the stage block is canonical).
   The guard 0x47 therefore makes later follow slots in the same frame no-ops.
7. `ae_camera_apply_slot` on the selected slot (u16 0x04) with stage eye8 / target8.
8. Success: `host->slot` = copy of the selected bank slot with bytes 0x0A / 0x12 replaced by
   the blended stage eye8 / target8 (for the existing renderer). The bank slot keeps the
   real follow state, so the next frame updates the bank slot, never the display copy.
   `host->frame.controls` mirrors the stage controls. `status = 0`.
9. Failure: stage, records, eye / target, host slot / frame / shadow / status are
   restored; `stage->status |= bits`.

## Status bits
`MISSING` 1 (null, not ready, unbound host, allocation), `UNSUPPORTED_HANDLER` 2 (indexed
unsupported area/id pair, follow gadget variant), `MATH_DOMAIN` 4, `UNSUPPORTED_MODE` 8 (the low nibble
equals `AE_CAMERA_FOLLOW_*`), `INVALID` 0x10 (area data, ids, indices, service failures;
region-update / select / script failures are reported only as INVALID).

## Omitted (explicitly)
CameraSetProjection, AimModeLeave, the AreaBumpAlloc allocator (the bank is embedded),
the CameraName link pass (names are loader-side), CameraLookAt / view matrices, the
original's 'x' write into the area blob, CameraShakeReset / Apply, EventCamUpdate,
StageExit flags / sequence, transition frame draw, ScreenFade, the pending offset adds at
0x800E68E0 (eye / target nudges), kind 2 / 3 writers (AimUpdate / FixedUpdate; the
reserved slots are inactive), kind 4 / 5 script opcodes outside the local set,
CameraFollowGadgetVariant. Global region pointer 0x4C is inert.

## Validation and limitations

Strict C11 build and actual reset/update instruction comparison cover all101areas,
102initializations and2940scoped camera ticks. This includes30calls through the actual
Molten Lava overlay18D handler at80140B0C, selected by positioning the synthetic player
at the second camera region center. All bank/control/focus/scratch/eye/target comparisons
pass; four other missing-handler cases roll back completely. See
validation/camera_stage_validation.md and check_camera_stage.py for the controlled endpoints.
Constant ground and synthetic positions are test inputs, not gameplay capture parity.

Handler ids are overlay-dependent: id11 is bound only in area05. Other indexed handlers,
selected aim/fixed slots, flight/RC/boss dispatch, global camera events and original GTE
projection remain incomplete. The independent Molten Lava kernel comparison covers6000
whole original overlay cases (see validation/camera_molten_validation.md).
