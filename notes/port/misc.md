# Miscellaneous stage and frame ports

This batch covers the 40 entries in `extract/codex-task/task.md`. The C ports and differential checker are in
`src/world/ae_misc.c`, `src/world/ae_misc.h`, and `tools/mips/check_misc.py`. The checker loads the original EXE and real
area members, and for the five overlay areas also loads the matching overlay member at `0x8013D000`.

## Call tree from instruction listings

```text
Mode_Stage
├─ ItemsUpdateDraw
│  ├─ PickupsUpdate
│  │  ├─ PickupSetPosition
│  │  └─ CollisionBodyAlloc / Register / Free / Unregister
│  └─ ItemsDraw
│     └─ ItemDrawModelEnvMap
│        ├─ MatrixRotYMul ──> LIBGTE rsin / rcos; SDK MulMatrix hook
│        ├─ MeshCalcEnvUVs ──> RenderEnvUV hook
│        ├─ LIBGTE MatrixIdentity / CompMatrix
│        └─ model transform / draw hooks
├─ IndicatorsUpdateDraw
│  ├─ IndicatorHintReady ──> MessageBoxBusy
│  ├─ IndicatorHintFree ──> HeapFree
│  ├─ PlayerClearDAT44A2
│  └─ MessageBoxSetText
├─ SoundEmitterUpdate
│  ├─ SoundEmitterAttenuation ──> LIBGTE SquareRoot0
│  ├─ SoundLoopStart (ported function)
│  ├─ SoundLoopStop hook
│  ├─ SfxVoiceSetPanVol ──> SpuWaitIdle hook; SpuVolumeClamp port
│  └─ BgmApplyPlayerEffect
│     ├─ SeqGetChannelProgram
│     └─ SeqFadeTrackProgram ──> SeqFadeInit port
├─ ColliderScriptsUpdate
│  └─ ColliderScriptRun ──> LIBGTE vector length; script action hooks
├─ HudPopupUpdate ──> RenderHudPopup hook
└─ AreaOverlayUpdate
   ├─ BossKindForArea
   └─ one area-code overlay entry (five entries remain unported)

PickupCollect
├─ PlayerHeal ──> HudShowHealth hook
├─ HudPanelB_Start / HudCounterC_Start
├─ StageTimerAdd
└─ CollectedItemsAdd
```

The other table functions are leaves or are called by the trees above: `CameraNudge`, `StageTimerAdd`,
`PlayerEyeBlinkUpdate` (which calls `AreaFeature30GetState`), `PickupCollect`, `CollisionRayTest`,
`SpecialActorPlayerNear`, the message-box and HUD start helpers, the sequencer helpers, and `SfxVoiceSetPanVol`.
`CollisionRayTest` calls the already-ported collider-probe, collider-type, and loop-slot functions directly. The
renderer-facing blocks stay named hooks; they do not replace the surrounding game-state updates.

## Port and validation status

The 35 EXE-resident entries are ported and diffed. The five entries below are reached through the `AreaOverlayUpdate`
dispatch and still use their named callbacks; their one-byte task-table lengths are Ghidra overlay stubs, not body
lengths. Each callback currently stands in for a substantial boss frame controller. The spans below run from the
entry prologue through its `jr $ra` and delay slot in the matching area member:

| Area | Member | Entry and body span | Observed work in the controller |
|---|---:|---|---|
| `0x44` | `0x19E` | `FUN_80140d60`, `0x80140D60..0x8014324F` (0x24F0 bytes) | Drives the boss animation and landing/home movement; updates debris, flashes, sparks, falling rocks, segments and projectiles; triggers ring bursts, foot dust, backdrop, camera shakes, sounds, death effects, player mode changes and collision-body removal. |
| `0x53` | `0x1A5` | `FUN_8013fd74`, `0x8013FD74..0x80140273` (0x500 bytes) | Projects the player position through the GTE and tests screen bounds, checks boss/collision-body state and player distance, advances timers and positional values, then triggers area effects and six repeated effect spawns. |
| `0x56` | `0x1A6` | `FUN_80140a60`, `0x80140A60..0x80141F6F` (0x1510 bytes) | Dispatches a 15-state boss controller; updates camera animation, player slide/knockback, side attacks, shockwaves, rocks, debris, sparks, timed effects and pieces; handles gimmicks, palette changes, pickup drops and intro/death effects. |
| `0x46` | `0x1A0` | `FUN_8013fa2c`, `0x8013FA2C..0x80141367` (0x193C bytes) | Handles boss start/reset and animation/hop states, player knockback, entry and shockwave tables, backdrop, debris, camera shakes, effects, sounds, loop stops and collision-body removal. |
| `0x57` | `0x1A8` | `FUN_8013f464`, `0x8013F464..0x8013FFF7` (0xB98 bytes) | Advances the boss state and animation, starts the boss, updates debris, entries, sparks, waves and ground ripples, and triggers effects, camera shakes, sounds and pickup drops. |

These are not small wrappers: the longest body is 0x24F0 bytes and each controller calls many overlay-local boss helpers.
I have not reconstructed their full state machines or their helper-call semantics, so none of the five callbacks is
counted as a port or covered by the checker. The span and behavior notes above come from disassembling the real overlay
member for each area; they are recorded to make the remaining work concrete without guessing at state transitions.

The default checker run has 211 cases over the 35 configured functions and takes about six seconds. It includes one
seeded randomized round in addition to the hand-picked edge cases. Its cases use real
area members for areas `0x01`, `0x07`, `0x16`, `0x44`, `0x46`, `0x53`, `0x56`, and `0x57`. All configured ports match;
`CollisionRayTest` has full coverage (395/395 instructions). The remaining registered coverage gaps are:

| Function | Coverage | Reason |
|---|---:|---|
| `IndicatorsUpdateDraw` | 143/201 | 58 projection, pose, shadow, and ordering-table instructions are replaced by `RenderIndicator`. |
| `HudPopupUpdate` | 41/188 | 147 primitive and ordering-table instructions are replaced by `RenderHudPopup`. |
| `PickupsUpdate` | 423/425 | The unsigned-byte mode fallback below 2 is unreachable after modes 0 and 1 dispatch. |
| `SoundEmitterAttenuation` | 95/99 | The zero-divisor and signed-overflow guards cannot be reached for the accepted distance/range interval. |

The two renderer ranges and both unreachable proofs are registered in `tools/mips/check_exclusions.csv`. The latest
mutation run tested two RAM-write mutations per configured function: 70/70 were caught. The append-only ledger rows for
the 35 validated functions are in `tools/verified.csv`, with `min_cases` taken from the 211-case default run.

## Findings

Two task-table byte counts are shorter than the instruction spans covered by the checker: `ItemsDraw` is listed as 500
bytes but runs through 572 bytes to its return delay slot, and `ColliderScriptRun` is listed as 244 bytes while its
checker span covers 388 bytes through the `jr $ra` instruction (its delay slot is at `0x8008B97C`, before the next
helper entry at `0x8008B980`). The checker uses the executed code ranges and has full coverage for both functions.

## Hooks still used by the validated ports

The native renderer owns `RenderEnvUV`, `RenderIndicator`, and `RenderHudPopup`, plus the SDK model/GTE projection and
draw calls. Platform audio owns `SpuWaitIdle` and `SoundLoopStop`; `SoundLoopStart` is called directly through its
already-ported function. Other out-of-table game calls remain named hooks, including `GetGroundHeight`, `HudShowHealth`,
`IsTimeAttack`, `ColliderScriptMotion`, and the runtime `ColliderScriptUpdate` action. Collision-probe callbacks retain
their original names and arguments. The five `FUN_8013...` / `FUN_80140...` entries above are still task functions and
are not classified as finished external hooks.
