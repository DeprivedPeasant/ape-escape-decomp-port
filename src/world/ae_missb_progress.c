#include "../ui/ae_missb.h"
#include "ae_progress.h"
#include "ae_zako_init.h"
#include "ae_zako2.h"
#include "ae_items_init.h"

#define AE_MISSB_G_COLLECTED_AREAS 0x800DFB70u
#define AE_MISSB_G_LEVEL_PROGRESS  0x800DFC70u
#define AE_MISSB_G_PROGRESS_FLAGS  0x800DFCC0u
#define AE_MISSB_G_STAGE_RESULT    0x800DFDD0u
#define AE_MISSB_G_STAGE_AUDIO_FLAG 0x800B38B8u
#define AE_MISSB_G_STAGE_MANAGER   0x800F4350u
#define AE_MISSB_G_LEVEL_TABLE     0x80102400u
#define AE_MISSB_G_LEVEL_RECORDS   0x80102400u
#define AE_MISSB_G_LEVEL_24_PROGRESS 24u
#define AE_MISSB_LEVEL_TABLE_MONKEY_COUNT 0x01u
#define AE_MISSB_LEVEL_TABLE_RECORD_SIZE 0x10u
#define AE_MISSB_STAGE_EXIT_RESULT 0x12Eu
#define AE_MISSB_LEVEL_TABLE_SIZE  0x10u

#define AE_MISSB_SAVE_AREA_RECORDS 0x068u
#define AE_MISSB_SAVE_LEVEL_STATE  0x028u
#define AE_MISSB_SAVE_LEVEL_ITEMS  0x048u
#define AE_MISSB_SAVE_RESULT_WORD  0x678u
#define AE_MISSB_SAVE_LEVEL_TIMES  0x690u
#define AE_MISSB_SAVE_RESULT_FLAGS 0x66Cu
#define AE_MISSB_SAVE_LEVEL_BYTES  32u
#define AE_MISSB_SAVE_AREA_RECORD_COUNT 128u
#define AE_MISSB_SAVE_RESULT_WORD_COUNT 8u
#define AE_MISSB_DEFAULT_STACK_POINTER 0x801FFF00u
#define AE_MISSB_SAVE_BUILD_STACK_SIZE 0x7B8u
#define AE_MISSB_SAVE_BUILD_BUFFER_OFFSET 0x10u
#define AE_MISSB_SAVE_CHECKSUM_LENGTH 0x048u
#define AE_MISSB_SAVE_AREA_STATE_OFFSET 0x028u
#define AE_MISSB_SAVE_AREA_PROGRESS 0x046u
#define AE_MISSB_SAVE_MEDAL_SILVER 0x008u
#define AE_MISSB_SAVE_MEDAL_GOLD 0x00Au
#define AE_MISSB_SAVE_MEDAL_PLATINUM 0x00Cu
#define AE_MISSB_SAVE_GADGET_FLAGS 0x668u
#define AE_MISSB_SAVE_ITEM_FLAGS 0x670u
#define AE_MISSB_SAVE_ITEM_COUNT 0x674u

static uint32_t missb_arithmetic_shift_right(uint32_t value, uint32_t amount)
{
    uint32_t shifted = value >> amount;
    if ((value & 0x80000000u) != 0u) {
        shifted |= 0xFFFFFFFFu << (32u - amount);
    }
    return shifted;
}

/* 800962B0 */
int32_t ae_ProgressChecksum(AeRam *r, uint32_t bytes)
{
    if (bytes == 0u) bytes = AE_PROGRESS_G_LEVEL_COLLECTED;

    uint32_t sum = 0u;
    for (uint32_t index = 0u; index < 25u; ++index) {
        sum += ae_rd8(r, bytes + index);
    }
    return (int32_t)sum;
}

/* 800962F4 */
int32_t ae_ProgressFlagSet(AeRam *r, int32_t bit)
{
    uint32_t shifted_index = (uint32_t)bit << 11;
    uint32_t signed_word_index = missb_arithmetic_shift_right(shifted_index, 16u);
    uint32_t word_offset = signed_word_index << 2;
    uint32_t word_address = AE_MISSB_G_PROGRESS_FLAGS + word_offset;
    uint32_t bit_mask = 1u << ((uint32_t)bit & 0x1Fu);
    uint32_t updated = 0u;
    for (uint32_t byte = 0u; byte < 4u; ++byte)
        updated |= ae_rd8(r, word_address + byte) << (byte * 8u);
    updated |= bit_mask;
    for (uint32_t byte = 0u; byte < 4u; ++byte)
        ae_wr8(r, word_address + byte, updated >> (byte * 8u));
    return 0;
}

/* 80096360 */
int32_t ae_ProgressSetByte(AeRam *r, int32_t index, int32_t value)
{
    uint32_t index_word_offset = (uint32_t)index & 0x003FFFFCu;
    uint32_t byte_lane_shift = ((uint32_t)index & 3u) * 8u;
    uint32_t address = AE_PROGRESS_G_STAGE_RESULT_WORDS + index_word_offset;
    uint32_t old_word = ae_rd32(r, address);
    uint32_t lane_mask = 0xFFu << byte_lane_shift;
    uint32_t new_word = (old_word & ~lane_mask) |
                        (((uint32_t)value & 0xFFu) << byte_lane_shift);
    ae_wr32(r, address, new_word);
    return 0;
}

/* 800964B0 */
int32_t ae_ProgressGetStageResult(AeRam *r, int32_t area)
{
    if (area == 19) return (int32_t)ae_rd8(r, AE_MISSB_G_STAGE_RESULT);
    return (int32_t)((ae_rd32(r, AE_MISSB_G_STAGE_RESULT) >> 8) & 0xFFu);
}

/* 800965EC */
int32_t ae_LevelMonkeyCountGet(AeRam *r, int32_t level)
{
    uint32_t special_level_offset = (uint32_t)level - 24u;
    if (special_level_offset < 6u) return 24;

    uint32_t entry = AE_MISSB_G_LEVEL_TABLE + ((uint32_t)level << 4);
    return (int32_t)ae_rd8(r, entry + AE_MISSB_LEVEL_TABLE_MONKEY_COUNT);
}

/* 80096638 */
int32_t ae_LevelProgressRecordExit(AeRam *r, int32_t level)
{
    if (ae_IsTimeAttack(r) != 0) return 0;

    uint32_t progress_byte = ae_rd8(r, AE_MISSB_G_STAGE_MANAGER +
                                     AE_MISSB_STAGE_EXIT_RESULT);
    uint32_t special_level_offset = (uint32_t)level - 24u;
    uint32_t destination = special_level_offset < 6u
        ? AE_MISSB_G_LEVEL_PROGRESS + AE_MISSB_G_LEVEL_24_PROGRESS
        : AE_MISSB_G_LEVEL_PROGRESS + (uint32_t)level;
    ae_wr8(r, destination, progress_byte);
    return 0;
}

/* 80097620 */
int32_t ae_GetDAT_800B38B8(AeRam *r)
{
    return ae_rds16(r, AE_MISSB_G_STAGE_AUDIO_FLAG);
}

/* 80097E60 */
uint32_t ae_LevelTableGet(AeRam *r, int32_t level)
{
    (void)r;
    if ((uint32_t)level >= 30u) return 0u;
    return (AE_MISSB_G_LEVEL_TABLE + ((uint32_t)level << 4)) | 8u;
}

/* 80097FB0 */
int32_t ae_LevelCaughtCount(AeRam *r, int32_t level)
{
    uint32_t first_area;
    uint32_t area_count;
    uint32_t special_level_offset = (uint32_t)level - 24u;

    if (special_level_offset < 6u) {
        first_area = (uint32_t)ae_GetLevelFirstArea(r, 24);
        area_count = ae_rd8(r, AE_MISSB_G_LEVEL_RECORDS +
                            24u * AE_MISSB_LEVEL_TABLE_RECORD_SIZE);
        for (uint32_t special_level = 25u; special_level < 30u; ++special_level) {
            area_count += ae_rd8(r, AE_MISSB_G_LEVEL_RECORDS +
                                 special_level * AE_MISSB_LEVEL_TABLE_RECORD_SIZE);
        }
    } else {
        first_area = (uint32_t)ae_GetLevelFirstArea(r, level);
        uint32_t entry = AE_MISSB_G_LEVEL_TABLE + ((uint32_t)level << 4);
        area_count = ae_rd8(r, entry);
    }

    uint32_t caught = 0u;
    uint32_t end_area = first_area + area_count;
    for (uint32_t area = first_area; area < end_area; ++area) {
        if (ae_CollectedItemsHas(r, area) != 0) caught += 1u;
    }
    return (int32_t)caught;
}

/* 80098088: build the temporary save image at the original stack location,
 * then return its computed checksum.  The stack window is intentionally local. */
int32_t ae_SaveBuildImage(AeRam *r)
{
    const uint32_t save_image = AE_MISSB_DEFAULT_STACK_POINTER -
                                AE_MISSB_SAVE_BUILD_STACK_SIZE +
                                AE_MISSB_SAVE_BUILD_BUFFER_OFFSET;

    for (uint32_t record = 0u; record < AE_MISSB_SAVE_AREA_RECORD_COUNT; ++record) {
        uint32_t value = ae_rd16(r, AE_MISSB_G_COLLECTED_AREAS + record * 2u);
        ae_wr16(r, save_image + AE_MISSB_SAVE_AREA_RECORDS + record * 2u, value);
    }
    ae_wr32(r, save_image + AE_MISSB_SAVE_RESULT_WORD,
            ae_rd32(r, AE_MISSB_G_STAGE_RESULT));

    for (uint32_t level = 0u; level < AE_MISSB_SAVE_LEVEL_BYTES; ++level) {
        ae_wr8(r, save_image + AE_MISSB_SAVE_LEVEL_STATE + level,
               ae_rd8(r, AE_MISSB_G_LEVEL_PROGRESS + level));
        ae_wr8(r, save_image + AE_MISSB_SAVE_LEVEL_ITEMS + level,
               ae_rd8(r, AE_PROGRESS_G_LEVEL_COLLECTED + level));
    }

    for (uint32_t word = 0u; word < AE_MISSB_SAVE_RESULT_WORD_COUNT; ++word) {
        ae_wr32(r, save_image + AE_MISSB_SAVE_RESULT_FLAGS + word * 4u,
                ae_rd32(r, AE_PROGRESS_G_STAGE_RESULT_WORDS + word * 4u));
    }
    return ae_SaveComputeChecksum(r, save_image);
}

/* 8009816C */
int32_t ae_SaveComputeChecksum(AeRam *r, uint32_t image)
{
    uint32_t checksum = (uint32_t)ae_ProgressChecksum(
        r, image + AE_MISSB_SAVE_CHECKSUM_LENGTH);
    uint32_t level_table = AE_MISSB_G_LEVEL_TABLE;

    for (uint32_t level = 0u; level < 25u; ++level) {
        if (level == 0u || level == 7u || level == 14u ||
            level == 18u || level == 19u || level == 23u) {
            level_table += AE_MISSB_LEVEL_TABLE_SIZE;
            continue;
        }

        uint32_t progress = ae_rd8(r, image + AE_MISSB_SAVE_AREA_STATE_OFFSET + level);
        if (progress == 1u) checksum += 15u;
        else if (progress == 2u) checksum += 25u;

        uint32_t caught = ae_rd32(r, image + AE_MISSB_SAVE_LEVEL_TIMES + level * 4u);
        uint32_t platinum = (uint32_t)ae_rds16(r, level_table + AE_MISSB_SAVE_MEDAL_PLATINUM);
        uint32_t gold = (uint32_t)ae_rds16(r, level_table + AE_MISSB_SAVE_MEDAL_GOLD);
        uint32_t silver = (uint32_t)ae_rds16(r, level_table + AE_MISSB_SAVE_MEDAL_SILVER);
        if (caught < platinum) checksum += 8u;
        else if (caught < gold) checksum += 7u;
        else if (caught < silver) checksum += 5u;
        level_table += AE_MISSB_LEVEL_TABLE_SIZE;
    }

    if (ae_rd8(r, image + AE_MISSB_SAVE_AREA_PROGRESS) == 2u) checksum += 15u;

    uint32_t flags = ae_rd32(r, image + AE_MISSB_SAVE_GADGET_FLAGS);
    if ((flags & 0x10u) != 0u) checksum += 15u;
    if ((flags & 0x1000u) != 0u) checksum += 15u;

    uint32_t area_record = image + AE_MISSB_SAVE_AREA_RECORDS;
    for (uint32_t record = 0u; record < AE_MISSB_SAVE_AREA_RECORD_COUNT; ++record) {
        if (ae_rd8(r, area_record) == 0xFFu) break;
        checksum += 2u;
        area_record += 2u;
    }

    uint32_t gadget_bytes = ae_rd32(r, image + AE_MISSB_SAVE_ITEM_FLAGS);
    if ((gadget_bytes & 0xFFu) == 5u) checksum += 8u;
    if (((gadget_bytes >> 8) & 0xFFu) == 5u) checksum += 8u;
    if (ae_rd8(r, image + AE_MISSB_SAVE_ITEM_COUNT) == 5u) checksum += 8u;
    return (int32_t)checksum;
}
