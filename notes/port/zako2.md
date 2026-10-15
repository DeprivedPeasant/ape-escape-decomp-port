# Zako and projectile batch 2

This batch covers the second half of the stage enemy update table in `extract/codex-task/task.md`. The C ports are in
`src/world/ae_zako2.c` with RAM addresses, field offsets, and hook types in `src/world/ae_zako2.h`.

## Call tree from the EXE

The call edges below were checked against the original EXE instructions and `notes/inventory/calls.csv`:

```text
Mode_Stage (8005BF70)
└─ ZakoUpdateAll (80050C34)
   ├─ ZakoSpawnNearby (80050B04)
   ├─ ZakoUpdateLoop (800511A4), when enemy updates are enabled
   │  ├─ per-kind handlers through the table at 800AF350
   │  └─ FUN_8013f660 (8013F660) for object kind 0x3F, at 80051360
   ├─ ZakoCullAndDraw (80051534)
   ├─ ProjectilesUpdateDraw (80059B08)
   │  ├─ ProjectilesUpdate (80059FA4), unless flags 0x06 at 800F4450 are set
   │  │  └─ ProjectileAddHitBody (80059A54) on the hit-body paths
   │  └─ ProjectilesDraw (80059B54)
   └─ Draw_Nop (8005E000)

SpawnPickups (80083AA8)
├─ IsTimeAttack (80097678)
├─ PickupPickKindA (8008373C) or PickupPickKindB (80083790)
└─ PickupSpawnSlot (80083844)

DebugTextPrintfWhite (8008CBFC)
├─ DebugTextPrintf (8008C6F0)
│  └─ DebugTextDrawGlyph (8008C434), once per emitted glyph
└─ ColorFadeSet (8008C3C0)
```

`SpawnPickups` is also called from event paths outside this frame tree, including `Indicator_Activate` (800717B4).
`FUN_8013f660` is a real call target in the EXE's kind-0x3F path, but its one-byte Ghidra RAM entry is outside the
loaded EXE text (`80010000..800B4FFF`); the 16 bytes at its RAM address are zero in the checker image. Its body belongs
to a runtime overlay and cannot be recovered from this executable image. The C entry is a named forwarding shim so
the native host can bind the overlay behavior without inventing it.

## Port and differential results

`tools/mips/check_zako2.py` compares the original instructions with the C ports on five real area members (area IDs
`01`, `02`, `04`, `05`, and `07`), directed boundary cases, and deterministic randomized projectile cases. The default
run completed 127 cases in under one second with no return-value, RAM, scratchpad, or hook-event mismatches.

| Original function | C entry | Cases | Original instructions covered |
|---|---|---:|---:|
| `ProjectileAddHitBody` | `ae_ProjectileAddHitBody` | 4 | 45/45 |
| `ProjectilesUpdateDraw` | `ae_ProjectilesUpdateDraw` | 4 | 19/19 |
| `ProjectilesDraw` | `ae_ProjectilesDraw` | 6 | 111/276; 165 replaced renderer instructions |
| `ProjectilesUpdate` | `ae_ProjectilesUpdate` | 59 | 724/724 |
| `PickupPickKindA` | `ae_PickupPickKindA` | 5 | 21/21 |
| `PickupPickKindB` | `ae_PickupPickKindB` | 5 | 45/45 |
| `SpawnPickups` | `ae_SpawnPickups` | 10 | 132/132 |
| `ColorFadeSet` | `ae_ColorFadeSet` | 4 | 7/7 |
| `DebugTextDrawGlyph` | `ae_DebugTextDrawGlyph` | 8 | 22/175; 153 replaced renderer instructions |
| `DebugTextPrintf` | `ae_DebugTextPrintf` | 11 | 221/249; 28 replaced renderer instructions |
| `DebugTextPrintfWhite` | `ae_DebugTextPrintfWhite` | 3 | 25/25 |
| `IsTimeAttack` | `ae_IsTimeAttack` | 4 | 16/16 |
| `FUN_8013f660` | `ae_FUN_8013f660` overlay shim | 4 | 0/1; no body is loaded at the target address |

All gaps are registered in `tools/mips/check_exclusions.csv`. Renderer replacements carry and compare the game-computed
inputs and resulting packet/cursor updates:

* `ProjectilesDraw`: `80059CD0..80059F60` calls `DrawProjectileSprite` with the projectile, position, kind, renderer
  state, and primitive pointer.
* `DebugTextDrawGlyph`: `8008C474..8008C56C` and `8008C57C..8008C6E0` call `DrawDebugGlyph` with the glyph, cursor,
  color, glyph data or ordering table, and packet pointer; cursor and packet advancement remain in the C logic.
* `DebugTextPrintf`: `8008CA34..8008CA9C` plus delay slot `8008CAA0` calls `FinishDebugText` with the ordering table,
  packet, fade values, draw color, and primitive count; the C port advances the packet by eight bytes.

The stack window `801FF000..80200000` is excluded because the C host stack and original MIPS stack frames differ;
all other main RAM and all scratchpad are compared. The one-byte target and each replaced renderer range have their own
registered coverage proof. There are no other uncovered instructions.

Mutation mode inserted 30 distinct wrong RAM writes at C function entries, at least twice for every table entry; the
default differential comparisons caught all 30. This includes the `FUN_8013f660` shim's forwarded object argument.

## Direct ports, native hooks, and open overlay

The batch calls existing verified code directly for collision-body allocation/registration, ground-height queries,
particle spawning, camera shake, matrix construction, vector normalization, square root, pickup slot allocation,
the two pickup-kind helpers, and `IsTimeAttack`.

Original game functions that remain named hooks are:

* `PlaySfx` (`800877F8`): starts a sound effect for projectile events.
* `DrawShadow` (`8001F604`): draws projectile shadows.
* `ModelTransformRigid` (`800161C0`) and `ModelDraw` (`80017CD4`): apply and draw the rigid projectile model.
* `FUN_8013f660` (`8013F660`): runtime overlay handler for object kind `0x3F`; the hook receives the object address.

The renderer owns `DrawProjectileSprite`, `DrawDebugGlyph`, and `FinishDebugText`, which replace inline GTE/GPU work.
The existing world hook `GetSolidHeight` (`8004D8C0`) remains the platform/world boundary used by the validated
ground-height port.

## Instruction-derived findings

* `SpawnPickups` advances each difficulty weight row by 25 bytes: the MIPS arithmetic is `3*d`, then `24*d`, then
  `+d`. When all weighted buckets miss, `$s0` was set to kind 1 before the scan, so the fallback is kind 1.
* With `choose_kind == 0`, the time-attack gate can select kind 3 without changing the one-item count. The two-item
  kind-3 override belongs to the weighted-selection branch.
* Kind-5 projectiles that move below ground or cross the ground during the step take the early render-field path and
  skip the later timer/proximity hit logic.
* Kind-6 projectiles increment their own timer. Their player-distance calculation biases Y by 128; field `0x0F` is
  set at distances `<= 0x960F`, while the body-hit check uses `< 0x4000`. The target-flag path adds 2 to Y velocity
  only when the timer byte allows the aim update.
* Kind 7/8/9 bounce sparks occur only when the bounced Y velocity is less than `-10`.
* Debug `%d` negates in 32-bit MIPS arithmetic before division, including the wraparound case for `INT_MIN`. `%5d`
  pads the cursor but does not add to the primitive count.
* The final debug-text ordering-table base is `800F03D0`; glyph drawing uses `800F04D0`.

The exclusion registry is append-only. An early `shared_random` row records an initial stale-`$s0` hypothesis for
`SpawnPickups`; the later instruction check at `80083B74` showed `$s0` is initialized to 1 before the scan. The
adjacent `dropped_mutation` row records that correction. The checker does not use the superseded shared-random
assumption.
