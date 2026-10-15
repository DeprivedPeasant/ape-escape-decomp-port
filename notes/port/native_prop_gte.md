# Restricted GTE depth and fog-colour helpers

Helpers: `native_prop_gte.c/.h` (pure C11, uint32 arithmetic). This is **not** a GTE emulator: no flags, MAC/IR
registers, FIFOs, projection or colour matrices. It makes **no** claim that the cyan-card appearance is fixed and does
no render integration. Independent testing against the pinned external emulator backend is complete; see `validation/prop_gte_validation.md`.

Reference: <https://psx-spx.consoledev.net/ps1/cpu/gte/geometrytransformationenginegte/> (AVSZ3/AVSZ4 and DPCS
sections).

## Original facts used

- `GteInitProjection` sets H = 328, ZSF3 = 341, ZSF4 = 256 (see `validation/prop_projection_setup.md`).
- `ApplyLevelFog` calls `SetFarColor(R,G,B)`, which loads the far-colour registers as `byte << 4`
  (see `native_prop_fog.md`).
- The F4 factor consumes OTZ from AVSZ4 (`ae_prop_fog_factor` in `native_prop_fog.c`). SZ values are inputs; these
  helpers do not generate SZ and do not assume world Z.

## AVSZ3 / AVSZ4

`ae_prop_otz3(sz[3], &otz)`: `OTZ = (341 * (SZ1+SZ2+SZ3)) >> 12`, saturated to 0..65535.
`ae_prop_otz4(sz[4], &otz)`: `OTZ = (256 * (SZ0+SZ1+SZ2+SZ3)) >> 12`, saturated to 0..65535.

Each SZ is uint16 (already hardware-saturated). All operands are non-negative, so the arithmetic shift is a plain
unsigned shift. With these constants the result is at most 16367 (AVSZ3) / 16383 (AVSZ4), so MAC0 never overflows and
the 65535 clamp never triggers; it is kept as the documented hardware bound. NULL argument: return 0, output unchanged.

## DPCS (restricted)

Domain: opcode `sf=1, lm=0`; source RGB bytes plus CODE byte; far colour bytes set via `SetFarColor`
(FC = far << 4); IR0 factor 0..4096.

Per channel in hardware: `MAC = source << 16`; `delta = ((far<<4)<<12 - MAC) >> 12 = (far - source) * 16`;
intermediate IR saturation to [-32768, 32767] does not bind for byte inputs; `MAC = (MAC + delta*factor) >> 12`;
result `= MAC >> 4` saturated to 0..255. Within this domain every value is non-negative and <= 255, and the nested
floor shifts equal the floor of the linear combination / 65536, i.e.

    out = (source * (4096 - factor) + far * factor) >> 12

computed in uint32 (max 255*4096), with no signed shifts, floats or overflow. `out[3] = source[3]` (CODE preserved).
Results are computed into a local buffer and copied last, so `out` may alias `source`, or `far` when that buffer provides at least four writable bytes for the output. All inputs are read before writing. Returns 0 with no writes for any NULL
pointer or `factor > 4096`.

Not claimed: raw FC registers, arbitrary IR0, `lm=1`, `sf=0`, CC, or GTE flag results.

## Completed independent validation

Strict C11 compile passes. `check_prop_gte.py` compares to unchanged PCSX-ReARMed GTE source at commit8b6c163575e6ce9a0537553e0d4ed4225315955e, built by `gte_oracle.py` under ignored extract/. Sources are pinned and SHA256 recorded in ignored oracle-source.json; no emulator implementation is copied into these helpers.

498747 AVSZ cases cover all achievable depth sums plus random SZ arrangements. 3142550 DPCS cases include2818560 synthetic configurations,64398 source palette records from101 actual areas at five factors, and random alias configurations.4000 alias checks/9guards/0failures. This is external emulator differential evidence, not console gameplay evidence. Flags/FIFOs/MAC sideeffects/CC/projection/runtime palettes and ordering remain excluded. Helpers are not wired into the viewer.
