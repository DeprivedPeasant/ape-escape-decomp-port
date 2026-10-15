# HUD, gadget icons and message box

`src/ui/ae_hud.{c,h}` ports the 29 functions listed for this batch. Gameplay
state, glyph stepping, coordinates, animation and RAM updates stay in the RAM
image. Renderer callbacks receive the values the original code computed;
native rendering owns PS1 packets, textures and ordering-table writes.

## Instruction call tree

`StageDrawScreenOverlay` emits the stage overlay first, then checks the area
feature and area kind. Its remaining path can call `HudDrawHealth`,
`HudDrawGadgetSlots` (`HudDrawGadgetSlotsFrame`, `HudDrawGadgetSlotIcons`,
`HudDrawGadgetIcon`), `HudPanelB_Update` (`HudDrawDigit`, `HudDrawNumber`,
`HudPanelB_DrawSprite`), `HudDrawGadgetIcons` (`HudDrawIconSprite`,
`RadarUsable`), `HudPanelA_Update` (`HudPanelA_DrawIcon`), and
`HudCounterC_Update` (`HudCounterC_Draw`, `HudDrawDigit`). Time-attack areas
also reach `HudDrawIcon16`, `HudDrawTime` (`HudDrawDigit`), and
`HudDrawTimerGauge`. Area texture copies go through `AreaTexCopyDraw`.

`MessageBoxUpdateDraw` calls `MessageBoxDrawPrompt` and
`MessageBoxDrawWindow`. The prompt draws through `MessageBoxDrawText`, which
uses `TextSetCursor`, `TextNextGlyph` and `FontDrawGlyph`; prompt sound events
call the already-ported `ae_PlaySfx` API.

`RadarUsable` calls already-ported player state helpers. Area feature 30 and
time-attack state use the existing `ae_AreaFeature30GetState` and
`ae_IsTimeAttack` ports. The pure GTE sine and cosine routines used by the
gadget-slot gauge are called directly from `src/core/ae_sdk.c`.

## Port status and validation

The default differential run uses the real EXE and real area members from
`KKIIDDZZ.DAT/.BNS` (at least three areas), rotates them across the cases, and
compares the original MIPS instructions with the C entry points. It compares
return values, RAM below the declared stack window, all scratchpad bytes,
external hook arguments and order, primitive cursor changes, and standard
sprite fields decoded from the original packets. The default run has 20 cases
per function (580 total) and passes in about 2.4 seconds.

| Function | Address | Cases | Instructions reached |
|---|---:|---:|---:|
| `RadarUsable` | `800468EC` | 20 | 33/33 |
| `HudDrawIconSprite` | `80046ACC` | 20 | 62/62 |
| `HudDrawGadgetIcons` | `80046BC4` | 20 | 111/111 |
| `GetDAT_800B01C2` | `8005DD34` | 20 | 4/4 |
| `StageDrawScreenOverlay` | `8005E0F0` | 20 | 207/207 |
| `HudDrawHealth` | `8005E4A8` | 20 | 83/83 |
| `AreaTexCopyDraw` | `8005EAB0` | 20 | 122/123; 1 proven unreachable |
| `HudPanelA_DrawIcon` | `8005F2E0` | 20 | 197/199; 2 proven unreachable |
| `HudPanelB_DrawSprite` | `8005F5FC` | 20 | 36/36 |
| `HudDrawDigit` | `8005F770` | 20 | 67/67 |
| `HudDrawNumber` | `8005F87C` | 20 | 55/63; 8 proven unreachable |
| `HudDrawTime` | `8005F978` | 20 | 196/203; 7 proven unreachable |
| `HudDrawIcon16` | `8005FCA4` | 20 | 55/55 |
| `HudCounterC_Draw` | `8005FD80` | 20 | 149/149 |
| `HudDrawTimerGauge` | `8005FFD4` | 20 | 148/148 |
| `HudDrawGadgetSlotsFrame` | `80081180` | 20 | 150/150 |
| `HudDrawGadgetSlotIcons` | `800813D8` | 20 | 163/163 |
| `HudDrawGadgetSlots` | `80081664` | 20 | 198/198 |
| `HudDrawGadgetIcon` | `800819E8` | 20 | 109/109 |
| `FontDrawGlyph` | `8008D9D0` | 20 | 230/230 |
| `TextNextGlyph` | `8008DD68` | 20 | 18/18 |
| `TextSetCursor` | `8008DDB0` | 20 | 6/6 |
| `MessageBoxUpdateDraw` | `8008DF4C` | 20 | 13/13 |
| `MessageBoxDrawText` | `8008DF80` | 20 | 31/31 |
| `MessageBoxDrawPrompt` | `8008DFFC` | 20 | 139/144; 5 proven unreachable |
| `MessageBoxDrawWindow` | `8008E23C` | 20 | 218/222; 4 proven unreachable |
| `HudPanelA_Update` | `8009529C` | 20 | 104/104 |
| `HudPanelB_Update` | `80095690` | 20 | 140/140 |
| `HudCounterC_Update` | `80095924` | 20 | 15/15 |

Coverage reaches 3,059 of 3,086 instructions; all 27 remaining instructions
have proof entries in `tools/mips/check_exclusions.csv`:

* `AreaTexCopyDraw`: its loop counter only advances from 0 through 6, so the
  negative-counter delay-slot path cannot run.
* `HudPanelA_DrawIcon`: a zero gate returns before division; the second BREAK
  guard cannot see numerator `INT32_MIN` and divisor `-1`.
* `HudDrawNumber`: division uses positive fixed divisors 1000, 100, 10 and 1;
  neither BREAK condition can occur.
* `HudDrawTime`: its segment index only takes values 0, 1 and 2. The digit-row
  arithmetic is unsigned and the `-1` digit case is replaced with 12, so the
  fallback branches and negative-rounding targets cannot run.
* `MessageBoxDrawPrompt`: the text scan stops on code 0 or `0x0F` and rewinds
  to that same terminator; its `0x0D` repeat-delay comparison is unreachable.
* `MessageBoxDrawWindow`: animation is shifted right before forming the
  divisor as animation + 1, keeping it in 1..32768; neither division BREAK
  guard can occur.

Two independent entry-write mutations were tested for every function. Both
variants were caught for all 29 functions (58/58 mutations). The checker
declares the top-of-RAM stack window and renderer-owned packet / ordering-table
ranges. It compares the remaining RAM and full scratchpad, checks the renderer
requests against original sprite packets where applicable, and compares
primitive deltas and external SDK / overlay hook calls. `EXTRA_COVERAGE` is
passed to `finish`; no extra target ranges are needed. The default coverage
run writes `extract/checks/results/check_hud.json`.

The 29 functions are appended as `diffed` with `min_cases=20` in
`tools/verified.csv`. The required `.venv/Scripts/python.exe
tools/check_verified.py` run completed with zero errors; its 45 warnings are
for other checkers without result files. `tools/check_verified.py` was not
modified.

## Instruction findings

* `StageDrawScreenOverlay` treats area kinds 31 and 32 specially: it draws the
  gadget slots and skips the health and slot-flag checks. Game mode 1 also
  skips health and gadget slots in the other area-kind branches, while the
  later panel and counter flag checks still run.
* Both panel update functions leave their slide displacement at zero in the
  HOLD state. Their timer keeps counting down, but no intermediate slide is
  drawn in that state.
* Button glyph `0x10B` shifts upward by five pixels whether the blink selects
  its alternate texture or not; blink only chooses the texture record.
* Prompt state 4 does not return after drawing the prompt glyph. Without input,
  it continues through the text terminator scan and delay logic.
* `HudDrawGadgetIcons` selects icon index 2 when the GPU frame bit is set, the
  current gadget is 2, gadget-icon mode is 1 and its charge value is nonzero.
* The counter model path sets the original GTE color-matrix mode scratch word
  before calling the native `ModelDrawScreenSpace` boundary.

## Hooks left open

* Renderer callbacks `draw_sprite`, `draw_quad`, `draw_glyph`,
  `draw_stage_overlay`, `draw_gauge` and `draw_message_frame` carry the
  computed image fields, colors, glyph, depth, primitive cursor and ordering
  entry. The native renderer supplies them; they replace inline PS1 packet,
  texture and ordering-table construction.
* `DR_MOVE` (8002B380) receives the primitive, rectangle and source UV used by
  `AreaTexCopyDraw`; the native renderer / VRAM layer supplies the SDK hook.
* `ClearMemory` (800A3740) clears the model argument blocks in
  `HudCounterC_Draw` and `HudDrawTimerGauge`; the platform supplies this SDK
  utility hook.
* `ModelDrawScreenSpace` (800207B4) receives model, position, rotation and
  scale addresses from those same counter and timer paths; the native
  renderer supplies the projection / draw hook.
* `Ovl_8013D424` (8013D424) is the existing named overlay hook called by
  `StageDrawScreenOverlay` for area kinds 7 and 14. That out-of-scope overlay
  function can replace the hook when its overlay member is ported.
* Sound uses the existing `ae_PlaySfx` port and `AeHudHooks.sound_hw`; no
  additional `PlaySfx` game-function hook remains in this batch.
