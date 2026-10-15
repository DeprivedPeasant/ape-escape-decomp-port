# Hand-off: Ape Escape decomp (state as of 2026-10-07; see notes/WORKLOG.md)

Read this first, then `notes/03-naming-log.md` and the topic notes it links.

> **One agent (Claude) owns the whole project since 2026-10-07.** The two-agent period (Claude + Codex,
> 2026-10-06/07) is over: `notes/COORDINATION.md`, `notes/CODEX_SESSION_PROMPT.md` and `notes/codex-inbox.md`
> are history only. Codex's work (glTF export, overlay mapping, effect VM, the native viewer / camera /
> indicators / renderer, the audio fidelity work) lives in `src/` (portable C), `tools/codex/` (checkers, packers) and `notes/50`-`69`; it is now
> ordinary project code that may be edited freely.

## Goal

A functional (non-matching) decompilation of **Ape Escape (USA, SCUS-94423)** leading to a
**native Windows port**. Mods and a level editor come later, but format knowledge for them
is being collected along the way. The current phase is **understanding and naming**: turn
Ghidra's `FUN_xxxxxxxx` into named functions, typed structs, and documented data formats,
validated against real game data wherever possible.

## Current state

| | |
|---|---|
| Functions in the Ghidra program (EXE + 49 overlays) | 3,611 |
| Still unnamed (`FUN_`) | 104: all of them are 1-byte `RAM`-block stubs that are Ghidra artefacts at overlay addresses (ignore). Every real function in the EXE and the code overlays is named |
| Symbols in `tools/symbols.csv` | 2,706 (batch 112) |
| Structs in `tools/structs.txt` | 23 |
| Prototypes in `tools/protos.txt` | 398 |
| PsyQ SDK functions identified by signatures | ~900 (SDK 4.5) |

Systems understood well enough to read their code (see notes):

| Area | Notes |
|---|---|
| Boot, main loop, game modes, CD/archive loader | 01, 02, 03, 04 |
| Monkeys: slots, active set, AI framework, spawns, nav graph | 05, 15 |
| SDK / hardware surface for the port | 06, 07 |
| Player: struct, data-driven state machine, PlayerUpdate, physics | 08, 09 |
| Model / animation engine, primitive emitters | 11 |
| Level: terrain heightfield, collision, area data blobs, props | 12, 13 |
| Interactive objects (gimmicks), area exits | 14, 15 |
| Model / area / texture exporters and formats | 16, 20, 21 |
| Monkey AI states, zako enemies | 17, 22 |
| Sound banks and the SEQ sequencer interpreter | 18, 23 |
| Camera slots | 19 |
| Mode overlays: save / load + card manager (ovl 186), title + scripted CD sequences (180), stage clear (187) / start (188), hub exit (189), STR movie player (1AC), debug Sound Test (183), Galaxy arcade minigame (1AB) | names only, see `tools/symbols.csv` (prefixes `SaveUi_`, `CardMgr_`, `SeqPlayer_`, `StageClearUi_`, `StageStartUi_`, `HubExitUi_`, `StrPlayer_`, `SoundTest_`, `Galaxy_`) |
| Area enemy / boss overlays (18D-1A8) | names only (`Ovl<id>_*` prefixes); they share an Obj helper library in the main EXE (`Obj*`, `Zako*`) |
| Runtime playtest findings | 10 |

## Native port: where the code is

The port is a set of portable C modules (layout: `src/README.md`) plus an SDL3 + OpenGL demo. Each module reads the game's own tables from
the EXE / archive members at run time (no game data in the repo) and has a checker that compares it with the
original code (instruction-level MIPS runs of the extracted EXE) or with the Python reference tools.

| Part | Code | Notes / reports |
|---|---|---|
| Music + sound effects (sequencer, SPU voices, ADSR, reverb, 24-voice pool) | `src/audio/` (checker `tools/native_audio/check_player.py`) | notes/30 |
| Text, font, message box | `src/text/` (checker `tools/native_text/check_text.py`) | notes/32 |
| Player state machine, body, collider volumes | `src/player/` (tests: `src/player/run_tests.sh`) | notes/70, replay checker in `src/player/replay/` |
| Collision, ground, terrain walking | `src/collision/` | notes/67-69 |
| Catchable monkeys (original spawn / patrol / nav data, approximate behaviour) | `src/monkeys/` (checker `src/monkeys/check_monkeys.py`) | `notes/port/monkeys.md`, notes/05, 15, 17 |
| Camera (follow, regions, slots, scripts, blends) | `src/camera/` | `notes/port/native_camera*.md`, `notes/port/validation/camera_*_validation.md`, notes/19 |
| Stage indicators (hint markers) | `src/indicators/` | `notes/port/native_indicator*.md`, `notes/port/validation/indicator_*_validation.md` |
| Props, sky, fog, GTE / SDK maths | `src/render/native_prop_*.c`, `native_sky.c`, `src/math/native_sdk_*.c` | `notes/port/native_prop_*.md`, `native_sdk_*.md` |
| Collision bodies (body list, slots, body-vs-body tests, per-frame `CollisionUpdate`) ported 1:1 onto a byte-exact PSX RAM image (`AeRam`) | `src/core/ae_ram.*`, `src/bodies/ae_bodies.*` (checker `tools/mips/check_bodies.py`, MIPS harness `tools/mips/mips_ram.py`) | `notes/port/bodies.md` |
| Particle effect VM | `src/effects/effect_vm.c` | notes/54, 64 |
| Demo (viewer) | `src/app/native_viewer.c`, built by `tools/codex/build_native_viewer.py` or the top-level `CMakeLists.txt` | notes/65, `notes/port/overnight_handoff.md` |

Validation ledger: `tools/verified.csv` lists every ported function with the minimum case count of its differential checker, `tools/mips/check_exclusions.csv` declares every deliberate blind spot of those checkers (RAM masks, shared-random stack words, unreachable instructions); `tools/check_verified.py` (part of `tools/run_checks.py`) enforces them. Lowering a `min_cases` or adding an exclusion row needs the project owner's review and a commit message saying why; porting a function means adding its ledger row. See `notes/port/validation/ledger.md`.

Controls (keyboard + PS4 / gamepad table): `notes/port/controls.md`.

Run the demo (Escape quits, H previews the nearest hint; area `0x05` = Molten Lava with a scripted camera):

```powershell
uv run python tools/codex/build_native_viewer.py --area 0x01 --stage-camera --indicator-proximity --sky-props --keyboard-only --run
```

`--keyboard-only` avoids an SDL gamepad enumeration stall on this PC. Quick regression of the combined demo:
`uv run python tools/codex/check_overnight_demo.py` (about 10 s, expect `"failures": 0`).
What the demo does not do yet: catch monkeys, most gadgets, stage progress / completion, enemies, the original
projection / fog / draw order (cloud cards still show cyan rectangles).

## Environment (Windows 11, project at `<repo>`)

- **Ghidra 12.1.2** in `tools/ghidra_12.1.2_PUBLIC` (pinned; PSX loader + GhidraMCP
  extensions require this exact version). Its settings dir is isolated via
  `support/launch.properties` (`tools/ghidra-settings`). Do not touch `%APPDATA%\ghidra`.
- **Ghidra project**: `ghidra-projects/ApeEscape.gpr`, program `/SCUS_944.23`
  (main EXE + overlay blocks `ovl_180`…`ovl_1B0`).
- Python via `uv` (repo root has `pyproject.toml`): `uv run python tools/<script>.py`.
- Game data (gitignored): `disc/`, `extract/disc/` (filesystem), `extract/members/XXX.bin`
  (KKIIDDZZ archive members), `bios/`.
- PCSX-Redux in `tools/pcsx-redux` + Lua loggers in `tools/pcsx/` — these need a human
  playing; an autonomous session can't use them meaningfully.

## The workflow (everything is headless and reproducible)

Names and types live in two source files; Ghidra is rebuilt from them:

- `tools/symbols.csv` — `address,name,comment`. Plain hex = main RAM, `ovl_XXX::addr` =
  overlay space, names starting `g_` are data labels. One section per batch with a
  `# --- batch N: ... ---` header.
- `tools/structs.txt` — struct definitions + `apply <addr> <Type>` lines (see its header).
  Fields named `unk_XX` until understood.

Ghidra scripts (`tools/ghidra_scripts/`):

| Script | Use |
|---|---|
| `ApplySymbols.java <symbols.csv>` | apply all names/comments (creates functions if missing) |
| `ApplyProtos.java <protos.txt>` | apply C prototypes (`<addr> <proto>;`, struct pointer types allowed; tested: decompiler shows `obj->pos[0]`) — run after ApplyStructs |
| `ApplyStructs.java <structs.txt>` | (re)create structs and apply them; removes stale labels inside |
| `ExportDecomp.java <out.c> <depth> <max> <addr...>` | decompile roots + callees to a text file (write to `notes/decomp/`, gitignored) |
| `DumpDataRefs.java <out.csv> <start> <end>` | every reference into an address range (find readers/writers of globals, recover struct layouts) |
| `DumpCallGraph.java <dir>` | functions.csv / calls.csv / gte.csv / hwrefs.csv into `notes/inventory/` |
| `CreateFunctionsAt.java <addr...>` / `CreateFunctionsFromTable.java <table> <max> ...` | code only reachable via pointer tables |
| `FunctionStats.java [out]` | counts; named-function list |
| `DumpFuncHashes.java <out.csv>` | position independent code hash + opcode shape per function (overlay code is heavily duplicated; see `propagate_names.py`) |
| `ImportOverlays.java` | (done) overlay import |

**Canonical headless invocation** — always start the command with this exact literal path
(a permission rule allows that prefix; variables like `$g` break the match):

```powershell
& '<repo>\tools\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat' '<repo>\ghidra-projects' ApeEscape -process SCUS_944.23 -noanalysis -scriptPath '<repo>\tools\ghidra_scripts' -postScript ApplySymbols.java '<repo>\tools\symbols.csv' -postScript ApplyStructs.java '<repo>\tools\structs.txt' *> '<repo>\ghidra-projects\run.stdout.txt'; Select-String -Path '<repo>\ghidra-projects\run.stdout.txt' -Pattern 'symbols applied|structs=|bad|could not|unknown|ERROR|Exception' | % { $_.Line.Trim() }
```

Add `-readOnly` for export-only runs. Chain several `-postScript` in one run (Ghidra
startup is the slow part). Expect `symbols applied=N failed=0` and `structs=N applied=M`.

Data tools (validate formats against the real game data and generate notes — **never edit
the generated notes by hand; change the script and re-run**):

| Tool | Output |
|---|---|
| `unpack_kkiiddzz.py` | `extract/members/` |
| `verify_overlay_map.py` | overlay load addresses vs the EXE's tables |
| `sdk_inventory.py` | `notes/06-sdk-inventory.md` |
| `dump_player_states.py` | `notes/09-player-state-tables.md`, `tools/pcsx/player_state_names.lua` |
| `dump_areas.py` | `notes/13-areas.md`, `extract/areas/*.png` heightmaps |
| `dump_objects.py` | `notes/14-area-objects.md` |
| `dump_monkeys.py` | `notes/15-monkeys-exits.md` |
| `native_audio/` | portable C music / effect player for the port (`sp_player.c`), validated against the two tools above by `native_audio/check_player.py` (notes/30) |
| `render_seq.py` / `render_sfx.py` | song of a sound bank / sound effect id -> WAV (`render_sfx.py note` regenerates notes/29-sound-effects.md) |
| `propagate_names.py` | prints symbols.csv lines for unnamed twins of named functions (needs `notes/decomp/hashes.csv` from `DumpFuncHashes.java`); append the output, apply, repeat after each naming batch |
| `unnamed_by_callers.py` | helper sweep ranking of unnamed functions by caller count |

## How naming is done here (keep this standard)

1. Export the target (and depth-1 callees) with ExportDecomp; read it in chunks with
   Read offset/limit. Use Grep on the export for strings / globals.
2. Name only what the code supports. If a name is a guess, say so in the comment
   ("tentative", "probably"). Prefer behavioural names (`Gimmick_ProxTrigger`) over
   guessed game-design names.
3. Find struct layouts with DumpDataRefs (widths + functions per offset), add fields to
   `structs.txt`, apply, re-export and check the decompiler output reads as fields.
4. When a data format is involved, write/extend a `tools/dump_*.py` that parses the real
   archive members and checks the result is sane (counts, ranges, cross-references). This
   caught several wrong assumptions this session (word vs byte offsets, 4 vs 8-byte records).
5. Apply via headless, confirm `failed=0`, update the relevant `notes/NN-*.md` and add a
   section to `notes/03-naming-log.md`.
6. Commit (see below).

## Pitfalls that cost time (read these)

- **PowerShell hex literals**: `0x80010000` and `0x800b5000` are parsed as *negative*
  Int32. Use `[long]` / decimal (2148225024 = 0x800B5000) or do the math in Python.
- **`R` is an alias for `Invoke-History`** — don't name helper functions `R`.
- **Don't pipe analyzeHeadless into `Select-Object -First N`**: it closes the pipe, the
  Java process blocks on output and keeps the project lock. Redirect to a file (`*>`)
  and read the file. If a run hangs: find the `java` process started by you and stop only it.
- **Project lock**: headless fails with "Unable to lock project" if the Ghidra GUI (or a
  stuck headless run) has the project open.
- **No `<` redirection** in PowerShell.
- **Line endings**: `tools/structs.txt` and other files may be CRLF; multi-line string
  replacements in PowerShell must match. Prefer the Edit tool for multi-line edits.
- **Decompiler pointer arithmetic is ambiguous** about element size (`ptr + n` may be
  bytes or elements). Confirm offsets against real data before trusting them.
- **Git commit messages**: write the message to a file and use `git commit -F <file>`
  (here-strings piped to `-F -` don't work in this shell). End messages with the
  `Co-Authored-By` line given in your session's system reminder.
- Many functions are reachable only through pointer tables (state handlers, gimmick
  handlers, condition/action tables). If an address isn't a function, create it with
  CreateFunctionsAt / CreateFunctionsFromTable first.
- Overlay addresses: functions in overlays appear as `FUN_ovl_18C__8013xxxx`; address them
  as `ovl_18C::8013xxxx`. Area handler lists can live in the area's code overlay.

## Rules

- Never commit game data, BIOS, decompiled output (`notes/decomp/` is gitignored), the
  Ghidra project, or third-party tools. `reference/ApeEscapeRecomp` is PolyForm
  Noncommercial — facts only, never copy its generated code.
- Network research and project downloads are allowed (`AGENTS.md`); keep downloads in ignored
  paths inside the repo (e.g. `extract/codex/deps/`) and record version / source / checksum.
- The remote `origin` (private GitHub repo) exists; don't `git push` unless the user asks. Don't
  modify `%APPDATA%`, the Ghidra install's `launch.properties`, or `.claude/settings.json`.
- Commit on `master` after each completed task (small, descriptive commits). Never leave
  the repo uncommitted for long.
- `extract/cloud_evidence/` holds decompiler text exports that the user allowed into the private
  repo for a cloud session (2026-10-06). Don't add more decompiler output anywhere else.

## Prioritized work queue

Each item: what to do, where to start, how to validate. Work top-down; if blocked for more
than ~45 minutes, write down what you found in `notes/WORKLOG.md` and move on.

1. (DONE) **Collision shape types** (notes/12) and the collider box height / contact helpers.
2. (DONE) **Terrain tile/texture bytes** (notes/12, 20).
3. (DONE) **Terrain + prop exporter** (`tools/export_area_obj.py`, textured).
4. (DONE) **Model exporter** (`tools/export_model.py`, textured and posed; notes/11, 16, 21); glTF + animation clips by Codex (`tools/export_gltf.py`, notes/50).
5. (DONE) **Per-area feature ids** (notes/12, 13).
6. (DONE at structural level) **Monkey AI states** (notes/17).
7. (DONE by text evidence) **Gadgets** — 0 Stun Club, 1 Monkey Net, 2 Radar, 3 Slingback Shooter, 4 Super Hoop, 5 Magic Punch, 6 Sky Flyer, 7 RC Car, 8 Water Net (confirmed by the gadget menu table, notes/08); names come from the decoded hint texts (notes/26, tools/dump_text.py). Runtime confirmation would still be nice.
8. (DONE, tentative meanings) **Camera** — slots / kinds named (notes/19); slot layout = struct `CameraSlot` with typed slot functions.
9. (DONE) **Effects / particles** — API and spawners named; the `EffectScriptUpdate` particle bytecode was decoded by Codex (notes/54, `tools/codex/effect_vm.py`, native C VM in notes/64). The 0x38-byte "effect" slots turned out to be collision bodies (`CollisionBodyAlloc`, struct `CollisionBody`).
10. (DONE) **Type 0x22 trigger indicator** — registered / activated / drawn (`IndicatorsUpdateDraw`, struct `Indicator`); models and animation ranges from area features 0x10 / 0x11 (notes/12, Codex notes/63); the hint texts are notes/26.
11. (DONE for data) **Sound** — banks (notes/18), SSsq songs + sequencer (notes/23), SShd tone tables + SFX sequences (notes/24) validated on all banks; `tools/render_seq.py` renders every song to WAV (SPU reverb with the game's preset, 24-voice allocation), `tools/render_sfx.py` renders every sound effect id and writes notes/29 (ids, sequences, 872 constant-id call sites). The native C player (notes/30) adds the 60 Hz clock, the shared 24-voice pool, integer ADSR and reverb. Open: portamento; a listening check of `extract/native_audio/136_native_integer.wav`.
12. (PARTIAL) **Menus / HUD** — pause menu, HUD popups, message box and mode overlay screens are named; font mapping, the 91 hint texts (notes/26), monkey bios (notes/27) and the menu / stage-name / dialogue strings (notes/28) are decoded. The stage name of every area is resolved (level table in ovl 0x188, notes/28). Open: the users of the copies of the name list in ovl 0x186 / 0x189 / 0x1AD (save files, era select, monkey book).
13. (DONE for names) **Mode overlays** 180, 182, 183, 186-189, 1AB, 1AC, 1AD, 1AE.
14. (ONGOING) **Helper sweeps** — `tools/propagate_names.py` plus the preview workflow (export the unnamed functions of one overlay with `ExportDecomp`, name them, propagate to twins) took the overlays from 969 to about 10 unnamed functions.
15. (DONE) The 2026-10-06 list: SEQ player, glTF export, prototype retyping (ongoing as needed), effect bytecode, overlay mapping, main-EXE leftovers.

## Next steps (2026-10-07, fidelity plan)

The user's direction (2026-10-07): match the real game; no invented behaviour or approximations; delegate simple /
mechanical work to Sonnet agents. Gameplay code is now ported 1:1 onto a byte-exact image of PSX RAM (`src/core/ae_ram`:
EXE data at its real addresses, the area member at 0x80154000 = g_AreaData, original struct layouts and globals), so
original data with absolute pointers works unchanged and every ported function is diffed against the original
instructions in the MIPS test harness (tools/mips/).

1. **Collision bodies** (DONE 1794446, 6,313 cases byte-identical to the original): the body pool / list, body-vs-body contacts and the body part of
   `CollisionUpdate` (notes/port/bodies.md).
2. **Net catch, the real path**: `PlayerRunAnimEvents` kinds 2 / 3 / 6 / 7 spawn hit bodies from the bone matrix
   (the Monkey Net's swing registers a type 8 body at frame 9 of state 0x80); `BodyCollideBodiesDetailed` writes the
   contact into the monkey body; `MonkeyUpdateContact` / `MonkeyAiStateUpdate` (activity 3 at 0x80041FB8, gated by
   `StageExitIsActive`); `MonkeyNetCatchUpdate` / `MonkeyNetCatchDraw` (capture sequence, net model, catch animations).
   This replaces the demo's temporary catch sphere.
3. **Monkey AI, full port** of 0x80032E30..0x80043300 (168 functions, ~16.5k instructions) in four groups: nav and
   small helpers; movement / goal selection; AI state handlers; top level (MonkeyInit, MonkeySpawnAll, MonkeyUpdateAll,
   MonkeySelectActive, MonkeyThinkActive, MonkeyAiStateUpdate, sight). Host hooks: model drawing / shadow / alert icon,
   particles (effect VM), sounds, projectiles, gimmick / object-slot queries, ground height (src/collision). This
   replaces the demo's temporary wander / flee behaviour in src/monkeys/.
4. **Player on the RAM image**: sync or move `g_Player` (struct Player) into AeRam so the body system, monkeys and
   events see the original fields; then gadget details (CanSwitchGadget, the flick tracker's spin states, the new-game
   gadget slots) from their code instead of the current stand-ins.
5. **Captures need a human player** (`tools/codex/player_frame_capture.lua` under PCSX-Redux): hard fall, ledge grab,
   hazards, ropes; emulator save states at those spots are enough (inputs can then be scripted).
6. **Renderer**: original `CameraLookAt` / view matrix composition, primitive colour / projection / depth order, fog.
## Pitfalls learned in the autonomous session
- `tools/protos.txt`: a pointer return type must be written `T* Name(...)` (ApplyProtos cannot parse `T *Name`). Never open a file for writing in the same expression that reads it (it truncates first).
- PowerShell cannot run python heredocs or `printf`; use Bash for those. Long address lists go to analyzeHeadless via a built command string.
- Chain order for apply: ApplySymbols, ApplyStructs, ApplyProtos, FunctionStats (expect failed=0 everywhere). ApplyProtos needs prototypes without a trailing `;` (handled in the script).
- The `RAM` block contains 104 one-byte pseudo functions at overlay addresses (Ghidra artefacts); `FunctionStats` counts them as unnamed. Ignore them.
- Gimmick handler records: +2 state, +4 collider index into `PTR_800F455C`, +6 timer / progress, +8.. parameters (`struct Gimmick`, 0x18 bytes). Init and update handlers are separate functions (`Gimmick_*_Init` / `_Update`).
- PowerShell script files are blocked by the execution policy; run long export commands through `powershell.exe -NoProfile -Command "..."` from Bash (see `tools/sweep_overlay.sh`) instead of `.ps1` files. A heredoc containing certain characters can make the Bash tool fail with `unexpected EOF`; writing the text to a file with the Write tool and appending it avoids that.
- Names marked "Tentative" in symbols.csv comments are evidence-based guesses; verify before relying on them.
