# Monkey base and levels 1–7 readability pass

Hand-written readability refactor of the existing port; no new original functions, integration hooks, ledger rows or exclusions. Levels 8–10 and C files outside `src/monkeys/` were not edited. Changes are left uncommitted for owner review.

## Source changes

Register aliases were replaced with function-local names, and values serving different roles were split into separate declarations. Actor, MonkeyInfo, nav-link, model, menu, animation and global addresses now use shared names in `ae_monkey.h`. Every function, including static helpers, has a one-line purpose comment. Call-order comments explain which data the next operation needs.

The artificial execution routers in `MonkeyAlertedDecide`, `MonkeyPatrolDecide` and `MonkeySelectNextNode` were replaced with nested branches and meaningful selection flags. The current-monkey reloads, short truncations, unsigned wraparound, division fault checks and scratch-buffer writes retain their original order. Rigid-model draw-context arithmetic uses its equivalent unsigned record stride. Shift-and-add multiplication ladders in aiming, debris and jump setup use equivalent wrapped products.

## Before / after audit

Register locals count declarations named `v0..v9`, `a0..a9`, `s0..s9`, `t0..t9` or `fp`, including arrays and repeated declarations in separate scopes. Raw offsets count executable `+ 0x...` displacements; comments are excluded. Decimal vector-component displacements, scratch-buffer positions and record strides remain explicit. Decimal scalar field offsets and debug-menu field descriptors were also named.

| File | Register locals before → after | Raw hex offsets before → after | Mutation pairs rewritten / retained |
|---|---:|---:|---:|
| `ae_monkey_base.c` | 56 → 0 | 71 → 0 | 19 / 37 |
| `ae_monkey_level1.c` | 119 → 0 | 136 → 0 | 46 / 82 |
| `ae_monkey_level2.c` | 58 → 0 | 154 → 0 | 62 / 96 |
| `ae_monkey_level3.c` | 61 → 0 | 114 → 0 | 86 / 138 |
| `ae_monkey_level4.c` | 50 → 0 | 82 → 0 | 82 / 124 |
| `ae_monkey_level5.c` | 14 → 0 | 13 → 0 | 43 / 75 |
| `ae_monkey_level6.c` | 19 → 0 | 17 → 0 | 52 / 73 |
| `ae_monkey_level7.c` | 30 → 0 | 20 → 0 | 78 / 113 |

All 738 existing mutations are retained; 921 individual replacement strings in 468 mutation pairs were rewritten. Replacement pairs continue to introduce the same semantic faults, including mutations of the former execution-router branches. Base mutation runs now select the affected scenario family, following the other monkey checkers; SDK mutations select their directly affected SDK entry, and all default runs still exercise every family. No cases, masks or exclusions were removed.

## Hop-entry coverage

`check_monkey_level2.py` retains its original 96 default `MonkeyEnterState_2A` cases and adds six complete-body scenarios (102 total). The old body setup intentionally left the animation pointer random, so most entry paths faulted in `MonkeyPickAnim` before reaching `8003648C..800364A8`. The additional setup provides a valid animation table, forces a different entry state and varies starting height. It covers the full 23-instruction function, including the hop counter/height writes and `MonkeyBodyOn`, without adding a proof or exclusion.

## Validation

Each file is checked at its existing default scale and seed against the real EXE and area-backed MIPS oracle, comparing return values, RAM, scratchpad and hook sequences; each full `--mutate` suite is also run. The source DLL builds with `-Wall -Wextra -Werror`. Default-scale per-function counts are listed below; no ledger minimum was lowered.

Identifier and constant edits were made in file batches; default differential runs were repeated after the substantial control-flow rewrites. A full checker was not run after each individual identifier-only function edit. The final runs validate every original function in the edited files.

### base

Default: 9209 cases, 58.1 s. Mutations: 37/37 caught (all passed).

| Function | Status | Default cases | Instruction coverage | Declared unreachable instructions |
|---|---|---:|---:|---:|
| `MonkeyRand` | ported + diffed | 120 | 100.0% | 0 |
| `MonkeyCaughtGroupValue` | ported + diffed | 40 | 100.0% | 0 |
| `MonkeyGetAnimIndex` | ported + diffed | 160 | 100.0% | 0 |
| `MonkeyNop` | ported + diffed | 1 | 100.0% | 0 |
| `MonkeyBodyRegister` | ported + diffed | 120 | 100.0% | 0 |
| `MonkeyBodyUnregister` | ported + diffed | 120 | 100.0% | 0 |
| `MonkeyActivityReset` | ported + diffed | 16 | 100.0% | 0 |
| `MonkeyStoreActivities` | ported + diffed | 160 | 100.0% | 0 |
| `GimmickAllowsMonkeyPassage` | ported + diffed | 200 | 100.0% | 0 |
| `MonkeyVoiceQueueReset` | ported + diffed | 8 | 100.0% | 0 |
| `MonkeyQueueVoice` | ported + diffed | 120 | 100.0% | 0 |
| `DelayedSfxUpdate` | ported + diffed | 120 | 100.0% | 0 |
| `NavNodeGetLink` | ported + diffed | 480 | 100.0% | 0 |
| `AreaIsSpecialMonkeyArea` | ported + diffed | 84 | 100.0% | 0 |
| `MonkeyIsState1E_to_20` | ported + diffed | 78 | 100.0% | 0 |
| `AreaIsBossArena` | ported + diffed | 84 | 100.0% | 0 |
| `MonkeyIsIdleActivityState` | ported + diffed | 78 | 100.0% | 0 |
| `MonkeyIsIdleEndState` | ported + diffed | 78 | 100.0% | 0 |
| `MonkeyIsAltAnimState` | ported + diffed | 78 | 100.0% | 0 |
| `MonkeyIsState0C_or_24` | ported + diffed | 78 | 100.0% | 0 |
| `MonkeyIsJumpArcState` | ported + diffed | 78 | 100.0% | 0 |
| `MonkeyIsAttackState` | ported + diffed | 78 | 100.0% | 0 |
| `MonkeySpawnProjectile` | ported + diffed | 160 | 100.0% | 0 |
| `AngleBetweenXZ` | ported + diffed | 304 | 100.0% | 0 |
| `DistanceIntToShort3D` | ported + diffed | 280 | 100.0% | 0 |
| `Distance3D` | ported + diffed | 280 | 100.0% | 0 |
| `DistanceXZ` | ported + diffed | 280 | 100.0% | 0 |
| `MonkeyCalcMoveDir` | ported + diffed | 280 | 100.0% | 0 |
| `DotXZ_SV` | ported + diffed | 280 | 100.0% | 0 |
| `DotXZ_V` | ported + diffed | 280 | 100.0% | 0 |
| `Atan2Angle4096` | ported + diffed | 280 | 100.0% | 0 |
| `NavNodeIsSpecial` | ported + diffed | 120 | 100.0% | 0 |
| `NavNodeGetLinkIds` | ported + diffed | 160 | 100.0% | 0 |
| `NavNodeGetLinkIdsEx` | ported + diffed | 160 | 100.0% | 0 |
| `ObjGetBonePos` | ported + diffed | 200 | 100.0% | 0 |
| `MonkeyReachedNode` | ported + diffed | 360 | 100.0% | 0 |
| `MonkeyNavBacktracked` | ported + diffed | 240 | 100.0% | 0 |
| `MonkeyGetCallVoice` | ported + diffed | 48 | 100.0% | 0 |
| `MonkeyGetCallVoice2` | ported + diffed | 48 | 100.0% | 0 |
| `MonkeyFacingFromLinkCode` | ported + diffed | 60 | 100.0% | 0 |
| `MonkeyAttackChance` | ported + diffed | 160 | 100.0% | 0 |
| `MonkeyApproachAngleNear` | ported + diffed | 160 | 100.0% | 0 |
| `MonkeyApproachAngleFar` | ported + diffed | 160 | 90.0% | 4 |
| `NavNodeLinkFlag` | ported + diffed | 240 | 100.0% | 0 |
| `MonkeyPuff14` | ported + diffed | 40 | 100.0% | 0 |
| `MonkeyPuff5` | ported + diffed | 40 | 100.0% | 0 |
| `StaticModelDraw` | ported + diffed | 48 | 100.0% | 0 |
| `MonkeyStub_ReturnZero` | ported + diffed | 1 | 100.0% | 0 |
| `MonkeyInterpolateLinkVector` | ported + diffed | 240 | 97.8% | 2 |
| `MonkeyResetNavVisits` | ported + diffed | 120 | 100.0% | 0 |
| `NavGroundHeightBetween` | ported + diffed | 320 | 97.22% | 2 |
| `ActorContactOwnerType` | ported + diffed | 120 | 100.0% | 0 |
| `MonkeyDebugMenuInit` | ported + diffed | 1 | 100.0% | 0 |
| `MonkeyDebugMenuInit2` | ported + diffed | 80 | 100.0% | 0 |

Undeclared gaps: none.

Mutation pairs rewritten: `MonkeyRand multiplier`, `StoreActivities state 0x2B`, `NavNodeGetLink mask`, `Projectile y offset`, `Projectile counter`, `AngleBetween cross rounding`, `ReachedNode state 9`, `Backtracked prev1`, `AttackChance x100`, `ApproachNear shift`, `ApproachFar divisor`, `LinkFlag stop`, `StaticModelDraw bones`, `Interpolate z`, `GroundHeight compare`, `ContactOwnerType mask`, `DebugMenuInit2 loop`, `DelayedSfx decrement`, `BodyRegister type`.

### level1

Default: 11843 cases, 71.3 s. Mutations: 82/82 caught (all passed).

| Function | Status | Default cases | Instruction coverage | Declared unreachable instructions |
|---|---|---:|---:|---:|
| `MonkeyRandomHesitate` | ported + diffed | 200 | 100.0% | 0 |
| `MonkeyAdvanceAnimFrame` | ported + diffed | 187 | 100.0% | 0 |
| `MonkeyPickAnim` | ported + diffed | 187 | 100.0% | 0 |
| `MonkeyAnimFinished` | ported + diffed | 187 | 100.0% | 0 |
| `MonkeyAnimAtStart` | ported + diffed | 187 | 100.0% | 0 |
| `MonkeyFireProjectile6` | ported + diffed | 80 | 100.0% | 0 |
| `MonkeyAimAtTarget` | ported + diffed | 600 | 100.0% | 0 |
| `MonkeyFireProjectileAimed` | ported + diffed | 160 | 100.0% | 0 |
| `MonkeyFireProjectile7` | ported + diffed | 80 | 100.0% | 0 |
| `MonkeyFireProjectile8` | ported + diffed | 80 | 100.0% | 0 |
| `MonkeyAngleToTarget` | ported + diffed | 280 | 100.0% | 0 |
| `MonkeyApplyMoveDir` | ported + diffed | 240 | 100.0% | 0 |
| `NavNodeGetPos` | ported + diffed | 360 | 100.0% | 0 |
| `MonkeyTargetInSightRange` | ported + diffed | 360 | 100.0% | 0 |
| `MonkeyBodyOff` | ported + diffed | 120 | 100.0% | 0 |
| `MonkeyBodyOn` | ported + diffed | 120 | 100.0% | 0 |
| `MonkeyDefeatBurst` | ported + diffed | 48 | 100.0% | 0 |
| `MonkeyFootDust` | ported + diffed | 160 | 100.0% | 0 |
| `MonkeyDebrisBurst6` | ported + diffed | 60 | 100.0% | 0 |
| `MonkeyDebrisBurst1` | ported + diffed | 60 | 100.0% | 0 |
| `MonkeyAlertPuff` | ported + diffed | 60 | 100.0% | 0 |
| `MonkeyUpdateContact` | ported + diffed | 2592 | 100.0% | 0 |
| `MonkeyProjectToScreen` | ported + diffed | 360 | 100.0% | 0 |
| `MonkeyAlertIconKind` | ported + diffed | 120 | 100.0% | 0 |
| `MonkeySelectLod` | ported + diffed | 160 | 100.0% | 0 |
| `MonkeyPickGoalNode` | ported + diffed | 480 | 95.05% | 9 |
| `MonkeyLoadLinkExtra` | ported + diffed | 360 | 100.0% | 0 |
| `NavLinkLookupExtra` | ported + diffed | 360 | 100.0% | 0 |
| `MonkeyMoveStep` | ported + diffed | 947 | 96.6% | 18 |
| `MonkeyTurnTowardTarget` | ported + diffed | 400 | 100.0% | 0 |
| `MonkeyLineOfSight` | ported + diffed | 280 | 93.03% | 17 |

Undeclared gaps: none.

Mutation pairs rewritten: `AdvanceAnimFrame clamp`, `AdvanceAnimFrame branch`, `AnimFinished compare`, `AnimAtStart plus one`, `Aim wobble y`, `Aim shift 6`, `Aim lod threshold`, `Aim slope 2896`, `Aim type 5 scale`, `Aimed y offset`, `Fire8 y offset`, `AngleToTarget wrap`, `NavNodeGetPos y`, `NavNodeGetPos record`, `SightRange nibble`, `BodyOff flags`, `BodyOn flags`, `FootDust modulus`, `FootDust y`, `Debris modulus`, `Debris count`, `Contact net state`, `Contact net clears`, `Contact type 11`, `Contact type 3 state`, `Contact flags2 bit`, `Contact +0x124`, `Project otz`, `AlertIcon nop`, `SelectLod 2000`, `SelectLod model`, `PickGoal table`, `LoadLinkExtra boss`, `LoadLinkExtra order`, `Lookup swap`, `Lookup result`, `MoveStep info speed`, `MoveStep angle`, `MoveStep double`, `Turn threshold`, `Turn sign`, `LOS y offset`, `LOS ground 200`, `LOS ground 50`, `LOS step z`, `LOS flag y`.

### level2

Default: 8604 cases, 60.3 s. Mutations: 96/96 caught (all passed).

| Function | Status | Default cases | Instruction coverage | Declared unreachable instructions |
|---|---|---:|---:|---:|
| `MonkeySetState` | ported + diffed | 232 | 100.0% | 0 |
| `MonkeyUpdateLinkSlope` | ported + diffed | 256 | 92.31% | 5 |
| `MonkeyUpdateGoalPos` | ported + diffed | 289 | 100.0% | 0 |
| `MonkeyEnterState_32` | ported + diffed | 160 | 100.0% | 0 |
| `MonkeyEnterState_3D` | ported + diffed | 64 | 100.0% | 0 |
| `MonkeyEnterState_3E` | ported + diffed | 64 | 100.0% | 0 |
| `MonkeyEnterState_3F` | ported + diffed | 64 | 100.0% | 0 |
| `MonkeyEnterState_17` | ported + diffed | 64 | 100.0% | 0 |
| `MonkeyEnterState_2A` | ported + diffed | 102 | 100.0% | 0 |
| `MonkeyPrepareJumpArc` | ported + diffed | 992 | 93.29% | 20 |
| `MonkeyTryAttackKind` | ported + diffed | 800 | 100.0% | 0 |
| `MonkeyNavLinkAllowed` | ported + diffed | 800 | 100.0% | 0 |
| `MonkeyDrawAlertIcon` | ported + diffed | 358 | 97.76% | 3 |
| `MonkeyScoreLinksByTarget` | ported + diffed | 288 | 100.0% | 0 |
| `MonkeyNodeTargetCosines` | ported + diffed | 224 | 100.0% | 0 |
| `MonkeyUpdateFacing` | ported + diffed | 2022 | 98.16% | 4 |
| `MonkeyFireProjectileByKind` | ported + diffed | 128 | 100.0% | 0 |
| `MonkeyUpdateSight` | ported + diffed | 1441 | 100.0% | 0 |
| `MonkeyGetWaypointPos` | ported + diffed | 256 | 100.0% | 0 |

Undeclared gaps: none.

Mutation pairs rewritten: `SetState range`, `SetState header`, `Enter3D node`, `Enter3E timer`, `Enter3F node`, `Enter2A y`, `GoalPos flag`, `GoalPos target`, `JumpArc 100`, `JumpArc 70`, `JumpArc 341`, `JumpArc 1002`, `JumpArc steep`, `JumpArc 1080000`, `JumpArc 60`, `JumpArc 300`, `JumpArc 110`, `JumpArc duration`, `JumpArc mask`, `JumpArc flat`, `JumpArc flat100`, `JumpArc vel 120`, `JumpArc vy`, `Attack timer`, `Attack count`, `Attack counter12`, `Attack kind7`, `Attack range`, `Attack range lo`, `Attack timercheck`, `Attack anim0`, `Allowed alerted`, `Allowed notalerted`, `Allowed goal`, `Allowed nodeflag`, `Icon 10`, `Icon 896`, `Icon lod3 y`, `Icon decrement`, `Score index`, `Score count`, `Score loop`, `Waypoint z`, `Facing 34`, `Facing 170`, `Facing 9`, `Facing 22div`, `Facing 2049`, `Facing sign`, `Facing neg`, `Facing 120`, `Sight 569`, `Sight 3528`, `Sight 1137`, `Sight 1480`, `Sight 2504`, `Sight mode`, `Sight seven`, `Sight range2`, `Sight boss`, `Sight rc`, `Sight mask`.

### level3

Default: 6270 cases, 50.2 s. Mutations: 138/138 caught (all passed).

| Function | Status | Default cases | Instruction coverage | Declared unreachable instructions |
|---|---|---:|---:|---:|
| `MonkeyRestoreActivities` | ported + diffed | 280 | 100.0% | 0 |
| `MonkeyUpdateGoal` | ported + diffed | 200 | 100.0% | 0 |
| `MonkeyStartNotice` | ported + diffed | 100 | 100.0% | 0 |
| `MonkeyEnterState_1E` | ported + diffed | 240 | 100.0% | 0 |
| `MonkeyEnterState_34` | ported + diffed | 60 | 100.0% | 0 |
| `MonkeyEnterKnockdown` | ported + diffed | 240 | 100.0% | 0 |
| `MonkeyEnterState2F` | ported + diffed | 100 | 100.0% | 0 |
| `MonkeyEnterState_30` | ported + diffed | 100 | 100.0% | 0 |
| `MonkeyEnterState_36` | ported + diffed | 80 | 100.0% | 0 |
| `MonkeyEnterState_20` | ported + diffed | 80 | 100.0% | 0 |
| `MonkeyEnterState_24` | ported + diffed | 100 | 100.0% | 0 |
| `MonkeyEnterState_0C` | ported + diffed | 100 | 100.0% | 0 |
| `MonkeyEnterState_35` | ported + diffed | 60 | 100.0% | 0 |
| `MonkeyEnterState_1A` | ported + diffed | 120 | 100.0% | 0 |
| `MonkeyEnterState_09` | ported + diffed | 240 | 100.0% | 0 |
| `MonkeyEnterIdleWait` | ported + diffed | 160 | 97.56% | 1 |
| `MonkeyResumeIdle` | ported + diffed | 60 | 100.0% | 0 |
| `MonkeyEnterRandomLook` | ported + diffed | 80 | 100.0% | 0 |
| `MonkeyPickAttackA` | ported + diffed | 480 | 100.0% | 0 |
| `MonkeyPickAttackB` | ported + diffed | 480 | 100.0% | 0 |
| `MonkeyPickAttackC` | ported + diffed | 480 | 100.0% | 0 |
| `MonkeyMarkBlockedLinks` | ported + diffed | 360 | 100.0% | 0 |
| `MonkeyUpdateTargetDistance` | ported + diffed | 270 | 100.0% | 0 |
| `MonkeyDrawModel` | ported + diffed | 1000 | 100.0% | 0 |
| `MonkeyAi_State3E` | ported + diffed | 200 | 100.0% | 0 |
| `MonkeyNavVisitUpdate` | ported + diffed | 600 | 99.55% | 1 |

Undeclared gaps: none.

Mutation pairs rewritten: `Restore four`, `Restore nodebit`, `Restore field`, `Restore target`, `1E timer`, `1E flag`, `1E boss4a`, `1E z230`, `1E state`, `1E from`, `1E pos`, `Knock 90`, `Knock 30`, `Knock timer`, `Knock voice`, `Knock bit`, `Knock sfx`, `Knock 158`, `Knock puff`, `Knock target`, `Knock clear`, `2F e2`, `2F y`, `2F r92`, `2F r124`, `2F sfx`, `30 flag`, `36 clear`, `36 voice`, `35 node`, `1A y`, `09 plus200`, `09 r22E`, `09 clear`, `Idle mask`, `Idle shl`, `Idle off`, `Idle store`, `Resume clear`, `Pick tries`, `Pick none`, `Pick mask`, `Mark clear`, `Mark base`, `Mark cond`, `Mark count`, `Mark args`, `Target store`, `Target angle`, `Target pos`, `3E limit`, `3E z`, `3E y`, `Visit bit`, `Visit node1`, `Visit parity`, `Visit a2`, `Visit cap`, `Visit mark`, `Visit count`, `Visit args`, `Visit goalAge`, `Visit clear27`, `Visit nodes0`, `Visit clearflag`, `Visit slot`, `Draw mod3`, `Draw 0C`, `Draw flagbits`, `Draw scale`, `Draw bossctx`, `Draw lod3`, `Draw ot`, `Draw ctx90`, `Draw state1A`, `Draw s2a`, `Draw s2b`, `Draw modelidx`, `Draw loop8`, `Draw lodend`, `Draw table`, `Draw rec`, `Draw modeloff`, `Draw handle`, `Draw screen`, `Draw bones`.

### level4

Default: 2574 cases, 26.7 s. Mutations: 124/124 caught (all passed).

| Function | Status | Default cases | Instruction coverage | Declared unreachable instructions |
|---|---|---:|---:|---:|
| `MonkeySetNavNode` | ported + diffed | 611 | 100.0% | 0 |
| `MonkeyEnterState_1D` | ported + diffed | 150 | 100.0% | 0 |
| `MonkeyEnterState_04` | ported + diffed | 150 | 100.0% | 0 |
| `MonkeyEnterState_2C` | ported + diffed | 125 | 100.0% | 0 |
| `MonkeyPickBestLink` | ported + diffed | 600 | 98.41% | 5 |
| `MonkeySwapNavNodes` | ported + diffed | 100 | 100.0% | 0 |
| `MonkeyAi_Fall` | ported + diffed | 150 | 100.0% | 0 |
| `MonkeyArriveAtNode` | ported + diffed | 488 | 99.68% | 1 |
| `MonkeyUpdateVisuals` | ported + diffed | 200 | 100.0% | 0 |

Undeclared gaps: none.

Mutation pairs rewritten: `NavNode shift`, `NavNode store`, `NavNode f2bit`, `NavNode copy`, `1D state`, `1D lookup`, `1D clear`, `1D n1`, `1D timer`, `04 state`, `04 count`, `04 len`, `04 v19`, `04 node`, `2C count`, `2C len`, `2C entry`, `Swap order`, `Swap ret`, `Fall mul`, `Fall off`, `Fall 50`, `Fall snap`, `Fall count`, `Arrive act`, `Arrive b5`, `Arrive f26`, `Arrive f22`, `Arrive alerted1`, `Arrive 30`, `Arrive f3`, `Arrive cf1`, `Arrive evnum`, `Arrive alert0`, `Arrive b2`, `Arrive 22C`, `Arrive st42`, `Arrive keep`, `Arrive 400000a`, `Arrive b8`, `Arrive bossst`, `Arrive contact`, `Arrive lookflag`, `Arrive st0`, `Arrive 100000`, `Arrive clr79`, `Pick bossnode`, `Pick dbl`, `Pick init`, `Pick hist`, `Pick cmp1`, `Pick s6`, `Pick s7`, `Pick slot`, `Pick flag27`, `Pick s3lt`, `Pick alerted`, `Pick state3D`, `Pick cosine`, `Pick cos2`, `Pick cand`, `Pick alert2`, `Pick bossret`, `Visuals ptr`, `Visuals mask`, `Visuals f0`, `Visuals lim`, `Visuals sign`, `Visuals f2`, `Visuals act2`, `Visuals area`, `Visuals snd`, `Visuals fade`, `Visuals text`, `Visuals lod3`, `Visuals slot`, `Visuals A`, `Visuals 12`, `Visuals zero`, `Visuals scratch`, `Visuals loop`, `Visuals draw`.

### level5

Default: 2346 cases, 28.1 s. Mutations: 75/75 caught (all passed).

| Function | Status | Default cases | Instruction coverage | Declared unreachable instructions |
|---|---|---:|---:|---:|
| `MonkeyNodeActionDispatch` | ported + diffed | 966 | 100.0% | 0 |
| `MonkeyConsiderTurnBack` | ported + diffed | 690 | 100.0% | 0 |
| `MonkeyConsiderTurnBackRandom` | ported + diffed | 690 | 100.0% | 0 |

Undeclared gaps: none.

Mutation pairs rewritten: `Disp flag22`, `Disp 100`, `Disp 800`, `Disp node1`, `Disp 20`, `Disp 20000`, `Disp 8`, `Disp 4`, `Disp 10`, `Disp cooldown`, `Disp cdoff`, `Disp 1`, `Disp 10000`, `Disp 800000`, `Disp 1000000`, `Disp st61`, `Disp hopf0`, `Disp hopf1`, `Disp hopdist`, `Disp hopdist2`, `Disp hopdist3`, `Disp boss2`, `Disp prev`, `Disp prevoff`, `Turn boss`, `Turn hist0`, `Turn swapret`, `Turn hist1`, `Turn link01`, `Turn link0ret`, `Turn dirx`, `Turn dirz`, `Turn cnt`, `Turn cnt1`, `Turn pos1`, `Turn pos0`, `Turn dirn`, `Turn dirn2`, `Rand hist0`, `Rand hist1`, `Rand link0`, `Rand 682`, `Rand cmp682`.

### level6

Default: 2308 cases, 26.5 s. Mutations: 73/73 caught (all passed).

| Function | Status | Default cases | Instruction coverage | Declared unreachable instructions |
|---|---|---:|---:|---:|
| `MonkeyAlertedDecide` | ported + diffed | 733 | 100.0% | 0 |
| `MonkeyChooseNextNodeA` | ported + diffed | 375 | 100.0% | 0 |
| `MonkeyChooseNextNodeB` | ported + diffed | 1200 | 98.58% | 4 |

Undeclared gaps: none.

Mutation pairs rewritten: `Alert st12`, `Alert st36`, `Alert 249`, `Alert 79`, `Alert 12C`, `Alert 3`, `Alert alerted1`, `Alert d300`, `Alert d1500`, `Alert distoff`, `Alert boss2`, `Alert st11`, `Alert s1a`, `Alert f26`, `Alert s1b`, `Alert st11b`, `Alert bit3`, `Alert 222`, `Alert 222u`, `Alert fin3`, `Alert fin8`, `Alert f23`, `Alert atkst`, `Alert k100`, `Alert st8`, `Alert st35`, `Alert st57`, `Alert cd`, `Alert cdoff`, `Alert r30`, `Alert r30b`, `Alert jump`, `Alert s9`, `Alert s1A`, `Alert s35`, `Alert f200`, `Alert boss3`, `Alert prev`, `Alert s2`, `B 682`, `B alerted`, `B hist`, `B blockedoff`, `B ret1`, `B cnt2`, `B scoreoff`, `B cmp7`, `B better`, `B take`, `B p2`, `B s4s7`, `B fallback`.

### level7

Default: 2616 cases, 37.8 s. Mutations: 113/113 caught (all passed).

| Function | Status | Default cases | Instruction coverage | Declared unreachable instructions |
|---|---|---:|---:|---:|
| `MonkeyPatrolDecide` | ported + diffed | 638 | 100.0% | 0 |
| `MonkeySelectNextNode` | ported + diffed | 700 | 93.42% | 20 |
| `MonkeyAi_IdleActivityStart` | ported + diffed | 434 | 100.0% | 0 |
| `MonkeyAi_IdleActivityLoop` | ported + diffed | 434 | 100.0% | 0 |
| `MonkeyAi_GoToFlaggedNode` | ported + diffed | 410 | 100.0% | 0 |

Undeclared gaps: none.

Mutation pairs rewritten: `Patrol alerted`, `Patrol st12`, `Patrol st36`, `Patrol alert0`, `Patrol bit3`, `Patrol 222`, `Patrol disp`, `Patrol info2`, `Patrol info4`, `Patrol i12`, `Patrol lt12`, `Patrol lim8`, `Patrol st0`, `Patrol cd`, `Patrol cdst`, `Patrol st25`, `Patrol st25b`, `Patrol st2`, `Patrol idleend`, `Patrol rm`, `Patrol rm2`, `Patrol b98st`, `Patrol b98r`, `Patrol b98r9`, `Patrol b98bit`, `Patrol b98mask`, `Patrol c14`, `Patrol c14i`, `Patrol c14c`, `Patrol c14s`, `Patrol c60`, `Patrol c60b`, `Patrol c60s`, `Patrol puff`, `Patrol prev`, `Select hist`, `Select f27`, `Select goal`, `Select alerted`, `Select unA`, `Select boss7`, `Select s2s`, `Select A1`, `Select st61`, `Select Bp`, `Select 10000`, `Select s2s1`, `Select backtr`, `Select score1`, `Select mark1`, `Select hist2`, `Select blk`, `Select score`, `Select less`, `Select s5`, `Select s5neg`, `Select s6zero`, `Select b454`, `Select b454s`, `Select arrive`, `Select arrnode`, `Select s0e`, `Select print`, `Select printarg`, `Start mod3`, `Start id`, `Start alerted`, `Start t10`, `Start t98`, `Loop dec`, `Loop mask`, `Loop special`, `Flag 2000`, `Flag lt`, `Flag s2`, `Flag pos`, `Flag n1`, `Flag load`.

## Existing coverage proofs and blind spots

No registry row was edited or added. The tables above distinguish executed instructions from previously declared unreachable instructions. Existing stack-window exclusions and shared stale values remain part of the established checker contract. The original proof rows for these eight checkers are reproduced below for review.

| Checker | Scope | Reason |
|---|---|---|
| `check_monkey_base` | `MonkeyApproachAngleFar 80037E3C 80037E4C..80037E54` | INT_MIN / constant divisor guard: the divisor is a constant, so the break can never fire (monkeys-port.md batch 1). |
| `check_monkey_base` | `MonkeyInterpolateLinkVector 8003C604 8003C658` | Second and third zero-divisor break guards: the same field was already divided by earlier in the function, so it is non-zero (monkeys-port.md batch 1). |
| `check_monkey_base` | `NavGroundHeightBetween 80040C84 80040CD4` | Zero-divisor break guards: both divisors are non-zero by the preceding branches (monkeys-port.md batch 1). |
| `check_monkey_base` | `ratan2 (SDK) 80014628 80014638..80014640 80014670..80014678` | Guards that cannot be reached with valid operands; the reachable ones are exercised by the direct SDK check (monkeys-port.md batch 1). |
| `check_monkey_level1` | `MonkeyPickGoalNode 8003AE44 8003AE54..8003AE5C 8003AECC 8003AEDC..8003AEE4 8003AFEC` | Divide-trap guards (divisors non-zero by construction) and the a1 == 0 loop guard (monkeys-port.md batch 2). |
| `check_monkey_level1` | `MonkeyMoveStep 8003BAD8..8003BAE0 8003BC94..8003BC9C 8003BE2C..8003BE58` | Divide-trap guards of the hop and state 62 paths (non-zero divisors by construction) and the second AreaIsBossArena test at 8003BE1C, which the first test already excluded (monkeys-port.md batch 2). |
| `check_monkey_level1` | `MonkeyLineOfSight 80040364 80040374..8004037C 800403F8 80040408..80040410 80040438 80040448..80040450` | Divide-trap guards: non-zero divisors by construction (monkeys-port.md batch 2). |
| `check_monkey_level1` | `MonkeyLineOfSight 80040540 80040550..80040558 80040590` | CONTRIVED, not proven unreachable: the INT_MIN / -1 guard at 80040550 is reachable only with contrived operands and is not covered (monkeys-port.md batch 2). |
| `check_monkey_level2` | `MonkeyUpdateLinkSlope 800353EC 800353FC..80035404 80035414` | Divide guards and the negation of a quotient that cannot be negative (monkeys-port.md batch 3 level 2). |
| `check_monkey_level2` | `MonkeyPrepareJumpArc 800366F0..80036708 8003674C 800367F8 80036808..80036810 80036848 80036858..80036860 800368CC 800368DC..800368E4` | s6 == 0 (the cross product cos(s0)*s1 - sin(s0)*s5 cannot vanish for the angles the function derives) and divide guards that are non-zero by construction (monkeys-port.md batch 3 level 2). |
| `check_monkey_level2` | `MonkeyDrawAlertIcon 80039C9C..80039CA0 80039DDC` | An icon kind other than 0 / 448 / 896 cannot occur (80039C9C / A0); the lbu at 80039DDC can never be negative, so the lod < 0 branch is dead (monkeys-port.md batch 3 level 2). |
| `check_monkey_level2` | `MonkeyUpdateFacing 8003C4C4 8003C4D4..8003C4DC` | Divide guards: the divisor is 22 / 34 / 170 (monkeys-port.md batch 3 level 2). |
| `check_monkey_level3` | `MonkeyEnterIdleWait 8003636C` | INT_MIN / -1 guard: the dividend is at most 131070 (monkeys-port.md batch 3 level 3). |
| `check_monkey_level3` | `MonkeyNavVisitUpdate 800409DC` | A blez on a value that is always >= 1 (monkeys-port.md batch 3 level 3). |
| `check_monkey_level4` | `MonkeyPickBestLink 800381AC..800381B4` | The INT_MIN / -1 guard of the boss-arena rand % count: the dividend is rand & 0xFFFF (monkeys-port.md batch 4). |
| `check_monkey_level4` | `MonkeyPickBestLink 80038560..80038564` | Second pass: the vis == 0 test with both counters 0, which the early exit 'best counter 0 and own counter 0 -> -1' already took (monkeys-port.md batch 4). |
| `check_monkey_level4` | `MonkeyArriveAtNode 80041130` | Target of a beq on node flag bit 3, which is known to be set on every path that reaches it (monkeys-port.md batch 4). |
| `check_monkey_level6` | `MonkeyChooseNextNodeB 80038EB4..80038EC0` | The fallback pass finding an admissible link after the scored pass found none: the scored pass accepts every admissible link (initial limit 0x2000 is above any cosine score, link ids are masked non-negative) (monkeys-port.md batch 4). |
| `check_monkey_level7` | `MonkeySelectNextNode 8003B3B0..8003B3FC` | Picking from the candidate list (s2 > 0) needs a score >= s4 with s4 = 0x40000000, which is impossible for an s16 (monkeys-port.md batch 4). |

## Integration hooks retained

No hook was opened or closed. These original external operations remain integration boundaries:

| Hook | Original operation |
|---|---|
| `ActorAnimDecode` | Decode the actor animation pose. |
| `AlertIconDraw` | Draw the alert/question icon at a short world position. |
| `ColorFadeSet` | Set the debug color-fade values. |
| `DebugTextPrintfWhite` | Print the monkey ID in the debug overlay. |
| `DrawShadow` | Draw the ground shadow using the scratchpad context. |
| `EffectBigBurst` | Spawn a large effect burst at a world position. |
| `GetGroundHeight` | Query terrain height beneath a position. |
| `GimmickEvent` | Send an arrival event to a gimmick. |
| `GimmickGetInfo` | Fetch a gimmick type and current value. |
| `MatrixFromDirXZ` | Build a horizontal facing matrix. |
| `MatrixLookDir` | Build a matrix from a full facing direction. |
| `MenuItemInitBound` | Bind a menu item to a RAM variable. |
| `MenuItemSetCallback` | Install the debug menu change callback. |
| `MenuItemSetRange` | Set a menu item value range. |
| `MenuItemSetStep` | Set a menu item increment. |
| `ModelBuildBoneMatrices` | Build world-space matrices for the animated bones. |
| `ModelDraw` | Draw transformed model geometry. |
| `ModelDrawWithOt` | Draw the model with its ordering-table offset. |
| `ModelInit` | Initialize an actor from its chosen model header. |
| `ModelPoseBones` | Apply the current animation to model bones. |
| `ModelTransformRigid` | Transform rigid model vertices. |
| `ModelTransformSkinned` | Transform animated skinned vertices. |
| `ObjSlotGetAnimIndex` | Read an object-slot animation index. |
| `ObjSlotGetTarget` | Fetch an object-slot target position. |
| `ObjSlotSetField_CF` | Set the object-slot control byte used by restored/arriving monkeys. |
| `PlaySfxAt` | Play a positioned sound effect. |
| `Printf` | Print diagnostic text. |
| `ProjectileSpawn` | Create the requested projectile or variant. |
| `RotTransPers` | Transform and perspective-project a short point. |
| `ScaleMatrix` | Scale the actor transformation matrix. |
| `SetRotMatrix` | Load the projection rotation matrix. |
| `SetTransMatrix` | Load the projection translation matrix. |
| `SoundLoopStartEx` | Start a monkey-associated looping sound. |
| `SoundLoopStopAndFree` | Stop and free the loop sound. |
| `SparkAddRandom` | Spawn randomized sparks from position and velocity. |
| `SpawnParticle` | Create a particle of the requested kind. |
| `SpawnPuff` | Create a puff effect. |
| `TargetListAdd` | Register an actor targeting entry. |
| `TargetListRemove` | Remove an actor targeting entry. |

Collision-body registration/unregistration and SDK mathematics remain direct calls to their existing verified ports. The body port continues to use the unchanged `AeBodyHooks` supplied through `h->body`.

## Findings and remaining qualifications

- `MonkeyAnimFinished` returns whether frames remain, despite its historical name; its purpose comment now states that behavior. The exported name and return convention were preserved.
- The hesitation/attack gate at `+0xD4` and actual model detail at `+0xD5` are separate bytes. `AE_MK_MODEL_LOD` names the latter; the existing `AE_MK_LOD` name for the former is retained for compatibility.
- The fallback argument of `MonkeyAi_GoToFlaggedNode` preserves the original caller-supplied value when no qualifying node exists. It is named `fallback_node`; the shared-value exclusion remains unchanged.
- Unwritten scratch words, the fourth copied goal-position word, repeated position reads and the aimed-projectile single-slot overwrite were retained. Field names describe observed use; they do not claim a new game-design interpretation.
- Literal state IDs, flag masks, numeric tuning values, vector components, scratch positions and fixed record strides remain where they express the verified behavior. No register-named local, raw hexadecimal field displacement, artificial execution router or delay-slot comment remains in the eight edited C files.
- Two existing level 2 mutations survived the original randomized default-scale families: `GoalPos drop` (200 ? 199 height correction) and `Sight 1137` (one-unit wider sight window). One pure vertical-distance case and one nearby-target window-edge case were added. `MonkeyUpdateGoalPos` now has 289 default cases (288 retained plus one), and `MonkeyUpdateSight` has 1441 (1440 retained plus one).
- The `Visit a2` mutation initially stopped compiling after the formerly reused local was split: removing its condition made the new propagation flag unused. Its replacement contains an explicit unused read while still ignoring the flag in the condition. The mutation remains a runtime differential failure rather than being counted as a compiler rejection.
- The target word at `+0x70` is `Player.stateFlags`, while the monkey field at that offset is velocity. `AE_MK_TARGET_STATE_FLAGS` names that distinction without changing the offset.
- `tools/check_verified.py` passes: 0 errors. All 150 refactored original functions meet their ledger minimums, with 42,522 primary differential cases and no undeclared primary coverage gaps. Seven pre-existing missing-result warnings remain outside this task: `check_bodies`, `check_monkey_level8`, `check_monkey_level9`, `check_monkey_level10`, `check_player_input`, `check_solid`, `check_world`. `git diff --check` passes. The ledger and exclusion registry are unchanged. Final default checks took 26.5?71.3 seconds on this machine; case scales and seeds were retained.
