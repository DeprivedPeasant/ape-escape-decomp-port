# native_sdk_apply: original ApplyMatrixLV (0x80030450)

`int ae_sdk_apply_matrix_lv(const uint8_t matrix[32], const uint8_t vector[16], uint8_t out[16])`

Pure C11, no dependencies. **Status: validated against the complete original CPU+GTE instruction stream.**

## Layout

- `matrix`: 9 LE s16 rotation (row-major) at bytes 0..17. Bytes 18..31
  (padding, translation) are never read; translation is not applied.
- `vector`: 3 LE s32 at 0..11. Bytes 12..15 are padding, ignored.
- `out`: 3 LE s32 written to 0..11. Bytes 12..15 are left unchanged.
- Returns 1 on success; 0 if any pointer is NULL (output untouched).
- `out` may alias `vector` exactly (all inputs are read before any write).
  Partial overlap is not supported.

## Bit pipeline

Each s32 component `x` is split. Nonnegative: `high = x >> 15`, `low = x & 0x7FFF`.
Negative: the original negates with `SUBU` (wraps at 32 bits), then
`high = -ASR(abs, 15)`, `low = -(abs & 0x7FFF)`. After signed16 truncation
of high, this is equivalent to `high = x / 32768`, `low = x % 32768`
with C truncate-toward-zero division.
`high` is then loaded into IR1..3, which truncates it to signed 16 bits.

INT_MIN: the original gives high 65536 (mathematically -65536); both truncate
to 0 in 16 bits, and low is 0, so the effective value is the same.

Then, per output row `i` with rotation row `R_i`:

1. High: `RTIR sf0 lm0`, no translation. `MAC = sum(R_ij * high_j)` as int64
   (fits 44 bits). MAC1..3 are read raw as the low 32 bits (no IR saturation).
2. Low: `RTIR sf1 lm0`. `MAC = floor(sum(R_ij * low_j) / 4096)`, low 32 bits.
3. `out_i = (highMAC << 3) + lowMAC`, modulo 2^32 (original `ADDU`), stored LE.

This is **not** a full-precision matrix-vector product. The s16 truncation of
the high part and the 32-bit wrap are significant, and are reproduced as-is.

Implementation notes: all arithmetic is on `int64_t` (bounded) or `uint32_t`.
There is no signed overflow, no negative left shift, no implementation-defined
right shift of negatives and no out-of-range cast. Floor division is done
with quotient/remainder adjustment.

## Validation

`check_sdk_apply.py` ran 21,728 complete original calls, including 10,864 exact
alias cases, 412,832 external GTE steps and three NULL guards, with zero failures.
See the generated `validation/sdk_apply_validation.md`. It runs the complete original CPU
instructions with the actual GTE commands
through the optional pinned external backend and compares all 16 output bytes
for: normal values, random full-32-bit vectors, INT_MIN components, matrix
rows with s16 extremes, exact input/output aliasing, and padding preservation.

## Scope

A helper only. It is not the full view matrix, projection, gameplay or render
fix. No original or third-party code was copied.
