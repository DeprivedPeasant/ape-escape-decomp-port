# Agent instructions

Since 2026-10-07 one agent (Claude Code) owns the whole project. The earlier two-agent setup
(Claude + ChatGPT / Codex) is retired: `notes/COORDINATION.md`, `notes/CODEX_SESSION_PROMPT.md`
and `notes/codex-inbox.md` are history only. Code Codex wrote (`src/`, `tools/codex/`, `notes/50`-`69`, `notes/port/`)
is ordinary project code now.

Project overview, current state, tools, workflow and next steps: `notes/HANDOFF.md`.

## Project goal (user decision, 2026-10-13)

A readable decompilation / source port like the Super Mario 64 and Zelda projects: hand-ported, named, verified C plus
native systems (renderer, audio, input, file I/O). No simulated PS1 hardware, no CPU emulation and no machine-translated
code in `src/` (that is what the separate PSXRecomp-based ApeEscapeRecomp does). Ported C uses structured control flow,
named locals and named field offsets, one function per original function; MIPS semantics are reproduced only where
they change results. Generators that emit C from instruction listings, simulated registers / delay slots and per-PC
labels are rejected. The MIPS interpreter in `tools/mips/` is a test oracle only.

## Network access (user update, 2026-10-06)

Web searches and downloads that help this project are authorized. This overrides older
no-network and no-download instructions in session prompts and older notes. Do not ask again
for routine project-related research or downloads.

Prefer official documentation and upstream releases. Keep downloaded dependencies and
third-party tools inside this repository, under ignored paths such as `extract/codex/deps/`
or an existing ignored tools directory; record versions, sources and checksums when available.
Do not install globally or modify user application settings. The no-push rule and the
prohibitions on committing game data or third-party tools still apply. Research may use facts
from noncommercial reference projects, but must not copy their code.
