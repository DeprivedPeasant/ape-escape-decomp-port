# Area 46 boss overlay member 1A0

`1A0.bin` is the Area 46 (`0x46`, Monkey Madness) overlay, loaded at `0x8013D000` while the area runs. The area table maps Area 46 to area member `0F7.bin`; the overlay map lists `1A0` only for Area 46. The boss entry points are `Area46_BossInit` at `0x8013DF90` and `Area46_BossUpdate` at `0x8013F1AC`.

Readable C ports are in [ae_ovl1a0.c](../../src/world/ae_ovl1a0.c) with RAM addresses and field offsets in [ae_ovl1a0.h](../../src/world/ae_ovl1a0.h). The differential checker is [check_ovl1a0.py](../../tools/mips/check_ovl1a0.py). It runs against the real EXE and Area 46 RAM image with `1A0.bin` loaded at its original address.

## Call tree and boundaries

```text
Area46_BossInit
  -> Area46_Boss_InitializeModelEntry
  -> Area46_Boss_InitializeActorRecords
  -> Area46_Boss_InitializeBackdrop
  -> Area46_Boss_ClearShockwaves
  -> Area46_Boss_ClearDebris / Area46_Boss_ResetLoopHandle

Area46_Boss_UpdateEncounterActor
  -> Area46_Boss_OnBossStart -> Area46_Boss_StartLoop
  -> BgmSetVolumeMode / AreaFeature30Request hooks
Area46_Boss_UpdateEntryProximity
  -> MeshInitFromAreaTable hook
Area46_BossUpdate
  -> phase-specific camera target setup and existing camera ports

Area46_Boss_UpdateEntries
  -> Area46_Boss_SpawnDropPickups / Area46_Boss_SpawnEffectNearWrap
  -> Area46_Boss_ProjectAndEmit
Area46_Boss_UpdateHop / Area46_Boss_UpdateBackdrop
  -> Area46_Boss_ProjectAndEmit
Area46_Boss_UpdateShockwaves
  -> Area46_Boss_SpawnEffectNearWrap / Area46_Boss_ProjectAndEmit
Area46_Boss_UpdateDebris
  -> Area46_Boss_DebrisProject / Area46_Boss_DebrisRender
  -> Area46_Boss_DebrisFinalize
```

The other update functions are separate call targets; the helper relationships above show their direct calls. `Area46_BossUpdate` itself handles phase and camera framing and does not dispatch them.

## Ported functions

Every listed C function was differentially checked on eight default cases, except `Area46_Boss_UpdateEntries`, which has 14 directed and randomized cases. Coverage is original instructions reached by the MIPS oracle; the registered native and unreachable instructions are described below.

| Address | Function | Status / cases | Instruction coverage |
|---|---|---|---:|
| `8013D000` | `Area46_Boss_RegisterEntry` | ported + diffed / 8 | 100% |
| `8013D06C` | `Area46_Boss_UpdateEncounterActor` | ported + diffed / 8 | 100% |
| `8013D2A0` | `Area46_Boss_UpdateEntryProximity` | ported + diffed / 8 | 100% |
| `8013D440` | `Area46_Boss_InitializeModelEntry` | ported + diffed / 8 | 100% |
| `8013D63C` | `Area46_Boss_ProjectAndEmit` | ported + diffed / 8 | 100% |
| `8013D77C` | `Area46_Boss_SpawnEffectNear` | ported + diffed / 8 | 100% |
| `8013D890` | `Area46_Boss_SpawnEffectNearWrap` | ported + diffed / 8 | 100% |
| `8013D8BC` | `Area46_Boss_ClearDebris` | ported + diffed / 8 | 100% |
| `8013D8E4` | `Area46_Boss_SpawnDebris` | ported + diffed / 8 | 100% |
| `8013DA4C` | `Area46_Boss_UpdateDebris` | game logic ported + diffed / 8 | 174/244 (71.31%); 70 native instructions registered |
| `8013DE1C` | `Area46_Boss_RumbleLong` | ported + diffed / 8 | 100% |
| `8013DE44` | `Area46_Boss_RumbleShort` | ported + diffed / 8 | 100% |
| `8013DE6C` | `Area46_Boss_RumbleMedium` | ported + diffed / 8 | 100% |
| `8013DE94` | `Area46_Boss_SpawnDropPickups` | ported + diffed / 8 | 100% |
| `8013DF28` | `Area46_Boss_ResetLoopHandle` | ported + diffed / 8 | 100% |
| `8013DF38` | `Area46_Boss_StartLoop` | ported + diffed / 8 | 100% |
| `8013DF60` | `Area46_Boss_LoopStop` | ported + diffed / 8 | 100% |
| `8013DF90` | `Area46_BossInit` | ported + diffed / 8 | 100% |
| `8013E14C` | `Area46_Boss_InitializeActorRecords` | ported + diffed / 8 | 100% |
| `8013E1AC` | `Area46_Boss_ClearEntries` | ported + diffed / 8 | 100% |
| `8013E1D0` | `Area46_Boss_AddEntry` | ported + diffed / 8 | 100% |
| `8013E2BC` | `Area46_Boss_UpdateEntries` | ported + diffed / 14 | 100% |
| `8013EC5C` | `Area46_Boss_InitializeBackdrop` | ported + diffed / 8 | 100% |
| `8013EC94` | `Area46_Boss_SetBackdrop` | ported + diffed / 8 | 100% |
| `8013ECD4` | `Area46_Boss_UpdateBackdrop` | game logic ported + diffed / 8 | 103/104 (99.04%); 1 instruction proven unreachable |
| `8013EE74` | `Area46_Boss_ResetBoss` | ported + diffed / 8 | 100% |
| `8013EEB8` | `Area46_Boss_OnBossStart` | ported + diffed / 8 | 100% |
| `8013EF00` | `Area46_Boss_ClearFlag` | ported + diffed / 8 | 100% |
| `8013EF0C` | `Area46_Boss_SetAnim` | ported + diffed / 8 | 100% |
| `8013EF44` | `Area46_Boss_StepAnim` | ported + diffed / 8 | 100% |
| `8013EFCC` | `Area46_Boss_UpdateHop` | ported + diffed / 8 | 100% |
| `8013F0E0` | `Area46_Boss_UpdatePlayerKnockback` | ported + diffed / 8 | 100% |
| `8013F1AC` | `Area46_BossUpdate` | ported + diffed / 8 | 100% |
| `8013F544` | `Area46_Boss_ClearShockwaves` | ported + diffed / 8 | 100% |
| `8013F5A8` | `Area46_Boss_AddShockwave` | ported + diffed / 8 | 100% |
| `8013F6F8` | `Area46_Boss_UpdateShockwaves` | ported + diffed / 8 | 100% |

## Native hooks

`Area46_Boss_ModelApplyLighting` (`8013D4E0`, 87 instructions) is a whole-function renderer hook for GTE lighting and GPU/ordering-table emission. `Area46_Boss_FillScreenFlash` (`8013F954`, 54 instructions) builds the GPU screen-flash packets and advances the ordering-table cursor. Both have no C game-state body and no `tools/verified.csv` row.

The C game logic also crosses these named native renderer hooks:

| Hook | Work delegated to the host renderer |
|---|---|
| `GTE_SetRotMatrix` | Install a rotation matrix in the GTE. |
| `GTE_SetTransMatrix` | Install a translation matrix in the GTE. |
| `RotTransPers` | Transform and project a point, returning depth and writing screen coordinates and flags. |
| `ModelTransformSkinned` | Transform the boss model's skinned vertices. |
| `Area46_Boss_DebrisProject` | Run the inline GTE projection and supply its scratchpad screen coordinates and depth to the original cull logic. |
| `Area46_Boss_DebrisRender` | Build and link one debris GPU packet using the computed coordinates, color, depth, cursor, and ordering table. |
| `Area46_Boss_DebrisFinalize` | Emit and link the debris packet terminator and advance the packet cursor. |

Registered `UpdateDebris` ranges are `8013DCA0..8013DCBC` and `8013DCC0..8013DCE4` for projection, `8013DD0C..8013DD88` for packet emission, and `8013DD98..8013DDE4` for final packet emission. The depth cull, debris lifetime, movement, loop state, and remaining cursor updates stay in the C port.

Non-renderer original calls left at named host hooks are `BgmSetVolumeMode` (change BGM volume mode), `AreaFeature30Request` (request feature-30 state changes), `MeshInitFromAreaTable` (initialize an actor mesh from its area table), and BIOS `memset` (fill the requested byte range). The oracle also adapts `PadGetState` (read controller state), `SoundStopAllVoices` (stop active voices), and `VSync` (wait for vertical sync) on the encountered original call paths. Existing C ports handle particle and effect spawning, collision bodies, sound playback, camera behavior, model setup and pose, and pure-math SDK calls such as `VectorNormalSS`, `rsin`, and `rcos`. No statically linked SDK library was identified in this member.

## Coverage, exclusions, and findings

The default `--coverage` run passed 294 cases. It covered 2,467 of 2,679 MIPS instructions across the 36 C functions and two whole-function native boundaries. The remaining 212 instructions are registered: 141 belong to the two whole-function native renderers, 70 are the inline `UpdateDebris` GTE/GPU ranges above, and one (`UpdateBackdrop` at `8013ED70`) is unreachable. `timer` is an unsigned byte, so `timer << 9` is always nonnegative and the signed-negative angle-reduction path cannot execute. The only RAM region excluded from memory comparison is the ABI stack window `801FF000..80200000`; the checker compares hook arguments and all other RAM and scratchpad bytes.

Mutation checks inserted two independent RAM-marker mutations at each of the 36 C function entries. All were detected (72/72). The checker wrote `extract/checks/results/check_ovl1a0.json`. `tools/check_verified.py` scans `src/**/ae_*.c` automatically, so its scan list did not need an edit. Thirty-six `diffed` ledger rows were appended with the required notes text `overlay member 1A0.bin`; eight coverage and renderer exclusions were appended to `tools/mips/check_exclusions.csv`.

Instruction-level review found these details that matter to the port:

- The initial active actor-entry path emits particle type 275 from the original delay-slot sequence before normal movement starts.
- The entry retreat transition fires when the updated Z is at least `-3135` **or** the stored turn is less than the tick. Both sides of this disjunction are covered by directed cases.
- A looping boss animation uses `frame - 1` after passing the clip end; it does not wrap to the clip's first frame in that branch.
- Player knockback writes the stored velocity, decays both components by signed division toward zero, and clamps the player Z at `-3136` and `-8120` on the corresponding bounds.
- The phase-25 camera branch reads its threshold metric from boss state offset `+0x0C` and takes the high branch at 73.
