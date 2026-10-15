# Stage-start overlay (member 188)

Member 188 is loaded at `0x80136000` from `extract/members/188.bin` (24 KiB). The port has one named `ae_*` entry for each of the 28 code ranges in the member inventory. The two helpers at the start of the overlay are separate functions in this member even though their machine code is shared with member 187.

## Call map

`Mode_StageStart` handles the mode transitions. Mode 8 first runs the opening sequence; otherwise it calls the stage-entry runner. After the runner returns, mode 18 becomes 19, mode 23 becomes 24, and the other modes become 11. It copies the stage name from the overlay's packed string table and writes the level information into the mode state.

`StageStartUi_RunA` and `StageStartUi_RunB` initialize their display and sound state, update pad input, and run frame loops. The loops drive `StageStartUi_DrawWipeA` / `StageStartUi_DrawWipeB`, scene updates, and the stage-start script. The script updates object flags, animation, vectors and counters, selects the soundtrack, and draws enabled objective lines. Area initialization reads the loaded area tables and creates prop and static-model state. `StageStartUi_UpdateObjects` interpolates object state and advances model animation before passing model work to the renderer.

The leaf routines include the string-table lookup, view-matrix calculation, objective and number selection, text/counter state updates, ripple-grid initialization and smoothing, and renderer requests for glyphs, wipe geometry, scanlines, copies and model drawing.

### Complete code inventory and differential status

The cases and instruction coverage below are from the default run of `tools/mips/check_ovl188.py` (192 cases). Every row passed; all uncovered instructions are accounted for by the exclusion registry.

| Original entry | Port status | Cases | Instruction coverage |
|---|---:|---:|---:|
| `StageStartUi_LookAt` `80136000` | ported, diffed | 4 | 100% |
| `StageStartUi_StringTableIndex` `801361D8` | ported, diffed | 4 | 100% |
| `Mode_StageStart` `8013683C` | ported, diffed | 5 | 100% |
| `StageStartUi_RunA` `80136A1C` | ported, diffed | 8 | 100% |
| `StageStartUi_CopyStrip` `80136EAC` | ported, diffed; renderer hook | 2 | 19.25%; 151 renderer instructions declared unreachable |
| `StageStartUi_DrawWipeA` `80137198` | ported, diffed | 18 | 98.48%; 4 unreachable clamp instructions |
| `StageStartUi_RunB` `801375B4` | ported, diffed | 14 | 100% |
| `StageStartUi_DrawWipeB` `80137C88` | ported, diffed | 20 | 98.56%; 4 unreachable clamp instructions |
| `StageStartUi_MeshInit` `801380DC` | ported, diffed | 2 | 100% |
| `StageStartUi_SetAreaMeshTable` `80138118` | ported, diffed | 2 | 100% |
| `StageStartUi_AreaInit` `80138138` | ported, diffed | 2 | 100% |
| `StageStartUi_RunScript` `801385B0` | ported, diffed | 33 | 94.00%; 32 unreachable dispatch/divide-guard instructions |
| `StageStartUi_DrawNumber` `80138E04` | ported, diffed | 6 | 100% |
| `StageStartUi_DrawTitleLines` `8013912C` | ported, diffed | 3 | 100% |
| `StageStartUi_DrawObjectives` `80139248` | ported, diffed | 12 | 100% |
| `StageStartUi_DrawStageIcon` `801394D0` | ported, diffed; renderer hook | 5 | 100% |
| `StageStartUi_DrawText` `80139628` | ported, diffed | 4 | 100% |
| `StageStartUi_SetDrawStp` `80139710` | ported, diffed; renderer hook | 2 | 100% |
| `StageStartUi_DrawCounter` `801397F4` | ported, diffed | 4 | 100% |
| `StageStartUi_UpdateObjects` `801399A4` | ported, diffed | 9 | 91.49%; 24 unreachable divide-guard instructions |
| `StageStartUi_CopyBuffer` `80139E0C` | ported, diffed; renderer hook | 4 | 100% |
| `StageStartUi_ClampVerts` `80139EE0` | ported, diffed | 6 | 100% |
| `StageStartUi_WipeQuadA` `80139FB8` | ported, diffed; renderer hook | 4 | 100% |
| `StageStartUi_WipeQuadB` `8013A10C` | ported, diffed; renderer hook | 4 | 100% |
| `StageStartUi_DrawScanlines` `8013A3C4` | ported, diffed; renderer hook | 5 | 85.54%; 36 unreachable divide/negative-coordinate instructions |
| `RippleGrid_Init2` `8013A7A8` | ported, diffed | 2 | 100% |
| `RippleGrid_Smooth2` `8013A8C0` | ported, diffed | 4 | 100% |
| `RippleGrid_Draw2` `8013AABC` | ported, diffed; renderer hook | 4 | 100% |

Overall coverage is 4,025 of 4,276 instructions. The 251 uncovered instructions are all declared in `tools/mips/check_exclusions.csv`: 151 are PS1 VRAM / ordering-table construction in `StageStartUi_CopyStrip`; four each are wipe clamps that cannot be reached when the preceding timer calculation is below six; 32 are masked script-dispatch fallback ranges or signed-divide traps; 24 are signed-divide traps skipped for nonpositive interpolation times; 36 are scanline divide traps and a negative-coordinate branch whose value is always 2 through 5.

The checker starts from the extracted EXE, places member 188 at its overlay address, and loads real area members from the project area map. The scenarios use several areas and cover transition modes, mode 18 progress thresholds, script opcodes, animation boundaries, text/glyph ranges, wipe timing, and ripple frame wrap. It compares return values, RAM outside the declared renderer/stack ranges, scratchpad, hook calls and semantic renderer requests. Every `MipsRam.call` in the checker has an explicit instruction limit: 20,000,000 for a member entry and 2,000,000 for heap setup.

Mutation mode injects two distinct observable RAM-write mutations into each of the 28 entries. The checker caught all 56/56 mutations. The normal result file is `extract/checks/results/check_ovl188.json`; ledger rows in `tools/verified.csv` use the required `overlay member 188.bin` note.

`tools/check_verified.py` exits successfully with 0 errors. Its 87 warnings include absent default result files for other checkers and member-188 ledger addresses missing from the generated `notes/inventory/functions.csv`; that generated inventory was left untouched.

## Hook boundary

The overlay hook table preserves original argument words. It is used for SDK/BIOS services and native system calls. Ported game routines are called directly where available: `ae_StageRequest` (with `AeOvl188Hooks.stage_hooks`), `ae_PadRumbleReset`, `ae_pad_PadUpdate`, `ae_ProgressGetByte`, `ae_HeapInit`, area-set/heap/model routines, sound-fade/SFX routines, HUD timer drawing, pose updates, and the validated SDK maths routines. `StageRequest` and `PadRumbleReset` used to be generic hook calls in the first draft; both now enter their existing ports. `XaPlay` stays a named hook because it starts CD/XA audio; `SetColorMatrixMode`, `printf`, pad functions, and `Exit` stay hooks at the SDK/BIOS boundary.

### Named call hooks still supplied by the host

| Hook names | Original job |
|---|---|
| `PadGetState`, `PadInfoMode`, `PadSetupDualShock`, `PadDemoPlaybackStep`, `StickToButtons` | Controller state, mode configuration, demo input and analog-to-button mapping. |
| `Exit`, `printf`, `VSync` | BIOS exit, PsyQ debug output and vertical-sync wait. `Exit` is the BIOS vector at `0xB0`. |
| `InitDisplay`, `ResetGraph`, `ClearImage`, `SetGeomScreen`, `SetColorMatrixMode`, `DrawSync`, `PutDispEnv`, `PutDrawEnv`, `DrawOTag`, `BeginFrame`, `DR_MOVE` | Display/GPU initialization, synchronization, environment selection, frame submission and transfer packets. |
| `MatrixBuild`, `MatrixScale`, `MeshInit`, `DrawAreaProps` | Engine matrix/mesh setup and static-area-prop drawing. |
| `ArcLoadEntry`, `LoadListStart`, `LoadListStep`, `CdMgrUpdate` | Archive-entry selection, asynchronous list loading and CD-manager progress. |
| `SpuWaitIdle`, `SpuWaitLoop`, `SeqTrackClose`, `SeqTrackSetPlayState`, `SoundBankFree`, `SoundStopAllVoices`, `SoundStopAndRestoreVolume`, `XaPlay` | SPU waits, sequence/bank/voice state, volume restore and XA streaming. |
| `AreaKindVisitedSet` | Records the area's visited state. |
| `LevelCaughtCount`, `LevelGetInfo`, `LevelTableGet`, `LevelProgressGet` | Reads caught-monkey counts, objective/progress records, time thresholds and saved level progress. |
| `HudDrawTimeRankIcon` | Draws the time-rank icon. |

The semantic renderer callback carries computed values for `HudDrawTimeCell`, `StageStartUi_DrawGlyph`, `StageStartUi_DrawCounterDigits`, `StageStartUi_DrawStageIcon`, `StageStartUi_SetDrawStp`, `StageStartUi_CopyStrip`, `StageStartUi_CopyBuffer`, `StageStartUi_WipeQuadA`, `StageStartUi_WipeQuadB`, `StageStartUi_DrawScanlines`, `StageStartUi_DrawWipeBackground`, and `RippleGrid_Draw2`. Model work uses `ModelTransformSkinned`, `ModelDraw`, `ModelEmitPrimsScratchColors`, `ModelTransform`, and `ModelDrawTransformed`. These hooks replace GPU/VRAM packet construction while the RAM state and values passed to the renderer remain compared.

No call hook that has an already-ported non-SDK game target remains in the module. `AeOvl188Hooks.stage_hooks` is required when the demo-input path can request a stage transition; the current test API leaves it null because the default cases do not enter that branch. The game-host wiring belongs to the other project task and was not changed here.

## Findings and shared code

* `StageStartUi_LookAt` at `80136000` is byte-identical to member 187's `StageClearUi_LookAt` at the same offset (472 bytes).
* `StageStartUi_StringTableIndex` at `801361D8` is byte-identical to member 187's `StageClearUi_StringTableIndex` at the same offset (48 bytes).
* No other function slice in the 28 member-188 ranges exactly matches a member-187 function slice.
* The old inventory row called `80138118` `SpecialEnterUi_Nop_188`. Its eight instructions load the area base `0x80154000`, read the zone-table offset at `+0x34`, multiply it by four, add the area base and store the resulting pointer at `0x8013B9C8`. It is not a no-op. The source and `tools/symbols.csv` now call it `StageStartUi_SetAreaMeshTable`; the generated `notes/inventory/functions.csv` was left untouched.
* None of the 28 member-188 entry points were already present in the verified ledger. LookAt and string-table lookup are still ported in this member as separate overlay functions, as requested.

