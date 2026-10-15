# Zako handler-table gaps

This batch ports the three in-scope functions from the enemy type and area-mode
tables. The table entries were confirmed in the loaded EXE image:

| Table entry | Slot | Target |
|---|---:|---:|
| `0x800AF350` enemy type handlers | `0x03`, `0x0E`, `0x49` | `ZakoUpdateGenericEnemy` (`0x80053550`) |
| `0x800AF350` enemy type handlers | `0x28` | `ZakoUpdateStageSpecificEnemy` (`0x800546DC`) |
| `0x800AF574` area-mode callbacks | `0` | `ZakoAreaModeNoop` (`0x80057680`) |

The first function ends at `0x800539B4` (0x464 bytes), before
`Zako_UpdateRunSfx`. The second ends at `0x80054780` (0xA4 bytes), before the
next function. The area-mode callback is the two-instruction return at
`0x80057680..0x80057687`.

## Port and differential results

| Original function | C entry | Status | Default cases | Instruction coverage |
|---|---|---|---:|---:|
| `ZakoUpdateGenericEnemy` | `ae_ZakoUpdateGenericEnemy` | ported + diffed | 27 | 281/281 |
| `ZakoUpdateStageSpecificEnemy` | `ae_ZakoUpdateStageSpecificEnemy` | ported + diffed | 6 | 41/41 |
| `ZakoAreaModeNoop` | `ae_ZakoAreaModeNoop` | ported + diffed | 2 | 2/2 |

The default checker uses EXE images with real area members 01 and 02, directed
state and level cases, and randomized bytes outside the fields needed by each
fixture. It compares return values, all main RAM below the declared stack
window, all scratchpad bytes, and named-hook calls with their consumed
arguments. There are no uncovered instructions. Thirty selector-gated
mutations were caught, ten per function.

Instruction-derived details for `ZakoUpdateGenericEnemy`:

* The state jump table at `0x800A4F80` maps states 8 and 9 to the common return
  tail, state 10 to the fall-to-ground path, state 11 to the hit-body / bounce
  path, and state 12 to its follow-up movement state.
* State 11 transitions to state 12 after a successful bounce, then jumps to the
  function's final type check. It does not execute the state-12 movement block
  in the same update.
* Type `0x49` runs the final height check unless its AI state is `0x0B`; an
  object above home height by more than 614 calls the already ported
  `ZakoDefeat` function.
* `ZakoUpdateStageSpecificEnemy` reads the stage level at
  `0x800F4350 + 0x124`. Levels 2, 3, 9, and 10 call overlay addresses
  `0x8013DF54`, `0x80140880`, `0x8013F504`, and `0x8014116C`, respectively.

Already ported helpers are called directly: `ObjGroundHeightCached`,
`ObjUnregisterBody`, `ObjReenableBody`, `ObjPlaySfx`, and `ZakoDefeat`. Remaining
EXE helper calls use named hooks so this batch does not claim their behavior:

| Hook | Original responsibility |
|---|---|
| `Zako_InitDefaultStats` | Initialize the common enemy radius, speed, and hit-body values, with type adjustments. |
| `Zako_Nop` | The target called at `0x80058134`; its instructions are outside this batch. |
| `ZakoReturnIfFar` | Handle an enemy that is beyond its return distance. |
| `Zako_RecoverFromStun` | Clear the stunned state and return to its idle state. |
| `Zako_WanderBounce` | Wander, then reverse direction when the next ground step fails. |
| `ObjTurnTowardsPlayerXZ`, `ObjTurnToPlayer` | Turn the object toward the player. |
| `ZakoChasePlayer`, `ObjFleeFromPlayerInArea`, `ZakoWalkToTarget` | Chase, flee within the configured area, or walk to the current target. |
| `ZakoStateHurt`, `ZakoStateWatch`, `ZakoEnterIdleWait`, `ZakoEnterDeath` | Handle the named AI state transitions. |
| `ObjCooldown` | Check and update an object cooldown. |
| `ObjTryStepOnGround`, `ObjFallToGround`, `ObjFallAndBounce`, `ObjWalkOnGround` | Check ground ahead, fall to the surface, bounce, or perform ground movement. |
| `ObjSetSpeed96`, `ObjAccelerate`, `ObjSetVelFromFacing` | Set, adjust, or apply movement speed and velocity. |
| `ObjClampFrame` | Clamp an animation frame to its clip limit. |
| `ObjGetLinkedObjState` | Read the state of a linked object. |
| `ObjAddHitBodyIfPlayerNear` | Create the state-specific hit body when the player is close enough. |
| `ObjSpawnParticleAt` | Spawn a particle at the supplied object and parameters. |
| `FUN_8013DF54`, `FUN_80140880`, `FUN_8013F504`, `FUN_8014116C` | Runtime overlay handlers selected by the stage level; overlay code is out of scope. |

`ae_ZakoUpdateLoop` still has its existing `ZakoTypeUpdate` host hook. The
separate host-wiring task can bind the four enemy-type slots and the area-mode
slot above to these new ports. Other entries in both tables remain under the
existing dispatch hook; this batch intentionally ports only the three
functions in its task table.

Validation: `.venv/Scripts/python.exe tools/mips/check_zakoai.py --coverage`
and `.venv/Scripts/python.exe tools/mips/check_zakoai.py --mutate`.
