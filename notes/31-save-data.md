# Save data and progress (live save block)

Static reading of `SaveHeaderFill` (0x80095D44), `SaveBufferFill` (0x80095B3C), `SaveRestoreState` (0x80095E80) and
`SaveNewGameInit` (0x80095960); struct `SaveBlock` in `tools/structs.txt`, applied at `g_SaveBuffer` (0x800E0018).
Field meanings follow the globals each field is copied from / to; names marked tentative there are guesses.

The live block is **0x7A0 bytes**. `SaveRestoreState` copies 0x7A0 bytes from `g_SaveSlotImages` (0x800E1A00) +
`g_SaveSlot` * 0x3D0 into it and restores the globals; `SaveBufferFill` captures the globals into it (when not in time
attack mode it also captures the gimmick states and the per level tables); `SaveBuildImage` / `SaveComputeChecksum`
build the memory card image (notes/04 mode 14 is the save / load screen, ovl 0x186).

| Offset | Size | Field | Source / meaning |
|---|---|---|---|
| 0x000 | u16 | resumeArea | `GetLevelFirstArea(g_CurrentLevel)`: the game resumes at the first area of the level |
| 0x002 | u8 | lives | `g_Lives` (5 in a new game) |
| 0x003 | u8 | unk_3 | 0x800F44B8 |
| 0x004 | u32 | gadgetUnlockMask | `g_GadgetUnlockMask` (bit n = gadget id n, notes/08) |
| 0x008 | u8[4] | gadgetSlots | `g_GadgetSlots` (gadget ids on triangle / square / circle / cross, 0xFF = empty) |
| 0x00C | u8 | ammo2 | `g_SlingAmmo2` |
| 0x00D | u8 | ammo1 | `g_SlingAmmo1` |
| 0x00E | u8 | currentGadget | equipped again with `PlayerSelectGadget` on restore |
| 0x010 / 0x011 | u8 | unk_10 / unk_11 | 0x800F44A6 / 0x800F44A4 |
| 0x014 | u32 | options | bit 0 = 0x800F448A (vibration?), bit 1 = 0x800F4488; bytes 2 / 3 = the two volumes (`SoundGetVolumes`) |
| 0x018 | u8[16] | unk_18 | cleared in a new game |
| 0x028 | u8[32] | progressA | `g_LevelPlayMode` (play mode saved per level) |
| 0x048 | u8[32] | progressB | `g_LevelCollected` (per level count, see `LevelGetInfo`) |
| 0x068 | u16[128] | areaRecords | `g_SavedAreaRecords` (0xFFFF = none) |
| 0x168 | 3 x 0x84 | slotInfo | `SaveSlotInfoPtr(slot)` records |
| 0x2F4 | u8[752] | gimmickStates | `g_GimmickStates` (`GimmickSaveStates`) |
| 0x664 / 0x668 | u32 | unk_664 / unk_668 | 0x800DFDCC / 0x800DFDD0 |
| 0x66C | u32[8] | unk_66c | 0x800DFDD8 |
| 0x68C | u32 | unk_68c | 0x800DFDF8 |
| 0x690 | u32[32] | timeRecords | `g_TimeRecords` (-1 = none; time attack) |
| 0x710 | u32 | unk_710 | 0x800F44A8 |
| 0x714 / 0x716 | u16 | hubDestB / hubDestA | the hub exit destination (`LeaveHubStoreDestination`, 0x800F461D / 0x800F461C) |
| 0x718 | u32 | unk_718 | 0x800DFDFC |
| 0x794 | u32 | unk_794 | cleared in a new game |

Per level info (`LevelGetInfo(level, out)`, 0x20 levels): +0 best score / time (`LevelBestScoreGet`), +4 collected
count (`g_StageCollected` in the stage, else `g_LevelCollected[level]`), +6 / +0xA bytes of the level table at
0x80102400 + level * 0x10, +8 monkey count (`LevelMonkeyCountGet`), +0xC `g_PickupCounter` (in the stage), +0xE caught
total (summed over levels 0x18-0x1D for Monkey Madness), +0x10 progress (`LevelProgressGet`). The level ids and their
stage names are in notes/28.

Open: the meaning of the unk fields and of `g_SavedAreaRecords`, and the memory card file layout around the 0x7A0
byte image (checksum in `SaveComputeChecksum`).
