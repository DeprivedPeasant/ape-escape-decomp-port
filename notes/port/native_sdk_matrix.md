# Native SDK matrix helpers (MulMatrix0, CompMatrix)

Status: **independently validated.** These are SDK helper foundations only. They make no
claim about CameraLookAt, render projection, or any cloud fix.

Files: `native_sdk_matrix.c`, `native_sdk_matrix.h` (pure C11; only `stdint.h`, `limits.h`,
`stddef.h`, `string.h`; no floats, no signed overflow, no right shift of negatives).

## Sources

| Original | Address | Native |
|---|---|---|
| MulMatrix0 | `0x80031830` | `ae_sdk_mul_matrix0` |
| CompMatrix | `0x8002FF50` | `ae_sdk_comp_matrix` |

Semantics are taken from the task's integer description of those routines; the original
bytes were not available in this worktree, so nothing here was checked against them.

## Matrix layout (32 bytes, little-endian)

- 0..17: 9 s16 rotation, row-major (`m[r][c]` at `2*(3r+c)`)
- 18..19: padding
- 20..31: 3 s32 translation (x, y, z)

## Rotation

The original runs GTE `RTV0` (sf=1, lm=0) once per right-matrix column. Each result element is
`floor(sum_k L[r][k] * R[k][c] / 4096)`, saturated to s16 `[-32768, 32767]`. Products and sums
fit in int64 with huge margin (about 44 bits). Floor division is implemented portably for
negative int64.

Padding (out 18..19): the original's last `gte_stIR3` is a 32-bit store at `out+16`, so
padding is the sign extension of the last result `m33`: `FF FF` if negative, else `00 00`.

## MulMatrix0

Writes only `out[0..19]`; `out[20..31]` is left unchanged, including when `out` aliases an input.
The original returns the out pointer; the native returns an int status (1 ok, 0 bad argument).

## CompMatrix

Rotation and padding are computed exactly as above. The translation vector comes from the
**low 16 bits** of each right-translation s32 (the original loads `lhu Tx`, `lw Ty` then `<<16`,
and `lw Tz` into VZ16). Each is truncated to s16 by explicit unsigned arithmetic (no
implementation-defined casts). It is rotated by the left rotation with sf=1, using the MAC1/2/3
values (unclamped, not the saturated IR): `floor(dot/4096)`, range `[-786432, 786432]`. The left
s32 translation is then added with MIPS `ADD` semantics.

On success all 32 bytes of `out` are written (rotation, padding, and 3 s32 translations).

## Domain and failure

- Any NULL argument returns 0 and leaves `out` untouched.
- If any translation sum falls outside the int32 range, the original `ADD` would trap. The native
  fails closed: returns 0, `out` untouched. **The original's trap behavior and any partial writes
  made before the trap are not reproduced.**
- All inputs are read into locals and the result is assembled in a local buffer before writing, so
  exactly equal input/output pointers work (in-place, either input). Partial overlap is
  unsupported.

## Completed independent validation

`check_sdk_matrix.py` executes full original MulMatrix0 and CompMatrix CPU instructions with each actual COP2 transfer/command handled by unchanged pinned PCSX-ReARMed GTE source.30069 complete32byte outputs/20046exactinput-outputaliases/782184GTEsteps/6NULLguards/0failures. Fifteen original ADD-trap cases are detected at the actual instruction and native fails closed without writes; original pre-trap partial mutations are intentionally excluded.

Optional external GTE backend was added only to the test MIPS interpreter. Its existing backend remains the default;44829camera math cases plus60528originalfog paths/101headers/3000ApplyFog+3000SDKshift regressions pass. No runtime/emulator code is copied into this port helper. See generated `validation/sdk_matrix_validation.md`.
