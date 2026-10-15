# RAM-image camera port

This batch ports the 26 camera functions listed in `extract/codex-task/task.md`.
The older `src/camera/native_camera*.c` modules remain untouched; this module
uses `AeRam`, original addresses, and a hook table for original routines outside
the batch.

## Call map recovered from the original MIPS listings

The call edges below were read from the EXE instruction ranges recorded in
`notes/inventory/functions.csv`. Internal calls stay direct in C and in the MIPS
oracle. Calls to other original functions are routed through named
`AeCameraHooks` callbacks so the checker can give both sides the same effects.

* Slot lifecycle: `CameraSlotCreate` calls `CameraSlotInitFollow`, which calls
  `CameraSlotResetFollow` for kinds 1/3 or `CameraSlotClearScript` for kinds
  4/5. `CameraSlotRemove` has no calls. `CameraSlotAimAtFocus` calls
  `VectorNormalSS` and `memset`; `CameraSlotUpdateYaw` calls `Atan2Safe` and
  `CameraSlotAimAtFocus`; `CameraSlotSnapshotEye` calls `VectorNormalSS` and
  `Atan2Safe`.
* Follow geometry: `CameraFollowUpdate` calls the matrix setup, vector
  normalization, ground, trigonometry, input, stage, RC-car and submerged-player
  routines; internally it calls `CameraSlotUpdateYaw`, `CameraSlotStickOrbit`,
  `CameraSlotGroundClampNear`, `CameraSlotGroundClamp`, and
  `CameraFollowGadgetVariant`. `CameraFollowAimedVariant` calls
  `CameraSlotPlaceBehindPlayer`, `CameraSlotSmoothTarget`,
  `CameraSlotSmoothEye`, and conditionally `CameraFollowGadgetVariant`.
  `CameraKind3FollowUpdate` calls `CameraSlotAimAtFocus` and the two smoothing
  functions.
* Kind dispatch: `CameraFollowDispatch` routes kind 3 to
  `CameraKind3FollowUpdate`, the default follow path to `CameraFollowUpdate`,
  and other player modes through RC-car, flight, area-script, boss, and stage
  hooks. `CameraKindAimUpdate` calls the aim request/mode hooks, the focus snap,
  then `CameraFollowAimedVariant`. `CameraKindFixedUpdate` calls radar/focus,
  fade and sound hooks, `CameraSlotPlaceBehindPlayer`, `CameraFollowUpdate`,
  and the empty kind callback.
* Per-slot update: `CameraSlotUpdate` calls `CallIndexedHandler`,
  `CameraSlotGetFocusPoint` (also via the local helper at `0x8004B880`),
  trigonometry, and `SquareRoot0`.
* `CameraFollowGadgetVariant` also builds the gadget camera's two display-list
  primitives. Its writes into the active draw-list and primitive pool are part
  of the original RAM behavior and remain in the C port; the GTE/matrix and
  ground queries are callbacks.

## Validation

`tools/mips/check_camera_ram.py` compares each entry point to the original
instructions with the real EXE and area `0x01` member loaded. Its report records
case counts, instruction coverage, and the mutations used to probe checker
sensitivity. See the generated default-run result under ignored
`extract/checks/results/` and the ledger rows in `tools/verified.csv`.

## Port and coverage results

The final default scale is 32 cases per function (832 cases total). All 26
entry points compare equal, and every instruction is either executed or has a
registered reachability proof. Coverage totals below count executed original
instructions over all instructions in the function.

| Function | Cases | Instructions | Coverage note |
|---|---:|---:|---|
| `CameraKindFixed_Nop` | 32 | 2/2 | complete |
| `CameraSlotInitFollow` | 32 | 56/56 | complete |
| `CameraSlotCreate` | 32 | 43/43 | complete |
| `CameraSlotRemove` | 32 | 52/52 | complete |
| `CameraSlotAimAtFocus` | 32 | 65/65 | complete |
| `CameraSlotSmoothEye` | 32 | 49/61 | 12 divide traps proved unreachable |
| `CameraSlotSmoothTarget` | 32 | 51/63 | 12 divide traps proved unreachable |
| `CameraKindAim_SnapToFocus` | 32 | 15/15 | complete |
| `CameraFollowDispatch` | 32 | 54/54 | complete |
| `CameraKindAimUpdate` | 32 | 189/189 | complete |
| `CameraKindFixedUpdate` | 32 | 138/140 | 2 state-gate instructions proved unreachable |
| `CameraSlotPlaceBehindPlayer` | 32 | 45/45 | complete |
| `CameraSlotUpdateYaw` | 32 | 103/103 | complete |
| `CameraSlotGroundClamp` | 32 | 64/64 | complete |
| `CameraSlotGroundClampNear` | 32 | 116/116 | complete |
| `CameraSlotStickOrbit` | 32 | 102/102 | complete |
| `CameraSlotResetFollow` | 32 | 80/80 | complete |
| `CameraSlotSnapshotEye` | 32 | 56/56 | complete |
| `CameraFollowUpdate` | 32 | 1228/1235 | 7 divisor / state / clamp instructions proved unreachable |
| `CameraSlotFlagBit81` | 32 | 13/13 | complete |
| `CameraFollowAimedVariant` | 32 | 189/189 | complete |
| `CameraFollowGadgetVariant` | 32 | 336/336 | complete |
| `CameraKind3FollowUpdate` | 32 | 144/144 | complete |
| `CameraSlotGetFocusPoint` | 32 | 68/70 | 2 focus-mode instructions proved unreachable |
| `CameraSlotClearScript` | 32 | 13/13 | complete |
| `CameraSlotUpdate` | 32 | 270/270 | complete |

The aggregate is 3541/3576 instructions executed, with 35 instruction
instances covered by five registered proofs and no undeclared gaps. The
final default checker completed in 41.0 seconds. The proofs are
in `tools/mips/check_exclusions.csv`: smoothing receives only positive
divisors from its supported camera callers and direct cases; the fixed update
state is an unsigned byte with all values routed by its prior gates; follow
update constrains the RC follow flag, damping divisor, and hit distance before
the listed branches; and focus mode 1 returns before the later mode test.

Mutation mode compiles two independent entry-write corruptions for every
function. All 52/52 mutations were detected (2/2 for each function).
`--audit-calls` also passes: all 38 external call sites map to named hooks and
all 26 internal camera calls map to ported functions.

## Hooks still outside this batch

These callbacks preserve the original call boundaries and arguments. Their
effects are deterministic test models in the checker; replacing them with
ports or native systems belongs to their respective batches. `FUN_8013d000`
has no recovered descriptive name yet.

| Hook | Original responsibility |
|---|---|
| `Atan2Safe` | Fixed-point two-axis angle calculation. |
| `VectorNormalSS` | Normalize a short vector in memory and return its squared-length result. |
| `SquareRoot0` | Integer square root. |
| `GetGroundHeight` | Query the collision ground height at a position. |
| `rsin`, `rcos` | Fixed-point sine and cosine. |
| `RotTransPers` | GTE rotation, translation, projection, and screen/depth outputs. |
| `SetRotMatrix`, `SetTransMatrix` | Load rotation and translation matrices into the GTE. |
| `RotMatrixY`, `ApplyMatrixSV`, `MatrixIdentity` | Build/apply camera orientation matrices. |
| `memset` | Clear the requested RAM range. |
| `PlayerCanExitStage` | Check whether current player state permits stage exit. |
| `PlayerIsSubmerged` | Query the player's submerged state. |
| `CameraRcCarActive`, `CameraRcCarFollow` | Query and update the remote-control car camera mode. |
| `IsFlightArea` | Check whether the current area uses flight controls. |
| `FUN_8013d000` | Unknown original helper called from the flight-area path. |
| `BossUpdate` | Update the boss camera path. |
| `CameraAreaScriptDispatch` | Dispatch the camera's area-specific script. |
| `CallIndexedHandler` | Invoke the handler selected by the camera script table. |
| `CameraGetPlayerDir` | Get the player's camera-relative direction vector. |
| `AimRequestStart`, `AimRequestClear` | Set and clear the aim-mode request. |
| `AimModeEnter`, `AimModeLeave` | Enter or leave the aim camera mode. |
| `CameraFixedModeOff` | Disable fixed-camera mode. |
| `StageExitPending` | Query pending stage-transition state. |
| `BossAimSfx`, `PlaySfx` | Play boss-aim and general sound effects. |
| `BossKindForArea` | Resolve the current area's boss camera kind. |
| `ScreenFadeSet` | Configure the screen fade. |
| `HudDrawMonkeyInfo` | Draw the monkey information HUD element. |
| `RadarUsable`, `MonkeyFlag248Set` | Check radar availability and query the associated monkey flag. |
| `CameraRadarFocusUpdate`, `CameraRadarFocusSelect` | Update or select the radar's camera focus target. |

## Findings

* `CameraSlotUpdateYaw` depends on the original branch-delay-slot arithmetic
  when wrapping signed yaw. The port keeps the resulting signed wrap behavior.
* `CameraKindAimUpdate` tests a halfword state as unsigned in its gate; signed
  comparisons would take different paths for high-bit values.
* `CameraFollowGadgetVariant` truncates its updated focus height to a signed
  halfword before comparing it. Retaining that store/load boundary is needed
  for equality with the original on overflow cases.

The remaining uncertainty is limited to external routines represented by the
hooks above, especially the unnamed `FUN_8013d000`; this checker proves the
camera entry points against the original with matching modeled hook effects,
not the gameplay behavior of those external routines.
