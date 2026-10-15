# Area-load terrain and collider zones

## Call tree from the EXE listings

Calls below come from `tools/mips/mips_ram.py` listings for the target address ranges. Calls whose target is in this
task are direct C calls; the other callees remain named host hooks.

```text
BuildTerrainBlockMask (800438F8)
└─ AreaBumpAlloc (8008C1B8) [hook]

DebugMenuInitPlayerParams (8005CB1C)
├─ MenuItemInitBound (8007D1E0) [18 times]
│  └─ ReadSizedValue (8007D118)
├─ MenuItemSetCallback (8007D29C)
└─ DebugMenuInitFlags (8005CA34)
   ├─ MenuItemInitBound (8007D1E0)
   │  └─ ReadSizedValue (8007D118)
   └─ MenuItemSetCallback (8007D29C)

DebugMenuInitCounters (8005CC58)
├─ MenuItemInitBound (8007D1E0) [5 times]
│  └─ ReadSizedValue (8007D118)
└─ MenuItemSetCallback (8007D29C) [5 times]

DebugMenuInitCounters2 (8005CCDC)
└─ MenuItemInitBound (8007D1E0) [6 times]
   └─ ReadSizedValue (8007D118)

ColliderZoneMasksUpdate (80088670)
├─ ColliderComputeQuadrantMask (800889EC) [for enabled non-water colliders]
│  └─ GetColliderQuadrantMaskXZ (800889B4) [4 corners, hook]
├─ SvecToDiagMatrix (800147D0) [collider type 12]
└─ FUN_8013fc00 (8013FC00) [collider type 13, runtime overlay hook]
```

`SvecToDiagMatrix` (800147D0), `ReadSizedValue` (8007D118), and `MenuItemSetCallback` (8007D29C) are leaves. The
one-byte `FUN_8013fc00` target is at `8013FC00`, beyond the loaded EXE text (`80010000..800B4FFF`); its call is
retained as a named hook because its implementation is not in the EXE image. `AreaBumpAlloc` and
`GetColliderQuadrantMaskXZ` are outside the exact function table and remain hooks.

## Port notes

* `BuildTerrainBlockMask` allocates `(grid width >> 1) * (grid depth >> 1)` bytes. Each output byte represents a 2x2
  block; it is `0xFF` only when bit 0 is clear in all four source cell words.
* `ColliderComputeQuadrantMask` narrows each center-plus/minus-half-size coordinate to signed 16-bit before asking
  `GetColliderQuadrantMaskXZ` for the four corner masks, then returns the sign-extended low 16 bits of their OR.
* Debug menu routines initialize the menu item records and globals in RAM, and store the original callback addresses.
  `AreaLoad`'s three calls stay named `DebugMenuInitPlayerParams`, `DebugMenuInitCounters`, and
  `DebugMenuInitCounters2` host hooks; a host can bind those hooks to the new `ae_` functions. The item-bound and
  callback setters are ported directly.
* `ColliderZoneMasksUpdate` walks collider records backwards. It refreshes zone masks, mirrors linked collider
  positions into `AreaProp`, and applies the original type-specific field changes. Type 2 doubles the three flow
  components and clears its pad; type 13 calls the `FUN_8013fc00` overlay hook and stores the collider index. A
  disabled collider gets zone mask 0; enabled types 0 and 9 compute the four-corner mask; other enabled nonzero
  types get `0x1111`.

## Validation

`tools/mips/check_zones.py` ran on the EXE and real area members for areas `01` (`05B`), `02` (`05E`), `04` (`064`),
and `05` (`066`), plus directed and randomized edge cases. The default run completed 62 cases in under three seconds with no
return, RAM, scratchpad, or hook-event mismatches.

| Function | Cases | Result | Original instruction coverage |
|---|---:|---|---:|
| SvecToDiagMatrix | 4 | diffed | 13/13 |
| BuildTerrainBlockMask | 7 | diffed | 101/101 |
| DebugMenuInitFlags | 3 | diffed | 20/20 |
| DebugMenuInitPlayerParams | 3 | diffed | 32/32 |
| DebugMenuInitCounters | 3 | diffed | 33/33 |
| DebugMenuInitCounters2 | 3 | diffed | 27/27 |
| ReadSizedValue | 7 | diffed | 28/28 |
| MenuItemInitBound | 6 | diffed | 23/23 |
| MenuItemSetCallback | 4 | diffed | 2/2 |
| ColliderZoneMasksUpdate | 14 | diffed | 205/205 |
| ColliderComputeQuadrantMask | 4 | diffed | 51/51 |
| FUN_8013fc00 | 4 | hook shim | 0/1; target unavailable in EXE |

Mutation mode ran 36 distinct source mutations, three for each table entry; all 36 were caught. This includes mutations
to the `FUN_8013fc00` hook forwarding, whose exact argument is checked against the original caller's hook event.

The only open function from the target table is `FUN_8013fc00` (one-byte target in the task table at `8013FC00`, absent from the main EXE image).
It is called for collider type 13 with the collider index; its area/overlay implementation is not present in the EXE
instructions available to this checker. `AreaBumpAlloc` and `GetColliderQuadrantMaskXZ` are named hooks for functions
outside the requested table. The three debug-menu setup entry points remain the existing named `AreaLoad` hooks and
are implemented by this module for a host to bind.

`tools/check_verified.py --results-only` passes the zone ledger rows. The full static check currently exits nonzero on
five duplicate ledger entries for `ae_GimmickSwingCarryPoint`, `ae_GimmickSwingAngle`, `ae_GimmickSwingFirstStep`,
`ae_GimmickSwingAdvance`, and `ae_GimmickSwingProbeWall`, each listed once from `ae_gimmick_a.c` and once from
`ae_gimmick_b.c`; those existing rows are outside this task's append-only ledger scope.

Instruction findings confirmed during diffing: `ReadSizedValue` uses signed `lb`/`lh` for sizes 0/1, `lw` for 2,
`lbu` for 3, and returns 1 or 3 for other sizes; `MenuItemInitBound` returns 1 after filling the item; terrain block
row words are one width apart; collider types 0 and 9 use corner masks while other enabled nonzero types receive
`0x1111`; collider type 5 divides the shape product by 360 and uses the original signed multiply sequence for its
derived dimensions divided by 5.
