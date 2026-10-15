# SShd tone tables and SFX sequences

Status: **validated against all 43 sound banks plus the common SFX bank** with `tools/dump_tones.py check`. The container, the SSsq song format and the sequencer are in notes/18 and notes/23.

Every bank member holds up to two `SShd` chunks (offsets relative to the 12 byte chunk header; the driver's bank pointer is the chunk start) and up to two sample blocks (VB0, VB1):

* **SShd0 = music instrument bank**, samples in **VB0**, played by the song in the SSsq chunk.
* **SShd1 = sound effect bank**, samples in **VB1**.
* The common SFX bank is a standalone member (0x00B; 0x01E is a small one) with only an SShd chunk of the SFX kind.

## SShd header

| Offset | Content |
|---|---|
| +0x00 u32 | chunk size |
| +0x04 u32 | size of the matching sample block |
| +0x0C | `SShd` |
| +0x10 u32 | music tone table offset, `0xFFFFFFFF` in an SFX chunk |
| +0x14 u32 | volume curve (2 byte prefix, then 128 bytes; the identity 0..127 in every shipped bank) |
| +0x18 u32 | LFO / modulation tables: u16 count (7), u16 offsets of 0x40 byte curves (sine, triangle, ramps, ...) |
| +0x1C u32 | SFX sequence table (SFX chunks; `0xFFFFFFFF` in a music chunk) |
| +0x20 u32 | SFX channel records + SFX tone table (SFX chunks) |

## Music tone table (validated)

`u16 last program`, then `u16` offsets (relative to the table start, `0xFFFF` = unused) of the program records.

Program record: 8 byte header + layers (tones) of 0x10 bytes each.

| Header byte | Meaning |
|---|---|
| [0] | low 7 bits = last layer index (layers = n + 1). Bit 7 set: all matching layers play together; clear: key split, only the first layer that contains the note plays. 0xFF = **key indexed kit** (drum kits in the music banks and all SFX programs): layer index = note - [6], layers = [7] - [6] + 1, no key range check, unused slots are 16 bytes of 0xFF |
| [1] | program volume (100) |
| [2] | program pan (0x40 centre) |
| [4] | pitch bend range in semitones (12) |
| [5] | modulation speed |
| [6], [7] | first / last key of a 0xFF program |

Layer record:

| Byte | Meaning |
|---|---|
| [0], [1] | low / high key (inclusive) |
| [2] | root key (the note that plays the sample at its recorded pitch) |
| [3] | fine tune, signed, in 1/16 semitone steps (the driver adds it to the table index) |
| [4:6] | sample start in the bank's sample block, **units of 8 bytes** (byte address = value * 8) |
| [6:8] | SPU ADSR1 register |
| [8:10] | SPU ADSR2 register |
| [10] | SPU volume mode bits (high byte of the voice volume register; 0 normally) |
| [11] | layer volume (90 typical) |
| [12] | layer pan (0x40 centre) |
| [13] | pitch bend range (used unless flag 0x10) |
| [14] | modulation speed (0x7F) |
| [15] | flags: 0x01, 0x10 bend range from the program header, 0x20 modulation (LFO) enabled with the depth from controller 1, 0x40 modulation speed from the program header, 0x80 reverb on |

Validation: 43 banks, 1,424 layers (plus 116 empty kit slots); every layer start is a real ADPCM sample start in VB0 (or 0 = the leading silent block); all 175,403 note on events of the songs find their program (channel program from the SSsq channel record or program change); 428 notes (0.24 %) fall outside every layer of their program (key range or kit slot) and are silent in the game as well (`SeqNoteInToneRange` fails).

ADSR registers use the standard PlayStation SPU layout: ADSR1 = attack mode (bit 15), attack shift (14-10), attack step (9-8), decay shift (7-4), sustain level (3-0); ADSR2 = sustain mode (15), direction (14), shift (12-8), step (7-6), release mode (5), release shift (4-0).

Pitch: the SPU pitch is read from the table at 0x800B38D0, which is exactly `0x1000 * 2^(i / 192)` (16 fine steps per semitone, 12 semitones per octave, octave shift on top; checked against the table). Index = semitone distance from the root * 16 + fine tune + (bend - 0x40) * bendRange / 4. The lookup is the integer function `SeqPitchFromNote` (0x800B38D0) with signed bend/fine indexing, unsigned octave shifts and SPU clipping; it is not floating point exponentiation, which does not equal the quantised table values written to the SPU. Current engine: notes/30-native-audio.md.

## SFX chunks (validated)

* `+0x1C` table: `u16 last program`, `u16` sub table offsets (relative to the table start); a sub table is `u16 last note` followed by `u16` offsets (relative to the table start) of **tiny event sequences**. So a sound effect is `(program, note)` -> sequence. PlaySfx maps its id to `(bank slot, program, note)` through the 8 byte table at 0x800B2D08 (200 ids; slot 0 = common bank 0x00B with programs 0-7, slot 2 = the stage's SFX bank with program 0).
* SFX sequence events (588 sequences, all end with `FF 2F`): `A0 note velocity program` (start a voice: the SFX tone table below chooses the layer), `B0 ctl ...` controller events with extra bytes in this mode (lengths including the status: ctl 1 = 5, 2 = 5, 7 = 6, 10 = 6, 0x41 = 6, 0x60 = 5; ctl 6 / 0x40 / 0x62 / 0x63 = 3), each followed by a variable length delta (`80 00` is a two byte zero), and `FF 2F`. The delta unit is the same tick as in songs.
* `+0x20` block: 24 channel records of 0x10 bytes after a 0x10 byte header (identical in shape to the SSsq channel records, [0xC] = 4), then at `+0x190` from the block start the **SFX tone table** with the same shape as the music table; its programs have header byte 0xFF: layer index = note - [6] (first key), last key = [7] (e.g. keys 0x20..0x24 = 5 layers).
* Validation: every `A0` event (804 over all banks) resolves to an existing layer, and in all but 4 the layer's sample start is a real ADPCM sample start in **VB1**. The 4 exceptions are all in bank 0x124: they point at 0xBC40, inside a sample of VB1 (48,192 bytes), so that layer starts in the middle of a sample (a loop or tail entry point, or a data error).

## Renderer (`tools/render_seq.py`)

`python tools/render_seq.py 01B` renders a bank's song to `extract/wav/01B.wav` (44.1 kHz stereo, all 43 banks render in 2-10 s each). It implements exactly the pieces above: tempo / loop unrolling (`--loops N`), channel volume / pan / bend / sustain pedal timelines applied to sounding notes, layer selection (layered, key split, kit), pitch from the integer `SeqPitchFromNote` table (older revisions of this renderer used `2^((note - root + fine/16 + range*(bend-64)/64)/12)`, which is only approximate), the game's integer volume formula with the pan tables read from the EXE (0x800B3D90 / 0x800B4190), SPU ADPCM with loop flags, cubic resampling (this describes the legacy `tools/render_seq.py` and the legacy SFX path only; the music path of the native engine now keeps the ADPCM predictor history across loop repeats, uses SPU Gaussian interpolation and honours the all-ones sustain-rate freeze, see notes/30-native-audio.md, music fidelity work is owned by Codex), SPU ADSR envelopes and the SPU reverb on layers with flag 0x80 (see below). The modulation LFO (layers with flag 0x20, 51 layers in 18 banks) works as in the driver: per 60 Hz frame the phase advances by the voice speed (+0x3C; the channel record speed +0xC, 0x10 in every song, unless program flag 0x40 selects program header [5] as the curve) and wraps at 0xF0 (then restarts at speed / 2); the value is `wave[phase >> 2] * depth / 255 - ((depth + 1) >> 1) + 64` with `wave` the 0x40 byte curve of the SShd +0x18 table and `depth` the channel's controller 1 value. It does **not** replace the pitch bend: for normal music (voice mode +0x1C == 0) `SeqUpdateVoices` first decomposes the written bend into a normalised note and fine offset (signed truncation) and then applies the LFO through `SeqPitchFromNote` with bend range 1 (verified against 1500 executions of the original instructions, notes/30). `NoteOnA0` writes mode 4 (non-additive branch). Controller 2 sets speed = 240 / (60 - floor(value * 58 / 127)); controller 1 forces the modulation flag of active voices, even at depth 0 (these two edge cases are still follow-up work in the renderer). Voice allocation follows the driver's music allocator `SeqAllocVoice` (0x8009DAB4) for 24 voices: a voice is free once its envelope is below 0x10 (or its one shot sample ended); a new note takes the next free voice round robin, else steals the stealable voice (from key on, or from key off for layers with flag 0x01) with the lowest age, else the first voice; ages are the active voice count at key on, decremented when an older voice is freed (`--no-voice-limit` turns this off). Over the 43 songs only 5 ever need to steal (0AD 1,642 voices: channel 7 holds 126 notes under the sustain pedal for 10-125 s; 04C 46, 0D5 38, 01D 4, 115 3). In the game the sound effects share the 24 voices (`SeqVoiceAlloc` 0x8009DCA0, up to 8 effect voices), which the renderer does not model. Not implemented: portamento and the frame quantised event timing.

Checks done (no listening possible in this environment):
* ADSR envelopes agree with a per-sample reference simulation of the SPU rules (max error 0.03 in level for typical settings, 0.12 for the instant exponential attack).
* The resampler produces the exact frequency (880 Hz test tone at pitch x2 gives 882 Hz).
* Pitch semantics: for 41 strongly periodic looped samples, 21 have their measured fundamental within 0.25 semitone of `root + fine/16` (the rest are octave or multi-cycle ambiguities), so root key / fine tune behave as described.
* All 43 songs render with the expected lengths (12 s to 206 s including the reverb tail) and 0-97 silent notes per song (the notes without a layer).

## SPU reverb (validated)

`SpuSetReverbPreset(n)` (0x800A2030) copies a 0x21 halfword record from 0x800B41D0 + (n - 1) * 0x42: the reverb work
area start (mBASE, 8 byte units) and the 32 SPU reverb registers 0x1F801DC0..0x1F801DFE (dAPF1, dAPF2, vIIR, vCOMB1-4,
vWALL, vAPF1/2, mLSAME ... mRAPF2, vLIN, vRIN). The 9 records are the standard SPU presets (1 room, 2-4 studio
small / medium / large, 5 hall, 6 space, 7 echo, 8 delay, 9 half echo; e.g. preset 4 mBASE 0xF204, 14,320 sample
work area). When a stage's sounds are uploaded the game calls `SpuSetReverbPreset(4)` and `SpuSetReverbVolume(10, 10)`
(reverb output volume 10 << 8 = 0x0A00 per side; `SeqSetMonoFlag` mixes it to mono). Songs can change it through NRPN
data entry parameters 0x0F (preset), 0x10 (volume), 0x11 (`SpuSetReverbFeedback`, echo / delay only) and 0x12 / 0x13
(`SpuSetReverbDelayTime`), but none of the 43 songs does.

`tools/render_seq.py` runs the documented hardware algorithm (22.05 kHz; SAME / DIFF IIR stages with the wall
feedback, four comb taps, two all-pass stages, 16 bit saturation of the buffer) on the sum of the layers with reverb
flag 0x80, with these registers (default preset 4, volume 10; `--reverb-preset`, `--reverb-vol`, `--reverb-mode conv`
for the old convolution). It is vectorised in blocks shorter than the smallest write-to-read distance of the
buffer and checked against a per-sample implementation of the same rules for all 9 presets (equal to 1e-17; only
the saturation of the IIR state can differ, at clipping levels). Impulse response checks: preset 4 starts after
43 ms and decays by 60 dB in 2.4 s, preset 1 0.9 s, preset 5 2.2 s, preset 7 is a 0.74 s echo. All 43 songs render
in 5-15 s each.

## Sound effect playback (`tools/render_sfx.py`, notes/29)

Effect tracks run at a fixed 60 BPM / 480 ticks per beat (`SeqReadDelta` sets them for the extended mode), so a tick is
1/480 s. Extended controllers `B0 ctl time value program note` act on the voice playing (program, note): 0x07 volume
ramp and 0x0A pan ramp (linear from the current value to `value` over time * 4 frames), 0x41 pitch glide (`value` bit 7
clear = up value / 10 semitones, set = down (256 - value) / 10, linear over time * 4 frames, `SeqCmd_Ctl41` and the
per frame voice update). `A0 note 0 program` is a key off. The common bank 0x00B takes its samples from member 0x00A,
0x01E from 0x01F; stage effects (table slot 2, ids 172..197) come from the second SShd / VB of the stage bank.

