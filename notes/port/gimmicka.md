# Gimmick batch A port

## Instruction call tree

Call edges below were taken from the original call inventory and checked against the instruction listings. Calls to
an original function outside this 33-function batch remain named hooks unless the callee was already ported.

| Path | Instruction-level callees |
| --- | --- |
| Save and stage setup | `GimmickSaveStates` → `GimmickSaveEntryAdd`; `GimmickSpawnAll` → `GimmickInitFromProgress` and `GimmickRestoreStates`; `GimmickInitFromProgress` → `ProgressFlagGet` / `ProgressGetByte` hooks; `GimmickSaveApplyProgress` → `ProgressFlagSet` / `ProgressSetByte` hooks. |
| Group update | `GimmickUpdateAll` dispatches the area handler table indirectly; those unported per-area callbacks use the `GimmickHandler` hook. |
| Carry and swing | `GimmickCarryBodiesSwing` → `GimmickSwingCarryPoint`; `GimmickSwingCarryPoint` → `GimmickSwingAngle`, SDK `rcos` / `rsin`; `GimmickSwingFirstStep` → SDK `RotMatrixZYX`; `GimmickSwingProbeWall` → `GetGroundHeight` (the native world port, with its `GetSolidHeight` hook). Carry paths call the `CameraNudge` hook. |
| Swing doors | `Gimmick_SwingDoor_Init` → already-ported `SwingDoorSetPivot` and the `SwingDoorBodyDelta` hook; update calls the same door displacement hook, `GimmickAnyBodyInside`, `GimmickPlayTypeSfx`, and `SwingDoorSetPivot`. |
| Lift, threshold, and slide doors | Lift and slide updates call `GimmickAnyBodyInside` and `GimmickPlayTypeSfx`. Threshold update calls `GimmickTypeToSfx` and sound hooks. Slide init/update call the `SlideDoorMove` hook. |
| Switches and hit objects | Switch, pair-toggle, and hit paths call the already-ported `GimmickActivate`; mesh changes use `PropSetMesh` or `MeshInitFromAreaTable`. Hit updates also call `PlaySfxAt` and `GimmickTouchCheck`. |
| Spinner | `Gimmick_SpinRoller_Update` calls SDK `RotMatrixZYX` for each configured prop. |

`GimmickActivate`, `SwingDoorSetPivot`, `GimmickAnyBodyInside`, `GimmickPlayTypeSfx`, `GimmickTypeToSfx`,
`GimmickSwingCarryPoint`, and the SDK/world functions named above are direct calls where already ported. `SwingDoorBodyDelta`
and `SlideDoorMove` are outside this task's function list and remain named hooks.

## Port and hook boundary

The 33 functions below are hand-written structured C in `src/world/ae_gimmick_a.c`; field and global addresses are named
in `ae_gimmick_a.h`. Small static helpers handle signed-halfword conversion, record addressing, hook calls, and collider
origin copying. No MIPS register frame or per-PC control flow is emitted.

Hooks still supplied by the native caller:

| Hook | Original responsibility |
| --- | --- |
| `GimmickHandler` | Indirect area-table init/update callbacks not included in this batch. |
| `ProgressFlagGet`, `ProgressFlagSet`, `ProgressGetByte`, `ProgressSetByte` | Progress storage read/write calls. |
| `CameraNudge` | Camera eye/target displacement when carried bodies move. |
| `BodyInsideBox`, `GimmickTouchCheck` | Collision-volume and touch queries. |
| `PlaySfx`, `PlaySfxAt`, `SoundLoopStart`, `SoundEmitterPan`, `SoundLoopStop` | Audio playback and loop control. |
| `PropSetMesh`, `MeshInitFromAreaTable` | Renderer-owned prop mesh and area-table mesh setup. |
| `GetSolidHeight` | Collision height query used by the native `GetGroundHeight` path. |
| `SwingDoorBodyDelta` (`0x8008F308`) | Door-specific body displacement routine outside the requested list. |
| `SlideDoorMove` (`0x8008FAD0`) | Door-specific movement routine outside the requested list. |

Hook calls are recorded and compared in order with their original arguments. Missing hooks fail closed. The C side calls
the already-ported `GimmickActivate` and `SwingDoorSetPivot` directly.

## Differential status

`tools/mips/check_gimmick_a.py` runs against the original EXE and real area member `0x0E`, with deterministic edge
fixtures, randomized save records, and the shared hook models. Default run: 1,162 differential cases; all RAM outside
the declared stack window, scratchpad bytes, return values, and hook calls match.

| Address | Function | Cases | Instructions covered |
| --- | --- | ---: | ---: |
| `8005AD90` | `GimmickSaveTableReset` | 4 | 26/26 |
| `8005ADF8` | `GimmickSaveStates` | 2 | 85/85 |
| `8005AF4C` | `GimmickRestoreStates` | 2 | 121/121 |
| `8005B130` | `GimmickSaveEntryAdd` | 66 | 122/122 |
| `8005B318` | `GimmickInitFromProgress` | 2 | 90/90 |
| `8005B480` | `GimmickSaveApplyProgress` | 1 | 46/46 |
| `8005B538` | `GimmickSpawnAll` | 3 | 85/85 |
| `8005B68C` | `GimmickUpdateAll` | 4 | 76/76 |
| `8005BB5C` | `GimmickGetState` | 6 | 15/15 |
| `8005BB98` | `GimmickTypeToSfx` | 274 | 30/30 |
| `8008AA6C` | `GimmickCarryBodiesByDelta` | 7 | 81/81 |
| `8008ABB0` | `GimmickCarryBodiesSwing` | 5 | 89/89 |
| `8008AD14` | `GimmickSwingCarryPoint` | 39 | 161/161 |
| `8008AF98` | `GimmickSwingAngle` | 270 | 46/50 |
| `8008B060` | `GimmickSwingFirstStep` | 10 | 108/114 |
| `8008B228` | `GimmickSwingAdvance` | 240 | 95/95 |
| `8008B3A4` | `GimmickSwingProbeWall` | 11 | 275/277 |
| `8008E5C0` | `Gimmick_SwingDoor_Init` | 12 | 210/210 |
| `8008E90C` | `Gimmick_SwingDoor_Update` | 13 | 118/118 |
| `8008EAE4` | `GimmickPlayTypeSfx` | 3 | 29/29 |
| `8008EB58` | `GimmickAnyBodyInside` | 4 | 70/70 |
| `8008EF60` | `Gimmick_LiftDoor_Init` | 6 | 87/87 |
| `8008F0BC` | `Gimmick_LiftDoor_Update` | 15 | 147/147 |
| `8008F468` | `Gimmick_Init_ThresholdDoor` | 2 | 53/53 |
| `8008F53C` | `Gimmick_ThresholdDoor_Update` | 6 | 121/121 |
| `8008F720` | `Gimmick_SlideDoor_Init` | 11 | 109/109 |
| `8008F8D4` | `Gimmick_SlideDoor_Update` | 13 | 127/127 |
| `80090370` | `Gimmick_Switch_Init` | 6 | 100/100 |
| `80090500` | `Gimmick_Switch_Update` | 3 | 41/41 |
| `800905A4` | `Gimmick_Init_PairToggle` | 2 | 47/47 |
| `80090660` | `Gimmick_Hit_Update` | 5 | 61/61 |
| `800909A8` | `Gimmick_Init_Counter` | 100 | 21/21 |
| `800909FC` | `Gimmick_SpinRoller_Update` | 15 | 220/220 |
| **Total** | **33 functions** | **1,162** | **3,112/3,124** |

All 33 functions have full reachable-instruction coverage. Twelve instructions are declared unreachable in
`tools/mips/check_exclusions.csv`: four signed divide trap guards in `GimmickSwingAngle`, six divide trap guards in
`GimmickSwingFirstStep`, and the fourth-sample repeat branch in `GimmickSwingProbeWall`. The first two sets require a
zero divisor or signed overflow after nonzero signed-halfword ranges have been checked; bounded numerators prevent
overflow. In the wall probe, the ground-height hook writes a separate scratch region, and the fourth sample can only be
raised after its earlier comparison, so the repeat branch cannot become true. The checker also declares the bounded
`801FF000..80200000` stack window because the original and host C frames differ; all lower RAM and scratchpad bytes are
still compared.

Five checker spans include out-of-line switch arms beyond the size recorded by the inventory: SwingDoor init/update,
LiftDoor update, and SlideDoor init/update. Their `FUNCS` extents end at the next function entry so that the coverage
check includes the reachable jump-table arms. The inventory sizes remain unchanged.

## Instruction findings

- Type-1 lift doors share a state/timer path for every subtype. Subtypes `10` and `12` choose one travel-parameter
  formula; other type-1 subtypes choose the second formula. The linked type-1 path starts in state 0 and does not
  perform the type-2 collider/prop height shift. This corrected a mismatch exposed by `lift_init_type1_other_linked`.
- Swing and slide doors use type-1 state transitions separately from type-2 link handling. Their state changes also
  determine whether the original calls the displacement helper.
- Threshold-door comparisons use the sum of signed linked `PARAM_16` values. Positive thresholds keep/start a loop;
  nonpositive thresholds stop an active loop, with a positional one-shot when leaving state 5.
- Spinner speed decay is `speed - ((speed / 100) * 8 + 8)` using signed integer division. Each prop's rotation is
  repeatedly wrapped in signed 0x400-unit steps, and subtype chooses the X or Z axis.
- Several inventory byte sizes end before jump-table arms that the original branches to. Those arms are included in
  checker coverage as described above.

## Mutation validation

The default `--mutate` run catches all 67 function-scoped mutations, with at least two per original function. The two
camera-hook mutations perturb the displacement argument; the suite also checks path thresholds, state transitions,
table values, mesh/SFX arguments, geometry, and spinner motion. Result: `67/67` caught.
