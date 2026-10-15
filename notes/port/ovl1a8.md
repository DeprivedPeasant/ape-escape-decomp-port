# Area 57 overlay member 1A8

Area `0x57` (Peak Point Matrix, area image `126.bin`) loads overlay member `1A8.bin` at `0x8013D000`. The area table identifies its entry points as `Area57_BossInit` (`8013DA10`) and `Area57_BossUpdate` (`8013E4B4`). The named RAM globals and record sizes are in [ae_ovl1a8.h](../../src/world/ae_ovl1a8.h); readable ports and the RAM-image oracle are [ae_ovl1a8.c](../../src/world/ae_ovl1a8.c) and [check_ovl1a8.py](../../tools/mips/check_ovl1a8.py).

## Call tree and boundaries

```text
Area57_BossInit
  -> BossModelInit / BossSetAnim / BossClearFlag
  -> clear debris, entry, ripple and spark records; reset loop handle

Area57_BossStart
  -> BossModelInit / BossClearTable8
  -> initialize actor sequence and collision fields

Area57_BossUpdate
  -> BossUpdateCameraSequence
       -> BossUpdateWakeSequence when the wake timer is active
       -> camera-slot smoothing and gadget-follow helpers
  -> phase-specific camera movement, actor state, collision and effect updates
  -> BossUpdateSequence for phases below 20
       -> animation, entry, spark, wave, ripple, debris and hit-response helpers
       -> BossUpdateLateSequence for phases 20 and above
  -> BossUpdateEntries / BossUpdateDebris / BossUpdateRipples /
     BossUpdateSparks / BossUpdateWaves as selected by the original phase path

BossUpdateSequence / BossUpdateLateSequence
  -> BossAddWaveEntry / BossSpawnGroundRipples / BossSlamImpact / BossDeathBurst
  -> BossSpawnDebris / BossSpawnEffectGroupsN / BossRumble* / BossSetAnim

BossUpdateEntries / BossUpdateWaves / BossUpdateRipples / BossUpdateSparks
  -> native effect, particle, collision, sound, camera, and renderer APIs
```

The stateful boss code, record lifetimes, animation values, movement, collision responses, random-seed steps, effect timers, and actor transitions are ported in C. Existing project ports implement the typed body, spawn, sound, camera, world, stage, and player services. Pure LIBGTE arithmetic uses the existing exact SDK C ports.

## Renderer and API hooks

The renderer callback carries the computed game values to native GTE/GPU code:

| Hook | Original work represented |
|---|---|
| `Area57_Boss_ProjectAndEmit` (`8013DB94`) | Boss model vertex transforms, lighting, primitive emission, and ordering-table links. It receives the actor, three scale values, and color flags. |
| `Area57_Boss_DebrisProjectAndEmit` (`8013D6AC`) | Projects one already-updated debris record and emits its primitive. Arguments include position, velocity, life, record index, and ordering-table cursor. |
| `Area57_Boss_DebrisBatchHeader` (`8013D818`) | Emits the debris batch terminator/header and final ordering-table link. |
| `Area57_Boss_SparkProject` (`8013F228`) | Projects and emits one spark; C owns its life, delay, color, and position updates. The hook returns the next cursor. |
| `Area57_Boss_RippleMeshSetup` (`80140A7C`) | Sets up the native ripple mesh transform and colors. C performs the single original random-seed advance and passes both seed values. |
| `Area57_Boss_RippleProjectAndEmit` (`80140C60`) | Projects and emits one already-updated ripple record. |
| `EffectRingDraw` (`8006E5B4`) | Emits a colored ring using its position, radius, layer, and color arguments. |
| `DrawShadow` (`8001F604`) | Emits the boss shadow from its computed position and size. |

`ModelApplyLighting` (`8013D0A0`) is a pure GTE/GPU lighting routine; it has no game-state stores and no ledger row. `ProjectAndEmit` is the full native model-renderer boundary and likewise has no C-function ledger row. The generic `original_call` callback is used for `SetRotMatrix` and `SetTransMatrix` while drawing sparks; these install the current GTE matrices. Other boundaries use the existing typed APIs: collision-body register/unregister, ground height, pickup/effect/particle/trail spawning, sound and loop handles, rumble, camera-slot operations, event-camera/gadget flash, and stage request.

## Ported functions

All 42 rows below are `diffed`; `min_cases` is the default per-function count. The two native-only renderer functions above are intentionally absent from the ledger.

| Address | Function | Status / cases |
|---|---|---|
| `8013D000` | `Area57_Boss_ModelInit` | ported + diffed / 6 |
| `8013D1FC` | `Area57_Boss_SpawnEffectNear` | ported + diffed / 6 |
| `8013D310` | `Area57_Boss_SpawnEffectNearWrap` | ported + diffed / 6 |
| `8013D364` | `Area57_Boss_SpawnDebris` | ported + diffed / 6 |
| `8013D4CC` | `Area57_Boss_UpdateDebris` | ported + diffed / 6; rendering blocks hooked |
| `8013D33C` | `Area57_Boss_ClearDebris` | ported + diffed / 6 |
| `8013DA10` | `Area57_Boss_Init` | ported + diffed / 6 |
| `80140E70` | `Area57_Boss_Start` | ported + diffed / 6 |
| `8013E8A8` | `Area57_Boss_UpdateCameraSequence` | ported + diffed / 6 |
| `8013E4B4` | `Area57_Boss_Update` | ported + diffed / 11 |
| `8013EA9C` | `Area57_Boss_UpdateWakeSequence` | ported + diffed / 11 |
| `8013EA24` | `Area57_Boss_Wake` | ported + diffed / 6 |
| `8013D89C` | `Area57_Boss_RumbleLong` | ported + diffed / 6 |
| `8013D8C4` | `Area57_Boss_RumbleShort` | ported + diffed / 6 |
| `8013D8EC` | `Area57_Boss_RumbleMedium` | ported + diffed / 6 |
| `8013D914` | `Area57_Boss_SpawnDropPickups` | ported + diffed / 6 |
| `8013D9A8` | `Area57_Boss_ResetLoopHandle` | ported + diffed / 6 |
| `8013D9B8` | `Area57_Boss_LoopStart` | ported + diffed / 6 |
| `8013D9E0` | `Area57_Boss_LoopStop` | ported + diffed / 6 |
| `8013E020` | `Area57_Boss_ClearEntries` | ported + diffed / 6 |
| `8013E044` | `Area57_Boss_AddEntry` | ported + diffed / 6 |
| `8013E120` | `Area57_Boss_UpdateEntries` | ported + diffed / 7 |
| `8013DFB0` | `Area57_Boss_SpawnEffectGroupsN` | ported + diffed / 6 |
| `8013EEC0` | `Area57_Boss_AddSparks` | ported + diffed / 6 |
| `8013F06C` | `Area57_Boss_UpdateSparks` | ported + diffed / 6; projection/emission hooked |
| `8013EE9C` | `Area57_Boss_ClearSparks` | ported + diffed / 6 |
| `8013F370` | `Area57_Boss_ResetFlag` | ported + diffed / 6 |
| `8013F398` | `Area57_Boss_ClearFlag` | ported + diffed / 6 |
| `8013F3A4` | `Area57_Boss_SetAnim` | ported + diffed / 6 |
| `8013F3DC` | `Area57_Boss_StepAnim` | ported + diffed / 6 |
| `8013FFFC` | `Area57_Boss_ClearTable8` | ported + diffed / 6 |
| `80140020` | `Area57_Boss_AddWaveEntry` | ported + diffed / 6 |
| `801400F4` | `Area57_Boss_UpdateWaves` | ported + diffed / 12 |
| `801407BC` | `Area57_Boss_ClearRipples` | ported + diffed / 6 |
| `801409D4` | `Area57_Boss_UpdateRipples` | ported + diffed / 6; mesh/projection blocks hooked |
| `80140794` | `Area57_Boss_RumbleBig` | ported + diffed / 6 |
| `801407E4` | `Area57_Boss_SpawnGroundRipples` | ported + diffed / 6 |
| `80140F08` | `Area57_Boss_SlamImpact` | ported + diffed / 6 |
| `80140F68` | `Area57_Boss_DeathBurst` | ported + diffed / 6 |
| `8013DDC4` | `Area57_Boss_DrawScaledModel` | ported + diffed / 21 |
| `8013F464` | `Area57_Boss_UpdateSequence` | ported + diffed / 45 |
| `80140FFC` | `Area57_Boss_UpdateLateSequence` | ported + diffed / 32 |

## Validation and instruction coverage

The default run passed 349 differential cases against member `1A8.bin` loaded over the real area `126.bin` image. Coverage collected 4,207 of 4,884 original instructions; every uncovered instruction belongs to a registered renderer span or a proof-backed unreachable branch. The two whole native renderer functions contribute 0/87 and 0/140 game-state instructions, respectively, and have no ledger rows.

| Function | Covered original instructions | Registered reason for the remainder |
|---|---:|---|
| `Area57_Boss_UpdateDebris` | 65/244 | GTE setup, per-record projection/packet emission, and OT terminator |
| `Area57_Boss_Update` | 252/253 | Unsigned 16-bit camera count bounds `48*count+1024` to 1024..3,146,704, so the signed-negative rounding arm is unreachable. |
| `Area57_Boss_UpdateSparks` | 120/193 | Inline GTE projection and GPU packet/OT work; the spark hook receives updated record fields. |
| `Area57_Boss_UpdateWaves` | 422/424 | Invalid-state jump: states are initialized to 0, allocated as 1, transitioned to 2, and cleared to 0. |
| `Area57_Boss_UpdateRipples` | 108/295 | Mesh GTE setup and per-record projection/packet emission; state, bounce, and lifetime logic remain covered. |
| `Area57_Boss_UpdateLateSequence` | 781/789 | Eight divide guard instructions: divisor `hit_count*20 + signed_counter*8 + 550` is clamped to at least 400, so divide-by-zero and `INT32_MIN/-1` are impossible. |

The checker also compares the full scratchpad and all main RAM outside `801FF000..801FFFFF`. The stack window is excluded because the original MIPS frame and native C frame differ. Three narrow record padding masks cover only the unused fourth halfword copied from caller position vectors: particle records (`100 x 0x34`, byte `0x1B`), effect records (`16 x 0x54`, bytes `0x06..0x07`), and trail records (`8 x 0x10`, bytes `0x06..0x07`). Their coordinates, timers, definitions, and other record data remain compared. These exclusions are registered in `tools/mips/check_exclusions.csv`.

## Instruction findings

- In phase 19, the impact burst uses a stack-local vector built from the actor's updated X/Z and Y `-2624`; it does not reuse the earlier sequence-position vector.
- The phase-1 effect delays are `6`, `4`, and `2` frames (`delay * 2`). On a non-final tagged contact, the sequence-substate write is in the sound-call delay slot and occurs before the following sound.
- Falling waves retain their stored ring color and radius; only the expanding-ring state recalculates those fields. The landing and impact branches then clear or dispatch the record before the shared renderer path.
- In the late zero-counter contact, the original clears actor sequence mode in a collision-call delay slot and computes turn values as normalized components divided by 48. The nonzero tagged-contact path rounds negative `/32` values toward zero by adding 31 before the arithmetic shift.

Mutations at both entry guards of every C function were caught: 84/84. The final ledger rows are in [verified.csv](../../tools/verified.csv), all with `status=diffed`, the default case count, and notes `overlay member 1A8.bin`.
