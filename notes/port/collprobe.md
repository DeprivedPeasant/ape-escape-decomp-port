# Collision probe ports

This batch ports the player-body collision probe handlers from the SCUS-94423 EXE to the `AeRam` image. The original
instruction listings and exact function sizes are in `notes/inventory/functions.csv`; the call edges below are from
`notes/inventory/calls.csv` and the EXE dispatch words at `0x800B20E0` / `0x800B2108`.

## Dispatch call tree

The grid-probe table at `0x800B20E0` has these entries by `(flags & 0xF000) >> 12`:

| Index | Function |
|---:|---|
| 0 | `CollProbe_Box` |
| 1 | `CollProbe_ThinWall` |
| 2 | `CollProbe_Ramp` |
| 3 | `CollProbe_Radial` |
| 4 | `CollProbe_Triangle` |
| 5 | `CollProbe_ThinWall` |
| 6, 7 | `CollProbe_EdgeWall` |
| 8 | `CollProbe_DiagonalWall` |
| 9 | `CollProbe_Radial` |

The collider table at `0x800B2108` dispatches indices 0..9 to `Box`, `Strip`, `Ramp`, `Radial`, `Triangle`, four
`NoContact` entries, and `Radial`, respectively. `CollProbe_Radial` then dispatches geometry types 36/37 to
`HalfPipe`, 70..73 to `Slope`, 83/84 to `Trench`, and rope geometry to `Rope`.

The direct call tree within the batch is:

```text
Box, Ramp, Triangle, Radial, HalfPipe, Trench, ThinWall, EdgeWall,
DiagonalWall, Slope, Rope, Strip, NoContact
  ├─ shared helpers in this batch: Radial -> HalfPipe / Trench / Slope / Rope; Trench -> HalfPipe
  ├─ already ported RAM-image helpers in src/world / src/core: shape queries and SquareRoot0
  └─ remaining external hooks: PointInBoxXZ, HalfPipeRangeTest, ColliderSlopeContactDir,
     DirIndexFromFlags, PlayerSetClimbTarget, PlayerSetRope, and printf
```

The `src/collision/native_collision.*` implementation is an older host-side module and is not part of these RAM-image
ports. The functions here use named offsets from `src/bodies/ae_probe.h` / `src/world/ae_solid.h` and named globals in
`ae_collprobe.h`.

## Validation

The new `tools/mips/check_collprobe.py` runs each original function against its `ae_*` port on the same EXE and real
area-member RAM image, compares return values, all main RAM below the declared stack window, scratchpad and hook calls,
and reports original instruction coverage. The default differential run checks 100 cases per function, except Rope, for
which 102 cases cover the directed horizontal and diagonal segment edges. It passed all 1,302 cases. Instruction coverage
is 3,426/3,644; every remaining instruction has a proof registered in `tools/mips/check_exclusions.csv`. Forty deliberate
source mutations were all caught. The per-function case counts are recorded in `tools/verified.csv`, and coverage results
are in `extract/checks/results/check_collprobe.json`. The six ledger rows that existed before the final mutation run remain
unchanged under the append-only rule; their `2/2` mutation notes describe the earlier subset, while the table below records
the final per-function mutation counts.

## Function status

| Function | Status | Default cases | Coverage / findings |
|---|---|---:|---|
| `CollProbe_NoContact` | diffed | 100 | 10/10 instructions; 4/4 mutations caught. Logs the 32-bit word at `probe + 4` through the `printf` hook and returns zero. |
| `CollProbe_Strip` | diffed | 100 | 81/81 instructions; 4/4 mutations caught. Collider strip height plus vertical overlap; type selects x/z response axis, which the first checker run exposed as a return-status distinction. |
| `CollProbe_ThinWall` | diffed | 100 | 153/153 instructions; 4/4 mutations caught. Handles 0x1000 and 0x5000 bands, including caller `$s1` state used by unsupported orientation words. |
| `CollProbe_EdgeWall` | diffed | 100 | 183/183 instructions; 4/4 mutations caught. Handles 0x6000 / 0x7000 edge and corner centers. |
| `CollProbe_Box` | diffed | 100 | 251/259 instructions; 8 divide guards proved unreachable; 4/4 mutations caught. Shifts the shape center in RAM for types 60-63, then computes the selected horizontal push and conditional vertical response. |
| `CollProbe_DiagonalWall` | diffed | 100 | 336/400 instructions; 64 divide guards proved unreachable; 4/4 mutations caught. The invalid orientation path returns 1 after the distance gate and leaves output untouched. |
| `CollProbe_Radial` | diffed | 100 | 637/669 instructions; 32 divide guards proved unreachable; 2/2 mutations caught. Dispatches collider geometry to the half-pipe, slope, trench, and rope handlers. |
| `CollProbe_HalfPipe` | diffed | 100 | 252/260 instructions; 8 divide guards proved unreachable; 3/3 mutations caught. Handles the half-pipe profile and vertical range gate. |
| `CollProbe_Slope` | diffed | 100 | 204/238 instructions; 34 proven-unreachable instructions; 2/2 mutations caught. Computes the slope contact and response, including the radial band checks. |
| `CollProbe_Ramp` | diffed | 100 | 297/306 instructions; 8 divide guards and one dead jump fallthrough proved unreachable; 2/2 mutations caught. Handles the ramp orientation sectors and boundary responses. |
| `CollProbe_Trench` | diffed | 100 | 322/322 instructions; 2/2 mutations caught. Handles trench geometry through the shared half-pipe range path. |
| `CollProbe_Triangle` | diffed | 100 | 393/456 instructions; 63 proven-unreachable instructions; 3/3 mutations caught. Selects a triangle side response from orientation and signed diagonal distance. |
| `CollProbe_Rope` | diffed | 102 | 307/307 instructions; 2/2 mutations caught. Handles horizontal and diagonal rope-segment cases. |

Differential testing exposed an important slope-corner tie: when a corner coordinate equals the probe coordinate, the
original takes the outward `+1` correction. The C port preserves this with a `>=` comparison on each axis. The Strip
handler also retains the distinct return status for its selected x/z response axis.

## Remaining hooks

These originals are called through the batch hook table and remain outside the port. Their call order and arguments are
checked; `ColliderSlopeContactDir` also writes the contact point used by the caller. `PlayerSetClimbTarget` and
`PlayerSetRope` remain hook calls because their original player-state side effects are outside this batch.

| Hooked original | Use in this batch |
|---|---|
| `PointInBoxXZ` | Tests whether a position lies inside an inclusive X/Z box. |
| `HalfPipeRangeTest` | Tests overlap between the half-pipe top height and a vertical range. |
| `ColliderSlopeContactDir` | Tentatively computes contact direction against a slope collider and writes contact coordinates (symbol description is marked tentative). |
| `DirIndexFromFlags` | Extracts direction index bits 10-11 from flags. |
| `PlayerSetClimbTarget` | Sets `climbTarget` and attachment flag bit 1. |
| `PlayerSetRope` | Sets rope endpoints and attachment flag bit 2. |
| `printf` | Emits the NoContact debug record. |
