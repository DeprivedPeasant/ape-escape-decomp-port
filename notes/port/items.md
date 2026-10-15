# Area items and render-state setup

## Call map

The `AreaLoad` path in [stage.md](stage.md) reaches three entry points in this batch:

```text
AreaLoad
├─ ItemsLoadForArea
│  ├─ GetLevelFirstArea                 external metadata hook
│  ├─ Printf                            external diagnostic hook, overflow only
│  ├─ CollectedItemsHas
│  │  └─ GetCollectedItemsTable
│  └─ PickupSpawnSlot
│     ├─ ae_sdk_rcos                    previously ported SDK routine
│     └─ ae_sdk_rsin                    previously ported SDK routine
├─ ApplyLevelFog
│  └─ SetFarColor
│     └─ renderer_set_gte_far_color     native renderer boundary
└─ SetColorMatrixMode
   ├─ MatrixCopy8
   ├─ SetBackColor
   │  └─ renderer_set_gte_background_color
   └─ SetColorMatrix
      └─ renderer_set_gte_color_matrix
```

`GetLevelFirstArea` (`8009764C`) supplies the first area number for a level. `ItemsLoadForArea`
uses the current-area delta to select a bit in the eight-word area mask table. A delta outside
that range reports through `Printf` (`800A3720`) and falls back to mask word zero. The two SDK
trigonometry functions are already ported and verified elsewhere; they remain ordinary C calls.

The renderer callbacks stand in for the original GTE control-register loads. They are named
renderer operations, and the port does not implement GTE behavior. The checker records the
original CTC2 control writes and compares their register values with the callback state. The
other checker-visible boundaries are `GetLevelFirstArea` and the overflow-only `Printf` call.

## Port behavior

The item header pointer is stored in the stage state at `+0x248`; area members loaded at
`0x80154000` provide that pointer at member offset `0x4DC`. A valid header has the `.ITM` magic,
a 16-bit record count, and 16-byte records. Loading marks the header as consumed, clears the 32
per-record flags and the active byte of each pickup slot, then spawns records not masked out by
the current area. Kind 4 is collection-sensitive and checks the current area in the collected
items table. The table address is `0x800DFB70`; it has 128 two-byte-spaced entries whose low
bytes are searched up to the `0xFF` terminator.

`PickupSpawnSlot` handles explicit slots and the negative-slot first-free scan, including the
all-slots-full return. It preserves the original seed update order for the three launch modes,
the signed fixed-point trigonometry rounding, and mode-specific velocities. `ItemsLoadForArea`
also clamps the level item kind to 4 when the level table contains a value of 5 or more. Bad
header magic returns after clearing pickup activity and item flags, before writing the trailing
load-state globals.

`SetColorMatrixMode` implements modes 0 through 3, including saving/restoring the eight-word
matrix block and mode 1's dimmed first column when the light-mode field selects it. Other modes
leave the matrix and renderer state alone. Color and matrix values are sent to named native
renderer callbacks. `ApplyLevelFog` reads each area's real far color and fog values, sends the
far color through its callback, and copies the fog words into scratchpad while zeroing the two
following words.

## Validation

`tools/mips/check_items_init.py` builds the C port with `-Wall -Wextra -Werror`, runs each
original function from the EXE against the RAM-image implementation, compares RAM below the
bounded stack window, the full scratchpad, return values, external hook sequences, and recorded
GTE control state. Item-loading cases use real members for areas 1, 2, 4, 8, 9, 10, 18, and 33;
fog cases use real area data for areas 1, 4, and 10. Edge cases cover masks, the overflow path,
empty counts, the record sentinel, invalid magic, collection membership, level-kind clamping,
pickup search exhaustion, and all pickup modes.

The latest default run passed 51 cases and covered all 517 instructions across the ten target
functions (100% per function, no uncovered instructions). The mutation run tried three mutations
per function; all 30 were caught. The only registered comparison exclusion is
`stack_window,801FF000..80200000`, because the MIPS and host C call frames differ. All other RAM
below that range and the full scratchpad are compared.

| Address | Original function | C function | Default cases | Coverage |
|---|---|---|---:|---:|
| `80015D58` | `SetColorMatrixMode` | `ae_SetColorMatrixMode` | 7 | 100% |
| `80015F24` | `MatrixCopy8` | `ae_MatrixCopy8` | 2 | 100% |
| `800315E0` | `SetBackColor` | `ae_SetBackColor` | 3 | 100% |
| `80031600` | `SetColorMatrix` | `ae_SetColorMatrix` | 2 | 100% |
| `80031630` | `SetFarColor` | `ae_SetFarColor` | 3 | 100% |
| `800323F8` | `ApplyLevelFog` | `ae_ApplyLevelFog` | 3 | 100% |
| `80083470` | `ItemsLoadForArea` | `ae_ItemsLoadForArea` | 13 | 100% |
| `80083844` | `PickupSpawnSlot` | `ae_PickupSpawnSlot` | 10 | 100% |
| `80084904` | `CollectedItemsHas` | `ae_CollectedItemsHas` | 7 | 100% |
| `800962E8` | `GetCollectedItemsTable` | `ae_GetCollectedItemsTable` | 1 | 100% |

No functions in this batch remain unported. The worktree originally lacked this topic note;
this file records the call map and port findings for the batch. The full repository
`tools/check_verified.py` run sees all ten new rows and the fresh checker result, but exits
with five existing duplicate-`c_function` errors for the `GimmickSwing*` rows recorded against
both `src/world/ae_gimmick_a.c` and `src/world/ae_gimmick_b.c`. The task's append-only rule
prevents correcting those unrelated rows here. It also warns that result files for other
checkers are absent; those warnings do not involve this batch.
