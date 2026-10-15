# BGM and sequencer RAM port

This module is the AreaLoad music and loop setup path. Each listed original
function has one structured C implementation in `src/sound/ae_bgm.c`; fields,
tables and globals are named in `src/sound/ae_bgm.h`. Calls to the earlier sound
port go directly to `ae_sound` functions.

## Instruction call tree

```text
SoundLoopsStopAll
  SfxVoiceStop, SoundLoopFree, SoundLoopAllocSlot
  Printf @800A3720 [AeSoundHw.printf_error]
  SoundLoopStart, SoundDelayedQueueReset
SoundLoopStart
  SfxVoiceStart or SfxVoiceStartEx, SoundLoopAllocSlot
  Printf @800A3720 [AeSoundHw.printf_error]
BgmStart
  BgmSeqOpen, BgmSeqSetVolume
BgmUpdateVolume
  BgmModeWantsMusic, ProgressGetByte
  SeqTrackRearm, BgmSeqSetVolume, SeqFadeTrackIn
SeqFadeTrackIn
  BgmSeqSetVolume, SeqTrackRearm, SeqFadeInit
BgmSeqSetVolume
  SeqUpdateVoiceVolume
SeqUpdateVoiceVolume
  SeqVoiceVolumeSide (left and right)
```

Other requested functions are leaves in this tree. The calls to `SfxVoice*`,
`SoundLoop*` and `SoundDelayedQueueReset` reuse their existing ports. `Printf`
is the only unported function call in this tree and stays behind
`AeSoundHw.printf_error`. Sequencer volume-register stores use the new
`AeSoundHw.set_voice_volume` callback. Existing SFX calls can also use the
`wait_idle`, `wait_loop`, `set_voice_adsr` and `voice_key_off` callbacks described
in [sound.md](sound.md).

## Validation

`tools/mips/check_bgm.py` runs the original instructions and C on the same EXE
and real area RAM images. It loads areas 1, 2 and 5 from members 05B, 05E and
066; the sound fixtures use SSsq/SShd data from member 01B and common SShd data
from 00B. The area-1 loop definitions include IDs 172 and 173 followed by the
original sentinel. The checker compares return values, hook order and arguments,
RAM below `801FF000`, and all scratchpad bytes. SPU volume stores are captured as
ordered hardware-hook events. The sole exclusion is the `801FF000..80200000`
stack window because host and MIPS stack frames differ; no gameplay RAM is
masked and no instruction coverage gap remains.

Default run: 516 direct cases plus one missing-`set_voice_volume` check in about
two seconds. Instruction coverage is 961/961. `--trapv --coverage` also passes.
The 30 deliberate mutations (two per function) were all caught.

| Function | Address | Cases | Instructions |
|---|---:|---:|---:|
| SoundLoopsStopAll | 80086540 | 4 | 146/146 |
| SoundAreaKeepsMusic | 80086F00 | 45 | 52/52 |
| BgmStart | 80087388 | 12 | 33/33 |
| BgmModeWantsMusic | 8008740C | 99 | 20/20 |
| BgmUpdateVolume | 8008745C | 22 | 108/108 |
| SoundLoopStart | 800878E0 | 208 | 80/80 |
| SoundDelayedQueueReset | 80088578 | 3 | 12/12 |
| ProgressGetByte | 800963AC | 53 | 12/12 |
| SeqUpdateVoiceVolume | 800A0900 | 5 | 89/89 |
| SeqVoiceVolumeSide | 800A0A64 | 12 | 42/42 |
| SeqFadeInit | 800A0DB0 | 7 | 53/53 |
| BgmSeqOpen | 800A1DB0 | 24 | 99/99 |
| BgmSeqSetVolume | 800A2428 | 11 | 105/105 |
| SeqTrackRearm | 800A2ACC | 4 | 40/40 |
| SeqFadeTrackIn | 800A2E3C | 7 | 70/70 |

## Findings from the instruction checks

* `SoundLoopsStopAll` copies the definition halfword at `+0x0A` into loop
  record `+0x10`, clears the trailer word at `+0x18`, and keeps walking the
  definition list after a full loop table while printing each failed ID.
* `BgmSeqOpen` scales each SSsq channel volume in place. The source byte is at
  channel record `+0x03`, and the scaled byte is written at `+0x0E`; the channel
  records begin at sequence offset `+0x10`. The initial four event bytes are
  copied into track event state, and tempo/PPQ come from sequence offsets `+4`
  and `+2` respectively.
* `SeqUpdateVoiceVolume` loads the current channel's scaled level into voice
  field `+0x28`, computes each side through `SeqVoiceVolumeSide`, then writes
  left and right SPU halfwords in that order. Those writes are now a native
  hardware boundary, with no register simulation.
* `SeqFadeInit` returns the current master or channel volume. Whole-track fade
  setup also sets the shared active byte at track `+0x38`.
* `BgmUpdateVolume` uses fade target 110 for game mode 3; modes 8, 16, 29 and 30
  rearm and set volume 75. Modes 9, 10, 18, 23, 12, 20 and 25 rearm and use the
  configured volume. Music modes are 11, 13, 17, 19 and 24; when the special
  word is 70 and progress byte 8 is 5, their target is divided by three.

The final standard result is recorded in `extract/checks/results/check_bgm.json`.
Git ownership checks in this worktree do not return a HEAD, so its result record
has an empty `head` field; no Git settings were changed. Changes remain
uncommitted for the project owner's review.

`tools/check_verified.py` finds no BGM source or ledger errors, but exits with
five pre-existing duplicate `GimmickSwing*` ledger entries for `ae_gimmick_a.c`
and `ae_gimmick_b.c`. It also reports missing result files for other checkers.
Those unrelated rows and the checker were left untouched; the BGM entries are
append-only and validate against the recorded result.
