# Setup (2026-10-05)

## Installed versions

| Tool | Version | Source |
|---|---|---|
| Ghidra | 12.1.2 (pinned) | github.com/NationalSecurityAgency/ghidra |
| ghidra_psx_ldr | 2026.10.03 build for 12.1.2 | github.com/lab313ru/ghidra_psx_ldr |
| GhidraMCP | 6.0.0 (extension + Python bridge) | github.com/bethington/ghidra-mcp |
| PCSX-Redux | nightly 26641.20261005.13 | distrib.app (SHA-1 95F9E9D0... verified) |
| mkpsxiso / dumpsxiso | 2.30 win64 | github.com/Lameguy64/mkpsxiso |
| splat64, spimdisasm | latest on PyPI | `uv` env in repo root |
| Java | 21.0.9 (system) | — |

Ghidra is pinned to 12.1.2 because Ghidra extensions only load on the exact
version they were built for, and GhidraMCP 6.0.0 targets 12.1.2. Upgrade
Ghidra only together with both extensions.

`support/launch.properties` sets `-Dapplication.settingsdir=.../tools/ghidra-settings`.
Without it, Ghidra also loads `%APPDATA%\ghidra\ghidra_12.1.2_PUBLIC\Extensions`, which
holds an older GhidraMCP 5.14.2 (plus XEXLoaderWV) from another project, and startup
fails with "Multiple modules collided with same name: GhidraMCP".

Verified 2026-10-05: headless import of a synthetic PS-X EXE selected
"PSX Executables Loader" / `PSX:LE:32:default`. The loader bundles PSY-Q signatures
for SDK 2.6–4.7 (`Extensions/ghidra_psx_ldr/data/psyq`), applied via its analyzer.

## One-time GUI steps

1. Run `tools\ghidra.cmd`. If prompted about new extensions, accept both.
   Confirm under **File > Install Extensions** that `ghidra_psx_ldr` and `GhidraMCP` are checked.
2. Open a CodeBrowser (any program), then **File > Configure > Configure All Plugins**,
   tick **GhidraMCP**.
3. **Tools > GhidraMCP > Start MCP Server** (listens on 127.0.0.1:8089).
4. Open Claude Code in `<repo>` and approve the `ghidra` MCP server
   from `.mcp.json`. The session's folder must be this one: `.mcp.json` is not picked
   up by a session rooted at `%USERPROFILE%\Coding` or `<projects>`.

Headless runs (`support\analyzeHeadless.bat`) need the project closed in the GUI, or they
fail with "Unable to lock project". Send their output to a file. If it's piped to
something that stops reading early, Java blocks on the full pipe and keeps the lock.

GhidraMCP enforces its own naming conventions on renames by default. If that
conflicts with decomp-style names (`func_800XXXXX`, SDK names), turn it off in
the tool options.

## Disc

Put the Redump bin/cue in `disc/`. Keep it as bin/cue — converting to ISO loses
the Mode-2 Form-2 XA streams. Expected hashes are in
`reference/ApeEscapeRecomp/DISC.md`. Boot EXE `SCUS_944.23`: load 0x80010000,
entry 0x800A3660. Overlay layout: `reference/ApeEscapeRecomp/docs/AOT_OVERLAYS.md`.

## BIOS

`bios/scph1001.bin` (gitignored). SCPH-1001 US v2.2, SHA-1
`10155D8D6E6E832D6EA66DB9BC098321FB5E8EBF`, copied from the user's PCSX-Reloaded install.
In PCSX-Redux: Configuration > Emulation > BIOS, point it at this file.

## Not yet set up

- PCSX-Redux GDB server (enable in Configuration > Emulation > Debugging).
