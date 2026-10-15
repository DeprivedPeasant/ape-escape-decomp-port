#ifndef AE_SOUND_H
#define AE_SOUND_H
#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Game-side sound requests. Host owns loaded banks and the sequencer tick.
 * No hardware registers are stored or emulated by this module. */
#define AE_SOUND_EFFECTS       0x800B2D08u /* 8-byte records: bank, program, note, last voice (s16) */
#define AE_SOUND_LOOPS         0x800BF370u /* 24 x 0x1C */
#define AE_SOUND_QUEUE         0x800BF610u /* 12 x 8 */
#define AE_SOUND_BANK_IDS      0x800BF670u /* s16 slot -> loaded bank */
#define AE_SOUND_LISTENER      0x800BF688u /* three s16 */
#define AE_SOUND_VOICES        0x800F5AC0u /* 24 x 0x120 */
#define AE_SOUND_STATE         0x800F75C0u
#define AE_SOUND_BANKS         0x800E1180u /* 12-byte loaded-bank records */
#define AE_SOUND_CHANNELS      0x800E07F0u /* 24 x 0x66 */
#define AE_SOUND_TICK_FLAG     0x800B4424u
#define AE_SOUND_BANK_HEADER   0x800E1870u
#define AE_SOUND_SFX_TABLE     0x800E186Cu
#define AE_SOUND_TONE_TABLE    0x800E1858u
#define AE_SOUND_VOLUME_CURVE  0x800E1860u
#define AE_SOUND_SPU_POINTER   0x800E1874u /* original only; native callbacks replace accesses */
#define AE_SOUND_CAMERA        0x800F4350u

enum AeSoundQueueField {
    AE_SQ_TYPE=0, AE_SQ_ID=2, AE_SQ_LEFT=4, AE_SQ_RIGHT=6, AE_SQ_SIZE=8
};
enum AeSoundEffectField {
    AE_SE_BANK=0, AE_SE_PROGRAM=2, AE_SE_NOTE=4, AE_SE_LAST_VOICE=6, AE_SE_SIZE=8
};
enum AeSoundBankField { AE_SB_LOADED=0, AE_SB_HEADER=4, AE_SB_SIZE=12 };
enum AeSoundHeaderField { AE_SH_CURVE=0x14, AE_SH_SEQUENCES=0x1C, AE_SH_TONES=0x20 };
enum AeSoundTableField { AE_ST_LAST_INDEX=0, AE_ST_VALUES=2 };
enum AeSoundCameraField {
    AE_CAM_PAN_ENABLED=0x13A, AE_CAM_EYE_X=0x258, AE_CAM_EYE_Z=0x25C,
    AE_CAM_TARGET_X=0x260, AE_CAM_TARGET_Z=0x264
};

enum AeSoundLoopField {
    AE_SL_ID=0, AE_SL_VOICE=2, AE_SL_EXTRA=0x10, AE_SL_ACTIVE=0x12,
    AE_SL_INDEX=0x14, AE_SL_FLAGS=0x16, AE_SL_SIZE=0x1C
};
enum AeSoundVoiceField {
    AE_SV_READ_POS=0x0C, AE_SV_EVENTS=0x10, AE_SV_PROGRAM=0x1D,
    AE_SV_NOTE=0x1F, AE_SV_BANK=0x20, AE_SV_ALLOC_FLAGS=0x24,
    AE_SV_PAUSED=0x25, AE_SV_ACTIVE=0x26, AE_SV_EFFECT_MODE=0x27,
    AE_SV_MOD_A=0x31, AE_SV_MOD_B=0x33, AE_SV_FLAG_E2=0xE2,
    AE_SV_FLAG_E3=0xE3, AE_SV_POSITIONAL=0xE5, AE_SV_BANK_FLAG=0xE6,
    AE_SV_SCALE=0xE8, AE_SV_LEFT=0xEA, AE_SV_RIGHT=0xEC,
    AE_SV_FLAG_100=0x100, AE_SV_FLAG_101=0x101, AE_SV_DELTA_FRAMES=0x114,
    AE_SV_SIZE=0x120
};
enum AeSoundStateField {
    AE_SS_ENABLED=3, AE_SS_HEADER=8, AE_SS_PENDING_PAN=0x1E,
    AE_SS_PENDING_FLAG=0x1F, AE_SS_SCALE=0x20, AE_SS_LEFT=0x22, AE_SS_RIGHT=0x24
};
enum AeSoundChannelField {
    AE_SC_STATE=0, AE_SC_VOICE=6, AE_SC_RELEASE=8, AE_SC_ACTIVE=0x1A, AE_SC_SIZE=0x66
};

typedef struct AeSoundHw {
    void *user;
    /* SpuWaitIdle @800A1398: wait for sequencer busy flag, 0 ready / 1 timeout.
     * Host synchronizes its audio thread here; callback must preserve game RAM. */
    int32_t (*wait_idle)(void *user, AeRam *r);
    /* SpuWaitLoop @8009D79C: original CPU delay based on units * 768.
     * A native host may make this a no-op. Preserve RAM. */
    void (*wait_loop)(void *user, AeRam *r, uint32_t units);
    /* Original writes zero to SPU channel ADSR1 (+8) then ADSR2 (+10).
     * channel is the sequencer channel index, not the SFX handle. */
    void (*set_voice_adsr)(void *user, AeRam *r, uint32_t channel, uint16_t adsr1, uint16_t adsr2);
    /* A single original key-off halfword write: half=0 (+0x18C) or 1 (+0x18E).
     * Exact 16-bit mask, ordered before wait_loop(2). */
    void (*voice_key_off)(void *user, AeRam *r, uint32_t half, uint16_t mask);
    /* printf @800A3720: fmt points into game RAM; up to three integer arguments.
     * argc indicates meaningful args; no string data is embedded here. */
    void (*printf_error)(void *user, AeRam *r, uint32_t fmt, uint32_t argc,
                         int32_t a, int32_t b, int32_t c);
    /* Sequencer SPU volume-register write: side 0 = left (+0), side 1 = right (+2).
     * Called once per original halfword write, in left-then-right order. */
    void (*set_voice_volume)(void *user, AeRam *r, uint32_t channel, uint32_t side, uint16_t volume);
} AeSoundHw;
/* Every needed NULL callback fails closed via ae_ram_fail; check it after calls. */
int32_t ae_SfxIsQueued(AeRam *r, int32_t id);
int32_t ae_SoundDelayedQueueAdd(AeRam *r, int32_t type, int32_t id, uint32_t pan);
uint32_t ae_SoundLoopAllocSlot(AeRam *r);
int32_t ae_SoundLoopFree(AeRam *r, int32_t id, int32_t voice);
int32_t ae_SeqSetTickFlag(AeRam *r, int32_t flag);
int32_t ae_SpuVolumeClamp(AeRam *r, int32_t maximum, int32_t volume);
uint32_t ae_SfxLookupTone(AeRam *r, uint32_t bank, uint32_t program, uint32_t note);
int32_t ae_SfxVoiceAlloc(AeRam *r, uint32_t bank, uint32_t program, uint32_t note);
int32_t ae_SfxVoiceStart(AeRam *r, const AeSoundHw *hw, uint32_t bank, uint32_t program, uint32_t note);
int32_t ae_SfxVoiceStartEx(AeRam *r, const AeSoundHw *hw, uint32_t bank, uint32_t program,
                         uint32_t note, int32_t left, int32_t right, int32_t scale);
void ae_SfxVoiceStop(AeRam *r, const AeSoundHw *hw, int32_t voice);
int32_t ae_SoundEmitterCalcPan(AeRam *r, uint32_t pos, int32_t range, uint32_t out);
int32_t ae_PlaySfxAt(AeRam *r, const AeSoundHw *hw, int32_t id, uint32_t pos);
int32_t ae_SoundLoopStartEx(AeRam *r, const AeSoundHw *hw, int32_t mode, int32_t id, int32_t extra);
int32_t ae_SoundLoopStopAndFree(AeRam *r, const AeSoundHw *hw, int32_t id, int32_t voice);
#ifdef __cplusplus
}
#endif
#endif
