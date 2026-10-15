# Solid collision height queries

## Instruction-derived call map

Inventory sizes are bytes; CFG instructions include disjoint basic blocks.

| Function | Address | Inventory bytes | CFG instructions | Calls |
|---|---|---:|---:|---|
| GetSolidHeight | 8004D8C0 | 1656 | 414 | ColliderHeightAt, CollShape_Box, CollShape_WallThin, CollShape_WallEdge, CollShape_WallDiagonal, CollShape_Ramp45, CollShape_Cylinder, CollShape_Ramp2, CollShape_Ramp4, CollShape_Wedge, CollShape_DiagonalBox, CollProbeCounterInc, GetSurfaceType, GetColliderQuadrantMaskXZ |
| CollShape_Box | 8004E33C | 264 | 66 |  |
| CollShape_WallThin | 8004E444 | 504 | 126 |  |
| CollShape_WallEdge | 8004E63C | 700 | 175 |  |
| CollShape_WallDiagonal | 8004E8F8 | 292 | 73 | DiagonalSignedDistance |
| CollShape_Ramp45 | 8004EA1C | 544 | 136 |  |
| CollShape_Cylinder | 8004EC3C | 244 | 61 | VecLength3, CollHalfPipeHeight |
| CollShape_Ramp2 | 8004ED30 | 356 | 89 |  |
| CollShape_Ramp4 | 8004EE94 | 364 | 91 |  |
| CollShape_Wedge | 8004F000 | 816 | 204 |  |
| CollShape_DiagonalBox | 8004F330 | 264 | 66 | DiagonalSignedDistance |
| ColliderHeightAt | 8004DF38 | 1028 | 257 | VecLength3, CollShape_Box, CollShape_WallThin, CollShape_WallEdge, CollShape_WallDiagonal, CollShape_Ramp45, CollShape_Cylinder, CollShape_Ramp2, CollShape_Ramp4, CollShape_Wedge, CollShape_DiagonalBox, ColliderRingHeight, ColliderStripHeightOut, ColliderSlopeHeight, ColliderHalfSpaceInside, CollHalfPipeHeight, ColliderTrenchHeight, ColliderTypeActive |
| CollProbeCounterInc | 80071D20 | 28 | 7 |  |
| GetSurfaceType | 800797F8 | 560 | 140 |  |
| GetColliderQuadrantMaskXZ | 800889B4 | 56 | 14 |  |
| DiagonalSignedDistance | 800774B8 | 660 | 165 | SquareRoot0 |
| CollHalfPipeHeight | 8004FFC8 | 272 | 68 | SquareRoot0 |
| ColliderRingHeight | 8004F438 | 220 | 55 | VecLength3 |
| ColliderStripHeightOut | 8004F514 | 52 | 13 | ColliderStripHeight |
| ColliderSlopeHeight | 8004F984 | 776 | 194 | ratan2, VectorNormalS, VecLength3, ColliderSlopeSide, ColliderSlopeCorner |
| ColliderHalfSpaceInside | 8004FF20 | 168 | 42 |  |
| ColliderTrenchHeight | 800500D8 | 952 | 238 | SquareRoot0, VecLength3 |
| ColliderTypeActive | 80079A28 | 224 | 56 |  |
| ColliderStripHeight | 8004F548 | 416 | 104 |  |
| ColliderSlopeSide | 8004FC8C | 444 | 111 |  |
| ColliderSlopeCorner | 8004FE48 | 216 | 54 |  |

Collision tree: 3019 instructions, plus four SDK maths callees. No collision callees deferred.

## Shape dispatch table (800AF180)

| Types (inclusive) | Handler |
|---|---|
| 0–2 | CollShape_Box |
| 3–6 | CollShape_WallThin |
| 7–10 | CollShape_Ramp45 |
| 11–18 | CollShape_Ramp2 |
| 19–34 | CollShape_Ramp4 |
| 35–37 | CollShape_Cylinder |
| 38–41 | CollShape_DiagonalBox |
| 42–45 | CollShape_WallThin |
| 46–53 | CollShape_WallEdge |
| 54 | CollShape_Box |
| 55–58 | CollShape_WallDiagonal |
| 59 | CollShape_Cylinder |
| 60–63 | CollShape_Box |
| 64–67 | CollShape_Ramp45 |
| 68 | CollShape_Box |
| 69–73 | CollShape_Cylinder |
| 74–77 | CollShape_WallThin |
| 78–81 | CollShape_Box |
| 82–84 | CollShape_Cylinder |
| 85–88 | CollShape_Wedge |

## SDK boundaries

| Routine | Original | C implementation |
|---|---|---|
| VecLength3 | 80046618 | Existing `ae_player_vec_length3`: narrow each lane to s16, wrap the sum of squared lanes, then IntSqrt (80046584). |
| SquareRoot0 | 800303C0 | Existing `ae_sdk_SquareRoot0`, reading the EXE lookup table. |
| VectorNormalS | 8003019C | New nine-line `ae_sdk_VectorNormalS` wrapper around the existing validated `ae_sdk_normalize_core`; reads s32, writes s16. |
| ratan2 | 800145D0 | Existing `ae_sdk_ratan2`, reading the EXE arctangent table. |

The MIPS side executes the original SDK instructions through the pinned PCSX-ReARMed GTE oracle. The C side calls the actual C maths above, without oracle hooks or substituted math results. There are no sound, effect or rendering calls.

EXE SHA256: `993b4e23509f23f38a196791c6c2a45eeb96c68a6b650da7787948abc6c25700`.

## Readable C port (2026-10-13)

All 26 collision functions in the map are hand-written in `src/world/ae_solid.{c,h}`. The shape dispatch reads
the original table at 800AF180 and switches on its function pointer, so the RAM table remains authoritative.
The code uses structured control flow, named field offsets, local coordinates/heights and the existing RAM API.
No instruction translator, simulated CPU state or generated C is involved. No collision callee is deferred.

The module links `src/core/ae_ram.c`, `src/core/ae_sdk.c` and `src/player/native_player.c` (for the validated
VecLength3). `src/world/ae_world.c` is unchanged; its existing GetSolidHeight hook can call
`ae_GetSolidHeight(r, policy, x, y, z)` through a host adapter when the owner integrates this module.

## Differential validation

Run with `.venv/Scripts/python.exe tools/mips/check_solid.py --coverage`.
The default seed is 94423. The default run has **5,576 direct cases**, taking about **36 seconds** on this machine.
The checker compares the return word, every scratchpad byte and all main RAM outside the declared original
call-stack window (801FF000..80200000). There are no RAM masks or external gameplay stubs. Trapping cases
require the port's sticky error and also compare all observable memory up to the trap.

The original code executes **2,973 / 3,024 instructions** (the collision tree's 3,019 plus the SDK wrapper's five).
The remaining **51 instructions** have six registered proofs. There are **zero undeclared coverage gaps** and
zero stale proofs. The checker treats either as a failure, rather than a warning.

| Function | Status | Direct cases | Original instructions executed / total | Proven impossible |
|---|---|---:|---:|---:|
| CollProbeCounterInc | ported + diffed | 4 | 7 / 7 | 0 |
| GetColliderQuadrantMaskXZ | ported + diffed | 144 | 14 / 14 | 0 |
| ColliderSlopeCorner | ported + diffed | 276 | 54 / 54 | 0 |
| ColliderSlopeSide | ported + diffed | 448 | 111 / 111 | 0 |
| ColliderHalfSpaceInside | ported + diffed | 288 | 42 / 42 | 0 |
| ColliderTypeActive | ported + diffed | 480 | 56 / 56 | 0 |
| ColliderStripHeight | ported + diffed | 110 | 98 / 104 | 6 |
| ColliderStripHeightOut | ported + diffed | 110 | 13 / 13 | 0 |
| CollShape_Box | ported + diffed | 195 | 66 / 66 | 0 |
| CollShape_Ramp2 | ported + diffed | 197 | 89 / 89 | 0 |
| CollShape_Ramp4 | ported + diffed | 197 | 91 / 91 | 0 |
| CollShape_Ramp45 | ported + diffed | 197 | 136 / 136 | 0 |
| CollShape_Wedge | ported + diffed | 198 | 180 / 204 | 24 |
| CollHalfPipeHeight | ported + diffed | 66 | 68 / 68 | 0 |
| CollShape_Cylinder | ported + diffed | 398 | 61 / 61 | 0 |
| ColliderTrenchHeight | ported + diffed | 98 | 232 / 238 | 6 |
| ColliderRingHeight | ported + diffed | 64 | 55 / 55 | 0 |
| DiagonalSignedDistance | ported + diffed | 42 | 165 / 165 | 0 |
| CollShape_WallThin | ported + diffed | 384 | 126 / 126 | 0 |
| CollShape_WallEdge | ported + diffed | 245 | 175 / 175 | 0 |
| CollShape_WallDiagonal | ported + diffed | 108 | 73 / 73 | 0 |
| CollShape_DiagonalBox | ported + diffed | 104 | 66 / 66 | 0 |
| ColliderSlopeHeight | ported + diffed | 178 | 185 / 194 | 9 |
| ColliderHeightAt | ported + diffed | 641 | 252 / 257 | 5 |
| GetSurfaceType | ported + diffed | 55 | 140 / 140 | 0 |
| GetSolidHeight | ported + diffed | 344 | 413 / 414 | 1 |
| VectorNormalS (SDK wrapper) | ported + diffed | 5 | 5 / 5 | 0 |

Real data cases load the complete original area members and set collision globals exactly to their section
addresses, including the counts and grid bounds:

| Area | Member | Collision grid | Colliders |
|---|---|---|---:|
| 01 | 05B | 20 x 20 | 29 |
| 05 | 066 | 16 x 16 | 19 |
| 08 | 06E | 24 x 44 | 56 |
| 10 | 080 | 22 x 22 | 15 |

Each area gets seeded random queries plus collider-center, exact-edge and just-outside probes, with
g_IgnoreCollider and g_CollWallProbe changed explicitly. Synthetic cases cover all 89 original shape codes,
the collider dispatch families, signed halfword and full word extremes, disabled surface queries, equal-height
shape/collider precedence, water-surface override, negative gradients, unsupported direct wall/slope types and
the signed division/normalization traps. Redundant probe-disabled and unsupported-type repetitions were
removed from the initial larger suite to keep the default run comfortably below one minute; instruction
coverage and all mutation detections are retained.

`.venv/Scripts/python.exe tools/mips/check_solid.py --mutate` builds **32 deliberate wrong C copies** under the
ignored build directory; **all 32 must be caught**. These change quadrant signedness, counter wrap increment,
box/strip/ring/wall boundaries, ramp division and direction, wedge rounding, arc/diagonal comparisons,
slope gradients, surface current shifts, collider heights and the top-level contact/precedence/ignore/32-unit
probe offset. An equivalent ring minimum-width mutation (64 -> 65, both divide to 32) was registered as
dropped and replaced with 64 -> 66. No non-equivalent mutation is dropped.

`--only <names...>` selects direct calls for incremental work; `--trapv` builds with GCC's signed-overflow
checks. Neither writes a default-run result. `--mutation-start` is only a diagnostic resumption option;
the complete mutation command starts at zero. The default writes `extract/checks/results/check_solid.json`.

Final checks: default differential run **5,576 cases / 31.8 s**, full `--trapv` run **5,576 cases, passed**,
complete mutation run **32 / 32 caught**, and `.venv/Scripts/python.exe tools/check_verified.py` **passed**
with zero errors. Its 14 warnings concern absent results for unrelated legacy checkers in this isolated worktree.

## Coverage proofs and caller policies

The exact ranges and proofs are appended to `tools/mips/check_exclusions.csv`:

- Strip division overflow: the denominator is twice an s16, so cannot be -1. Zero divisors are tested.
- Wedge division guards: both extents exceed 64 before any division; zero and -1 divisors are impossible.
- Trench overflow: denominator -1 requires radius 255; the logical-shift bound on SquareRoot0 rules out the
  sole numerator congruence that would yield INT_MIN.
- Slope angle/division guards: angles are normalized nonnegative; denominator 628 times the adjusted
  halfword sum cannot be zero or -1.
- ColliderHeightAt's half-space call: types 78..81 return earlier; only type 68 reaches the subsequent test,
  whose unsigned range check always skips this call.
- GetSolidHeight's negative rounding of a height: the preceding branch already established height >=512.

`AeSolidHooks` supplies only original undefined caller values; there are **no deferred original functions**:

| Fields | Original behavior requiring a host policy |
|---|---|
| stale_wall_axis | WallThin outside its central intersection with a type other than 3..6 or 42..45 reads the caller's undefined axis; WallEdge does so for unsupported types. |
| stale_edge_x/z | WallEdge with an unsupported type can also read the caller's undefined edge centers. |
| stale_initial_layer | GetSolidHeight may read the caller's untouched saved layer when y >32767; for smaller positive y it cannot affect the result. |
| stale_slope_corner_x/z, stale_slope_angle_low/high | Direct ColliderSlopeHeight calls with a non-slope type leave the corner/angle stack locals untouched. Normal collider dispatch only sends types 70..73. |

The checker seeds these original register/stack values and the matching policy fields through two registered
shared-random exclusions. The small optional `initial_regs` addition to the test-only MIPS harness provides
the caller registers without patching or skipping instructions. All four policy-consuming entry points have
explicit NULL-policy fail-closed checks. A NULL policy is otherwise usable on paths that consume no undefined
value. These are host choices for undefined original inputs, not replacement collision algorithms.

## Instruction findings

- VecLength3 takes three arguments, including the trench query's radial coordinate that older pseudocode
  omits. Its GTE inputs truncate to s16; it does not square arbitrary full-word coordinates.
- Collider records are **0x38 bytes**, with geometry at +10, surface type at +12, mode at +14 and disabled
  flags at +18. The three words must not be conflated as one "type".
- ColliderHeightAt's apparent half-space/ring fallthrough in the pseudocode is unreachable for the half-space
  types because of an earlier branch. The independent half-space helper still uses strict planes.
- Ramp2 leaves its output untouched when outside; Ramp4 clears it first. Box clears it even for degenerate
  extents. Wall and half-pipe helpers preserve some existing slope words rather than always zeroing both.
- Cylinder types 70..73 and 82..84 intentionally return no hit. Types 36/37 first try a half-pipe and, on
  failure, fall back to a disc rather than returning immediately.
- GetSolidHeight restarts the backward collider scan after each improvement and lowers its query y by 32.
  Equal shape/collider heights choose the shape, and a matching 400/500 water class can override the collider.
- Shape dispatch follows the actual RAM function pointer table, and the diagonal orientation flags are read
  from the original EXE table rather than invented from geometric assumptions.

Changes are left uncommitted in this worktree for owner review, including the new exclusion rows. No game
data, third-party code, Ghidra state, player/body/monkey modules or world behavior was changed.
