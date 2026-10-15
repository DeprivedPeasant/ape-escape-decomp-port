# Original ordinary-prop F4 depth-acceptance prefix

Proof scaffolding for later renderer proof. Helpers: `native_prop_depth.c/.h` (pure C11). It models only the integer
prefix of `EmitPolyF4` after `AVSZ4`/`stOTZ` and before the stored NCLIP-result decisions, colour and packet work. It is **not** whole-primitive
visibility, not a GTE depth generator, and makes no claim of a fully playable game, scene, or visual parity.
The fog factor is independent: see `native_prop_fog.md`.

## Original bytes used (private, 0x800193F0..0x80019448)

- `193F0` `lw` depth from scratch `0x3DC` (actual OTZ, unsigned 16-bit zero-extended).
- `193F8` `slti depth, 27`; `193FC` `bne` to `197B8` (skip primitive) if depth < 27.
- `19404` `lbu` flags from scratch `0x3F4`; `1940C` `andi 1`; `19410` branch to `19430` if bit0 set (bypasses near test).
- Otherwise `19418` `lhu` near from scratch `0x3C2`; `19420` `subu depth, near`; `19424` `slti delta, 257`;
  `19428` branch to `197B8` if delta >= 257 (signed).
- `19430` `lw` depth; `19438` `slti depth, 4032`; `1943C` branch to `19448` if depth < 4032; else the delay slot
  `li 4031` and `19444` store 4031 to scratch `0x3DC`. Prefix ends at `19448`, before stored NCLIP-result decisions.

## Semantics

`int ae_prop_f4_depth(uint16_t otz, uint16_t near, uint8_t flags, uint16_t *out_depth)`

- Reject (return 0, output untouched): `otz < 27`; or flags bit0 clear and `otz - near >= 257`; or `out_depth` NULL.
- Accept (return 1): `*out_depth = otz` if `otz < 4032`, else `4031`.
- Inputs are unsigned 16-bit, so the difference lies in [-65535, 65535] and is exact in int32. Negative deltas
  (depth below near) pass the near test.
- Only flags bit0 is used. No other flag meaning, including any fog/no-fog bit, is assumed.
- The upper rejection of the near window applies only when bit0 is clear; the 27 minimum and 4031 clamp always apply.

## Completed independent validation

Strict C11 compile passes. `check_prop_depth.py` executes the actual original prefix for 534288 cases, including all 65536 depths at each near value 0/1200/2000/65535 with both bit0 states (524288 exhaustive cases), plus 10000 randomized configurations. Acceptance, output clamp and unchanged rejection output agree; NULL output guard passes. Zero failures. See generated `validation/prop_depth_validation.md`.

The original NCLIP instructions occur earlier in the emitter; this prefix ends before their stored-result decisions. This is not a new NCLIP implementation. No viewer depth/fog integration is claimed.
