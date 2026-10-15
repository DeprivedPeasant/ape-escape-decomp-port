#include "ae_zakohelpers.h"

#include <stdint.h>

static int32_t zakohelpers_s32(uint32_t bits) {
    int64_t value = bits;
    if ((bits & 0x80000000u) != 0u) value -= 0x100000000LL;
    return (int32_t)value;
}

static int32_t zakohelpers_s16(uint32_t bits) {
    uint32_t value = bits & 0xFFFFu;
    return (value & 0x8000u) != 0u ? (int32_t)value - 0x10000 : (int32_t)value;
}

static int32_t zakohelpers_asr32(int32_t value, unsigned shift) {
    if (shift == 0u) return value;
    uint32_t shifted = (uint32_t)value >> shift;
    if (value < 0) shifted |= UINT32_MAX << (32u - shift);
    return zakohelpers_s32(shifted);
}

static int32_t zakohelpers_add32(int32_t left, int32_t right) {
    return zakohelpers_s32((uint32_t)left + (uint32_t)right);
}

static int32_t zakohelpers_sub32(int32_t left, int32_t right) {
    return zakohelpers_s32((uint32_t)left - (uint32_t)right);
}

static int32_t zakohelpers_mullo32(int32_t left, int32_t right) {
    return zakohelpers_s32((uint32_t)left * (uint32_t)right);
}

static int32_t zakohelpers_div1000(int32_t dividend) {
    int32_t magic = zakohelpers_s32(0xB60B60B7u);
    int64_t product = (int64_t)dividend * magic;
    int32_t high = zakohelpers_s32((uint32_t)((uint64_t)product >> 32));
    int32_t quotient = zakohelpers_asr32(zakohelpers_add32(high, dividend), 8u);
    return zakohelpers_sub32(quotient, dividend < 0 ? -1 : 0);
}

static uint32_t zakohelpers_anim_clip(AeRam *r, uint32_t obj) {
    uint32_t slot = ae_rd8(r, obj + AE_OBJ_ANIM_SLOT);
    uint32_t clips = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE);
    return clips + slot * AE_ANIM_CLIP_RECORD_SIZE;
}

static void zakohelpers_clear_stack_words(AeRam *r, const AeZako1Hooks *h) {
    if (h == NULL || h->FUN_800A3740 == NULL) {
        ae_ram_fail(r, "FUN_800A3740 BIOS clear hook not provided");
        return;
    }
    (void)h->FUN_800A3740(h->user, r, AE_ZAKO_HELPER_STACK_CLEAR,
                          0u, 8u, 0u);
}

static void zakohelpers_store_move_vector(AeRam *r, int32_t x, int32_t z) {
    ae_wr16(r, AE_ZAKO_HELPER_STACK_MOVE_VECTOR,
            (uint32_t)zakohelpers_s16((uint32_t)x));
    ae_wr16(r, AE_ZAKO_HELPER_STACK_MOVE_VECTOR + AE_ZAKO_HELPER_VECTOR_Y, 0u);
    ae_wr16(r, AE_ZAKO_HELPER_STACK_MOVE_VECTOR + AE_ZAKO_HELPER_VECTOR_Z,
            (uint32_t)zakohelpers_s16((uint32_t)z));
    ae_wr16(r, AE_ZAKO_HELPER_STACK_MOVE_VECTOR + AE_ZAKO_HELPER_VECTOR_PAD, 0u);
}

void ae_ZakoWalkToTarget(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    (void)ae_ObjWalkToPoint(r, h, obj,
                            ae_rds16(r, obj + AE_OBJ_TARGET_X),
                            ae_rds16(r, obj + AE_OBJ_TARGET_Z), 8);
    if (ae_ram_failed(r)) return;
    (void)ae_ObjTryStepOnGround(r, h, obj, 64);
    if (ae_ram_failed(r)) return;

    int32_t dx = zakohelpers_sub32(ae_rds16(r, obj + AE_OBJ_TARGET_X),
                                   ae_rds32(r, obj + AE_OBJ_POS_X));
    int32_t dz = zakohelpers_sub32(ae_rds16(r, obj + AE_OBJ_TARGET_Z),
                                   ae_rds32(r, obj + AE_OBJ_POS_Z));
    int32_t distance_squared = zakohelpers_add32(
        zakohelpers_mullo32(dx, dx), zakohelpers_mullo32(dz, dz));
    if (distance_squared < 1024) {
        ae_wr8(r, obj + AE_OBJ_AI_STATE, 1u);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, 0u);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Y, 0u);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_X, 0u);
    }
}

void ae_ZakoStateHurt(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    if (ae_ObjGetLinkedObjState(r, obj, 0) == 6) {
        ae_wr8(r, obj + AE_OBJ_AI_STATE, 6u);
        ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 0u);
        ae_wr32(r, obj + AE_OBJ_FLAGS,
                ae_rd32(r, obj + AE_OBJ_FLAGS) &
                    ~AE_ZAKO_HELPER_FLAG_UNK_16);
        uint32_t clip = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE);
        ae_wr16(r, obj + AE_OBJ_SPEED, 320u);
        ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                ae_rd16(r, clip + AE_ANIM_CLIP_FIRST_FRAME));
        ae_ObjPlaySfx(r, h, obj, 0x5F);
        if (ae_ram_failed(r)) return;

        int32_t cooldown = ae_rds8(r, AE_G_ZAKO_HELPER_COOLDOWN);
        if (cooldown == 0) cooldown = 50;
        (void)ae_ObjCooldown(r, obj, cooldown);
    }

    int32_t body_height = zakohelpers_asr32(
        zakohelpers_s32(ae_rd16(r, obj + AE_OBJ_BODY_HEIGHT) << 16), 17u);
    int32_t center_y = zakohelpers_s16(
        (uint32_t)ae_rd16(r, obj + AE_OBJ_POS_Y) - (uint32_t)body_height);
    uint32_t body = ae_ObjAddHitBodyIfPlayerNear(
        r, h, obj, 120,
        ae_rds16(r, obj + AE_OBJ_POS_X), center_y,
        ae_rds16(r, obj + AE_OBJ_POS_Z),
        ae_rds16(r, obj + AE_OBJ_BODY_PARAM),
        ae_rds16(r, obj + AE_OBJ_BODY_FLAGS));
    ae_wr32(r, obj + AE_OBJ_HIT_TARGET, body);
}

void ae_ZakoEnterIdleWait(AeRam *r, uint32_t obj) {
    uint32_t clip = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE);
    ae_wr8(r, obj + AE_OBJ_AI_STATE, 5u);
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 0u);
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
            ae_rd16(r, clip + AE_ANIM_CLIP_FIRST_FRAME));
    int32_t cooldown = ae_rds8(r, AE_G_ZAKO_HELPER_COOLDOWN);
    if (cooldown == 0) cooldown = 50;
    (void)ae_ObjCooldown(r, obj, cooldown);
}

void ae_ObjFootstepFx(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    uint32_t clip = zakohelpers_anim_clip(r, obj);
    int32_t frame_delta = (int32_t)ae_rd16(r, obj + AE_OBJ_ANIM_FRAME) -
                          (int32_t)ae_rd16(r, clip + AE_ANIM_CLIP_FIRST_FRAME);
    int32_t cadence = ae_rd8(r, obj + AE_OBJ_TYPE) == 3u ? 4 : 8;
    int32_t phase = cadence == 4 ? 0 : 3;
    if (frame_delta >= 13) return;
    if ((frame_delta % cadence) != phase) return;

    ae_ObjPlaySfx(r, h, obj, 0x45);
    if (ae_ram_failed(r)) return;
    ae_ObjSpawnParticleAt(r, h, obj, 4, 0, 0, 0);
}

void ae_ZakoChasePlayer(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    int32_t speed = zakohelpers_asr32(
        zakohelpers_s32(ae_rd16(r, obj + AE_OBJ_SPEED) << 16), 19u);
    ae_ObjChasePlayer(r, h, obj, speed);
    if (ae_ram_failed(r)) return;
    (void)ae_ObjTryStepOnGround(r, h, obj, 64);
    if (ae_ram_failed(r)) return;
    ae_ObjFootstepFx(r, h, obj);
    if (ae_ram_failed(r)) return;
    if (ae_ObjTargetBeyondRange(r, obj, 0) != 0)
        ae_wr8(r, obj + AE_OBJ_AI_STATE, 4u);

    int32_t target_speed = 40;
    if (ae_rd8(r, obj + AE_OBJ_TYPE) == 0x49u &&
        (ae_rd16(r, obj + AE_ZAKO_HELPER_ACTION_FLAGS) & 1u) != 0u) {
        target_speed = 20;
    }
    int32_t accelerate_step = ae_rds8(r, AE_G_ZAKO_HELPER_CHASE_ACCEL) + 10;
    int32_t decelerate_step = -21 -
        ae_rds8(r, AE_G_ZAKO_HELPER_ACCEL_STEP);
    if (ae_ObjAccelerate(r, obj, target_speed, accelerate_step,
                         decelerate_step) != 0) {
        ae_ZakoEnterIdleWait(r, obj);
    }
}

void ae_ZakoEnterDeath(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    uint32_t clips = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE);
    ae_wr8(r, obj + AE_OBJ_AI_STATE, 11u);
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 4u);
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
            ae_rd16(r, clips + 4u * AE_ANIM_CLIP_RECORD_SIZE +
                              AE_ANIM_CLIP_FIRST_FRAME));
    ae_ObjAimAtPlayer(r, obj);
    ae_ObjPlaySfx(r, h, obj, 0x68);
    if (ae_ram_failed(r)) return;
    ae_ObjSpawnParticleAt(r, h, obj, 4, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~2u);
    ae_wr16(r, obj + AE_OBJ_BODY_RADIUS, 45u);
    ae_wr16(r, obj + AE_OBJ_BODY_HEIGHT, 114u);
}

void ae_ObjTurnTowardsPlayerXZ(AeRam *r, const AeZako1Hooks *h,
                               uint32_t obj) {
    uint32_t player = AE_G_PLAYER_OBJECT;
    int32_t dx = zakohelpers_s16(ae_rd16(r, player + AE_OBJ_POS_X) -
                                 ae_rd16(r, obj + AE_OBJ_POS_X));
    int32_t dz = zakohelpers_s16(ae_rd16(r, player + AE_OBJ_POS_Z) -
                                 ae_rd16(r, obj + AE_OBJ_POS_Z));
    ae_ObjTurnTowardsXZ(r, h, obj, dx, dz, 0, 182);
    if (ae_ram_failed(r)) return;
    int32_t on_ground = ae_ObjFallToGround(r, h, obj);
    if (ae_ram_failed(r)) return;
    if (on_ground != 0) {
        ae_wr32(r, obj + AE_OBJ_FLAGS,
                ae_rd32(r, obj + AE_OBJ_FLAGS) | AE_ZAKO_HELPER_FLAG_UNK_16);
        ae_wr16(r, obj + AE_OBJ_SPEED, 0u);
        int32_t turn = ae_rds8(r, AE_G_ZAKO_HELPER_TURN_ADJUST);
        int32_t scale = turn + (ae_rd8(r, obj + AE_OBJ_TYPE) == 0x49u ? 9 : 3);
        int32_t angular_velocity = zakohelpers_div1000(
            zakohelpers_s32((uint32_t)scale << 12));
        ae_wr16(r, obj + AE_OBJ_UNK_B0, (uint32_t)angular_velocity);
        uint32_t clips = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE);
        ae_wr8(r, obj + AE_OBJ_AI_STATE, 3u);
        ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 1u);
        ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                ae_rd16(r, clips + AE_ANIM_CLIP_RECORD_SIZE +
                                  AE_ANIM_CLIP_FIRST_FRAME));
    }
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Y,
            ae_rd16(r, obj + AE_OBJ_VELOCITY_Y) + 10u);
}

void ae_ObjSetSpeed96(AeRam *r, uint32_t obj, int32_t speed) {
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Y, (uint32_t)speed);
}

void ae_ZakoStateWatch(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    int32_t in_target_range = ae_PlayerWithinRadiusXZ(
        r, ae_rds16(r, obj + AE_OBJ_TARGET_X),
        ae_rds16(r, obj + AE_OBJ_TARGET_Z),
        ae_rds16(r, obj + AE_OBJ_RADIUS));
    if (in_target_range == 0) return;

    int32_t global_adjust = ae_rds8(r, AE_G_ZAKO_HELPER_WATCH_RADIUS);
    int32_t radius = 625 + global_adjust * 99;
    int32_t outside_object_range = ae_ObjPlayerNearPointXZ(
        r, obj, ae_rds32(r, obj + AE_OBJ_POS_X),
        ae_rds32(r, obj + AE_OBJ_POS_Y),
        ae_rds32(r, obj + AE_OBJ_POS_Z), radius);
    if (outside_object_range != 0) return;

    ae_wr8(r, obj + AE_OBJ_AI_STATE, 2u);
    ae_ObjSetVelTowardPlayer(r, h, obj, 1);
    if (ae_ram_failed(r)) return;
    ae_ObjPlaySfx(r, h, obj, 0x61);
    if (ae_ram_failed(r)) return;
    ae_ObjSetSpeed96(r, obj, -42);
}

void ae_Zako_WanderBounce(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    (void)ae_ObjWanderOrReturn(r, h, obj, 8, 0);
    if (ae_ram_failed(r)) return;
    if (ae_ObjTryStepOnGround(r, h, obj, 64) != 0 || ae_ram_failed(r)) return;

    int32_t velocity_x = -ae_rds16(r, obj + AE_OBJ_VELOCITY_X);
    int32_t velocity_z = -ae_rds16(r, obj + AE_OBJ_VELOCITY_Z);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_X, (uint32_t)velocity_x);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, (uint32_t)velocity_z);
    ae_ObjTurnTowardsXZ(r, h, obj,
                        ae_rds16(r, obj + AE_OBJ_VELOCITY_X),
                        ae_rds16(r, obj + AE_OBJ_VELOCITY_Z),
                        0, ae_rds16(r, obj + AE_OBJ_UNK_B0));
}

void ae_Zako_InitDefaultStats(AeRam *r, uint32_t obj) {
    ae_wr16(r, obj + AE_OBJ_SHADOW_SIZE, 50u);
    ae_wr16(r, obj + AE_OBJ_SPEED, 8u);
    ae_wr16(r, obj + AE_OBJ_UNK_B0, 91u);
    ae_wr16(r, obj + AE_OBJ_BODY_PARAM, 1u);
    ae_wr16(r, obj + AE_OBJ_BODY_FLAGS, 0xC103u);
    ae_wr16(r, obj + AE_OBJ_BODY_RADIUS, 90u);
    ae_wr16(r, obj + AE_OBJ_BODY_HEIGHT, 228u);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) | AE_ZAKO_HELPER_FLAG_UNK_16);
    ae_wr8(r, obj + AE_OBJ_REACT_ANIM, 2u);

    uint32_t type = ae_rd8(r, obj + AE_OBJ_TYPE);
    if (type == 14u) {
        ae_wr16(r, obj + AE_OBJ_BODY_PARAM, 0u);
        ae_wr16(r, obj + AE_OBJ_BODY_FLAGS, 0xC101u);
    } else if (type == 3u) {
        ae_wr8(r, obj + AE_OBJ_DROP_TABLE, 0u);
    } else if (type == 0x49u) {
        ae_wr8(r, obj + AE_OBJ_SCALE, 5u);
        ae_wr8(r, obj + AE_OBJ_HP, 5u);
        ae_wr8(r, obj + AE_OBJ_DROP_TABLE, 2u);
    }

    uint32_t clips = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE);
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 0u);
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
            ae_rd16(r, clips + AE_ANIM_CLIP_FIRST_FRAME));
}

void ae_Zako_RecoverFromStun(AeRam *r, uint32_t obj) {
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    if ((flags & AE_OBJ_FLAG_UNK_20) == 0u) return;

    ae_wr32(r, obj + AE_OBJ_FLAGS, flags & ~AE_OBJ_FLAG_UNK_20);
    ae_wr16(r, obj + AE_OBJ_BODY_RADIUS, 90u);
    ae_wr16(r, obj + AE_OBJ_BODY_HEIGHT, 228u);
    if (ae_ObjPlayerInConeFromPos2(r, obj, 90, 2896, 1536) != 0 &&
        ae_rd8(r, obj + AE_OBJ_TYPE) == 0x49u) {
        ae_wr8(r, obj + AE_OBJ_AI_STATE, 10u);
        ae_wr8(r, obj + AE_OBJ_SUB_STATE, 0u);
        ae_ObjSetSpeed96(r, obj, -42);
    } else {
        ae_ZakoEnterIdleWait(r, obj);
        ae_wr16(r, obj + AE_OBJ_UNK_B0, 91u);
    }
}

void ae_ZakoReturnIfFar(AeRam *r, uint32_t obj) {
    if (ae_ObjTargetBeyondRange(r, obj, 0) == 0) return;
    uint32_t state = ae_rd8(r, obj + AE_OBJ_AI_STATE);
    if (state == 5u || state == 10u || state == 11u || state == 12u) return;
    ae_ZakoEnterIdleWait(r, obj);
    ae_wr16(r, obj + AE_OBJ_UNK_B0, 91u);
}

int32_t ae_ObjTargetBeyondRange(AeRam *r, uint32_t obj,
                                int32_t extra_range) {
    int32_t velocity_x = ae_rds16(r, obj + AE_OBJ_VELOCITY_X);
    int32_t velocity_z = ae_rds16(r, obj + AE_OBJ_VELOCITY_Z);
    int32_t target_x = ae_rds16(r, obj + AE_OBJ_TARGET_X);
    int32_t target_z = ae_rds16(r, obj + AE_OBJ_TARGET_Z);
    int32_t predicted_x = zakohelpers_add32(
        ae_rds32(r, obj + AE_OBJ_POS_X), velocity_x);
    int32_t predicted_z = zakohelpers_add32(
        ae_rds32(r, obj + AE_OBJ_POS_Z), velocity_z);
    int32_t dx = zakohelpers_sub32(target_x, predicted_x);
    int32_t dz = zakohelpers_sub32(target_z, predicted_z);
    int32_t extra = zakohelpers_s16((uint32_t)extra_range);
    int32_t range = zakohelpers_add32(ae_rds16(r, obj + AE_OBJ_RADIUS), extra);
    int32_t distance_squared = zakohelpers_add32(
        zakohelpers_mullo32(dx, dx), zakohelpers_mullo32(dz, dz));
    int32_t range_squared = zakohelpers_mullo32(range, range);
    return distance_squared > range_squared;
}

void ae_ObjWalkOnGround(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    int32_t ground_y = ae_ObjGroundHeightCached(
        r, h, obj, ae_rds16(r, obj + AE_OBJ_VELOCITY_X), 0,
        ae_rds16(r, obj + AE_OBJ_VELOCITY_Z));
    if (ae_ram_failed(r)) return;
    ground_y = zakohelpers_s16((uint32_t)ground_y);
    int32_t old_y = ae_rds32(r, obj + AE_OBJ_POS_Y);
    if (ground_y >= zakohelpers_sub32(old_y, 64)) {
        ae_wr32(r, obj + AE_OBJ_POS_X,
                ae_rd32(r, obj + AE_OBJ_POS_X) +
                    (uint32_t)ae_rds16(r, obj + AE_OBJ_VELOCITY_X));
        ae_wr32(r, obj + AE_OBJ_POS_Z,
                ae_rd32(r, obj + AE_OBJ_POS_Z) +
                    (uint32_t)ae_rds16(r, obj + AE_OBJ_VELOCITY_Z));
    } else {
        ae_wr32(r, obj + AE_OBJ_FLAGS,
                ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_ZAKO1_OBJ_FLAG_UNK_8);
    }
    ae_wr32(r, obj + AE_OBJ_POS_Y,
            (uint32_t)zakohelpers_s16((uint32_t)ground_y));
}

void ae_ObjSetVelAlongDir(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                          uint32_t direction, int32_t speed,
                          int32_t turn_speed) {
    (void)ae_sdk_VectorNormalSS(r, direction, direction);
    if (ae_ram_failed(r)) return;
    int32_t x = ae_rds16(r, direction + AE_ZAKO_HELPER_VECTOR_X);
    int32_t y = ae_rds16(r, direction + AE_ZAKO_HELPER_VECTOR_Y);
    int32_t z = ae_rds16(r, direction + AE_ZAKO_HELPER_VECTOR_Z);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_X,
            (uint32_t)zakohelpers_asr32(zakohelpers_mullo32(x, speed), 12u));
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Y,
            (uint32_t)zakohelpers_asr32(zakohelpers_mullo32(y, speed), 12u));
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Z,
            (uint32_t)zakohelpers_asr32(zakohelpers_mullo32(z, speed), 12u));
    ae_ObjTurnTowardsXZ(r, h, obj,
                        ae_rds16(r, direction + AE_ZAKO_HELPER_VECTOR_X),
                        ae_rds16(r, direction + AE_ZAKO_HELPER_VECTOR_Z),
                        zakohelpers_s16((uint32_t)turn_speed),
                        ae_rds16(r, obj + AE_OBJ_UNK_B0));
}

void ae_ObjSetVelTowardPlayer(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                              int32_t speed) {
    zakohelpers_clear_stack_words(r, h);
    if (ae_ram_failed(r)) return;
    uint32_t player = AE_G_PLAYER_OBJECT;
    int32_t dx = zakohelpers_sub32(ae_rd16(r, player + AE_OBJ_POS_X),
                                   ae_rd16(r, obj + AE_OBJ_POS_X));
    int32_t dz = zakohelpers_sub32(ae_rd16(r, player + AE_OBJ_POS_Z),
                                   ae_rd16(r, obj + AE_OBJ_POS_Z));
    zakohelpers_store_move_vector(r, dx, dz);
    ae_ObjSetVelAlongDir(r, h, obj, AE_ZAKO_HELPER_STACK_MOVE_VECTOR,
                         speed, 0);
}

void ae_ObjSetVelTowardTarget(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                              int32_t speed, int32_t turn_speed) {
    zakohelpers_clear_stack_words(r, h);
    if (ae_ram_failed(r)) return;
    int32_t dx = zakohelpers_sub32(ae_rd16(r, obj + AE_OBJ_TARGET_X),
                                   ae_rd16(r, obj + AE_OBJ_POS_X));
    int32_t dz = zakohelpers_sub32(ae_rd16(r, obj + AE_OBJ_TARGET_Z),
                                   ae_rd16(r, obj + AE_OBJ_POS_Z));
    zakohelpers_store_move_vector(r, dx, dz);
    ae_ObjSetVelAlongDir(r, h, obj, AE_ZAKO_HELPER_STACK_MOVE_VECTOR,
                         speed, turn_speed);
}

int32_t ae_ObjCooldown(AeRam *r, uint32_t obj, int32_t cooldown) {
    if (cooldown >= 0) {
        ae_wr8(r, obj + AE_OBJ_COOLDOWN, (uint32_t)cooldown);
        return 0;
    }
    uint32_t current = ae_rd8(r, obj + AE_OBJ_COOLDOWN);
    if (current == 0u) return 0;
    ae_wr8(r, obj + AE_OBJ_COOLDOWN, current - 1u);
    return 1;
}

int32_t ae_ObjWalkToPoint(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                          int32_t target_x, int32_t target_z,
                          int32_t arrival_radius) {
    int32_t full_dx = zakohelpers_sub32(target_x,
                                        zakohelpers_s32(ae_rd32(r, obj + AE_OBJ_POS_X)));
    int32_t full_dz = zakohelpers_sub32(target_z,
                                        zakohelpers_s32(ae_rd32(r, obj + AE_OBJ_POS_Z)));
    int32_t dx = zakohelpers_s16((uint32_t)full_dx);
    int32_t dz = zakohelpers_s16((uint32_t)full_dz);
    ae_ObjTurnTowardsXZ(r, h, obj, dx, dz,
                        0, ae_rds16(r, obj + AE_OBJ_UNK_B0));
    if (ae_ram_failed(r)) return 0;

    int32_t delta_squared = zakohelpers_add32(
        zakohelpers_mullo32(dx, dx), zakohelpers_mullo32(dz, dz));
    int32_t radius_squared = zakohelpers_mullo32(arrival_radius,
                                                  arrival_radius);
    if (delta_squared < radius_squared) {
        ae_wr16(r, obj + AE_OBJ_VELOCITY_X, (uint32_t)full_dx);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, (uint32_t)full_dz);
        if (dx == 0 && dz == 0) return 1;
        if (ae_rds16(r, obj + AE_OBJ_TIMER) < 3)
            ae_wr16(r, obj + AE_OBJ_TIMER, 0u);
        return 1;
    }

    ae_ObjSetVelFromFacing(r, obj,
                           zakohelpers_s16((uint32_t)arrival_radius));
    return 0;
}

void ae_ObjChasePlayer(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                       int32_t speed) {
    uint32_t player = AE_G_PLAYER_OBJECT;
    int32_t dx = zakohelpers_s16(ae_rd16(r, player + AE_OBJ_POS_X) -
                                 ae_rd16(r, obj + AE_OBJ_POS_X));
    int32_t dz = zakohelpers_s16(ae_rd16(r, player + AE_OBJ_POS_Z) -
                                 ae_rd16(r, obj + AE_OBJ_POS_Z));
    ae_ObjTurnTowardsXZ(r, h, obj, dx, dz,
                        0, ae_rds16(r, obj + AE_OBJ_UNK_B0));
    ae_ObjSetVelFromFacing(r, obj, zakohelpers_s16((uint32_t)speed));
}

void ae_ObjWanderStep(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                      int32_t speed, int32_t turn_speed) {
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Y, 0u);
    uint32_t first_seed = (ae_rd16(r, AE_G_ZAKO_HELPER_WANDER_SEED_2) * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_G_ZAKO_HELPER_WANDER_SEED_2, first_seed);
    int32_t velocity_x = (int32_t)(((first_seed >> 8) * 3u) >> 5) - 12;
    ae_wr16(r, obj + AE_OBJ_VELOCITY_X, (uint32_t)velocity_x);

    uint32_t second_seed = (first_seed * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_G_ZAKO_HELPER_WANDER_SEED_2, second_seed);
    int32_t velocity_z = (int32_t)(((second_seed >> 8) * 3u) >> 5) - 12;
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, (uint32_t)velocity_z);

    ae_ObjSetVelAlongDir(r, h, obj, obj + AE_OBJ_VELOCITY_X,
                         speed, turn_speed);
}

int32_t ae_ObjWanderOrReturn(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                             int32_t speed, int32_t turn_speed) {
    if (ae_ObjTargetBeyondRange(r, obj, 0) != 0) {
        ae_ObjSetVelTowardTarget(r, h, obj, speed, turn_speed);
        return 1;
    }

    uint32_t seed = (ae_rd16(r, AE_G_ZAKO_HELPER_WANDER_SEED) * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_G_ZAKO_HELPER_WANDER_SEED, seed);
    if (((seed >> 8) & 0x1Fu) != 0u) return 0;
    ae_ObjWanderStep(r, h, obj, speed, turn_speed);
    return 1;
}

void ae_ObjFleeFromPlayerInArea(AeRam *r, const AeZako1Hooks *h,
                                uint32_t obj, int32_t speed) {
    int32_t to_target_x = zakohelpers_s16(
        ae_rd16(r, obj + AE_OBJ_POS_X) - ae_rd16(r, obj + AE_OBJ_TARGET_X));
    int32_t to_target_z = zakohelpers_s16(
        ae_rd16(r, obj + AE_OBJ_POS_Z) - ae_rd16(r, obj + AE_OBJ_TARGET_Z));
    int32_t radius = ae_rds16(r, obj + AE_OBJ_RADIUS);
    int32_t target_distance = zakohelpers_add32(
        zakohelpers_mullo32(to_target_x, to_target_x),
        zakohelpers_mullo32(to_target_z, to_target_z));
    int32_t radius_squared = zakohelpers_mullo32(radius, radius);
    if (target_distance < radius_squared) {
        uint32_t player = AE_G_PLAYER_OBJECT;
        int32_t away_x = zakohelpers_s16(
            ae_rd16(r, obj + AE_OBJ_POS_X) -
            ae_rd16(r, player + AE_OBJ_POS_X));
        int32_t away_z = zakohelpers_s16(
            ae_rd16(r, obj + AE_OBJ_POS_Z) -
            ae_rd16(r, player + AE_OBJ_POS_Z));
        zakohelpers_store_move_vector(r, away_x, away_z);
        ae_ObjSetVelAlongDir(r, h, obj, AE_ZAKO_HELPER_STACK_MOVE_VECTOR,
                             zakohelpers_s16((uint32_t)speed), 0);
    } else {
        ae_wr16(r, obj + AE_OBJ_VELOCITY_X, 0u);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, 0u);
    }
}

void ae_ObjSetVelFromFacing(AeRam *r, uint32_t obj, int32_t speed) {
    int32_t signed_speed = zakohelpers_s16((uint32_t)speed);
    int32_t velocity_x = zakohelpers_asr32(
        zakohelpers_mullo32(ae_rds16(r, obj + AE_OBJ_FACING_X), signed_speed),
        12u);
    int32_t velocity_z = zakohelpers_asr32(
        zakohelpers_mullo32(ae_rds16(r, obj + AE_OBJ_FACING_Z), signed_speed),
        12u);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_X, (uint32_t)velocity_x);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, (uint32_t)velocity_z);
}

int32_t ae_ObjTryStepOnGround(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                              int32_t step_down) {
    int32_t ground_y = ae_ObjGroundHeightCached(
        r, h, obj, ae_rds16(r, obj + AE_OBJ_VELOCITY_X), 0,
        ae_rds16(r, obj + AE_OBJ_VELOCITY_Z));
    if (ae_ram_failed(r)) return 0;
    ground_y = zakohelpers_s16((uint32_t)ground_y);
    int32_t allowed_drop = zakohelpers_s16((uint32_t)step_down);
    if (ground_y < zakohelpers_sub32(ae_rds32(r, obj + AE_OBJ_POS_Y),
                                    allowed_drop)) {
        ae_wr32(r, obj + AE_OBJ_FLAGS,
                ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_ZAKO1_OBJ_FLAG_UNK_8);
        return 0;
    }

    ae_wr32(r, obj + AE_OBJ_POS_Y,
            zakohelpers_s32((uint32_t)ground_y));
    ae_wr32(r, obj + AE_OBJ_POS_X,
            ae_rd32(r, obj + AE_OBJ_POS_X) +
                (uint32_t)ae_rds16(r, obj + AE_OBJ_VELOCITY_X));
    ae_wr32(r, obj + AE_OBJ_POS_Z,
            ae_rd32(r, obj + AE_OBJ_POS_Z) +
                (uint32_t)ae_rds16(r, obj + AE_OBJ_VELOCITY_Z));
    return 1;
}

int32_t ae_ObjFallToGround(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    int32_t ground_y = ae_ObjGroundHeightCached(r, h, obj, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    ground_y = zakohelpers_s16((uint32_t)ground_y);
    int32_t next_y = zakohelpers_add32(ae_rds32(r, obj + AE_OBJ_POS_Y),
                                       ae_rds16(r, obj + AE_OBJ_VELOCITY_Y));
    if (next_y < ground_y) {
        ae_wr32(r, obj + AE_OBJ_POS_Y, (uint32_t)next_y);
        return 0;
    }
    ae_wr32(r, obj + AE_OBJ_POS_Y, (uint32_t)ground_y);
    return 1;
}

int32_t ae_ObjPlayerOutOfYRange(AeRam *r, uint32_t obj, int32_t y) {
    if ((ae_rd32(r, obj + AE_OBJ_FLAGS) &
         AE_ZAKO_HELPER_FLAG_NO_Y_CHECK) != 0u) return 1;
    int32_t player_y = ae_rds32(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_Y);
    int32_t difference = zakohelpers_sub32(player_y, y);
    if (difference >= 0) return difference >= 1537;
    return zakohelpers_sub32(y, player_y) >= 1537;
}

int32_t ae_ObjPlayerNearPointXZ(AeRam *r, uint32_t obj, int32_t x,
                                int32_t y, int32_t z, int32_t radius) {
    if (ae_ObjPlayerOutOfYRange(r, obj, y) != 0) return 1;
    int32_t narrowed_radius = zakohelpers_s16((uint32_t)radius);
    if (narrowed_radius == 0) return 0;
    return ae_PlayerWithinRadiusXZ(r, x, z, narrowed_radius) == 0;
}

int32_t ae_ObjPlayerInCone(AeRam *r, uint32_t obj, int32_t cone_angle,
                           int32_t dot_threshold, int32_t offset_x,
                           int32_t offset_z) {
    int32_t angle_numerator = zakohelpers_s32((uint32_t)cone_angle << 12);
    int32_t fixed_angle = zakohelpers_div1000(angle_numerator);
    ae_wr16(r, obj + AE_ZAKO_HELPER_CONE_ANGLE, (uint32_t)fixed_angle);

    ae_wr16(r, AE_ZAKO_HELPER_STACK_CONE_VECTOR,
            (uint32_t)zakohelpers_s16((uint32_t)offset_x));
    ae_wr16(r, AE_ZAKO_HELPER_STACK_CONE_VECTOR + AE_ZAKO_HELPER_VECTOR_Y, 0u);
    ae_wr16(r, AE_ZAKO_HELPER_STACK_CONE_VECTOR + AE_ZAKO_HELPER_VECTOR_Z,
            (uint32_t)zakohelpers_s16((uint32_t)offset_z));
    (void)ae_sdk_VectorNormalSS(r, AE_ZAKO_HELPER_STACK_CONE_VECTOR,
                                AE_ZAKO_HELPER_STACK_CONE_VECTOR);
    if (ae_ram_failed(r)) return 0;

    int32_t x_dot = zakohelpers_mullo32(
        ae_rds16(r, AE_ZAKO_HELPER_STACK_CONE_VECTOR),
        ae_rds16(r, obj + AE_OBJ_FACING_X));
    int32_t z_dot = zakohelpers_mullo32(
        ae_rds16(r, AE_ZAKO_HELPER_STACK_CONE_VECTOR + AE_ZAKO_HELPER_VECTOR_Z),
        ae_rds16(r, obj + AE_OBJ_FACING_Z));
    int32_t dot = zakohelpers_add32(x_dot, z_dot);
    if (dot < 0) dot = zakohelpers_add32(dot, 4095);
    dot = zakohelpers_asr32(dot, 12u);
    return dot_threshold < dot;
}

int32_t ae_ObjPlayerInConeFromPos2(AeRam *r, uint32_t obj,
                                   int32_t cone_angle,
                                   int32_t dot_threshold,
                                   int32_t vertical_radius) {
    if (ae_ObjPlayerNearPointXZ(r, obj,
            ae_rds32(r, obj + AE_OBJ_POS_X),
            ae_rds32(r, obj + AE_OBJ_POS_Y),
            ae_rds32(r, obj + AE_OBJ_POS_Z), vertical_radius) != 0) {
        return 0;
    }

    int32_t dx = zakohelpers_s16(
        ae_rd16(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_X) -
        ae_rd16(r, obj + AE_OBJ_POS_X));
    int32_t dz = zakohelpers_s16(
        ae_rd16(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_Z) -
        ae_rd16(r, obj + AE_OBJ_POS_Z));
    return ae_ObjPlayerInCone(r, obj, cone_angle, dot_threshold, dx, dz);
}

void ae_ObjTurnToPlayer(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    int32_t dx = zakohelpers_s16(
        ae_rd16(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_X) -
        ae_rd16(r, obj + AE_OBJ_POS_X));
    int32_t dz = zakohelpers_s16(
        ae_rd16(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_Z) -
        ae_rd16(r, obj + AE_OBJ_POS_Z));
    ae_ObjTurnTowardsXZ(r, h, obj, dx, dz, 0,
                        ae_rds16(r, obj + AE_OBJ_UNK_B0));
}

int32_t ae_ObjAccelerate(AeRam *r, uint32_t obj, int32_t target_speed,
                         int32_t accelerate_step,
                         int32_t decelerate_step) {
    uint32_t action_flags = ae_rd16(r, obj + AE_ZAKO_HELPER_ACTION_FLAGS);
    int32_t speed = ae_rd16(r, obj + AE_OBJ_SPEED);
    if ((action_flags & 1u) == 0u) {
        speed = zakohelpers_add32(speed, decelerate_step);
        ae_wr16(r, obj + AE_OBJ_SPEED, (uint32_t)speed);
        if (zakohelpers_s16((uint32_t)speed) <= 0) {
            ae_wr32(r, obj + AE_OBJ_FLAGS,
                    ae_rd32(r, obj + AE_OBJ_FLAGS) |
                        AE_ZAKO_HELPER_FLAG_UNK_16);
            return 1;
        }
        return 0;
    }

    speed = zakohelpers_add32(speed, accelerate_step);
    ae_wr16(r, obj + AE_OBJ_SPEED, (uint32_t)speed);
    int32_t signed_speed = zakohelpers_s16((uint32_t)speed);
    int32_t target = zakohelpers_s32((uint32_t)target_speed << 3);
    if (target < signed_speed) {
        ae_wr16(r, obj + AE_OBJ_SPEED,
                (uint32_t)zakohelpers_s32((uint32_t)target_speed << 3));
        ae_wr32(r, obj + AE_OBJ_FLAGS,
                ae_rd32(r, obj + AE_OBJ_FLAGS) &
                    ~AE_ZAKO_HELPER_FLAG_UNK_16);
    }
    return 0;
}

void ae_ObjClampFrame(AeRam *r, uint32_t obj, int32_t frame_limit) {
    if ((int32_t)ae_rd16(r, obj + AE_OBJ_ANIM_FRAME) >= frame_limit)
        ae_wr16(r, obj + AE_OBJ_ANIM_FRAME, (uint32_t)frame_limit - 1u);
}

int32_t ae_PlayerWithinRadiusXZ(AeRam *r, int32_t x, int32_t z,
                                int32_t radius) {
    int32_t dx = zakohelpers_sub32(
        ae_rds32(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_X), x);
    int32_t dz = zakohelpers_sub32(
        ae_rds32(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_Z), z);
    int32_t distance_squared = zakohelpers_add32(
        zakohelpers_mullo32(dx, dx), zakohelpers_mullo32(dz, dz));
    return distance_squared < zakohelpers_mullo32(radius, radius);
}

void ae_ObjAimAtPlayer(AeRam *r, uint32_t obj) {
    int32_t dx = zakohelpers_sub32(
        ae_rds32(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_X),
        ae_rds32(r, obj + AE_OBJ_POS_X));
    int32_t dz = zakohelpers_sub32(
        ae_rds32(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_Z),
        ae_rds32(r, obj + AE_OBJ_POS_Z));
    /* ae_Atan2Safe is the exact zero-Z guard around the already-ported SDK
     * ratan2 routine. Keep that wrapper's behavior here without linking the
     * camera update translation unit into the gameplay checker. */
    int32_t angle = dz == 0 ? (dx < 0 ? 0x800 : 0) :
                    ae_sdk_ratan2(r, dz, dx);
    int32_t shifted = zakohelpers_add32(angle, 4096);
    int32_t rounded = shifted < 0 ? zakohelpers_add32(angle, 8191) : shifted;
    int32_t multiple = zakohelpers_s32((uint32_t)zakohelpers_asr32(rounded, 12u) << 12);
    int32_t relative_angle = zakohelpers_sub32(shifted, multiple);
    ae_wr16(r, obj + AE_OBJ_HEADING, (uint32_t)relative_angle);

    int32_t facing_x = ae_sdk_rcos(r, ae_rds16(r, obj + AE_OBJ_HEADING));
    int32_t facing_z = ae_sdk_rsin(r, ae_rds16(r, obj + AE_OBJ_HEADING));
    ae_wr16(r, obj + AE_OBJ_FACING_X, (uint32_t)facing_x);
    ae_wr16(r, obj + AE_OBJ_FACING_Z, (uint32_t)facing_z);

    int32_t vertical_sine = ae_sdk_rsin(r, 568);
    int32_t vertical_product = zakohelpers_mullo32(vertical_sine, 50);
    int32_t velocity_y = -zakohelpers_asr32(
        zakohelpers_s32((uint32_t)vertical_product << 1), 12u);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Y, (uint32_t)velocity_y);

    int32_t forward_cosine = ae_sdk_rcos(r, 568);
    int32_t forward_product = zakohelpers_mullo32(forward_cosine, 50);
    int32_t forward_speed = zakohelpers_asr32(
        zakohelpers_s32((uint32_t)forward_product << 1), 12u);
    int32_t velocity_x = zakohelpers_asr32(
        zakohelpers_mullo32(ae_rds16(r, obj + AE_OBJ_FACING_X),
                            forward_speed), 12u);
    int32_t velocity_z = zakohelpers_asr32(
        zakohelpers_mullo32(ae_rds16(r, obj + AE_OBJ_FACING_Z),
                            forward_speed), 12u);
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    flags &= ~AE_ZAKO_HELPER_FLAG_UNK_5;
    flags |= AE_ZAKO_HELPER_FLAG_UNK_10;
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_X, (uint32_t)velocity_x);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, (uint32_t)velocity_z);
}

int32_t ae_ObjFallAndBounce(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Y,
            ae_rd16(r, obj + AE_OBJ_VELOCITY_Y) + 7u);
    int32_t ground_y = ae_ObjApplyVelocity(r, h, obj);
    if (ae_ram_failed(r)) return 0;
    int32_t bounced = ae_ObjBounceOnGround(
        r, obj, zakohelpers_s16((uint32_t)ground_y));
    if ((ae_rd32(r, obj + AE_OBJ_FLAGS) & AE_ZAKO_HELPER_FLAG_UNK_5) == 0u)
        return bounced;

    ae_wr32(r, obj + AE_OBJ_POS_X,
            ae_rd32(r, obj + AE_OBJ_POS_X) +
                (uint32_t)ae_rds16(r, obj + AE_OBJ_VELOCITY_X));
    ae_wr32(r, obj + AE_OBJ_POS_Z,
            ae_rd32(r, obj + AE_OBJ_POS_Z) +
                (uint32_t)ae_rds16(r, obj + AE_OBJ_VELOCITY_Z));

    int32_t angle = zakohelpers_add32(ae_rds16(r, obj + AE_OBJ_HEADING), 2048);
    int32_t adjusted = angle < 0 ? zakohelpers_add32(angle, 4095) : angle;
    int32_t multiple = zakohelpers_s32((uint32_t)zakohelpers_asr32(adjusted, 12u) << 12);
    angle = zakohelpers_sub32(angle, multiple);
    ae_wr16(r, obj + AE_OBJ_HEADING, (uint32_t)angle);
    ae_wr16(r, obj + AE_OBJ_FACING_X,
            (uint32_t)ae_sdk_rcos(r, ae_rds16(r, obj + AE_OBJ_HEADING)));
    ae_wr16(r, obj + AE_OBJ_FACING_Z,
            (uint32_t)ae_sdk_rsin(r, ae_rds16(r, obj + AE_OBJ_HEADING)));
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_ZAKO_HELPER_FLAG_UNK_5);
    return 0;
}

void ae_ObjSpawnParticleAt(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                           int32_t type, int32_t offset_x,
                           int32_t offset_y, int32_t offset_z) {
    uint32_t position = AE_ZAKO_HELPER_STACK_PARTICLE_POS;
    ae_wr16(r, position,
            ae_rd16(r, obj + AE_OBJ_POS_X) + (uint32_t)offset_x);
    ae_wr16(r, position + AE_ZAKO_HELPER_VECTOR_Y,
            ae_rd16(r, obj + AE_OBJ_POS_Y) - (uint32_t)offset_y);
    ae_wr16(r, position + AE_ZAKO_HELPER_VECTOR_Z,
            ae_rd16(r, obj + AE_OBJ_POS_Z) + (uint32_t)offset_z);
    if (h == NULL || h->spawn_hooks == NULL ||
        h->spawn_hooks->position_padding == NULL) {
        ae_ram_fail(r, "AeSpawnHooks.position_padding not provided for ObjSpawnParticleAt");
        return;
    }
    ae_wr16(r, position + AE_ZAKO_HELPER_VECTOR_PAD,
            h->spawn_hooks->position_padding(h->spawn_hooks->user, r,
                                             AE_ZAKO_HELPER_PARTICLE_FUNCTION));
    ae_SpawnParticle(r, h != NULL ? h->spawn_hooks : NULL,
                     zakohelpers_s16((uint32_t)type), position);
}

void ae_Zako_Nop(AeRam *r, uint32_t obj, int32_t radius) {
    (void)r;
    (void)obj;
    (void)radius;
}

uint32_t ae_ObjAddHitBody(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                          int32_t kind, int32_t x, int32_t y, int32_t z,
                          int32_t damage, int32_t radius) {
    uint32_t body = ae_CollisionBodyAlloc(r);
    if (body == 0u) return 0u;

    for (uint32_t i = 0; i < 8u; ++i)
        ae_wr8(r, body + AE_ZAKO_HELPER_HIT_BODY_DIRECTION + i,
               ae_rd8(r, obj + AE_OBJ_FACING_X + i));
    ae_wr32(r, body + AE_ZAKO_HELPER_HIT_BODY_POSITION_X,
            (uint32_t)zakohelpers_s16((uint32_t)x));
    ae_wr32(r, body + AE_ZAKO_HELPER_HIT_BODY_POSITION_Y,
            (uint32_t)zakohelpers_s16((uint32_t)y));
    ae_wr32(r, body + AE_ZAKO_HELPER_HIT_BODY_POSITION_Z,
            (uint32_t)zakohelpers_s16((uint32_t)z));
    ae_wr16(r, body + AE_ZAKO_HELPER_HIT_BODY_KIND,
            (uint32_t)zakohelpers_s16((uint32_t)kind));
    ae_wr16(r, body + AE_ZAKO_HELPER_HIT_BODY_FLAGS, 0u);
    ae_wr16(r, body + AE_ZAKO_HELPER_HIT_BODY_DAMAGE,
            (uint32_t)zakohelpers_s16((uint32_t)damage));
    ae_wr16(r, body + AE_ZAKO_HELPER_HIT_BODY_RADIUS,
            (uint32_t)zakohelpers_s16((uint32_t)radius));
    (void)ae_CollisionBodyRegister(r, h != NULL ? h->body_hooks : NULL,
                                   6, body, obj);
    return body;
}

uint32_t ae_ObjAddHitBodyIfPlayerNear(AeRam *r, const AeZako1Hooks *h,
                                      uint32_t obj, int32_t kind,
                                      int32_t x, int32_t y, int32_t z,
                                      int32_t damage, int32_t radius) {
    int32_t narrow_kind = zakohelpers_s16((uint32_t)kind);
    int32_t narrow_x = zakohelpers_s16((uint32_t)x);
    int32_t narrow_y = zakohelpers_s16((uint32_t)y);
    int32_t narrow_z = zakohelpers_s16((uint32_t)z);
    int32_t near_radius = zakohelpers_s16((uint32_t)(narrow_kind + 512));
    if (ae_ObjPlayerNearPointXZ(r, obj, narrow_x, narrow_y, narrow_z,
                                near_radius) != 0) {
        return 0u;
    }
    return ae_ObjAddHitBody(r, h, obj, narrow_kind, narrow_x, narrow_y,
                            narrow_z, damage, radius);
}

int32_t ae_ObjGetLinkedObjState(AeRam *r, uint32_t obj,
                                int32_t link_index) {
    uint32_t pointer_address = obj + (uint32_t)link_index * 4u +
                               AE_OBJ_HIT_TARGET;
    uint32_t linked_obj = ae_rd32(r, pointer_address);
    int32_t state = linked_obj == 0u ? 0 :
        (int32_t)(ae_rd32(r, linked_obj + AE_OBJ_POS_Z) >> 28);
    ae_wr32(r, pointer_address, 0u);
    return state;
}
