#include "../ui/ae_missb.h"
#include "ae_sound.h"
#include "ae_bgm.h"
#include "../world/ae_misc.h"
#include "../world/ae_stagefade.h"

#define AE_MISSB_G_GAME_MODE 0x800F4470u
#define AE_MISSB_G_SOUND_BANK_STATE 0x800BF670u
#define AE_MISSB_G_MONO_FLAG 0x800F75F2u
#define AE_MISSB_G_SOUND_VOICE_SHADOW 0x800E1874u
#define AE_MISSB_G_SOUND_BANKS 0x800E1180u
#define AE_MISSB_G_SOUND_CHANNELS 0x800E07F0u
#define AE_MISSB_G_SOUND_SEQUENCE_VOICES 0x800E07F0u
#define AE_MISSB_G_SEQ_TRACKS 0x800F5AC0u
#define AE_MISSB_G_SOUND_BANK_TABLE_PTR 0x800E1870u
#define AE_MISSB_G_TONE_VOLUME_TABLE_PTR 0x800B4DE4u
#define AE_MISSB_G_TONE_VOLUME_CURSOR 0x800B4DE0u
#define AE_MISSB_SOUND_BANK_SIZE 12u
#define AE_MISSB_SOUND_BANK_HEADER 4u
#define AE_MISSB_SOUND_HEADER_SEQUENCE_TABLE 0x20u
#define AE_MISSB_SOUND_HEADER_SEQUENCE_STATE 0x10u
#define AE_MISSB_SOUND_TONE_RECORD_SIZE 16u
#define AE_MISSB_SOUND_TONE_VOLUME 3u
#define AE_MISSB_SOUND_TONE_LEVEL 14u
#define AE_MISSB_SEQ_TRACK_SIZE 0x120u
#define AE_MISSB_SOUND_BANK_BGM 0x04u
#define AE_MISSB_SOUND_BANK_BGM_VOICE 0x06u
#define AE_MISSB_SOUND_BANK_SFX_VOLUME 0x10u
#define AE_MISSB_SOUND_BANK_RESTORE_SENTINEL 0x16u
#define AE_MISSB_SOUND_VOICE_RECORD_SIZE 0x120u
#define AE_MISSB_SOUND_SEQUENCE_VOICE_STRIDE 0x66u
#define AE_MISSB_SEQUENCE_VOICE_ACTIVE 0x0000u
#define AE_MISSB_SEQUENCE_VOICE_MODE 0x001Au
#define AE_MISSB_SEQUENCE_VOICE_BANK 0x0022u
#define AE_MISSB_SOUND_VOICE_MODE 0x001Au
#define AE_MISSB_SOUND_VOICE_STOP 0x0008u
#define AE_MISSB_SOUND_VOICE_PITCH 0x00E8u
#define AE_MISSB_SOUND_VOICE_DIRTY 0x0100u
#define AE_MISSB_SOUND_VOICE_ACTIVE 0x0026u
#define AE_MISSB_SOUND_VOICE_BANK 0x0022u
#define AE_MISSB_SOUND_VOICE_ENV_VALUE 0x0114u
#define AE_MISSB_SOUND_VOICE_ENV_STAGE 0x0027u
#define AE_MISSB_SOUND_VOICE_ENV_DELAY 0x0101u
#define AE_MISSB_SOUND_VOICE_ENV_STATE 0x0100u
#define AE_MISSB_SOUND_VOICE_ADSR2 0x00ECu
#define AE_MISSB_SOUND_VOICE_ADSR1 0x00EAu
#define AE_MISSB_SOUND_VOICE_FLAGS 0x00E5u
#define AE_MISSB_SOUND_VOICE_STATUS 0x00E3u
#define AE_MISSB_SEQ_TRACK_BANK 0x0020u
#define AE_MISSB_SEQ_TRACK_READY 0x0024u
#define AE_MISSB_SEQ_TRACK_PLAYING 0x0025u
#define AE_MISSB_SEQ_TRACK_VOLUME 0x0036u
#define AE_MISSB_SEQ_TRACK_TIMER 0x000Cu
#define AE_MISSB_SEQ_TRACK_STATE 0x0018u
#define AE_MISSB_SEQ_TRACK_FADE_STATE 0x00E4u
#define AE_MISSB_SEQ_TRACK_FLAGS 0x0031u
#define AE_MISSB_SEQ_TRACK_PACKET 0x0010u
#define AE_MISSB_SEQ_TRACK_KEY_OFF_LOW 0x00DEu
#define AE_MISSB_SEQ_TRACK_KEY_OFF_HIGH 0x00E0u
#define AE_MISSB_CHANNEL_TRACK 0x0006u
#define AE_MISSB_CHANNEL_STATE 0x0000u
#define AE_MISSB_CHANNEL_STOP 0x0008u
#define AE_MISSB_CHANNEL_MODE 0x001Au
#define AE_MISSB_CHANNEL_LEFT_VOLUME 0x0014u
#define AE_MISSB_CHANNEL_RIGHT_VOLUME 0x0016u
#define AE_MISSB_CHANNEL_RESTORE_VOLUME 0x0038u
#define AE_MISSB_SOUND_SHADOW_LEFT 0x0008u
#define AE_MISSB_SOUND_SHADOW_RIGHT 0x000Au
#define AE_MISSB_SOUND_PACKET_VOLUME 0x0009u
#define AE_MISSB_SOUND_TONE_RECORDS 16u

static int32_t missb_sound_s16(uint32_t value)
{
    uint32_t low = value & 0xFFFFu;
    return (low & 0x8000u) != 0u ? (int32_t)low - 0x10000 : (int32_t)low;
}

static int32_t missb_sound_hw_wait_idle(AeRam *r, const AeMissbHooks *h)
{
    const AeSoundHw *hw = h != NULL ? h->sound_hw : NULL;
    if (hw == NULL || hw->wait_idle == NULL) {
        ae_ram_fail(r, "AeSoundHw.wait_idle not provided");
        return 1;
    }
    return hw->wait_idle(hw->user, r);
}

static void missb_sound_hw_wait_loop(AeRam *r, const AeMissbHooks *h,
                                     uint32_t units)
{
    const AeSoundHw *hw = h != NULL ? h->sound_hw : NULL;
    if (hw == NULL || hw->wait_loop == NULL) {
        ae_ram_fail(r, "AeSoundHw.wait_loop not provided");
        return;
    }
    hw->wait_loop(hw->user, r, units);
}

static void missb_sound_hw_key_off(AeRam *r, const AeMissbHooks *h,
                                   uint32_t half, uint16_t mask)
{
    const AeSoundHw *hw = h != NULL ? h->sound_hw : NULL;
    if (hw == NULL || hw->voice_key_off == NULL) {
        ae_ram_fail(r, "AeSoundHw.voice_key_off not provided");
        return;
    }
    hw->voice_key_off(hw->user, r, half, mask);
}

/* 800A2294 */
void ae_SeqSetMonoFlag(AeRam *r, int32_t mono)
{
    ae_wr16(r, AE_MISSB_G_MONO_FLAG, (uint32_t)mono);
}

/* 80087C64 */
int32_t ae_SoundSetStereo(AeRam *r, int32_t stereo)
{
    ae_SeqSetMonoFlag(r, stereo ^ 1);
    return 0;
}

/* 800A2640: set the selected loaded bank's software volume and update all
 * tone levels. Voice reinitialization is an out-of-batch sound service. */
int32_t ae_SoundBankSetVolume(AeRam *r, const AeMissbHooks *h,
                              int32_t bank, int32_t volume)
{
    int32_t bank_index = missb_sound_s16((uint32_t)bank);
    int32_t requested = missb_sound_s16((uint32_t)volume);
    uint32_t bank_record = AE_MISSB_G_SOUND_BANKS +
                           (uint32_t)bank_index * AE_MISSB_SOUND_BANK_SIZE;
    uint32_t header_offset = ae_rd32(r, bank_record + AE_MISSB_SOUND_BANK_HEADER);
    ae_wr32(r, AE_MISSB_G_SOUND_BANK_TABLE_PTR, header_offset);

    if (ae_rd32(r, bank_record) != 1u ||
        ae_rd32(r, header_offset + AE_MISSB_SOUND_HEADER_SEQUENCE_TABLE) == 0xFFFFFFFFu ||
        (bank_index & 0xFF80) != 0 || requested >= 128) {
        return -1;
    }

    uint32_t sequence_offset = ae_rd32(r, header_offset +
                                      AE_MISSB_SOUND_HEADER_SEQUENCE_TABLE);
    uint32_t tone_table = header_offset + sequence_offset;
    uint8_t old_volume = (uint8_t)ae_rd8(r, tone_table);
    ae_wr32(r, AE_MISSB_G_TONE_VOLUME_TABLE_PTR, tone_table);
    ae_wr8(r, tone_table, (uint32_t)requested);

    uint32_t tone_cursor = header_offset + sequence_offset + AE_MISSB_SOUND_TONE_RECORDS;
    ae_wr32(r, AE_MISSB_G_TONE_VOLUME_CURSOR, tone_cursor);
    for (uint32_t i = 0; i < 24u; ++i) {
        int32_t scaled = (int32_t)((uint32_t)ae_rd8(
            r, tone_cursor + AE_MISSB_SOUND_TONE_VOLUME) *
            (uint32_t)requested);
        ae_wr8(r, tone_cursor + AE_MISSB_SOUND_TONE_LEVEL,
               (uint32_t)(scaled >> 7));
        tone_cursor += AE_MISSB_SOUND_TONE_RECORD_SIZE;
        ae_wr32(r, AE_MISSB_G_TONE_VOLUME_CURSOR, tone_cursor);
    }

    for (uint32_t voice_index = 0; voice_index < 24u; ++voice_index) {
        uint32_t voice = AE_MISSB_G_SOUND_SEQUENCE_VOICES +
                         voice_index * AE_MISSB_SOUND_SEQUENCE_VOICE_STRIDE;
        if (ae_rd16(r, voice + AE_MISSB_SEQUENCE_VOICE_ACTIVE) == 1u &&
            ae_rd16(r, voice + AE_MISSB_SEQUENCE_VOICE_MODE) == 1u &&
            ae_rds16(r, voice + AE_MISSB_SEQUENCE_VOICE_BANK) == bank_index) {
            ae_SeqUpdateVoiceVolume(r, h != NULL ? h->sound_hw : NULL,
                                    voice_index);
        }
    }
    return old_volume;
}

/* 800A2F54: the sequencer accepts signed halfword frame and volume values. */
int32_t ae_SeqFadeTrackOut(AeRam *r, const AeMissbHooks *h,
                           int32_t track, int32_t frames, int32_t volume)
{
    (void)h;
    int32_t track_index = missb_sound_s16((uint32_t)track);
    int32_t fade_frames = missb_sound_s16((uint32_t)frames);
    int32_t target_volume = missb_sound_s16((uint32_t)volume);
    uint32_t record = AE_MISSB_G_SEQ_TRACKS +
                      (uint32_t)track_index * AE_MISSB_SEQ_TRACK_SIZE;
    if (fade_frames >= 256 || target_volume >= 128 ||
        ae_rd8(r, record + AE_MISSB_SEQ_TRACK_PLAYING) == 0u) {
        return -1;
    }
    if (target_volume == 0) ae_wr8(r, record + AE_MISSB_SEQ_TRACK_VOLUME, 1u);
    int32_t result = ae_SeqFadeInit(r, track_index, fade_frames, -1,
                                    target_volume);
    return missb_sound_s16((uint32_t)result);
}

/* 800A2B6C: update a sequencer track's play state and stop its channels when
 * the request switches it to the stopped state. The per-channel reset helper
 * remains an out-of-batch sound service. */
void ae_SeqTrackSetPlayState(AeRam *r, const AeMissbHooks *h,
                             int32_t track, int32_t state)
{
    int32_t track_index = missb_sound_s16((uint32_t)track);
    int32_t requested_state = missb_sound_s16((uint32_t)state);
    uint32_t record = AE_MISSB_G_SEQ_TRACKS +
                      (uint32_t)track_index * AE_MISSB_SEQ_TRACK_SIZE;
    uint32_t bank_index = ae_rd8(r, record + AE_MISSB_SEQ_TRACK_BANK);
    uint32_t bank = AE_MISSB_G_SOUND_BANKS + bank_index * AE_MISSB_SOUND_BANK_SIZE;
    uint32_t header = ae_rd32(r, bank + AE_MISSB_SOUND_BANK_HEADER);
    ae_wr32(r, AE_MISSB_G_SOUND_BANK_TABLE_PTR, header);

    if (ae_rd8(r, record + AE_MISSB_SEQ_TRACK_READY) != 1u ||
        ae_rd32(r, header + AE_MISSB_SOUND_HEADER_SEQUENCE_STATE) == 0xFFFFFFFFu ||
        ae_rd32(r, bank) != 1u) {
        return;
    }

    int32_t reset_channels = 0;
    int32_t should_stop = 0;
    uint8_t bank_loaded = (uint8_t)ae_rd32(r, bank);
    if (requested_state == (int32_t)bank_loaded || requested_state == 0) {
        ae_wr32(r, record + AE_MISSB_SEQ_TRACK_TIMER, 272u);
        ae_wr8(r, record + AE_MISSB_SEQ_TRACK_PLAYING, 0u);
        ae_wr32(r, record + AE_MISSB_SEQ_TRACK_STATE, bank_loaded);
        ae_wr8(r, record + AE_MISSB_SEQ_TRACK_FADE_STATE, 0u);
        ae_wr8(r, record + AE_MISSB_SEQ_TRACK_FLAGS, 0u);
        reset_channels = requested_state == (int32_t)bank_loaded;
        should_stop = 1;
    } else if (requested_state == 2) {
        if (ae_rd8(r, record + AE_MISSB_SEQ_TRACK_PLAYING) != 0u) {
            ae_wr8(r, record + AE_MISSB_SEQ_TRACK_PLAYING, 0u);
            ae_wr8(r, record + AE_MISSB_SEQ_TRACK_FADE_STATE, bank_loaded);
            should_stop = 1;
        } else if (ae_rd32(r, record + AE_MISSB_SEQ_TRACK_STATE) == bank_loaded) {
            should_stop = 1;
        } else {
            ae_wr8(r, record + AE_MISSB_SEQ_TRACK_PLAYING, bank_loaded);
            return;
        }
    } else if (requested_state == 3) {
        if (ae_rd8(r, record + AE_MISSB_SEQ_TRACK_PLAYING) != 0u) {
            ae_wr8(r, record + AE_MISSB_SEQ_TRACK_PLAYING, 0u);
            ae_wr8(r, record + AE_MISSB_SEQ_TRACK_FADE_STATE, bank_loaded);
            return;
        }
        if (ae_rd32(r, record + AE_MISSB_SEQ_TRACK_STATE) == bank_loaded) return;
        ae_wr8(r, record + AE_MISSB_SEQ_TRACK_PLAYING, bank_loaded);
        ae_wr8(r, record + AE_MISSB_SEQ_TRACK_FADE_STATE, 0u);
        return;
    } else {
        return;
    }

    if (should_stop == 0) return;
    for (uint32_t channel_index = 0; channel_index < 24u; ++channel_index) {
        uint32_t channel = AE_MISSB_G_SOUND_CHANNELS + channel_index * 0x66u;
        if (ae_rd16(r, channel + AE_MISSB_CHANNEL_TRACK) != (uint32_t)track_index ||
            ae_rd16(r, channel + AE_MISSB_CHANNEL_MODE) != 0u) {
            continue;
        }
        ae_wr16(r, channel, 0u);
        ae_wr16(r, channel + AE_MISSB_CHANNEL_STOP, 1u);
        ae_wr16(r, channel + AE_MISSB_CHANNEL_RESTORE_VOLUME, 64u);
        ae_wr16(r, channel + AE_MISSB_CHANNEL_LEFT_VOLUME, 0u);
        ae_wr16(r, channel + AE_MISSB_CHANNEL_RIGHT_VOLUME, 0u);
        (void)ae_missb_call(r, h, "SeqTrackChannelReset", 0x8009E4D8u,
                            2u, (uint32_t)track_index, channel_index, 0u, 0u);
        if (reset_channels != 0) {
            uint32_t shadow = ae_rd32(r, AE_MISSB_G_SOUND_VOICE_SHADOW) +
                              channel_index * 16u;
            ae_wr16(r, shadow + AE_MISSB_SOUND_SHADOW_LEFT, 0u);
            ae_wr16(r, shadow + AE_MISSB_SOUND_SHADOW_RIGHT, 0u);
        }
    }

    uint32_t packet_cursor = ae_rd32(r, record + AE_MISSB_SEQ_TRACK_PACKET) +
                             AE_MISSB_SOUND_TONE_RECORDS;
    for (uint32_t i = 0; i < 16u; ++i) {
        ae_wr32(r, AE_MISSB_G_TONE_VOLUME_CURSOR, packet_cursor);
        ae_wr8(r, ae_rd32(r, AE_MISSB_G_TONE_VOLUME_CURSOR) +
                  AE_MISSB_SOUND_PACKET_VOLUME, 0u);
        packet_cursor = ae_rd32(r, AE_MISSB_G_TONE_VOLUME_CURSOR) + 16u;
        ae_wr32(r, AE_MISSB_G_TONE_VOLUME_CURSOR, packet_cursor);
    }
    missb_sound_hw_key_off(r, h, 0u,
                           (uint16_t)ae_rd16(r, record + AE_MISSB_SEQ_TRACK_KEY_OFF_LOW));
    missb_sound_hw_wait_loop(r, h, 2u);
    missb_sound_hw_key_off(r, h, 1u,
                           (uint16_t)ae_rd16(r, record + AE_MISSB_SEQ_TRACK_KEY_OFF_HIGH));
    ae_wr16(r, record + AE_MISSB_SEQ_TRACK_KEY_OFF_HIGH, 0u);
    ae_wr16(r, record + AE_MISSB_SEQ_TRACK_KEY_OFF_LOW, 0u);
}

/* 800A35FC: the pitch value is stored in the voice record after SPU idle. */
void ae_SfxVoiceSetPitch(AeRam *r, const AeMissbHooks *h,
                         int32_t voice, int32_t pitch)
{
    uint32_t voice_index = (uint32_t)missb_sound_s16((uint32_t)voice);
    uint32_t voice_record = AE_SOUND_VOICES + voice_index * AE_MISSB_SOUND_VOICE_RECORD_SIZE;

    if (missb_sound_hw_wait_idle(r, h) != 0) return;
    ae_wr16(r, voice_record + AE_MISSB_SOUND_VOICE_PITCH,
            (uint32_t)pitch & 0x3FFFu);
    ae_wr8(r, voice_record + AE_MISSB_SOUND_VOICE_DIRTY, 1u);
}

/* 80087BA4 */
int32_t ae_SoundEmitterVolume(AeRam *r, const AeMissbHooks *h,
                              uint32_t emitter, int32_t voice,
                              int32_t volume, int32_t divisor)
{
    (void)emitter;
    int32_t voice_id = missb_sound_s16((uint32_t)voice);
    if (voice_id < 0) return -1;

    int32_t scaled_volume = missb_sound_s16((uint32_t)volume) * 4096;
    int32_t signed_divisor = missb_sound_s16((uint32_t)divisor);
    if (signed_divisor == 0) {
        ae_ram_fail(r, "SoundEmitterVolume: signed division by zero");
        return 0;
    }
    int32_t quotient = scaled_volume / signed_divisor;
    int32_t lane_volume = missb_sound_s16((uint32_t)quotient);
    ae_SfxVoiceSetPanVol(r, h != NULL ? h->misc : NULL, voice_id,
                         lane_volume, lane_volume);
    return 0;
}

/* 8008760C */
int32_t ae_BgmFadeOut(AeRam *r, const AeMissbHooks *h)
{
    int32_t bgm_voice = ae_rds16(r, AE_MISSB_G_SOUND_BANK_STATE +
                                 AE_MISSB_SOUND_BANK_BGM_VOICE);
    if (bgm_voice >= 0) {
        (void)ae_SeqFadeTrackOut(r, h, bgm_voice, 90, 0);
        ae_wr16(r, AE_MISSB_G_SOUND_BANK_STATE +
                AE_MISSB_SOUND_BANK_RESTORE_SENTINEL, 0xFFFFu);
    }
    return 0;
}

/* 800A342C: state changes remain in RAM; writes to SPU registers and their
 * settle waits cross the AeSoundHw boundary. */
void ae_SoundStopAllVoices(AeRam *r, const AeMissbHooks *h, int32_t mode)
{
    int32_t stop_mode = missb_sound_s16((uint32_t)mode);
    if (missb_sound_hw_wait_idle(r, h) != 0) return;

    uint32_t voice_shadow_table = ae_rd32(r, AE_MISSB_G_SOUND_VOICE_SHADOW);
    for (uint32_t voice_index = 0u; voice_index < 24u; ++voice_index) {
        uint32_t channel = AE_MISSB_G_SOUND_CHANNELS + voice_index * 0x66u;
        if (ae_rd16(r, channel + AE_MISSB_CHANNEL_MODE) != 0u) {
            ae_wr16(r, channel + AE_MISSB_CHANNEL_STOP, 1u);
            if (stop_mode != 0) {
                uint32_t shadow = voice_shadow_table + voice_index * 16u;
                ae_wr16(r, shadow + AE_MISSB_SOUND_SHADOW_LEFT, 0u);
                ae_wr16(r, shadow + AE_MISSB_SOUND_SHADOW_RIGHT, 0u);
                ae_wr16(r, channel + AE_MISSB_CHANNEL_STATE, 0u);
            }

            uint32_t bit_index = voice_index < 16u ? voice_index : voice_index - 16u;
            uint32_t mask = 1u << bit_index;
            missb_sound_hw_key_off(r, h, voice_index < 16u ? 0u : 1u,
                                   (uint16_t)mask);
            missb_sound_hw_wait_loop(r, h, 2u);
        }
    }

    for (uint32_t voice_index = 0u; voice_index < 24u; ++voice_index) {
        uint32_t voice = AE_MISSB_G_SEQ_TRACKS +
                         voice_index * AE_MISSB_SOUND_VOICE_RECORD_SIZE;
        if (ae_rd8(r, voice + AE_MISSB_SOUND_VOICE_ACTIVE) != 0u) {
            ae_wr32(r, voice + AE_MISSB_SOUND_VOICE_ENV_VALUE, 0u);
            ae_wr8(r, voice + AE_MISSB_SOUND_VOICE_ACTIVE, 0u);
            ae_wr8(r, voice + AE_MISSB_SOUND_VOICE_ENV_STAGE, 0u);
            ae_wr8(r, voice + AE_MISSB_SOUND_VOICE_ENV_DELAY, 0u);
            ae_wr8(r, voice + AE_MISSB_SOUND_VOICE_ENV_STATE, 0u);
            ae_wr16(r, voice + AE_MISSB_SOUND_VOICE_ADSR2, 0u);
            ae_wr16(r, voice + AE_MISSB_SOUND_VOICE_ADSR1, 0u);
            ae_wr8(r, voice + AE_MISSB_SOUND_VOICE_FLAGS, 0u);
            ae_wr8(r, voice + AE_MISSB_SOUND_VOICE_STATUS, 0u);
        }
    }
}

/* 80086170 */
int32_t ae_SoundStopAndRestoreVolume(AeRam *r, const AeMissbHooks *h)
{
    ae_SoundStopAllVoices(r, h, 1);
    (void)ae_missb_call(r, h, "VSync", 0x8002E520u, 1u, 0u, 0u, 0u, 0u);

    uint32_t mode = ae_rd16(r, AE_MISSB_G_GAME_MODE);
    int32_t volume;
    if ((mode >= 10u && mode <= 13u) ||
        (mode >= 17u && mode <= 20u) ||
        (mode >= 23u && mode <= 25u) || mode == 9u) {
        volume = (int32_t)ae_rd16(r, AE_MISSB_G_SOUND_BANK_STATE +
                                  AE_MISSB_SOUND_BANK_SFX_VOLUME);
    } else {
        volume = 127;
    }

    int32_t sfx_bank = ae_rds16(r, AE_MISSB_G_SOUND_BANK_STATE);
    if (sfx_bank >= 0) (void)ae_SoundBankSetVolume(r, h, sfx_bank, volume);
    int32_t bgm_bank = ae_rds16(r, AE_MISSB_G_SOUND_BANK_STATE + AE_MISSB_SOUND_BANK_BGM);
    if (bgm_bank >= 0) (void)ae_SoundBankSetVolume(r, h, bgm_bank, volume);
    return 0;
}

/* 80086148 */
int32_t ae_SoundSilence(AeRam *r, const AeMissbHooks *h)
{
    (void)ae_SoundLoopsReset(r);
    (void)ae_SoundStopAndRestoreVolume(r, h);
    return 0;
}
