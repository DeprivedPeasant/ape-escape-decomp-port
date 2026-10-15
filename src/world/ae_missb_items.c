#include "../ui/ae_missb.h"
#include "ae_zako_init.h"
#include "ae_areaset.h"

#define AE_MISSB_G_ITEM_AREA_MASKS 0x800BF2B8u
#define AE_MISSB_G_ITEM_MASK_TEST 0x800BF2D8u
#define AE_MISSB_G_ITEM_RECORDS 0x800BEFA0u
#define AE_MISSB_G_CURRENT_GAME_STATE 0x800F4350u
#define AE_MISSB_G_CURRENT_LEVEL 0x0124u
#define AE_MISSB_G_CURRENT_AREA 0x0126u
#define AE_MISSB_ITEM_RECORD_SIZE 0x18u
#define AE_MISSB_ITEM_RECORD_ACTIVE 0x0Au
#define AE_MISSB_ITEM_AREA_MASK_CAPACITY 8u
#define AE_MISSB_ITEM_MASK_COUNT 32u
#define AE_MISSB_ITEM_DIAGNOSTIC_FORMAT 0x800A6160u

/* 800847C4 */
void ae_ItemsClearFlags(AeRam *r)
{
    for (uint32_t index = 0u; index < AE_MISSB_ITEM_AREA_MASK_CAPACITY; ++index) {
        ae_wr32(r, AE_MISSB_G_ITEM_AREA_MASKS + index * 4u, 0u);
    }
}

/* 800847EC: build the inverse occupancy mask for the current area's 32 item
 * records and save it in the area-relative eight-word table. */
void ae_ItemSaveCollectedMask(AeRam *r, const AeMissbHooks *h)
{
    uint32_t game_state = AE_MISSB_G_CURRENT_GAME_STATE;
    int32_t current_level = (int32_t)ae_rd16(r, game_state + AE_MISSB_G_CURRENT_LEVEL);
    int32_t first_area = ae_GetLevelFirstArea(r, current_level);
    int32_t current_area = (int32_t)ae_rd16(r, game_state + AE_MISSB_G_CURRENT_AREA);
    int32_t area_index = current_area - first_area;

    if (area_index >= (int32_t)AE_MISSB_ITEM_AREA_MASK_CAPACITY) {
        (void)ae_printf(r, h != NULL ? h->area_set : NULL,
                        AE_MISSB_ITEM_DIAGNOSTIC_FORMAT, 0u,
                        (uint32_t)area_index, 0u);
        area_index = 0;
    }

    uint32_t mask = 0u;
    int32_t item_index = (int32_t)AE_MISSB_ITEM_MASK_COUNT - 1;
    uint32_t record = AE_MISSB_G_ITEM_RECORDS +
                      (AE_MISSB_ITEM_MASK_COUNT - 1u) * AE_MISSB_ITEM_RECORD_SIZE;

    for (; item_index >= 0; --item_index) {
        mask <<= 1;
        uint32_t active = ae_rd8(r, record + AE_MISSB_ITEM_RECORD_ACTIVE);
        if (active == 0u || ae_rd8(r, AE_MISSB_G_ITEM_MASK_TEST +
                                   (uint32_t)item_index) == 0u) {
            mask |= 1u;
        }
        record -= AE_MISSB_ITEM_RECORD_SIZE;
    }

    uint32_t destination = AE_MISSB_G_ITEM_AREA_MASKS +
                          (uint32_t)area_index * 4u;
    ae_wr32(r, destination, mask);
}
