#ifndef AE_PLAYERUPDATE_INTERNAL_H
#define AE_PLAYERUPDATE_INTERNAL_H

/* Portions of the complete PlayerUpdate @80060780, shared with the independent
 * range checker. The enclosing implementation is ae_playerupdate_frame.c. */
#include "../core/ae_ram.h"

#define PU_PLAYER 0x800EC1E0u
#define PU_LOCK 0x800F44A2u
#define PU_HEALTH 0x0E8u
#define PU_STATE 0x05Eu
#define PU_GADGET 0x0F2u
#define PU_VELOCITY 0x030u
#define PU_POSITION_Y 0x028u
#define PU_FRACTION 0x108u
#define PU_TARGET_VELOCITY 0x114u
#define PU_MOVE_VELOCITY 0x120u
#define PU_GROUND_TYPE 0x138u
#define PU_WATER_Y 0x13Cu
#define PU_GRAVITY 0x800EC014u
#define PU_AIM_ACTIVE 0x800E6902u
#define PU_CAMERA_ACTIVE 0x800E6908u
#define PU_AIM_MODE 0x800E6916u
#define PU_PAD_HELD 0x800E5478u
#define PU_PAD_CHANGED 0x800E5480u
#define PU_PAD_CLASS 0x800E5498u
#define PU_PAD_GESTURE 0x800E549Au
#define PU_PAD_UNK18 0x800E549Cu
#define PU_PAD_EVENTS 0x800E549Eu
#define PU_PAD_LEFT_MAG 0x800E54A0u
#define PU_PAD_RIGHT_MAG 0x800E54A2u
#define PU_PAD_UNK20 0x800E54C4u
#define PU_PAD_UNK24 0x800E54C8u
#define PU_PAD_RIGHT_CAM 0x800E54CCu
#define PU_PAD_LEFT_CAM 0x800E54D0u
#define PU_PAD_ROTATION 0x800E54D4u
#define PU_PAD_UNK32 0x800E54D6u
#define PU_PAD_TRAVEL_RIGHT 0x800E54DCu
#define PU_PAD_TRAVEL_LEFT 0x800E54DEu
#define PU_SCRATCH 0x1F800000u
#define PU_INPUT_STATE_TABLE 0x048u
#define PU_INPUT_TRANSITIONS 0x04Cu
#define PU_INPUT_EVENTS 0x050u
#define PU_INPUT_HELD 0x054u
#define PU_INPUT_CHANGED 0x058u
#define PU_INPUT_CLASS 0x05Cu
#define PU_INPUT_GESTURE 0x05Eu
#define PU_INPUT_UNK18 0x060u
#define PU_INPUT_GESTURE_EVENTS 0x062u
#define PU_INPUT_LEFT_MAG 0x064u
#define PU_INPUT_RIGHT_MAG 0x066u
#define PU_INPUT_UNK20 0x068u
#define PU_INPUT_UNK24 0x06Cu
#define PU_INPUT_RIGHT_CAM 0x070u
#define PU_INPUT_LEFT_CAM 0x074u
#define PU_INPUT_ROTATION 0x078u
#define PU_INPUT_UNK32 0x07Au
#define PU_INPUT_TRAVEL_RIGHT 0x07Cu
#define PU_INPUT_TRAVEL_LEFT 0x07Eu
#define PU_INPUT_LOCKED 0x080u
#define PU_STATE_SETS 0x800F4DE0u
#define PU_STATE_SET_SIZE 0x010u
#define PU_STATE_SET_STATES 0x004u
#define PU_STATE_SET_TRANSITIONS 0x008u
#define PU_STATE_SET_EVENTS 0x00Cu
#define PU_VECTOR_Y 4u
#define PU_VECTOR_Z 8u
#define PU_SVECTOR_Z 4u

/* 80060A30..80060C64: the two camera masks are applied in order. Death
 * disables left input, but still permits right-stick aim unless lock == 11. */
static inline void playerupdate_copy_input(AeRam *r)
{
    int aim = ae_rd8(r, PU_AIM_ACTIVE) == 1;
    int camera = ae_rd8(r, PU_CAMERA_ACTIVE) == 1;
    int aim_mode = ae_rd8(r, PU_AIM_MODE) == 1;
    int locked = ae_rd16(r, PU_LOCK) == 11;
    int alive = ae_rds16(r, PU_PLAYER + PU_HEALTH) != 0;
    if (aim || camera || aim_mode || locked || !alive) {
        if (locked || !alive) {
            ae_wr32(r, PU_SCRATCH + PU_INPUT_HELD, 0);
            ae_wr32(r, PU_SCRATCH + PU_INPUT_CHANGED, 0);
        } else {
            if (aim) {
                ae_wr32(r, PU_SCRATCH + PU_INPUT_HELD, ae_rd32(r, PU_PAD_HELD) & 0xFAu);
                ae_wr32(r, PU_SCRATCH + PU_INPUT_CHANGED, ae_rd32(r, PU_PAD_CHANGED));
            }
            if (camera || aim_mode) {
                ae_wr32(r, PU_SCRATCH + PU_INPUT_HELD, ae_rd32(r, PU_PAD_HELD) & 0x2F0u);
                ae_wr32(r, PU_SCRATCH + PU_INPUT_CHANGED, ae_rd32(r, PU_PAD_CHANGED));
            }
        }
        ae_wr16(r, PU_SCRATCH + PU_INPUT_CLASS, 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_LEFT_MAG, 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_GESTURE,
                aim_mode && !locked ? ae_rd16(r, PU_PAD_GESTURE) : 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_RIGHT_MAG,
                aim_mode && !locked ? ae_rd16(r, PU_PAD_RIGHT_MAG) : 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_RIGHT_CAM, 4096);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_LEFT_CAM, 4096);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_UNK18, 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_GESTURE_EVENTS, 0);
        ae_wr32(r, PU_SCRATCH + PU_INPUT_UNK20, 0);
        ae_wr32(r, PU_SCRATCH + PU_INPUT_UNK24, 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_RIGHT_CAM + 2u, 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_LEFT_CAM + 2u, 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_ROTATION, 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_UNK32, 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_TRAVEL_RIGHT, 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_TRAVEL_LEFT, 0);
        ae_wr32(r, PU_SCRATCH + PU_INPUT_LOCKED, 1);
    } else {
        ae_wr32(r, PU_SCRATCH + PU_INPUT_HELD, ae_rd32(r, PU_PAD_HELD));
        ae_wr32(r, PU_SCRATCH + PU_INPUT_CHANGED, ae_rd32(r, PU_PAD_CHANGED));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_CLASS, ae_rd16(r, PU_PAD_CLASS));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_GESTURE, ae_rd16(r, PU_PAD_GESTURE));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_UNK18, ae_rd16(r, PU_PAD_UNK18));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_GESTURE_EVENTS, ae_rd16(r, PU_PAD_EVENTS));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_LEFT_MAG, ae_rd16(r, PU_PAD_LEFT_MAG));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_RIGHT_MAG, ae_rd16(r, PU_PAD_RIGHT_MAG));
        ae_wr32(r, PU_SCRATCH + PU_INPUT_UNK20, ae_rd32(r, PU_PAD_UNK20));
        ae_wr32(r, PU_SCRATCH + PU_INPUT_UNK24, ae_rd32(r, PU_PAD_UNK24));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_RIGHT_CAM + 2u, ae_rd16(r, PU_PAD_RIGHT_CAM + 2u));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_RIGHT_CAM, ae_rd16(r, PU_PAD_RIGHT_CAM));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_LEFT_CAM + 2u, ae_rd16(r, PU_PAD_LEFT_CAM + 2u));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_LEFT_CAM, ae_rd16(r, PU_PAD_LEFT_CAM));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_ROTATION, ae_rd16(r, PU_PAD_ROTATION));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_UNK32, ae_rd16(r, PU_PAD_UNK32));
        ae_wr16(r, PU_SCRATCH + PU_INPUT_TRAVEL_RIGHT, ae_rd16(r, PU_PAD_TRAVEL_RIGHT));
        ae_wr32(r, PU_SCRATCH + PU_INPUT_LOCKED, 0);
        ae_wr16(r, PU_SCRATCH + PU_INPUT_TRAVEL_LEFT, ae_rd16(r, PU_PAD_TRAVEL_LEFT));
    }
}

/* 80060C68..80060D04, ending immediately before GetGroundHeightEx. The
 * current gadget is loaded with lb, not lbu, including the index addition. */
static inline void playerupdate_select_tables(AeRam *r)
{
    uint32_t gadget_set = PU_STATE_SETS +
        (uint32_t)(ae_rds8(r, PU_PLAYER + PU_GADGET) + 1) * PU_STATE_SET_SIZE;
    uint32_t states = ae_rd16(r, PU_PLAYER + PU_STATE) < 128 ?
        PU_STATE_SETS : gadget_set;
    ae_wr32(r, PU_SCRATCH + PU_INPUT_STATE_TABLE, ae_rd32(r, states + PU_STATE_SET_STATES));
    ae_wr32(r, PU_SCRATCH + PU_INPUT_TRANSITIONS, ae_rd32(r, gadget_set + PU_STATE_SET_TRANSITIONS));
    ae_wr32(r, PU_SCRATCH + PU_INPUT_EVENTS, ae_rd32(r, gadget_set + PU_STATE_SET_EVENTS));
}

/* 800614E8..80061690, ending before PlayerCheckGadgetSelect. This is a
 * switch inside PlayerUpdate, not an indirect function call or early return. */
static inline void playerupdate_prepare_vertical_move(AeRam *r)
{
    uint32_t state = ae_rd16(r, PU_PLAYER + PU_STATE);
    if (state == 0x49 || state == 0x4A) {
        for (uint32_t axis = 0; axis < 3; ++axis)
            ae_wr32(r, PU_PLAYER + PU_VELOCITY + axis * 4u, 0);
        ae_wr32(r, PU_PLAYER + PU_FRACTION + PU_VECTOR_Y, 0);
        ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY, 0);
        ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY + PU_VECTOR_Z, 0);
    }
    if (state == 0x46 || state == 0x47 || state == 0x49 || state == 0x4A || state == 0x4C) {
        uint32_t fluid = ae_rd16(r, PU_PLAYER + PU_GROUND_TYPE) & 15u;
        uint32_t fraction = ae_rd32(r, PU_PLAYER + PU_FRACTION + PU_VECTOR_Y);
        if (fluid == 0) {
            if (state != 0x47) fraction -= 2048u;
        } else if (fluid == 1) {
            ae_wr32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y,
                    (uint32_t)(ae_rds32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y) / 2));
            int32_t submerged_y = (int32_t)(ae_rd32(r, PU_PLAYER + PU_POSITION_Y) - 156u);
            if (submerged_y < ae_rds16(r, PU_PLAYER + PU_WATER_Y)) fraction += 8192u;
        } else if (fluid == 2) {
            fraction += 20480u;
        }
        ae_wr32(r, PU_PLAYER + PU_FRACTION + PU_VECTOR_Y, fraction);
        int32_t velocity = ae_rds32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y);
        if (velocity < -16) velocity = -16;
        if (velocity > 16) velocity = 16;
        ae_wr32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y, (uint32_t)velocity);
    } else if (state == 0x4D) {
        for (uint32_t axis = 0; axis < 3; ++axis)
            ae_wr32(r, PU_PLAYER + PU_VELOCITY + axis * 4u, 0);
    } else {
        ae_wr32(r, PU_PLAYER + PU_FRACTION + PU_VECTOR_Y,
                ae_rd32(r, PU_PLAYER + PU_FRACTION + PU_VECTOR_Y) +
                (ae_rd32(r, PU_GRAVITY) << 7));
    }
    uint32_t vertical = (ae_rd32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y) << 12) +
        ae_rd32(r, PU_PLAYER + PU_FRACTION + PU_VECTOR_Y);
    ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + PU_VECTOR_Z, 0);
    ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY, 0);
    ae_wr32(r, PU_PLAYER + PU_FRACTION + PU_VECTOR_Z, 0);
    ae_wr32(r, PU_PLAYER + PU_FRACTION + PU_VECTOR_Y, 0);
    ae_wr32(r, PU_PLAYER + PU_FRACTION, 0);
    ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY + PU_VECTOR_Y, vertical);
    ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + PU_VECTOR_Y, vertical);
}

#endif
