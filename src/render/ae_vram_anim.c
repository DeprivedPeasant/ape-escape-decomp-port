#include "ae_vram_anim.h"

/* Temporary RECT in the original caller stack window. */
#define AE_VA_STACK_MOVE_RECT 0x801FFEE0u

int32_t ae_AreaSetVramAnimList(AeRam *ram, const AeVramAnimHooks *hooks, uint32_t list)
{
    if (list == 0u) {
        ae_wr32(ram, AE_VA_G_AREA_ANIM_COUNT, 0u);
        ae_wr32(ram, AE_VA_G_AREA_ANIM_LIST, 0u);
        return (int32_t)AE_VA_G_AREA_ANIM_COUNT;
    }

    ae_wr32(ram, AE_VA_G_AREA_ANIM_LIST, list);
    uint32_t entry = list;
    while (ae_rd8(ram, entry) != 0u) {
        uint32_t type = ae_rd8(ram, entry);
        int should_move = type == 2u ||
                          (type == 1u && ae_rd8(ram, entry + AE_VA_AREA_ANIM_TYPE_FILTER) == type);
        if (should_move) {
            ae_wr16(ram, AE_VA_STACK_MOVE_RECT + AE_VA_RECT_X,
                    ae_rd16(ram, entry + AE_VA_AREA_ANIM_RECT_X));
            ae_wr16(ram, AE_VA_STACK_MOVE_RECT + AE_VA_RECT_Y,
                    ae_rd16(ram, entry + AE_VA_AREA_ANIM_RECT_Y));
            ae_wr16(ram, AE_VA_STACK_MOVE_RECT + AE_VA_RECT_WIDTH,
                    ae_rd8(ram, entry + AE_VA_AREA_ANIM_RECT_WIDTH));
            ae_wr16(ram, AE_VA_STACK_MOVE_RECT + AE_VA_RECT_HEIGHT,
                    ae_rd8(ram, entry + AE_VA_AREA_ANIM_RECT_HEIGHT));
            int32_t source_x = (int32_t)ae_rd16(ram, entry + AE_VA_AREA_ANIM_SOURCE_X);
            int32_t source_y = (int32_t)ae_rd16(ram, entry + AE_VA_AREA_ANIM_SOURCE_Y);
            if (!hooks || !hooks->MoveImage) {
                ae_ram_fail(ram, "AeVramAnimHooks.MoveImage not provided");
                return 0;
            }
            (void)hooks->MoveImage(hooks->user, ram, AE_VA_STACK_MOVE_RECT, source_x, source_y);
            if (ae_ram_failed(ram)) return 0;
        }

        ae_wr32(ram, AE_VA_G_AREA_ANIM_COUNT,
                ae_rd32(ram, AE_VA_G_AREA_ANIM_COUNT) + 1u);
        entry += AE_VA_AREA_ANIM_RECORD_SIZE;
    }
    return 0;
}

