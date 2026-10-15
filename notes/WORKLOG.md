# Work log

## Session start 2026-10-06 03:17
Start commit 3c097c2, 2104 unnamed. Working queue top-down from HANDOFF.

- Queue 1 (collision shapes) DONE: 10 handlers named, shape records validated (count u16 + 0x14 records, type = low byte of +0x12). dump_areas.py now tabulates types. Unnamed 2104 -> 2101 (+ 10 created from table).
- Queue 2 (terrain tile bytes) DONE: cell low16 = render attrs (draw/near/mirror/tile/orient), cellTiles = colour indices (validated), 16 areas (flags&3==3) have junk grids. dump_areas.py extended. TPAGE 0x18 texture source not yet traced.
- Queue 3 (OBJ exporter) DONE: tools/export_area_obj.py, all 51 areas export. Corner colour order (t0,t1,t6,t7) and collider y extent tentative; no visual check done (no viewer).
- Queue 4 (model exporter) mostly DONE: prim record layouts decoded + validated (643 actor models, 4729 meshes all valid), tools/export_model.py scan/export, notes/16-models.md. OPEN: which actor model is player/monkey (identify via ModelInit callers' blob loads), animation frames not exported, textures (TPAGE/CLUT -> VRAM) not handled.
- Queue 5 (area feature ids) + 10 (type 0x22 indicators) DONE at survey level: 0x08 zone boxes (fully decoded), 0x10/11 indicator models, 0x20 VRAM anim list, 0x30-32 and 0x40/41 only tentative. Unnamed 2085.
- Queue 6 (monkey AI states) DONE at structural level: dispatch table = code blocks calling handlers; 15 handlers + 10 helpers named, notes/17 generated. 8003dc7c (patrol, 5 KB) and 8003cd84 not decoded. Unnamed 2060.
- Queue 7 (gadgets): static identification only (notes/08 tentative table); AimMode/StopGadgetSounds/RcCarSpawn named. Needs a runtime check to confirm ids 3-6, 8.
- Helper sweep 1: tools/unnamed_by_callers.py added; 16 helpers named. Unnamed 2040.
- Queue 11 (sound) partly DONE: bank container (SShd/SSsq/SSvb-raw) decoded and validated on 43 members, dump_sound.py (+WAV export, rate placeholder), 15 driver API functions named. NOT done: SShd tone table layout, SSsq opcode set, sequencer functions 0x8009B...-0x800A0E84. Unnamed 2025.
- Queue 8 (camera): survey only (notes/19-camera.md), 6 functions named. Slot layout / follow logic undecoded. Unnamed 2019.
- Textures: tools/dump_tims.py (TIM lists incl. RLE variants), tools/vram.py (VRAM rebuild + (tpage,clut) texture), notes/20-textures.md. Terrain atlas validated (TPAGE 0x18, CLUT row 0xF0+tile).
- tools/export_area_obj.py is now a textured exporter (terrain atlas + UV orientation, real prop meshes, colliders); all 51 areas export; visually checked areas 1 and 4 with tools/preview_obj.py (numpy, system python): area 1 reproduces the first stage (lawn + trees inside a backdrop ring).
- export_model.py: actor OBJ+MTL with textures from the global VRAM (lists 0-2) - actor identification (which model is player/monkey) still open.
- Models: fixed hasNormals layout (positions then normals); textured actor export; player model = member 0x008 +0xBC (Spike) verified by render; monkey LOD models labelled from the monkey section (147 hits). notes/16-models.md has a Role column.
- Posed export: tools/export_model.py pose <member> <hdr> <anim> <frame>; Spike renders correctly textured and posed (stand frame 0 of the state set 0 anim at member 008 +0x1035C). Rotation order Rz*Ry*Rx and parent semantics (p-1) confirmed visually. preview_obj.py got front/side modes.
- Player static models: table A/B/C of member 0x008 parsed and validated (48 models), rendered (club, net frame, hoop rings, ...), notes/21-player-models.md generated. Gadget id -> model mapping still open. MonkeyNetCatchDraw/Update named (tentative). Unnamed 2013.
- Helper sweep 3: ~40 more helpers named (object helpers Obj*, sound loop table, SPU reverb, Distance/IntSqrt, EventCam). Unnamed 1956.

## Session summary (stopped early: usage limit reached)
Done: collision shape handlers (queue 1), terrain tile/CLUT/UV decode (2), textured area OBJ exporter (3), model prim layouts + posed/textured actor export, player = member 0x008 +0xBC (Spike), monkey LOD models labelled (4), area features / type 0x22 indicators (5, 10), monkey AI state dispatch (6, structural), gadget notes (7, tentative), camera survey (8), effects survey (9), sound bank container + SPU API names (11, partial), TIM/RLE textures + VRAM rebuild, player static models, helper sweeps 1-3. Unnamed 2104 -> 1956.
Open: tools/ghidra_scripts/ApplyProtos.java was just written and is UNTESTED (needs tools/protos.txt; idea: apply C prototypes so decompiler output uses Monkey*/Player*/Obj*). Sound SShd tone tables / SSsq opcodes, gadget id->model mapping, 8003dc7c monkey patrol, menus/HUD (12), mode overlays (13), area overlays (~1000 funcs), glTF/animation export.
Recommended next: test ApplyProtos, define an Obj struct (offsets in symbols for Obj* helpers), more helper sweeps (tools/unnamed_by_callers.py), decode SShd.
- ApplyProtos tested OK (tools/protos.txt, 10 Obj/Monkey helpers); Obj struct (partial) added. Chain it after ApplyStructs in the headless command.
- Enemy ('zako') system discovered: Obj[16] at 0x800E6960 (0xE4 each), '.ZED' spawn table in area blob +0x4E0 (validated: 409 entries / 103 types, notes/22-enemies.md via tools/dump_zako.py), ~45 Zako*/Obj* functions named (batch 27). Per-type handlers: table 0x800AF350 (many in area overlays). Unnamed 1903.
- Sweeps 5-12 (monkey, projectiles/bosses/HUD, effects, collision bodies, items/sound volume, message box): +300 names. Bosses: areas 0x44/0x53/0x56/0x46/0x57 dispatch into overlay code via BossInit/BossUpdate. Type-0x22 indicators confirmed as message-box hint spots. Unnamed 1603.

## Session summary (final, autonomous run 2)
Tasks done: collision shapes, terrain/UV decode, textured area OBJ export, model/anim/static-model formats, enemy (zako) system, sound bank container, camera internals, progress/save, message box, item/gadget/projectile sweeps, player state-table conditions/actions, ApplyProtos tooling, batches 14-46 of names.
Counts: unnamed 2104 -> 1460; symbols ~1075; structs 14. Last apply: symbols failed=0, protos failed=0.
Open questions: SShd tone tables and SSsq opcodes; gadget ids 3/4/5/8; monkey patrol handlers 0x8003DC7C/0x8003CD84; collider-script command semantics; overlay functions; glTF/animation export.
Blockers: none.
Next steps: finish rem1/rem2 main-EXE leftovers (notes/decomp/rem*c.txt, gitignored; regenerate with DumpCallGraph + ExportDecomp), then overlay areas, then more ApplyProtos retyping.

## Session summary (continuation: overlay sweep and main-EXE leftovers)
Done: named the mode overlays (180 title / scripted CD sequences, 182, 183 debug Sound Test, 186 save + card manager, 187 / 188 stage clear / start, 189 hub exit, 18A / 18B, 1AB Galaxy arcade minigame, 1AC STR movie player, 1AD, 1AE), all area enemy / boss overlays (18D-1A8) and the main-EXE Obj / Zako helper library, pickups, projectiles, door gimmicks, collider height helpers, progress tables, pause menu / HUD leftovers, and the SEQ sequencer interpreter (notes/23-sequencer.md).
New tooling: `DumpFuncHashes.java` + `propagate_names.py` (name once, apply to byte-identical or same-shape twins), `sweep_overlay.sh` + `compact_decomp.py` (preview of unnamed functions).
Counts: unnamed 1491 -> 227 (of which 104 are Ghidra `RAM` artefacts), symbols ~1,044 -> ~2,308, all applies failed=0.
Open questions: the sequencer needs validation against real SSsq data; gadget ids 3 / 4 / 5 / 8; which area uses which enemy / boss overlay (names carry only the overlay id); `EffectScriptUpdate` bytecode; many names are tentative (see the comments).
Blockers: none (one PowerShell `.ps1` was blocked by the execution policy; I ran the same commands inline instead).
Next steps: see the queue in notes/HANDOFF.md (item 15).

## Continuation 3 (sound tools, retyping, codex inbox)
- Sound: SSsq format + sequencer validated on all 43 banks (tools/dump_seq.py, notes/23), SShd tone tables + SFX sequences decoded and validated (tools/dump_tones.py, notes/24), software SEQ player (tools/render_seq.py, all 43 songs render; ADSR / pitch / resampler checked).
- Retyping: 300 Obj / Zako / boss helper prototypes generated from the symbol comments (tools/protos.txt, 325 total, failed=0); Obj struct gained animFrame, link0/1, motionState, speed, heading, hitTarget. Prototype names must stay in sync with symbols.csv (a prototype renames the function).
- Codex coordination (notes/COORDINATION.md): applied the inbox (ZED comments, 101-area reader fix in dump_areas.area_chains(), Area<id>_* renames, particle script labels). Generated notes 13, 14, 15, 20, 22 regenerated for all 101 areas (they covered only 51 before); all 101 areas export to OBJ.
- Open: modulation LFO in the player, remaining ~110 main-EXE leftovers (low value), next inbox check.

## Continuation 4 (inbox batch, game strings, gadget ids)
- Applied the codex-inbox (77326b3): 50 overlay enemy dispatch handlers (batch 101), StageStartUi_DrawObjectives, SpecialActorPlaceAtCollider (new function at 0x8009B578), struct AreaEntryPoint, facts in notes 08 / 11 / 12. Apply failed=0.
- Font digits 6-9 are glyph codes 0x7A-0x7D (seen on the font page); decoder fixed, two more monkey bios recovered (233 records).
- notes/28-game-strings.md (tools/dump_text.py): EXE strings (gadget menu, options, records), Professor dialogue, save / clear / start / stage select / monkey book overlays, including all 22 English stage names and the 8 era names.
- Gadget ids settled by the gadget menu table 0x800B25A0 (cursor index = gadget id): 5 Magic Punch, 6 Sky Flyer, 7 RC Car, 8 Water Net.
- Blockers: none.
- Sound, continued: SPU reverb with the game's preset 4 / volume 10 (registers from 0x800B41D0, checked against a per-sample version for all 9 presets), the 24-voice allocator SeqAllocVoice (only 5 songs steal; 0AD: 126 notes held by the sustain pedal), `tools/render_sfx.py` + notes/29 (every PlaySfx id, its sequence and 872 constant-id call sites; extended controllers = volume / pan ramps and pitch glides; effect tracks at 60 BPM / 480 PPQ), and the portable C player `tools/native_audio/sp_player.c` (music + effects) that matches the Python tools (songs <= 2.4e-5 relative RMS, effects 4e-7) - notes/30.
- Main-EXE leftovers named (batch 102, 77 functions); unnamed 118 = 104 RAM artefacts + 14 small stubs. Codex inbox applied twice (batches 101, 103).


## 2026-10-07 — single-agent takeover and docs audit

The user handed the whole project to Claude; the two-agent period is over. Audit of notes and instructions:
- Retired notes/COORDINATION.md, CODEX_SESSION_PROMPT.md and codex-inbox.md (banners; 27 open inbox entries closed,
  their actionable items applied: batch 112 camera script handlers + CameraForceFollow, CameraSlotSnapshotEye comment,
  export_area_obj untextured-first-primitive material fix). AGENTS.md rewritten for one agent.
- HANDOFF: new counts (3,611 functions, 104 unnamed artefacts, 2,706 symbols, 23 structs, 398 protos), a "Native port"
  map of where each module lives, the demo launch / regression commands, corrected rules (network allowed, remote
  exists, no push unless asked), and a "Next steps" list replacing the 2026-10-06 queue item 15.
- Stale facts fixed in notes/07 (sound, SDL2 plan), 08 (gadget 5/8 sections), 19 (pointer to the camera port),
  23 and 30 (frame clock / shared voices are implemented). SESSION_PROMPT rules updated.
Open structural items (need a user decision): merge claude/player-local into master; whether to keep
tools/codex/cloud_evidence (2,452 decompiler export files) in the tree; reorganising tools/codex (303 flat files).
