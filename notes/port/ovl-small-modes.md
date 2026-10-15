# Small mode overlays 183, 184, 185 and 18A

The readable ports are in `src/modes/ae_ovl183.c`, `ae_ovl184.c`,
`ae_ovl185.c` and `ae_ovl18a.c`. Each ported game-logic function has one
structured C function and a differential checker; pure native rendering stays
behind named hooks. The checkers load the matching overlay over real EXE and
area RAM. Every MIPS call uses the explicit 2,000,000 instruction budget in
`tools/mips/overlay_small_harness.py`.

## Port and default differential results

| Overlay member | Function | Default cases | Instruction coverage |
| --- | --- | ---: | ---: |
| 183 | `ovl183_entry_80136000` | 8 | 111/111 |
| 183 | `SoundTest_Update` | 8 | 34/34 |
| 183 | `SoundTest_DrawMainMenu` | 8 | 168/277 |
| 183 | `SoundTest_DrawCommonTest` | 8 | 109/109 |
| 183 | `SoundTest_DrawSightTest` | 8 | 138/138 |
| 184 | `ovl184_entry_80136000` | 8 | 14/14 |
| 184 | `Ovl184_LoadListLoop` | 8 | 102/102 |
| 184 | `Ovl184_DemoLoop` | 8 | 96/96 |
| 184 | `Ovl184_DrawDemoPrompt` | 8 | 33/33 |
| 185 | `ovl185_entry_80146000` | 8 | 29/31 |
| 185 | `Ovl185_Run` | 8 | 109/109 |
| 185 | `Ovl185_Update` | 8 | 81/180 |
| 185 | `Ovl185_LoadScene` | 8 | 91/91 |
| 18A | `StageClearUi_LookAt_18A` | 8 | 118/118 |
| 18A | `Ovl18A_StringTableIndex` | 8 | 12/12 |
| 18A | `ovl_18A_entry_80136208` | 8 | 266/327 |
| 18A | `Ovl18A_WaitInputTimeout` | 8 | 23/23 |
| 18A | `Ovl18A_SceneInit` | 8 | 83/83 |
| 18A | `Ovl18A_UpdateSpin` | 8 | 117/117 |
| 18A | `Ovl18A_AdvanceAnim` | 8 | 89/89 |
| 18A | `Ovl18A_StartClip` | 8 | 26/26 |
| 18A | `Ovl18A_PlayXa` | 8 | 10/10 |
| 18A | `Ovl18A_PickScene` | 28 | 85/85 |

All default differential cases pass: 40 for member 183, 32 for 184, 32 for
185 and 100 for 18A. Each ported function has two checker mutations; all 46
mutations were caught (10, 8, 8 and 20 by member).

## Declared coverage gaps

- Member 183's 109 uncovered `DrawMainMenu` instructions build the selection
  rectangle and GPU ordering-table links. `SoundTest_DrawMainMenuSelection`
  receives the computed selection and draw values through the renderer hook;
  the cursor update and later state logic remain compared.
- Member 185's entry has two unreachable dispatch instructions: the entry
  initializes its mode to zero, while `Ovl185_Run` only writes one and
  `Ovl185_LoadScene` resets it to zero. Its 99 uncovered `Update` instructions
  emit the selection rectangle and ordering-table link through
  `Ovl185_DrawSelection`; the following cursor update remains compared.
- Member 18A's 61 declared entry gaps include two inline panel renderer blocks
  (`Ovl18A_DrawClearPanel` and `Ovl18A_DrawAnimatedPanel`) plus signed division
  trap paths. The camera interpolation divides only by positive frames-left,
  and differences of signed 16-bit coordinates cannot be `INT32_MIN`; those
  trap paths are unreachable.
- Each checker excludes `801FF000..80200000` because MIPS and C use separate
  host stacks. No other RAM is masked. Member 184 and all remaining instructions
  in the other members are covered.

The exclusion reasons and byte ranges are recorded in
`tools/mips/check_exclusions.csv`.

## Hooks left at native boundaries

- **183:** `SoundTest_DrawMainMenuSelection` emits the highlighted menu
  geometry. `DrawDebugGlyph` and `FinishDebugText` emit text packets;
  `DebugTextSetCursor` sets the text cursor. `SoundLoopTableGetParams` supplies
  loop bounds, and `SoundStopAllVoices` stops SPU voices. `ArcLoadEntry`,
  `LoadListStep` and `CdMgrUpdate` perform archive/CD work. `InitDisplay`,
  `ResetGraph`, `ClearImage`, `DrawSync`, `BeginFrame`, `VSync`, `PutDispEnv`,
  `PutDrawEnv` and `DrawOTag` perform display, synchronization and frame
  presentation.
- **184:** `DebugTextSetCursor`, `DrawDebugGlyph` and `FinishDebugText` retain
  the native text-packet boundary around the existing debug-text port.
  `LoadListStart` and `LoadListStep` handle resident-list loading. `InitDisplay`,
  `ResetGraph`, `ClearImage`, `DrawSync`, `BeginFrame`, `VSync`, `PutDispEnv`,
  `PutDrawEnv` and `DrawOTag` own display setup and frame presentation.
- **185:** `Ovl185_DrawText` is a pure text-rendering function and remains a hook
  without a ledger row. `Ovl185_DrawSelection` emits the selection geometry.
  `AreaFeature30Request` and `StageRenderInit` remain named game/native hooks;
  `ArcLoadEntry`, `LoadListStep` and `CdMgrUpdate` handle archive/CD work.
  `InitDisplay`, `ResetGraph`, `ClearImage`, `DrawSync`, `BeginFrame`, `VSync`,
  `PutDispEnv`, `PutDrawEnv`, `DrawOTag` and `QueueFrameFlip` handle display and
  frame presentation.
- **18A:** the two panel hooks emit their respective GPU packets. `MeshInit`,
  `SetGeomScreen`, `ScaleMatrixL`, `ModelTransformSkinned`, `ModelDraw`,
  `MeshTransform` and `MeshDrawAnimColors` remain renderer or SDK hooks.
  `SetBackColor`, `SetFarColor` and `SetColorMatrix` update GPU color and matrix
  state.
  `ArcLoadEntry`, `LoadListStep`, `CdMgrStopStream` and `CdMgrUpdate` handle
  archive and CD streaming. `SoundStopAndRestoreVolume` retains the native sound
  boundary. `InitDisplay`, `ClearImage`, `DrawSync`, `BeginFrame`, `VSync`,
  `PutDispEnv`, `PutDrawEnv` and `DrawOTag` own display setup and presentation.
  `Ovl18A_PlayXa` calls the verified `ae_XaPlay` port directly; `CdIntToPos`,
  `CdControl`, `CdReadyCallback`, `CdPosToInt`, `VSync` and `printf` remain the
  player port's CD SDK hooks for queue-full or command-issue paths. The verified
  `SoundSetVolumeMode` port runs directly through the player hook adapter.

## Findings

- The sound-test path exposed a real negative-track mismatch in
  `ae_SoundStop`: the negative “no BGM track” value must not clear the BGM
  state fields. The proven correction is in `src/world/ae_stagefade.c`.
- The 18A entry writes fog fields in the level-grid global at `0x800EBFA0`;
  an earlier address typo targeted `0x800FBFA0`. The named global now points at
  the original location.
- `Ovl18A_PlayXa` has a 24-byte wrapper frame before `XaPlay`'s 64-byte frame.
  Its port preserves the original CD output pointer and the untouched tail bytes
  copied from the two byte-sized stack locals into CD queue words.
- `tools/check_verified.py` already scans the new `src/modes/ae_ovl*.c` files,
  so its scan list did not need editing. Every diffed row added for this batch
  names its overlay member because the overlay addresses are reused.
