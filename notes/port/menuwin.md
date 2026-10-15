# Menu-window port

This batch ports the four menu-window functions to the RAM-image API in `src/ui/ae_menuwin.{h,c}`. The port is readable C with named window, item, input, render-context, and local stack fields. Calls to already-ported functions go directly through `src/**`; the remaining host and renderer boundaries use named hooks in `AeMenuWinHooks`.

## Call tree

```text
MenuWindowAdd (8007D2D8)
?? MenuStrLen (8007D2AC) [direct port]

MenuWindowsRun (8007D530)
?? PadUpdate (8001325C) [direct port]
?? StageRequestUpdate (8005D890) [direct port]
?? ColorFadeSet (8008C3C0) [direct port]
?? DebugTextPrintf (8008C6F0) [direct port]
?? ReadSizedValue (8007D118) [direct port]
?? WriteSizedValue (8007D188) [direct port]
?? PlaySfx (800877F8) [direct port]
?? StageRequest (8005D100) [direct port]
?? MenuWindowAdd (8007D2D8) [direct port]
?? RotMatrixZYX (8002FCC0) [direct pure-math LIBGTE port]
?? ScaleMatrix (80030E60) [direct pure-math LIBGTE port]
?? BeginFrame (8005DF48) [native frame hook]
?? ColorFadeReset (8008C390) [unported game-helper hook]
?? OtLinkSkipEmpty (8001F3A0) [native ordering-table hook]
?? ModelTransformRigid (800161C0) [renderer hook]
?? ModelDraw (80017CD4) [renderer hook]
?? SetDrawMove / DrawPrim / SetDrawArea / SetDrawTPage [PsyQ hooks]
?? DrawSync / VSync / PutDrawEnv / PutDispEnv / DrawOTag [PsyQ / BIOS hooks]
?? DrawMenuChrome / DrawMenuRows / DrawMenuHighlight [inline GPU hooks]
?? DrawMenuItem / DrawMenuSelection / DrawMenuCursor [inline GPU hooks]
?? (*item.callback)(item or value) [dynamic item-callback hook]
```

The indirect item callback target comes from item record `+0x0C`; the hook receives the original address and the original call argument. The initializer's `0x8007D110` callback is a no-op. Other menu builders can store their own callback target.

Each native hook field uses a C function-pointer type with the callee's real argument count. The checker records the same count per hook and ignores the caller's unused `$a` registers on the MIPS side; those comparisons are registered in `tools/mips/check_exclusions.csv`.

## Status and differential results

| Original function | C function | Default cases | Instruction coverage |
|---|---|---:|---:|
| `WriteSizedValue` `8007D188` | `ae_WriteSizedValue` | 320 | 22/22 |
| `MenuStrLen` `8007D2AC` | `ae_MenuStrLen` | 240 | 11/11 |
| `MenuWindowAdd` `8007D2D8` | `ae_MenuWindowAdd` | 360 | 149/150; one declared unreachable |
| `MenuWindowsRun` `8007D530` | `ae_MenuWindowsRun` | 85 | 1957/1972; 15 declared unreachable |

The default checker uses real area members for areas 01, 02, 04, 05, and 07, plus deterministic randomized and edge-case state. The final default run was 1,005 differential cases with zero mismatches. The eight registered inline-renderer instruction ranges all reached 100% coverage.

Mutation checking tried 28 wrong copies and caught every one: six mutations each for `WriteSizedValue`, `MenuStrLen`, and `MenuWindowAdd`, plus ten for `MenuWindowsRun`.

## Renderer and native hooks

The hook arguments retain the state values computed by the menu logic:

| Hook | Original operation represented |
|---|---|
| `BeginFrame` | Starts a native ordering-table frame. |
| `OtLinkSkipEmpty` | Links the frame ordering table while skipping empty buckets. |
| `ColorFadeReset` | Resets the menu's game-managed color-fade state. |
| `SetDrawMove`, `DrawPrim`, `SetDrawArea`, `SetDrawTPage` | Updates primitive coordinates, submits a primitive, and selects drawing-area and texture-page state. |
| `DrawSync`, `VSync`, `PutDrawEnv`, `PutDispEnv`, `DrawOTag` | Waits for GPU/frame completion, installs draw/display environments, and submits the ordering table. |
| `ModelTransformRigid`, `ModelDraw` | Positions and draws the animated menu cursor model. |
| `DrawMenuChrome` `D71C..D9EF` | Builds menu frame and panel packets. Receives the window, items, window index, and render context. |
| `DrawMenuRows` `D9F0..DF63` | Builds the item-row packets. Receives the window, items, item count, and render context. |
| `DrawMenuHighlight` `E030..E12F` | Builds the selected-row highlight packets. |
| `DrawMenuItem` `E1B8..E283` | Builds a menu item's inline packet after its text/value formatting. |
| `DrawMenuSelection` `E3F8..E5D7` | Builds the current selection marker packets. |
| `DrawMenuCursor` `F148..F2DF` | Builds the animated cursor packet. Receives the active primitive pointer, selected row, and cursor phase. |
| `item_callback` | Dispatches the item-specific callback address with the argument from that call site. |

`tools/mips/check_menuwin.py` records and exercises the packet-building instruction ranges separately: `D71C..D9A7`, `D9F0..DB63`, `DB28..DD23`, `DD24..DF63`, `E030..E12F`, `E1B8..E283`, `E3F8..E5D7`, and `F148..F2DF`. Their packet bytes are in the renderer RAM mask; menu records, state outside that range, hook calls, and scratchpad remain compared.

## Instruction-derived findings

* `MenuStrLen` counts the terminating NUL because the loop increments its pointer in the branch delay slot, including on the terminating iteration. A leading backtick advances the start pointer by two bytes; the returned length is the remaining text plus one.
* `WriteSizedValue` returns the value left in `$v0`, including the branch-delay-slot addition on the 32-bit write path. Sizes 0 and 1 return 1; sizes 2 and 3 return 3; sizes below 2 return 1 and sizes above 2 return 3. Size 0 writes a byte, size 1 a halfword, size 2 a word, and size 3 a byte.
* `MenuWindowAdd` registers at most six `0x2C`-byte windows at `800F5270`; the count and active-window globals are `800BEF60` and `800BEF62`. Each `0x24`-byte item definition is scanned to a zero flag word. The first item with flag `0x8000` is selected, and flags `0x6000` add five characters to the measured label width.
* The static registration and runtime views reuse the window record bytes: the draw colors become open/animation flags, and the selected-item byte becomes the runtime active-item index. The port preserves the byte writes and signed reads at those shared offsets.
* Held direction repeats decrement `800BEF64`. When the timer expires the menu plays SFX 1; a newly changed direction resets it to 20 frames, and a held repeat resets it to 3. Value items use the same repeat gate.
* A `C005` item enters the deferred-close path on its action input. A separate changed-button bit writes the item's stored value to the external variable. Its repeat-driven adjustment path invokes the item callback. `C008` invokes its callback on the action path and does not take the `C005` save path.
* A non-null `8002` item creates one child window and invokes its item callback with argument zero. `8002` and `8003` items with null variables play SFX 36 followed by SFX 33; the directed runner cases assert both MIPS calls.
* The `8003` submenu path creates two child windows. The second `MenuWindowAdd` result is stored in the first child's display byte; the code then reads the displayed child's first item value to initialize the second child. This follows the MIPS writes even though that field use is unusual.
* The `8002` path stores the child item-list pointer in the definition record before calling `MenuWindowAdd`. Unknown item types above `8004` enter deferred close without invoking an item callback.
* `MenuWindowAdd` takes only the definition pointer. Its listing at `8007D2F8..8007D344` loads the item-list pointer from the definition's `+4` field before using `$a3` as a local, so the child list set in that record is its actual input and the caller's `$a3` residue is ignored.
* `BeginFrame` and `ColorFadeReset` have zero arguments. `DrawPrim`, ordering-table, synchronization, environment, and OT hooks each take one; `SetDrawArea` takes two; model transform/draw hooks take three; `SetDrawMove`, `SetDrawTPage`, and the menu packet hooks take four. `SetDrawMove`'s four SDK inputs are the primitive, rectangle, x, and y; only these values reach its callback.
* Menu label formats consume one vararg, while value formats consume two. The argument frame is zeroed and receives only those format arguments, so a previous `$a2` value is not carried into a later label call.
* The cursor updates the window's right and bottom edges from its current frame dimensions and selected row before calling the renderer. Its rotation uses the already-ported `RotMatrixZYX` and `ScaleMatrix` routines.

## Coverage exclusions and proofs

All exclusions are appended to `tools/mips/check_exclusions.csv` and enforced by the checker.

* `801FF000..80200000` is the original o32 stack window. The native C stack is separate; main RAM below that range and all scratchpad are compared.
* `800EC354:32764` is the renderer-owned draw-context, ordering-table, and packet-buffer region beginning at `g_PrimAlloc`. Inline GPU packet construction is replaced by renderer hooks. The comparison resumes at the stage-state block `800F4350` and still includes the menu records at `800F5270`.
* `MenuWindowAdd 8007D4DC` is unreachable for windows created by this function. It starts with frame height 15 and stores an unsigned-byte item count. Its calculated item width is `2 * (15 + 5 * item_count) - 5`, always 25..2575, so the signed negative-width correction cannot execute.
* The `MenuWindowsRun` divide trap arms at `8007DFE4`, `8007DFF4..8007DFFC`, `8007E2C4`, `8007E2D4..8007E2DC`, `8007E3A4`, and `8007E3B4..8007E3BC` all divide by the positive maximum-text-height value produced by `MenuWindowAdd` (25..2575). That divisor cannot be zero or -1.
* `8007F00C`, `8007F050`, and `8007F098` are negative-correction arms for the cursor's magic divide-by-ten path. `animation_x` is read as an unsigned halfword. The two signed inputs are bounded by 134,377,472 and 805,294,080, so the divisions and wrapped angles stay nonnegative.

No coverage gaps remain undeclared in the default result.
