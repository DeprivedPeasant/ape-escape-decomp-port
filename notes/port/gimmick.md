# Gimmick activation port

## Instruction call tree

`GimmickActivate` (`0x8008FC30`, 1,536 bytes) dispatches by gimmick type. Its instruction listing shows these direct calls:

| Callee | Address | Path |
| --- | --- | --- |
| `GimmickActivate` | `0x8008FC30` | Recursive activation of linked gimmicks |
| `GimmickDeactivateChain` | `0x80090230` | Type `0x0A`; recursively follows the three link fields |
| `SwingDoorSetPivot` | `0x8008ECFC` | Hinged door type `0x65` |
| `PropSetMesh` | `0x80090E54` | Replaces a prop mesh for the door and mesh gimmick paths |
| `PlaySfx` | `0x800877F8` | Sound effect without a position |
| `PlaySfxAt` | `0x80087A20` | Sound effect at a collider position |
| `FUN_801414a0` | `0x801414A0` | Type `0x29` overlay callback |

The rendering subtree beneath `PropSetMesh` is `MeshInitFromAreaTable` (`0x80015CC8`) then `MeshInit` (`0x80015C34`). It remains on the renderer side of the boundary; the port hooks `PropSetMesh` by its original name. The sound routines and their callees remain sound hooks, and the overlay routine remains an overlay hook. No rendering, sound or overlay instructions are included in this checker's coverage target.

`GimmickDeactivateChain` (`0x80090230`, 320 bytes) has three recursive calls to itself, one for each of `Gimmick` fields `+0x0E`, `+0x10` and `+0x12`. `SwingDoorSetPivot` (`0x8008ECFC`, 612 bytes) has no callees.

## Ported behavior and listing findings

- `GimmickActivate` uses a structured `switch` for the type table, named RAM globals and named record offsets. Unsupported table entries return without side effects. Recursive calls retain the original source pointer and the original signed-halfword index behavior.
- Type `0x03` and type `0x14` change state `0` to `1` and state `2` to `3`; state `1` returns unchanged. The checker distinguishes that from a `1` to `3` transition.
- Type `0x06` sets state `1`, sets the linked prop zone mask when a prop exists, and applies the common collider low-nibble state update. Type `0x0A` is the type that invokes `GimmickDeactivateChain`.
- Type `0x16` updates the prop zone mask and collider state bits; its positional sound branch is selected when level is `6` or area is `0x65`.
- Type `0x1B` accepts either a state-zero type-`0x02` source or a type-`0x1B` source. Type `0x28` instead requires both source and target state zero; it does not test the source type.
- Type `0x65` has distinct state and direction paths. `SwingDoorSetPivot` narrows direction and mode to signed halfwords, selects an eight-entry jump table, uses the mode-`2` formulas only for mode `2`, and swaps the collider's X/Z half extents on every call. The prop-mask write for a direction outside `0..7` belongs to the state-`1` path, not the state-`5` path.
- The deactivation helper sets collider low state bits to `2`, sets the prop zone mask when the prop index is nonnegative, recursively processes links in field order, then saves collider position into the three link fields.

These details were taken from the original instruction listings and confirmed by the RAM differential checks.

## Hook boundary

| Hook | Original address | Responsibility kept outside this port |
| --- | --- | --- |
| `PlaySfx` | `0x800877F8` | Sound playback without positional input |
| `PlaySfxAt` | `0x80087A20` | Positional sound playback; checker records the collider position passed to it |
| `PropSetMesh` | `0x80090E54` | Renderer-owned prop mesh replacement, including the `MeshInitFromAreaTable` → `MeshInit` subtree |
| `FUN_801414a0` | `0x801414A0` | Area overlay callback for type `0x29` |

The checker uses deterministic hook models for both executions, compares hook arguments and call order, and verifies that a missing hook fails closed before later RAM writes.

## Validation

`tools/mips/check_gimmick.py --coverage --mutate` passes at the default scale:

| Original function | Cases | Original instruction coverage |
| --- | ---: | ---: |
| `GimmickActivate` | 315 | 384 / 384 |
| `GimmickDeactivateChain` | 4 | 80 / 80 |
| `SwingDoorSetPivot` | 22 | 153 / 153 |
| **Total** | **341** | **617 / 617** |

The checker includes type and state edges, linked chains, door directions, and all gimmicks in real area tables for areas `0x0E`, `0x29`, `0x41`, `0x52` and `0x66` (members `0x07B`, `0x0B8`, `0x0EB`, `0x114` and `0x132`). It has 61 function-scoped mutations: 41 activation, 10 deactivation and 10 pivot mutations; all 61 are caught.

The only declared exclusion is the original/host stack window `0x801FF000..0x80200000`, because the C and MIPS frames differ. RAM outside that range and scratchpad bytes are compared. There are no uncovered instructions or coverage exclusions for the three ported functions.
