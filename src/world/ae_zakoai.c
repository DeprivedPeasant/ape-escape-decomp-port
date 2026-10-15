#include "ae_zakoai.h"

#include <stdint.h>

static int32_t zakoai_s32(uint32_t bits) {
    return (int32_t)bits;
}

static int32_t zakoai_asr32(int32_t value, unsigned shift) {
    if (shift == 0) return value;
    uint32_t shifted = (uint32_t)value >> shift;
    if (value < 0) shifted |= UINT32_MAX << (32u - shift);
    return zakoai_s32(shifted);
}

static int32_t zakoai_call(AeRam *r, const AeZakoAiHooks *h,
                           AeZakoAiHookFn fn, const char *name,
                           uint32_t a0, uint32_t a1, uint32_t a2,
                           uint32_t a3, uint32_t a4, uint32_t a5,
                           uint32_t a6) {
    if (h == NULL || fn == NULL) {
        ae_ram_fail(r, name);
        return 0;
    }
    int32_t result = fn(h->user, r, a0, a1, a2, a3, a4, a5, a6);
    return ae_ram_failed(r) ? 0 : result;
}

static int32_t zakoai_anim_frame_limit(AeRam *r, uint32_t obj) {
    uint32_t slot = ae_rd8(r, obj + AE_OBJ_ANIM_SLOT);
    uint32_t anim = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE) +
                    slot * AE_ANIM_CLIP_RECORD_SIZE;
    return (uint16_t)ae_rd16(r, anim + AE_ANIM_CLIP_LAST_FRAME);
}

static void zakoai_update_recovery_state(AeRam *r, const AeZakoAiHooks *h,
                                        uint32_t obj) {
    const AeZako1Hooks *zako_hooks = h ? h->zako1_hooks : NULL;
    if ((ae_rd16(r, obj + AE_ZAKOAI_OBJ_ACTION_FLAGS) &
         AE_ZAKOAI_ACTION_FLAG_0) == 0) {
        (void)ae_ObjAccelerate(r, obj, 0, 0, 2);
        if (ae_ram_failed(r)) return;
        int32_t speed = zakoai_asr32(
            (int32_t)((uint32_t)ae_rd16(r, obj + AE_OBJ_SPEED) << 16), 19);
        ae_ObjSetVelFromFacing(r, obj, speed);
        ae_ObjWalkOnGround(r, zako_hooks, obj);
        if (ae_ram_failed(r)) return;
    }

    if (ae_rd16(r, obj + AE_OBJ_ANIM_FRAME) >=
        (uint16_t)zakoai_anim_frame_limit(r, obj)) {
        ae_ZakoEnterIdleWait(r, obj);
    }
}

static void zakoai_update_ground_attack(AeRam *r, const AeZakoAiHooks *h,
                                        uint32_t obj, uint32_t hit_radius) {
    const AeZako1Hooks *zako_hooks = h ? h->zako1_hooks : NULL;
    uint32_t slot = ae_rd8(r, obj + AE_OBJ_ANIM_SLOT);
    uint32_t anim = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE) +
                    slot * AE_ANIM_CLIP_RECORD_SIZE;
    ae_ObjClampFrame(r, obj,
                     (int32_t)ae_rd16(r, anim + AE_ANIM_CLIP_LAST_FRAME));

    int32_t linked_state = ae_ObjGetLinkedObjState(r, obj, 0);
    if (linked_state == 6) {
        uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS) |
                         AE_ZAKOAI_OBJ_FLAG_LINKED_HIT_STATE;
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
        ae_ObjUnregisterBody(r,
            zako_hooks ? zako_hooks->init_hooks : NULL, obj);
        if (ae_ram_failed(r)) return;
    }

    if ((ae_rd32(r, obj + AE_OBJ_FLAGS) &
         AE_ZAKOAI_OBJ_FLAG_LINKED_HIT_STATE) == 0) {
        uint32_t target = ae_ObjAddHitBodyIfPlayerNear(
            r, zako_hooks, obj, AE_ZAKOAI_HIT_BODY_KIND,
            ae_rds16(r, obj + AE_ZAKOAI_OBJ_HIT_POS_X),
            ae_rds16(r, obj + AE_ZAKOAI_OBJ_HIT_POS_Y),
            ae_rds16(r, obj + AE_ZAKOAI_OBJ_HIT_POS_Z),
            1, (int32_t)hit_radius);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, obj + AE_OBJ_HIT_TARGET, target);
    }

    int32_t fell = ae_ObjFallAndBounce(r, zako_hooks, obj);
    if (ae_ram_failed(r) || fell == 0) return;

    uint32_t anim_table = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE);
    ae_wr8(r, obj + AE_OBJ_AI_STATE, AE_ZAKOAI_STATE_POST_BOUNCE);
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 3u);
    ae_wr16(r, obj + AE_OBJ_UNK_B0, 91u);
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS) &
                     ~AE_ZAKOAI_FLAGS_CLEAR_AFTER_BOUNCE;
    ae_wr16(r, obj + AE_OBJ_SPEED, 0x50u);
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
            ae_rd16(r, anim_table + 3u * AE_ANIM_CLIP_RECORD_SIZE +
                              AE_ANIM_CLIP_FIRST_FRAME));
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
    ae_ObjReenableBody(r, zako_hooks, obj);
    if (ae_ram_failed(r)) return;
    ae_ObjPlaySfx(r, zako_hooks, obj, 0x45);
    if (ae_ram_failed(r)) return;
    ae_ObjSpawnParticleAt(r, zako_hooks, obj, 4, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, obj + AE_OBJ_BODY_RADIUS, 90u);
    ae_wr16(r, obj + AE_OBJ_BODY_HEIGHT, 228u);
}

void ae_ZakoUpdateGenericEnemy(AeRam *r, const AeZakoAiHooks *h,
                               uint32_t obj) {
    const AeZako1Hooks *zako_hooks = h ? h->zako1_hooks : NULL;
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    if ((flags & AE_OBJ_FLAG_UNK_3) == 0) {
        ae_Zako_InitDefaultStats(r, obj);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, obj + AE_OBJ_FLAGS,
                ae_rd32(r, obj + AE_OBJ_FLAGS) | AE_OBJ_FLAG_UNK_3);
    }

    ae_Zako_Nop(r, obj, ae_rds16(r, obj + AE_OBJ_BODY_RADIUS));
    ae_ZakoReturnIfFar(r, obj);
    ae_Zako_RecoverFromStun(r, obj);

    uint32_t state = ae_rd8(r, obj + AE_OBJ_AI_STATE);
    switch (state) {
    case 1:
        ae_Zako_WanderBounce(r, zako_hooks, obj);
        if (ae_ram_failed(r)) return;
        /* This handler shares the watch-state tail with state 5. */
        ae_ZakoStateWatch(r, zako_hooks, obj);
        break;
    case 2:
        ae_ObjTurnTowardsPlayerXZ(r, zako_hooks, obj);
        break;
    case 3:
        ae_ZakoChasePlayer(r, zako_hooks, obj);
        if (ae_ram_failed(r)) return;
        ae_ZakoStateHurt(r, zako_hooks, obj);
        break;
    case 4:
        ae_ZakoEnterIdleWait(r, obj);
        if (ae_ram_failed(r)) return;
        ae_wr16(r, obj + AE_OBJ_UNK_B0, 91u);
        break;
    case 5: {
        ae_ZakoWalkToTarget(r, zako_hooks, obj);
        if (ae_ram_failed(r)) return;
        int32_t cooldown = ae_ObjCooldown(r, obj, -1);
        if (cooldown == 0) {
            ae_ZakoStateWatch(r, zako_hooks, obj);
        }
        break;
    }
    case 6: {
        int32_t adjust = (int8_t)ae_rd8(r, AE_G_ZAKOAI_ACCEL_ADJUST);
        if (adjust == 0) adjust = -30;
        else adjust = (int32_t)(0u - (uint32_t)adjust);
        (void)ae_ObjAccelerate(r, obj, 0, 0, adjust);
        if (ae_ram_failed(r)) return;

        if ((ae_rd16(r, obj + AE_ZAKOAI_OBJ_ACTION_FLAGS) &
         AE_ZAKOAI_ACTION_FLAG_0) == 0) {
            int32_t speed = zakoai_asr32(
                (int32_t)((uint32_t)ae_rd16(r, obj + AE_OBJ_SPEED) << 16), 19);
            int32_t facing_x = (int16_t)ae_rd16(r, obj + AE_OBJ_FACING_X);
            int32_t facing_z = (int16_t)ae_rd16(r, obj + AE_OBJ_FACING_Z);
            int32_t dx = zakoai_asr32(
                zakoai_s32((uint32_t)facing_x * (uint32_t)speed), 12);
            int32_t dz = zakoai_asr32(
                zakoai_s32((uint32_t)facing_z * (uint32_t)speed), 12);
            ae_wr32(r, obj + AE_OBJ_POS_X,
                    ae_rd32(r, obj + AE_OBJ_POS_X) - (uint32_t)dx);
            ae_wr32(r, obj + AE_OBJ_POS_Z,
                    ae_rd32(r, obj + AE_OBJ_POS_Z) - (uint32_t)dz);
            int32_t ground_y = ae_ObjGroundHeightCached(
                r, h ? h->zako1_hooks : NULL, obj, 0, 0, 0);
            if (ae_ram_failed(r)) return;
            ae_wr32(r, obj + AE_OBJ_POS_Y,
                    (uint32_t)(int32_t)(int16_t)ground_y);
        } else {
            ae_wr8(r, obj + AE_OBJ_AI_STATE, 7u);
        }

        uint32_t cooldown = ae_rd8(r, obj + AE_OBJ_COOLDOWN);
        if (cooldown != 0)
            ae_wr8(r, obj + AE_OBJ_COOLDOWN, cooldown - 1u);
        break;
    }
    case 7: {
        ae_ObjFleeFromPlayerInArea(r, zako_hooks, obj, 8);
        if (ae_ram_failed(r)) return;
        (void)ae_ObjTryStepOnGround(r, zako_hooks, obj, 64);
        if (ae_ram_failed(r)) return;
        int32_t cooldown = ae_ObjCooldown(r, obj, -1);
        if (cooldown == 0) {
            ae_wr16(r, obj + AE_OBJ_UNK_B0, 91u);
            ae_wr8(r, obj + AE_OBJ_AI_STATE, AE_ZAKOAI_STATE_IDLE_WATCH);
        }
        break;
    }
    case 8:
    case 9:
        /* The original function table maps both values directly to its return tail. */
        break;
    case AE_ZAKOAI_STATE_FALL_TO_GROUND: {
        ae_ObjTurnToPlayer(r, zako_hooks, obj);
        if (ae_ram_failed(r)) return;
        int32_t on_ground = ae_ObjFallToGround(r, zako_hooks, obj);
        if (ae_ram_failed(r)) return;
        if (on_ground != 0) {
            ae_ObjPlaySfx(r, h ? h->zako1_hooks : NULL, obj, 0x45);
            if (ae_ram_failed(r)) return;
            uint32_t count = (ae_rd8(r, obj + AE_OBJ_SUB_STATE) + 1u) & 0xFFu;
            ae_wr8(r, obj + AE_OBJ_SUB_STATE, count);
            if (count < 2u) {
                ae_ObjSetSpeed96(r, obj, -52);
            } else {
                ae_ZakoEnterDeath(r, zako_hooks, obj);
                if (ae_ram_failed(r)) return;
            }
        }
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Y,
                ae_rd16(r, obj + AE_OBJ_VELOCITY_Y) + 10u);
        break;
    }
    case AE_ZAKOAI_STATE_HIT_BODY_BOUNCE:
        zakoai_update_ground_attack(r, h, obj,
                                    AE_ZAKOAI_HIT_BODY_RADIUS);
        break;
    case 12:
        zakoai_update_recovery_state(r, h, obj);
        break;
    default:
        break;
    }
    if (ae_ram_failed(r)) return;

    if (ae_rd8(r, obj + AE_OBJ_TYPE) == AE_ZAKOAI_TYPE_HIGH_FALL_CHECK &&
        ae_rd8(r, obj + AE_OBJ_AI_STATE) !=
            AE_ZAKOAI_STATE_HIT_BODY_BOUNCE) {
        int32_t fall_limit = (int16_t)ae_rd16(r, obj + AE_OBJ_HOME_Y) +
                             AE_ZAKOAI_STAGE_FALL_MARGIN;
        if (fall_limit < zakoai_s32(ae_rd32(r, obj + AE_OBJ_POS_Y))) {
            (void)ae_ZakoDefeat(r, h ? h->zako1_hooks : NULL, obj);
        }
    }
}

void ae_ZakoUpdateStageSpecificEnemy(AeRam *r, const AeZakoAiHooks *h,
                                     uint32_t obj) {
    uint32_t level = ae_rd16(r, AE_G_ZAKOAI_STAGE_STATE +
                                AE_ZAKO_STAGE_LEVEL);
    AeZakoAiHookFn fn = NULL;
    const char *name = NULL;
    switch (level) {
    case 2:
        fn = h ? h->FUN_8013DF54 : NULL;
        name = "FUN_8013DF54 overlay hook not provided";
        break;
    case 3:
        fn = h ? h->FUN_80140880 : NULL;
        name = "FUN_80140880 overlay hook not provided";
        break;
    case 9:
        fn = h ? h->FUN_8013F504 : NULL;
        name = "FUN_8013F504 overlay hook not provided";
        break;
    case 10:
        fn = h ? h->FUN_8014116C : NULL;
        name = "FUN_8014116C overlay hook not provided";
        break;
    default:
        return;
    }
    (void)zakoai_call(r, h, fn, name, obj, 0, 0, 0, 0, 0, 0);
}

void ae_ZakoAreaModeNoop(AeRam *r) {
    (void)r;
}
