# Native camera SDK math

`native_camera_math.c/.h` reconstruct the integer SDK math the camera follow code
needs: `rsin`, `rcos`, `Atan2Safe` (over `ratan2`) and `VectorNormalSS`. No
dependencies beyond C99. No game data is embedded and no host `sin`/`cos`/floating
point is used; the tables are read from the caller's PSX-EXE image.

## Provenance

Written from the instruction listings of `rsin` 0x8002FB50, `sin_1` 0x8002FB8C,
`rcos` 0x8002FC20, `ratan2` 0x800145D0, `Atan2Safe` 0x80046340, `VectorNormalSS`
0x800301E0 and its core 0x80030210. No code from external reference projects.

**Status: reviewed and validated by Codex.** Strict GCC compilation and44829original-instruction comparisons pass. See validation/native_camera_math_validation.md.

## Tables (`ae_camera_math_load`)

File offset = address - 0x80010000 + 0x800, signed little-endian halfwords.

| Table | Address | Entries |
|---|---|---|
| `sine` (quarter wave, 4096 = 1.0) | 0x800A9DB0 | 1025 |
| `atan` (ratan) | 0x800B4430 | 1025 |
| `reciprocal` (VectorNormalSS factor) | 0x800A9C14 | 192 |

Bounds are checked before any read. Failure (null argument, short image, or
`sine[0] != 0`, `sine[1024] != 4096`, `atan[0] != 0`, `atan[1024] != 512`)
returns 0 and leaves `loaded = 0`. The PSX-EXE magic is not checked. The
reciprocal base follows from the core: the lookup is `lh -0x63EC(0x800B0000 + 2*(idx-0x40))`.

## rsin / rcos

Angles are reduced as in the original: negative angles are negated (wrapped) first,
`& 0xFFF`. rsin of a negative angle is the negated result for `-angle`. Index
mapping (all within 0..1024):

- rsin: `a<=0x400` `sine[a]`; `<=0x800` `sine[0x800-a]`; `<=0xC00` `-sine[a-0x800]`; else `-sine[0x1000-a]`.
- rcos (`|angle|`): `<=0x400` `sine[0x400-a]`; `<=0x800` `-sine[a-0x400]`; `<=0xC00` `-sine[0xC00-a]`; else `sine[a-0xC00]`.

The table offsets in rcos (`-0x6250`, `-0x6A50`, `-0x7A50`) and rsin (`-0x7250`) were
converted to sine indices relative to 0x800A9DB0. Both return 0 if the tables
are null or not loaded.

## ae_camera_atan2_safe(x, z)

- `z == 0`: result is 0x800 if `x < 0` else 0 (original `sra 31; andi 0x800`). INT32_MIN x is fine here.
- Otherwise `ratan2(y = z, x = x)`: absolute values with x / y negative flags;
  `|x| == |z| == 0` returns 0 (unreachable here). If `|z| < |x|`: quotient of
  `|z|` by `|x|` scaled by 1024, angle `atan[q]`; else `0x400 - atan[q]`. When the
  numerator (the smaller operand) has any of the bits `0x7FE00000`, the
  denominator is shifted right by 10 and the numerator is used unshifted;
  otherwise the numerator is shifted left by 10. Truncating signed division.
  Then x negative: `0x800 - angle`; z negative: `-angle`.
- Fails closed (returns 0, `*out` unchanged): null / unloaded tables, null `out`,
  nonzero z with x or z equal to INT32_MIN (abs wraps in the original), table index
  outside 0..1024, divide by zero or INT32_MIN / -1 (original `break`). Normal
  camera inputs are 16 bit, so none of these occur there.

## ae_camera_normalize

In place version of the wrapper (loads three halfwords, core, stores three
halfwords).

1. SQR0: squares of the signed 16 bit components, summed with unsigned 64 bit
   arithmetic. If the sum exceeds INT32_MAX the original `add` raises an
   overflow exception: return 0, nothing written.
2. Zero sum: all components are zero, stored back as zero, length 0, return 1.
   (The original's table lookup for this case reads outside the table, but it is
   multiplied by zero.)
3. Otherwise `lz` = leading zeros of the sum, `v1 = lz & ~1`, `t6 = (31 - v1) >> 1`.
   Index value: `sum << (v1-24)` if `v1 >= 24`, else `sum >> (24-v1)`, giving
   64..255; table index = that `- 0x40` (checked against 192), factor = signed 16 bit
   `reciprocal[index]`.
4. Each original component times factor (GPF sf=0, 16x16 product fits 32 bits),
   arithmetic shift right by `t6 & 31` (portable floor shift), low 16 bits stored.
   MAC output is used, with no IR saturation.
5. `*squared_length` (optional) gets the original sum (the original's return value).

No square root here; the existing original-checked `ae_collision_sqrt(value, 1)`
can serve the callbacks.

## Validation still required

Compile with strict flags and compare against the original instructions: all
4096 sine and cosine phases (including negative angles), atan2 quadrants and
large-magnitude branches, zero / bounded normalize vectors, and the failure
cases above.
