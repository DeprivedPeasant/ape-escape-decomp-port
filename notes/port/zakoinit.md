# Zako initialization port

`src/world/ae_zako_init.c` ports the 18 AreaLoad enemy-initialization functions in
the task table. They operate on `AeRam`, preserving the original globals and
object records. Direct calls to already-ported collision body and target-list
functions stay native; unresolved formatting and menu helpers are named hooks.

## Call tree from the instruction listings

```text
ZakoInit
├─ ZakoHooksReset
├─ ZakoDebugMenuInit
│  ├─ MenuItemInitToggle
│  │  └─ ReadSizedValue [hook]
│  ├─ MenuItemInitBound [hook]
│  └─ MenuItemSetCallback [hook]
├─ ZakoTypeIsIgnored
├─ ZakoSpawnFromEntry
│  ├─ ObjSetPathFromTable [when the entry path index is not -1]
│  ├─ ZakoSetupType
│  │  ├─ ZakoSetAnimFromEntry
│  │  │  └─ ZakoGetAnimTable
│  │  │     └─ ZakoFindEntryByType
│  │  ├─ ObjInitModel
│  │  │  ├─ ZakoFindEntryByType
│  │  │  └─ ModelInitAuto
│  │  ├─ CollisionBodyRegister [existing port]
│  │  └─ TargetListAdd [existing port, for the selected types]
│  └─ ZakoSetAreaMode
│     └─ ObjKill [type 0x6f in area 0x4c]
│        └─ ObjUnregisterBody
│           ├─ CollisionBodyUnregister [existing port]
│           └─ TargetListRemove [existing port]
├─ GetLevelFirstArea
├─ Printf [hook, only when the area index is outside the eight-word table]
├─ ObjKill [for defeated active entries]
│  └─ ObjUnregisterBody
│     ├─ CollisionBodyUnregister [existing port]
│     └─ TargetListRemove [existing port]
└─ ZakoClearSlotTable [special area kind 13]
```

The root sets sight-grid limits, resets the enemy state and 16 object slots,
registers the debug menu, binds the area's `.ZED` tables, spawns eligible types,
then applies the defeated-entry mask for the current area. `ZakoInit` reads
real area members at `0x80154000`; the checker exercises areas 0x01, 0x02, 0x03,
and 0x06. It also tests an invalid `.ZED` pointer, special area kind 13, an
overflowing area index, grid-limit boundaries, and 36 deliberate mutations
(two per function), all caught.

## Validation

Checker: `tools/mips/check_zako_init.py`. The default run compares all main RAM
below the declared top stack window, scratchpad, returns, and hook sequences
against the original instruction execution. It passes 286 differential cases;
all 999 instructions across the 18 functions are reached.

| Function | Address | Cases | Instruction coverage |
|---|---:|---:|---:|
| `ModelInitAuto` | `80015BA0` | 3 | 37/37 |
| `ZakoTypeIsIgnored` | `80050580` | 17 | 18/18 |
| `ZakoInit` | `800505C8` | 9 | 188/188 |
| `ZakoSpawnFromEntry` | `800508B8` | 4 | 59/59 |
| `ZakoClearSlotTable` | `800509A4` | 1 | 10/10 |
| `ZakoSetupType` | `80050D04` | 184 | 296/296 |
| `ObjUnregisterBody` | `80051E34` | 4 | 25/25 |
| `ObjKill` | `80051E98` | 4 | 33/33 |
| `ZakoFindEntryByType` | `80051FE8` | 6 | 20/20 |
| `ObjInitModel` | `80052038` | 5 | 28/28 |
| `ZakoGetAnimTable` | `800520A8` | 5 | 35/35 |
| `ZakoSetAnimFromEntry` | `80052134` | 5 | 14/14 |
| `ZakoHooksReset` | `80057558` | 1 | 12/12 |
| `ZakoSetAreaMode` | `80057FD0` | 12 | 89/89 |
| `ObjSetPathFromTable` | `800583BC` | 3 | 17/17 |
| `ZakoDebugMenuInit` | `800587EC` | 1 | 90/90 |
| `MenuItemInitToggle` | `8007D23C` | 16 | 21/21 |
| `GetLevelFirstArea` | `8009764C` | 6 | 7/7 |

The sole checker exclusion is the standard top stack window
(`801FF000..80200000`); all other main RAM and all scratchpad bytes are
compared.

## Hooks retained

* `Printf`: prints the sight-index overflow diagnostic. `ZakoInit` ignores its
  return value.
* `MenuItemInitBound`: initializes a bounded debug-menu value record and reads
  its initial value.
* `MenuItemSetCallback`: installs the debug-menu callback on item 7.
* `ReadSizedValue`: reads a signed byte, signed halfword, signed word, or
  unsigned byte according to the menu item's size selector.

The checker applies each helper's observed RAM effects on both sides and also
compares its arguments and call order. These helper functions remain outside
this batch's port.

## Instruction findings

* `ZakoTypeIsIgnored` excludes exact types `0x61`, `0x63`, `0x6d`, `0x6e`, and
  `0x84`, plus the inclusive range `0x72..0xb5`.
* `ZakoSetupType` reaches `TargetListAdd` only inside the existing body-active
  registration block and when the target-list flag is set.
* `ZakoSetAreaMode` type `0x6f` kills its object only in area `0x4c`; elsewhere
  the area-mode flag remains set.
* `ZakoGetAnimTable` stores the pointer after the animation clips in
  `Obj+0x18`, but returns the clip-table start at header `+4`. The machine code
  leaves that earlier pointer in `$v0`.
* `ZakoSetAnimFromEntry` consumes the type in `$a1` even though the old symbol
  prototype listed only the object. The port makes this effective argument
  explicit.
* The debug-menu listing skips item records 2 and 11, reuses record 7 for its
  callback, and ends at record 15. The port follows those actual helper-call
  addresses and variable offsets.
