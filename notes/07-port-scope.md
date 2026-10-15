# Native port scope (from the SDK inventory, 2026-10-06)

Data: `notes/06-sdk-inventory.md` (generated). This page is the interpretation.

## What a PC build has to replace

| Area | What the game uses | Port approach |
|---|---|---|
| Graphics (libgpu) | 23 functions, 459 call sites. Mostly the double-buffer / OT / VRAM upload set: DrawSync, Put{Disp,Draw}Env, DrawOTag, ClearImage, LoadImage, SetDrawMove, MoveImage, StoreImage | Implement PS1-GPU semantics on top of a modern API: VRAM as a 1024x512 16-bit texture, walk the ordering table, rasterize the GPU packets the game builds. That's the standard approach of PS1 PC ports and HLE renderers. |
| Geometry (libgte + inline GTE) | 31 libgte functions (889 sites) **and** ~1100 inline COP2 instructions in 91 game functions | A GTE implementation in C (well documented; the recomp and emulators have reference behaviour). Inline GTE means the decompiled C keeps `gte_*` macros that call it. A later option is a float rewrite per renderer for widescreen/precision. |
| Timing / interrupts (libetc, libapi) | VSync (111 sites), VSyncCallback, DrawSyncCallback, events, Exec | Frame loop driven by the host. VSync = wait for next 1/60 s tick; callbacks run at frame boundaries. `Exec` (minigames) becomes a direct call into the minigame code. |
| CD (libcd + St* streaming) | 21 functions, 100 sites, plus the game's own CD manager | Replace at the game's CdMgr / ArcLoadMember layer with file reads from the extracted disc data. STR/XA streaming (St*) needs an MDEC/XA decoder (libpress usage) or pre-converted movies. |
| Sound | **No libspu, almost no libsnd.** The game has its own SPU driver (~80 functions, ~21 KB at 0x8009D000–0x800A3700) that writes SPU and reverb registers directly | Either emulate the SPU (as emulators do) underneath the original driver, or decompile the driver and retarget it. Emulating the SPU is lower risk for accuracy. |
| Pads (libpad) | 9 functions, 38 sites, DualShock analog + vibration | Map to SDL/XInput. Analog sticks and rumble already exist in the game, so this is a clean mapping. |
| Memory card (libmcrd, libcard, libmcx) | 11 + 3 + 7 functions | Replace with save files. libmcx is the PocketStation API (only in overlays), so stub it. |
| libc | memset, printf, memcpy, exit, srand | Host libc. |
| Scratchpad | 4107 refs from 162 functions | Keep a 1 KB static buffer; just fast RAM on PS1. |

## Overlays

The 49 code overlays all load to two fixed addresses. In a native build they become
ordinary linked code: the "load overlay" step turns into "select function table".
`ovl_186` (save menu) statically links its own copy of ~200 SDK functions (memory card,
PocketStation, MDEC). Those collapse into the shared implementations.

## Order of work this suggests

1. Decompile the platform-independent core first (main loop, modes, Mode_Stage
   subsystems), with the SDK calls left as calls.
2. Write a "PSX SDK shim" library with these 134 functions over SDL2 + a GPU-packet
   renderer + a GTE implementation. Hook the CD layer to the extracted files.
3. SPU emulation under the game's own sound driver.
4. Movies (MDEC/XA) last; can be pre-converted.

## Sound data and driver (2026-10-06 night session)

- 43 archive members are **sound banks** with the container `SShd` (header/tone tables) + `SSsq`
  (sequences) + raw SPU ADPCM (`tools/dump_sound.py`, `notes/18-sound-banks.md`; all offsets validated).
  Each bank has one or two sample sets (≈33–35 / 2–9 samples) plus sequence data; `00B`/`01E` are
  header-only members. Decoding to PCM works (placeholder 22050 Hz; the true rates are in the tone tables).
- API layers: `PlaySfx` / `PlaySfxAt` / `SoundLoopStart` → `SfxVoiceStart(Ex)` → `SfxVoiceAlloc` (24 voice
  slots) with tone lookup in the SShd tables (`SfxLookupTone`); BGM: `BgmSeqOpen` / `BgmSeqSetVolume`
  (24 sequence slots, driven by the sequencer opcode handlers at 0x8009D79C–0x800A0E84, called from the
  sequencer core around 0x8009BA90–0x8009CE18); uploads: `SoundBankLoadHeader` → `SpuUploadEnqueue`.
- For the port: decode the SShd tone tables and SSsq event streams (MIDI-like, `FF 2F 00` end-of-track
  markers were seen inside the header tables) and replace the driver with a software mixer, or emulate the
  SPU under the original driver. (Done since: tone tables decoded in notes/24, songs in notes/23, and the port uses a
  software player, `tools/native_audio/`, notes/30. The port also uses SDL3 + OpenGL rather than the SDL2 shim
  sketched above; status in notes/HANDOFF.md "Native port".)
