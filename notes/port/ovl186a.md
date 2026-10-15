# Overlay member 186A: save and memory-card menu

## Port and validation

The hand-written port is in [ae_ovl186a.c](../../src/modes/ae_ovl186a.c), with named globals and
field offsets in [ae_ovl186a.h](../../src/modes/ae_ovl186a.h). The checker is
[check_ovl186a.py](../../tools/mips/check_ovl186a.py). It loads `186.bin` at `0x80136000` over
the extracted EXE RAM image and tests it with real area members 05B, 05E, 064, and 066. It
compares return values, RAM outside the declared stack window, scratchpad, and ordered hook
events. Every MIPS call has a 250,000-instruction budget.

The final default run passed 518 cases and covered 2,410 of 2,698 instructions. All remaining
instructions are covered by registered renderer or worker-loop ranges, or by three proof-backed
unreachable branches. Mutation mode ran two mutations for each of the 30 C entries; all 60 were
caught. `tools/check_verified.py` passed with zero errors. It reported existing warnings for
missing results from other checkers and inventory addresses; the generated inventory was left
unchanged as required.

| Original function | Address | Cases | Covered / total | Excluded | Status |
| --- | ---: | ---: | ---: | ---: | --- |
| `SaveUi_DrawText` | `80136000` | 5 | 48 / 72 | 24 glyph packet | ported, diffed |
| `SaveUi_TextWidth` | `80136120` | 5 | 16 / 16 | — | ported, diffed |
| `SaveUi_LookAt` | `801368E8` | 5 | 104 / 104 | — | ported, diffed |
| `SaveUi_StringTableIndex` | `80136A88` | 7 | 12 / 12 | — | ported, diffed |
| `Mode_SaveMenu` | `80136AB8` | 9 | 168 / 168 | — | ported, diffed |
| `SaveUi_FrameReset` | `80136D58` | 4 | 14 / 14 | — | ported, diffed |
| `SaveUi_SceneInit` | `80136D90` | 8 | 82 / 82 | — | ported, diffed |
| `SaveUi_RunMenuA` | `80136ED8` | 9 | 137 / 139 | 2 unreachable | ported, diffed |
| `SaveUi_RunMenuB` | `80137104` | 8 | 130 / 130 | — | ported, diffed |
| `SaveUi_FileSelectInit` | `8013730C` | 6 | 70 / 70 | — | ported, diffed |
| `SaveUi_TotalMonkeyCount` | `80137424` | 4 | 12 / 12 | — | ported, diffed |
| `SaveUi_MenuUpdate` | `80137454` | 9 | 78 / 78 | — | ported, diffed |
| `SaveUi_RunMenuC` | `8013758C` | 15 | 151 / 155 | 4 unreachable | ported, diffed |
| `SaveUi_CountEntries` | `801377F8` | 4 | 21 / 21 | — | ported, diffed |
| `SaveUi_SetState` | `8013784C` | 50 | 270 / 270 | — | ported, diffed |
| `SaveUi_FormatNumber3` | `80137C84` | 12 | 73 / 73 | — | ported, diffed |
| `SaveUi_MemCardStart` | `80137DA8` | 4 | 29 / 29 | — | ported, diffed |
| `SaveUi_StateTick` | `80137E1C` | 4 | 12 / 12 | — | ported, diffed |
| `SaveUi_StateMachine` | `80137E4C` | 184 | 276 / 276 | — | ported, diffed |
| `SaveUi_OnCardResult_0` | `801383E0` | 11 | 55 / 55 | — | ported, diffed |
| `SaveUi_OnCardResult_1` | `801384BC` | 10 | 26 / 26 | — | ported, diffed |
| `SaveUi_OnCardResult_2` | `80138524` | 10 | 50 / 50 | — | ported, diffed |
| `SaveUi_OnCardResult_3` | `801385EC` | 11 | 56 / 56 | — | ported, diffed |
| `SaveUi_OnCardResult_4` | `801386CC` | 10 | 30 / 30 | — | ported, diffed |
| `SaveUi_OnCardResult_5` | `80138744` | 10 | 46 / 46 | — | ported, diffed |
| `SaveUi_OnCardResult_6` | `801387FC` | 14 | 36 / 36 | — | ported, diffed |
| `SaveUi_DrawFileSelect` | `8013888C` | 8 | 130 / 333 | 203 GPU packet | ported, diffed |
| `SaveUi_DrawSlot` | `80138DC0` | 24 | 203 / 252 | 49 GPU packet | ported, diffed |
| `SaveUi_CardStateThread` | `8013829C` | 48 | 49 / 53 | 4 bounded worker loop | ported, diffed |
| `SaveUi_CardResultThread` | `80138370` | 10 | 26 / 28 | 2 bounded loop | ported, diffed |

`SaveUi_CardStateThread` and `SaveUi_CardResultThread` were also found and ported in the task
range even though they were omitted from its function table. `SaveUi_DrawWindow` (`80136160`)
and `SaveUi_DrawWindowBorder` (`80136554`) are renderer-only functions; they remain native
renderer hooks and have no C body or ledger row.

## Call tree

```text
Mode_SaveMenu
├─ display and input setup, SceneInit, then the frame / mode loop
├─ RunMenuA / RunMenuB / RunMenuC -> MenuUpdate, FadeTick, FileSelectInit
└─ StateTick -> StateMachine -> SetState / MemCardStart / OnCardResult_*
   ├─ CardStateThread -> card-state dispatch
   └─ CardResultThread -> card-result dispatch, including OnCardResult_7 hook

FileSelectInit -> CountEntries / FormatNumber3 / TotalMonkeyCount
DrawFileSelect -> DrawSlot -> DrawText / TextWidth / StringTableIndex
LookAt -> the validated LIBGTE matrix and vector ports
```

## Remaining native and overlay hooks

These boundaries retain the original call names and arguments. The save-menu state, timers,
file-list data, input decisions, text layout, cursor movement, and save-record writes stay in C.

- **Native renderer-only functions:** `SaveUi_DrawWindow` and `SaveUi_DrawWindowBorder` build
  window and border graphics. Inline packet hooks are `SaveUi_DrawGlyph` (one glyph and its
  position/texture row), `SaveUi_FilePanelPacket` (panel cursor and OT index),
  `SaveUi_FileSlotPacket` (cursor and selected slot/entry/save record),
  `SaveUi_MonkeyListPacket` (cursor, item index, selected item),
  `SaveUi_SlotHeaderPacket` (computed header layout), and `SaveUi_SlotDetailPacket` (computed
  detail layout). The original packet-writing instruction ranges are registered in
  `tools/mips/check_exclusions.csv`; game-computed arguments and cursor updates remain compared.
- **Functions in adjacent overlay ranges:** `SaveUi_CardReady`, `SaveUi_DrawMainWindow`,
  `SaveUi_DrawBackground3D`, `SaveUi_YesNoUpdate`, `SaveUi_DrawStateOverlay`,
  `SaveUi_ConfirmPressed`, `SaveUi_CancelPressed`, `SaveUi_DrawDetail`, `SaveUi_DrawCursorAnim`,
  `SaveUi_DrawFileIcons`, `SaveUi_DrawIcon`, `SaveUi_DrawEmptySlotText`,
  `SaveUi_DrawFileHeader`, `SaveUi_DrawPortrait`, `CardMgr_DrawMenuLabels`,
  `CardMgr_DrawMenuItems`, `SaveUi_NameEntryInit`, `SaveUi_NameEntryAccept`,
  `SaveUi_NameEntryCheck`, and `SaveUi_NameEntryUpdate` provide menu widgets, file/portrait
  graphics, card readiness, and name-entry behavior. `SaveUi_FadeTick` owns the separate fade
  step; `SaveUi_LastUsedEntry` selects the saved-entry index; `SaveUi_GetColorBuffer` supplies its
  text color buffer; `SaveUi_OnCardResult_7` handles the later result state. `SaveUi_McxStartCom`,
  `SaveUi_McxStopCom`, `SaveUi_ThreadSetHook`, `SaveUi_ThreadSetHookReset`, `SaveUi_ThreadSwitch`,
  and `SaveUi_ThreadClose` manage the later memory-card communication worker; `SaveUi_ModeStep`
  advances a later save-menu mode.
- **Platform and engine calls:** `MemCardOp` and `MemCardCheckFile` perform card operations;
  `LevelProgressGet`, `ProgressChecksum`, `SaveScore`, and `SaveRestoreState` read or update save
  data. `PadGetState`, `PadInitDirect`, `PadSampleStickHistory`, `PadSetupDualShock`,
  `PadUpdate`, and `StickToButtons` are controller APIs. `PlaySfx`, `BgmGetVoice`,
  `SoundStopAllVoices`, `SoundStopAndRestoreVolume`, `FUN_8009BA90`, and
  `SoundDelayedQueueReset` are audio APIs. `ArcLoadEntry`, `LoadListStep`, `CdMgrUpdate`, and
  `VSync` service archive loading and frame timing. `HeapInit`, `HeapAlloc`, and `MeshInit`
  allocate and initialize the scene; `SetGeomScreen`, `SetColorMatrix`, and `ApplyLevelFog` set
  scene rendering state. `InitDisplay`, `ResetGraph`, `ResetGraphOrDisplay`, `ClearImage`,
  `DrawSync`, `PutDispEnv`, `PutDrawEnv`, `DrawOTag`, and `BeginFrame` are display APIs.
  Pure matrix/vector work, including `LookAt`, calls the validated SDK math ports directly.

## Coverage exclusions

The 24 `DrawText` instructions and the 203 `DrawFileSelect` / 49 `DrawSlot` instructions form
GPU packet writes replaced by the hooks above. Four `CardStateThread` instructions and two
`CardResultThread` instructions bound each MIPS worker comparison to one dispatch iteration;
the state dispatch step is still compared for every registered case. `RunMenuA`'s two excluded
instructions require a signed selection below 2 but not 0 after the negative and selection-1
checks. `RunMenuC`'s four excluded instructions require impossible `StateTick` values -3 or
less than -3 other than the observed -4/-5. These nine rows (six renderer blocks, two worker
loop sites plus two delay slots, and three proof ranges) are listed in `tools/mips/check_exclusions.csv`.
The 14 unreachable rows (six renderer blocks, five worker-loop sites, and three proof-backed
branches) are registered in `tools/mips/check_exclusions.csv`. The original stack window
`801FF000..80200000` is excluded from RAM comparison; other RAM, scratchpad, returns, and hook
events are compared.

## Findings

- Save records use a `0x7A0` byte stride. Using `0x800` selects the wrong slot for subsequent
  files and does not match the MIPS address calculation.
- `SaveUi_MemCardStart` operation 0 clears the menu state before issuing the card operation.
- State 7 in `SaveUi_StateMachine` deletes the selected record and starts card operation 2.
- State 41 plays the confirmation sound twice and returns `-4`; neighboring confirm states
  play it once and proceed to operation 7.
- `Mode_SaveMenu` passes the caller's initial `a0` to `PadGetState`; its cleanup path uses the
  original call-site context. The `RunMenuC` return path sets the later cleanup's argument to 9,
  while the `RunMenuA` / `RunMenuB` exits use 0.
- The checker drives fade completion and card/input callbacks with deterministic hook outcomes
  to cover the real transition logic. `SaveUi_RunMenuA` selection cases include each valid
  selection and the cancel path; `SaveUi_RunMenuC` includes every observed `StateTick` action.
- The case counts in `tools/verified.csv` were updated from WIP to the final default counts.
  Earlier WIP exclusion rows were narrowed or corrected when the expanded coverage run proved
  their original ranges stale; the final exclusions are the exact registered ranges above.

The generated `extract/checks/results/check_ovl186a.json` reports the 518-case passing run,
coverage, 250,000 instruction budget, and 60 caught mutations. No commit was made.
