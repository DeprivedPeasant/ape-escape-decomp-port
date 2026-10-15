# Player condition call tree

The condition functions occupy `0x80064AF0..0x80065C20` and are reached indirectly from the player state transition tables. The instruction call scan of all 65 requested functions found these direct edges:

```text
PlayerCond_AnimDoneRandom
└── PlayerCond_AnimDone                         (in this batch; direct C call)

PlayerCond_CanJump
└── PlayerCond_JumpPressed                      (in this batch; direct C call)

PlayerCond_GroundBehindLow
└── GetGroundHeight 0x80043400                  (already ported; ae_GetGroundHeight)
    └── GetSolidHeight 0x8004D8C0               (already ported, reached through AeWorldHooks)

PlayerCond_LandingSoon
└── GetGroundHeight 0x80043400                  (already ported; ae_GetGroundHeight)
    └── GetSolidHeight 0x8004D8C0               (already ported, reached through AeWorldHooks)

PlayerCond_WadingOnGround
└── PlayerIsOnGround 0x800652E4                 (outside this batch; named hook)

PlayerCond_JumpOutOfWater
└── PlayerSplash 0x8006B950                     (outside this batch; named hook)

PlayerCond_JumpPressedAtApex
└── PlayerFxJumpBurst 0x8006C58C                (outside this batch; named hook)
```

No other direct `jal` or `jalr` occurs in the requested function ranges. `GetGroundHeight` is in `tools/verified.csv`, so the port calls the existing RAM-image implementation; its `GetSolidHeight` dependency is supplied through its established world hook interface. The three player callees outside the requested set remain hooks, with their original names and addresses documented in `ae_player_cond.h`.

The old `src/player/native_player.c` condition switch is orientation material only. The port and checker are based on the original EXE instructions and preserve their signed/unsigned comparisons, wrapping, halfword truncation, call order, and state mutations.

## Validation and completed port

`src/player/ae_player_cond.c` and `.h` contain readable, structured C for all 65 original predicates. The checker runs each function against the original SCUS_944.23 instructions on identical RAM images with the real EXE and area 1 member loaded. The indirect player-state table is backed with valid RAM entries; the terrain pointers are fixed up as `AreaLoad` does for area 1. Out-of-batch callbacks receive the same deterministic results and RAM/scratch effects on both sides.

Default validation: 975 cases total (15 per function: three targeted edge scenarios plus 12 deterministic random scenarios), 1,003/1,003 instructions covered, no RAM/scratch/return/hook mismatches, and 47.7 seconds elapsed. The checker is `tools/mips/check_player_cond.py`. Mutation testing used two wrong early-return mutations for each function: 130/130 caught. Each function is `diffed` with `min_cases=15` in `tools/verified.csv`. There are no coverage gaps or unreachable-instruction declarations.

| Address | Function | Status | Default cases | Coverage |
|---|---|---:|---:|---:|
| `80064AF0` | `PlayerCond_False` | diffed | 15 | 100% (2/2) |
| `80064AF8` | `PlayerCond_JumpHeld` | diffed | 15 | 100% (4/4) |
| `80064B08` | `PlayerCond_JumpPressed` | diffed | 15 | 100% (7/7) |
| `80064B24` | `PlayerCond_RStickEvent02` | diffed | 15 | 100% (38/38) |
| `80064BBC` | `PlayerCond_RStickEvent40` | diffed | 15 | 100% (32/32) |
| `80064C3C` | `PlayerCond_RStickBackHard` | diffed | 15 | 100% (14/14) |
| `80064C74` | `PlayerCond_GadgetBusyOrGesture` | diffed | 15 | 100% (21/21) |
| `80064CC8` | `PlayerCond_FacingDesiredDir` | diffed | 15 | 100% (20/20) |
| `80064D18` | `PlayerCond_RStickSideNeg` | diffed | 15 | 100% (13/13) |
| `80064D4C` | `PlayerCond_RStickSidePos` | diffed | 15 | 100% (12/12) |
| `80064D7C` | `PlayerCond_RStickFwd` | diffed | 15 | 100% (12/12) |
| `80064DAC` | `PlayerCond_RStickBack` | diffed | 15 | 100% (13/13) |
| `80064DE0` | `PlayerCond_AttachTimerDone` | diffed | 15 | 100% (9/9) |
| `80064E04` | `PlayerCond_GroundBehindLow` | diffed | 15 | 100% (26/26) |
| `80064E6C` | `PlayerCond_RStickGestureOrTravel` | diffed | 15 | 100% (16/16) |
| `80064EAC` | `PlayerCond_AnimDoneRandom` | diffed | 15 | 100% (19/19) |
| `80064EF8` | `PlayerCond_NoMoveInput` | diffed | 15 | 100% (12/12) |
| `80064F28` | `PlayerCond_LStickFwd` | diffed | 15 | 100% (13/13) |
| `80064F5C` | `PlayerCond_LStickBack` | diffed | 15 | 100% (13/13) |
| `80064F90` | `PlayerCond_RStickEvent04` | diffed | 15 | 100% (35/35) |
| `8006501C` | `PlayerCond_RStickEvent08` | diffed | 15 | 100% (35/35) |
| `800650A8` | `PlayerCond_NoGestureR3Held` | diffed | 15 | 100% (12/12) |
| `800650E8` | `PlayerCond_RStickRotating` | diffed | 15 | 100% (17/17) |
| `8006512C` | `PlayerCond_Gadget1RStickFwdFlick` | diffed | 15 | 100% (19/19) |
| `80065178` | `PlayerCond_Gadget3RStickBack` | diffed | 15 | 100% (19/19) |
| `800651C4` | `PlayerCond_RStickGesture10` | diffed | 15 | 100% (4/4) |
| `800651D4` | `PlayerCond_RStickGesture20` | diffed | 15 | 100% (4/4) |
| `800651E4` | `PlayerCond_RStickNoGesture` | diffed | 15 | 100% (4/4) |
| `800651F4` | `PlayerCond_GestureEvent04` | diffed | 15 | 100% (4/4) |
| `80065204` | `PlayerCond_GestureEvent08` | diffed | 15 | 100% (4/4) |
| `80065214` | `PlayerCond_LStickIdleL3Held` | diffed | 15 | 100% (12/12) |
| `80065244` | `PlayerCond_LStickIdle` | diffed | 15 | 100% (4/4) |
| `80065254` | `PlayerCond_LStickClass2` | diffed | 15 | 100% (4/4) |
| `80065264` | `PlayerCond_LStickClass4` | diffed | 15 | 100% (4/4) |
| `80065274` | `PlayerCond_LStickClass8` | diffed | 15 | 100% (4/4) |
| `80065284` | `PlayerCond_LStickFull` | diffed | 15 | 100% (6/6) |
| `8006529C` | `PlayerCond_RStickFull` | diffed | 15 | 100% (6/6) |
| `800652B4` | `PlayerCond_LStickPartial` | diffed | 15 | 100% (6/6) |
| `800652CC` | `PlayerCond_RStickPartial` | diffed | 15 | 100% (6/6) |
| `80065318` | `PlayerCond_LandingSoon` | diffed | 15 | 100% (42/42) |
| `800653C0` | `PlayerCond_FacingContactWall` | diffed | 15 | 100% (31/31) |
| `8006543C` | `PlayerCond_ContactKind8` | diffed | 15 | 100% (7/7) |
| `80065458` | `PlayerCond_NotContactKind8` | diffed | 15 | 100% (7/7) |
| `80065474` | `PlayerCond_FallingHigh` | diffed | 15 | 100% (14/14) |
| `800654AC` | `PlayerCond_RStickEvent10` | diffed | 15 | 100% (35/35) |
| `80065538` | `PlayerCond_RStickEvent20` | diffed | 15 | 100% (35/35) |
| `800655C4` | `PlayerCond_FirstFrameRStickEvent10` | diffed | 15 | 100% (26/26) |
| `8006562C` | `PlayerCond_FirstFrameRStickEvent20` | diffed | 15 | 100% (26/26) |
| `800656DC` | `PlayerCond_JumpPressedAtApex` | diffed | 15 | 100% (27/27) |
| `80065748` | `PlayerCond_LStickSidePos` | diffed | 15 | 100% (6/6) |
| `80065760` | `PlayerCond_LStickSideNeg` | diffed | 15 | 100% (4/4) |
| `80065770` | `PlayerCond_LStickSideAny` | diffed | 15 | 100% (8/8) |
| `80065790` | `PlayerCond_LStickIdleStillMoving` | diffed | 15 | 100% (20/20) |
| `800657E0` | `PlayerCond_GestureEvent0C` | diffed | 15 | 100% (4/4) |
| `800658E8` | `PlayerCond_InWater` | diffed | 15 | 100% (7/7) |
| `80065904` | `PlayerCond_WadingOnGround` | diffed | 15 | 100% (31/31) |
| `80065980` | `PlayerCond_ContactWall3B` | diffed | 15 | 100% (11/11) |
| `800659AC` | `PlayerCond_JumpOutOfWater` | diffed | 15 | 100% (30/30) |
| `80065A24` | `PlayerCond_CanJump` | diffed | 15 | 100% (49/49) |
| `80065AE8` | `PlayerCond_LStickIdleL3Pressed` | diffed | 15 | 100% (14/14) |
| `80065B20` | `PlayerCond_Dead` | diffed | 15 | 100% (13/13) |
| `80065B54` | `PlayerCond_Gadget4Mode1` | diffed | 15 | 100% (6/6) |
| `80065B6C` | `PlayerCond_AnimDone` | diffed | 15 | 100% (15/15) |
| `80065BA8` | `PlayerCond_False2` | diffed | 15 | 100% (2/2) |
| `80065BB0` | `PlayerCond_Still` | diffed | 15 | 100% (28/28) |

### Remaining hooks

These original functions are outside the 65-predicate batch and remain named hooks in `AePlayerCondHooks`:

- `PlayerIsOnGround` (`800652E4`): reports grounded state (`groundY <= pos.y` and `hasGround`).
- `PlayerSplash` (`8006B950`): spawns splash particle `0x101` at the water surface and plays the corresponding splash sound.
- `PlayerFxJumpBurst` (`8006C58C`): spawns jump-burst effects at the player feet using the player velocity; the symbol description marks this detail tentative.

`GetGroundHeight` (`80043400`) is already ported and called directly. Its existing `AeWorldHooks.GetSolidHeight` dependency is exercised by this checker with identical arguments and side effects.

### Findings

- `PlayerCond_JumpHeld` and `PlayerCond_JumpPressed` return the matching button mask (`0`, `2`, `8`, or `0xA`), not a normalized Boolean. The interrupted C version normalized the return and was corrected.
- `PlayerCond_NoMoveInput` first requires `leftStickClass == 0`, then checks `held & 0x20A`; it does not test `inputLocked`. The interrupted C version used the wrong field and was corrected.
- The MIPS instruction listing confirms the other branch and signedness details in the port, including signed history-slot division for events `02`/`40`, unsigned slot arithmetic for `04`/`08`/`10`/`20`, 16-bit contact/slope fields, the jump-slope table, and the LCG state write. Differential cases include wrap and threshold edges for these paths.

The checker exclusion registry contains only the standard MIPS/host stack-window exclusion (`801FF000..80200000`); all remaining main RAM and all scratchpad bytes are compared. `tools/check_verified.py` discovers `src/player/ae_player_cond.c` automatically through its `src/**/ae_*.c` scan.
