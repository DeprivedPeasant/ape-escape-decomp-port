# Progress, save, hub, and pause helpers

The 26 main-EXE functions in this batch are readable RAM-image ports in
`src/world/ae_progress.c` / `.h`. The port keeps the original entry points and
uses named globals and record offsets. Calls to already ported code remain
direct; calls outside this batch use `AeProgressHooks` or the existing sound
hardware hook table.

## Functions

Every entry is **ported and diffed** by `tools/mips/check_progress.py`.
Per-function counts below are the default checker counts recorded in
`tools/verified.csv`.

| Address | Function | Cases |
|---|---|---:|
| 800119F4 | SaveSlotInfoPtr | 8 |
| 80016E68 | MeshAnimAdvance | 8 |
| 8006D548 | SpriteEffectQueue0 | 8 |
| 8007292C | SoundLoopsPause | 8 |
| 8007F400 | ClearDAT_800BEF70 | 8 |
| 8007F56C | OpenPauseMenu | 6 |
| 8008227C | SetDAT_800BEF80 | 8 |
| 80082288 | DrawTextSprites | 10 |
| 800862E4 | SoundSetVolumes | 10 |
| 80086428 | SoundVolumesDefault | 8 |
| 800864B4 | SoundGetVolumes | 8 |
| 800864DC | SoundSaveVolumes | 8 |
| 800864FC | SoundRestoreVolumes | 8 |
| 80095B3C | SaveBufferFill | 8 |
| 80095CEC | SaveCaptureState | 6 |
| 80095D44 | SaveHeaderFill | 8 |
| 800961D0 | RecordBestTime | 10 |
| 8009628C | SaveBlockPtr | 8 |
| 800963DC | ProgressMarkStageResult | 12 |
| 80096514 | AreaKindVisitedGet | 8 |
| 80096580 | LevelProgressGet | 8 |
| 800976B8 | HubDestinationState | 12 |
| 800977BC | HubGateLocked | 10 |
| 80097820 | UnlockSpecialArea | 7 |
| 800978E0 | HubGateBitTest | 8 |
| 800978FC | ExitSpecialArea | 10 |

## Calls and hooks

The checker loads the retail EXE and real area members `05B`, `05E`, `066`,
`120`, `138`, `129`, and `135` (areas `01`, `02`, `05`, `13`, `58`, `63`, and
`67`). It compares returns, hook and hardware event order, all main RAM outside
the declared stack window, and the complete scratchpad.

Direct calls to existing verified ports include `SaveBlockPtr`,
`SpriteEffectQueue`, `SoundLoopStop`, `CutsceneModeOff`, `BgmSeqSetVolume`,
`GetLevelFirstArea`, `IsTimeAttack`, `GimmickSaveStates`,
`GimmickSaveApplyProgress`, `AreaGetKind`, `AreaGetLevel`, and
`GadgetUnlockMask`.

The named calls outside this batch stay hooks:

| Hook | Original behavior |
|---|---|
| `SoundLoopsSetPan` (`80086788`) | Walks the 24 sound-loop records and asks the sound subsystem to update active loop voices for the requested pan. `SoundLoopsPause` calls it with zero. |
| `FUN_8013FB20` | Area-overlay routine called only when the current level is `0x1B`; overlay code is outside the EXE port. |
| `SoundBankSetVolume` (`800A2640`) | Applies a volume to a loaded sound bank and its tone data. |
| `SetDrawTPage` (`8002B410`) | PsyQ SDK helper that writes the draw-page GPU command into the supplied packet. `DrawTextSprites` passes `(packet_cursor, 0, 1, 54)`. |
| `ProgressFlagSet` (`800962F4`) | Sets a bit in the saved progress flag array. |
| `ProgressSetByte` (`80096360`) | Replaces one byte in the packed saved-progress words. |

`SoundLoopStop` and `BgmSeqSetVolume` keep their original `AeSoundHw` boundary.
The differential model records `SpuWaitIdle`, `SpuWaitLoop`, key-off, ADSR, and
voice-volume effects without emulating the SPU device.

## Findings from the instruction comparison

* `MeshAnimAdvance` freezes on flag bit 0. When the next frame passes the end
  marker, a clear flag bit 1 restarts at frame one; a set bit 1 keeps the
  current frame.
* `OpenPauseMenu` stores request byte `1` in both branches. Request `1` also
  sets pause state `0x0F` and calls `CutsceneModeOff`; other values set state
  `1`.
* `DrawTextSprites` uses globals at `0x800EC354`, `0x800EC35C`, and ordering
  table base `0x800F03D0`. Its `lwl` / `swl` sequence links the low 24 bits of
  the packet address while preserving the packet-length byte. The C port writes
  those named tag bytes explicitly and calls the SDK helper even for empty
  strings.
* `SaveBufferFill`'s time-attack branch copies time records and jumps to the
  epilogue. It skips the progress words, gate flags, visited-area word, and
  auxiliary word copied by the regular-save branch.
* `RecordBestTime` uses the first time-attack record slot (index 24) for every
  time-attack level. The listing checks whether the level is in 24..29, then
  reads and writes at `0x800DFD40 + 96`; the level value does not select a
  different record within that branch.
* `SaveHeaderFill` takes the high ammo byte from `0x800EC2D2`; the option bit
  tests use `0x800F448A` for bit 0 and `0x800F4488` for bit 1.
* `HubDestinationState` returns `3` when the destination level has progress
  value 2 and the exit code is `0x800`. Its kind-level mismatch path returns
  `1`.
* `ExitSpecialArea` maps area `0x5E` to area `20` and game mode `8`. The three
  instructions that choose mode `18` cannot execute: that path requires exit
  code `0x800`, but the caller loads the saved source area with `lh`, which
  sign-extends `0x0800` to `-2048`. This proof is registered in
  `tools/mips/check_exclusions.csv`.

## Validation

`.venv/Scripts/python.exe tools/mips/check_progress.py` passes **221
differential cases**. `--coverage` reports **858 / 861** original instructions
executed; the remaining three are the proven unreachable `ExitSpecialArea`
mode-18 path described above. `--mutate` builds two behavior-level source
mutants per function (wrong conditions, values, offsets, or argument use) and
checks each against the retail instructions using the default differential
cases. All **52 / 52** are caught. The `SoundLoopsPause` scenario seeds an
active loop record so the wrong sound-ID mutant changes saved loop state.
`tools/check_verified.py` checks the ledger and result record. The new source
file is already covered by its automatic `src/**/ae_*.c` scan, so
`tools/check_verified.py` needed no change.
