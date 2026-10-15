# Shared Zako helpers

This batch ports the 42 listed EXE functions into [ae_zakohelpers.c](../../src/world/ae_zakohelpers.c), with declarations and named RAM fields in [ae_zakohelpers.h](../../src/world/ae_zakohelpers.h). `ZakoUpdateGenericEnemy` now calls these functions directly. `AeZako1Hooks` no longer contains callbacks for the ported helpers; the remaining `AeZakoAiHooks` entries are the four out-of-scope overlay handlers described below.

## Call tree

`ZakoUpdateGenericEnemy` runs the common prelude (`Zako_InitDefaultStats`, `Zako_Nop`, `ZakoReturnIfFar`, `Zako_RecoverFromStun`) and dispatches by the object's state:

- State 1: `Zako_WanderBounce` then `ZakoStateWatch`.
- State 2: `ObjTurnTowardsPlayerXZ`.
- State 3: `ZakoChasePlayer` then `ZakoStateHurt`.
- State 4: `ZakoEnterIdleWait`.
- State 5: `ZakoWalkToTarget`, `ObjCooldown`, then `ZakoStateWatch` when the timer expires.
- States 6 and 7: acceleration / walking and fleeing movement helpers.
- State 10: `ObjTurnToPlayer`, `ObjFallToGround`, then the landing animation or `ZakoEnterDeath`.
- State 11: `ObjClampFrame`, `ObjGetLinkedObjState`, `ObjAddHitBodyIfPlayerNear`, and `ObjFallAndBounce`.
- State 12: recovery movement through `ObjAccelerate`, `ObjSetVelFromFacing`, `ObjWalkOnGround`, and `ZakoEnterIdleWait`.

The shared movement subgraph is:

- `ZakoWalkToTarget` → `ObjWalkToPoint` → `ObjTurnTowardsXZ` / `ObjSetVelFromFacing`; then `ObjTryStepOnGround`.
- `Zako_WanderBounce` → `ObjWanderOrReturn` → `ObjTargetBeyondRange` and either `ObjSetVelTowardTarget` → `ObjSetVelAlongDir`, or `ObjWanderStep` → `ObjSetVelAlongDir`.
- `ZakoChasePlayer` → `ObjChasePlayer`, `ObjTryStepOnGround`, `ObjFootstepFx`, `ObjTargetBeyondRange`, and `ObjAccelerate`.
- `ZakoStateWatch` and `Zako_RecoverFromStun` use the Y-range, radius, and cone helpers before choosing movement or state changes.
- `ObjTurnTowardsPlayerXZ`, `ObjWalkOnGround`, `ObjTryStepOnGround`, and `ObjFallToGround` use the existing `ObjGroundHeightCached` port. `ObjFallAndBounce` uses the existing `ObjApplyVelocity` and `ObjBounceOnGround` ports.
- The hit-body branch goes through `ObjPlayerNearPointXZ` / `PlayerWithinRadiusXZ`, then `ObjAddHitBody` and the existing collision-body allocator/registrar.
- `ObjAimAtPlayer` calls the existing SDK `ratan2`, `rsin`, and `rcos` ports directly. `ObjPlayerInCone` uses the existing `VectorNormalSS` port.

## Port and differential status

`tools/mips/check_zakohelpers.py` executes each original entry against the hand-ported C function using area members 01, 02, 03, and 06. The default run has 138 comparisons. All RAM outside the declared stack window, the full scratchpad, return values, and modeled hook calls are compared.

| Function | Status | Cases | Instruction coverage |
|---|---:|---:|---:|
| `ZakoWalkToTarget` | diffed | 3 | 100% |
| `ZakoStateHurt` | diffed | 4 | 100% |
| `ZakoEnterIdleWait` | diffed | 2 | 100% |
| `ObjFootstepFx` | diffed | 4 | 44/48; 4 proven unreachable |
| `ZakoChasePlayer` | diffed | 5 | 100% |
| `ZakoEnterDeath` | diffed | 3 | 100% |
| `ObjTurnTowardsPlayerXZ` | diffed | 5 | 100% |
| `ObjSetSpeed96` | diffed | 1 | 100% |
| `ZakoStateWatch` | diffed | 3 | 100% |
| `Zako_WanderBounce` | diffed | 3 | 100% |
| `Zako_InitDefaultStats` | diffed | 6 | 100% |
| `Zako_RecoverFromStun` | diffed | 3 | 100% |
| `ZakoReturnIfFar` | diffed | 3 | 100% |
| `ObjTargetBeyondRange` | diffed | 3 | 100% |
| `ObjWalkOnGround` | diffed | 3 | 100% |
| `ObjSetVelAlongDir` | diffed | 4 | 100% |
| `ObjSetVelTowardPlayer` | diffed | 3 | 100% |
| `ObjSetVelTowardTarget` | diffed | 3 | 100% |
| `ObjCooldown` | diffed | 4 | 100% |
| `ObjWalkToPoint` | diffed | 3 | 100% |
| `ObjChasePlayer` | diffed | 3 | 100% |
| `ObjWanderStep` | diffed | 3 | 100% |
| `ObjWanderOrReturn` | diffed | 3 | 100% |
| `ObjFleeFromPlayerInArea` | diffed | 3 | 100% |
| `ObjSetVelFromFacing` | diffed | 3 | 100% |
| `ObjTryStepOnGround` | diffed | 3 | 100% |
| `ObjFallToGround` | diffed | 3 | 100% |
| `ObjPlayerOutOfYRange` | diffed | 5 | 100% |
| `ObjPlayerNearPointXZ` | diffed | 4 | 100% |
| `ObjPlayerInCone` | diffed | 3 | 100% |
| `ObjPlayerInConeFromPos2` | diffed | 3 | 100% |
| `ObjTurnToPlayer` | diffed | 3 | 100% |
| `ObjAccelerate` | diffed | 5 | 100% |
| `ObjClampFrame` | diffed | 3 | 100% |
| `PlayerWithinRadiusXZ` | diffed | 3 | 100% |
| `ObjAimAtPlayer` | diffed | 4 | 65/66; 1 proven unreachable |
| `ObjFallAndBounce` | diffed | 4 | 100% |
| `ObjSpawnParticleAt` | diffed | 3 | 100% |
| `Zako_Nop` | diffed | 1 | 100% |
| `ObjAddHitBody` | diffed | 2 | 100% |
| `ObjAddHitBodyIfPlayerNear` | diffed | 3 | 100% |
| `ObjGetLinkedObjState` | diffed | 3 | 100% |

The five instructions not executed are registered in `tools/mips/check_exclusions.csv`:

- `ObjFootstepFx` at `80052F90` and `80052FA0..80052FA8`: cadence is always 4 or 8, and the difference of two unsigned halfwords cannot be `INT32_MIN`; neither divide BREAK guard can fire.
- `ObjAimAtPlayer` at `8005720C`: `Atan2Safe` returns zero for zero Z, otherwise an SDK angle in `[-2048, 2048]`. Adding 4096 cannot produce a negative signed value, so this rounding arm cannot execute.

The checker also records two explicit comparison exclusions. `stack_window` is `801FF000..80200000`; all other RAM and the complete scratchpad are checked. `ObjSpawnParticleAt` copies an uninitialized fourth halfword from its stack vector; the checker supplies the C padding callback with the matching MIPS stack residue (`shared_random`).

## Remaining hooks

The generic helper batch is direct-call wired. The stage-specific dispatcher still calls overlay code by name, as required for addresses at or above `0x80100000`:

- `FUN_8013DF54`: selected for stage 2; receives the enemy object and performs that stage's enemy update.
- `FUN_80140880`: selected for stage 3; receives the enemy object and performs that stage's enemy update.
- `FUN_8013F504`: selected for stage 9; receives the enemy object and performs that stage's enemy update.
- `FUN_8014116C`: selected for stage 10; receives the enemy object and performs that stage's enemy update.

Their overlay bodies remain outside this batch. Existing named boundaries used below the helper layer remain in the shared modules: `Atan2Safe` supplies the turn angle, `GetSolidHeight` supplies area ground height, `FUN_800A3740` is the BIOS clear wrapper, and the sound hardware interface handles audio-device work. SDK math, particle creation, collision-body handling, and the listed helper calls use their existing C ports directly.

## Findings and validation

- `ZakoStateHurt` requests sound effect `0x5F` and stores the returned body pointer at the object's hit-target field.
- The two RNG seeds used by the original code are `0x800AE8F0` and `0x800AE8F2`.
- `ObjSetVelAlongDir` stores velocity before reloading the direction vector for the turn call, preserving the original behavior when those pointers alias.
- `ObjTurnTowardsPlayerXZ` adds 10 to vertical velocity after either ground branch. The airborne branch still receives that increment; a non-grounded case in the helper checker covers it.
- The helper checker passes all 138 default comparisons in about 10 seconds, covers all 1495 instructions except the five registered proofs, and catches both mutations per helper (84/84).
- `tools/mips/check_zakoai.py --coverage` now checks its direct helper wiring with deterministic linked-object and collision-pool fixtures; all 38 caller cases pass, covering all 324 instructions across its three entries.
- `tools/check_verified.py` passes with zero errors after the 42 `diffed` rows are appended. It still reports workspace-wide warnings for other checkers without result JSON and inventory omissions for the three AI caller entries; none of the 42 helper rows has a coverage gap or missing result.
