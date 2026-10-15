# native_sdk_sqrt: original SDK SquareRoot0 (0x800303C0)

Pure C11, no embedded game bytes, no third-party or reference code. Written from
the supplied task specification of the original routine.

## Table

- Original EXE virtual address `0x800AE5C0`, 192 little-endian signed 16-bit entries (384 bytes).
- File offset = `0x800AE5C0 - 0x80010000 + 0x800` = `0x9EDC0`.
- `ae_sdk_sqrt_load` decodes bytes explicitly (LE), stages all 192 values, then copies and sets `loaded = 1`.
- Rejects (returns 0, `loaded = 0` if `tables` non-NULL): NULL exe, image too short, any value outside 4096..8192, `values[0] != 4096`, `values[191] != 8175`.
- The endpoints 4096 / 8175 and all 192 entries were checked against the actual EXE by Codex. No table bytes are committed.

## ae_sdk_sqrt0

Returns 0 with `*out` untouched for NULL args, unloaded tables, `value > INT32_MAX`, or an index outside 0..191.

- `value == 0`: returns 1, `*out = 0`, no table read.
- Otherwise: `lz` = leading zeros of the 32-bit value (original reads LZCR); `even = lz & ~1`; `exponent = (31 - even) >> 1` (0..15);
  `normalized = even >= 24 ? value << (even - 24) : value >> (24 - even)`; `index = normalized - 64` (must be 0..191);
  `factor = (uint32_t)table[index]`; `out = (factor << exponent) >> 12`, both uint32 logical shifts (original `sllv` then `srl 12`, wrap32 preserved).
- No floating point.

## Scope

- Domain is the original SquareRoot0 input only. The original ADD overflow trap and downstream callers are separate and not modelled here.
- This is not a full CameraLookAt or projection claim.

## Validation

- Strict C11 builds with warnings treated as errors.
- `check_sdk_sqrt.py`:90,368 complete original calls, including exhaustive0..65535, scaled table-bin boundaries, powers of two and neighbors, INT32_MAX and random nonnegative signed32 inputs.
- 180,736 actual external GTE transfer steps and11guard checks; zero failures. See generated `validation/sdk_sqrt_validation.md`.
- Negative inputs are unsupported; this is not an unrestricted signed32 SDK emulation.
- Caller must load the actual EXE table; nothing is embedded here.
