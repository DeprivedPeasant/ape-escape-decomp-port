# Claude cloud session handoff

Read `AGENTS.md`, `notes/COORDINATION.md`, then `notes/HANDOFF.md` before work.
The Windows paths in those documents describe the local workstation, not the cloud
checkout. Use checkout-relative paths in cloud code and commands.

## Goal and first checkpoint

Primary target: source-backed, portable C player movement, turning, jump, falling,
and landing. Net use and monkey capture are stretch goals after movement works.
The existing native viewer is a collision/walking harness, not the full original
player state machine. Begin with `notes/08-player.md`,
`notes/09-player-state-tables.md`, `tools/dump_player_states.py`,
`tools/structs.txt`, `tools/protos.txt`, and `tools/symbols.csv`.
Original player reference evidence is now available in
`extract/cloud_evidence/player/`: start with its README and manifest.
The complete existing text export archive is in `extract/cloud_evidence/archive/`.
Read its README and search `functions.csv` by name/address to locate focused sections.
For body position integration, ground probing and landing, prefer its fresh
`exports/codex/cloud-movement/` supplement. `CollisionUpdate` applies body movement;
`BodyProbeGround` and `BodyLandOnGround` handle ground contact; `Mode_Stage` establishes
update order. These are separate from `PlayerUpdate` and `PlayerAct_Land`.
Use the function index, decoded state tables, and matching instruction listings
to reconstruct behavior without a cloud Ghidra installation.

Claim a dedicated new player module under `src/player/` in the append-only
Claims section of COORDINATION.md before implementation. Keep changes on a separate
cloud branch. Leave Codex renderer/collision files and the music engine alone;
describe required integration changes in a handoff for local Codex work.

First deliverable: identify which movement behaviors have enough committed evidence
to implement, list exact missing original functions/tables, and establish a portable
module interface compatible with the existing collision engine. Implement only
behaviors supported by evidence. Commit each useful checkpoint with tests and a
concise report of completed behavior, evidence, and remaining dependencies.

## Important cloud limitations

This repository deliberately excludes the game disc, EXE, archive members,
textures/models/audio exports, BIOS, general decompiler exports, Ghidra databases,
and third-party tool installations. The user authorized private upload of the player
package and all existing text decompiler exports, including a fresh movement
supplement (see COORDINATION.md Claims). These snapshots live under tools/codex/
cloud_evidence; the original local notes/decomp remains ignored. Do not expand this
exception to binaries, assets, BIOS or Ghidra databases. Do not download
ROMs or claim original-game differential validation without
the original inputs. Do not substitute invented physics for missing original logic.
Record missing evidence for the local agent to supply or verify through an agreed
transfer mechanism; do not add excluded artifacts to Git.

`pyproject.toml` references a local Ghidra bridge wheel that is not uploaded.
Consequently, a clean cloud checkout cannot blindly reproduce the Windows `uv run`
environment. Prefer system Python for standard-library tooling, and an available
C compiler for portable modules. Do not edit the shared dependency files merely to
work around a missing local tool.

The native demo CMake target requires SDL3 and OpenGL. Its Windows launcher and SDK
setup are workstation tools. Cloud success should first mean portable module builds
and meaningful synthetic tests. Rendering, controller feel, game-data validation,
and original-instruction comparison will be verified locally.

The committed `.mcp.json` points to a Windows Ghidra bridge. It is not a usable cloud
service. Do not start it or alter the local Ghidra configuration.

## Budget checkpoints

Use the user's $100 budget as a ceiling, not a promised completion estimate.
Prioritize player movement; reserve budget for validation and fixes before stretch
goals. Keep sessions focused, read large files in relevant chunks, and report a
blocker instead of repeatedly spending on unavailable original inputs. If the
available notes are insufficient, deliver the evidence inventory and interface
work first so the next paid session has the inputs it needs.
