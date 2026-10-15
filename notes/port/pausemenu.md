# Pause menu port

## Status

All 19 functions in `extract/codex-task/task.md` are ported and marked `diffed` in `tools/verified.csv`. The default
RAM-image differential run compares 736 function cases against the original instructions using area members `000.bin`,
`002.bin`, and `004.bin`. The result is `extract/checks/results/check_pausemenu.json`; no uncovered instruction addresses
remain after the registered proofs and renderer replacements are applied.

| Function | Cases | Instruction coverage | Mutations caught |
|---|---:|---:|---:|
| `SoundLoopsResume` | 16 | 100% | 2/2 |
| `PlayerStopGadgetSounds` | 16 | 100% | 2/2 |
| `DrawScaledModelAt` | 19 | 99.28% | 2/2 |
| `PauseMenuDrawButton` | 130 | 98.64% | 2/2 |
| `ScreenWipeReset` | 16 | 100% | 2/2 |
| `ScreenSnapshotRestore` | 16 | 100% | 2/2 |
| `ScreenWipeBlurBlock` | 4 | 100% | 2/2 |
| `ScreenWipeStep` | 3 | 100% | 2/2 |
| `PauseMenuRun` | 26 | 75.65% | 2/2 |
| `PauseMenuSetLayout` | 8 | 100% | 2/2 |
| `PauseMenuDrawButtons` | 8 | 100% | 2/2 |
| `HudDrawWindowFrame` | 3 | 0%* | 2/2 |
| `PauseItemEnabled` | 324 | 100% | 2/2 |
| `PauseMenuUpdate` | 3 | 100% | 2/2 |
| `SetDAT_800BEF80` | 64 | 100% | 2/2 |
| `SoundLoopsSetPan` | 64 | 100% | 2/2 |
| `MessageBoxDrawString` | 8 | 100% | 2/2 |
| `TextWidthUntilGlyph` | 4 | 100% | 2/2 |
| `RadarDrawModel` | 4 | 100% | 2/2 |

The mutation run caught all 38 active probes. Two exploratory probes were dropped and recorded in `check_exclusions.csv`:
they changed stack-local data that the renderer test hook does not observe, so each was replaced by a probe that changes a
RAM result or a recorded hook argument.

`HudDrawWindowFrame`'s 0% is expected: all 70 original instructions build GPU frame packets and are replaced by the
`RendererHudDrawWindowFrame(x, y, width, height)` native renderer hook. `PauseMenuRun` has 358 declared-unreachable
instructions. Its 325-instruction gadget-preview renderer block (`8008052C..80080A3C`) is replaced by
`RendererPauseMenuDrawGadgetScreen`; the remaining exclusions are signed divide guards, cursor fallback arms, and the
primitive-buffer overrun diagnostic, each with its proof in `tools/mips/check_exclusions.csv`. `DrawScaledModelAt` has
one unreachable signed-overflow BREAK, and `PauseMenuDrawButton` has one impossible integer-range branch.

## Call tree and native boundaries

* `PauseMenuUpdate` calls the frame-boundary `WaitFrameFlip`, stops the current gadget's loop sounds, then calls
  `PauseMenuRun`.
* `PauseMenuRun` owns the pause frame loop, input and mode transitions, layouts, text, gadget slot updates, HUD state,
  screen wiping, and cleanup. It calls the already ported player selector when the selected gadget matches the active
  slot.
* `SoundLoopsResume` conditionally calls the out-of-scope area-1B overlay, then calls `SoundLoopsSetPan`.
* `ScreenWipeStep` calls `ScreenWipeBlurBlock`; `ScreenSnapshotRestore` constructs two rectangles and sends them through
  the `LoadImage` VRAM hook.
* `DrawScaledModelAt`, `PauseMenuDrawButton`, and `RadarDrawModel` use the existing SDK matrix and trig ports for
  arithmetic. Projection, model submission, and GPU packet work remain renderer hooks.
* `HudDrawWindowFrame` and `PauseMenuDrawButtons` keep their menu state and arguments in C while named renderer hooks
  create the native packets. `ScreenWipeBlurBlock` keeps its blur kernel, accumulation, and pixel writes in C while
  `RendererIrgbToIr123` and `RendererPackIrgb` perform the hardware-facing color conversion.

Calls outside the 19-function batch remain named fields in `AePauseMenuHooks` in `src/ui/ae_pausemenu.h`. They include
PsyQ sync, environment, VRAM, ordering-table, and GTE calls; frame, pad, heap, CD, sound, and stage calls; the overlay
`PauseMenuOverlay1B`; HUD and pause-screen helpers such as `PauseDrawCounter`, `PauseDrawCaptureGrid`,
`PauseMenuDrawSubScreen`, `HudDrawGadgetSlots`, and the `HudDraw*` helpers; text helpers `TextNextGlyph` and
`FontDrawGlyph`; and renderer work `ModelTransformRigid`, `ModelDraw`, `ModelEmitPrimsScratchColors`,
`ModelDrawScreenSpace`, `GadgetDrawSlot0Attachment`, `RendererPauseMenuDrawGadgetScreen`,
`RendererPauseMenuDrawButtons`, and `RendererHudDrawWindowFrame`.

## Findings from the instructions

* The `PauseMenuRun` inventory size is 3980 bytes, but its epilogue is at `80081144`, a 5880-byte span from `8007FA4C`.
* `$s5` carries both the mode-3 exit result and the selected gadget index during the gadget screen. The C port preserves
  the caller's initial value and updates it from the normalized gadget cursor at the same point as the MIPS code.
* The normal submenu's back choice plays SFX 1; the other submenu choices play SFX 3.
* The gadget cursor stays in 0..7; the normal submenu cursor stays in 0..1 and the time-attack cursor in 0..2.
* The ring's division-by-zero cases at progress 26 and 37 are reproduced as C error traps at the same MIPS BREAK
  instructions. The signed-overflow checks cannot divide by -1 because their divisors are always even.
* `PauseMenuSetLayout` writes only the selected 16-bit fields. `PauseMenuDrawButton` maps item numbers to model indices
  `[2, 3, 1, 0]`; disabled items set the scratch visibility flag and clear rotation. `PauseItemEnabled` preserves the
  MIPS return-register values of 1 for negative out-of-range items and 3 for positive values above 3.

## Validation

The default run completes in under one minute:

```text
.venv/Scripts/python.exe tools/mips/check_pausemenu.py
```

The 1% case run with mutation testing passed 38/38 probes, two per function:

```text
.venv/Scripts/python.exe tools/mips/check_pausemenu.py --cases 1 --mutate
```
