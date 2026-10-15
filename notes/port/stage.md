# Stage requests, progress and area loading

`src/world/ae_stage.{h,c}` is the RAM-image port for the fifteen stage-flow
functions listed in `extract/codex-task/task.md`. Function boundaries follow
the original EXE. Calls outside this batch remain named hooks unless a verified
port already covers the callee.

## Instruction call tree

The edges below come from `extract/codex-task/stage_disassembly.txt` (MIPS
`jal` targets), with internal batch edges shown as indented children.

```text
StageRequestUpdate
  StageRequest (dispatches by exit code; can recurse for retry/warp flows)
    StageProgressOnEnter
      AreaGetKind, AreaGetLevel; reset monkey/gimmick/item/enemy/sound state
      GetLevelFirstArea, LevelMonkeyCountGet
    PlayerInit
    AreaLoad
      HeapInit, AreaBumpAllocSet, AreaGetKind/Level, MatrixIdentity
      area feature setters, BGM/audio, fog/render configuration
      AreaLoadGridAndProps
        AreaBumpAlloc, MeshInit, GetZoneMask, MatrixIdentity
        three unresolved data helpers at 80030890, 800A3720, 800A3700
      collision/player/target/enemy/monkey/effects/gimmick/item setup
      AreaTexCopyInit, RcCarDespawn
    StageProgressRestore
      GimmickSaveTableReset, AreaGetLevel, GetLevelFirstArea
      StageProgressOnEnter
      StageProgressRestore_Nop2, StageProgressRestore_Nop
    StageRequestExit -> StageExitStart
    StageProgressStoreArea -> IsTimeAttack, MonkeyStoreActivities,
      GetLevelFirstArea
    AreaLoadStart -> AreaUsesAltMember, ArcGetMemberPos,
      unresolved 80025E24, five CdMgrQueueCommand calls
    AreaLoadStartSeek -> AreaUsesAltMember, ArcGetMemberPos,
      unresolved 80025E24
    AreaLoadQueueRead -> ArcGetMemberPos, four CdMgrQueueCommand calls
    StageProgressOnExit -> GimmickSaveStates, GimmickSaveApplyProgress,
      IsTimeAttack, item/enemy masks, MonkeyStoreActivities,
      GetLevelFirstArea
  StageRequestRetry -> StageRequest
  AreaCheckExits, CdMgrUpdate, FadeFrame, LoadListStep, Stage UI updates
  StageRequestUpdate also calls AreaLoadStartSeek while streaming.

StageProgressStoreArea, StageProgressOnExit and StageProgressRestore preserve
progress snapshots in global tables. The two `StageProgressRestore_Nop*`
functions are empty return stubs in the original. `AreaLoad` reads the blob at
`0x80154000`; its feature entries and terrain layout are documented in
`notes/12-level-collision.md` and `notes/13-areas.md`. Disc commands and member
lookup remain hooks so this port does not simulate CD or archive hardware.
`StageRequest` exit-code meanings are summarized in `notes/04-game-modes.md`.

## Validation

`tools/mips/check_stage.py` compares the hand-written functions against the
instruction oracle on a real area member and records instruction coverage,
case counts, hook calls, RAM writes and mutations. Progress here is listed in
the checker and `tools/verified.csv`; unresolved hardware/subsystem callees
are named in `AeStageHooks` and documented with the final port status.

## Port status and validation

All fifteen requested functions are ported and diffed. Counts are from the
default run (`--cases 1 --seed 94423`):

| Function | Cases | Instruction coverage |
|---|---:|---:|
| `AreaLoadStartSeek` | 5 | 31/31 |
| `AreaLoadQueueRead` | 2 | 55/55 |
| `AreaLoadStart` | 5 | 71/71 |
| `AreaLoad` | 7 | 546/550; four proven unreachable |
| `AreaLoadGridAndProps` | 4 | 175/175 |
| `StageRequest` | 52 | 484/484 |
| `StageRequestUpdate` | 27 | 250/250 |
| `StageRequestRetry` | 1 | 14/14 |
| `StageRequestExit` | 1 | 8/8 |
| `StageProgressStoreArea` | 4 | 104/104 |
| `StageProgressOnExit` | 10 | 205/205 |
| `StageProgressOnEnter` | 12 | 470/470 |
| `StageProgressRestore` | 6 | 93/93 |
| `StageProgressRestore_Nop` | 1 | 2/2 |
| `StageProgressRestore_Nop2` | 1 | 2/2 |

The default checker passes 138 differential cases. Its mutation run generated 40
faulty C variants and caught all 40. `AreaLoad` instructions `80032234` and
`80032244..8003224C` are signed divide-by-zero/overflow traps after unsigned
fog halfwords are loaded and nonpositive ranges replaced by 1. They cannot
execute; the proof is registered in `tools/mips/check_exclusions.csv`. The
checker also declares its stack window. It compares all other RAM and
scratchpad bytes, return values, hook calls and observed hook payloads.

## Instruction findings

* `AreaLoadStartSeek` returns the result of `FUN_80025E24`; this unresolved
  seek operation stays a hook and its return value flows back to the caller.
* The AreaLoad special-area index uses a nine-entry table modulo 9. Music
  preservation copies go to the existing pointer in stage state, and the sound
  copy is skipped when its byte count is zero.
* StageRequest action 8 does not fade music. It clears the transition vectors,
  calls action 32 for level 33, and otherwise calls `FUN_80096638` before
  saving area progress. Action 9 performs the fade and saturating time-station
  counter update.
* Action 10 routes area kind 3 to action 26. Otherwise it may add play time in
  mode 19 or decrement the remaining area count; the decrement depends on the
  current level. Action 13 restores progress, initializes the player, loads
  the area, then clears the transition flags.
* StageRequestUpdate action 5 uses its second exit-check result for the
  action-6 request. For action 6, a negative `LoadListStep` result sets the
  read status to 2 and returns; a later update with status already 2 performs
  action 2 and resets the arc state.
* Several transition tails call `AimRequestClear` and `AimModeLeave`. The
  latter clears `g_AimMode` and plays SFX 5 only when that flag was set.

## Open hook interfaces

These callbacks remain outside the fifteen-function port. Their original
arguments pass through `AeStageHooks`; unresolved symbols remain opaque.

* **Archive, exit and frame flow:** `AreaUsesAltMember` selects the archive
  member variant; `ArcGetMemberPos` resolves a member to its CD location and
  size; `FUN_80025E24` is the unresolved CD seek/result helper;
  `CdMgrQueueCommand` queues a CD command and its original parameter pointer;
  `CdMgrUpdate` advances the CD manager; `LoadListStep` postprocesses the
  completed load-list record and starts the next; `AreaCheckExits` tests the
  player against area exits and returns `destination << 8 | entry` or -1;
  `WaitFrameFlip` waits for the queued display flip; `FadeFrame` advances the
  fade overlay; `VSync` waits for a video-sync tick.
* **Area and saved progress:** `AreaGetKind` and `AreaGetLevel` query the area
  table; `GetLevelFirstArea` and `LevelMonkeyCountGet` query level metadata;
  `IsTimeAttack` tests the active time-attack mode; `ProgressSetByte` writes a
  save-progress byte; `GimmickSaveStates` snapshots persistent gimmicks;
  `GimmickSaveApplyProgress` applies their progress records;
  `GimmickSaveTableReset` resets gimmick-save entries; `ItemSaveCollectedMask`
  and `ZakoSaveDefeatedMask` store collected-item and defeated-enemy masks;
  `ItemsClearFlags` and `ZakoClearSlotTable` clear those per-area records;
  `SoundSilence` stops sound and restores volume.
* **Memory, geometry and area configuration:** `HeapInit` resets the heap;
  `AreaBumpAlloc` allocates area memory and `AreaBumpAllocSet` sets its cursor;
  `MeshInit` initializes a prop mesh; `GetZoneMask` computes a visibility-zone
  mask; `FUN_80030890` is an unresolved prop-rotation helper;
  `FUN_800A3700` is an unresolved model-over warning and `Printf` is the
  diagnostic printer; `AreaSpecialFlagsClear` clears area special flags;
  `AreaSetVramAnimList`, `AreaSetZoneBoxes`, `AreaSetFeature30`,
  `AreaSetFeature31`, `AreaSetFeature32`, `AreaSetSpecialActor`,
  `AreaSetIndicatorModelA`, `AreaSetIndicatorModelB`, and `IndicatorsReset`
  configure area-header features; `ApplyLevelFog` configures hardware fog;
  `SetColorMatrixMode` changes the lighting matrix;
  `BuildTerrainBlockMask` builds terrain visibility;
  `TableClearDAT_800BEF60` clears two renderer words (tentative symbol meaning).
* **Audio:** `SoundAreaKeepsMusic` decides whether a transition preserves BGM;
  `BgmStart`, `BgmUpdateVolume`, and `BgmFadeOut` control the BGM voice;
  `SoundStop` stops the requested audio banks; `SoundLoopsStopAll` clears
  eligible loop voices; `PlaySfx` starts an SFX table entry; `XaPlay` starts an
  XA stream.
* **Player and area systems:** `PlayerInit` initializes player state;
  `PlayerSpawnAtEntry` positions the player and camera at an entry;
  `PlayerSetMode` selects player behavior; `PlayerSelectGadget` selects a
  gadget; `HudFindGadgetIconPos` positions its HUD icon;
  `PlayerEnterCutsceneMode` enters the cutscene state; `MonkeyClearWords`
  clears a word range; `MonkeyLodModelsInit` loads monkey LOD models;
  `FxSlotsReset`, `ProjectilesReset`, and `EffectsReset` clear runtime pools;
  `ColliderZoneMasksUpdate` updates collider visibility; `GimmickSpawnAll`
  restores area gimmicks; `BossInit` calls the area's boss initializer;
  `ZakoInit` initializes the area enemy system; `TargetListClear` clears its
  target pointer list; `ItemsLoadForArea` loads item records;
  `AreaTexCopyInit` saves a VRAM rectangle; `RcCarDespawn` unregisters an
  existing RC car.
* **Transition, cutscene and UI behavior:** `StageExitStart` starts an exit
  request when allowed; `CutsceneModeOff` and `CutsceneModeOn` clear/set the
  cutscene flag; `FUN_80028850` and `FUN_800286CC` are unresolved stage-context
  helpers; `PlayTimeAdd` adds play time; `StageGradeUpdate` updates the caught
  monkey grade; `StageVictoryStart` starts the victory camera and fades BGM;
  `StageTypeClearInit` and `StageTypeClearUpdate` initialize/advance the clear
  sub-state machine; `StageIntroStart` and `StageIntroUpdate` initialize and
  advance the intro; `GimmickNopUpdate3` and `GimmickNopUpdate4` are empty
  update stubs; `MessageBoxClose` clears the active message box;
  `DebugMenuInitPlayerParams`, `DebugMenuInitCounters`, and
  `DebugMenuInitCounters2` bind debug-menu values; `FUN_80096638` is an
  unresolved progress helper; `FUN_8013EA24` is an unresolved overlay action.
* **Aim cleanup:** `AimRequestClear` clears the pending aim-request byte when
  set; `AimModeLeave` clears aim mode and conditionally plays SFX 5.
