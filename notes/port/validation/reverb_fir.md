# reverb_fir.h

Standalone C11 header-only integer FIR stage for PS1 reverb. No malloc, I/O or dependencies beyond `<stdint.h>` and `<string.h>`.
Hardware exactness is **not** claimed; it implements the coordinator-specified contract only.

## API
- `AeReverbFir` – `phase` (0..63), `down_hist[2][64]`, `up_hist[2][32]`.
- `void ae_reverb_fir_reset(AeReverbFir*)` – zeroes everything (phase 0).
- `int ae_reverb_fir_push(AeReverbFir*, const int16_t input[2], int16_t down[2])` – stores input at `phase`; on odd phase writes the 22.05kHz sample to `down` and returns 1, otherwise returns 0 and leaves `down` untouched.
- `void ae_reverb_fir_pull(AeReverbFir*, const int16_t wet[2], int16_t out[2])` – on odd phase stores `wet` at `up_hist[phase>>1]` (`wet` is ignored on even phase and may be NULL then); always writes `out`; then `phase = (phase+1) & 63`.

## Caller order per 44.1kHz frame
1. `ready = push(in, down)`
2. if `ready`, run the reverb network on `down` to get `wet`
3. `pull(wet, out)` (exactly once per frame; it advances the phase)

Output volume is applied by the caller.

## Math
- Down: 39 taps, `clamp16(floor((sum coef[k]*down_hist[(phase-38+k)&63]) >> 15))`.
- Up, odd phase: 20 taps over even-index coefficients (indices 0,2,..,38; the middle 0x4000 at index 19 is excluded), `wet[(phase/2-19+k)&31]`, shift 14, clamp16.
- Up, even phase: `wet[(phase/2-10)&31]` directly.
- Accumulator is int64; right shifts are floor shifts implemented portably.

## Limits / notes
- Stereo only; state is not saved/serialized beyond the struct.
- Even-phase output reads history only; before the first odd phase it is 0.
- Validation still required: compile and compare against the external reference (impulses, random, phase wrap, chunk boundaries). Not compiled or run by the author (no shell).

## Concerns
None found in the contract; the 20 up-taps equal the even-index list entries, and the omitted 0x4000 center corresponds to the direct even-phase output (0x4000>>14 = 1).
