# Overlay member 18B: special-area entry mode

## Port and validation

The hand-written port is in [ae_ovl18b.c](../../src/modes/ae_ovl18b.c) with named RAM fields,
globals, and the native boundary in [ae_ovl18b.h](../../src/modes/ae_ovl18b.h). The differential
checker is [check_ovl18b.py](../../tools/mips/check_ovl18b.py). It loads the original `18B.bin`
at `0x80136000` over the real EXE RAM image and overlays real area-member images for scenarios.
Mode-entry cases use area IDs 98, 92, and 93; wipe and object cases also use real areas 1, 2,
and 3. The checker compares return values, all main RAM outside the declared stack window,
scratchpad, and ordered native/renderer hook events.

The default run passed 135 cases in about 24 seconds and covered 1,984 of 2,589 instructions.
There are no undeclared instruction gaps. The remaining coverage omissions below are registered
native packet/rendering blocks or mathematically unreachable branches. Mutation mode injects two
wrong state writes at each function entry; all 32 mutations were caught. `check_verified.py`
reported zero errors and confirmed the ledger and exclusion registry are consistent.

## Call tree

```text
Mode_SpecialAreaEnter
├─ SetColorMatrixMode, PadUpdate, then frame loop
└─ SpecialEnterUi_DrawWipe
   ├─ first area setup: AreaSetZoneBoxes, HeapAlloc, MeshInit, ModelInit
   ├─ SpecialEnterUi_SetAreaTablePointer
   ├─ SpecialEnterUi_RunScript
   │  ├─ StageClearUi_CopyStrip_18B
   │  ├─ SpecialEnterUi_PlayXa
   │  ├─ SpecialEnterUi_DrawBlink -> DrawMove / RendererLinkOtag
   │  ├─ SaveUi_StringTableIndex_18B -> SpecialEnterUi_DrawText
   │  ├─ SaveUi_StringTableIndex_18B -> SpecialEnterUi_DrawMessageShadow
   │  │  └─ MessageBoxDrawString (called twice for shadow and foreground)
   │  └─ SpecialEnterUi_DrawScene (renderer boundary)
   ├─ StageClearUi_LookAt_18B -> LIBGTE matrix/vector ports
   ├─ DrawAreaProps
   ├─ SpecialEnterUi_UpdateObjects -> ModelPoseBones
   └─ later wipe frames: RunScript, LookAt, DrawAreaProps, UpdateObjects,
      StageClearUi_DrawScanlines_18B -> RendererBuildScanlineBase / DrawMove

Standalone overlay helpers: SpecialEnterUi_TextWidth calls TextLineWidth;
StageClearUi_SetDrawStp_18B emits SetDrawStp.
```

| Original function | Address | Cases | Covered / total | Excluded | Status |
| --- | ---: | ---: | ---: | ---: | --- |
| `StageClearUi_LookAt_18B` | `80136000` | 8 | 118 / 118 (100%) | — | ported, diffed |
| `SaveUi_StringTableIndex_18B` | `801361D8` | 8 | 12 / 12 (100%) | — | ported, diffed |
| `Mode_SpecialAreaEnter` | `80136208` | 8 | 467 / 467 (100%) | — | ported, diffed |
| `SpecialEnterUi_DrawWipe` | `80136954` | 8 | 286 / 560 (51.07%) | 274 renderer/unreachable | ported, diffed |
| `SpecialEnterUi_DrawText` | `80137214` | 8 | 36 / 72 (50%) | 36 glyph packet | ported, diffed |
| `SpecialEnterUi_DrawMessageShadow` | `80137334` | 8 | 28 / 28 (100%) | — | ported, diffed |
| `SpecialEnterUi_DrawBlink` | `801373A4` | 8 | 69 / 75 (92%) | 6 renderer/unreachable | ported, diffed |
| `SpecialEnterUi_RunScript` | `801374D0` | 15 | 475 / 513 (92.59%) | 38 unreachable | ported, diffed |
| `StageClearUi_SetDrawStp_18B` | `80137CD4` | 8 | 19 / 30 (63.33%) | 11 renderer | ported, diffed |
| `StageClearUi_CopyStrip_18B` | `80137D4C` | 8 | 28 / 41 (68.29%) | 13 renderer | ported, diffed |
| `SpecialEnterUi_UpdateObjects` | `80137DF0` | 8 | 162 / 186 (87.10%) | 24 unreachable | ported, diffed |
| `SpecialEnterUi_DrawScene` | `801380D8` | 8 | 71 / 184 (38.59%) | 113 native renderer | ported, diffed |
| `SpecialEnterUi_SetAreaTablePointer` | `801383B8` | 8 | 8 / 8 (100%) | — | ported, diffed |
| `SpecialEnterUi_TextWidth` | `801383D8` | 8 | 36 / 36 (100%) | — | ported, diffed |
| `SpecialEnterUi_PlayXa` | `80138468` | 8 | 10 / 10 (100%) | — | ported, diffed |
| `StageClearUi_DrawScanlines_18B` | `80138490` | 8 | 159 / 249 (63.86%) | 90 renderer/unreachable | ported, diffed |

The store audit found one persistent write in `DrawScene`: eight words copied from the base
matrix at `0x800AF0A0` to `g_ViewMatrix` at `0x800E78C0` (`80138104..80138138`). The C port performs
that copy and prepares the same 32-byte scene settings block. The checker executes the original
prologue, matrix copy, and epilogue, compares RAM, then replaces the remaining GTE/model projection
and GPU work with `SpecialEnterUi_RenderScene` at `801381D8`. The 113 skipped instructions are
registered as the renderer range.

`DrawText`'s skipped stores at `80137258..801372E4` target glyph packet memory and the three-byte OT
link reached through `UI+8`; its text-X and primitive-cursor writes after that block remain in C and
are compared. The skipped `DrawWipe` stores at `80136CB8..80136E6C` and `80136F3C..801371C0` target
wipe packets and OT links. Its area/model records, wipe timer, primitive cursor, and state updates
outside those ranges remain ported and compared. The remaining exclusions are signed-division traps
ruled out by positive frame counters or fixed divisors, masked script-opcode defaults that no value
of `instruction & 0x3FC0` can reach, and draw-state fallback arms bypassed by earlier state checks.

## Remaining native hooks

These are deliberate host boundaries, not unported gameplay functions:

- **Renderer and GPU packet work:** `SpecialEnterUi_RenderScene` renders the area scene and model
  objects; `SpecialEnterUi_DrawWipePackets` emits wipe polygons; `SpecialEnterUi_DrawGlyph` emits
  each text glyph; `RendererBuildScanlineBase` builds the scanline base packet;
  `RendererLinkOtag` links OT entries; `DrawAreaProps`, `MeshInit`, `ModelTransformSkinned`,
  `ModelDraw`, `DrawMove`, and `SetDrawStp` perform native scene/primitive work. The port still
  computes script state, glyph widths/cursor positions, wipe transitions, transforms, and object
  animation in RAM.
- **Display and archive/CD flow:** `InitDisplay`, `ResetGraph`, `ClearImage`, `SetGeomScreen`,
  `PutDispEnv`, `PutDrawEnv`, `DrawSync`, `VSync`, `DrawOTag`, `ArcLoadEntry`, `LoadListStart`,
  `LoadListStep`, `CdMgrUpdate`, and `CdMgrStopStream` manage display setup, presentation,
  archive/list loading, and streaming.
- **Text, audio, and remaining SDK calls:** `TextLineWidth` and `TextSetCursor` are public text
  APIs; `XaPlay` and `SoundStopAndRestoreVolume` are audio APIs; `ScaleMatrixL` is a PsyQ matrix
  API. The exact project LIBGTE math ports (`VectorNormal`, `VectorNormalSS`, matrix composition,
  matrix application, identity, and square root) are called directly.

## Findings

- `801383B8` is not a no-op. It writes the current area base plus four times its table index to
  `g_AreaTable`; `tools/symbols.csv` and this note name it
  `SpecialEnterUi_SetAreaTablePointer`.
- The supplied inventory omitted `801383D8`, a 144-byte `SpecialEnterUi_TextWidth` helper. It
  asks `TextLineWidth` for the first line and the segment after the last carriage return, takes
  the larger width, then returns the centered x coordinate.
- `Mode_SpecialAreaEnter` selects destinations through the halfword at `0x800F4476`, uses the
  selected area’s table fields, and preserves the staged load/fade/input/stream-stop order.
- Script opcodes are formed by masking the instruction with `0x3FC0`. The `0x0240` operation
  changes a table pointer, and the wait/poll logic sets its completion flag only after every
  entry’s signed wait value is earlier than the next wait threshold.
- Object animation termination compares the signed halfword frame limit. Using an unsigned
  comparison changes behavior for the sentinel value.

The new C file is picked up by `tools/check_verified.py` automatically; its source scan uses
`src/**/ae_*.c`. The 16 rows in `tools/verified.csv` and the function-specific exclusions in
`tools/mips/check_exclusions.csv` are recorded for `18B.bin`. The generated
`notes/inventory/functions.csv` remains unchanged; overlay names and the omitted helper are documented in
`tools/symbols.csv` and this note. No commit was made.
