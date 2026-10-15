# Original ordinary-prop F4 fog factor and fog configuration

Proof scaffolding for later opt-in rendering. This does **not** claim the cyan-card appearance is fixed, nor a
complete GTE / colour projection. Helpers: `native_prop_fog.c/.h` (pure C) and `prop_fog.py` (area header reader).

## Original facts used

- `ApplyLevelFog` (0x800323F8, private name) loads the unsigned bytes `gLevelGrid+0x1C/0x1D/0x1E` and calls
  `SetFarColor(R,G,B)`; the SDK `SetFarColor` shifts each by 4 into the GTE far-colour registers. It then sets scratch
  `0x3C6 = 0`, `0x3C4 = 0`, `0x3C0 = low16(gLevelGrid.fogParam at +0x24)`, `0x3C2 = low16(fogStart at +0x28)`.
- `AreaLoad` (normal path) assigns `fogParam = 0xF000`. From the area header: fog colour at `+0x20`, `fogEnabled`
  s16 at `+0x28`; if enabled is 0, `fogStart = 0x4B0`, else `fogStart = u16 header+0x2C`. The far u16 at `+0x2E` is
  read and a delta computed, but neither the F4 near/factor helper nor this code uses it. It is kept as metadata
  only; nothing here claims the scale uses far.
- Source width / draw distance formula: `fogStart * 4 + 0x400`.
- Special sky types 4/5 zero `fogParam` during their draw and restore it afterwards; these helpers do not apply to
  types 4/5 (see `native_sky.md`).

## F4 factor (8001950C..80019564)

`near = lhu scratch[0x3C2]`, `depth = lw scratch[0x3DC]` (OTZ), `coef = lhu scratch[0x3C0]`.
`delta = subu(depth, near)` (modulo 2^32); signed `MULT delta * coef` (coef is unsigned 16-bit, so non-negative);
`MFLO` takes the low 32 bits; `sra 12`; clamp to 0..4096. The multiply is wrapped to 32 bits **before** the shift:
there is no wide or saturating product. The two unclipped F4 alternates (80019628..80019680,
80019728..80019780) use the same integer formula.

`ae_prop_fog_factor` computes this with only unsigned modular arithmetic: the low word's sign bit set means the
arithmetic shift is negative, so the result is 0; otherwise `low >> 12` clamped to 4096. Defined for all inputs.

Depth is OTZ from GTE `AVSZ4`, not world Z. Source RGB stays separate from the fog colour.

## Configuration

`ae_prop_fog_config(header, size, &out)` requires `size >= 0x30`, rejects NULL (no output written on failure) and
fills `AePropFog {rgb, coefficient = 0xF000, near_distance, draw_distance}` from a zero-initialised local, copied
only on success so padding stays zero. `prop_fog.py load(member)` reads the header at the pointer in member `+0x4D0`
and returns `member`, `header_offset`, `rgb`, `fog_enabled` (signed 16), `near` / `far` (source u16),
`effective_near`, `coefficient`, `draw_distance`. It only reads; no global state is modified.

Example, Fossil member 0x05B header at 0x84C, bytes `+0x20..+0x2F` =
`000000000000000001000000D0070010`: RGB 0,0,0, enabled 1, near 2000, far 4096. This alone does **not** prove the
cyan cards need any particular fogged pixel value.

## Not reconstructed (proof pending)

DPCS / CC / RGB saturation, the GTE fixed depth, projection and the full primitive packet, the OTZ value per
primitive, runtime colours and draw ordering are not ported here.

## Completed independent validation

Strict C11 compile passes. `check_prop_fog.py` validates all 101 actual headers, 25 synthetic configurations, six guards, 3000 complete original ApplyLevelFog calls, 3000 SDK far-color shift prefixes, and 60528 original integer factor paths with zero failures. See generated `validation/prop_fog_validation.md`.

Coordinator review corrected its own prompt: the original draw-distance offset is hexadecimal 0x400 (1024), not decimal 400. Padding is explicitly zeroed with memset and copied with memcpy; aggregate assignment alone did not guarantee padding. No viewer fog integration or cloud appearance fix is claimed.
