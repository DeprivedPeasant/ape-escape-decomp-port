# Native indicator runtime composition

`native_indicator_runtime.c/.h` composes the existing `native_indicator_trigger`, `native_indicators` and `native_camera_math` interfaces into the state effects of Gimmick_ProxTrigger_Init (0x80091060) and Gimmick_ProxTrigger_Update (0x80091178). It is source-backed by the instruction listings of those two functions. No viewer, player, UI, progress, CD, shadow or pickup/effect engine code is executed.

## Init: `ae_indicator_runtime_init`

The caller resolves the trigger's collider (24-byte trigger, 56-byte collider). Collider flags (+0x18) first get the low nibble set to 1. Then:

- Time attack: trigger state (+2) becomes 2 and collider flags gain 0x8000. Nothing is registered; only trigger and collider are needed (slots, count, math may be NULL).
- Ordinary: collider +6 and +0xA become 0x50, +8 becomes 0x100. The collider's position halfwords are copied to an 8-byte local and `ae_indicator_register` is called with trigger kind (+0xA) and sign-extended angle (+0xC). The returned slot index is stored at trigger +0x14, trigger +6 is cleared. State is 0 in areas 0x5C..0x62; elsewhere it is 1 if trigger +8 is nonzero, else 0. Trigger +0x10 is cleared.

The original local position buffer has an undefined fourth halfword; the native port zero-fills it by policy, and register overwrites that halfword with the timer (0), so nothing depends on it. Other bytes of trigger and collider are preserved. A full bank (original exit()) is a failure, as are count > 4, missing slots/count or unloaded math.

All work is on local copies; trigger, collider, slots and count are committed only on success. Returns 1/0.

## Update: `ae_indicator_runtime_step`

Fields are little endian. Stage exit codes 0x17, 7 and 0x15 (low 8 bits) succeed with cleared events before any distance is computed; player and slots are not needed. Otherwise the player is required; distance is the native VecLength3 equivalent against collider signed16 position (+0/+2/+4), touchedBy is the signed16 at collider +0x1E, then `ae_indicator_trigger_update` runs. If it requests activation, `ae_indicator_activate` runs on the slot at `event.index` with `event.sector` from the trigger. The index must satisfy 0 <= index < count <= 4 and slots must be non-null; otherwise the call fails. Without an activation request slots/count are not examined.

Events are ordered: `activation` requests (hint or pickups, then sound) precede the trigger's `effect` notification (effect 0x112 at the trigger's effect_position), matching original Activate-before-SpawnEffect order. Trigger, slot and events are committed only on success; failure returns 0 with them untouched.

## Ownership

The caller owns all buffers; the module keeps no state. Executing the returned requests (hint mode, pickups, SFX, effect) is the host's job.

## Codex validation

Strict C11 build and full original instruction comparisons pass:4230initializer cases,
including all115on-disc placements in ordinary/time-attack modes,6000composed update
ticks,307original Indicator_Activate calls,791ordered external engine requests,6dependency
and transaction checks,zero failures. Initializer uses actual original Indicator_Register
and SDK trig; update uses original VecLength3/IntSqrt and Indicator_Activate. Controlled
endpoints cover external hint/CD/stage/player-mode/pickup/sound/effect requests. Complete
trigger/collider/bank bytes match. See validation/indicator_runtime_validation.md and
check_indicator_runtime.py. Signed kind conversion was made explicit during Codex review.

This is pure state/request composition, not viewer integration or a continuous capture.
Runtime collider touch/displacement, save-progress initialization and execution of the
hint/player-mode/pickup/effect protocol remain separate dependencies.
