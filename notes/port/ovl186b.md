# Overlay member 186: save-menu functions, part 2

## Port and validation

The hand-written port is in [ae_ovl186b.c](../../src/modes/ae_ovl186b.c) and
[ae_ovl186b.h](../../src/modes/ae_ovl186b.h). The differential checker is
[check_ovl186b.py](../../tools/mips/check_ovl186b.py). It loads `186.bin` at `0x80136000` over
the real EXE RAM image and loads real area members for area IDs 1, 2, 5, 19, 36, and 44
(members 091, 094, 102, 288, 291, and 190). It compares returns, all RAM outside the declared
stack window, scratchpad, and ordered hook calls. The default run passes 102 cases in about 13
seconds. Mutation mode catches 36/36 mutations, two per ported function.

There are no undeclared coverage gaps or RAM masks. The 446 instructions not run are registered
native packet-building ranges or invalid-state backedges. All other instructions in the 18 C
functions were covered.

| Original function | Address | Cases | Covered / total | Excluded | Status |
| --- | ---: | ---: | ---: | ---: | --- |
| `SaveUi_DrawDetail` | `801391B0` | 4 | 41 / 175 | 134 renderer packet instructions | ported, diffed |
| `SaveUi_LastUsedEntry` | `801399E4` | 4 | 22 / 22 | — | ported, diffed |
| `SaveUi_DrawMainWindow` | `80139A3C` | 1 | 11 / 11 | — | ported, diffed |
| `SaveUi_DrawBackground3D` | `80139A68` | 4 | 162 / 162 | — | ported, diffed |
| `SaveUi_YesNoUpdate` | `80139F08` | 5 | 48 / 48 | — | ported, diffed |
| `SaveUi_FileSelectUpdate` | `80139FC8` | 21 | 270 / 270 | — | ported, diffed |
| `SaveUi_ConfirmPressed` | `8013A400` | 4 | 15 / 15 | — | ported, diffed |
| `SaveUi_CancelPressed` | `8013A43C` | 4 | 15 / 15 | — | ported, diffed |
| `SaveUi_FadeTick` | `8013A634` | 7 | 54 / 54 | — | ported, diffed |
| `SaveUi_OnCardResult_7` | `8013B400` | 10 | 31 / 31 | — | ported, diffed |
| `SaveUi_ConfirmDialogUpdate` | `8013B47C` | 3 | 53 / 53 | — | ported, diffed |
| `SaveUi_NameEntryInit` | `8013B550` | 3 | 93 / 93 | — | ported, diffed |
| `SaveUi_NameEntryUpdate` | `8013B6C4` | 18 | 360 / 360 | — | ported, diffed |
| `SaveUi_NameEntryDraw` | `8013BC64` | 5 | 234 / 542 | 308 renderer packet instructions | ported, diffed |
| `CardMgr_Run` | `8013C7F0` | 3 | 58 / 62 | 4 invalid-state backedge instructions | ported, diffed |
| `CardMgr_Init` | `8013C8E8` | 2 | 27 / 27 | — | ported, diffed |
| `CardMgr_RunFileList` | `8013C954` | 2 | 119 / 119 | — | ported, diffed |
| `CardMgr_RunFileDetail` | `8013CB30` | 2 | 123 / 123 | — | ported, diffed |

Six listed originals contain native rendering only and have no C ledger row: `SaveUi_DrawIcon`
(`8013946C`) emits icon packets and advances the packet cursor; `SaveUi_DrawPortrait`
(`80139570`) emits save-slot portrait quads; `SaveUi_DrawCursorAnim` (`80139CF0`) builds the
animated cursor packets; `SaveUi_DrawFileIcons` (`8013A478`) draws slot icons and queries
`SaveUi_LastUsedEntry` for the used-entry marker; `SaveUi_FadeDraw` (`8013A70C`) emits the fade
primitive; and `SaveUi_DrawModel` (`8013A804`) performs GTE model projection and primitive
emission. The host renderer should provide these named hooks.

## Call tree

```text
SaveUi_DrawDetail
├─ SaveUi_DrawSlot (earlier overlay range)
├─ SaveUi_DrawPortrait (native renderer hook)
└─ SaveUi_DrawDetailPanel (inline packet block replaced at 80139240)

SaveUi_DrawBackground3D
├─ SaveUi_LookAt (earlier overlay range), SetGeomScreen
├─ MatrixIdentity / RotMatrixX,Y,Z and rsin / rcos (direct verified SDK ports)
├─ MeshTransform / ModelEmitPrimsAlt (renderer hooks)
├─ SaveUi_DrawModel (native renderer hook)
└─ ae_StepToward

SaveUi_YesNoUpdate ─ SaveUi_DrawYesNo, ConfirmPressed, PlaySfx
SaveUi_FileSelectUpdate ─ ConfirmPressed / CancelPressed, PlaySfx, MemCardOp, SaveUi_SetState
SaveUi_ConfirmDialogUpdate ─ DrawStateOverlay, YesNoUpdate, MainWindow, Background3D,
                             MemCardOp, SaveUi_SetState
SaveUi_NameEntryInit ─ SaveUi_GetBuffer2; copy the selected or default name into the editor
SaveUi_NameEntryUpdate ─ PlaySfx; update the name buffer, page, cursor, and repeat movement
SaveUi_NameEntryDraw ─ DrawMenuArrows, CardMgr_DrawMenuTexts, DrawText, DrawIcon,
                       name panel/footer packet hooks, MainWindow, Background3D

CardMgr_Run ─ InitDisplay / ResetGraph, CardMgr_Init, CardMgr_RunMessage
├─ CardMgr_RunFileList
└─ CardMgr_RunFileDetail
   FileList / FileDetail ─ PadUpdate, BeginFrame, FadeTick, CardMgr_StartOps,
                           CardMgr_IsBusy, thread hooks, display SDK hooks, MemCardOp
                           FileDetail also calls ae_SaveHeaderFill
```

## Hook boundaries

The six rendering-only originals above remain named host hooks. Inline graphics work is also
replaced at `80139240` (`SaveUi_DrawDetailPanel`), `8013BF44` (`SaveUi_NameEntryPanel`), and
`8013C43C` (`SaveUi_NameEntryFooter`). The checker compares the game-computed selection, name
page, cursor, text positions, icon positions, and packet cursor effects around these ranges.
`MeshTransform`, `ModelEmitPrimsAlt`, and `SaveUi_DrawModel` carry the computed model and matrix
data to the renderer.

Calls outside this member remain named hooks: `SaveUi_DrawSlot`, `SaveUi_LookAt`, `SaveUi_DrawText`,
`SaveUi_DrawYesNo`, `SaveUi_SetState`, `SaveUi_GetBuffer2`, `CardMgr_DrawMenuTexts`,
`SaveUi_DrawStateOverlay`, `CardMgr_StartOps`, `CardMgr_RunMessage`, `CardMgr_IsBusy` (wrappers at
`8013D1F8` and `8013D218`), and `SaveUi_ThreadSetHook` / `SaveUi_ThreadSwitch` /
`SaveUi_ThreadClose`. They respectively draw earlier-range UI, provide the save-menu state and
buffer operations, start card work, report card state/busy status, and manage the card worker
thread. `PlaySfx` handles audio; `MemCardOp` handles BIOS card requests; `ProgressChecksum` provides
the progress checksum.

Display and input APIs remain platform hooks: `InitDisplay`, `ResetGraph`, `BeginFrame`, `DrawSync`,
`VSync`, `PutDispEnv`, `PutDrawEnv`, `DrawOTag`, `ClearImage`, `SetGeomScreen`, and `DrawMenuArrows`.
`PadUpdate` calls the already verified `ae_pad_PadUpdate`, with pad hardware callbacks supplied by
the host. `ae_SaveHeaderFill`, `ae_StepToward`, and the matrix/trig routines are called directly.

## Findings

- Name-entry page movement has two different maps. Buttons `0x08` and `0x04` lead to different
  adjacent pages and update different grid markers; treating them as one transition table changed
  RAM. Name pages 0 and 1 use 15 columns with a gap after every fifth cell, while pages 2 through
  4 use 11 columns.
- In confirmation mode, `SaveUi_FileSelectUpdate` moves the selected save slot on repeat input
  while it waits for confirm/cancel. This path wraps through slots 0 to 3.
- File detail calls the busy wrapper at `8013D218`; file list calls its sibling at `8013D1F8`.
- `SaveUi_OnCardResult_7` dispatches through the jump table at `8013C4E0`.
- `NameEntryInit` reads its caller's saved `$s0` when forming the default-name character. The test
  seeds that same caller value on both sides and registers it as `shared_random`.
- `CardMgr_Run` initializes its state to 0. Only states 1 and 2 dispatch workers; both worker
  functions reset it to 0. The remaining backedges are for invalid values below 0 or above 2 and
  are registered with that state-domain proof.

The source scan in `tools/check_verified.py` picks up this new module automatically. The 18 diffed
rows are appended to `tools/verified.csv`; the renderer/invalid-state exclusions are documented in
`tools/mips/check_exclusions.csv`. No inventory file or commit was changed.
