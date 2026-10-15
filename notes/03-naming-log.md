# Naming log

Source of truth for names: `tools/symbols.csv`, applied with
`tools/ghidra_scripts/ApplySymbols.java`. Inspect with
`tools/ghidra_scripts/ExportDecomp.java <out> <depth> <max> <roots...>`, which writes to
`notes/decomp/` (gitignored because it contains decompiled game code).

Struct types live in `tools/structs.txt`, applied with
`tools/ghidra_scripts/ApplyStructs.java <structs.txt>` (replaces existing definitions,
then retypes the `apply` addresses).

Typical headless call (needs the project closed in the GUI):

```
& '<repo>\tools\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat' '<repo>\ghidra-projects' ApeEscape -process SCUS_944.23 -noanalysis -scriptPath '<repo>\tools\ghidra_scripts' -postScript ApplySymbols.java '<repo>\tools\symbols.csv' -postScript ExportDecomp.java '<repo>\notes\decomp\out.c' 2 50 <addr...>
```

## 2026-10-05 — overlay map verification

`tools/verify_overlay_map.py` reads the tables straight from `SCUS_944.23`:
- Mode table 0x800AFA30: 37 modes. 16 distinct overlay members (0x180–0x18B,
  0x1AB–0x1AF), all at 0x80136000. Entries with member 0xFFFFFFFF are resident code.
- Area records 0x800A71E0..0x800A8190: 31 overlay members (0x18C–0x1AA), all at
  0x8013D000, postprocess 0. Each area is 5 records: a code overlay plus data members
  loaded to 0x80154000 (post 1) and 0x80146000 (post 2).
- Area pointer table 0x800A81B0: 51 entries of {list ptr, id}, ids 0x10001..0x70011
  (look like world<<16 | stage).
- 0x185 and 0x1B0 aren't referenced by either table.

Result: all 47 established load addresses match, 0 conflicts.

## 2026-10-05 — batch 1+2: boot path (36 symbols)

Top-down from `start` (0x800A3660): clears BSS 0x800B4C60..0x800F760C, then calls `main`.

**main loop** (`main` 0x8005BDC0): `g_GameMode` indexes `g_ModeTable`. If the member is
not -1, `ArcLoadMember(member, 0x80136000)` and wait. Mode 0xD also runs an area load
list first. Then it calls the entry; a negative return exits.

**Archive.** `ArcInit` finds `\KKIIDDZZ.HED;1`, reads one sector (`CdRead(1, ..., 0x80)`) into
`g_ArcIndex`, and adds HED LBA + 1 to the first 0x1C0 descriptors only. That's why
descriptors 0x1C0+ are absolute LBAs. It then caches STR start positions for
members 0x1C0–0x1C5 into the stream table at 0x800A8530 (12-byte entries).

**Load lists.** `LoadListStart(kind, id)` → `LoadListStep` repeatedly. Each record is
`{member, postprocess, dest}`. `LoadListPostprocess` handles: 1 = graphics upload
(0x80068D10 + DrawSync), 2/3/4 = sound bank modes via 0x80086CAC.

**CD manager.** A 16-entry command ring (0x1C-byte entries at 0x800E3908), driven by
`CdMgrUpdate` once per frame. States are in `g_CdMgrState`. The debug strings
("CdManage: Read Retry", "Cdlib:DiskError", "XAmanage") confirm the names.

**Mode_Stage** (0x8005BF70) is the per-frame stage loop. It calls about 40 subsystem
updates (unnamed yet), double-buffers through `g_RenderBuffers`, and handles pad
START (0x800) / SELECT (0x100) for pause. On exit it switches on `g_StageExitCode`
to set the next `g_GameMode`.

## 2026-10-05 — batch 3: Mode_Stage subsystems (48 more, 84 total)

Read all ~60 direct callees of Mode_Stage (`notes/decomp/stage-subsys.c`). Highlights:

- **Input:** `PadUpdate` builds `g_PadHeld` / `g_PadChanged` / `g_PadRepeat` for port 1
  (pressed = held & changed). It also keeps a 40-sample analog history
  (`g_StickHistory`) and classifies stick rotation/flick gestures. Those results go to
  0x800E5498.. and get copied into a scratchpad input block (0x1F800054..) by `PlayerUpdate`.
- **Frame pipeline:** `BeginFrame` (flip buffer index, ClearOTagR 0x1000) → subsystems
  add prims via `g_PrimAlloc` into `g_OrderingTable` → `QueueFrameFlip` hands
  DRAWENV/DISPENV/OT to the VSync callback; `WaitFrameFlip` blocks on it.
- **Camera:** `CameraUpdate` → eye/target (`g_CameraEye`, `g_CameraTarget`) → look-at
  into `g_ViewMatrix` (0x800E78C0, the matrix nearly every renderer loads).
- **Stage flow:** `StageRequest(code, area, arg)` is how gameplay asks for transitions.
  `StageRequestUpdate` streams the new room. `StageFadeUpdate` runs fades and ends
  the stage loop.
- **Sound:** `PlaySfx(id)` (122 callers), `SoundEmitterUpdate` (24 positional emitters),
  `SoundBankUpload`, `SoundStop`, `BgmUpdateVolume`.
- **Misc:** `RunMinigameExe` Exec()s the minigame EXEs and re-inits on return.
  `EffectScriptUpdate` is a small bytecode VM for effect emitters. `DrawTextSprites` is
  the 16x16 font. Pause menu: START → `OpenPauseMenu(1)`, SELECT → `OpenPauseMenu(10)`.

Still unnamed at this level (need deeper reads): 0x80043A8C (5 KB GTE renderer,
probably level geometry), 0x80042E18 (8 slots of 0x250 at 0x800E5500 — likely monkeys),
0x8007247C (object/collision pass), 0x80016F34, 0x80063628 (alternate player
update), 0x8006A9F8 (calls 10 updaters), 0x800719F8, 0x80088AB8, 0x8005E0F0, 0x8007D530.

## 2026-10-06 — structs and SDK inventory

- `Monkey` struct (notes/05-monkeys.md) and `Player` struct (notes/08-player.md) in
  `tools/structs.txt`. `g_CurrentGadget` became `g_Player.currentGadget`; ApplyStructs
  removes old labels that fall inside an applied struct.
- SDK / hardware inventory: `notes/06-sdk-inventory.md` (generated) and
  `notes/07-port-scope.md` (interpretation). The game uses 134 SDK functions and
  has its own SPU sound driver (no libspu).

## 2026-10-06 (autonomous session) — batch 14: collision shape handlers

10 `CollShape_*` handlers named from `g_CollShapeHandlers` plus `CollHalfPipeHeight`,
`DiagonalSignedDistance`; shape record layout validated on all areas (notes/12, notes/13).

## Open questions / next

- Identify each mode table slot: run in PCSX-Redux with a write breakpoint on
  `g_GameMode` (0x800F4470) and note which screen is active. That's faster than
  static reading, because the mode overlays have almost no strings.
- The ~40 per-frame calls in Mode_Stage are the game's subsystems (objects, camera,
  collision, monkeys, gadgets, HUD, sound). Name them next, starting with the ones
  that have strings (0x80069BEC "LoopDepth error" looks like a script/command interpreter).
- Pad globals 0x800E5478 / 0x800E5480 (held vs pressed TBD).

## 2026-10-15 — batch 113: collider-script table handlers

Named three code targets reached by the collider motion table and added their
entry points to `tools/symbols.csv`: `ColliderScriptFollowPath` at 0x800892C4
(route interpolation and body/prop carry), `ColliderScriptPlayerMove` at
0x8008A4B0 (player and RC-car flow motion), and `ColliderScriptCarry` at
0x8008A91C (linked-body carry and collider state updates). Their function ends
are established from the next table targets / `jr $ra` and delay slots; all
three are differentially covered in `notes/port/misc_batch.md`.
