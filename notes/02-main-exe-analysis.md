# SCUS_944.23 — first Ghidra pass (2026-10-05)

Project: `ghidra-projects/ApeEscape.gpr`, program `/SCUS_944.23`.
Imported headlessly with the PSX loader + default analysis (157 s, of which the
PsyQ signature pass took 108 s). The loader picked the **PsyQ 4.5** type archive
(`psyq450.gdt`).

## Header

| Field | Value |
|---|---|
| pc0 (entry) | 0x800A3660 |
| gp0 | 0x800BCC60 (past end of text, so in BSS) |
| t_addr / t_size | 0x80010000 / 0xA5000 (ends 0x800B5000) |
| d_addr / d_size | 0x800A37F0 / 0x32C0. Inside text, so the loader's attempt to map it again fails with a harmless MemoryConflictException |
| b_addr / b_size | 0 / 0. BSS isn't declared in the header, so reads past 0x800B5000 warn during decompile |

## Function coverage

`tools/ghidra_scripts/FunctionStats.java` produces `notes/scus-named-functions.txt`.

| | Count |
|---|---|
| Functions total | 1924 |
| Loader pseudo-functions (GTE macros at 0x2000xxxx, etc.) | 153 |
| PsyQ SDK functions identified in game text | 680 (323 with public names, 357 internal `LIB_OBJ_xxx`) |
| **Unidentified (game code to decompile)** | **1091** |

SDK libraries present include CD (`CdRead`, `St*` streaming, ISO9660), GPU
(`ResetGraph`, ...), GTE (`GEO_*`, `PATCHGTE`, `RATAN`), pads (`PAD*`), memory card
(`LIBMCRD`), interrupts/events/VSync, and BIOS stubs. For the native port these
are the hardware layer to reimplement. Everything else is game logic.

## Overlays (added 2026-10-05)

`tools/ghidra_scripts/ImportOverlays.java` (run as a `-preScript`, map in
`tools/overlay_map.json`) adds each BNS member as a Ghidra overlay block `ovl_XXX`
at its load address (16 at 0x80136000, 31 at 0x8013D000, plus tentative 0x185 @
0x80146000 and 0x1B0 @ 0x80136000), with a function and `ovl_XXX_entry_*` label at
each known entry.

No separate BSS block is needed. The PSX loader already maps `RAM` 0x800B5000–0x801FFFFF
as uninitialized, so the "unable to read" decompiler warnings are just reads of
uninitialized globals.

After re-analysis with overlays:

| | Count |
|---|---|
| Functions total | 3379 |
| Named (PsyQ signatures + loader) | 1066 (IMPORTED 419 + USER_DEFINED 647) |
| Overlay entry functions | 49 (ANALYSIS) |
| **Unnamed** | **2264** |

The PsyQ pass also matched about 200 SDK functions inside the overlays, which are
statically linked copies.

Overlay load addresses verified against the game's own tables on 2026-10-05; see
notes/03-naming-log.md.
