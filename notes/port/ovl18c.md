# Area 02 overlay member 18C

The 12 KiB member `extract/members/18C.bin` is loaded at `0x8013D000` over area 02's real `094.bin` RAM image. Its private tables stay at their original RAM addresses and are named in [ae_ovl18c.h](../../src/world/ae_ovl18c.h). The readable C ports and differential harness are [ae_ovl18c.c](../../src/world/ae_ovl18c.c) and [check_ovl18c.py](../../tools/mips/check_ovl18c.py).

## Call tree and boundaries

```text
Area02_EnemyType0B_Update
  -> Area02_EnemyAnimationTriggerUpdate
  -> Area02_UpdateEnemyWake / Area02_UpdateEnemyWakeOrigin
  -> Area02_InitializeEffects / Area02_UpdateEffects
       -> Area02_SpawnRadialEffect / Area02_SpawnEffectBurst
            -> Area02_SpawnEffectParticle / Area02_EffectBelowGround
       -> Area02_EnemyModelDraw [native renderer]
  -> Area02_EnemyHitUpdate / Area02_EnemySplashUpdate
       -> Area02_EnemyEnterHitReaction / Area02_EnemyEnterDefeat
            -> Area02_SpawnEnemyHitParticle
  -> Area02_EnemyPhase6Update

Area02_StageSpecificEnemyUpdate
  -> Area02_InterpolateAnimValue
  -> Area02_PlayerWithinHeightAndRadius / Area02_PlayerOutsideHeightAndRadius
  -> existing Zako, body, path, particle and shadow helpers
Area02_SurfaceEffectsInit
Area02_EffectDraw [ported record lifetime/animation; renderer hook per visible record]
Area02_SurfaceDraw [ported record animation/state; projection and packet renderer hooks]
```

`Area02_EnemyModelDraw` (`8013D700..8013D7C4`, 50 instructions) remains a whole-function renderer hook. Its only stores are stack-local: `8013D704`, `8013D720`, and `8013D730` save registers; `8013D744`, `8013D748`, and `8013D74C` prepare a three-word local vector; `8013D7B4..8013D7BC` restore registers. The model transform and GPU/ordering-table calls use the renderer-owned enemy model instance. There are no stores to gameplay records.

`Area02_EffectDraw` (`8013E33C..8013E94C`, 389 instructions) is ported in C through lifetime and animation updates. The renderer hook `Area02_EffectDrawRecord` replaces only the projection, clipping, and packet block (`8013E590..8013E924`) and receives the updated record fields and ordering-table cursor. `Area02_SurfaceDraw` (`8013E950..8013F0BC`, 476 instructions) is ported in C through all 230 animation records, including initialization, coordinate updates, visibility state, and cursor changes. Named renderer hooks replace the two GTE projection blocks and the primary/secondary packet blocks; the projection hooks return the same scratch results consumed by the C visibility logic, while packet hooks receive computed coordinates, dimensions, record fields, and cursor. Thus the checker compares the effect and surface records, animation phases, cursor, other RAM, and scratchpad after each draw call.

The EXE helper hooks retain original names and argument words: `PlayerWithinRadiusXZ`, `ObjCooldown`, `ObjPlayerNearPointXZ`, `ObjTurnToPlayer`, `ObjSpawnParticleAt`, `Zako_Nop`, `ObjAddHitBody`, `ObjGetLinkedObjState`, `GetSolidHeight`, `MatrixLookDir`, and `DrawShadow`. Area-02 movement now calls `ObjMove`, `ObjFollowPath`, `ObjSetPosToPathPoint`, `ObjFacePathPoint`, and `ZakoHooksAdd` directly; `ObjFollowPath` reaches the ported seek and advance helpers. Existing ports are called directly for ground height, particle/effect spawning, turn helpers and `VectorNormalSS`.

## Ported functions

Each function below is ported in readable C and passed eight differential cases on the real area 02 plus overlay image. The shared `FUN_8013F0C0` entry was already ported and diffed; it is unchanged and keeps its existing ledger row.

| Address | Function | Status / cases |
|---|---|---|
| `8013D000` | `Area02_SpawnAmbientParticle` | ported + diffed / 8 |
| `8013D0B0` | `Area02_InterpolateAnimValue` | ported + diffed / 8 |
| `8013D1C4` | `Area02_PlayerWithinHeightAndRadius` | ported + diffed / 8 |
| `8013D224` | `Area02_PlayerOutsideHeightAndRadius` | ported + diffed / 8 |
| `8013D294` | `Area02_EnemyAnimationTriggerUpdate` | ported + diffed / 8 |
| `8013D3B8` | `Area02_UpdateEnemyWakeOrigin` | ported + diffed / 8 |
| `8013D430` | `Area02_UpdateEnemyWake` | ported + diffed / 8 |
| `8013D518` | `Area02_EnemyType0B_Update` | ported + diffed / 8 |
| `8013D660` | `Area02_CopyEnemyModel` | ported + diffed / 8 |
| `8013D7C8` | `Area02_InitializeEffects` | ported + diffed / 8 |
| `8013D81C` | `Area02_FindFreeEffectSlot` | ported + diffed / 8 |
| `8013D858` | `Area02_SpawnRadialEffect` | ported + diffed / 8 |
| `8013D990` | `Area02_EffectBelowGround` | ported + diffed / 8 |
| `8013D9CC` | `Area02_SpawnEffectParticle` | ported + diffed / 8 |
| `8013DA0C` | `Area02_SpawnEffectBurst` | ported + diffed / 8 |
| `8013DA94` | `Area02_UpdateEffects` | ported + diffed / 8 |
| `8013DC70` | `Area02_SpawnEnemyHitParticle` | ported + diffed / 8 |
| `8013DC9C` | `Area02_EnemyHitUpdate` | ported + diffed / 8 |
| `8013DD2C` | `Area02_EnemyEnterHitReaction` | ported + diffed / 8 |
| `8013DDBC` | `Area02_EnemySplashUpdate` | ported + diffed / 8 |
| `8013DE20` | `Area02_EnemyEnterDefeat` | ported + diffed / 8 |
| `8013DEF8` | `Area02_EnemyPhase6Update` | ported + diffed / 8 |
| `8013DF54` | `Area02_StageSpecificEnemyUpdate` | ported + diffed / 8 |
| `8013E1E0` | `Area02_SurfaceEffectsInit` | ported + diffed / 8 |
| `8013E33C` | `Area02_EffectDraw` | game logic ported + diffed / 8; projection/clip/packet hook |
| `8013E950` | `Area02_SurfaceDraw` | game logic ported + diffed / 8; projection/packet hooks |

## Validation and instruction findings

The default checker run passed 208 cases across the 26 ported C functions. Coverage reached 1,679 of 2,096 original instructions. The three interpolation divide guards (`8013D10C`, `8013D128`, `8013D140`) are proven unreachable. The 50-instruction `Area02_EnemyModelDraw` remains a whole renderer hook because its listing shows no gameplay-record writes. In `Area02_EffectDraw`, only 230 renderer instructions are replaced. In `Area02_SurfaceDraw`, 134 instructions are registered: the GTE projection blocks, primary/secondary packet blocks, and guarded divisions whose depth range is already checked as 1..2047. Mutations at the 26 C function entries were all caught (52/52).

The instruction listings exposed several details not apparent from pseudocode:

- `Area02_SurfaceEffectsInit` writes each valid record's flag and parameter in the branch delay slots, so those writes occur whether or not the first ground-height result is below the source height.
- `Area02_UpdateEnemyWakeOrigin` uses object Y minus 180; the hit-body position uses object Y minus 250.
- `Area02_UpdateEffects` writes effect positions into the model and normalizes the model's facing vector in place. The velocity slots are not the normalization destination.
- `Area02_StageSpecificEnemyUpdate` passes body radius to `Zako_Nop`.
- `ae_ObjTurnTowardsXZ` calls `Atan2Safe` with its two consumed values, `dx` and `dz`; the generic four-word hook ABI's unused slots are zero. The checker compares only the two arguments consumed by `Atan2Safe`. The overlay checker uses the same per-hook argument count.
- The ground-height adapter preserves the original stale `a3` word used by `GetSolidHeight` in `Area02_UpdateEffects`.

The only declared shared stack value is the unwritten fourth halfword copied through `ObjPuffAt` during `Area02_EnemyHitUpdate`; the checker seeds the same value on both sides and records the exclusion. Main RAM outside `801FF000..80200000` and the full scratchpad are compared.

`tools/check_verified.py` scans this new `ae_*.c` module automatically, so its scan list was not changed. Ledger rows are appended in `tools/verified.csv`, and the checker exclusions are appended in `tools/mips/check_exclusions.csv`.
