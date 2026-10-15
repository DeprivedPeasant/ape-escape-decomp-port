#ifndef AE_BGM_H
#define AE_BGM_H

#include "ae_sound.h"

#ifdef __cplusplus
extern "C" {
#endif

/* BGM and sequencer globals in the original RAM image. */
#define AE_BGM_VOICE                    0x800BF676u
#define AE_BGM_START_FADE_STATE         0x800BF678u
#define AE_BGM_VOLUME                   0x800BF67Eu
#define AE_BGM_UPDATED_VOICE            0x800BF686u
#define AE_BGM_LOOP_DATA_POINTERS       0x800B29B0u
#define AE_BGM_PROGRESS_BYTES           0x800DFDD8u
#define AE_BGM_GAME_MODE                0x800F4470u
#define AE_BGM_AREA_INDEX               0x800F4476u
#define AE_BGM_SPECIAL_MODE_WORD        0x800E38B4u
#define AE_BGM_SEQUENCE_DATA_BASE       0x800E1854u
#define AE_BGM_CURRENT_CHANNEL_RECORD   0x800B4DE0u
#define AE_BGM_CURRENT_TRACK_VOLUME     0x800B4DE4u
#define AE_BGM_TRACKS                   0x800F5AC0u
#define AE_BGM_TRACK_COUNT              24u
#define AE_BGM_TRACK_SIZE               0x120u
#define AE_BGM_TRACK_ALLOC_MASK         0x00FF00FFu
#define AE_BGM_SEQUENCER_VOICES          0x800E07F0u
#define AE_BGM_SEQUENCER_VOICE_COUNT    24u
#define AE_BGM_SEQUENCER_VOICE_SIZE     0x66u
#define AE_BGM_PROGRESS_MASK            0x0003FFFCu
#define AE_BGM_LOOP_POINTER_STRIDE      8u
#define AE_BGM_LOOP_COUNT               24u
#define AE_BGM_LOOP_DEFINITION_STRIDE   0x10u
#define AE_BGM_LOOP_DEFINITION_FLAGS    0x02u
#define AE_BGM_LOOP_DEFINITION_FIRST    0x04u
#define AE_BGM_LOOP_DEFINITION_EXTRA    0x0Au
#define AE_BGM_LOOP_DEFINITION_LAST     0x0Eu
#define AE_BGM_LOOP_TRAILER_WORD        0x18u
#define AE_BGM_LOOP_TYPE_MASK           0xF000u
#define AE_BGM_LOOP_TYPE_KEEP           0x5000u
#define AE_BGM_LOOP_TYPE_DIRECT_RESET   0x7000u
#define AE_BGM_LOOP_ID_MASK             0x0FFFu
#define AE_BGM_LOOP_START_ERROR         0x800A6240u
#define AE_BGM_LOOP_SLOT_ERROR          0x800A625Cu
#define AE_BGM_LOOP_TABLE_ERROR         0x800A6200u
#define AE_BGM_SSQ_MAGIC_OFFSET         0x0Cu
#define AE_BGM_SSQ_MASTER_VOLUME        0x00u
#define AE_BGM_SSQ_TPQ_OFFSET           0x02u
#define AE_BGM_SSQ_TEMPO_OFFSET         0x04u
#define AE_BGM_SSQ_CHANNELS_OFFSET      0x10u
#define AE_BGM_SSQ_CHANNEL_SIZE         0x10u
#define AE_BGM_SSQ_CHANNEL_VOLUME       0x03u
#define AE_BGM_SSQ_CHANNEL_COUNT        16u
#define AE_BGM_SSQ_CHANNEL_SCALED       0x0Eu
#define AE_BGM_SSQ_EVENT_OFFSET         0x110u
#define AE_BGM_QUEUE_COUNT              12u
#define AE_BGM_HEADER_TONE_OFFSET       0x20u
#define AE_BGM_TONE_HEADER_SIZE         0x10u
#define AE_BGM_SSQ_CHANNEL_RECORD_START 0x10u
#define AE_BGM_TRACK_CLEAR_COUNTER      0x18u
#define AE_BGM_TRACK_BGM_SEQUENCE       0x800F4580u
#define AE_BGM_TRACK_BGM_BANK           0x800BF672u
#define AE_BGM_HEADER_REARM_WORD        0x10u
#define AE_BGM_SSQ_MAGIC               0x71735353u

/* Fields used by these original functions. */
enum AeBgmTrackField {
    AE_BT_EVENT_STATUS = 0x00,
    AE_BT_EVENT_DATA = 0x01,
    AE_BT_READ_POSITION = 0x0C,
    AE_BT_CHANNEL_RECORDS = 0x10,
    AE_BT_BANK = 0x20,
    AE_BT_ACTIVE = 0x24,
    AE_BT_PAUSED = 0x25,
    AE_BT_EVENT_SUBSTATUS = 0x26,
    AE_BT_EFFECT_MODE = 0x27,
    AE_BT_FADE_TRACK_CLEAR = 0x36,
    AE_BT_FADE_TRACK_ACTIVE = 0x38,
    AE_BT_FADE_PROGRAM_ACTIVE = 0x3A,
    AE_BT_FADE_PROGRAM_TARGET = 0x4A,
    AE_BT_FADE_PROGRAM_START = 0x5A,
    AE_BT_FADE_PROGRAM_CURRENT = 0x6A,
    AE_BT_FADE_PROGRAM_VOLUME = 0x7A,
    AE_BT_FADE_TRACK_FLAG = 0x8A,
    AE_BT_FADE_TRACK_TARGET = 0x9A,
    AE_BT_FADE_TRACK_START = 0xAA,
    AE_BT_FADE_TRACK_CURRENT = 0xBA,
    AE_BT_FADE_TRACK_VOLUME = 0xCA,
    AE_BT_REARM_STATE = 0xE4,
    AE_BT_TEMPO = 0x104,
    AE_BT_PPQ = 0x106,
    AE_BT_DELTA = 0x114
};

enum AeBgmVoiceField {
    AE_BV_ACTIVE = 0x00,
    AE_BV_PROGRAM = 0x04,
    AE_BV_TRACK = 0x06,
    AE_BV_VOLUME_A = 0x28,
    AE_BV_VOLUME_B = 0x2A,
    AE_BV_VOLUME_C = 0x2C,
    AE_BV_VOLUME_D = 0x2E,
    AE_BV_PAN_LEFT = 0x30,
    AE_BV_PAN_RIGHT = 0x32,
    AE_BV_IS_EFFECT = 0x1A,
    AE_BV_BANK = 0x22,
    AE_BV_PHASE = 0x4A
};

int32_t ae_SoundLoopsStopAll(AeRam *r, const AeSoundHw *hw);
int32_t ae_SoundAreaKeepsMusic(AeRam *r, int32_t from_area, int32_t to_area);
int32_t ae_BgmStart(AeRam *r, const AeSoundHw *hw);
int32_t ae_BgmModeWantsMusic(AeRam *r);
int32_t ae_BgmUpdateVolume(AeRam *r, const AeSoundHw *hw);
int32_t ae_SoundLoopStart(AeRam *r, const AeSoundHw *hw, int32_t mode, int32_t id);
int32_t ae_SoundDelayedQueueReset(AeRam *r);
uint32_t ae_ProgressGetByte(AeRam *r, uint32_t offset);
void ae_SeqUpdateVoiceVolume(AeRam *r, const AeSoundHw *hw, uint32_t channel);
int32_t ae_SeqVoiceVolumeSide(AeRam *r, uint32_t channel, uint32_t side);
int32_t ae_SeqFadeInit(AeRam *r, int32_t track, int32_t frames, int32_t program, int32_t target_volume);
int32_t ae_BgmSeqOpen(AeRam *r, int32_t bank, uint32_t sequence);
int32_t ae_BgmSeqSetVolume(AeRam *r, const AeSoundHw *hw, int32_t track, int32_t volume);
void ae_SeqTrackRearm(AeRam *r, int32_t track);
int32_t ae_SeqFadeTrackIn(AeRam *r, const AeSoundHw *hw, int32_t track, int32_t frames, int32_t volume);

#ifdef __cplusplus
}
#endif
#endif
