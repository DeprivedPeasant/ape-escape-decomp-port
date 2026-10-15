# Area 02 movement helpers

The nine requested EXE functions are hand-ported as structured C over `AeRam` in
[ae_area02.c](../../src/world/ae_area02.c), [ae_area02_paths.c](../../src/world/ae_area02_paths.c),
and [ae_area02.h](../../src/world/ae_area02.h). Their MIPS implementations are checked against the original EXE
with real area members 01, 02, 04, and 05 by [check_area02.py](../../tools/mips/check_area02.py).

## Call tree

```text
HalfPipeRangeTest
  <- CollProbe_Strip / Box / Radial and related collision probes

ObjMove
ZakoHooksAdd
ObjSetPosToPathPoint
ObjFacePathPoint
  -> VectorNormalSS (ported SDK math)
  -> Atan2Safe (ported helper)

ObjFollowPath
  -> ObjSeekPathPoint
       -> ObjSeekPoint
            -> FUN_800A3740 (named BIOS word-clear hook)
            -> ObjTurnTowardsXZ or ObjSetVelAlongDir (existing direct ports)
  -> ObjAdvancePath
```

`ObjSeekPoint`, `ObjSeekPathPoint`, and `ObjFollowPath` call the same seek logic but preserve each original stack
frame's BIOS clear destination (`0x801FFEE0`, `0x801FFEC0`, and `0x801FFEA0`). The stack window is excluded from the
RAM comparison because the C and MIPS call frames differ; BIOS hook arguments, RAM outside that window, scratchpad,
and return values are compared.

## Function status and coverage

All nine functions passed 32 differential cases each (288 comparisons total). The default run covered 363 of 367
instructions; the four unexecuted instructions have explicit proofs in `tools/mips/check_exclusions.csv`.

| Address | Function | Status | Original instruction coverage |
|---|---|---|---:|
| `80050490` | `HalfPipeRangeTest` | ported + diffed, 32 cases | 59/59 |
| `800554F0` | `ObjMove` | ported + diffed, 32 cases | 13/13 |
| `80055718` | `ObjSeekPoint` | ported + diffed, 32 cases | 93/93 |
| `80057588` | `ZakoHooksAdd` | ported + diffed, 32 cases | 33/33 |
| `80058400` | `ObjSeekPathPoint` | ported + diffed, 32 cases | 27/27 |
| `800584C8` | `ObjAdvancePath` | ported + diffed, 32 cases | 63/66; 3 unreachable |
| `800585D0` | `ObjFollowPath` | ported + diffed, 32 cases | 26/26 |
| `80058638` | `ObjSetPosToPathPoint` | ported + diffed, 32 cases | 13/13 |
| `8005866C` | `ObjFacePathPoint` | ported + diffed, 32 cases | 36/37; 1 unreachable |

The three `ObjAdvancePath` gaps are the divide-by-zero and signed-overflow trap guards: its divisor is loaded with
`lhu`, so it cannot be `-1`. `ObjFacePathPoint`'s negative-rounding branch is unreachable because `Atan2Safe` returns
in `-2048..2048`, making the value after adding 4096 nonnegative.

Mutation tests changed two observable operations in each function; all 18 mutations were caught. The path-seek
mutations alter the BIOS clear destination and are detected through the hook arguments.

## Caller integration

The 18C enemy update now calls the path and movement ports directly. Its caller checker passed 216 cases after its
fixture was given a valid two-point path and nonzero turn step. `HalfPipeRangeTest` is a direct call from collision
probes; `check_collprobe.py --coverage` passed 1,306 comparisons and covered 3,426/3,644 instructions with the
checker’s registered unreachable paths. The body-movement checker passed all 160 cases. The broader miscellaneous
checker passed 297 cases and now seeds an actual vertical overlap for its collision-ray scenarios, rather than
expecting the former mocked `HalfPipeRangeTest` result.

The collision-probe sweep also exposed a proven call-order mismatch in `CollProbe_Triangle`: MIPS calls
`PointInBoxXZ` for the current and previous positions before rejecting negative distance. The C order was corrected
and the caller checker now passes with that deeper path exercised.

No requested game function remains hooked. `FUN_800A3740` remains a named BIOS hook that clears eight bytes at the
original caller-specific destination; it is not a game function and is intentionally not ported.
