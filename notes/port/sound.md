# Game-side sound requests

Instruction-derived call tree (SCUS-94423, entries and direct `jal` targets):

```
PlaySfxAt 80087A20
  SoundEmitterCalcPan 80087C8C
    SquareRoot0 800303C0 (existing ae_sdk_SquareRoot0)
    VectorNormalSS 800301E0, core 80030210 (existing ae_sdk_normalize_core)
  SfxIsQueued 800885A8
  SoundDelayedQueueAdd 800885FC
  SfxVoiceStartEx 800A31B8
    SpuWaitIdle 800A1398 [host synchronization hook]
      SeqSetTickFlag 800A1378 (query 2; standalone C port)
    SpuVolumeClamp 800A119C
    SfxVoiceAlloc 8009E940
      SfxLookupTone 8009E82C
  printf 800A3720 [diagnostic hook]
SoundLoopStartEx 800882C4
  SoundLoopAllocSlot 800881D0
  SfxVoiceStart 800A3158
    SpuWaitIdle
    SfxVoiceAlloc
  SfxVoiceStartEx
  printf
SoundLoopStopAndFree 800880D0
  SfxVoiceStop 800A329C
    SpuWaitIdle
    SPU writes (channel ADSR1/ADSR2 +8/+10; key-off +18C/+18E) [hooks]
    SpuWaitLoop 8009D79C [timing hook]
  SoundLoopFree 8008820C
```

No indirect calls appear in this tree. Neither starting a request nor allocating its
0x120-byte SFX slot writes sample address, pitch, ADSR, or key-on registers. Those
operations belong to the later sequencer tick, outside this task's request tree.
No SPU register image or emulation is added. Despite the existing g_SpuShadow
name, SpuDriverInit @800A13EC sets the pointer at 800E1874 to **1F801C00**
(lui/ori at 800A1484/1488; store at 800A14E0). It points at hardware, not RAM.

The module runs on AeRam; named table addresses and field offsets are in
`src/sound/ae_sound.h`. All external callbacks take user context and AeRam.
Missing required callbacks set the sticky error and stop further work.

## Hardware boundary and native audio mapping

`AeSoundHw.wait_idle` replaces SpuWaitIdle, which queries SeqSetTickFlag(2) until
zero or until its 0x89CFF polling budget is exhausted. It returns 0 for ready, 1
for timeout, and preserves RAM. A native adapter must synchronize its audio thread,
not reproduce a CPU busy loop. SeqSetTickFlag itself is ported for game RAM use.

`wait_loop(units)` replaces the CPU-only SpuWaitLoop delay based on 768 * units.
A native adapter may implement a no-op; RAM is preserved.

`set_voice_adsr(channel,adsr1,adsr2)` represents the two adjacent envelope-register
writes in ADSR1 then ADSR2 order. This tree uses (0,0) on high-bit stop handles.
Offsets +8/+A are ADSR1/2, confirmed by the driver initializer and the
[SPU register map](https://psx-spx.consoledev.net/soundprocessingunitspu/#spu-io-port-summary).
`voice_key_off(half,mask)` preserves each individual 16-bit key-off mask write;
half 0 addresses hardware voices 0..15, half 1 addresses 16..23. No masks are
coalesced, and each write is followed by wait_loop(2). `printf_error` retains the
original RAM format address and meaningful integer argument count.

`set_voice_volume(channel,side,value)` replaces each sequencer SPU volume-register
halfword write at channel offsets +0 (left) and +2 (right). The callback receives
side 0 then side 1 with the exact 16-bit values; it does not maintain a simulated
SPU register image.

The existing `src/audio/sp_player.h` provides SpSfx bank installation, SFX sequence
play/stop, and SpMix's shared 24-voice mixer; `sp_voice_pool.h`, `sp_adsr.h` and
`sp_reverb.h` own the native voice lifecycle. A future adapter should connect the
RAM sequencer's selected channel to those native voices, apply these ADSR/key-off
callbacks to that exact channel, and retain integer ADSR/reverb processing. The
current public sp_sfx_stop(effect_handle) stops an entire effect; it cannot directly
implement a per-channel key-off mask without a channel mapping/API extension.
Similarly, the request slot is an SFX sequence handle, not a hardware voice. The
later tick must consume AE_SV_EVENTS and the pending left/right/scale fields before
starting native sample voices. This task does not change src/audio or pretend that
calling sp_sfx_play_id already implements this bridge.

## Findings

* SfxVoiceStartEx's last three arguments are **left volume, right volume, scale**;
  older symbol comments describe pitch/pan. The normal positional path passes its
  calculated two volumes and scale 4096. Loop mode != 0 passes (1,1,4096).
* SfxLookupTone updates its header/table globals before most validity checks and
  reads the loaded-bank header even when the bank is disabled. Do not add early
  validation that changes these writes. Missing program offset FFFF is rejected;
  note offset FFFF is returned as base+FFFF rather than rejected.
* Voice allocation tests 0x00FF00FF at +0x24, not the entire flags word. Its
  +0xE6 bank flag is only set when pending, never unconditionally reset.
* Pending pan and flag survive allocation failure and can affect the next start.
* Stop accepts signed high-bit handles when their masked index is below 24; the
  per-channel stored u16 handle is compared against signed handle or masked index.
  Full high-bit u16 channel values do not equal a negative sign-extended handle.
  These handles clear ADSR1/2 before key-off, so a native adapter must update the
  active SpAdsr register parameters without restarting its envelope, then release.

## Validation

`tools/mips/check_sound.py` builds the module with ae_ram.c and ae_sdk.c using
gcc C11, -Wall -Wextra -Werror. The default run takes about 11 seconds, performs
1,861 differential cases and 775 separate missing-callback checks, and writes
extract/checks/results/check_sound.json. Main RAM outside 801FF000..80200000,
the entire scratchpad, defined return values, and ordered hook arguments match.
SfxVoiceStop has no defined return; its original stale v0 is not an API result.
No gameplay RAM masks or shared uninitialized values are used.

Each table below is a ported and diffed original function in src/sound/ae_sound.c.
Counts are direct calls at the default seed/scale; call-tree executions also add
instruction coverage but do not inflate direct-case counts.

| Function | Address | Cases | Executed instructions |
|---|---|---:|---:|
| SfxIsQueued | 800885A8 | 26 | 21/21 |
| SoundDelayedQueueAdd | 800885FC | 79 | 28/28 |
| SoundLoopAllocSlot | 800881D0 | 25 | 15/15 |
| SoundLoopFree | 8008820C | 50 | 29/29 |
| SeqSetTickFlag | 800A1378 | 7 | 8/8 |
| SpuVolumeClamp | 800A119C | 98 | 24/24 |
| SfxLookupTone | 8009E82C | 207 | 69/69 |
| SfxVoiceAlloc | 8009E940 | 299 | 91/91 |
| SfxVoiceStart | 800A3158 | 101 | 24/24 |
| SfxVoiceStartEx | 800A31B8 | 101 | 57/57 |
| SfxVoiceStop | 800A329C | 26 | 100/100 |
| SoundEmitterCalcPan | 80087C8C | 383 | 144/149 |
| PlaySfxAt | 80087A20 | 207 | 69/69 |
| SoundLoopStartEx | 800882C4 | 241 | 92/92 |
| SoundLoopStopAndFree | 800880D0 | 11 | 20/20 |

The existing SDK helpers are reused directly and run as original instructions on
the oracle side: SquareRoot0 33/33, VectorNormalSS 12/12, normalization core 48/48.
SquareRoot0's inventory length misses its zero-result return at 8003043C/440;
the checker includes both instructions. Total coverage is 884/889 instructions;
every unexecuted instruction has a registered impossibility proof:

* Pan 80087D54 and 80087D64..D6C: attenuation requires distance > 2048 and
  signed range >= distance. The denominator range-2048 is strictly positive,
  so the divide-zero and divide-overflow trap paths cannot execute.
* Pan 80087E78: each normalized component has magnitude below 8192: the norm
  LUT is at most 4096 and abs(input) < 2^(shift+1). The cross-product magnitude
  is below 2*8192^2, so negating a nonpositive cross cannot produce a negative
  signed result. Quantization can produce components above 4096; an append-only
  correction supplements the registry's initial overly tight bound. Neither row
  widens the excluded instruction range.

Fixtures load the real EXE and area-1 member, then exercise all 198 original
effect records with common SShd 00B and stage SShd from 01B. Emitter coordinates
come from the area's monkey nav nodes. Synthetic fixtures cover every free slot,
full pools, signed and high-bit handles, queue and output aliasing, truncation,
phase-bit clamping, missing tones, disabled banks, pending flags, and wait timeout.
Random RAM surrounds the structured fields. Pan additionally tests random emitter,
camera and target positions, zero vectors, attenuation boundaries, wrapped squared
distances and normalization overflow (original exception versus AeRam error, with
matching RAM at the fault). A persistent chain tests duplicate requests, pool
exhaustion, retained pending settings, free and reuse without resetting RAM.

The checker uses the pinned PCSX-ReARMed GTE oracle already provided by the project
(8b6c163575e6ce9a0537553e0d4ed4225315955e); no emulator code is added to src/.
`--mutate` compiles 51 independent wrong copies under ignored extract/mips;
all 51 are caught, with zero survivors or dropped mutations. Copies include
wrong boundaries, flags, offsets, volume/ADSR values, key-off masks and halves,
hook arguments, queue IDs and retained state. `--trapv --coverage` also passes.

tools/verified.csv has 15 appended diffed rows with the counts above;
tools/mips/check_exclusions.csv has only appended stack/proof rows.
tools/check_verified.py adds src/sound/ae_sound.c to SCAN and passes (0 errors;
existing warnings for the other checkers' missing results in this worktree).
Git refuses this worktree's ownership even for read-only queries, so the standard
result writer records an empty HEAD field; no git settings were changed.

Remaining host work is implementation of the six AeSoundHw callbacks documented
above and the later sequencer-to-native-voice bridge. The new sequencer module
adds `set_voice_volume` for its left/right SPU writes; no request-layer game logic
is left hooked. Protected modules were not changed. Changes are left uncommitted
for the project owner's review; no game data or third-party code is added.
