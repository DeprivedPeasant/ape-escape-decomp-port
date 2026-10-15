# Camera (survey, 2026-10-06 night session)

Entry: `CameraUpdate` (0x80047BD0, called from Mode_Stage). Export: `notes/decomp/camera.c`.

Later, more detailed work (2026-10-07): the native camera port in `src/camera/native_camera*.c` reconstructs follow /
orbit, the area region records (CAM section), slot selection (`CameraSelectMode` 0x80046DE8), snapshot / activation,
blends and the CAM script VM, each checked against the original instructions; see `notes/port/native_camera*.md`,
`camera_*_validation.md` and `camera_scripts.md`. `CameraSlotSnapshotEye` copies only the eye, not the target.

Frame flow:
1. `CameraSnapshotPlayer` copies player state into the scratchpad camera input block.
2. `FUN_8004BF20` (unless a scripted camera is active) and `CameraSelectMode` pick which of the 12
   **camera slots** (`g_CameraSlots`, 0xB0 bytes each, from the area data) is active and its kind:
   1 = follow (default), 2 = aim (gadget aim / `g_AimMode`), 3 = fixed (`DAT_800E6908`),
   4/5 = area camera (player is inside an area camera region, `DAT_800E68EB`). A change starts a
   blend (`DAT_800E68FE` blend style 0-3, eye/target of the old camera saved at 0x800E68EC..).
3. Area-camera slots (kinds 4/5) are updated by `FUN_8004C318` + `FUN_8004CA70`; every other slot by
   kind: `CameraFollowDispatch` (default), `FUN_80048454` (kind 2), `FUN_8004875C` (kind 3).
4. Post passes: `FUN_80047FF0`, `CameraShakeApply`, `EventCamUpdate` (scripted event cameras started by `EventCamStart(id)`, skippable with the face/start buttons), `FUN_8004D40C`, `FUN_8004CEB4`, then external offsets
   (`DAT_800E68E0..E4`) are added to eye and target.
5. `CameraLookAt` writes `g_ViewMatrix` / `g_CameraRotMatrix` (the matrix every renderer loads and
   `PadUpdate` uses for camera-relative stick input); `FUN_80046A78` finishes.

State bytes: `DAT_800E68DE` current slot, `DAT_800E68DF` current kind, `DAT_800E6902` / `g_AimMode`
(aim camera requests), `DAT_800E6908` (fixed camera), `DAT_800E6911` / `DAT_800E6912` (forced area / follow).

Still unnamed: the per-kind follow logic (0x80049xxx–0x8004Bxxx), area region test, camera
collision (pull-in) and the blend helper 0x80046DAC. Camera slot layout: struct `CameraSlot` (0xB0 bytes, `tools/structs.txt`; `g_CameraSlots` is a pointer to 12 of them) from the uses in the 22 slot functions, which now have `CameraSlot *` prototypes: kind, eye / target (s16[3] at +0x0A / +0x12, smoothed), follow distance / yaw / heights, ground clamp state, eye / target velocities, script pointer / timer / step and operands (+0x84..+0xA0), focus offsets and the region anchor. Offsets are certain, the meanings tentative.

## Names added later (batch 41)

Camera slots: `CameraSlotCreate` / `CameraSlotRemove` / `CameraActivateSlot`; per kind: follow = `CameraFollowUpdate` (4.9 KB, with
`CameraSlotGroundClamp`, `CameraSlotStickOrbit`), aim = `CameraKindAimUpdate`, fixed = `CameraKindFixedUpdate`,
RC car = `CameraRcCarFollow` (`CameraRcCarActive`), area cameras via `CameraRegionUpdate` (regions at DAT_800E691C,
0x30 bytes each) and scripts (`CameraScriptRun`). **Event cameras** (`EventCamCreate`, `EventCamStart*`,
`EventCamUpdate`) are kind-4 slots with scripts from the area data. Screen flashes: `ScreenFadeSet` / `ScreenFadeUpdate`.
`CameraApplySlot` copies the active slot's eye/target to `g_CameraEye` / `g_CameraTarget`.
