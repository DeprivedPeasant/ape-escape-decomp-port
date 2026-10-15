| Address | Direct callee | Current implementation |
|---|---|---|
| 80011F6C | XaPlay | src/player/ae_player1.c; hardware-facing entry must remain a named archive hook |
| 80012360 | CdMgrQueueCommand | src/player/ae_player1.c; hardware-facing entry must remain a named archive hook |
| 80012C0C | ArcGetMemberPos | src/player/ae_player1.c |
| 80014168 | PadRumble | src/camera/ae_camupd.c |
| 80014804 | MatrixFromDirXZ | src/core/ae_sdk_matrix.c |
| 80025D20 | CdIntToPos | named hook |
| 80025E24 | CdPosToInt | named hook |
| 8002FB50 | rsin | src/core/ae_sdk.c |
| 8002FC20 | rcos | src/core/ae_sdk.c |
| 800300B0 | ApplyMatrixSV | src/core/ae_sdk.c |
| 800301E0 | VectorNormalSS | src/core/ae_sdk.c |
| 80032ADC | PlayerIdleModelSetup | src/player/ae_player1.c |
| 800433B0 | GetGroundHeightEx | src/world/ae_world.c |
| 80046618 | VecLength3 | src/player/native_player.c |
| 80048430 | AimRequestClear | src/player/ae_player1.c |
| 8004B510 | AimModeLeave | src/player/ae_player_spawn.c |
| 8005D100 | StageRequest | src/world/ae_stage.c |
| 8005DCE4 | StageExitIsActive | src/world/ae_world.c |
| 8005E6F0 | PlayerDamage | src/player/ae_player1.c |
| 80063628 | PlayerAnimateAndDraw | named hook |
| 80064208 | PlayerSetState | src/player/ae_player_act_state.c |
| 80064454 | PlayerRunAnimEvents | src/player/ae_anim_events.c |
| 800649D8 | PlayerRunTransitions | src/player/ae_player2_leaf.c |
| 800652E4 | PlayerIsOnGround | src/player/ae_player2_leaf.c |
| 800657F0 | PlayerCheckWaterEntry | src/player/ae_player2_leaf.c |
| 80066094 | PlayerEnterSwimIfInWater | src/player/ae_player2_leaf.c |
| 800665E0 | PlayerResetAir | src/player/ae_player2_leaf.c |
| 800668A0 | PlayerOnWallBump | src/player/ae_player2_leaf.c |
| 8006699C | PlayerAccelXZ | src/player/ae_player2_leaf.c |
| 80066ADC | PlayerAccelXYZ | src/player/ae_player2_leaf.c |
| 80066C90 | TurnTowardsY | src/player/ae_playerupdate.c |
| 80066FC8 | PlayerOnContact | src/player/ae_player2_leaf.c |
| 80067974 | PlayerCheckGadgetSelect | src/player/native_player.c |
| 80067A1C | PlayerApplyGadgetStance | src/player/ae_player2_leaf.c |
| 80067CAC | PlayerUpdateClimb | src/player/ae_player2_leaf.c |
| 800681E4 | PlayerUpdateRope | src/player/ae_player2_leaf.c |
| 80068634 | PlayerUpdateHeldByObject | src/player/ae_player2_leaf.c |
| 800687D8 | PlayerUpdateIdleAnim | src/player/ae_player2_leaf.c |
| 8006896C | PlayPlayerVoice | src/player/ae_player2_leaf.c |
| 80068ADC | PlayerDrawSquashedWithFx | src/player/ae_player2_leaf.c |
| 80068B1C | PlayerDrawSquashedFlattened | src/player/ae_player2_leaf.c |
| 800698B8 | SpawnParticle | src/effects/ae_spawn.c |
| 8006B70C | PlayerSpawnDust | src/player/ae_player2_leaf.c |
| 8006BF10 | PlayerSpawnDustBehind | src/player/ae_player2_leaf.c |
| 8006BF84 | EffectGroundImpactAtPlayer | src/player/ae_player2_leaf.c |
| 8006C2E8 | PlayerSetTintEffect | src/player/ae_player_act_state.c |
| 80071E44 | CollisionBodyRegister | src/bodies/ae_bodies.c |
| 80072170 | GetObjectByIndex | src/bodies/ae_bodies.c |
| 80072220 | CollisionBodyAlloc | src/bodies/ae_bodies.c |
| 800797F8 | GetSurfaceType | src/world/ae_solid.c |
| 8007AC64 | PlayerSelectGadget | src/player/ae_player2_leaf.c |
| 8007B374 | PlayerSetMode | src/player/ae_player_act_mode.c |
| 8007C2A0 | MonkeyNetCatchUpdate | src/player/ae_player2_leaf.c |
| 800877F8 | PlaySfx | src/player/ae_player_spawn.c |
| 8008C064 | HeapFree | src/render/ae_tex_copy.c |
| 8008C09C | HeapAlloc | src/world/ae_areaset.c |
| 800A3740 | memset | named hook |
| 80142808 | FUN_80142808 | named hook |

Indirect call sites:
80060fb4  0040f809  jalr $ra, $v0
80060fd0  0040f809  jalr $ra, $v0
80062c60  0040f809  jalr $ra, $v0

mode jump table (modes 1..13) @800A5A40:
 0: 80060FA8 internal branch
 1: 80060FE0 internal branch
 2: 80060FC4 internal branch
 3: 80061124 internal branch
 4: 80061000 internal branch
 5: 80061018 internal branch
 6: 80061030 internal branch
 7: 80060FF0 internal branch
 8: 80061124 internal branch
 9: 80061048 internal branch
10: 80061124 internal branch
11: 80061058 internal branch
12: 80061068 internal branch

water-state jump table (states 0x46..0x4D) @800A5A78:
 0: 80061540 internal branch
 1: 80061540 internal branch
 2: 8006163C internal branch
 3: 80061520 internal branch
 4: 80061520 internal branch
 5: 8006163C internal branch
 6: 80061540 internal branch
 7: 80061624 internal branch

PlayerActionFns @800B03AC:
 0: 80064AF0 PlayerCond_False
 1: 80065C20 PlayerAct_ScaleMoveVel
 2: 80065D1C PlayerAct_JumpHoldBoost
 3: 80065D68 PlayerAct_JumpPct3
 4: 80065DEC PlayerAct_VerticalOnly
 5: 80065E18 PlayerAct_JumpPct2
 6: 80065E9C PlayerAct_SquashReset
 7: 80065ED4 PlayerAct_FlyerThrust
 8: 800660C8 PlayerAct_Gadget3Aim
 9: 800660E8 PlayerAct_DriftForward
10: 8006611C PlayerAct_NegateInputXZ
11: 80066140 PlayerAct_TurnAround
12: 80066770 PlayerAct_Land
13: 80066168 PlayerAct_TurnAroundAndJump
14: 80066190 PlayerAct_Nop
15: 80066198 PlayerAct_FaceStick
16: 800666C8 PlayerAct_FaceStickOnRotation
17: 8006671C PlayerAct_Rebound
18: 80066744 PlayerAct_HurtVoiceFlag
19: 800661D4 PlayerAct_JumpFromLStick
20: 800662DC PlayerAct_StepBackDrop
21: 80066358 PlayerAct_BounceBack
22: 80066434 PlayerAct_ExitToMode0Forward
23: 800664D0 PlayerAct_AnimSpeedNormal
24: 800664E0 PlayerAct_AnimSpeedHalf
25: 800664F0 PlayerAct_FaceStickMaybeHalfSpeed
26: 80066530 PlayerAct_FaceStickRotateA
27: 80066568 PlayerAct_FaceStickRotateB
28: 800665A0 PlayerAct_AimModeEnter
29: 800665C0 PlayerAct_AimModeLeave
30: 800665E0 PlayerResetAir
31: 800665F8 PlayerAct_FaceRightPerp
32: 8006663C PlayerAct_FaceLeftPerp
33: 80066680 PlayerAct_TurnFacingLeft90
34: 800666A4 PlayerAct_TurnFacingRight90
35: 80065FC8 PlayerAct_FlyerLaunchA
36: 80065FF0 PlayerAct_FlyerLaunchB
37: 80066018 PlayerStartHurtBlink
38: 80066028 PlayerAct_GadgetSpinFromStick
39: 80066094 PlayerEnterSwimIfInWater
