# Effect scripts, projectiles, and RC car

This batch ports the 12 entries in `extract/codex-task/task.md` as RAM-image C in
`src/effects/ae_fxscript.c` / `.h`. The call relationships below follow the original instruction call sites. Functions
already ported in other modules are direct C calls; functions outside this batch remain named host hooks.

## Call tree

```text
EffectScriptUpdate
├── ParticlesDraw
│   ├── ProjectEffectPoint (renderer projection hook)
│   └── DrawParticleQuad (renderer packet hook)
├── RibbonDraw (script opcode 52)
├── SpawnParticle / EffectSpawnFromDef
└── script command interpreter

ProjectileSystemsUpdate
├── ProjectileRecordsUpdate
│   ├── ProjectileExpire
│   ├── RibbonDraw
│   ├── CollisionBodyAlloc / CollisionBodyRegister
│   ├── GetSurfaceType / GetGroundHeight
│   └── SpawnParticle / sound requests
└── ProjectileGroupsUpdate
    ├── ProjectileGroupSpawnSpread
    │   └── ProjectileGroupSpawn
    │       ├── ProjectileAllocSlot / ProjectileInit / ProjectileSet*
    │       └── SDK matrix and vector maths
    ├── FUN_8014022c / FUN_8014263c (overlay callbacks)
    ├── CollisionBodyAlloc / CollisionBodyRegister
    ├── GetGroundHeight
    └── ModelTransformRigid / ModelDraw

RcCarUpdate
├── ProjectileGroupSpawnSpread
├── CollisionBodyAlloc / CollisionBodyRegister
├── GetGroundHeight / GetSurfaceType
├── SpawnParticle / sound requests
└── PlayerCanActivateGadget / TurnTowardsY / StepToward

RibbonDraw
├── ProjectEffectPoint (renderer projection hook)
└── DrawRibbonStrip (renderer packet hook)
```

The target-to-target leaves were implemented and checked before their callers: the draw and callback boundaries,
`ProjectileExpire`, `ProjectileGroupSpawn`, and then the spread/update dispatchers. Shared dependencies such as body,
world, sound, spawn and pure SDK math routines call their existing ports.

## Port and differential results

The default run uses the EXE plus real area members for areas `01`, `02`, `05`, `07` and `1D`. It completed in 10.6 s,
with 106 function cases and zero differential failures. The checker compares return values, non-stack RAM, scratchpad,
and ordered hook calls. The only RAM comparison exclusion is the original MIPS stack window
`801FF000..80200000`; its reason is registered in `tools/mips/check_exclusions.csv`.

| Address | Original | C function | Status | Default cases | Original instructions covered |
|---|---|---|---|---:|---:|
| `80069920` | ParticlesDraw | `ae_ParticlesDraw` | ported + diffed | 7 | 112/179 |
| `80069BEC` | EffectScriptUpdate | `ae_EffectScriptUpdate` | ported + diffed | 11 | 881/881 |
| `8006F3D0` | RibbonDraw | `ae_RibbonDraw` | ported + diffed | 11 | 299/399 |
| `80084AA0` | ProjectileSystemsUpdate | `ae_ProjectileSystemsUpdate` | ported + diffed | 2 | 10/10 |
| `80084BC0` | ProjectileExpire | `ae_ProjectileExpire` | ported + diffed | 2 | 17/17 |
| `80084C94` | ProjectileRecordsUpdate | `ae_ProjectileRecordsUpdate` | ported + diffed | 17 | 358/358 |
| `80085254` | ProjectileGroupSpawn | `ae_ProjectileGroupSpawn` | ported + diffed | 11 | 247/247 |
| `80085630` | ProjectileGroupSpawnSpread | `ae_ProjectileGroupSpawnSpread` | ported + diffed | 5 | 51/51 |
| `800856FC` | ProjectileGroupsUpdate | `ae_ProjectileGroupsUpdate` | ported + diffed | 18 | 449/451 |
| `800994A8` | RcCarUpdate | `ae_RcCarUpdate` | ported + diffed | 18 | 363/398 |
| `8014022C` | FUN_8014022c | `ae_FUN_8014022c` | ported + diffed | 2 | 0/1 |
| `8014263C` | FUN_8014263c | `ae_FUN_8014263c` | ported + diffed | 2 | 0/1 |

All uncovered instructions are registered with a specific proof or native boundary in `tools/mips/check_exclusions.csv`;
there are no unregistered coverage gaps and no stale exclusions in the default result. The 206 declared-unreachable
instructions are:

* `ParticlesDraw` (67): GTE projection and GPU/ordering-table packet instructions belong to the renderer hooks;
  projection-return copies are supplied by `ProjectEffectPoint`; both division `BREAK` arms are unreachable after the
  nonzero depth gate.
* `RibbonDraw` (100): GTE projection and strip packet construction belong to renderer hooks; the projection hook
  supplies SXY and depth; depth is constrained to the valid positive range before division, making its zero and
  `-1/INT_MIN` trap arms unreachable; the final ordering-table cursor store is performed by `DrawRibbonStrip`.
* `ProjectileGroupsUpdate` (2): the loop skips a group with active state zero, so its default zero-state body is
  unreachable; active states 1 and 2 take explicit branches and states 3 through 7 reach the later dispatch.
* `RcCarUpdate` (35): the impact branch tests a scratch low bit that `GetGroundHeight` clears with `0xFFE0` and
  `VectorNormalSS` does not rewrite; slope divisions are entered only with denominators outside `[-512,512]`, excluding
  zero and the `-1 / INT_MIN` trap; the vertical-state clamp and `+7` update make the later `< -7` branch impossible.
* `FUN_8014022c` and `FUN_8014263c` (one instruction each): these one-byte overlay stubs are absent from the EXE image.
  Their native adapters are instead checked at the callback boundary for argument and return forwarding.

The two overlay adapters are therefore marked `diffed`: their observable contract is the named overlay callback
boundary, while their overlay implementation is outside the EXE being checked.

## Mutation checks

`--mutate` builds three one-change variants per table function and runs each through the same MIPS/C differential
scenarios. All 36 were caught. The mutations cover particle clipping/depth, script opcode and error handling, ribbon
geometry, dispatcher order/failure propagation, expiry state and callback reason, projectile timer/damping/motion,
spawn normalization/modes/aim offsets/spread, group random and bounce thresholds, RC aim/timer/body gates, and both
overlay adapter argument/return/hook forwarding.

## Findings from the instruction behavior

* Water-effect opcode 51 advances the separate seed at `800AE8F2`. A loop-depth error still performs the random step
  before reporting once and continuing through the following script commands. Invalid opcodes print six command words
  and then the final diagnostic.
* Ground collision uses a strict `ground_y < particle_y` comparison. At equality, the water test still runs.
* Body kind 10 with flag `0x20` replaces height with arithmetic-floor `(-7 * height + 7) >> 3`; it does not add a
  damping delta. The rounding boundary at height 25 is included in the differential cases.
* Group-state restart reads alternating X/Z record components, advances the random seed with multiplier 5, and writes
  the state-5 live particle and record-height side effects before rendering.
* RC aim components use signed division by `0x3000`. The test includes a quotient boundary where changing that
  divisor changes the value after the game's step-toward update.
* Signed halfword truncation, arithmetic right shifts, C/MIPS division truncation, RNG update order and MIPS divide
  traps are explicit in the port rather than delegated to host-width assumptions.

## Hooks still supplied by the host

The hook table leaves these functions outside the port. The checker supplies deterministic callbacks with the same
arguments and return values to both sides; production native integrations still need to provide them.

| Hook | Role at this boundary |
|---|---|
| `ProjectEffectPoint`, `DrawParticleQuad`, `DrawRibbonStrip` | Project effect vertices and emit particle/ribbon GPU packets and ordering-table entries. |
| `SetRotMatrix`, `SetTransMatrix` | Set the renderer's current rotation and translation matrices. |
| `ModelTransformRigid`, `ModelDraw` | Transform and submit the projectile-group model. |
| `ClearMemory` | Clear effect-script local scratch/RAM ranges. |
| `Printf` | Emit script diagnostics and body error messages. |
| `SoundLoopStop` | Stop an active game-side sound loop. |
| `SpuWaitIdle`, `SpuWaitLoop` | Synchronize with the sequencer or supply the original CPU delay. |
| sound `set_voice_adsr`, `voice_key_off`, `set_voice_volume`, `printf_error` callbacks | Write SPU voice state and route sound diagnostics to the host. |
| `PadRumble` | Request controller rumble for projectile/group events. |
| `PlayerCanActivateGadget` | Check whether gadget 7 can steer the RC car. |
| `TurnTowardsY`, `StepToward` | Update aim orientation and step steering values toward their targets. |
| `SpawnRandomParticle` | Emit the RC car's random movement effect. |
| `ProjectileExpireCallback` | Invoke the callback address stored in each projectile record, passing the expiry reason. |
| `FUN_8014022c`, `FUN_8014263c` | Area-overlay callbacks used by projectile groups for their two overlay-specific record transitions. |
| `GetSolidHeight` | Query the area's collision grid/boxes for ground height. |
| `GimmickActivate` | Activate the linked gimmick when the world event path requests it. |
| `MeshInitFromAreaTable` | Select or initialize a prop mesh using the current area's mesh table. |
| `FUN_8013d358` | Area `0x29` overlay effect: burst at collider position `(x, y-128, z)` using definition `0x80141B54`, then spawn particle type `0x112`. |
| `CameraShake`, `EventCamStart` | Apply camera shake or start the requested event camera. |

The `FUN_8013d358` dependency has a confirmed area-specific role; the two table adapters are separate overlay
callbacks whose implementations remain external. No function in the 12-entry table remains unported or unvalidated.
