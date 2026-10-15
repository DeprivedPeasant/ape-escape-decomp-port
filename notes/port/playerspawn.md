# Player entry spawn and camera reset

Scope: the eleven RAM image functions listed in `extract/codex-task/task.md`, ported in
`src/player/ae_player_spawn.c` with field and global offsets in `ae_player_spawn.h`. The
port uses structured C and calls already verified RAM image ports for ground height,
vector normalization, body registration, camera slot creation, and sound queue/voice work.

## Instruction call map

Calls reconstructed from the original EXE:

```text
PlayerSpawnAtEntry
  AreaGetEntryPoint
  GetGroundHeight                 (verified port; GetSolidHeight remains a world hook)
  VectorNormalSS                  (verified port)
  CollisionBodyRegister           (verified port)
  CameraResetAll
    SetProjection
      SetGeomScreen
    AimModeLeave
      PlaySfx
    AreaBumpAlloc                  (external allocator hook)
    Printf                         (unmatched CAM name diagnostic only)
    CameraSlotCreate               (verified port; Atan2Safe remains a camera hook)
    EventCamSetScript
    CameraActivateSlot
    CameraLookAt                    (full view/render path remains a hook)
    CameraShakeReset
    StageExitFlagsClear
```

`AreaGetEntryPoint` indexes the active entry table installed by `AreaLoad`, using 0x24-byte
records. The area argument is retained for the original ABI. `PlayerSpawnAtEntry` copies the
three camera and position components, samples ground height, initializes the player fields,
normalizes the low halfword from each 32-bit facing component, registers the player body, and
resets the camera.

`CameraResetAll` clears the 0x7C-byte camera state, selects projection 0x148, parses and marks a
valid CAM header, links region names, allocates the 12-slot pool, creates the follow/aim/fixed
slots plus area camera slots, attaches scripts, selects the follow slot and calls the view path.
When `AreaLoad` leaves the camera pointer at the area-base sentinel, or the CAM signature is
invalid, the original switches to the built-in empty header at 0x800BF120. `EventCamSetScript`
selects either the area script-index table or the name-entry table, resets the slot timer and
step, and stores the resolved script pointer.

## Remaining host hooks

- `AreaBumpAlloc` (0x8008C1B8) allocates the 0x840-byte camera slot pool.
- `Printf` (0x800A3720) reports an unmatched 32-byte CAM region name. The checker compares the
  format and the consumed `%s` argument; unused a2/a3 values are stale caller registers.
- `CameraLookAt` (0x80046670) builds the full view and rotation matrices and applies roll. The
  existing player-input port validates only its `g_CameraRotMatrix` component, so this task
  preserves the call as a named hook.
- `SetGeomScreen` writes GTE control register 26. The new callback is the native renderer/GTE
  boundary; the checker compares its distance with the pinned GTE oracle's H register.
- `GetSolidHeight` (0x8004D8C0) supplies the world ground query used under the verified
  `GetGroundHeight` port; `Atan2Safe` (0x80046340) supplies camera yaw. These are hooks behind
  already verified dependencies, outside this task's 11-function table.
- `SpuWaitIdle` and `Printf` are the `AeSoundHw` boundary used by `PlaySfx` for voice readiness
  and a failed voice diagnostic. `PlaySfx`'s queue and RAM voice allocation use verified ports.

The checker models the allocator, diagnostic, ground, yaw, and view hooks with deterministic
results. It supplies the hardware wait result for the sound port. `SoundLoopStart` and
`SoundLoopStop` are part of shared host structures but are not reached by these eleven calls.

## Differential validation

`.venv/Scripts/python.exe tools/mips/check_player_spawn.py --coverage` passed 41 cases using the
original EXE and real member data from areas 01, 02, 05, and 07. `CameraResetAll` also covers
area 04 with no CAM, named CAM regions, an unmatched name, and an area-camera slot. It covered
every original instruction:

| Function | Cases | Instructions |
|---|---:|---:|
| AreaGetEntryPoint | 3 | 26/26 |
| SetGeomScreen | 4 | 3/3 |
| PlayerSpawnAtEntry | 12 | 95/95 |
| SetProjection | 3 | 11/11 |
| CameraActivateSlot | 3 | 15/15 |
| CameraResetAll | 5 | 287/287 |
| CameraShakeReset | 1 | 5/5 |
| AimModeLeave | 2 | 15/15 |
| EventCamSetScript | 2 | 36/36 |
| StageExitFlagsClear | 1 | 5/5 |
| PlaySfx | 5 | 58/58 |
| **Total** | **41** | **556/556** |

The default run records results in `extract/checks/results/check_player_spawn.json`. The only
declared comparison exclusion is the emulated MIPS stack window `801FF000..80200000`; the native
port uses the host stack, while externally visible RAM below it and scratchpad are compared.
There are no uncovered instruction ranges.

`.venv/Scripts/python.exe tools/mips/check_player_spawn.py --mutate` caught all 32 hand-selected
mutations (at least two for every target function). The active `AimModeLeave` scenario uses
distinct, playable sound definitions for IDs 5 and 6; `CameraResetAll` starts from a nonzero
camera-state pattern so the full clear is observable.

## Findings from differential runs

- Entry facing is stored as three 32-bit words; the original packs their low halfwords at a
  2-byte stride before `VectorNormalSS`. Copying six contiguous source bytes loses the Z axis.
- `PlayerSpawnAtEntry` initializes the player's model-bone buffer pointer at +0x14 and up vector
  at +0x68/+0x6A/+0x6C, in addition to the player data pointer at +0x18.
- The camera reset sentinels are the existing `AE_CAM_G_REGION_SLOT` (+0x1B) and
  `AE_CAM_G_REGION_OLD` (+0x0D) fields; the first displacement is decimal 27 in the EXE, not
  +0x27. The CAM header halfword at +0x06 is copied to camera state +0x6A; its meaning is still
  unclear, so that field remains named `UNKNOWN` rather than receiving an unsupported name.
- `CameraLookAt` receives absolute addresses for stage state +0x258/+0x260, not bare offsets.
- Stage exit flags are fields at +0x20/+0x24 from global base 0x800B8CC0.

The checker stages the pinned PCSX-ReARMed GTE inputs under the ignored local `extract/mips`
build directory before compiling. This keeps generated manifests, wrappers, and DLLs inside
the worktree when the shared dependency cache is a read-only junction.
