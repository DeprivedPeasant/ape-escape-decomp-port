# Disc layout (SCUS-94423)

Disc: `disc/SCUS-94423.bin/.cue` — Redump SHA-1 `466CCE4B...2FB7FC` verified 2026-10-05.
Extracted with `dumpsxiso -l` to `extract/disc/`; rebuild script with LBAs: `extract/SCUS-94423.xml`.

## Files

| File | LBA | Notes |
|---|---|---|
| `SYSTEM.CNF` | 23 | `BOOT=cdrom:\SCUS_944.23;1`, TCB 20, EVENT 20, STACK 0x801FFFF0 |
| `SCUS_944.23` | 24 | Main EXE, 677,888 bytes. Load 0x80010000, entry 0x800A3660 |
| `KKIIDDZZ.HED` | 355 | Archive index, 512 × u32 |
| `KKIIDDZZ.DAT` | 356 | Archive payload, 28,672 sectors |
| `KKIIDDZZ.BNS` | 29028 | Archive payload continued (physically adjacent), 432 sectors |
| `MINIGAME/MINI1/` | 29461+ | `MINI1.EXE` (PS-X EXE @ 0x80100000) + `SK*`/`CRS*` data (probably Ski Kidz Racing) |
| `MINIGAME/MINI2/` | 30544+ | `MINI2.EXE` (PS-X EXE @ 0x80100000) + `BX*` data (probably Specter Boxing) |
| `MINIGAME/MINI3/` | | `BANANA.DAT/.TMS` |
| `STR/`, `DEMO/` | | MDEC/XA movies (`.STR`, mixed-mode 2336-byte sectors); `DEMO/*.ALL` alongside |

## KKIIDDZZ archive

Unpacked by `tools/unpack_kkiiddzz.py` → `extract/members/XXX.bin` + `index.json`.

Descriptor u32: `count = d >> 20` (sectors, 12 bits), `offset = d & 0xFFFFF`.

| Range | Count | Meaning |
|---|---|---|
| 0x000–0x14A | 331 | DAT members, offset relative to DAT start (HED LBA + 1) |
| 0x14B–0x17F, 0x1B1–0x1BF, 0x1D4–0x1FF | — | unused (zero) |
| 0x180–0x1B0 | 49 | BNS members — code overlays. Exactly cover BNS. |
| 0x1C0–0x1D3 | 20 | **Absolute disc LBAs** of STR movies, one per file. Count is the file's sector count truncated to 12 bits (e.g. WEPGET 5424 → 1328), so the game must not rely on it for streaming. |

Members 0x055 and 0x056 start with a `PS-X EXE` header.

Overlay load paths (from the recomp's `docs/AOT_OVERLAYS.md`, to verify in Ghidra):
- Mode overlays → 0x80136000, selected via mode table 0x800AFA30, loader 0x8005BDC0.
- Area/stage code overlays → 0x8013D000, via area records 0x800A71E0..0x800A8190.
- 0x185 likely links at 0x80146000; 0x1B0 at 0x80136000 (no load path found).
- HED init 0x80011CF8, resolver 0x80012C0C, sector loader 0x800112E4. No compression.
