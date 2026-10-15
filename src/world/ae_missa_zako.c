#include "../core/ae_missa.h"
#include "ae_zako_init.h"

#define AE_MISSA_G_OBJECT_COUNT          0x800E77E0u
#define AE_MISSA_OBJECT_TABLE           0x800E6960u
#define AE_MISSA_OBJECT_STRIDE          0x00E4u
#define AE_MISSA_OBJECT_TYPE            0x007Eu
#define AE_MISSA_OBJECT_FLAGS           0x00E0u
#define AE_MISSA_OBJECT_ZAKO_INDEX       0x00CAu
#define AE_MISSA_OBJECT_INDEX_BIT        0x00CCu
#define AE_MISSA_OBJECT_FLAG_MASK        0x00000010u
#define AE_MISSA_LEVEL_OBJECTS            0x800E77D0u
#define AE_MISSA_LEVEL_OBJECTS_SPAWNS     0x0004u
#define AE_MISSA_LEVEL_OBJECTS_COUNT      0x000Cu
#define AE_MISSA_SPAWN_RECORD_STRIDE      0x0020u
#define AE_MISSA_SPAWN_SLOT               0x0000u
#define AE_MISSA_SPAWN_DEFEATED           0x0016u
#define AE_MISSA_STAGE_STATE              0x800F4350u
#define AE_MISSA_STAGE_LEVEL              0x0124u
#define AE_MISSA_STAGE_AREA               0x0126u
#define AE_MISSA_STAGE_SPECIAL_LEVEL      0x0160u
#define AE_MISSA_STAGE_SPECIAL_LEVEL_ID   32u
#define AE_MISSA_AREA_MASK_BASE           0x800B8D00u
#define AE_MISSA_AREA_MASK_COUNT          8u
#define AE_MISSA_PRINTF_BAD_AREA_FORMAT   0x800A4F60u
#define AE_MISSA_OBJECT_TYPE_ZAKO_A      0x68u
#define AE_MISSA_OBJECT_TYPE_ZAKO_B      0x69u

void ae_ZakoClearDefeatedBits(AeRam *r, uint32_t defeated_mask)
{
    int32_t object_count = ae_rds16(r, AE_MISSA_G_OBJECT_COUNT);
    int32_t object_index = object_count - 1;

    while (object_index >= 0) {
        uint32_t object = AE_MISSA_OBJECT_TABLE +
                          (uint32_t)object_index * AE_MISSA_OBJECT_STRIDE;
        uint32_t object_flags = ae_rd32(r, object + AE_MISSA_OBJECT_FLAGS);
        uint32_t object_type = ae_rd8(r, object + AE_MISSA_OBJECT_TYPE);

        if ((object_flags & AE_MISSA_OBJECT_FLAG_MASK) != 0u &&
            (object_type == AE_MISSA_OBJECT_TYPE_ZAKO_A ||
             object_type == AE_MISSA_OBJECT_TYPE_ZAKO_B)) {
            int32_t zako_index = ae_rds8(r, object + AE_MISSA_OBJECT_ZAKO_INDEX);
            uint32_t zako = AE_MISSA_OBJECT_TABLE +
                            (uint32_t)zako_index * AE_MISSA_OBJECT_STRIDE;
            uint32_t bit_index = ae_rd8(r, zako + AE_MISSA_OBJECT_INDEX_BIT);
            uint32_t bit = 1u << (bit_index & 31u);
            ae_wr32(r, defeated_mask, ae_rd32(r, defeated_mask) & ~bit);
        }
        --object_index;
    }
}

static int32_t missa_zako_hook(AeRam *r, const AeMissaHooks *hooks,
                               AeMissaHookFn fn, const char *missing,
                               uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    if (hooks == NULL || fn == NULL) {
        ae_ram_fail(r, "%s", missing);
        return 0;
    }
    return fn(hooks->user, r, a0, a1, a2, a3);
}

void ae_ZakoSaveDefeatedMask(AeRam *r, const AeMissaHooks *hooks)
{
    uint32_t stage = AE_MISSA_STAGE_STATE;
    uint32_t level = ae_rd16(r, stage + AE_MISSA_STAGE_LEVEL);
    uint32_t object_list = AE_MISSA_LEVEL_OBJECTS;
    int32_t spawn_count = ae_rds16(r, object_list + AE_MISSA_LEVEL_OBJECTS_COUNT);
    uint32_t spawn_records = ae_rd32(r, object_list + AE_MISSA_LEVEL_OBJECTS_SPAWNS);
    int32_t area_index;

    if (level == AE_MISSA_STAGE_SPECIAL_LEVEL_ID) {
        ae_wr16(r, stage + AE_MISSA_STAGE_SPECIAL_LEVEL, 0u);
        return;
    }
    if (spawn_count == 0) return;

    int32_t first_area = ae_GetLevelFirstArea(r, (int32_t)level);
    area_index = (int32_t)ae_rd16(r, stage + AE_MISSA_STAGE_AREA) - first_area;
    if (area_index >= (int32_t)AE_MISSA_AREA_MASK_COUNT) {
        (void)missa_zako_hook(
            r, hooks, hooks != NULL ? hooks->Printf : NULL,
            "ZakoSaveDefeatedMask: Printf hook missing",
            AE_MISSA_PRINTF_BAD_AREA_FORMAT, 0u, 0u, 0u);
        if (ae_ram_failed(r)) return;
        area_index = 0;
    }

    uint32_t entry_sp = hooks != NULL ? hooks->stack_pointer : 0u;
    if (entry_sp < 32u) {
        ae_ram_fail(r, "ZakoSaveDefeatedMask: entry stack pointer is unavailable");
        return;
    }
    uint32_t defeated_mask_address = entry_sp - 32u;
    ae_wr32(r, defeated_mask_address, 0u);
    int32_t spawn_index = spawn_count - 1;
    uint32_t record = spawn_records + (uint32_t)spawn_index * AE_MISSA_SPAWN_RECORD_STRIDE;
    while (spawn_index >= 0) {
        int32_t slot = (int32_t)ae_rd16(r, record + AE_MISSA_SPAWN_SLOT);
        int32_t status = ae_ZakoTypeIsIgnored(r, slot);
        if (status == 0) {
            uint32_t mask = ae_rd32(r, defeated_mask_address) << 1;
            if (ae_rd8(r, record + AE_MISSA_SPAWN_DEFEATED) == 0u) mask |= 1u;
            ae_wr32(r, defeated_mask_address, mask);
        }
        --spawn_index;
        record -= AE_MISSA_SPAWN_RECORD_STRIDE;
    }

    ae_ZakoClearDefeatedBits(r, defeated_mask_address);
    ae_wr32(r, AE_MISSA_AREA_MASK_BASE + (uint32_t)area_index * 4u,
            ae_rd32(r, defeated_mask_address));
}
