# Reverb pipeline (bounded component)

`reverb_pipeline.h` composes `src/audio/sp_reverb.h` (22.05 kHz integer network)
with `reverb_fir.h` (44.1 <-> 22.05 kHz FIR). Header-only C11, static inline. It is not
wired into music, effects or the viewer.

## State and memory
`AeReverbPipeline` owns `int16_t ram[262144]` (512 KiB), a `SpReverb`, an `AeReverbFir`
and two `int16_t` output volumes. Allocate it on the heap or statically, not on the stack.

## Pointer lifetime
`network.ram` points at the struct's own `ram`. Never copy or move the struct by value
(memcpy, assignment, realloc, container growth). After any relocation call
`ae_reverb_pipeline_init` again (this also wipes state). Re-init in place is allowed and
resets everything. Do not re-init per callback block; RAM is cleared only on init.

## API
- `int ae_reverb_pipeline_init(p, preset[33], left_vol, right_vol)`: returns -1 on NULL
  `p` or `preset`, else 0. Zeroes RAM, network and filter; `reg[0..31] = preset[1..32]`;
  `base = cursor = preset[0]*4` (halfword units; any uint16 gives 0..262140, base 0 allowed);
  sets `network.ram`.
- `void ae_reverb_pipeline_step(p, input[2], output[2])`: one 44.1 kHz tick.

## Per-tick phases
1. `ae_reverb_fir_push` stores the input. On odd phase it returns 1 with a downsampled
   signed16 pair, and `sp_reverb_step` runs once, producing signed16 wet values.
2. `ae_reverb_fir_pull` consumes the wet pair on odd phase only (zeros are passed on
   even phase and ignored), produces the 44.1 kHz output and advances phase exactly once.
3. Each channel: `floor(sample * volume / 32768)` via 64-bit multiply and the portable
   floor shift from `reverb_fir.h`. Volumes are independent per channel. Output is int32
   (-32768 * -32768 yields 32768); no clipping here.

The network therefore runs once per two ticks. No extra gain on the filter, no dry
signal, no allocation, I/O or events.

## Integration requirements (caller)
- Input must be the clamped signed16 sum of all wet sends for the tick.
- Add the returned values to the dry mix and clip in the final mixer.
- Reverb master is assumed always enabled; disabled master, IRQ and RAM aliasing with
  sample data are not modeled.

## Not proven
- No build or test was run (shell unavailable). Needs strict GCC compile and the full
  independent pipeline tests by Codex.
- No hardware bit-exactness is claimed. Preset-to-register layout (preset[0] as base,
  preset[1..32] as reg[0..31]) follows the task statement, unverified against data.
- Even-phase wet argument being ignored relies on the current `reverb_fir.h` behavior.

## Blockers
None for the header. Original data is absent, so no data-driven validation was possible.
