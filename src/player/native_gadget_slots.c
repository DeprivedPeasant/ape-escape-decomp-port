/* Gadget slot defaults, translated from the listings of 0x8007B16C / 0x8007AFC4 / 0x8007C9D8 / 0x8007CA10. */
#include "native_gadget_slots.h"

#define SLOT(i) (AE_G_GADGET_SLOTS + (uint32_t)(i))

/* GadgetSlotsSave @8007C9D8: slots -> saved copy, current gadget (lb) -> word at +0x90 of the block (0x800F51B0). */
void ae_GadgetSlotsSave(AeRam *r) {
    for (uint32_t i = 0; i < 4; ++i) ae_wr8(r, AE_G_GADGET_SLOTS_SAVED + i, ae_rd8(r, SLOT(i)));
    ae_wr32(r, AE_G_GADGET_SLOTS_SAVED + 4, (uint32_t)ae_rds8(r, AE_G_PLAYER_CURRENT_GADGET));
}

/* GadgetSlotsRestore @8007CA10 */
void ae_GadgetSlotsRestore(AeRam *r) {
    for (uint32_t i = 0; i < 4; ++i) ae_wr8(r, SLOT(i), ae_rd8(r, AE_G_GADGET_SLOTS_SAVED + i));
    ae_wr8(r, AE_G_PLAYER_CURRENT_GADGET, ae_rd8(r, AE_G_GADGET_SLOTS_SAVED + 4));
}

/* GadgetSlotsInitForArea @8007AFC4 */
int32_t ae_GadgetSlotsInitForArea(AeRam *r, int32_t gadget) {
    int32_t s0 = gadget;
    ae_GadgetSlotsSave(r);
    uint32_t area = ae_rd16(r, 0x800F4476u);
    for (uint32_t i = 0; i < 4; ++i) ae_wr8(r, SLOT(i), 0xFF);
    uint32_t k = area - 0x5C;
    if (k < 12) {
        switch (k) {
        case 0: case 1: case 2: case 3: case 4: case 5:       /* areas 0x5C..0x61: the gadget of the area on Cross */
            ae_wr8(r, SLOT(3), k + 2); s0 = (int32_t)(k + 2); break;
        case 6: ae_wr8(r, SLOT(3), 0); s0 = 0; break;          /* 0x62 */
        case 7: case 8:                                         /* 0x63 / 0x64 */
            s0 = 0; ae_wr8(r, SLOT(0), 0); ae_wr8(r, SLOT(3), 1); break;
        case 9:                                                 /* 0x65 */
            s0 = 0; ae_wr8(r, SLOT(1), 3); ae_wr8(r, SLOT(2), 7); ae_wr8(r, SLOT(3), 1);
            ae_wr8(r, 0x800F51C1u, 9); ae_wr8(r, SLOT(0), 0); ae_wr8(r, 0x800F51C2u, 5); break;
        case 10:                                                /* 0x66 */
            s0 = 0; ae_wr8(r, SLOT(1), 2); ae_wr8(r, SLOT(2), 4); ae_wr8(r, SLOT(0), 0); ae_wr8(r, SLOT(3), 1); break;
        default:                                                /* 0x67 */
            s0 = 0; ae_wr8(r, SLOT(1), 6); ae_wr8(r, SLOT(2), 7); ae_wr8(r, SLOT(0), 0); ae_wr8(r, SLOT(3), 1); break;
        }
    }
    return s0;
}

/* GadgetSlotsInitForExit @8007B16C */
void ae_GadgetSlotsInitForExit(AeRam *r, const AeGadgetHooks *h) {
    int32_t s0 = ae_rds8(r, AE_G_PLAYER_CURRENT_GADGET), a0;
    uint32_t code = ae_rd8(r, AE_G_STAGE_EXIT_CODE);
    int area_init = 0;
    a0 = s0;
    if (code < 9) {
        switch (code) {
        case 0: case 7: case 8:                                  /* new game / default loadout */
            s0 = 0;
            ae_wr8(r, SLOT(1), 0xFF); ae_wr8(r, SLOT(2), 0xFF); ae_wr8(r, SLOT(3), 1); ae_wr8(r, SLOT(0), 0);
            ae_GadgetSlotsSave(r);
            ae_wr32(r, AE_G_GADGET_UNLOCK_MASK, 3);
            a0 = s0;
            break;
        case 1: {
            uint32_t v = ae_rd16(r, AE_G_LEVEL_FLAG_4472);
            if (v == 0xE || v == 0xF) area_init = 1;
            else if (v == 3) {
                ae_GadgetSlotsSave(r);
                ae_wr8(r, SLOT(0), 0xFF); ae_wr8(r, SLOT(1), 0xFF); ae_wr8(r, SLOT(2), 0xFF);
                if (ae_rd16(r, 0x800F4476u) == 0x13) { ae_wr8(r, SLOT(3), 0xFF); s0 = 9; }
                else { ae_wr8(r, SLOT(3), 6); s0 = 6; }
                a0 = s0;
            }
            break; }
        case 2:
            if (ae_rd16(r, 0x800F4476u) == 0x5A && ae_rd16(r, 0x800F447Au) != 0) {
                ae_GadgetSlotsRestore(r);
                s0 = ae_rds8(r, AE_G_PLAYER_CURRENT_GADGET);
                a0 = s0;
            } else if (ae_rd16(r, 0x800F4476u) - 0x5Cu < 7u) area_init = 1;
            break;
        default: break;                                          /* 3..6: neither slots nor gadget change */
        }
        if (area_init) { s0 = ae_GadgetSlotsInitForArea(r, s0); a0 = s0; }
    } else a0 = s0;
    ae_wr8(r, AE_G_PLAYER_CURRENT_GADGET, 9);
    ae_wr16(r, 0x800F51E8u, 0xFFFF);
    ae_wr16(r, 0x800F51EAu, 0xFFFF);
    ae_wr8(r, 0x800F51C3u, 1);
    if (!h || !h->select_gadget) ae_ram_fail(r, "GadgetSlotsInitForExit: hook select_gadget not provided");
    else h->select_gadget(h->user, r, a0, -1);
    if (!h || !h->hud_find_icon_pos) ae_ram_fail(r, "GadgetSlotsInitForExit: hook hud_find_icon_pos not provided");
    else h->hud_find_icon_pos(h->user, r);
}
