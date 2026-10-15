# Per-frame effects: RAM-image port

`EffectsUpdateAll` (`8006A9F8`) is the stage-loop entry for effects and particles. Its original dispatch order is preserved; completed functions in the task table call their C ports directly.

```text
EffectsUpdateAll
├── FlyingMeshesUpdateDraw
├── EffectObjsUpdate
│   └── EffectObjDraw
├── RandomParticlesUpdate
├── Gadget4TrailUpdate                 (outside this task; named hook)
├── TrailsUpdate
├── FxPlayerState6Update
├── GroundSparksUpdateDraw
├── GadgetFlashUpdate                  (outside this task; named hook)
├── SpriteEffectsDraw
└── PlayerEffectsUpdate
    ├── PlayerFxTrailUpdate
    ├── PlayerCanExitStage
    ├── PlayerWaterEffectsUpdate
    │   ├── PlayerSplash
    │   └── TrailPointAdd
    ├── PlayerTintEffectUpdate
    │   └── SpriteEffectQueue2
    └── TrailPointsDraw
```

`PlayerFxTrailUpdate` also calls `EffectSpawnFromDef` and `SpawnParticle`. `PlayerWaterEffectsUpdate` uses the existing `GetSurfaceType` port and calls `PlaySfx`, `printf`, and `EffectSpawnFromDef` where required. `EffectObjsUpdate` calls the collision-body allocation and registration ports. These dependencies were already ported and are called directly. Pure LIBGTE math functions use the existing `src/core/ae_sdk*.c` ports.

## Validation

`tools/mips/check_effects.py` compares the C result with the original instructions on real EXE and area-member RAM from nine areas. The default run performs 40 deterministic cases for each of the 21 functions, compares return values, all main RAM below the MIPS stack window, scratchpad, and hook-event order, then checks two behavior-changing C mutations per function. The completed run passed 840 cases and caught all 42 mutations. MIPS execution coverage was 2,083 of 2,822 instructions; every gap is registered in `tools/mips/check_exclusions.csv` with its boundary or reachability reason.

| Address | Function | Status | Cases | MIPS instructions covered |
|---|---|---:|---:|---:|
| `800160EC` | `MeshTransform` | diffed | 40 | 0/53; full projection loop is replaced by `GpuProjectMesh` |
| `80017E5C` | `MeshDraw` | diffed | 40 | 0/40; full GTE lighting and packet emission is replaced by `GpuDrawMesh` |
| `8005DC78` | `PlayerCanExitStage` | diffed | 40 | 27/27 |
| `8006A9F8` | `EffectsUpdateAll` | diffed | 40 | 26/26 |
| `8006B168` | `PlayerFxTrailUpdate` | diffed | 40 | 109/109 |
| `8006B800` | `PlayerEffectsUpdate` | diffed | 40 | 18/18 |
| `8006B950` | `PlayerSplash` | diffed | 40 | 42/42 |
| `8006BA2C` | `PlayerWaterEffectsUpdate` | diffed | 40 | 313/313 |
| `8006C334` | `PlayerTintEffectUpdate` | diffed | 40 | 150/150 |
| `8006D4C0` | `SpriteEffectQueue` | diffed | 40 | 33/34; negative cursor fallback at `8006D534` is unreachable |
| `8006D588` | `SpriteEffectQueue2` | diffed | 40 | 8/8 |
| `8006D5A8` | `SpriteEffectsDraw` | diffed | 40 | 156/328; projection and packet range is replaced by `GpuDrawSprite` |
| `8006E1C0` | `FxPlayerState6Update` | diffed | 40 | 199/253; triangle projection and packet range is replaced by `GpuDrawFxPlayerState6` |
| `8006EE3C` | `TrailsUpdate` | diffed | 40 | 50/51; unsigned bounds make `8006EED8` unreachable |
| `8006EF08` | `TrailPointAdd` | diffed | 40 | 32/32 |
| `8006EF88` | `TrailPointsDraw` | diffed | 40 | 94/187; GTE projection, clipping, and packet range is replaced by `GpuDrawTrailPoint` |
| `8006FA10` | `EffectObjDraw` | diffed | 40 | 121/273; native projection and packet range is replaced by `GpuDrawEffectObj` |
| `8006FEE8` | `EffectObjsUpdate` | diffed | 40 | 146/146 |
| `80070560` | `GroundSparksUpdateDraw` | diffed | 40 | 207/378; renderer projection and particle-spawn boundaries are recorded |
| `80070D94` | `RandomParticlesUpdate` | diffed | 40 | 217/217 |
| `80071370` | `FlyingMeshesUpdateDraw` | diffed | 40 | 135/137; `SpawnParticle` jal and delay slot are reproduced by the checker patch |

The two remaining `EffectObjDraw` correction instructions at `8006FB58` and `8006FBA8` are unreachable in the checker fixture: the renderer boundary follows the first radial vertex pair, whose full-turn sine is zero. The sprite queue fallback at `8006D534` follows a branch whose input is the zero-extended cursor plus one (1 through 256). `TrailsUpdate` loads its step and width as unsigned halfwords, so its negative adjustment is impossible. All declared gaps are listed in the exclusion registry.

The renderer boundaries pass the game-computed state to the native renderer while retaining game-owned decisions and RAM writes in C. The effect-object and ground-spark paths use the SDK matrix ports before dispatch. The ground-spark renderer supplies visibility; the original game-side movement, bounce, life decrement, and spawn path then run in C. The test fixture makes a visible bounce reach the `-48` velocity clamp and the `SpawnParticle` path.

## Hooks left open

The following original game functions are outside the task table and remain named callbacks:

| Hook | Original behavior |
|---|---|
| `Gadget4TrailUpdate` (`8006DAF8`) | Samples player motion into the Super Hoop trail and draws it. |
| `GadgetFlashUpdate` (`8006C6D4`) | Updates and draws the gadget flash effect. |
| `SpawnRandomParticle` (`80070B70`) | Adds a randomized type-7 particle to the random-particle queue. |
| `EffectRingDraw` / `TrailDraw` (`8006E5B4`) | Draws one fading trail ribbon. |
| `RibbonDraw` (`8006F3D0`) | Draws a ribbon from a point list. |
| `SparkAddRandom` (`8006E0DC`) | Adds a randomized spark record for player state 6. |

`SetRotMatrix` (`8002FB00`) and `SetTransMatrix` (`8002FB30`) remain SDK hooks, per the task boundary. `GpuProjectMesh`, `GpuDrawMesh`, `GpuDrawSprite`, `GpuDrawTrailPoint`, `GpuDrawEffectObj`, `GpuDrawFxPlayerState6`, and `GpuDrawGroundSpark` are native renderer boundaries for the specified projection and packet-writing work. `LocalPositionPadding` only supplies the stack halfword values that the original water routine reads without initializing; the checker seeds and compares those bytes on both sides.

The ledger records all 21 table functions as `diffed` with `min_cases=40`. `tools/check_verified.py` scans `src/effects/ae_effects*.c` automatically, so no scan-list change was needed.
