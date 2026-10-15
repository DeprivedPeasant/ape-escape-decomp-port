# Native indicator proximity world (AEIND2)

Extends `native_indicator_world` with optional runtime records and a pure proximity step
that run on the world's **same** original indicator slots (`world->slots`/`count`); no
second copy of slot state exists. It composes `native_indicator_runtime`
(Gimmick_ProxTrigger_Init 0x80091060 / Update 0x80091178), `native_indicator_trigger` and
`native_indicators`. It is not viewer, player or capture integration.

## Pack versions

| | AEIND1 (default) | AEIND2 (`--runtime`) |
|---|---|---|
| file | `indicators_AA.aei` | `indicators_runtime_AA.aei` |
| magic | `AEIND1\0\0` | `AEIND2\0\0` |
| header | 16 bytes `<8sII>`: magic, area, count (<= 4) | same |
| record | 84 bytes | 164 bytes |
| max size | 352 | 672 |
| size rule | exactly `16 + 84*count` | exactly `16 + 164*count` |

AEIND2 record = the unchanged first 84 bytes (kind, angle, position, unused, source
object, hint sector, actor basename), then the raw 24-byte Gimmick instance (the trigger,
as unpacked by `dump_objects.parse`, losslessly repacked) and the raw 56-byte collider it
references. The collider index inside the trigger (+4) is the real area collider index and is
not zeroed; the native caller has already resolved that pointer by embedding the collider.
`pack(area=1)` output is byte-identical to before; `pack(area, runtime=True)` writes the new
file. The JSON sidecar gains `runtime` (bool); runtime rows additionally carry
`raw_trigger`/`raw_collider` hex for audit. The pack and sidecar are ignored extraction output;
no game data is embedded in code. The packer asserts source row order/count (<= 4), 24/56 byte
sizes, the collider index and that the collider's first 6 bytes equal the prepared position.

AEIND1 stays manual-preview only and has no runtime behaviour: `has_runtime` returns 0,
`runtime_record` and `proximity_step` fail.

## Loader

Both versions share the loader (magic selects stride, exact length). For AEIND2 each entry stores
trigger24/collider56 and, before initialising, the loader checks the raw source against the
prepared record: trigger kind (+0x0A) == kind, angle (+0x0C) == angle, hint sector (+0x12,
sign-extended) == sector, collider halfwords 0..2 (6 bytes) == position. A mismatch rejects the
pack with a specific message. It then calls `ae_indicator_runtime_init` (ordinary mode, area =
scene area) on the world's own slot bank, so the registered slot, trigger index (+0x14) and
order must equal the record index; otherwise the load fails. Counts and order are preserved.

## API

- `ae_indicator_world_has_runtime(world)`: 1 only for an AEIND2 world; 0 for NULL.
- `ae_indicator_world_runtime_record(world, index, trigger, collider[56], slot)`: copies the
  stored trigger, collider and the shared slot. Needs a runtime world, index < count and all three
  outputs; no writes on failure.
- `ae_indicator_world_proximity_step(world, player, exit_code, message_busy, touched_by[4],
  touch_known_mask, events[4], missing_touch_mask*)`.

## Step semantics

Requires a runtime world, `events` and `missing_touch_mask`. Stage exit codes (low 8 bits)
0x17/7/0x15 succeed without player or touch inputs, zero all four events and the mask and leave
the world untouched. Otherwise `player` is required; `touch_known_mask` may only contain bits below
count, and `touched_by` is required only when the mask is nonzero.

Work is transactional on copies of triggers, colliders and slots. Entries run
`ae_indicator_runtime_step` in registration order (so an earlier entry's activation is visible
to later ones through the shared slots). `message_busy` is a single host input fixed over the batch;
the original message-box busy side effects are not modelled. On success the updated triggers,
colliders and slots are copied back, events for unused entries are zero. On failure the world,
`events` and the mask are unchanged. The step does no animation tick; the caller ticks
`ae_indicator_world_tick` independently.

## Missing-touch protocol

The collider touched-by halfword (+0x1E) is a live game contact value; the stored disk value is never
used. For known entries the caller's value is written exactly. For unknown entries -1 is written
into the working collider. An unknown entry sets its bit in `missing_touch_mask` only when the
touch-reading branch is reachable: state 1 and distance < radius, or state 3 and distance <= radius
(distance from the player to the static collider centre). State 0 first activation does not read touch,
so it is never missing, and state 3 beyond the radius still resets to 0 without touch. A set bit means
the result assumed no contact and may be wrong; the caller decides whether to accept it.

## Limits

Static collider centre only: no live collider displacement or collision service. Events are returned,
not executed: hint/CD, stage, player mode, progress, pickups, sound and effect 0x112 services are
unimplemented, as are save-progress initialisation and original message-box side effects.

## Ownership

The world owns all trigger/collider/slot state. `ae_indicator_world_runtime_record` returns copies;
callers own their buffers.

## Codex validation

Reviewed all delivered diffs/hash/baseline before integration. Strict C11 build and actual
runtime packs cover all101areas/115trigger records. Original initializer and1840shared-
bank update steps agree on complete trigger/collider/slot bytes and338ordered engine
requests.187batches exercise explicit missing-touch status; four source/prepared metadata
mismatches and five dependency/transaction/legacy cases fail or short-circuit correctly.
See check_indicator_proximity_world.py and validation/indicator_proximity_world_validation.md.

Existing AEIND1 regression also passes:101areas/115indicators,20700idle and13800activation
frame checks,15failure checks,180repeat walking plus90manualhint SDL frames,strict default
and player builds. Viewer does not yet invoke proximity_step; manual preview is unchanged.
These are controlled original-code and native integration checks, not original gameplay
capture or complete hint/player-mode protocol validation.
