/* Progress / save helpers ported from the main EXE. */
#include "ae_progress.h"

/* Existing RAM-image ports used directly by this module. */
int32_t ae_SoundLoopStop(AeRam *r, const struct AeSoundHw *sound_hw,
                         int32_t id, int32_t voice);
void ae_CutsceneModeOff(AeRam *r);
void ae_SpriteEffectQueue(AeRam *r, uint32_t spec, int32_t kind);
void ae_GadgetUnlockMask(AeRam *r, uint32_t mask);
int32_t ae_BgmSeqSetVolume(AeRam *r, const struct AeSoundHw *sound_hw,
                           int32_t track, int32_t volume);
int32_t ae_GimmickSaveStates(AeRam *r);
int32_t ae_GimmickSaveApplyProgress(AeRam *r,
                                    const struct AeGimmickAHooks *h);
int32_t ae_GetLevelFirstArea(AeRam *r, int32_t level);
int32_t ae_IsTimeAttack(AeRam *r);
int32_t ae_AreaGetKind(AeRam *r, const struct AeAreaSetHooks *h,
                       int32_t area);
int32_t ae_AreaGetLevel(AeRam *r, const struct AeAreaSetHooks *h,
                        int32_t area);

static int32_t progress_signed16(uint32_t value)
{
    uint32_t low = value & 0xFFFFu;
    return (low & 0x8000u) != 0u ? (int32_t)low - 0x10000 : (int32_t)low;
}

static void progress_call(AeRam *r, const AeProgressHooks *h,
                          AeProgressHookFn fn, const char *name,
                          uint32_t a0, uint32_t a1,
                          uint32_t a2, uint32_t a3)
{
    if (h == NULL || fn == NULL) {
        ae_ram_fail(r, "AeProgressHooks.%s not provided", name);
        return;
    }
    (void)fn(h->user, r, a0, a1, a2, a3);
}

/* Ordering-table tags store a 24-bit link in the first three bytes and keep
 * the high-byte packet length.  The MIPS code uses partial-word stores here. */
static void progress_write_tag24(AeRam *r, uint32_t address, uint32_t tag)
{
    ae_wr8(r, address, tag);
    ae_wr8(r, address + 1u, tag >> 8);
    ae_wr8(r, address + 2u, tag >> 16);
}

/* 800119F4 */
uint32_t ae_SaveSlotInfoPtr(AeRam *r, uint32_t slot)
{
    uint32_t slot_offset = slot * AE_PROGRESS_SAVE_SLOT_SIZE;
    return ae_SaveBlockPtr(r) + AE_PROGRESS_SAVE_BLOCK_SLOT_INFO + slot_offset;
}

/* 80016E68: advance one frame unless frozen. At the end marker the clear loop
 * bit restarts at frame one; the set bit preserves the current frame. */
void ae_MeshAnimAdvance(AeRam *r, uint32_t mesh_anim)
{
    uint32_t animation_data = ae_rd32(r, mesh_anim + AE_PROGRESS_MESH_ANIM_DATA);
    uint32_t frame = ae_rd16(r, mesh_anim + AE_PROGRESS_MESH_ANIM_FRAME);
    uint32_t flags = ae_rd16(r, mesh_anim + AE_PROGRESS_MESH_ANIM_FLAGS);
    uint32_t frame_count = ae_rd16(r, animation_data + AE_PROGRESS_ANIM_FRAME_COUNT);
    uint32_t end_marker = ae_rd16(
        r, animation_data + frame_count * AE_PROGRESS_ANIM_TABLE_ENTRY_SIZE);

    if ((flags & 1u) != 0u) return;

    uint32_t next_frame = frame + 1u;
    if (end_marker < next_frame) {
        next_frame = (flags & 2u) == 0u ? 1u : frame;
    }
    ae_wr16(r, mesh_anim + AE_PROGRESS_MESH_ANIM_FRAME, next_frame);
}

/* 8006D548 */
void ae_SpriteEffectQueue0(AeRam *r, uint32_t spec)
{
    ae_SpriteEffectQueue(r, spec, 0);
}

/* 8007292C */
int32_t ae_SoundLoopsPause(AeRam *r, const AeProgressHooks *h)
{
    uint32_t loop_a = AE_PROGRESS_G_PAUSE_LOOP_A;
    if (ae_rds16(r, loop_a + AE_PROGRESS_SOUND_LOOP_VOICE) != 0) {
        (void)ae_SoundLoopStop(r, h != NULL ? h->sound_hw : NULL,
                               0x16, ae_rds16(r, loop_a));
        ae_wr16(r, loop_a + AE_PROGRESS_SOUND_LOOP_VOICE, 0u);
    }

    uint32_t loop_b = AE_PROGRESS_G_PAUSE_LOOP_B;
    if (ae_rds16(r, loop_b + AE_PROGRESS_SOUND_LOOP_VOICE) != 0) {
        (void)ae_SoundLoopStop(r, h != NULL ? h->sound_hw : NULL,
                               0xAD, ae_rds16(r, loop_b));
        ae_wr16(r, loop_b + AE_PROGRESS_SOUND_LOOP_VOICE, 0u);
    }

    if (ae_rd16(r, AE_PROGRESS_G_CURRENT_LEVEL) == 0x1Bu) {
        progress_call(r, h, h != NULL ? h->FUN_8013FB20 : NULL,
                      "FUN_8013FB20", 0u, 0u, 0u, 0u);
    }
    progress_call(r, h, h != NULL ? h->SoundLoopsSetPan : NULL,
                  "SoundLoopsSetPan", 0u, 0u, 0u, 0u);
    return 0;
}

/* 8007F400 */
void ae_ClearDAT_800BEF70(AeRam *r)
{
    ae_wr32(r, AE_PROGRESS_G_PAUSE_MENU_CLEAR, 0u);
}

/* 8007F56C */
void ae_OpenPauseMenu(AeRam *r, int32_t request)
{
    const uint32_t open_flag = AE_PROGRESS_G_PAUSE_MENU_FLAG;
    const uint32_t pause_state = AE_PROGRESS_G_PAUSE_MENU_STATE;
    uint32_t stored_request;

    ae_wr8(r, open_flag, 0u);
    ae_ClearDAT_800BEF70(r);
    if (request == 1) {
        ae_wr16(r, pause_state, 0x0Fu);
        ae_CutsceneModeOff(r);
        stored_request = (uint32_t)request;
    } else {
        stored_request = 1u;
        ae_wr8(r, open_flag, stored_request);
        ae_wr16(r, pause_state, stored_request);
    }
    ae_wr8(r, AE_PROGRESS_G_PAUSE_MENU_REQUEST, stored_request);
}

/* 80082288: queue one 16-byte glyph packet for each byte >= 0x10, advance
 * the text cursor for every byte, then ask PsyQ to write the draw-page packet. */
void ae_DrawTextSprites(AeRam *r, const AeProgressHooks *h, uint32_t text,
                        int32_t x, int32_t y, int32_t page)
{
    uint32_t packet_cursor = ae_rd32(r, AE_PROGRESS_G_TEXT_SPRITE_CURSOR);
    uint32_t ot_index = ae_rd32(r, AE_PROGRESS_G_TEXT_SPRITE_OT_INDEX);
    uint32_t ot_stride = ot_index * AE_PROGRESS_TEXT_SPRITE_OT_STRIDE;
    uint32_t ot_cursor = AE_PROGRESS_G_TEXT_SPRITE_PACKET_TABLE + ot_stride;
    uint32_t input = text;
    uint32_t x_cursor = (uint32_t)x;
    uint32_t page_word = (((uint32_t)page +
        AE_PROGRESS_TEXT_SPRITE_PAGE_OFFSET) <<
        AE_PROGRESS_TEXT_SPRITE_PAGE_SHIFT) | AE_PROGRESS_TEXT_SPRITE_PAGE_BASE;

    for (;;) {
        uint32_t glyph = ae_rd8(r, input);
        if (glyph == 0u) break;

        if (glyph >= 0x10u) {
            uint32_t packet = packet_cursor;
            uint32_t previous_link = ae_rd32(
                r, ot_cursor + AE_PROGRESS_TEXT_SPRITE_TAG);

            ae_wr8(r, packet + AE_PROGRESS_TEXT_SPRITE_CODE,
                   AE_PROGRESS_TEXT_SPRITE_CODE_VALUE);
            ae_wr8(r, packet + AE_PROGRESS_TEXT_SPRITE_COLOR,
                   AE_PROGRESS_TEXT_SPRITE_COLOR_VALUE);
            ae_wr16(r, packet + AE_PROGRESS_TEXT_SPRITE_X, x_cursor);
            ae_wr16(r, packet + AE_PROGRESS_TEXT_SPRITE_Y, (uint32_t)y);
            ae_wr8(r, packet + AE_PROGRESS_TEXT_SPRITE_U,
                   (glyph & 0x0Fu) << 4);
            ae_wr8(r, packet + AE_PROGRESS_TEXT_SPRITE_V, glyph & 0xF0u);
            ae_wr16(r, packet + AE_PROGRESS_TEXT_SPRITE_TPAGE, page_word);

            progress_write_tag24(r,
                ot_cursor + AE_PROGRESS_TEXT_SPRITE_TAG, packet_cursor);
            progress_write_tag24(r,
                packet + AE_PROGRESS_TEXT_SPRITE_TAG, previous_link);
            packet_cursor += AE_PROGRESS_TEXT_SPRITE_PACKET_SIZE;
        }

        x_cursor += ae_rd8(r, AE_PROGRESS_G_TEXT_SPRITE_ADVANCE_TABLE + glyph);
        input += AE_PROGRESS_TEXT_SPRITE_SCAN_STEP;
    }

    progress_call(r, h, h != NULL ? h->SetDrawTPage : NULL,
                  "SetDrawTPage", packet_cursor, 0u, 1u,
                  AE_PROGRESS_TEXT_SPRITE_DRAW_PAGE);

    uint32_t tpage_link = ae_rd32(
        r, ot_cursor + AE_PROGRESS_TEXT_SPRITE_TAG);
    progress_write_tag24(r,
        ot_cursor + AE_PROGRESS_TEXT_SPRITE_TAG, packet_cursor);
    progress_write_tag24(r,
        packet_cursor + AE_PROGRESS_TEXT_SPRITE_TAG, tpage_link);
    packet_cursor += AE_PROGRESS_TEXT_SPRITE_PACKET_SIZE;
    ae_wr32(r, AE_PROGRESS_G_TEXT_SPRITE_CURSOR, packet_cursor);
}

static void progress_sound_bank_set_volume(AeRam *r,
                                           const AeProgressHooks *h,
                                           int32_t bank, int32_t volume)
{
    progress_call(r, h, h != NULL ? h->SoundBankSetVolume : NULL,
                  "SoundBankSetVolume", (uint32_t)bank,
                  (uint32_t)volume, 0u, 0u);
}

/* 800862E4 */
int32_t ae_SoundSetVolumes(AeRam *r, const AeProgressHooks *h,
                           int32_t sfx_volume, int32_t bgm_volume)
{
    if (sfx_volume >= 0x80) sfx_volume = 0x7F;

    if (sfx_volume != ae_rds16(r, AE_PROGRESS_G_SFX_VOLUME)) {
        int32_t sfx_bank = ae_rds16(r, AE_PROGRESS_G_SOUND_BANK_SFX);
        ae_wr16(r, AE_PROGRESS_G_SFX_VOLUME, (uint32_t)sfx_volume);
        if (sfx_bank >= 0) {
            progress_sound_bank_set_volume(r, h, sfx_bank, sfx_volume);
        }

        int32_t bgm_bank = ae_rds16(r, AE_PROGRESS_G_SOUND_BANK_BGM);
        if (bgm_bank >= 0) {
            progress_sound_bank_set_volume(r, h, bgm_bank,
                                           ae_rds16(r, AE_PROGRESS_G_SFX_VOLUME));
        }
    }

    if (bgm_volume >= 0x80) bgm_volume = 0x7F;
    if (bgm_volume != ae_rds16(r, AE_PROGRESS_G_BGM_VOLUME)) {
        ae_wr16(r, AE_PROGRESS_G_BGM_VOLUME, (uint32_t)bgm_volume);
        int32_t voice = ae_rds16(r, AE_PROGRESS_G_SOUND_BGM_VOICE);
        if (voice >= 0) {
            (void)ae_BgmSeqSetVolume(r, h != NULL ? h->sound_hw : NULL,
                                     voice, bgm_volume);
        }
    }
    return 0;
}

/* 80086428 */
int32_t ae_SoundVolumesDefault(AeRam *r, const AeProgressHooks *h,
                               int32_t apply_to_hardware)
{
    ae_wr16(r, AE_PROGRESS_G_BGM_VOLUME, 75u);
    ae_wr16(r, AE_PROGRESS_G_SFX_VOLUME, 0x7Fu);

    if (apply_to_hardware != 0) {
        int32_t sfx_bank = ae_rds16(r, AE_PROGRESS_G_SOUND_BANK_SFX);
        if (sfx_bank >= 0) {
            progress_sound_bank_set_volume(r, h, sfx_bank, 0x7F);
        }

        int32_t bgm_bank = ae_rds16(r, AE_PROGRESS_G_SOUND_BANK_BGM);
        if (bgm_bank >= 0) {
            progress_sound_bank_set_volume(r, h, bgm_bank, 0x7F);
        }

        int32_t voice = ae_rds16(r, AE_PROGRESS_G_SOUND_BGM_VOICE);
        if (voice >= 0) {
            (void)ae_BgmSeqSetVolume(r, h != NULL ? h->sound_hw : NULL,
                                     voice, 75);
        }
    }
    return 0;
}

/* 800864B4 */
int32_t ae_SoundGetVolumes(AeRam *r, uint32_t out)
{
    int32_t sfx_volume = ae_rds16(r, AE_PROGRESS_G_SFX_VOLUME);
    int32_t bgm_volume = ae_rds16(r, AE_PROGRESS_G_BGM_VOLUME);
    ae_wr32(r, out, (uint32_t)sfx_volume);
    ae_wr32(r, out + AE_PROGRESS_HEADER_VOLUME_WORD_SIZE, (uint32_t)bgm_volume);
    return 0;
}

/* 800864DC */
int32_t ae_SoundSaveVolumes(AeRam *r)
{
    uint32_t bgm_volume = ae_rd16(r, AE_PROGRESS_G_BGM_VOLUME);
    uint32_t sfx_volume = ae_rd16(r, AE_PROGRESS_G_SFX_VOLUME);
    ae_wr16(r, AE_PROGRESS_G_SAVED_BGM_VOLUME, bgm_volume);
    ae_wr16(r, AE_PROGRESS_G_SAVED_SFX_VOLUME, sfx_volume);
    return 0;
}

/* 800864FC */
int32_t ae_SoundRestoreVolumes(AeRam *r, const AeProgressHooks *h)
{
    int32_t bgm_volume = progress_signed16(
        ae_rd16(r, AE_PROGRESS_G_SAVED_BGM_VOLUME));
    int32_t sfx_volume = progress_signed16(
        ae_rd16(r, AE_PROGRESS_G_SAVED_SFX_VOLUME));
    ae_wr16(r, AE_PROGRESS_G_BGM_VOLUME, (uint32_t)bgm_volume);
    ae_wr16(r, AE_PROGRESS_G_SFX_VOLUME, (uint32_t)sfx_volume);
    return ae_SoundSetVolumes(r, h, sfx_volume, bgm_volume);
}

/* 80095B3C */
int32_t ae_SaveBufferFill(AeRam *r, const AeProgressHooks *h)
{
    uint32_t save = AE_PROGRESS_SAVE_BLOCK;
    uint32_t level = ae_rd16(r, AE_PROGRESS_G_CURRENT_LEVEL);
    int32_t first_area = ae_GetLevelFirstArea(r, (int32_t)level);
    ae_wr16(r, save + AE_PROGRESS_SAVE_BLOCK_RESUME_AREA,
            (uint32_t)first_area);

    ae_wr16(r, save + AE_PROGRESS_SAVE_BLOCK_UNKNOWN_710,
            ae_rd16(r, AE_PROGRESS_G_SAVE_HEADER_WORD_158));
    (void)ae_SaveHeaderFill(r);

    ae_wr32(r, save + AE_PROGRESS_SAVE_BLOCK_STAGE_RESULTS,
            ae_rd32(r, AE_PROGRESS_G_STAGE_RESULT_FLAGS));
    for (uint32_t i = 0; i < AE_PROGRESS_SAVE_RECORD_COUNT; ++i) {
        ae_wr16(r, save + AE_PROGRESS_SAVE_BLOCK_AREA_RECORDS +
                   i * AE_PROGRESS_SAVE_ARRAY_HALFWORD_SIZE,
                ae_rd16(r, AE_PROGRESS_G_SAVED_AREA_RECORDS +
                           i * AE_PROGRESS_SAVE_ARRAY_HALFWORD_SIZE));
    }

    if (ae_IsTimeAttack(r) != 0) {
        for (uint32_t i = 0; i < AE_PROGRESS_SAVE_LEVEL_COUNT; ++i) {
            ae_wr32(r, save + AE_PROGRESS_SAVE_BLOCK_TIME_RECORDS +
                       i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE,
                    ae_rd32(r, AE_PROGRESS_G_TIME_RECORDS +
                               i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE));
        }
    } else {
        (void)ae_SaveCaptureState(r, h);
        for (uint32_t i = 0; i < AE_PROGRESS_GIMMICK_STATE_BYTE_COUNT; ++i) {
            ae_wr8(r, save + AE_PROGRESS_SAVE_BLOCK_GIMMICK_STATES + i,
                   ae_rd8(r, AE_PROGRESS_G_GIMMICK_STATES + i));
        }
        for (uint32_t i = 0; i < AE_PROGRESS_SAVE_LEVEL_COUNT; ++i) {
            ae_wr8(r, save + AE_PROGRESS_SAVE_BLOCK_LEVEL_PLAY_MODE + i,
                   ae_rd8(r, AE_PROGRESS_G_LEVEL_PLAY_MODE + i));
            ae_wr8(r, save + AE_PROGRESS_SAVE_BLOCK_LEVEL_COLLECTED + i,
                   ae_rd8(r, AE_PROGRESS_G_LEVEL_COLLECTED + i));
        }
        for (uint32_t i = 0; i < AE_PROGRESS_SAVE_PROGRESS_WORD_COUNT; ++i) {
            ae_wr32(r, save + AE_PROGRESS_SAVE_BLOCK_PROGRESS_FLAGS2 +
                       i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE,
                    ae_rd32(r, AE_PROGRESS_G_STAGE_RESULT_WORDS +
                               i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE));
        }
        ae_wr32(r, save + AE_PROGRESS_SAVE_BLOCK_GATE_FLAGS,
                ae_rd32(r, AE_PROGRESS_G_SPECIAL_GATE_FLAGS));
        ae_wr32(r, save + AE_PROGRESS_SAVE_BLOCK_UNKNOWN_68C,
                ae_rd32(r, AE_PROGRESS_G_AREA_KIND_VISITED));
        ae_wr32(r, save + AE_PROGRESS_SAVE_BLOCK_UNKNOWN_718,
                ae_rd32(r, AE_PROGRESS_G_STAGE_RESULT_AUX_WORD));
    }
    return 0;
}

/* 80095CEC */
int32_t ae_SaveCaptureState(AeRam *r, const AeProgressHooks *h)
{
    (void)ae_GimmickSaveStates(r);
    (void)ae_GimmickSaveApplyProgress(r,
        h != NULL ? h->gimmick_a : NULL);
    for (uint32_t i = 0; i < AE_PROGRESS_SAVE_LEVEL_COUNT; ++i) {
        ae_wr32(r, AE_PROGRESS_SAVE_BLOCK +
                   AE_PROGRESS_SAVE_BLOCK_PROGRESS_FLAGS +
                   i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE,
                ae_rd32(r, AE_PROGRESS_G_FLAGS +
                           i * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE));
    }
    return 0;
}

/* 80095D44 */
int32_t ae_SaveHeaderFill(AeRam *r)
{
    uint32_t save = AE_PROGRESS_SAVE_BLOCK;
    (void)ae_SoundGetVolumes(r, AE_PROGRESS_HEADER_VOLUME_STACK);

    uint32_t options = 0u;
    if (ae_rd16(r, AE_PROGRESS_G_OPTION_FLAG_B) != 0u) options |= 1u;
    if (ae_rd8(r, AE_PROGRESS_G_OPTION_FLAG_A) != 0u) options |= 2u;
    options |= ae_rd8(r, AE_PROGRESS_HEADER_SFX_LOCAL) << 16;
    options |= ae_rd8(r, AE_PROGRESS_HEADER_BGM_LOCAL) << 24;
    ae_wr32(r, save + AE_PROGRESS_SAVE_BLOCK_OPTIONS, options);

    uint32_t lives = ae_rd16(r, AE_PROGRESS_G_LIVES);
    uint32_t saved_byte = ae_rd16(r, AE_PROGRESS_G_SAVE_HEADER_BYTE_168);
    ae_wr16(r, save + AE_PROGRESS_SAVE_HEADER_LIVES,
            lives | (saved_byte << 8));
    ae_wr32(r, save + AE_PROGRESS_SAVE_HEADER_GADGET_MASK,
            ae_rd32(r, AE_PROGRESS_G_GADGET_UNLOCK_MASK));
    ae_wr32(r, save + AE_PROGRESS_SAVE_HEADER_GADGET_SLOTS,
            ae_rd32(r, AE_PROGRESS_G_GADGET_SLOTS));

    uint32_t ammo = (ae_rd8(r, AE_PROGRESS_G_SAVED_OPTION_BYTE) << 16) & 0x00FF0000u;
    ammo |= ae_rd8(r, AE_PROGRESS_G_SLING_AMMO1) << 8;
    ammo |= ae_rd8(r, AE_PROGRESS_G_SLING_AMMO2);
    ae_wr32(r, save + AE_PROGRESS_SAVE_HEADER_AMMO, ammo);

    uint32_t state = (ae_rd16(r, AE_PROGRESS_G_SAVE_HEADER_WORD_154) << 8) & 0x0000FF00u;
    state |= ae_rd8(r, AE_PROGRESS_G_SAVE_HEADER_BYTE_156);
    ae_wr32(r, save + AE_PROGRESS_SAVE_HEADER_GADGET_STATE, state);
    return 0;
}

/* 800961D0 */
int32_t ae_RecordBestTime(AeRam *r)
{
    if (ae_IsTimeAttack(r) == 0) return 0;

    uint32_t level = ae_rd16(r, AE_PROGRESS_G_CURRENT_LEVEL);
    uint32_t record_index;
    if (level - AE_PROGRESS_STAGE_RESULT_TIME_ATTACK_FIRST <
        AE_PROGRESS_STAGE_RESULT_LEVELS) {
        /* The listing uses the level only for this range check, then reads
         * and writes 0x800DFD40 + 96: record slot 24 for every TA level. */
        record_index = AE_PROGRESS_STAGE_RESULT_TIME_ATTACK_FIRST;
        uint32_t record = AE_PROGRESS_G_TIME_RECORDS +
                          record_index * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE;
        uint32_t previous = ae_rd32(r, record);
        uint32_t current = ae_rd32(r, AE_PROGRESS_G_PLAY_TIME);
        ae_wr32(r, AE_PROGRESS_G_PREVIOUS_PLAY_TIME, previous);
        if (current < previous) ae_wr32(r, record, current);
    } else {
        record_index = level;
        uint32_t record = AE_PROGRESS_G_TIME_RECORDS +
                          record_index * AE_PROGRESS_SAVE_ARRAY_WORD_SIZE;
        uint32_t previous = ae_rd32(r, record);
        uint32_t current = ae_rd32(r, AE_PROGRESS_G_PLAY_TIME);
        ae_wr32(r, AE_PROGRESS_G_PREVIOUS_PLAY_TIME, previous);
        if (current < previous) ae_wr32(r, record, current);
    }
    return 0;
}

/* 8009628C */
uint32_t ae_SaveBlockPtr(AeRam *r)
{
    (void)r;
    return AE_PROGRESS_SAVE_BLOCK;
}

/* 800963DC */
int32_t ae_ProgressMarkStageResult(AeRam *r, int32_t area,
                                   int32_t exit_code)
{
    uint32_t result_bits = 1u;
    uint32_t progress = ae_rd32(r, AE_PROGRESS_G_STAGE_RESULT_FLAGS);

    if (area == AE_PROGRESS_STAGE_RESULT_SPECIAL_AREA) {
        if (exit_code == 8) {
            result_bits |= AE_PROGRESS_STAGE_RESULT_SPECIAL_BIT;
            if ((progress & AE_PROGRESS_STAGE_RESULT_SPECIAL_BIT) == 0u) {
                uint16_t count = (uint16_t)ae_rd16(r,
                                                   AE_PROGRESS_G_PICKUP_COUNTER);
                ae_wr16(r, AE_PROGRESS_G_PICKUP_COUNTER,
                        (uint16_t)(count + AE_PROGRESS_STAGE_RESULT_BONUS));
                result_bits = 9u | AE_PROGRESS_STAGE_RESULT_SPECIAL_BIT;
            }
        }
        progress &= ~AE_PROGRESS_STAGE_RESULT_SPECIAL_CLEAR;
    } else {
        result_bits = AE_PROGRESS_STAGE_RESULT_STANDARD_BIT;
        if (exit_code == 8) {
            result_bits |= AE_PROGRESS_STAGE_RESULT_STANDARD_EXTRA;
            if ((progress & AE_PROGRESS_STAGE_RESULT_STANDARD_EXTRA) == 0u) {
                uint16_t count = (uint16_t)ae_rd16(r,
                                                   AE_PROGRESS_G_PICKUP_COUNTER);
                ae_wr16(r, AE_PROGRESS_G_PICKUP_COUNTER,
                        (uint16_t)(count + AE_PROGRESS_STAGE_RESULT_BONUS));
                result_bits = 0x0900u |
                              AE_PROGRESS_STAGE_RESULT_STANDARD_EXTRA;
            }
        }
        progress &= ~AE_PROGRESS_STAGE_RESULT_STANDARD_CLEAR;
    }

    ae_wr32(r, AE_PROGRESS_G_STAGE_RESULT_FLAGS,
            progress | (result_bits & 0xFFFFu));
    return 0;
}

/* 80096514 */
int32_t ae_AreaKindVisitedGet(AeRam *r,
                              const struct AeAreaSetHooks *area_hooks,
                              int32_t area)
{
    int32_t kind = ae_AreaGetKind(r, area_hooks, area);
    uint32_t visited = ae_rd32(r, AE_PROGRESS_G_AREA_KIND_VISITED);
    return (int32_t)((visited >> ((uint32_t)kind & 31u)) & 1u);
}

/* 80096580 */
int32_t ae_LevelProgressGet(AeRam *r, uint32_t level)
{
    if (level - AE_PROGRESS_TIME_ATTACK_LEVEL_FIRST <
        AE_PROGRESS_STAGE_RESULT_LEVELS) {
        return (int32_t)ae_rd8(r, AE_PROGRESS_G_TIME_ATTACK_LEVEL_COUNT);
    }
    return (int32_t)ae_rd8(r, AE_PROGRESS_G_LEVEL_PLAY_MODE + level);
}

/* 800976B8 */
int32_t ae_HubDestinationState(AeRam *r, const AeProgressHooks *h,
                               int32_t area, int32_t exit_code)
{
    const struct AeAreaSetHooks *area_hooks = h != NULL ? h->area_set : NULL;

    if (ae_HubGateBitTest(r, 6u) == 0 &&
        ae_LevelProgressGet(r, 0x18u) > 0 &&
        ae_AreaGetKind(r, area_hooks, area) != 3) {
        ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 0xFFFFu);
        ae_wr16(r, AE_PROGRESS_G_HUB_EXIT_SOURCE_AREA, (uint32_t)exit_code);
        ae_wr16(r, AE_PROGRESS_G_HUB_EXIT_DEST_AREA, (uint32_t)area);
        return 2;
    }

    int32_t level = ae_AreaGetLevel(r, area_hooks, area);
    if (ae_LevelProgressGet(r, (uint32_t)level) == 2 &&
        exit_code == AE_PROGRESS_HUB_STAGE_EXIT_CODE) {
        return AE_PROGRESS_HUB_DESTINATION_READY;
    }
    if (ae_HubGateLocked(r, area) != 0) return 0;

    int32_t kind = ae_AreaGetKind(r, area_hooks, area);
    uint32_t expected_level = ae_rds16(
        r, AE_PROGRESS_G_HUB_KIND_LEVELS +
               (uint32_t)kind * AE_PROGRESS_HUB_TABLE_ENTRY_SIZE);
    if (level != (int32_t)expected_level) return 1;
    return ae_AreaKindVisitedGet(r, area_hooks, area) != 0 ? 1 : 0;
}

/* 800977BC */
int32_t ae_HubGateLocked(AeRam *r, int32_t area)
{
    uint32_t progress = ae_rd32(r, AE_PROGRESS_G_SPECIAL_GATE_FLAGS);
    for (uint32_t index = 0u; ; ++index) {
        int32_t listed_area = ae_rds16(
            r, AE_PROGRESS_G_HUB_EXIT_GATE_AREAS +
                   index * AE_PROGRESS_HUB_TABLE_ENTRY_SIZE);
        if (listed_area < 0) return 0;
        if (listed_area == area &&
            (progress & (1u << (index & 31u))) == 0u) {
            return 1;
        }
    }
}

/* 80097820 */
int32_t ae_UnlockSpecialArea(AeRam *r)
{
    uint32_t area = ae_rd16(r, AE_PROGRESS_G_CURRENT_AREA);
    uint32_t special_index = area - AE_PROGRESS_SPECIAL_AREA_FIRST;
    uint32_t progress_index = ae_rd8(
        r, AE_PROGRESS_G_HUB_SPECIAL_PROGRESS_IDS + special_index);
    uint32_t progress_bit = 1u << (progress_index & 31u);
    ae_wr32(r, AE_PROGRESS_G_SPECIAL_GATE_FLAGS,
            ae_rd32(r, AE_PROGRESS_G_SPECIAL_GATE_FLAGS) | progress_bit);

    if (special_index < AE_PROGRESS_SPECIAL_AREA_COUNT) {
        static const uint32_t gadget_masks[AE_PROGRESS_SPECIAL_AREA_COUNT] = {
            AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_0,
            AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_1,
            AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_2,
            AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_3,
            AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_4,
            AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_5,
            AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_6
        };
        ae_GadgetUnlockMask(r, gadget_masks[special_index]);
    }
    return 0;
}

/* 800978E0 */
int32_t ae_HubGateBitTest(AeRam *r, uint32_t bit)
{
    uint32_t mask = 1u << (bit & 31u);
    return (ae_rd32(r, AE_PROGRESS_G_SPECIAL_GATE_FLAGS) & mask) != 0u;
}

/* 800978FC: special areas select a hub destination and a follow-up mode. */
int32_t ae_ExitSpecialArea(AeRam *r, const AeProgressHooks *h)
{
    uint32_t area = ae_rd16(r, AE_PROGRESS_G_CURRENT_AREA);
    uint32_t special_index = area - AE_PROGRESS_SPECIAL_AREA_FIRST;
    if (special_index >= AE_PROGRESS_SPECIAL_AREA_COUNT) return 0;

    switch (special_index) {
    case 0u:
        ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 6u);
        ae_wr16(r, AE_PROGRESS_G_GAME_MODE, 8u);
        break;
    case 1u:
        ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 15u);
        ae_wr16(r, AE_PROGRESS_G_GAME_MODE, 10u);
        break;
    case 2u:
        ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 20u);
        ae_wr16(r, AE_PROGRESS_G_GAME_MODE, 8u);
        break;
    case 3u: {
        int32_t source_area = ae_rds16(r, AE_PROGRESS_G_HUB_EXIT_SOURCE_AREA);
        int32_t destination = ae_rds16(r, AE_PROGRESS_G_HUB_EXIT_DEST_AREA);
        ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, (uint32_t)destination);
        int32_t state = ae_HubDestinationState(r, h, destination, source_area);
        ae_wr16(r, AE_PROGRESS_G_GAME_MODE,
                state == AE_PROGRESS_HUB_DESTINATION_READY ? 18u : 10u);
        break;
    }
    case 4u:
        ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 30u);
        ae_wr16(r, AE_PROGRESS_G_GAME_MODE, 10u);
        break;
    case 5u:
        ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 53u);
        ae_wr16(r, AE_PROGRESS_G_GAME_MODE, 8u);
        break;
    case 6u:
        ae_wr16(r, AE_PROGRESS_G_CURRENT_AREA, 2u);
        ae_wr16(r, AE_PROGRESS_G_GAME_MODE, 10u);
        break;
    default:
        break;
    }
    return 0;
}
