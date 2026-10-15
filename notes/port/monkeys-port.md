# Monkey code: 1:1 port on a PSX RAM image (`src/monkeys/`, `src/core/ae_sdk.*`)

The monkey code (`0x80032E30..0x80043300`) is ported function by function from the original instructions, on the same
byte-exact RAM image (`AeRam`) as the collision bodies (`notes/port/bodies.md`). The port is done in batches by
call-graph level (level 0 = functions that call no other monkey-range function), so every batch can be diffed against
the original code before the next one builds on it.

## Conventions (all batches)

* Header `src/monkeys/ae_monkey.h`: addresses of the globals (`AE_G_*`), `Monkey` field offsets (`AE_MK_*`), the hook
  table `AeMonkeyHooks`, the prototypes. One `.c` per batch / area (`ae_monkey_base.c` = batch 1).
* `ae_<OriginalName>(AeRam *r, [const AeMonkeyHooks *h,] original arguments...)`. The implicit `this` is the global
  `g_CurMonkey` (`0x800B4D08`), read from RAM exactly where the original reads it. Argument types follow the original
  register use (`int32_t` where it sign-extends an `s16`, `uint32_t` for addresses). Void functions (the original leaves an
  unrelated `$v0`) return `void`; the checker does not compare `$v0` for them.
* **Hooks** (`AeMonkeyHooks`): every call outside the port is a hook named after the original function, one uniform
  signature `(user, ram, a0, a1, a2, a3) -> $v0` that mirrors the MIPS calling convention, so a hook is exactly "the
  original function". Arguments the original passes by stack pointer are RAM addresses inside the per-function scratch
  block `AE_STK(n)` (`0x801FF000 + 0x40 * n`, at most 64 blocks, inside the stack window that the checks do not compare; the enum in the
  header hands out `n`). A NULL hook sets the `AeRam` error flag and returns 0 (fail closed). Calls into `src/bodies`
  use the ports directly (`AeMonkeyHooks.body` carries the `AeBodyHooks` for their error paths).
  A call to a monkey function of a **later batch** is a hook named after that function; the later batch replaces it by the
  direct call and deletes the field (the check script derives its hook table from `ae_monkey.h`, see below).
* SDK maths is `src/core/ae_sdk.{c,h}` (new in batch 1, also diffed on its own): `ae_sdk_VectorNormal` (800301B0),
  `ae_sdk_VectorNormalSS` (800301E0), their shared core (80030210), `ae_sdk_square0_lane` (Square0 80014498),
  `ae_sdk_SquareRoot0` (800303C0), `ae_sdk_ratan2` (800145D0), `ae_sdk_ApplyMatrixSV` (800300B0). Tables are read from the
  RAM image; the `add` / `div` traps of the original set the error flag. (`ae_VectorNormalS` of `src/bodies` is unchanged.)
  The older `ae_player_normalize` approximation is not used.
* Failure model as in `bodies.md`: unmapped / unaligned access, signed-add overflow in the normalise core, a divide trap
  (`break`) -> sticky `AeRam` error, the function returns early.

## Validation: `tools/mips/check_monkey_<batch>.py` + `tools/mips/monkey_harness.py`

`monkey_harness.py` (shared, not a check itself) holds the machinery: the hook table parsed from `ae_monkey.h` (one
`AeMonkeyHookFn Name; /* 8 digit address ... */` line per hook, so removing / adding a hook line is all a batch has to do),
`HOOK_SPECS` (which arguments of each hook are logged, stack buffers dumped, how the deterministic result is produced;
a hook that gets ported is removed there too), `MonkeyHarness` (MIPS side = `tools/mips/mips_ram.py` with the pinned
PCSX-ReARMed GTE oracle, C side = DLL over `AeRam`; both sides run on identical RAM, identical hook model; compared are the
return value (not for void functions), main RAM outside the stack window `0x801FF000+`, the scratchpad and the sequence
of hook calls with arguments), and the RAM scenario builders (`build_monkey_world`: 8 monkeys with random fields, the
`MonkeyInfo` table, a random nav graph with sorted link lists, the globals). The collision-body ports reached through the
monkey functions run against the original `CollisionBodyRegister` / `Unregister` / `GetObjectByIndex` with the body
hook model of `check_bodies.py`. Extra checks: NULL-hook runs (the port must flag "not provided" exactly when the original
called a hook), original faults / traps (the port must flag its error flag), `--mutate` (deliberately wrong copies of the
port; the checker must fail on each), `--coverage`, `--trapv`.

## Batch 1: functions

`uv run python tools/mips/check_monkey_base.py` (default `--cases 40`, about 55 s; `--cases 100` about 2.5 min;
`--coverage` prints the executed-instruction coverage; `--trapv` builds the port with `-ftrapv`; `--mutate` runs the
mutation test).

**Results (last run):** default scale: 9209 cases, 3142 hook calls compared, 14.4 million original instructions executed,
0 mismatches (336 cases in which the original faults / traps, e.g. wild node indices or a zero divisor, were matched by
the port's error flag; 156 NULL-hook cases checked for fail-closed behaviour); scale 50: 11359 cases; also clean with
`--trapv`. Per function (scale 100): 1 for the constants, 20..90 for the small state / area tests (the state byte is
always swept 0..0x47 plus random bytes), 150..300 for the table / struct readers, 350..700 for the vector maths and
node functions, 600 for NavNodeGetLink / NavNodeLinkFlag (plus 600 hand-built link lists), 900 MonkeyReachedNode; the
SDK maths directly: 800 SquareRoot0, 800 ratan2, 500 each VectorNormal / VectorNormalSS, 600 ApplyMatrixSV (extreme
operands included); AngleBetweenXZ additionally gets 20..60 vector pairs found with a replica of the normalise core whose
cross product is 1 mod 4096 (the only place the `+4095` rounding is observable).

**Coverage** of the original instructions (every ported function is 100 % except the dead guards below): 2330 of 2389
instructions over the batch, the SDK routines and the body code it reaches. Not executed: the zero-divisor `break` of
`NavGroundHeightBetween` (80040C84, 80040CD4: both divisors are non-zero by the earlier branches), the second / third
zero-divisor `break` of `MonkeyInterpolateLinkVector` (8003C604, 8003C658: the same field was already divided by),
`MonkeyApproachAngleFar` 80037E3C..54 (INT_MIN / constant divisor guard), `ratan2` 80014628, 80014638..40, 80014670..78
(guards that cannot be reached with valid operands; the reachable ones are exercised by the direct SDK check), and the
parts of `CollisionBodyRegister` / `Unregister` that belong to body types other than 3 and to list-middle unlinks (the
body module is diffed by `check_bodies.py`).

**Mutation test** (`--mutate`, 37 deliberately wrong copies of `ae_monkey_base.c` / `ae_sdk.c`: changed constants, state
sets, offsets, rounding, loop bounds, table indices, wrong body type ...): all 37 are caught. The first run let 5 survive
(a state sweep that shrank with the scale, a rare rounding case, a flag mask no scenario set, a queue with only the last
slot free, an index range of the body table); the checker was strengthened for each (and one mutation turned out to be
equivalent code and was replaced).

| Original (address) | C | Status |
|---|---|---|
| MonkeyRand 80032e30 | `ae_MonkeyRand` | ported + diffed |
| MonkeyCaughtGroupValue 80032e58 | `ae_MonkeyCaughtGroupValue` | ported + diffed |
| MonkeyGetAnimIndex 80032e84 | `ae_MonkeyGetAnimIndex` | ported + diffed |
| MonkeyNop 80033000 | `ae_MonkeyNop` | ported + diffed |
| MonkeyBodyRegister 80033008 | `ae_MonkeyBodyRegister` | ported + diffed (direct `ae_CollisionBodyRegister`; hook `TargetListAdd`) |
| MonkeyBodyUnregister 80033068 | `ae_MonkeyBodyUnregister` | ported + diffed (direct `ae_CollisionBodyUnregister`; hook `TargetListRemove`) |
| MonkeyActivityReset 8003317c | `ae_MonkeyActivityReset` | ported + diffed |
| MonkeyStoreActivities 800334c8 | `ae_MonkeyStoreActivities` | ported + diffed |
| GimmickAllowsMonkeyPassage 800335d0 | `ae_GimmickAllowsMonkeyPassage` | ported + diffed (hook `GimmickGetInfo`) |
| MonkeyVoiceQueueReset 80033708 | `ae_MonkeyVoiceQueueReset` | ported + diffed |
| MonkeyQueueVoice 80033744 | `ae_MonkeyQueueVoice` | ported + diffed (hook `Printf`) |
| DelayedSfxUpdate 800337c4 | `ae_DelayedSfxUpdate` | ported + diffed (hook `PlaySfxAt`) |
| NavNodeGetLink 8003386c | `ae_NavNodeGetLink` | ported + diffed |
| AreaIsSpecialMonkeyArea 8003398c, AreaIsBossArena 800339ec | `ae_Area*` | ported + diffed |
| MonkeyIsState1E_to_20, IsIdleActivityState, IsIdleEndState, IsAltAnimState, IsState0C_or_24, IsJumpArcState, IsAttackState | `ae_MonkeyIs*` | ported + diffed (state byte swept 0..0x47 + random) |
| MonkeySpawnProjectile 80033ccc | `ae_MonkeySpawnProjectile` | ported + diffed (hook `ProjectileSpawn`) |
| AngleBetweenXZ 800344c8, DotXZ_SV 800348cc, DotXZ_V 80034940 | `ae_AngleBetweenXZ`, `ae_DotXZ_SV`, `ae_DotXZ_V` | ported + diffed (SDK normalise, ratan2) |
| DistanceIntToShort3D 800345d4, Distance3D 80034684, DistanceXZ 8003473c | `ae_Distance*` | ported + diffed (SDK Square0 / SquareRoot0) |
| MonkeyCalcMoveDir 800347d8, MonkeyInterpolateLinkVector 8003c568 | `ae_Monkey*` | ported + diffed |
| Atan2Angle4096 800349b4 | `ae_Atan2Angle4096` | ported + diffed |
| NavNodeIsSpecial 800349f4, NavNodeGetLinkIds 80034b74, NavNodeGetLinkIdsEx 80034c08, NavNodeLinkFlag 80038608 | `ae_NavNode*` | ported + diffed |
| ObjGetBonePos 80034d30 | `ae_ObjGetBonePos` | ported + diffed (hooks `MatrixFromDirXZ`, `ModelPoseBones`; SDK ApplyMatrixSV) |
| MonkeyReachedNode 8003516c, MonkeyNavBacktracked 80035604 | `ae_Monkey*` | ported + diffed |
| MonkeyGetCallVoice 800356c0, MonkeyGetCallVoice2 80035750 | `ae_MonkeyGetCallVoice*` | ported + diffed |
| MonkeyFacingFromLinkCode 80036bd0 | `ae_MonkeyFacingFromLinkCode` | ported + diffed (jump table read from RAM) |
| MonkeyAttackChance 80037068, MonkeyApproachAngleNear 80037d6c, MonkeyApproachAngleFar 80037dec | `ae_Monkey*` | ported + diffed |
| MonkeyPuff14 80039200, MonkeyPuff5 8003929c | `ae_MonkeyPuff*` | ported + diffed (hook `SpawnParticle`) |
| StaticModelDraw 80039e50 | `ae_StaticModelDraw` | ported + diffed (hooks `ModelTransformRigid`, `ModelDraw`) |
| MonkeyStub_ReturnZero 8003a334 | `ae_MonkeyStub_ReturnZero` | ported + diffed |
| MonkeyResetNavVisits 800407f0, NavGroundHeightBetween 80040bdc | `ae_*` | ported + diffed |
| ActorContactOwnerType 80041280 | `ae_ActorContactOwnerType` | ported + diffed (direct `ae_GetObjectByIndex`) |
| MonkeyDebugMenuInit 80043060, MonkeyDebugMenuInit2 800430f0 | `ae_MonkeyDebugMenuInit*` | ported + diffed (hooks `MenuItemInitBound`, `MenuItemSetRange`, `MenuItemSetCallback`, `MenuItemSetStep`) |
| BodyContactType 800392e4 | `ae_BodyContactType` (`src/bodies`) | already ported and diffed; reuse it |

### Hooks of batch 1 (all non-monkey code)

`Printf` 800A3720, `GimmickGetInfo` 8005B7BC, `PlaySfxAt` 80087A20, `ProjectileSpawn` 80059304, `SpawnParticle` 800698B8,
`TargetListAdd` 8007CD88, `TargetListRemove` 8007CDC4, `MatrixFromDirXZ` 80014804, `ModelPoseBones` 80015780,
`ModelTransformRigid` 800161C0, `ModelDraw` 80017CD4, `MenuItemInitBound` 8007D1E0, `MenuItemSetRange` 8007D290,
`MenuItemSetCallback` 8007D29C, `MenuItemSetStep` 8007D2A4. No call to a monkey function of a later batch exists at level 0.
`memset` (800A3740, a libc routine used for the stack temporaries of the distance functions) is not a hook: the temporaries
live only in the stack window, so the port does not need them; the original's own `memset` runs in the MIPS reference.

## Findings: pseudocode / names vs. instructions

* **`MonkeyBodyRegister` / `Unregister` use `flags3` (+0x24C), not `flags` (+0x248)**; the registered bit is bit 1 of `flags3`.
* **`DistanceIntToShort3D` is a distance in X and Z only** (it subtracts all three components but only squares x and z), in
  spite of the name `...3D`; `DistanceXZ` skips the y subtraction. `Distance3D` squares all three. The inputs go through the
  GTE `SQR`, so only the **low 16 bits** of each 32-bit difference are squared (a difference >= 32768 wraps); the sum is added
  with `addu`.
* **`StaticModelDraw` reads `0x800EC350 + 0x0C`** (not the `0x800F4350` game-state block); the draw context is
  `0x800EC3D0 + 16504 * index`. The `ModelTransformRigid` bone pointer is `0x800E9C60 + (model->u16 +4) * 32`.
* `GimmickAllowsMonkeyPassage` passes its own `a0` on to `GimmickGetInfo` (the pseudocode shows it too); the result tests are
  per type (1: value is 0 or 5, 6: 3, 9 / 17 / 24: positive, 49: 0, 101: u16 < 2 or 5, others: 2).
* `MonkeyQueueVoice` calls `printf` (string at `0x800A4900`, "voice full") only when all 16 queue slots are taken.
* `NavNodeGetLinkIds*` split the `s16` at node +0x10 (and +0x12) by 100 with the multiply-by-reciprocal idiom
  (`0x51EB851F`, hi >> 5, + sign) i.e. truncating division; the remainder is the unsigned 16 bit value minus 100 * quotient,
  stored as 16 bit; `Ex` additionally maps the quotient through the u16 table at `0x800AEB6C`. `NavNodeGetLinkIdsEx` takes
  its fifth argument on the stack.
* `AngleBetweenXZ`: `ratan2(cross >> 12, dot >> 12)` with the cross product rounded toward zero (`+4095` when negative) and
  the dot product shifted arithmetically; both through `VectorNormal` (32-bit lanes).
* `DotXZ_SV` / `DotXZ_V` round with `(v + (v < 0 ? 4095 : 0)) << 4 >> 16` (a `sll 4` first, not a plain `>> 12`).
* `MonkeyReachedNode`: radius = `reachRadius * 2` (states 0x0C / 0x24), `* 15 / 10` (state 9; reciprocal-multiply division),
  or halved by an **unsigned** `srl` for the node lists of area 0x4D / 0x1A (a negative radius becomes huge);
  the comparison value is the `s16` truncation of that.
* `MonkeyFacingFromLinkCode` dispatches through the code table at `0x800A4910` in the EXE; the port reads the table from
  RAM and maps each target to its block (flags an error for a modified table).
* `NavNodeGetLink` counts only links with bit 14 set; the returned id is the **position** of the matching link in the list,
  masked with 0x3FFF.
* `MonkeyStoreActivities` always walks all 8 monkeys and leaves `g_CurMonkey` pointing at the last one.
* Dead code in the original (reported, not coverable): the zero-divisor `break` guards of `NavGroundHeightBetween`
  (both divisors are non-zero by the preceding branches), the 2nd / 3rd zero-divisor guards of
  `MonkeyInterpolateLinkVector` (the same field was already divided by) and `MonkeyApproachAngleFar` (constant divisor),
  and four `ratan2` guards that cannot be reached from valid operands.

## Batch 2 (call-graph level 1): `src/monkeys/ae_monkey_level1.c`, `tools/mips/check_monkey_level1.py`

Status: all 31 functions ported and diffed against the original instructions (0 mismatches). Run with
`uv run python tools/mips/check_monkey_level1.py` (`--cases N` scale, default 40, about 55 s; `--coverage`, `--trapv`,
`--mutate [--mutate-only WORD..]`). The batch 1 functions it calls are used directly (no hooks); on the MIPS side they run as the
original code, so batch 1 is also exercised in context.

| Original (address) | C | Status |
|---|---|---|
| MonkeyRandomHesitate 80032f24, MonkeyAdvanceAnimFrame 800330c8, MonkeyPickAnim 800331b0, MonkeyAnimFinished 80033a3c, MonkeyAnimAtStart 80033acc | `ae_Monkey*` | ported + diffed (state byte swept 0..0x42, random animation tables) |
| MonkeyFireProjectile6/7/8 80033df0 / 800343b0 / 8003440c, MonkeyFireProjectileAimed 8003429c, MonkeyAimAtTarget 80033e4c | `ae_Monkey*` | ported + diffed (hooks `ProjectileSpawn`; batch 1 `QueueVoice`, `SpawnProjectile` direct) |
| MonkeyAngleToTarget 8003455c, MonkeyApplyMoveDir 80034834, NavNodeGetPos 80034ef8, MonkeyTargetInSightRange 800361b4 | `ae_*` | ported + diffed |
| MonkeyBodyOff 80036b24, MonkeyBodyOn 80036b78, MonkeyDefeatBurst 80036c84 | `ae_Monkey*` | ported + diffed (hooks `EffectBigBurst`, `SpawnParticle`, `TargetListAdd/Remove`) |
| MonkeyFootDust 80038f00, MonkeyDebrisBurst6 80038fd8, MonkeyDebrisBurst1 800390f8, MonkeyAlertPuff 80039248 | `ae_Monkey*` | ported + diffed (hook `SpawnParticle`) |
| MonkeyUpdateContact 800392f0 | `ae_MonkeyUpdateContact` | ported + diffed: every state 0..0x42 x every contact type 0..15, plus a state x contact x flags2 bit 15 x sign(+0x124) grid and random cases; 100 % instruction coverage |
| MonkeyProjectToScreen 800393e4, MonkeyAlertIconKind 80039b90, MonkeySelectLod 8003a33c | `ae_Monkey*` | ported + diffed (hooks `SetRotMatrix`, `SetTransMatrix`, `RotTransPers` = GTE, `ModelInit`) |
| MonkeyPickGoalNode 8003ad9c, MonkeyLoadLinkExtra 8003b674, NavLinkLookupExtra 8003b880 | `ae_*` | ported + diffed (hook `Printf` with its arguments) |
| MonkeyMoveStep 8003b9c0, MonkeyTurnTowardTarget 8003cc3c, MonkeyLineOfSight 800401f4 | `ae_Monkey*` | ported + diffed (hooks `SparkAddRandom`, `GetGroundHeight`) |
| rsin 8002fb50, rcos 8002fc20, RotMatrixYXZ 80030fa0 | `ae_sdk_rsin/rcos/RotMatrixYXZ` (`src/core/ae_sdk.*`) | ported + diffed directly (RotMatrixYXZ is pure integer maths, no GTE) |
| CopySvecAt1C 800154dc | static helper of level1.c | ported inline, diffed through MonkeyLineOfSight |

New hooks (non-monkey code, all in `AeMonkeyHooks`): `EffectBigBurst` 8006B3F0, `SparkAddRandom` 8006E0DC, `ModelInit` 80015AC0,
`SetRotMatrix` 8002FB00, `SetTransMatrix` 8002FB30, `RotTransPers` 80031330 (the three GTE ones stay hooks until there is a GTE
port), `GetGroundHeight` 80043400. No hook for a monkey function of a later batch is needed at this level.

Findings (listing vs pseudocode / names):
* `MonkeyProjectToScreen` returns 1 when the monkey is OFF screen (too far, behind, outside x -50..434 / y -25..265), 0 when visible.
* `MonkeyUpdateContact`: type = `+0x50 >> 28`. States 30 / 32 with type 8 (the Monkey Net): `+0x126 = 0`, returns 0. Otherwise
  `+0x126 = type`; types 8, 1, 10, 11 return 1; type 3 returns `s16(+0x124) <= 0` only if state == 2, an idle activity state or
  flags2 bit 15 is set, else 0; all other types 0.
* `MonkeyFireProjectileAimed` / `Fire8` store all three components of the offset to the target into the SAME halfword
  (`sp+24`), so only z survives and the other two halfwords the hook receives are uninitialised stack in the original.
* `MonkeyTurnTowardTarget`: the `subu s0,zero,v0` in the delay slot of the second `jal` negates the FIRST Atan2Angle4096 result.
* `MonkeyAimAtTarget` skips the animation / direction part in state 30; fires only when `s16(arg) == 0` and `frame & 3 == 0` or
  `s16(arg) == 5` and `frame % 5 == 0`.
* `MonkeyBodyOff/On` write `flags (+0x248)`: Off = `|0x40000000, &~0x30000, |0x20000`; On = `&~0x40000000, &~0x30000, |0x10000`.
* Dead code in the original (not coverable): the second `AreaIsBossArena` test at 8003BE1C of MoveStep (the first one already
  excluded it), the divide-trap guards of PickGoalNode / MoveStep (hop, state 62) / LineOfSight (non-zero divisors by construction),
  the `a1 == 0` loop guard of PickGoalNode; the INT_MIN / -1 guard at 80040550 of LineOfSight is reachable only with contrived
  operands and is not covered.

**Results (batch 2, last run):** default scale (`--cases 40`, about 56 s): 11843 cases, 4449 hook calls compared, 16.2 million
original instructions, 0 mismatches (143 cases where the original faults / traps matched by the port's error flag, 525 NULL-hook
fail-closed cases); also clean with `--trapv`; batch 1 check unchanged and clean. Coverage of the original instructions
(`--coverage`): 3587 of 3649 over the batch, the SDK routines and the batch 1 code it reaches; every ported function is 100 %
except the dead / contrived guards listed above (PickGoalNode 8003AE44.., 8003AECC.., 8003AFD4/AFEC; MoveStep 8003BAD8, 8003BC94,
8003BE2C..BE58; LineOfSight 80040364.., 80040540..590), ratan2 guards (known), and `MonkeySpawnProjectile` types 3 / 4 (batch 1 code,
not used by this batch).
**Mutation test** (`--mutate`, 82 deliberately wrong copies of `ae_monkey_level1.c` / `ae_sdk.c`, each run only against the
families it can influence): all caught. The first run let 15 survive; for each the scenarios were strengthened (non-boss areas,
exact thresholds computed in the setup, targeted MoveStep families, dx near 6000, ...) or the mutation was equivalent code (a clamp
whose both branches store the same value, a `< 1061` on a value that only has multiples of 64, an `a <= last` whose else branch
stores the same frame, a table quadrant edge where sin = -1 on both sides, a distance clamp above the point where cos < 0) and was replaced.

## Batch 3 (call-graph levels 2 and 3): `src/monkeys/ae_monkey_level2.c` / `ae_monkey_level3.c`, `tools/mips/check_monkey_level2.py` / `level3.py`

Shared scenario builders for the checkers of batch 2 and later: `tools/mips/monkey_scen.py`.

### Level 2 (status: all 19 functions ported and diffed, 0 mismatches)

`uv run python tools/mips/check_monkey_level2.py` (`--cases N` scale, default 40, about 70 s; `--coverage`, `--trapv`, `--mutate`).

| Original (address) | C | Status |
|---|---|---|
| MonkeySetState 80033220, MonkeyEnterState_32 80035918 / 3D 800360D8 / 3E 8003612C / 3F 80036174 / 17 8003641C / 2A 80036460 | `ae_Monkey*` | ported + diffed |
| MonkeyUpdateLinkSlope 80035370, MonkeyUpdateGoalPos 80035474, MonkeyPrepareJumpArc 800364BC | `ae_Monkey*` | ported + diffed |
| MonkeyTryAttackKind 8003710C, MonkeyNavLinkAllowed 80037E8C, MonkeyDrawAlertIcon 80039C38 | `ae_Monkey*` | ported + diffed (new hooks `ObjSlotGetAnimIndex` 80052C4C, `AlertIconDraw` 8006ABF0) |
| MonkeyScoreLinksByTarget 8003AC54, MonkeyNodeTargetCosines 8003B534, MonkeyUpdateFacing 8003C204, MonkeyFireProjectileByKind 8003CD24, MonkeyUpdateSight 800405C4, MonkeyGetWaypointPos 80040CFC | `ae_Monkey*` | ported + diffed |

New hooks (non-monkey code): `ObjSlotGetAnimIndex` 80052C4C, `AlertIconDraw` 8006ABF0. No hook for a monkey function of a later batch.

**Results (level 2, last run):** default scale (`--cases 40`, about 75 s): 10692 cases, 2880 hook calls compared, 48 million original
instructions, 0 mismatches (852 cases where the original faults / traps, e.g. wild node indices or a zero divisor, matched by the
port's error flag; 1276 NULL-hook cases checked for fail-closed behaviour); also clean with `--trapv`. Coverage (`--coverage`) of
the original instructions of the 19 functions: 100 % except dead / contrived guards: UpdateLinkSlope 800353EC, 800353FC..14
(divide guards and the negation of a quotient that cannot be negative), PrepareJumpArc 800366F0..08 (`s6 == 0`: the cross product
cos(s0) * s1 - sin(s0) * s5 cannot vanish for the angles the function derives), 8003674C, 800367F8, 80036808..10, 80036848,
80036858..60, 800368CC, 800368DC..E4 (divide guards: non-zero by construction), DrawAlertIcon 80039C9C / A0 (an icon kind other than
0 / 448 / 896) and 80039DDC (`lbu` can never be negative: the `lod < 0` branch is dead), UpdateFacing 8003C4C4.. (divisor is
22 / 34 / 170). **Mutation test** (`--mutate`, 97 deliberately wrong copies of `ae_monkey_level2.c`): all caught. The first run let
11 survive; the scenarios were strengthened with exact-threshold families aimed through the validated batch 1 ports
(slope quotient 10 / 11 / 12, the flat / steep switch of the jump arc at angle 979..982, move-direction angle differences at
+-2048 / 2049, a DotXZ_V result of -3686 +- 2, the distance limit of NavLinkAllowed, the sight windows), and the mutations that were
equivalent code were replaced (the `s1 = 22` entry of the facing table is never used because states 12 / 36 take the other
branch; `dxz & 0xFFFF` is a no-op because DistanceXZ is at most 65535; `s0 = 1024` for `s5 == 0` leads to the steep branch either way).

Findings (listing vs pseudocode / names):
* `MonkeyNavLinkAllowed` is called as `(from, to)` (the pseudocode shows one argument for the callers in `MonkeyNavVisitUpdate`); the
  record value (`NavLinkLookupExtra`'s out word) means gimmick id (0x40 / 0x80), distance limit (0x400, tested with Distance3D to the
  node), monkey id (0x1000), object slot (0x40000, hook `ObjSlotGetAnimIndex`); 0x200000 / 0x400000 test the alerted byte (+0x79 == 1),
  node flag 0x100 only allows the goal node (+0xE6).
* `MonkeyUpdateFacing`: the pseudocode is scrambled; the listing copies the 8 bytes at +0x60 to the stack, calls `MonkeyApplyMoveDir`,
  and at the end copies them back (so the move direction is restored; only +0x94 / +0x9C.. / +0x100.. / +0x120 / +0x122 change). The
  turn step comes from a jump table (0x800A49B8: state 2 -> 34, states 12 / 36 -> 22 (unused), everything else 170).
* `MonkeyUpdateGoalPos` / `MonkeyEnterState_1E` / `_09`: where the original reads a stack slot that was never written (the out value
  of `NavLinkLookupExtra` or the position buffer when the position copy is skipped), the port reads the per-function scratch block
  as it is in the RAM image, which is what the checks reproduce (zero); in the running game the original reads leftover stack contents.
* `MonkeyUpdateSight`: pattern argument 0..4 selects a window of the link position counter (+0x21C) through the jump table at 0x800A4A98
  (0: always, 1: < 569 or 3528..4096, 2: 456..1592, 3: 1480..2616, 4: 2504..3640, others: always); it passes mode 0 (arguments 2..4) or 1
  (arguments 0, 1, others) to `MonkeyLineOfSight`; the flags bits 28 / 29 receive the result.
* `MonkeySetState` writes the animation set pointer (`0x800F4504` / `0x800F4508`) and table (`area data 0x800F4590` + header offset
  +0x18 / +0x1C of `0x800B4D14`) for states 0x32..0x34 (set 2) and the others (set 1, then `MonkeyPickAnim` twice when it changed).
* `MonkeyPrepareJumpArc`: s1 = pos.y(32 bit) - node.y, s5 = DistanceXZ & 0xFFFF; angle by three paths (drop >= 100: 70 % of the
  atan2 angle, at least 341; flat: (angle + 1024) / 2; s5 == 0: 1024 = straight up); angles >= 1002 take the "steep" branch with
  a fixed duration 30; otherwise the arc maths (sin / cos, two square roots, one VectorNormal) fills the velocity (+0x13C..+0x144) and duration (+0x128).


### Level 3 (status: all 26 functions ported and diffed, 0 mismatches)

`uv run python tools/mips/check_monkey_level3.py` (`--cases N`, default 40, about 42 s; `--coverage`, `--trapv`, `--mutate`).

| Original (address) | C | Status |
|---|---|---|
| MonkeyRestoreActivities 800332F8, MonkeyUpdateGoal 800355DC, MonkeyStartNotice 800357E0 | `ae_Monkey*` | ported + diffed (hooks `ObjSlotSetField_CF`, `ObjSlotGetTarget`) |
| MonkeyEnterState_1E 8003581C / _34 / _20 / _24 / _0C / _35 / _1A / _09 / _30 / _36, MonkeyEnterState2F, MonkeyEnterKnockdown 80035BD0, MonkeyEnterIdleWait 80036300, MonkeyResumeIdle 800363A4, MonkeyEnterRandomLook 800363E4 | `ae_Monkey*` | ported + diffed (hooks `SoundLoopStartEx`, `SpawnPuff`; existing `PlaySfxAt`, `SpawnParticle`) |
| MonkeyPickAttackA 8003738C / B 80037420 / C 800374B8 | `ae_MonkeyPickAttack*` | ported + diffed |
| MonkeyMarkBlockedLinks 80038054, MonkeyUpdateTargetDistance 80039B30, MonkeyAi_State3E 8003FC08, MonkeyNavVisitUpdate 8004086C | `ae_Monkey*` | ported + diffed |
| MonkeyDrawModel 80039ED4 | `ae_MonkeyDrawModel` | ported + diffed (hooks `MatrixLookDir`, `ScaleMatrix`, `ModelBuildBoneMatrices`, `ModelTransformSkinned`, `ModelDrawWithOt`, `SoundLoopStopAndFree`; existing `MatrixFromDirXZ`, `ModelDraw`) |

New hooks (non-monkey code): `ObjSlotSetField_CF` 80052CA0, `ObjSlotGetTarget` 80052D04, `SoundLoopStartEx` 800882C4, `SoundLoopStopAndFree`
800880D0, `SpawnPuff` 8006B3CC, `MatrixLookDir` 8001484C, `ScaleMatrix` 80030E60, `ModelBuildBoneMatrices` 80015504,
`ModelTransformSkinned` 80016A60, `ModelDrawWithOt` 80017D94. No hook for a monkey function of a later batch.

**Results (level 3, last run):** default scale: 6270 cases, 5822 hook calls compared, 20.8 million original instructions, 0 mismatches
(235 cases where the original faults / traps matched by the port's error flag, 704 NULL-hook fail-closed cases); also clean with
`--trapv`. Coverage: 100 % of all 26 functions except 8003636C (`MonkeyEnterIdleWait`: INT_MIN / -1 guard, the dividend is at most
131070) and 800409DC (`MonkeyNavVisitUpdate`: a `blez` on a value that is always >= 1). **Mutation test** (`--mutate`, 138 deliberately
wrong copies of `ae_monkey_level3.c`): all caught on the first run.

Findings:
* `AE_STK` blocks now start at 0x801FF000 (64 blocks of 0x40; the batch 2/3 enum needed more than 32). `MonkeyUpdateGoalPos` uses the
  block of `MonkeyUpdateLinkSlope`: both are callees of `MonkeyUpdateGoal` at the same depth, and the original's never-written position
  at `sp + 32` (state 30 without flags bit 24, or a node with bit 6) holds what `UpdateLinkSlope` left there (the second node position).
* `MonkeyNavVisitUpdate`: `MonkeyNavLinkAllowed` is called with (node index, neighbour id), not one argument as in the pseudocode.
  The visit counters are bytes at `node + slot + 0x14`; for slots >= 4 they overlap the first bytes of the next node record. The
  trailing maximum loop computes a value that is never used (its reads are kept because they can fault). Odd `goalAge` calls without
  the "wait" bit only age the counters (+1, max 255); otherwise unvisited nodes next to a node with counter >= 2 are marked 1 if the
  link may be used, the count goes to +0xEE and flags bit 27 is set when nothing was marked.
* `MonkeyEnterKnockdown(state)` copies the target position into the goal unless in a jump arc state, enters `state`, +0xE2 = 90 for
  state 0x30 else 30, +0x124 = 120, clears +0x50, voice 71, the once-per-knockdown sound 78 (flags bit 21) and a puff 128 above.
* `MonkeyDrawModel` draws only for activity 4 (or 3 with +0x90 % 3 == 0); states 0x0C / 0x24 use `MatrixLookDir`, others
  `MatrixFromDirXZ`; `+0xF0 <= 0` with flags bits 16/17 = 1 / 2 scales the matrix by sin / cos(+0xF0 << 10); the draw context is
  `0x800EC3D0 + 16504 * index` (`index` = word at 0x800EC35C), the non-boss path passes `+0x14C + MonkeyInfo.u16` as the last argument.
  Area 0x46 draws the static model 0x800F46B0, area 0x24 the looping-sound models 0x800EC180 / 0x800EC0C0 (sound 0x602D, handle at +0x160).
* `MonkeyPickAttackA/B/C` differ only in the bit (0 / 1 / 2) of the flag word at `0x800AEBB8 + 8 * kind`; all return 100 when
  nothing started or when `g_Player` +0xEA is non-zero.


## Batch 4 (call-graph levels 4-7): `ae_monkey_level4.c` .. `ae_monkey_level7.c`, `check_monkey_level4.py` .. `level7.py`

Status: all 21 functions ported and diffed against the original instructions, 0 mismatches. Run
`uv run python tools/mips/check_monkey_levelN.py` (`--cases N` percent, `--seed`, `--coverage`, `--trapv`, `--mutate`). Default scales
(25 / 30 / 15 / 10 for levels 4 / 5 / 6 / 7) give 39..57 s per level; `--mutate` automatically uses at least scale 100 (at the lower
scales rare-equality mutations survive). Scenario builders shared by the levels are in `tools/mips/monkey_scen4.py`;
`tools/mips/monkey_stack_probe.py` is a diagnostic (not a check) that finds which results depend on uninitialised stack words.

| Level | Original (address) | C | Status |
|---|---|---|---|
| 4 | MonkeySetNavNode 80035080, MonkeyEnterState_1D 800359F0, _04 80035A7C, _2C 80036964, MonkeyPickBestLink 80038120, MonkeySwapNavNodes 80038694, MonkeyAi_Fall 8003FCD8, MonkeyArriveAtNode 80040D90, MonkeyUpdateVisuals 80042B68 | `ae_Monkey*` | ported + diffed |
| 5 | MonkeyNodeActionDispatch 80036CD0, MonkeyConsiderTurnBack 800386C8, MonkeyConsiderTurnBackRandom 80038914 | `ae_Monkey*` | ported + diffed |
| 6 | MonkeyAlertedDecide 80037550, MonkeyChooseNextNodeA 80038A1C, MonkeyChooseNextNodeB 80038A9C | `ae_Monkey*` | ported + diffed |
| 7 | MonkeyPatrolDecide 80037908, MonkeySelectNextNode 8003B074, MonkeyAi_IdleActivityStart 8003F18C, MonkeyAi_IdleActivityLoop 8003F308, MonkeyAi_GoToFlaggedNode 8004002C | `ae_Monkey*` | ported + diffed |

New non-monkey hooks (batch 4a): `GimmickEvent` 8005B880, `ActorAnimDecode` 80014E34, `DrawShadow` 8001F604, `ColorFadeSet` 8008C3C0,
`DebugTextPrintfWhite` 8008CBFC (plus the existing `Printf`, `GetGroundHeight`, `ObjSlotSetField_CF`, `SoundLoopStopAndFree`,
`ObjSlotGetAnimIndex`). **No hook for a monkey function of a later batch is left open**: levels 4-7 call only batch 1-7 functions directly.

**Results (last run, default scales, 0 mismatches everywhere, also clean with `--trapv` at the time of the first runs):**

| Level | cases | hook calls compared | original instructions | faults matched / NULL-hook cases | per function (cases) |
|---|---|---|---|---|---|
| 4 | 2574 | 2563 | 78.5 M | 109 / 243 | ArriveAtNode 488, PickBestLink 600, SetNavNode 611, UpdateVisuals 200, EnterState_1D / _04 / Ai_Fall 150 each, _2C 125, SwapNavNodes 100 |
| 5 | 2346 | 485 | 46.8 M | 20 / 266 | NodeActionDispatch 966, ConsiderTurnBack 690, ConsiderTurnBackRandom 690 |
| 6 | 2308 | 859 | 82.4 M | 307 / 252 | AlertedDecide 733, ChooseNextNodeA 375, ChooseNextNodeB 1200 |
| 7 | 2616 | 1368 | 72.9 M | about 190 / 240 | GoToFlaggedNode 410, IdleActivityLoop 434, IdleActivityStart 434, PatrolDecide 638, SelectNextNode about 700 |

At 100 % scale the totals are about 3.8 k / 3.1 k / 5.7 k / 8.3 k cases (levels 4 / 5 / 6 / 7), also 0 mismatches.

**Instruction coverage** (`--coverage`, instructions of the original executed at least once): 100 % of MonkeySetNavNode, EnterState_1D /
_04 / _2C, SwapNavNodes, Ai_Fall, UpdateVisuals, all of level 5, AlertedDecide, ChooseNextNodeA, PatrolDecide, IdleActivityStart / Loop and
GoToFlaggedNode. The remaining instructions are provably unreachable (checked on the listings):
* `MonkeyPickBestLink` 800381AC..B4: the INT_MIN / -1 guard of the boss-arena `rand % count` (the dividend is `rand & 0xFFFF`);
  80038560 / 64: second pass, the `vis == 0` test with both counters 0, which the early exit "best counter 0 and own counter 0 -> -1" already took.
* `MonkeyArriveAtNode` 80041130: the target of a `beq` on node flag bit 3 that is known to be set on every path that reaches it.
* `MonkeyChooseNextNodeB` 80038EB4..C0: the fallback pass finding an admissible link after the scored pass found none; the scored pass accepts
  every admissible link (the initial limit 0x2000 is above any cosine score, links ids are masked non-negative).
* `MonkeySelectNextNode` 8003B3B0..FC: picking from the candidate list (`s2 > 0`) needs a score `>= s4` with `s4 = 0x40000000`, impossible for an s16.

**Mutation test** (`--mutate`, deliberately wrong copies of the C): level 4: 125, level 5: 76, level 6: 74, level 7: 114, all caught on the final run
(389 in total). The first runs left survivors that the scenarios now cover: `PickBestLink` cosine comparisons (needed the larger scale),
`ChooseNextNodeB` `sel >= 0` (a link to node 0 that is not first in the list), `SelectNextNode` state 61 after a boss arena exit, `PatrolDecide`
random thresholds. Three mutations were removed as equivalent / dead: `SelectNextNode` list cap 10 (dead path above),
`GoToFlaggedNode` initial limit 60000 -> 60001 and the 16-bit mask of the distance (`Distance3D` loads each axis difference into a 16-bit GTE
register and squares it, so the distance is at most 56755: neither the 60000 limit nor the mask can matter).

Findings:
* **Uninitialised stack / register reads in the original** (the checks put the same random value on both sides, see `set_stale*` in
  `monkey_scen4.py`; the real game reads whatever an earlier call left there, so these results depend on the call depth):
  `MonkeyEnterState_1D` reads `sp+16`, the out word of `NavLinkLookupExtra`, which is only written when the link has an extra record (the same stack
  word is `MonkeyEnterState_1E`'s `sp+32`); `MonkeyEnterState_09` reads its `sp+16` the same way; `MonkeyUpdateLinkSlope` / `MonkeyUpdateGoalPos` share
  the frame `sp+16..47` (UpdateGoalPos reads the second node position when it skips the copy); `MonkeyAi_GoToFlaggedNode` uses the caller's `s2`
  as the node index when no flagged node is within range (the C port takes it as the argument `s2_in`; the checks pass 0);
  `MonkeyPickBestLink` keeps `s7` (the chosen link id) uninitialised until a link is chosen, but only reads it after one was (no effect).
  Levels 6 and 7 switch these paths off in most scenarios (`no_stale_goal`, the dispatch bits cleared) because the leftover depends on a
  call chain that is not modelled; level 5 and 4 inject the values for the exact call depth.
* `MonkeySwapNavNodes` returns the `$v0` left by `Distance3D` inside `MonkeyUpdateGoalPos`; `MonkeyConsiderTurnBack` returns it on one path.
* The word at monkey `+0xC8` is excluded from the RAM comparison (`h.mask`, see `check_monkey_level4.py`): it is a dead pad word that the
  two sides do not write identically.
* `MonkeySelectNextNode(p1, p2)`: p2 is forced to 1 in boss arenas; the history of six node ids is saved at entry and restored when no
  candidate exists after a backtrack (then `MonkeyEnterRandomLook`); `ChooseNextNodeA` / `B` / `NodeActionDispatch` / `ArriveAtNode` details are in
  the comments of the C functions.

## Batch 5 (call-graph levels 8-12): `ae_monkey_level8.c`, `ae_monkey_level9.c`, `ae_monkey_level9_patrol.c`, `ae_monkey_level10.c`; `check_monkey_level8.py` .. `level10.py`

Status: all 21 functions (+ SDK `RotMatrix`) ported and diffed against the original instructions, 0 mismatches; all ledger rows in `tools/verified.csv`
are `diffed`. Default scale runs take 40-50 s (level 8), 30-45 s (level 9), 25-40 s (level 10). Scenario helpers: `monkey_scen5.py`
(`has_flag40` / `narrow_init_mask`, `select_world`, `move_fields` ...).

| Level | Original (address) | C | cases | coverage |
|---|---|---|---|---|
| 8 | MonkeyDecideAction 800378C0 | `ae_MonkeyDecideAction` | 48 | 100 % |
| 8 | MonkeyInit 8003A414 | `ae_MonkeyInit` | 504 | 98 % (10 instructions declared unreachable: divide guards) |
| 8 | MonkeyAi_MoveAlerted 8003C6D4 | `ae_MonkeyAi_MoveAlerted` | 680 | 100 % |
| 8 | MonkeyAi_State1E_20 8003CD84 | `ae_MonkeyAi_State1E_20` | 816 | 99 % (8 declared) |
| 8 | MonkeyAi_MoveUnalerted 8003D9D4 | `ae_MonkeyAi_MoveUnalerted` | 480 | 100 % |
| 8 | MonkeyAi_IdleActivityEnd 8003F4B4, MonkeyAi_State07 80040170 | `ae_MonkeyAi_*` | 56, 80 | 100 % |
| 9 | MonkeyAi_Patrol 8003DC7C | `ae_MonkeyAi_Patrol` | 1360 | 99 % (14 declared) |
| 9 | MonkeyAi_JumpArc 8003F530, JumpArcPhased 8003F86C, JumpArcB 8003FD7C | `ae_MonkeyAi_JumpArc*` | 320, 320, 280 | 100 %, 95 % (9 declared), 100 % |
| 9 | MonkeyAi_State3D 8003FB44, MonkeySelectActive 800426C4 | `ae_*` | 112, 200 | 100 % |
| 10 | MonkeyAiStateUpdate 800412B0 | `ae_MonkeyAiStateUpdate` | 1009 | 100 % |
| 10 | MonkeySpawnAll 8003958C, MonkeyThinkActive 800424D4 (level 11), MonkeyUpdateAll 80042E18 (level 12) | `ae_*` | 32, 120, 64 | 99 % (1 declared), 98 % (2 declared), 100 % |
| 10 | RotMatrix (SDK) 80030BD0 | `ae_sdk_RotMatrix` | 48 | 100 % |

The recorded case counts are in `extract/checks/results/check_monkey_level{8,9,10}.json` and the `min_cases` column of `tools/verified.csv`
(the table repeats them).

**Coverage.** Every instruction that is not covered is an `unreachable` row of `tools/mips/check_exclusions.csv` with its proof: INT_MIN / -1 and
zero-divisor guards (MonkeyRand returns at most 255, dividends are `rand & 0xFFFF`, divisors were just tested), the negative-product fix-up of two
MonkeyRand values, the default targets of the `rand & 3` switch in Patrol, Patrol state 0x37 in the not-finished tail (the state-55 branch leaves earlier),
the node flag 0x10000 test that follows a test of the same word, JumpArcPhased state 0x1D (the state was just set to 0x2E), and loop clamps of
SpawnAll plus the second divide guard of ThinkActive.

**Masks.** `monkey+0xC8:4` is unchanged (batch 4). The MonkeyInit distances `monkey+0x214:8` are now masked per case only when a node with flag 0x40 can
be read (`monkey_scen5.has_flag40`; about 15 % of the MonkeyInit cases and 28 % of the AiStateUpdate cases, none of the others in levels 9 / 10 beyond
that rule): the flag makes `MonkeyUpdateGoalPos` measure to the never-written `sp+32` position (see findings). The `ProjectileSpawn` peek mask (two
uninitialised halfwords) is unchanged.

**Mutation test** (`--mutate`, at least scale 100): level 8: 72, level 9: 119, level 10: 132 deliberate wrong copies, all caught on the final run. The first run left 14
survivors: 6 got scenario fixes (Patrol link-angle boundaries 1366 / 3357, JumpArc vertical rounding edge `vy*t = 1 mod 4096`, equal-distance monkeys in SelectActive,
attack states in UpdateAll, the SpawnAll printf argument is now compared by the hook spec), 8 were removed and recorded as `dropped_mutation` rows
(equivalent or dead code; one is not proven: MoveUnalerted's goal x, which is always rewritten before it is read in 889 reaching cases).

**Open (non-monkey) hooks the integration must provide** (the `AeMonkeyHooks` entries these batches use):
* `GetGroundHeight` 80043400 (x, y, z): floor height below a point. Used by State1E_20, MoveAlerted / Unalerted, JumpArcB, Patrol, AiStateUpdate.
* `SoundLoopStopAndFree` 800880D0 (sound): stops and frees a looping sound (State1E_20 when the flight ends).
* `ObjSlotGetAnimIndex` 80052C4C (object slot): animation index of a world object (Patrol waits for a node object, result 2).
* `ObjSlotSetField_CF` 80052CA0 (slot, 4): sets an object field (Patrol).
* `SpawnParticle` 800698B8 (type, position), `GimmickEvent` 8005B880 (event, id, 0), `Printf` 800A3720 (format 0x800A4930: too many active monkeys).
* New in batch 5: `ClearWords` 80032708 (address, word count): zero-fills (SpawnAll clears the voice queue and the eight 0x94-word records; the original
  jumps into the BIOS A0 memset, absent from the image, so the harness runs a real memset on both sides); `VSync` 8002E520 (mode, -1 = the vertical blank
  counter, SpawnAll seeds the random number from it); `StageExitIsActive` 8005DCE4 (no arguments: AiStateUpdate's net-catch test).
No hook for a monkey function of a later batch is open (everything the batch calls is ported).

**Findings.**
* Uninitialised stack words read by the original (both sides get the same value in the checks, `shared_random` rows): MonkeyInit's `UpdateGoalPos` reads `sp+32`, the
  position it never wrote, when the node has flag 0x40 (and `+0xC8` receives the unwritten `sp+28`); the `EnterState_1D / _1E / _09` words as in batch 4;
  MonkeyAiStateUpdate passes its caller's `$s2` to `GoToFlaggedNode` as a node index when no flagged node is in range (ThinkActive's `$s2` is the player address
  0x800EC1E0, which the port passes as the original does; the direct checks of AiStateUpdate pass the constant 0 on both sides).
* The pseudocode missed a third argument of two `GetGroundHeight` calls in Patrol (the node z), and `ProjectileSpawn` types 5 / 8 store all three offset components into one halfword.
* MonkeyRand returns `seed >> 8`, at most 255: all `rand * rand` products are below 65536, `% 300` / `% 600` tests need factor pairs, divide guards are unreachable.
* **MonkeyAiStateUpdate net-catch transition (0x80041F9C..0x80041FD0):** for contact type `+0x126 == 8` and `StageExitIsActive() != 0` the original stores activity 3 in the
  *delay slot* of the `jal MonkeyBodyUnregister` (`sb 3,122(v1)` at 0x80041FB8), i.e. before the callee runs, so the body is unregistered with activity 3; then the
  monkey id (`+0x7C`) goes to `g_CaughtMonkey` (0x800AEB5A) and execution continues at the common tail 0x80042140. The port writes the activity first, then calls
  `ae_MonkeyBodyUnregister`, then stores the id (same order); without the exit flag the code falls through to the knockdown tests at 0x80041FD4.
* Checker robustness: with another seed (0x944281 on level 10) ThinkActive can pick a node index near 8000 from random fields; the node read lands in the stack window
  (garbage on both sides) and gives a false mismatch. The default seeds do not hit it; cap node indices of random fields if the seeds are changed.
