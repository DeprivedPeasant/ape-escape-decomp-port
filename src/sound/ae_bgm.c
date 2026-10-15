#include "ae_bgm.h"

static int16_t bgm_s16(uint32_t value) {
    return (int16_t)(uint16_t)value;
}

static int32_t bgm_asr32(uint32_t value, unsigned int shift) {
    int32_t signed_value = (int32_t)value;
    if (signed_value >= 0) return signed_value >> shift;
    return (int32_t)~((~value) >> shift);
}

static uint32_t bgm_track_address(int32_t track) {
    return AE_BGM_TRACKS + (uint32_t)track * AE_BGM_TRACK_SIZE;
}

static uint32_t bgm_voice_address(uint32_t channel) {
    return AE_BGM_SEQUENCER_VOICES + channel * AE_BGM_SEQUENCER_VOICE_SIZE;
}

static int32_t bgm_print(AeRam *r, const AeSoundHw *hw, uint32_t format,
                         uint32_t argc, int32_t a, int32_t b, int32_t c) {
    if (!hw || !hw->printf_error) {
        ae_ram_fail(r, "AeSoundHw.printf_error not provided");
        return 0;
    }
    hw->printf_error(hw->user, r, format, argc, a, b, c);
    return !ae_ram_failed(r);
}

static int32_t bgm_write_voice_volume(AeRam *r, const AeSoundHw *hw, uint32_t channel,
                                      uint32_t side, uint16_t volume) {
    if (!hw || !hw->set_voice_volume) {
        ae_ram_fail(r, "AeSoundHw.set_voice_volume not provided");
        return 0;
    }
    hw->set_voice_volume(hw->user, r, channel, side, volume);
    return !ae_ram_failed(r);
}

/* SoundDelayedQueueReset @80088578. The volume halfwords are intentionally retained. */
int32_t ae_SoundDelayedQueueReset(AeRam *r) {
    for (uint32_t i = 0; i < AE_BGM_QUEUE_COUNT; ++i) {
        uint32_t entry = AE_SOUND_QUEUE + AE_SQ_SIZE * i;
        ae_wr16(r, entry + AE_SQ_TYPE, 0xFFFFu);
        ae_wr16(r, entry + AE_SQ_ID, 0);
    }
    return 0;
}

/* SoundAreaKeepsMusic @80086F00. These are the exact accepted area transitions. */
int32_t ae_SoundAreaKeepsMusic(AeRam *r, int32_t from_area, int32_t to_area) {
    (void)r;
    if (from_area == 75 || from_area == 34 || from_area == 4) return 1;
    if (from_area == 72 && to_area == 76) return 1;
    if (from_area == 3 && to_area == 4) return 1;
    if (from_area == 33 && to_area == 34) return 1;
    if (from_area == 32 && to_area == 30) return 1;
    if (from_area == 30 && to_area == 32) return 1;
    if (from_area == 8 && to_area == 6) return 1;
    if (from_area == 6 && to_area == 8) return 1;
    if (from_area == 9 && to_area == 10) return 1;
    if (from_area == 10 && to_area == 9) return 1;
    return 0;
}

/* SoundLoopStart @800878E0. */
int32_t ae_SoundLoopStart(AeRam *r, const AeSoundHw *hw, int32_t mode, int32_t id) {
    int16_t loop_id = bgm_s16((uint32_t)id);
    uint32_t row = AE_SOUND_EFFECTS + AE_SE_SIZE * ((uint32_t)loop_id & AE_BGM_LOOP_ID_MASK);
    int32_t bank_slot = ae_rds16(r, row + AE_SE_BANK);
    uint32_t loaded_bank = AE_SOUND_BANK_IDS + 2u * (uint32_t)bank_slot;
    int32_t loaded_id = ae_rds16(r, loaded_bank);
    if (ae_ram_failed(r) || loaded_id < 0) return -1;

    uint32_t program = (uint32_t)ae_rds16(r, row + AE_SE_PROGRAM);
    uint32_t note = (uint32_t)ae_rds16(r, row + AE_SE_NOTE);
    int32_t voice;
    if (mode == 0) {
        voice = ae_SfxVoiceStart(r, hw, (uint32_t)loaded_id, program, note);
    } else {
        voice = ae_SfxVoiceStartEx(r, hw, (uint32_t)loaded_id, program, note, 1, 1, 4096);
    }
    if (ae_ram_failed(r)) return -1;
    if (voice < 0) {
        if (!bgm_print(r, hw, AE_BGM_LOOP_START_ERROR, 3,
                       (int32_t)((uint32_t)loop_id & AE_BGM_LOOP_ID_MASK),
                       ae_rds16(r, row + AE_SE_PROGRAM), ae_rds16(r, row + AE_SE_NOTE))) return -1;
        return -1;
    }
    if (((uint32_t)loop_id & AE_BGM_LOOP_TYPE_MASK) != 0) return bgm_s16((uint32_t)voice);

    uint32_t loop = ae_SoundLoopAllocSlot(r);
    if (ae_ram_failed(r)) return -1;
    if (!loop) {
        bgm_print(r, hw, AE_BGM_LOOP_SLOT_ERROR, 0, 0, 0, 0);
        return -1;
    }
    ae_wr16(r, loop + AE_SL_ID, (uint16_t)loop_id);
    ae_wr16(r, loop + AE_SL_VOICE, (uint16_t)voice);
    ae_wr16(r, loop + AE_SL_ACTIVE, 1);
    ae_wr16(r, loop + AE_SL_FLAGS, (uint16_t)mode);
    return bgm_s16((uint32_t)voice);
}

/* SoundLoopsStopAll @80086540. Restarts the current area's persistent loop list after clearing transient loops. */
int32_t ae_SoundLoopsStopAll(AeRam *r, const AeSoundHw *hw) {
    for (uint32_t i = 0; i < AE_BGM_LOOP_COUNT; ++i) {
        uint32_t loop = AE_SOUND_LOOPS + AE_SL_SIZE * i;
        uint16_t raw_id = (uint16_t)ae_rd16(r, loop + AE_SL_ID);
        int16_t id = bgm_s16(raw_id);
        if (id < 0 || (bgm_s16((uint32_t)id & AE_BGM_LOOP_TYPE_MASK) == AE_BGM_LOOP_TYPE_KEEP)) continue;

        int16_t voice = ae_rds16(r, loop + AE_SL_VOICE);
        if (voice >= 0) {
            ae_SfxVoiceStop(r, hw, voice);
            if (ae_ram_failed(r)) return 0;
        }
        if ((raw_id & AE_BGM_LOOP_TYPE_MASK) == AE_BGM_LOOP_TYPE_DIRECT_RESET) {
            ae_wr16(r, loop + AE_SL_ID, 0xFFFFu);
            ae_wr16(r, loop + AE_SL_VOICE, 0xFFFFu);
            ae_wr16(r, loop + AE_SL_ACTIVE, 0);
            ae_wr16(r, loop + AE_SL_INDEX, 0xFFFFu);
            ae_wr16(r, loop + AE_SL_FLAGS, 0);
        } else {
            ae_SoundLoopFree(r, id, voice);
        }
        if (ae_ram_failed(r)) return 0;
    }

    uint32_t area_index = ae_rd16(r, AE_BGM_AREA_INDEX);
    uint32_t definitions = ae_rd32(r, AE_BGM_LOOP_DATA_POINTERS + AE_BGM_LOOP_POINTER_STRIDE * area_index);
    if (ae_ram_failed(r) || definitions == 0) return 0;

    for (uint32_t i = 0;; ++i) {
        uint32_t record = definitions + AE_BGM_LOOP_DEFINITION_STRIDE * i;
        int16_t configured_id = ae_rds16(r, record);
        if (ae_ram_failed(r) || configured_id < 0) break;

        uint16_t combined_id = (uint16_t)(ae_rd16(r, record)
                                        | ae_rd16(r, record + AE_BGM_LOOP_DEFINITION_FLAGS));
        ae_wr16(r, record, combined_id);
        uint32_t loop = ae_SoundLoopAllocSlot(r);
        if (ae_ram_failed(r)) return 0;
        if (!loop) {
            bgm_print(r, hw, AE_BGM_LOOP_TABLE_ERROR, 1, bgm_s16(combined_id), 0, 0);
            if (ae_ram_failed(r)) return 0;
            continue;
        }

        int32_t voice = ae_SoundLoopStart(r, hw, 1, bgm_s16(combined_id));
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, loop + AE_SL_VOICE, (uint16_t)voice);
        ae_wr16(r, loop + AE_SL_ID, combined_id);
        for (uint32_t offset = AE_BGM_LOOP_DEFINITION_FIRST;
             offset <= AE_BGM_LOOP_DEFINITION_LAST; offset += 2) {
            ae_wr16(r, loop + offset, ae_rd16(r, record + offset));
        }
        ae_wr16(r, loop + AE_SL_EXTRA,
                ae_rd16(r, record + AE_BGM_LOOP_DEFINITION_EXTRA));
        ae_wr32(r, loop + AE_BGM_LOOP_TRAILER_WORD, 0);
        ae_wr16(r, loop + AE_SL_ACTIVE, voice < 0 ? 0u : 1u);
        if (ae_ram_failed(r)) return 0;
    }

    ae_SoundDelayedQueueReset(r);
    return 0;
}

/* ProgressGetByte @800963AC. The original masks offsets to the 0x40000-byte table. */
uint32_t ae_ProgressGetByte(AeRam *r, uint32_t offset) {
    uint32_t word = AE_BGM_PROGRESS_BYTES + (offset & AE_BGM_PROGRESS_MASK);
    uint32_t shift = (offset & 3u) * 8u;
    return (ae_rd32(r, word) >> shift) & 0xFFu;
}

/* BgmModeWantsMusic @8008740C. */
int32_t ae_BgmModeWantsMusic(AeRam *r) {
    uint32_t mode = ae_rd16(r, AE_BGM_GAME_MODE);
    return mode == 0x0Bu || mode == 0x0Du || mode == 0x11u || mode == 0x13u || mode == 0x18u;
}

/* BgmSeqOpen @800A1DB0. */
int32_t ae_BgmSeqOpen(AeRam *r, int32_t bank, uint32_t sequence) {
    int16_t bank_id = bgm_s16((uint32_t)bank);
    if ((sequence & 3u) != 0 || (((uint16_t)bank_id & 0xFF80u) != 0)) return -1;
    if (ae_rd32(r, sequence + AE_BGM_SSQ_MAGIC_OFFSET) != AE_BGM_SSQ_MAGIC || ae_ram_failed(r)) return -1;

    uint32_t bank_record = AE_SOUND_BANKS + AE_SB_SIZE * (uint32_t)bank_id;
    if (ae_rd32(r, bank_record + AE_SB_LOADED) == 0 || ae_ram_failed(r)) return -1;

    uint32_t track = AE_BGM_TRACKS;
    uint32_t slot = 0;
    for (; slot < AE_BGM_TRACK_COUNT; ++slot, track += AE_BGM_TRACK_SIZE) {
        if ((ae_rd32(r, track + AE_BT_ACTIVE) & AE_BGM_TRACK_ALLOC_MASK) == 0) break;
    }
    if (slot == AE_BGM_TRACK_COUNT || ae_ram_failed(r)) return -1;

    ae_wr8(r, track + AE_BT_EVENT_SUBSTATUS, 0);
    ae_wr8(r, track + AE_BT_EFFECT_MODE, 0);
    ae_wr32(r, AE_BGM_SEQUENCE_DATA_BASE, sequence);
    ae_wr8(r, track + AE_BT_ACTIVE, 1);
    ae_wr32(r, track + AE_BT_CHANNEL_RECORDS, sequence);
    ae_wr32(r, track + AE_BT_READ_POSITION, AE_BGM_SSQ_EVENT_OFFSET);
    ae_wr32(r, track + AE_BT_DELTA, 0);
    uint32_t initial_status = ae_rd8(r, sequence + AE_BGM_SSQ_EVENT_OFFSET);
    ae_wr8(r, track + AE_BT_EVENT_STATUS, initial_status);
    ae_wr8(r, track + AE_BT_EVENT_DATA + 1u,
           ae_rd8(r, sequence + AE_BGM_SSQ_EVENT_OFFSET + 1u));
    ae_wr8(r, track + AE_BT_EVENT_DATA + 2u,
           ae_rd8(r, sequence + AE_BGM_SSQ_EVENT_OFFSET + 2u));
    ae_wr8(r, track + AE_BT_EVENT_DATA, initial_status);
    ae_wr8(r, track + AE_BT_BANK, (uint32_t)bank_id);

    for (uint32_t channel = 0; channel < AE_BGM_SSQ_CHANNEL_COUNT; ++channel) {
        uint32_t record = sequence + AE_BGM_SSQ_CHANNELS_OFFSET + channel * AE_BGM_SSQ_CHANNEL_SIZE;
        uint32_t master_volume = ae_rd8(r, sequence + AE_BGM_SSQ_MASTER_VOLUME);
        int32_t scaled = bgm_asr32(master_volume * ae_rd8(r, record + AE_BGM_SSQ_CHANNEL_VOLUME), 8);
        ae_wr8(r, record + AE_BGM_SSQ_CHANNEL_SCALED, (uint32_t)scaled);
    }
    ae_wr16(r, track + AE_BT_TEMPO, ae_rd16(r, sequence + AE_BGM_SSQ_TEMPO_OFFSET));
    ae_wr16(r, track + AE_BT_PPQ, ae_rd16(r, sequence + AE_BGM_SSQ_TPQ_OFFSET));
    return (int32_t)slot;
}

/* SeqVoiceVolumeSide @800A0A64. Each multiply retains the MIPS low word before arithmetic shifting. */
int32_t ae_SeqVoiceVolumeSide(AeRam *r, uint32_t channel, uint32_t side) {
    uint32_t voice = bgm_voice_address(channel & 0xFFFFu);
    uint32_t factor = ae_rd16(r, voice + AE_BV_VOLUME_A);
    factor *= ae_rd16(r, voice + AE_BV_VOLUME_B);
    factor *= ae_rd16(r, voice + AE_BV_VOLUME_C);
    factor *= ae_rd16(r, voice + AE_BV_VOLUME_D);
    int32_t level = bgm_asr32(factor, 14);
    uint32_t pan_address = voice + ((side & 0xFFFFu) + AE_BV_PAN_LEFT / 2u) * 2u;
    uint32_t pan_product = (uint32_t)level * ae_rd16(r, pan_address);
    int32_t result = bgm_asr32(pan_product, 7);
    uint32_t phase = ae_rd16(r, voice + AE_BV_PHASE);
    if (phase != 0) {
        int32_t signed_level = bgm_s16((uint32_t)result);
        result = bgm_asr32((uint32_t)signed_level, 7) | (int32_t)(phase << 8);
    }
    return bgm_s16((uint32_t)result);
}

/* SeqUpdateVoiceVolume @800A0900. The SPU registers remain a host boundary. */
void ae_SeqUpdateVoiceVolume(AeRam *r, const AeSoundHw *hw, uint32_t channel) {
    channel &= 0xFFFFu;
    uint32_t voice = bgm_voice_address(channel);
    uint32_t track_index = ae_rd16(r, voice + AE_BV_TRACK);
    uint32_t track = AE_BGM_TRACKS + track_index * AE_BGM_TRACK_SIZE;
    if (ae_rd16(r, voice + AE_BV_IS_EFFECT) != 0) {
        uint32_t bank = ae_rd8(r, track + AE_BT_BANK);
        uint32_t bank_record = AE_SOUND_BANKS + AE_SB_SIZE * bank;
        uint32_t header = ae_rd32(r, bank_record + AE_SB_HEADER);
        ae_wr32(r, AE_SOUND_BANK_HEADER, header);
        uint32_t program = ae_rd16(r, voice + AE_BV_PROGRAM);
        uint32_t tone = header + ae_rd32(r, header + AE_BGM_HEADER_TONE_OFFSET)
                      + (program << 4) + AE_BGM_TONE_HEADER_SIZE;
        ae_wr32(r, AE_BGM_CURRENT_CHANNEL_RECORD, tone);
    } else {
        uint32_t records = ae_rd32(r, track + AE_BT_CHANNEL_RECORDS);
        uint32_t program = ae_rd16(r, voice + AE_BV_PROGRAM);
        ae_wr32(r, AE_BGM_CURRENT_CHANNEL_RECORD,
                records + (program << 4) + AE_BGM_SSQ_CHANNEL_RECORD_START);
    }
    if (ae_ram_failed(r)) return;
    ae_wr16(r, voice + AE_BV_VOLUME_A,
            ae_rd8(r, ae_rd32(r, AE_BGM_CURRENT_CHANNEL_RECORD)
                   + AE_BGM_SSQ_CHANNEL_SCALED));

    int32_t left = ae_SeqVoiceVolumeSide(r, channel, 0);
    if (ae_ram_failed(r) || !bgm_write_voice_volume(r, hw, channel, 0, (uint16_t)left)) return;
    int32_t right = ae_SeqVoiceVolumeSide(r, channel, 1);
    if (ae_ram_failed(r)) return;
    bgm_write_voice_volume(r, hw, channel, 1, (uint16_t)right);
}

/* BgmSeqSetVolume @800A2428. */
int32_t ae_BgmSeqSetVolume(AeRam *r, const AeSoundHw *hw, int32_t track_index, int32_t volume) {
    int16_t track_id = bgm_s16((uint32_t)track_index);
    int16_t target_volume = bgm_s16((uint32_t)volume);
    if (track_id >= (int16_t)AE_BGM_TRACK_COUNT || target_volume >= 0x80) return -1;

    uint32_t track = bgm_track_address(track_id);
    if (ae_rd8(r, track + AE_BT_ACTIVE) == 0 || ae_ram_failed(r)) return -1;
    uint32_t sequence = ae_rd32(r, track + AE_BT_CHANNEL_RECORDS);
    uint32_t old_volume = ae_rd8(r, sequence);
    ae_wr8(r, sequence, (uint32_t)target_volume);
    ae_wr32(r, AE_BGM_CURRENT_TRACK_VOLUME, sequence);
    ae_wr32(r, AE_BGM_CURRENT_CHANNEL_RECORD, sequence + AE_BGM_SSQ_CHANNEL_RECORD_START);
    for (uint32_t i = 0; i < AE_BGM_SSQ_CHANNEL_COUNT; ++i) {
        uint32_t record = ae_rd32(r, AE_BGM_CURRENT_CHANNEL_RECORD);
        int32_t scaled = bgm_asr32(ae_rd8(r, record + AE_BGM_SSQ_CHANNEL_VOLUME)
                                   * (uint32_t)target_volume, 7);
        ae_wr8(r, record + AE_BGM_SSQ_CHANNEL_SCALED, (uint32_t)scaled);
        ae_wr32(r, AE_BGM_CURRENT_CHANNEL_RECORD, record + AE_BGM_SSQ_CHANNEL_SIZE);
        if (ae_ram_failed(r)) return 0;
    }

    uint32_t track_bank = ae_rd8(r, track + AE_BT_BANK);
    for (uint32_t i = 0; i < AE_BGM_SEQUENCER_VOICE_COUNT; ++i) {
        uint32_t voice = bgm_voice_address(i);
        if (ae_rd16(r, voice + AE_BV_ACTIVE) != 1) continue;
        if (ae_rd16(r, voice + AE_BV_BANK) != track_bank) continue;
        if (ae_rd16(r, voice + AE_BV_IS_EFFECT) != 0) continue;
        if (ae_rd16(r, voice + AE_BV_TRACK) != (uint16_t)track_id) continue;
        ae_SeqUpdateVoiceVolume(r, hw, i);
        if (ae_ram_failed(r)) return 0;
    }
    return (int32_t)old_volume;
}

/* BgmStart @80087388. */
int32_t ae_BgmStart(AeRam *r, const AeSoundHw *hw) {
    uint32_t sequence = ae_rd32(r, AE_BGM_TRACK_BGM_SEQUENCE);
    if (sequence == 0) return -1;
    int16_t bank = ae_rds16(r, AE_BGM_TRACK_BGM_BANK);
    if (bank < 0) return -1;

    int32_t voice = ae_BgmSeqOpen(r, bank, sequence);
    ae_wr16(r, AE_BGM_VOICE, (uint16_t)voice);
    if (bgm_s16((uint32_t)voice) < 0 || ae_ram_failed(r)) return 0;
    ae_wr16(r, AE_BGM_START_FADE_STATE, 0);
    ae_BgmSeqSetVolume(r, hw, voice, 0);
    return 0;
}

/* SeqFadeInit @800A0DB0. */
int32_t ae_SeqFadeInit(AeRam *r, int32_t track_index, int32_t frames, int32_t program, int32_t target_volume) {
    int16_t track_id = bgm_s16((uint32_t)track_index);
    uint16_t program_id = (uint16_t)program;
    uint32_t track = bgm_track_address(track_id);
    uint32_t sequence = ae_rd32(r, track + AE_BT_CHANNEL_RECORDS);
    if (program_id == 0xFFFFu) {
        ae_wr32(r, AE_BGM_CURRENT_TRACK_VOLUME, sequence);
        ae_wr8(r, track + AE_BT_FADE_TRACK_FLAG, 1);
        ae_wr8(r, track + AE_BT_FADE_TRACK_TARGET, (uint32_t)target_volume);
        ae_wr8(r, track + AE_BT_FADE_TRACK_START, (uint32_t)frames);
        ae_wr8(r, track + AE_BT_FADE_TRACK_CURRENT, (uint32_t)frames);
        uint32_t current_volume = ae_rd8(r, sequence);
        ae_wr8(r, track + AE_BT_FADE_TRACK_VOLUME, current_volume);
        ae_wr8(r, track + AE_BT_FADE_TRACK_ACTIVE, 1);
        return (int32_t)current_volume;
    }

    uint32_t channel_record = sequence + ((uint32_t)program_id << 4) + AE_BGM_SSQ_CHANNELS_OFFSET;
    ae_wr32(r, AE_BGM_CURRENT_CHANNEL_RECORD, channel_record);
    uint32_t fade_program = track + program_id;
    ae_wr8(r, fade_program + AE_BT_FADE_PROGRAM_ACTIVE, 1);
    ae_wr8(r, fade_program + AE_BT_FADE_PROGRAM_TARGET, (uint32_t)target_volume);
    ae_wr8(r, fade_program + AE_BT_FADE_PROGRAM_START, (uint32_t)frames);
    ae_wr8(r, fade_program + AE_BT_FADE_PROGRAM_CURRENT, (uint32_t)frames);
    uint32_t volume = ae_rd8(r, channel_record + AE_BGM_SSQ_CHANNEL_VOLUME);
    ae_wr8(r, fade_program + AE_BT_FADE_PROGRAM_VOLUME, volume);
    ae_wr8(r, track + AE_BT_FADE_TRACK_ACTIVE, 1);
    return (int32_t)volume;
}

/* SeqTrackRearm @800A2ACC. */
void ae_SeqTrackRearm(AeRam *r, int32_t track_index) {
    int16_t track_id = bgm_s16((uint32_t)track_index);
    uint32_t track = bgm_track_address(track_id);
    uint32_t bank = ae_rd8(r, track + AE_BT_BANK);
    uint32_t bank_record = AE_SOUND_BANKS + AE_SB_SIZE * bank;
    uint32_t header = ae_rd32(r, bank_record + AE_SB_HEADER);
    ae_wr32(r, AE_SOUND_BANK_HEADER, header);
    uint32_t active = ae_rd8(r, track + AE_BT_ACTIVE);
    if (active == 1 && ae_rd32(r, header + AE_BGM_HEADER_REARM_WORD) != 0xFFFFFFFFu) {
        if (ae_rd32(r, bank_record + AE_SB_LOADED) == active) {
            ae_wr8(r, track + AE_BT_PAUSED, active);
            ae_wr8(r, track + AE_BT_REARM_STATE, 0);
        }
    }
    ae_wr32(r, track + AE_BGM_TRACK_CLEAR_COUNTER, 0);
}

/* SeqFadeTrackIn @800A2E3C. */
int32_t ae_SeqFadeTrackIn(AeRam *r, const AeSoundHw *hw, int32_t track_index, int32_t frames, int32_t volume) {
    int16_t track_id = bgm_s16((uint32_t)track_index);
    int16_t frame_count = bgm_s16((uint32_t)frames);
    int16_t target_volume = bgm_s16((uint32_t)volume);
    uint32_t track = bgm_track_address(track_id);
    uint32_t sequence = ae_rd32(r, track + AE_BT_CHANNEL_RECORDS);
    ae_wr32(r, AE_BGM_CURRENT_TRACK_VOLUME, sequence);
    if (frame_count >= 256 || target_volume >= 0x80) return -1;

    uint32_t paused = ae_rd8(r, track + AE_BT_PAUSED);
    if (paused == 0) {
        ae_BgmSeqSetVolume(r, hw, track_id, 0);
        if (ae_ram_failed(r)) return -1;
        ae_SeqTrackRearm(r, track_id);
        if (ae_ram_failed(r)) return -1;
        ae_wr8(r, track + AE_BT_FADE_TRACK_CLEAR, 0);
    } else if (paused == 1) {
        if ((int32_t)ae_rd8(r, sequence) >= target_volume || ae_ram_failed(r)) return -1;
        ae_wr8(r, track + AE_BT_FADE_TRACK_CLEAR, 0);
    } else {
        return -1;
    }
    return ae_SeqFadeInit(r, track_id, frame_count, -1, target_volume);
}

/* BgmUpdateVolume @8008745C. */
int32_t ae_BgmUpdateVolume(AeRam *r, const AeSoundHw *hw) {
    int16_t voice = ae_rds16(r, AE_BGM_VOICE);
    if (voice < 0) return 0;

    if (ae_BgmModeWantsMusic(r)) {
        uint32_t target = ae_rd16(r, AE_BGM_VOLUME);
        int32_t mode_result = 0;
        if (ae_rd32(r, AE_BGM_SPECIAL_MODE_WORD) == 70u) {
            mode_result = (int32_t)ae_ProgressGetByte(r, 8u);
        }
        if (mode_result == 5) target = (uint16_t)(bgm_s16(target) / 3);
        ae_SeqFadeTrackIn(r, hw, voice, 90, (int32_t)target);
    } else {
        uint32_t mode = ae_rd16(r, AE_BGM_GAME_MODE);
        if (mode == 3u) {
            ae_SeqFadeTrackIn(r, hw, voice, 90, 110);
        } else if (mode == 8u || mode == 16u || mode == 29u || mode == 30u) {
            ae_SeqTrackRearm(r, voice);
            ae_BgmSeqSetVolume(r, hw, voice, 75);
        } else if (mode == 9u || mode == 10u || mode == 18u || mode == 23u
                   || mode == 12u || mode == 20u || mode == 25u) {
            ae_SeqTrackRearm(r, voice);
            ae_BgmSeqSetVolume(r, hw, voice, bgm_s16(ae_rd16(r, AE_BGM_VOLUME)));
        } else {
            ae_SeqFadeTrackIn(r, hw, voice, 90, 75);
        }
    }
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r, AE_BGM_UPDATED_VOICE, (uint16_t)voice);
    return 0;
}

/* SeqTrackRearm and SeqFadeTrackIn use these complete ports above; the sole new chip access is set_voice_volume. */

