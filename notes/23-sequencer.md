# Sound sequencer (SEQ interpreter), SSsq format and SPU helpers

Status: **the SSsq song format is validated against all 43 sound banks** (`tools/dump_seq.py`). The driver internals (track / voice records, SPU shadow, tone records) are read from the decompile and are still tentative; names are in `tools/symbols.csv` (batches 84-85).

## SSsq chunk format (validated)

One SSsq chunk per bank member (offsets from `tools/dump_sound.py`; chunk = 12 byte header + `SSsq`, but the fields below are relative to the start of the 12 byte header). It holds exactly **one song** (the bank's BGM).

| Offset | Content |
|---|---|
| +0x00 u16 | master volume (always 100) |
| +0x02 u16 | ticks per quarter note (always 480) |
| +0x04 u16 | initial tempo in BPM (e.g. 170, 240, 88) |
| +0x06 u16 | 0 |
| +0x0C | `SSsq` magic |
| +0x10 .. +0x110 | 16 channel records of 0x10 bytes: [0]=0, [1]=channel number, [2]=tone program (0xFF = none; per bank), [3]=volume 100, [4]=pan 0x40 (centre), [0xC]=0x10, [0xE]=100 (the same in all 688 records except [2]) |
| +0x110 | event stream (the restart position used by `SeqCmd_MetaEndOfTrack`) |

Event stream: `status [data...] delta`. The status byte is a MIDI status (high nibble = type, low nibble = channel; no running status in the data):

| Status | Data bytes | Meaning |
|---|---|---|
| 0x8n | note, velocity | note off |
| 0x9n | note, velocity | note on |
| 0xAn | 2 | (not present in the data) |
| 0xBn | controller, value | controller (see below) |
| 0xCn | program | program (tone) change |
| 0xEn | 1 byte | pitch bend; **a single 7 bit byte** (0x40 = centre, whole range 0..127 used) |
| 0xFF | `51` lo hi | tempo: u16 BPM, little endian |
| 0xFF | `2F` | end of track (followed by one delta byte) |

`delta` is a MIDI style variable length quantity (7 bits per byte, bit 7 = more, up to 4 bytes) giving the ticks **after** this event. Ticks per second = BPM * 480 / 60.

Controllers present in the data: 1 (values 0-121, modulation / effect depth), 2 (5 uses, values 95-110), 6 (data entry: 127 right after the loop start), 7 (volume), 10 (pan), 0x40 (sustain pedal, values 0 / 127), 0x63 (NRPN select: **20 = loop start, 30 = loop end**, once each per song, 86 uses over the 43 songs). Controllers 0x41, 0x60, 0x62 are handled by the driver but never occur in the song data.

### Validation results

* All 43 banks decode linearly from +0x110 to the end-of-track meta (`FF 2F`) with no bad status byte and exactly one trailing delta byte.
* Note on count equals note off count (175,403 each over all banks); every note off matches an earlier note on of the same channel / note, no note is left hanging.
* Song lengths at the stated tempo are 12 s to 190 s (e.g. 01B 147 s, 04E 178 s), which fits the game's music.
* `python tools/dump_seq.py midi` writes a standard MIDI file (format 0, 480 PPQ) per bank into `extract/midi/` (gitignored); the files parse back with the same note count and tick total. They can be played in any MIDI player (instrument mapping is not meaningful without the SShd tone tables).

## Driver (tentative, from the decompile)

The track and voice arrays are typed in Ghidra as `g_SeqTracks` (`SeqTrack[24]`) and `g_SeqVoices` (`SeqVoice[24]`, `tools/structs.txt`); the driver state globals are named `g_Seq*` / `g_Spu*` (batch 105).

`SeqTick` (0x8009BA90) runs once per frame (from `FrameEndUpdate` and from UI loops that run without the main loop).

* 24 tracks of 0x120 bytes at 0x800F5AC0 are **concurrent sequence players** (one song plus sound effect sequences), not the 16 MIDI channels. A track holds the status / data bytes of the current event (+0x00 / +0x01 .. +0x05), the read position (`DAT_800F5ACC` stride 0x120), the sequence data pointer (+0x10), the bank index (+0x20 / `DAT_800F5AE0`), a mode flag (`DAT_800F5AE7`: 0 = song mode with the event lengths above; nonzero = extended SFX mode in which several controllers carry 3 extra bytes, e.g. controller 7 / 10 / 0x41 advance 6, controller 1 / 2 advance 5) and the delta counter (`DAT_800F5BD4`).
* 24 voices of 0x66 bytes at 0x800E07F0: note, track, program, flags, current volumes, envelope state.
* The SPU register shadow is at `DAT_800E1874` (per voice 0x10 bytes: left / right volume, pitch ...; main volume +0x1B0, CD volume +0x1B4, control bits +0x1AA). `SeqFlushSpu` (0x800A0E84) writes it out at the end of the tick.
* Bank access: `DAT_800E1184` (3 words per loaded bank) and `SeqSelectTrackData` (0x8009D7C8) pick the event data and the tone table (`DAT_800E1858`, 0x10 byte tone records, volume at +0xE).
* `SeqReadDelta` (0x800A01BC) converts the VLQ delta into frame ticks: ticks * BPM * 480 * 10 / (frame rate * 0x3C) with a remainder carry (`DAT_800F75FE` = frame rate).
* `SeqReadStatus` (0x800A03BC) reads the status and data bytes (also handles running status in other modes).

Dispatch by status nibble in `SeqTick`: 0x80 `SeqCmd_NoteOff`, 0x90 `SeqCmd_NoteOn`, 0xA0 `SeqCmd_NoteOnA0`, 0xB0 controller (1 `SeqCmd_Ctl01`, 2 `SeqCmd_Ctl02`, 6 `SeqCmd_Ctl06_DataEntry`, 7 `SeqCmd_Ctl07_Volume`, 10 `SeqCmd_Ctl0A_Pan`, 0x40 `SeqCmd_Ctl40_Sustain`, 0x41, 0x60, 0x62, 0x63 `SeqCmd_Ctl63_Nrpn`), 0xC0 `SeqCmd_ProgramChange`, 0xE0 `SeqCmd_PitchBend`, 0xF0 meta (0x2F `SeqCmd_MetaEndOfTrack`, 0x51 `SeqCmd_MetaTempo`).

Pitch / volume / pan: pitch table at 0x800B38D0 (12 semitone rows with 0x10 fine steps, octave by shifting, `SeqPitchFromNote`); volume = tone volume * layer volume * track volume * pan curve (0x800B4190) >> 14 / >> 7 (`SeqCalcVoiceVolume`), bit 15 selects the phase inverted mode; pan table 0x800B3D90 (`SeqCalcPan`).

## Open items

* The SShd tone tables are decoded (notes/24-tone-tables.md, `tools/dump_tones.py`); the sound effect sequences (extended SFX mode) turned out to be the tiny `(program, note)` sequences of the SFX chunk, also validated there.
* Confirm controller 2 and the effects of controllers 0x41 / 0x60 / 0x62 in the SFX sequences.
* Software players: `tools/render_seq.py` / `render_sfx.py` (Python reference, notes/24, 29) and the native C player `tools/native_audio/` (notes/30). The native player now models the music allocator `SeqAllocVoice` (0x8009DAB4) and the effect allocator `SeqVoiceAlloc` (0x8009DCA0: reuses the voice of the same effect, up to `DAT_800F75C3` = 8 effect voices, steals by priority +0x1E) in one shared 24-voice pool, the 60 Hz frame-quantised clock, integer ADSR counters and integer reverb (notes/30, last sections). Still missing: portamento. Controller 1 is the modulation depth (it feeds the LFO of layers with flag 0x20 and forces the modulation flag of active voices); controller 2 sets the LFO speed = 240 / (60 - floor(value * 58 / 127)) (notes/24).
