/* Portable C player for Ape Escape sound banks (songs) - the native port's music driver.
 *
 * Mirrors the model of tools/render_seq.py (notes/23-sequencer.md, notes/24-tone-tables.md): SSsq events with tempo
 * changes and loop markers, SShd programs / layers (layered, key split, key indexed kits), the game's integer volume
 * and pan/pitch laws (tables read from the EXE), streaming ADPCM with repeat history, PS1 Gaussian interpolation, ADSR envelopes,
 * modulation LFO, the 24 voice allocator (SeqAllocVoice) and the SPU reverb with the game's preset registers.
 *
 * The caller provides the sound bank member and the game EXE image (SCUS_944.23) - no game data is embedded here.
 * Output: 44100 Hz stereo float samples (interleaved L, R), the same scale as render_seq.py before normalisation.
 */
#ifndef SP_PLAYER_H
#define SP_PLAYER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SP_RATE 44100

enum {
    SP_NO_VOICE_LIMIT = 1, /* do not emulate the 24 voices (no stealing) */
    SP_NO_REVERB = 2,
    SP_NO_MOD = 4,         /* no modulation LFO */
    SP_COUNTER_ADSR = 16,  /* integer SPU envelope counters; legacy reference mode remains available */
    SP_FRAME_TIMING = 8,   /* original 60 Hz delta rounding/carry; sample timing remains available for comparison */
    SP_INTEGER_REVERB = 32 /* opt in: integer reverb pipeline (reverb_pipeline.h) instead of the float waveform
                              reference; ignored with SP_NO_REVERB. Wet send is summed per 44.1 kHz frame, clamped to
                              signed16 and run through the 22.05 kHz network. Attached to an SpMix the mixer runs the
                              pipeline once for music + effects. */
};

typedef struct SpPlayer SpPlayer;

/* bank: a sound bank archive member (SShd + SSsq + VB chunks); exe: the SCUS_944.23 file.
 * loops: how often the loop section plays (1 = once, straight through; 0 = stop at the loop end; -1 = forever, as in
 * the game). reverb_preset / reverb_volume: the game uses 4 / 10. Returns NULL when the member is not a sound bank
 * (or, with SP_INTEGER_REVERB, when the reverb preset table or pipeline allocation fails). */
SpPlayer *sp_create(const uint8_t *bank, size_t bank_size, const uint8_t *exe, size_t exe_size, int loops, int flags,
                    int reverb_preset, int reverb_volume);
void sp_destroy(SpPlayer *p);

/* Render `frames` stereo frames into out (2 * frames floats). Returns the number of frames that still had sound or
 * events (0 once the song has ended and every voice is silent). */
int sp_render(SpPlayer *p, float *out, int frames);

/* Statistics: voices started, stolen, dropped, notes without a layer. */
void sp_stats(const SpPlayer *p, int *voices, int *stolen, int *dropped, int *silent);
/* Original SeqReadDelta integer clock. State is per sequence, retained through tempo changes. */
uint32_t sp_delta_frames(uint32_t delta,uint16_t tempo,uint16_t ppq,uint16_t hz,int32_t *remainder,uint8_t *round_up);


/* ------------------------------------------------------------------ sound effects (tools/render_sfx.py model)
 * An effect mixer: PlaySfx ids -> table 0x800B2D08 in the EXE (bank slot, program, note) -> the effect sequence of
 * the bank in that slot (slot 0 = SShd member 0x00B + samples 0x00A, slot 2 = the stage bank's second SShd / VB). */
typedef struct SpSfx SpSfx;

SpSfx *sp_sfx_create(const uint8_t *exe, size_t exe_size);
void sp_sfx_destroy(SpSfx *s);
/* install the SShd chunk (with its SFX tables) and the sample block of a bank slot (0..3); returns 0 on success */
int sp_sfx_bank(SpSfx *s, int slot, const uint8_t *shd, size_t shd_size, const uint8_t *vb, size_t vb_size);
/* start effect (slot, program, note) / PlaySfx id at the current output position; returns a handle or -1 */
int sp_sfx_play(SpSfx *s, int slot, int program, int note);
int sp_sfx_play_id(SpSfx *s, int id);
/* key off every voice of an effect (looping effects run until stopped) */
void sp_sfx_stop(SpSfx *s, int handle);
/* testing: release voices that got no key off after `seconds` (tools/render_sfx.py --max-seconds); < 0 = never */
void sp_sfx_auto_off(SpSfx *s, double seconds);
/* Enable the same original60Hz frame clock for effects; standalone reference mode defaults off. */
void sp_sfx_frame_timing(SpSfx *s,int enabled);
/* Select integer ADSR for subsequently started effects. */
void sp_sfx_counter_adsr(SpSfx *s,int enabled);
/* render frames (stereo floats, overwrites out); returns the number of frames that had sound.
 * The standalone effect output is always dry (no reverb); the reverb send is only consumed by SpMix. */
int sp_sfx_render(SpSfx *s, float *out, int frames);
/* Shared24voice mixer. Attach freshly created music/effects before rendering.
 * Inputs remain caller-owned; destroy mixer before destroying either input.
 * With a music player created with SP_INTEGER_REVERB the mixer sums the music and effect wet sends and runs the
 * music player's reverb pipeline once per frame after both sources; the reverb state stays owned by the SpPlayer. */
typedef struct SpMix SpMix;
SpMix *sp_mix_create(SpPlayer *music,SpSfx *effects);
void sp_mix_destroy(SpMix *m);
int sp_mix_render(SpMix *m,float *out,int frames);
void sp_mix_stats(const SpMix *m,int *active,int *effects,int *peak,int *dropped);

#ifdef __cplusplus
}
#endif
#endif
