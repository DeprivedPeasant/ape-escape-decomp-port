/* ABI exceptions whose public C interface differs from the original entry point. */
#include "ae_calls_special.h"
#include "ae_calls.h"
#include "../core/ae_sdk.h"
#include "../player/native_player.h"

#define SPECIAL(NAME) int32_t special_##NAME(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
#define UNUSED_WORDS (void)a0; (void)a1; (void)a2; (void)a3

SPECIAL(ae_PlayerAct_ScaleMoveVel) {
    UNUSED_WORDS;
    ae_PlayerAct_ScaleMoveVel(g->ram, &g->player_act, AE_PLAYER_ACT_PLAYER,
                              AE_PLAYER_ACT_GADGET4_STATE);
    return 0;
}

SPECIAL(ae_PlayerAct_TurnAroundAndJump) {
    UNUSED_WORDS;
    ae_PlayerAct_TurnAroundAndJump(g->ram, &g->player_act, AE_PLAYER_ACT_PLAYER,
                                  AE_PLAYER_ACT_GADGET4_STATE);
    return 0;
}

SPECIAL(ae_sdk_square0_lane) {
    (void)a2; (void)a3;
    int32_t input[3];
    for (uint32_t i = 0; i < 3; ++i) input[i] = ae_rds32(g->ram, a0 + i * 4);
    for (uint32_t i = 0; i < 3; ++i)
        ae_wr32(g->ram, a1 + i * 4, (uint32_t)ae_sdk_square0_lane(input[i]));
    return (int32_t)a1;
}

SPECIAL(ae_player_normalize) {
    (void)a2; (void)a3;
    /* This ledger helper duplicates VectorNormal's core with native arrays.
     * The RAM implementation preserves the original VECTOR (s32) input/output ABI. */
    return ae_sdk_VectorNormal(g->ram, a0, a1);
}

SPECIAL(ae_sdk_normalize_core) {
    UNUSED_WORDS;
    /* 80030210 is an internal tail entry taking t0/t1/t2 and a destination
     * in a1, not an a0-a3 function. It cannot be invoked by this public ABI. */
    ae_ram_fail(g->ram, "VectorNormal core is an internal register entry; call VectorNormal or VectorNormalSS");
    return 0;
}

SPECIAL(ae_pad_CameraRotMatrix) {
    /* The ledger explicitly verifies only the yaw matrix, not CameraLookAt.
     * Dispatch the complete entry to the native camera host until it is ported. */
    if (g->host.renderer.CameraLookAt)
        return g->host.renderer.CameraLookAt(g->host.user, g->ram, a0, a1, a2, a3,
                                             ae_rd32(g->ram, g->call_stack_pointer + 16u));
    return ae_game_missing_call(g, "CameraLookAt", 0x80046670u, a0, a1, a2, a3);
}

SPECIAL(ae_player_is_swimming) {
    UNUSED_WORDS;
    return ae_player_is_swimming((uint16_t)ae_rd16(g->ram, 0x800EC23Eu));
}

SPECIAL(ae_player_can_switch_gadget) {
    UNUSED_WORDS;
    return ae_player_can_switch_gadget((uint16_t)ae_rd16(g->ram, 0x800F44A0u),
                                       (uint16_t)ae_rd16(g->ram, 0x800EC23Eu));
}

SPECIAL(ae_player_gadget_select_request) {
    UNUSED_WORDS;
    /* Compose the verified decision helper with the complete selection port.
     * This adapter preserves the original selector's return value and effects. */
    int8_t slots[4];
    for (uint32_t i = 0; i < 4; ++i) slots[i] = (int8_t)ae_rds8(g->ram, 0x800F51A8u + i);
    int gadget;
    if (!ae_player_gadget_select_request((uint16_t)ae_rd16(g->ram, 0x800F44A0u),
        (uint16_t)ae_rd16(g->ram, 0x800EC23Eu), ae_rd32(g->ram, 0x1F800054u),
        ae_rd32(g->ram, 0x1F800058u), slots, &gadget)) return 0;
    return ae_PlayerSelectGadget(g->ram, &g->player2, &g->player_act, &g->spawn,
        &g->body, &g->host.sound_hw, gadget, ae_rds8(g->ram, 0x800EC2D2u));
}

/* Compiled in an isolated translation unit because native_gadget_slots.h and
 * ae_gadget.h use the same tag for different callback layouts. */
extern void ae_game_legacy_gadget_init(AeRam *, const void *);
SPECIAL(ae_GadgetSlotsInitForExit) {
    UNUSED_WORDS;
    ae_game_legacy_gadget_init(g->ram, &g->legacy_gadget);
    return 0;
}
