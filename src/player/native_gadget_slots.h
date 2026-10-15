#ifndef AE_NATIVE_GADGET_SLOTS_H
#define AE_NATIVE_GADGET_SLOTS_H
/*
 * Gadget slot / unlock defaults, ported instruction for instruction onto an AeRam image:
 * GadgetSlotsInitForExit 0x8007B16C (with GadgetSlotsInitForArea 0x8007AFC4, GadgetSlotsSave 0x8007C9D8 and
 * GadgetSlotsRestore 0x8007CA10).  Diffed by tools/mips/check_player_input.py.
 *
 * A NEW GAME (GameStateReset 0x8005CE04 sets g_StageExitCode = 0 and calls GadgetSlotsInitForExit; SaveNewGameInit
 * 0x80095960 does not touch the gadget globals at all) ends with
 *     g_GadgetSlots = { 0, -1, -1, 1 }   Triangle = gadget 0 (Stun Club), Square / Circle empty, Cross = gadget 1 (Monkey Net)
 *     g_GadgetUnlockMask = 3             (bit n = gadget n unlocked: club and net)
 *     g_Player.currentGadget = 9, then PlayerSelectGadget(0, -1): the Stun Club is equipped.
 * (PlayerCheckGadgetSelect maps Triangle / Square / Circle / Cross = pad bits 0x10 / 0x80 / 0x20 / 0x40 to slots 0..3.)
 */
#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_G_GADGET_SLOTS 0x800F51A8u        /* 4 x s8: gadget id on Triangle / Square / Circle / Cross, -1 = empty */
#define AE_G_GADGET_SLOTS_SAVED 0x800F51ACu  /* GadgetSlotsSave copy (4 bytes) + the current gadget (u8) at +4 */
#define AE_G_GADGET_UNLOCK_MASK 0x800F51C4u  /* u32 */
#define AE_G_PLAYER_CURRENT_GADGET 0x800EC2D2u
#define AE_G_STAGE_EXIT_CODE 0x800F447Cu     /* u8 */
#define AE_G_LEVEL_FLAG_4472 0x800F4472u     /* DAT_800f4472 (u16) */

typedef struct AeGadgetHooks {
    void *user;
    void (*select_gadget)(void *user, AeRam *r, int32_t gadget, int32_t old);   /* PlayerSelectGadget 8007AC64 */
    void (*hud_find_icon_pos)(void *user, AeRam *r);                             /* HudFindGadgetIconPos 8008197C */
} AeGadgetHooks;

void ae_GadgetSlotsSave(AeRam *r);                                    /* 8007C9D8 */
void ae_GadgetSlotsRestore(AeRam *r);                                 /* 8007CA10 */
int32_t ae_GadgetSlotsInitForArea(AeRam *r, int32_t gadget);          /* 8007AFC4: returns the gadget to equip */
void ae_GadgetSlotsInitForExit(AeRam *r, const AeGadgetHooks *h);     /* 8007B16C (a NULL hook that is called fails closed) */

#ifdef __cplusplus
}
#endif
#endif
