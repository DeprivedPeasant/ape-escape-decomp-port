# Ape Escape decomp → native port

A readable decompilation of **Ape Escape** (USA, SCUS-94423), leading to a native port in the spirit of the Super
Mario 64 and Zelda projects. Original game functions are hand-ported to structured, named C and **verified one by one
against the original MIPS instructions**. The port then runs on native systems (renderer, audio, input, file I/O):
there is no emulator, no recompiler and no machine-translated code.

> **No game data is included.** This repository contains only original code, tools and documentation. You need
> your own legally obtained copy of the game. Disc images, extracted files, BIOS files, decompiler output and game
> text are never committed (see `.gitignore`).

## Status

- About 1,500 original functions are ported and differentially verified. The ledger is `tools/verified.csv`;
  `uv run python tools/check_verified.py` checks it.
- The **game host** (`src/game/`) runs `main`'s mode dispatch (boot -> title, matching the original) and the ported
  `Mode_Stage` loop on one RAM image:
  - area load, the player (`PlayerUpdate`), monkeys and their AI, enemies, collision, camera, HUD, pause menu and
    stage fades;
  - the Monkey Net catch;
  - every frame of areas 01, 02, 05 and 44 matches the original instructions byte for byte.
- The **native viewer** (`src/app/`, SDL3 + OpenGL) runs the real game logic and renders terrain, props and sky.
  It reaches no missing functions in areas 01 and 02. Drawing of characters, HUD and effects from game data is still
  in progress.
- **Overlays:** nearly all mode overlays (boot, intro, title, save menu, Stage Clear, Special Area Enter, minigames)
  and the area overlays 18C, 1A0 and 1A8 are ported. Most area and boss overlays are not yet.
- The project is paused until about 2026-10-23; `notes/port/paused_2026-10-15.md` lists the open work.

See [notes/HANDOFF.md](notes/HANDOFF.md) for the full picture and [AGENTS.md](AGENTS.md) for the project rules.

## Layout

| Path | Contents |
|---|---|
| `src/` | Portable C by subsystem: ported game code (`ae_*.c`), the game host (`src/game/`), native systems and viewer. See [src/README.md](src/README.md). |
| `tools/mips/` | MIPS interpreter (test oracle only) and the differential checkers `check_*.py`. |
| `tools/verified.csv`, `tools/mips/check_exclusions.csv` | Verification ledger and the registry of declared, evidence-backed comparison exclusions. |
| `tools/` | Extraction, dump, export and inventory scripts; Ghidra scripts. |
| `notes/` | Findings: formats, structures, function maps, per-module port notes (`notes/port/`). |
| `disc/`, `extract/`, `bios/` | Your local copies (gitignored, never committed). |

## Getting started

1. Install [uv](https://docs.astral.sh/uv/) and a MinGW-w64 GCC. On Windows, GCC 8+ on `PATH` is enough for the
   checkers.
2. Put your own Redump `SCUS-94423` bin/cue in `disc/`. Keep it as bin/cue: converting to ISO loses the XA streams.
3. Extract it:
   - extract the disc filesystem to `extract/disc/` with `dumpsxiso -l`;
   - unpack the archive members with `uv run python tools/unpack_kkiiddzz.py`, which produces `extract/members/`.

   See [notes/00-setup.md](notes/00-setup.md) and [notes/01-disc-layout.md](notes/01-disc-layout.md).
4. Run a checker, for example `uv run python tools/mips/check_game_area.py` (the integrated game host against the
   original). For the native viewer, see `tools/codex/build_native_viewer.py --help` (it expects the SDL3
   MinGW development package under `extract/codex/deps/`).

Optional analysis tools (not included; versions in `notes/00-setup.md`): Ghidra 12.1.2 with `ghidra_psx_ldr`,
PCSX-Redux, splat / spimdisasm (`uv run splat ...`).

## Contributing

- **Port one function per original function** as readable, structured C: named locals, named field offsets and
  globals (`#define`), and no simulated registers, delay slots or per-PC labels.
- **Add a differential checker,** or extend one in `tools/mips/`, that runs the original instructions and your C on
  identical RAM images with real area data. It needs:
  - full instruction coverage, or a registered proof for each gap;
  - mutation tests, every one caught;
  - a ledger row in `tools/verified.csv`.
- **Hardware-facing code stays a named hook:** PsyQ SDK / BIOS calls and native systems (CD, SPU, GPU packets, VRAM).
  Pure GTE maths routines are the exception and are ported (`src/core/ae_sdk*.c`).
- **Before you open a pull request,** run the checker for the module you changed plus
  `uv run python tools/check_verified.py`.

Rules and conventions are in [AGENTS.md](AGENTS.md) and [notes/port/validation/ledger.md](notes/port/validation/ledger.md).

## License

MIT for the original code, tools and documentation in this repository; see [LICENSE](LICENSE). Ape Escape and all of
its data belong to their respective owners.
