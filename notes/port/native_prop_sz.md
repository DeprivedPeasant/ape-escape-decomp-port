# native_prop_sz: restricted GTE RTPS depth (SZ3)

Integer-only helper for the depth result of one GTE RTPS (sf=1) vertex transform.

```c
int ae_prop_sz(const int16_t depth_row[3], int32_t translation_z,
               const int16_t vertex[3], uint16_t *out);
```

## Semantics

- `depth_row`: third row of the GTE rotation matrix (signed 16-bit).
- `translation_z`: TRZ (signed 32-bit).
- `vertex`: VX/VY/VZ (signed 16-bit).
- `MAC3 = signed44(translation_z * 4096 + row[0]*v[0] + row[1]*v[1] + row[2]*v[2])`
- `SZ3 = clamp(MAC3 >> 12, 0, 65535)`

The sum is formed in `int64_t` (every term is at most 2^43 in magnitude, so nothing overflows).
Wrapping to signed 44 bits at each step or only at the end gives the same final value
modulo 2^44, so one wrap at the end is used. The wrap is done with a `uint64_t` mask
`(1<<44)-1` and bit 43 as the sign: no implementation-defined signed shift or out-of-range
signed cast. If bit 43 is set the value is negative and SZ is 0; otherwise `word >> 12`
is clamped to 65535.

Return value: 1 with `*out` set; 0 with `*out` unchanged if any pointer is NULL.
All inputs are read and the result computed before the store, so `out` may alias the input
arrays provided it is valid writable `uint16_t` storage.

## Out of scope

Matrix composition / view matrix, world Z or floats, RTPS lm choice (affects IR, not SZ),
screen H / divider / NCLIP / GPU / colour, flags, IR, MAC state, FIFO. No claim is made about
scene or cloud fixes; a future pipeline still needs the original view matrix / CompMatrix,
draw flags, primitive indices and order.

Reference: https://psx-spx.consoledev.net/ps1/cpu/gte/geometrytransformationenginegte/ (RTPS/MAC3).

## Completed independent validation

Strict C11 compile passes. `check_prop_sz.py` compares only SZ3 against full RTPS(sf1/lm0) dispatch in unchanged pinned PCSX-ReARMed source.617446 cases including27000edge cases/100000random cases/244223actual source vertices from101areas at two synthetic transforms pass.4000aliases/3guards/0failures. See generated `validation/prop_sz_validation.md`. Transform inputs are test data, not actual captured camera matrices. No other GTE state or scene-parity claim.
