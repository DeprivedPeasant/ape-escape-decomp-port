# Model pose, animation, and bone matrix ports

Status: all ten requested functions are hand-ported and pass the differential checker. The C is in
`src/core/ae_pose.c` and `src/core/ae_sdk_matrix.c`; the checker is `tools/mips/check_pose.py`.

## Verified functions

| Original function | C function | Default cases | Result |
|---|---|---:|---|
| `MatrixIdentity` | `ae_sdk_MatrixIdentity` | 12 | pass |
| `MatrixFromDirXZ` | `ae_sdk_MatrixFromDirXZ` | 72 | pass |
| `MatrixLookDir` | `ae_sdk_MatrixLookDir` | 160 | pass |
| `ActorAnimDecode` | `ae_ActorAnimDecode` | 100 | pass |
| `AnimDecodeLerp` | `ae_AnimDecodeLerp` | 180 | pass |
| `ModelBuildBoneMatrices` | `ae_ModelBuildBoneMatrices` | 100 | pass |
| `ModelPoseBones` | `ae_ModelPoseBones` | 140 | pass |
| `RotMatrixZYX (SDK)` | `ae_sdk_RotMatrixZYX` | 180 | pass |
| `CompMatrix (SDK)` | `ae_sdk_CompMatrix` | 220 | pass |
| `ScaleMatrix (SDK)` | `ae_sdk_ScaleMatrix` | 120 | pass |

The checker executes the original SCUS_944.23 instructions and the C ports on matching RAM and scratchpad images. It
compares the complete initialized RAM image below the stack window, the scratchpad, return values, and MIPS traps against
the C error flag. The real 05B area member supplies the 17-bone monkey model and fixed-frame animation data, plus the
feature 0x10 model and channel stream. Synthetic streams add zero intervals, before-range frames, channel combinations,
and interpolation boundaries. Matrix cases include aliases, signed extremes, angle boundaries, and negative scales.

## Instruction coverage

| Function / helper range | Executed | Declared unreachable |
|---|---:|---:|
| `MatrixIdentity` | 10/10 | — |
| `MatrixFromDirXZ` | 18/18 | — |
| `MatrixLookDir` | 140/142 | 80014928, 800149D8 |
| `ActorAnimDecode` | 40/47 | 80014E7C..80014E84, 80014EA4, 80014EB4..80014EBC |
| `AnimDecodeLerp` | 168/168 | — |
| `ModelBuildBoneMatrices` | 159/159 | — |
| `ModelPoseBones` | 208/208 | — |
| `RotMatrixZYX (SDK)` | 25/25 | — |
| `RotMatrixZYX` tail blocks | 138/138 | — |
| `CompMatrix (SDK)` | 88/88 | — |
| `ScaleMatrix (SDK)` | 78/78 | — |
| `VectorNormalSS` wrapper and core | 12/12, 48/48 | — |

Overall, 1132/1141 listed instructions execute. The nine remaining PCs have proof entries in
`tools/mips/check_exclusions.csv`. MatrixLookDir's first skipped rounding branch tests a component that its basis selection
makes identically zero. Its second skipped branch tests the Y component of `dir × normalized(basis × dir)`, which is
nonnegative because normalization preserves component signs. ActorAnimDecode's skipped overflow guards compare a
zero-extended halfword interval against `0xFFFFFFFF`; the second divide-by-zero break is preceded by the first divide's
break for the same zero interval.

The only comparison exclusion is `801FF000..80200000`: the original and C use different stack frames. The checker
initializes every stack vector input consumed by the C ports, and compares the full non-stack RAM image.

## Mutation checks

`check_pose.py --mutate` runs 26 source mutations across the ten functions. All 26 were detected by at least one
differential case. The fixtures include a right translation whose upper bits differ from its signed low half, a negative
animation remainder at -4095, and an exact -2048 interpolation tie. No mutation was dropped as equivalent or dead.

## Findings recorded while matching the instructions

- `ModelPoseBones` copies the actor root into `g_BoneWorldMatrices` before applying the view matrix. It always composes
  each local bone with its local parent; flag bit 1 additionally writes the world-space child matrix.
- `ModelBuildBoneMatrices` uses flag `0x2` for world matrices, `0x1` for view matrices, and `0x4` for additive root
  translation. Parent indices address matrix slots directly, with the root at slot zero.
- `ActorAnimDecode` passes the signed remainder of its 12-bit fixed-point division into `AnimDecodeLerp`. Frames before
  the first key can therefore use a negative fraction; masking it to 12 bits changes the result.
- `CompMatrix` follows GTE fixed-point shifts and IR saturation for the rotation, takes the signed low half of each
  right-hand translation, and uses MIPS overflow behavior for the final translation additions.
- `AnimDecodeLerp` keeps the packed animation's X/Z/Y component ordering when writing the actor root bone record and
  returns the interpolated root Y value.

The exact per-function case counts are also appended to `tools/verified.csv`. Run
`.venv/Scripts/python.exe tools/mips/check_pose.py --coverage` for the default differential run and
`.venv/Scripts/python.exe tools/mips/check_pose.py --mutate` for the mutation suite.
