# Area setup and reset ports

This batch ports the 43 functions listed in `extract/codex-task/task.md` to
`src/world/ae_areaset.c` and `src/world/ae_areaset.h`. The checker runs original
EXE and overlay instructions from real member images and compares their return
values, writes below the bounded stack window, scratchpad contents, and named
hook calls.

## Call tree from instruction listings

```text
AreaLoad
  AreaGetKind / AreaGetLevel
  HeapInit
  AreaBumpAllocSet
  AreaSetZoneBoxes
  AreaSetIndicatorModelA ─┐
  AreaSetIndicatorModelB ─┼─> ModelInit
  AreaSetSpecialActor ────┼─> AreaBumpAlloc ─> HeapAlloc ─> SmallocError
                          │                         └──────> printf, exit
                          └─> StaticModelInit (feature 0x40 only)
  AreaSetFeature30 / 31 / 32
  AreaSpecialFlagsClear
  MonkeyLodModelsInit ───────> ModelInit
  TargetListClear
  TableClearDAT_800BEF60
  MessageBoxClose
  FxSlotsReset ──────────────> ProjectileGroupsClear
  ProjectilesReset
  EffectsReset ──────────────> EffectsResetSubsystems
                    └────────> PlayerFxBlinkReset
  EffectsResetSubsystems ────> FxTableClear12
                         ├───> EffectObjsReset
                         ├───> RandomParticlesReset
                         ├───> ae_Gadget4TrailReset (already verified)
                         ├───> TrailsReset
                         └───> SparkTableClear
  BossInit ─> BossKindForArea ─> area-specific overlay init port
  RcCarDespawn ──────────────> CollisionBodyUnregister
```

`exit` and `printf` are PsyQ BIOS syscall veneers. `BossInit` selects the
overlay callback from the EXE dispatch table. The five callback bodies are in
the real area-code overlays, not in the EXE image; the table's one-byte sizes
are Ghidra RAM-stub sizes. The checker loads each overlay member at
`0x8013D000` and tests the actual instructions:

| Area | Overlay member | Function | Code span |
|---|---:|---|---:|
| 0x44 | 0x19E | `FUN_8013dc68` | 0x1BC bytes |
| 0x46 | 0x1A0 | `FUN_8013df90` | 0x1BC bytes |
| 0x53 | 0x1A5 | `FUN_8013d1ac` | 0x174 bytes |
| 0x56 | 0x1A6 | `FUN_8013dae8` | 0x258 bytes |
| 0x57 | 0x1A8 | `FUN_8013da10` | 0x180 bytes |

The overlay ports reproduce their direct object/table writes and call the
already-ported `CollisionBodyRegister` for boss bodies. Calls to helpers that
are outside this batch remain individual hooks named after their original
function. The checker loads real area and matching overlay members for areas
0x44, 0x46, 0x53, 0x56, and 0x57, and seeds the collision-table pointer from
the member record at offset 0x4E0. Other scenarios use real area members for
areas 0x01, 0x07, 0x08, 0x09, 0x16, 0x21, 0x30, 0x33, and 0x42. Real feature
records and model headers are used for setters and actor setup.

## Validation

Default run (`.venv/Scripts/python.exe tools/mips/check_areaset.py --coverage`):
177 cases across the 43 functions. All direct instruction coverage is complete
except the six instructions excluded below; the result is recorded in
`extract/checks/results/check_areaset.json`. Per-function default case counts
are printed by the checker and recorded in `tools/verified.csv`.

| Function | Cases | Function | Cases | Function | Cases |
|---|---:|---|---:|---|---:|
| AreaGetKind | 15 | AreaGetLevel | 15 | ModelInit | 6 |
| StaticModelInit | 1 | AreaSetZoneBoxes | 2 | ProjectilesReset | 4 |
| BossKindForArea | 6 | BossInit | 6 | EffectsReset | 4 |
| EffectsResetSubsystems | 4 | PlayerFxBlinkReset | 3 | SparkTableClear | 4 |
| TrailsReset | 4 | EffectObjsReset | 4 | RandomParticlesReset | 4 |
| FxTableClear12 | 3 | IndicatorsReset | 4 | AreaSetIndicatorModelA | 6 |
| AreaSetIndicatorModelB | 4 | AreaSpecialFlagsClear | 3 | MonkeyLodModelsInit | 6 |
| TargetListClear | 4 | TableClearDAT_800BEF60 | 3 | FxSlotsReset | 4 |
| ProjectileGroupsClear | 4 | HeapInit | 3 | AreaBumpAllocSet | 1 |
| HeapAlloc | 11 | AreaBumpAlloc | 5 | SmallocError | 1 |
| MessageBoxClose | 3 | RcCarDespawn | 3 | AreaSetSpecialActor | 10 |
| AreaSetFeature30 | 2 | AreaSetFeature31 | 2 | AreaSetFeature32 | 2 |
| exit | 1 | printf | 1 | FUN_8013d1ac | 1 |
| FUN_8013da10 | 2 | FUN_8013dae8 | 2 | FUN_8013dc68 | 2 |
| FUN_8013df90 | 2 |  |  |  |  |

Coverage exclusions, both caused by the nonreturning PsyQ `exit` call in
`SmallocError`:

- `HeapAlloc` 0x8008C0F4..0x8008C0FC follows its call to `SmallocError` and can
  execute only if process termination returns.
- `SmallocError` 0x8008C384..0x8008C38C is after the exit veneer returns. The
  checker patches that return point to the MIPS call sentinel.

The default suite is supplemented by a seeded 561-case run (`--cases 5
--seed 94423`) covering randomized area lookups, reset inputs, heap sizes and
occupancy, bump cursors, overlay progress values, and overlay record kinds /
positions. All cases pass. Mutation testing inserts two distinct RAM stores
into each port: 86 mutations were tested and all 86 were caught.

`tools/check_verified.py` sees all 43 new rows as diffed, but exits with five
duplicate-name errors for the pre-existing `ae_GimmickSwing*` rows from
`ae_gimmick_a.c` and `ae_gimmick_b.c` (the duplicates are present in `HEAD`).
It also reports missing result files for 29 other checkers. The append-only
ledger rule means those older rows were left untouched; no area-set row causes
an error.

## Hooks left outside this batch

- `ProgressGetByte` (0x800963AC) reads one byte from the progress/save table at
  0x800DFDD8. Four boss overlays use it to skip or run their initialization.
- `printf` and `exit` implement the PsyQ formatted-output and process-exit
  syscalls. `SoundLoopStop` (0x8008806C) is retained through the shared body
  hook interface.
- Overlay-local helpers outside the 43-function table remain hooks. Their
  call sites show their role: each boss overlay delegates per-object setup,
  animation/flag changes, and effect, projectile, spawn, arena, boss, segment,
  or rock initialization. Their addresses and original symbol names are
  explicit in `AeAreaSetHooks` and `OVERLAY_HELPERS` in the checker. The named
  hooks set boss animations (`Area44_Boss_SetAnim`, `Area46_Boss_SetAnim`,
  `Area57_Boss_SetAnim`), clear boss flags (`Area46_Boss_ClearFlag`,
  `Area57_Boss_ClearFlag`), or set the Area 56 camera-object animation
  (`Area56_Boss_SetCameraObjAnim`). Several helpers remain `FUN_<address>`
  because no more specific symbol name is available:
  area 0x44 uses `FUN_8013d1f0`, `FUN_8013f68c`, `FUN_8013f130`,
  `FUN_8013ecf4`, `FUN_8013d52c`, `FUN_8013db98`, and `FUN_8013dc00`; area
  0x46 uses `FUN_8013d440`, `FUN_8013f544`, `FUN_8013e14c`, `FUN_8013ec5c`,
  `FUN_8013d8bc`, and `FUN_8013df28`; area 0x53 uses `FUN_8013d10c`,
  `FUN_8013f4f0`, `FUN_8013e8c0`, `FUN_8013de14`, `FUN_8013e4d8`, and
  `FUN_8013d804`; area 0x56 uses `FUN_8013d000`, `FUN_8013f86c`,
  `FUN_8013e4a8`, `FUN_8013d47c`, `FUN_8013e070`, `FUN_8013e7f8`, and
  `FUN_8013f080`; area 0x57 uses `FUN_8013d000`, `FUN_8013e020`,
  `FUN_8013d33c`, `FUN_801407bc`, `FUN_8013d9a8`, and `FUN_8013ee9c`.
  Their precise internal behavior was not ported or verified in this batch.

These hooks are compared by original function name and supplied arguments on
both sides; their internal behavior is outside this batch.
