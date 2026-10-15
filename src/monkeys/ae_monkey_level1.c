/*
 * Monkey level1 routines, hand-ported and verified against the original game.
 * Field layouts and integration hooks are declared in ae_monkey.h.
 */
#include "ae_monkey.h"

#define RD8(a) ae_rd8(r, (a))
#define RD16(a) ae_rd16(r, (a))
#define RD32(a) ae_rd32(r, (a))
#define RDS16(a) ae_rds16(r, (a))
#define RDS32(a) ae_rds32(r, (a))
#define WR8(a, v) ae_wr8(r, (a), (uint32_t)(v))
#define WR16(a, v) ae_wr16(r, (a), (uint32_t)(v))
#define WR32(a, v) ae_wr32(r, (a), (uint32_t)(v))

/* Narrow to a signed halfword with wraparound. */
static int32_t s16(uint32_t v) { return (int32_t)(int16_t)(uint16_t)v; }
/* Shift right with sign extension. */
static int32_t sra32(int32_t v, uint32_t n) {
    n &= 31u;
    return v < 0 ? (int32_t)~((~(uint32_t)v) >> n) : (int32_t)((uint32_t)v >> n);
}
/* Multiply with 32-bit wraparound. */
static int32_t mul_lo(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
/* Return the signed high half of a 64-bit product. */
static int32_t mul_hi(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * (int64_t)b) >> 32); }
/* Return the unsigned high half of a 64-bit product. */
static uint32_t mulu_hi(uint32_t a, uint32_t b) { return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32); }
/* Negate with 32-bit wraparound. */
static int32_t neg32(int32_t v) { return (int32_t)(0u - (uint32_t)v); }
/* Add with 32-bit wraparound. */
static int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
/* Subtract with 32-bit wraparound. */
static int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
/* Read the current monkey address. */
static uint32_t cur_monkey(AeRam *r) { return RD32(AE_G_CUR_MONKEY); }
/* Locate the information record for a monkey. */
static uint32_t info_of(AeRam *r, uint32_t m) {          /* MonkeyInfo of the monkey: table + id * 0x88 */
    return RD32(AE_G_MONKEY_INFO_TABLE) + RD8(m + AE_MK_ID) * AE_MONKEY_INFO_SIZE;
}
/* Locate a navigation node. */
static uint32_t node_of(AeRam *r, uint32_t index) {      /* nav node address: table + index * 0x18 */
    return index * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES);
}
/* MIPS div: break (a trap) on a zero divisor and INT_MIN / -1 */
/* Divide signed values and report invalid divisors. */
static int32_t div_trap(AeRam *r, int32_t a, int32_t b) {
    if (b == 0 || (b == -1 && a == INT32_MIN)) {
        ae_ram_fail(r, "divide trap (div by zero / INT_MIN / -1; the original executes break)");
        return 0;
    }
    return a / b;
}
/* Take a signed remainder and report invalid divisors. */
static int32_t rem_trap(AeRam *r, int32_t a, int32_t b) {
    if (b == 0 || (b == -1 && a == INT32_MIN)) {
        ae_ram_fail(r, "divide trap (div by zero / INT_MIN / -1; the original executes break)");
        return 0;
    }
    return a % b;
}
/* Shift left with 32-bit wraparound. */
static int32_t shift_left32(int32_t v, uint32_t n) { return (int32_t)((uint32_t)v << n); }   /* Preserve 32-bit wraparound. */

/* CopySvecAt1C @800154DC: copies the 8 bytes at src + 0x1C to dst (lwl / lwr / swl / swr pairs; not a monkey function,
   self-contained, diffed through MonkeyLineOfSight) */
/* Copy an actor short position while preserving overlapping-copy behavior. */
static void copy_svec_at_1c(AeRam *r, uint32_t src, uint32_t dst) {
    uint32_t v[8];
    for (uint32_t i = 0; i < 8; ++i) v[i] = RD8(src + AE_MK_POS_SHORT + i);
    for (uint32_t i = 0; i < 8; ++i) WR8(dst + i, v[i]);
}

/* ================================================================================================
 * Animation
 * ============================================================================================== */

/* MonkeyRandomHesitate @80032F24: 1 = hesitate (lod / cooldown byte +0xD4 counts down, a 3 % chance restarts it at 4) */
/* Tick the hesitation timer and occasionally restart it. */
int32_t ae_MonkeyRandomHesitate(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    int32_t cooldown = (int8_t)RD8(monkey + AE_MK_LOD);
    uint32_t cooldown_bits = RD8(monkey + AE_MK_LOD);
    if (cooldown > 0) WR8(monkey + AE_MK_LOD, cooldown_bits - 1u);
    uint32_t reloaded_monkey = cur_monkey(r);
    uint32_t flags = RD32(node_of(r, RD16(reloaded_monkey + AE_MK_NAV_NODE0)));
    if (flags & 0x100000u) return 1;
    if ((int8_t)RD8(reloaded_monkey + AE_MK_LOD) > 0) return 1;
    uint32_t rnd = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t quotient_hi = mulu_hi(rnd, 0x51EB851Fu);
    uint32_t q = quotient_hi >> 5;
    uint32_t scaled_quotient = q * 100u;
    uint32_t rem = (rnd - scaled_quotient) & 0xFFFFu;
    if (rem < 3u) {
        WR8(cur_monkey(r) + AE_MK_LOD, 4);
        return 1;
    }
    return 0;
}

/* MonkeyAdvanceAnimFrame @800330C8: frame++ and clamp it to the last frame + 1 of the current animation */
/* Advance the animation frame without passing its terminal frame. */
void ae_MonkeyAdvanceAnimFrame(AeRam *r) {
    uint32_t saved_monkey = cur_monkey(r);
    WR16(saved_monkey + AE_MK_ANIM_FRAME, RD16(saved_monkey + AE_MK_ANIM_FRAME) + 1u);
    int32_t frame = (int32_t)RD16(saved_monkey + AE_MK_ANIM_FRAME);
    int32_t idx = ae_MonkeyGetAnimIndex(r);
    uint32_t monkey = cur_monkey(r);
    uint32_t tbl = RD32(monkey + AE_MK_ANIM_TABLE);
    int32_t last1 = (int32_t)RD16(tbl + ((uint32_t)idx << 3) + AE_MK_ANIM_LAST_FRAME) + 1;
    if (frame < last1) {
        WR16(saved_monkey + AE_MK_ANIM_FRAME, RD16(monkey + AE_MK_ANIM_FRAME));
    } else {
        idx = ae_MonkeyGetAnimIndex(r);
        uint32_t reloaded_monkey = cur_monkey(r);
        tbl = RD32(reloaded_monkey + AE_MK_ANIM_TABLE);
        WR16(saved_monkey + AE_MK_ANIM_FRAME, (uint32_t)RD16(tbl + ((uint32_t)idx << 3) + AE_MK_ANIM_LAST_FRAME) + 1u);
    }
}

/* MonkeyPickAnim @800331B0: frame = first frame of the current animation; states without bit 2 reset +0x96 */
/* Restart the animation selected by the current state. */
void ae_MonkeyPickAnim(AeRam *r) {
    int32_t idx = ae_MonkeyGetAnimIndex(r);
    uint32_t monkey = cur_monkey(r);
    uint32_t tbl = RD32(monkey + AE_MK_ANIM_TABLE);
    WR16(monkey + AE_MK_ANIM_FRAME, RD16(tbl + ((uint32_t)idx << 3)));
    uint32_t flags = RD32(AE_G_MONKEY_STATE_TABLE + RD8(monkey + AE_MK_AI_STATE) * 8u + 4u);
    if (!((flags >> 2) & 1u)) WR16(monkey + AE_MK_COUNTER96, 0);
}

/* MonkeyAnimFinished @80033A3C */
/* Test whether the current animation still has frames remaining. */
int32_t ae_MonkeyAnimFinished(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    uint32_t flags = RD32(AE_G_MONKEY_STATE_TABLE + RD8(monkey + AE_MK_AI_STATE) * 8u + 4u);
    uint32_t past_end;
    if (!((flags >> 2) & 1u)) {
        int32_t frame = RDS16(monkey + AE_MK_COUNTER96);
        past_end = (int32_t)RD16(monkey + AE_MK_ANIM_LENGTH) < frame;
    } else {
        int32_t idx = ae_MonkeyGetAnimIndex(r);
        monkey = cur_monkey(r);
        uint32_t tbl = RD32(monkey + AE_MK_ANIM_TABLE);
        uint32_t frame = RD16(monkey + AE_MK_ANIM_FRAME);
        past_end = RD16(tbl + ((uint32_t)idx << 3) + AE_MK_ANIM_LAST_FRAME) < frame;
    }
    return (int32_t)(past_end ^ 1u);
}

/* MonkeyAnimAtStart @80033ACC: frame == first frame + 1 */
/* Test for the first advanced animation frame. */
int32_t ae_MonkeyAnimAtStart(AeRam *r) {
    int32_t idx = ae_MonkeyGetAnimIndex(r);
    uint32_t monkey = cur_monkey(r);
    uint32_t tbl = RD32(monkey + AE_MK_ANIM_TABLE);
    uint32_t first_frame_plus_one = RD16(tbl + ((uint32_t)idx << 3)) + 1u;
    uint32_t frame = RD16(monkey + AE_MK_ANIM_FRAME);
    return (frame ^ first_frame_plus_one) < 1u;
}

/* ================================================================================================
 * Projectiles and aiming
 * ============================================================================================== */

/* the common tail of the three "fire" functions: cur->0x222 (a u8 counter) is decremented */
/* Consume one shot from the current attack. */
static void decrement_shots_left(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    WR8(monkey + AE_MK_PATROL_COUNTER, RD8(monkey + AE_MK_PATROL_COUNTER) - 1u);
}

/* MonkeyFireProjectile6 @80033DF0 */
/* Fire projectile six, queue its sound and consume one shot. */
void ae_MonkeyFireProjectile6(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 1u);
    ae_MonkeySpawnProjectile(r, h, 6);
    ae_MonkeyQueueVoice(r, h, 75, 5);
    decrement_shots_left(r);
}

/* MonkeyFireProjectile7 @800343B0 */
/* Fire projectile seven, queue its sound and consume one shot. */
void ae_MonkeyFireProjectile7(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 1u);
    ae_MonkeySpawnProjectile(r, h, 7);
    ae_MonkeyQueueVoice(r, h, 72, 2);
    decrement_shots_left(r);
}

/* MonkeyAimAtTarget @80033E4C: aims a throw at the target with a wobble that depends on the frame counter. */
/* Aim with periodic wobble and fire on the mode-specific cadence. */
void ae_MonkeyAimAtTarget(AeRam *r, const AeMonkeyHooks *h, int32_t aim_kind) {
    uint32_t sp = AE_STK(AE_STK_AIM_TARGET);
    uint32_t aim_mode_bits = (uint32_t)aim_kind;
    int32_t aim_mode = aim_kind;
    uint32_t ang = mulu_hi(RD32(AE_G_FRAME_COUNTER) << 12, 0x51EB851Fu) >> 5;
    int32_t c = ae_sdk_rcos(r, (int32_t)ang);
    int32_t scaled_wobble = mul_lo(c, 400);
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    if (scaled_wobble < 0) scaled_wobble = add32(scaled_wobble, 4095);
    int32_t wobble = sra32(scaled_wobble, 12);
    ang = mulu_hi(RD32(AE_G_FRAME_COUNTER) << 12, 0x51EB851Fu) >> 5;
    uint32_t aim_origin = RD32(AE_G_CUR_MONKEY);
    uint32_t component = RD16(target + AE_MK_POS);
    component = component + (uint32_t)wobble - RD16(aim_origin + AE_MK_POS);
    WR16(sp + 24, component);
    int32_t sn = ae_sdk_rsin(r, (int32_t)ang);
    scaled_wobble = mul_lo(sn, 400);
    uint32_t tgt_actor = RD32(AE_G_MONKEY_TARGET);
    if (scaled_wobble < 0) scaled_wobble = add32(scaled_wobble, 4095);
    wobble = sra32(scaled_wobble, 12);
    uint32_t origin_monkey = RD32(AE_G_CUR_MONKEY);
    component = RD16(tgt_actor + AE_MK_POS + 8) + (uint32_t)wobble - RD16(origin_monkey + AE_MK_POS + 8);
    WR16(sp + 28, component);
    if ((aim_mode_bits << 16) != 0) {
        component = RD16(tgt_actor + AE_MK_POS + 4) - RD16(origin_monkey + AE_MK_POS + 4);
    } else {
        component = RD16(tgt_actor + AE_MK_POS + 4) + 125u - RD16(origin_monkey + AE_MK_POS + 4);
    }
    WR16(sp + 26, component);
    ae_sdk_VectorNormalSS(r, sp + 24, sp + 24);
    if (ae_ram_failed(r)) return;
    int32_t direction_x_rounded = RDS16(sp + 24);
    if (direction_x_rounded < 0) direction_x_rounded += 63;
    int32_t velocity_x = sra32(direction_x_rounded, 6);
    int32_t direction_y_rounded = RDS16(sp + 26);
    WR16(sp + 24, velocity_x);
    if (direction_y_rounded < 0) direction_y_rounded += 63;
    int32_t velocity_y = sra32(direction_y_rounded, 6);
    int32_t direction_z = RDS16(sp + 28);
    WR16(sp + 26, velocity_y);
    if (direction_z < 0) direction_z += 63;
    uint32_t cm = cur_monkey(r);
    int32_t velocity_z = sra32(direction_z, 6);
    WR16(sp + 28, velocity_z);
    uint32_t px = RD16(cm + AE_MK_POS);
    WR16(sp + 16, px);
    uint32_t py = RD16(cm + AE_MK_POS + 4);
    WR16(sp + 18, py);
    uint32_t pz = RD16(cm + AE_MK_POS + 8);
    WR16(sp + 20, pz);
    if (RD8(cm + AE_MK_AI_STATE) == 30) {
        WR16(sp + 18, py + 20u);
    } else {
        WR16(sp + 16, px + (uint32_t)velocity_x * 3u);
        WR16(sp + 18, py - 100u + (uint32_t)velocity_y * 2u);
        WR16(sp + 20, pz + (uint32_t)velocity_z * 3u);
    }
    int32_t pitch_velocity = RDS16(sp + 26);
    cm = cur_monkey(r);
    int32_t direction_y = (int32_t)((uint32_t)pitch_velocity << 6);
    int32_t abs_direction_y = direction_y < 0 ? neg32(direction_y) : direction_y;
    int32_t aim_frame_offset = 0;
    if (RD8(cm + AE_MK_AI_STATE) != 30) {
        int32_t abs_pitch = s16((uint32_t)abs_direction_y);
        if (abs_pitch < 1060) {
            aim_frame_offset = 4;
        } else if (abs_pitch < 2896) {
            aim_frame_offset = (pitch_velocity <= 0) ? 6 : 2;
        } else {
            aim_frame_offset = (pitch_velocity < 1) ? 8 : 0;
        }
        int32_t idx = ae_MonkeyGetAnimIndex(r);
        uint32_t aim_monkey = cur_monkey(r);
        uint32_t tbl = RD32(aim_monkey + AE_MK_ANIM_TABLE);
        uint32_t w = RD16(tbl + ((uint32_t)idx << 3)) + (uint32_t)aim_frame_offset;
        WR16(aim_monkey + AE_MK_AIM_FRAME, w);
        WR16(aim_monkey + AE_MK_ANIM_FRAME, w);
        WR16(aim_monkey + AE_MK_MOVE_DIR, RD16(sp + 24));
        WR16(aim_monkey + AE_MK_MOVE_DIR + 4, RD16(sp + 28));
        ae_sdk_VectorNormalSS(r, aim_monkey + AE_MK_MOVE_DIR, aim_monkey + AE_MK_MOVE_DIR);
        if (ae_ram_failed(r)) return;
    }
    int32_t shot_mode = s16((uint32_t)aim_mode);
    uint32_t cnt = RD32(AE_G_FRAME_COUNTER);
    if (shot_mode == 0) {
        if ((cnt & 3u) != 0) return;
    } else {
        if (shot_mode != 5) return;
        cnt = RD32(AE_G_FRAME_COUNTER);
        uint32_t q = mulu_hi(cnt, 0xCCCCCCCDu) >> 2;
        if (cnt != q * 5u) return;
    }
    uint32_t shot_monkey = cur_monkey(r);
    WR16(shot_monkey + AE_MK_COUNTER96, RD16(shot_monkey + AE_MK_COUNTER96) + 1u);
    int32_t projectile_type;
    if (shot_mode == 5) {
        int32_t x = RDS16(sp + 24);
        int32_t scaled_velocity_x = mul_lo(x, 180);
        int32_t y = RDS16(sp + 26);
        int32_t scaled_velocity_y = mul_lo(y, 180);
        int32_t z = RDS16(sp + 28);
        int32_t scaled_velocity_z = mul_lo(z, 180);
        projectile_type = 2;
        WR16(sp + 24, sub32(sra32(mul_hi(scaled_velocity_x, 0x51EB851F), 5), sra32(scaled_velocity_x, 31)));
        WR16(sp + 26, sub32(sra32(mul_hi(scaled_velocity_y, 0x51EB851F), 5), sra32(scaled_velocity_y, 31)));
        WR16(sp + 28, sub32(sra32(mul_hi(scaled_velocity_z, 0x51EB851F), 5), sra32(scaled_velocity_z, 31)));
        ae_MonkeyQueueVoice(r, h, 79, 3);
    } else {
        projectile_type = 1;
        int32_t x = RDS16(sp + 24);
        int32_t z = RDS16(sp + 28);
        WR16(sp + 24, (uint32_t)x << (projectile_type & 31));
        int32_t y = RDS16(sp + 26);
        WR16(sp + 28, (uint32_t)z << (projectile_type & 31));
        WR16(sp + 26, (uint32_t)y << (projectile_type & 31));
        ae_MonkeyQueueVoice(r, h, 100, projectile_type);
        ae_MonkeyQueueVoice(r, h, 100, 3);
    }
    AE_MK_HOOK(r, h, ProjectileSpawn, sp + 16, RD32(AE_G_CUR_MONKEY), projectile_type, sp + 24);
    shot_monkey = cur_monkey(r);
    uint32_t w = RD16(shot_monkey + AE_MK_AIM_FRAME) + 1u;
    uint32_t b = RD8(shot_monkey + AE_MK_PATROL_COUNTER) - 1u;
    WR16(shot_monkey + AE_MK_ANIM_FRAME, w);
    WR8(shot_monkey + AE_MK_PATROL_COUNTER, b);
}

/* MonkeyFireProjectileAimed @8003429C: type 6 (arg == 6) picks a random 0..2 variant, otherwise type 5 with the
   offset to the target (all three components are stored at the same place: the original does, so only z survives). */
/* Fire an aimed throw or a random projectile variant. */
void ae_MonkeyFireProjectileAimed(AeRam *r, const AeMonkeyHooks *h, int32_t arg) {
    uint32_t sp = AE_STK(AE_STK_FIRE_AIMED);
    uint32_t monkey = cur_monkey(r);
    WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 1u);
    WR16(sp + 16, RD16(monkey + AE_MK_POS));
    WR16(sp + 18, RD16(monkey + AE_MK_POS + 4) - 200u);
    WR16(sp + 20, RD16(monkey + AE_MK_POS + 8));
    int32_t projectile_kind = s16((uint32_t)arg);
    if (projectile_kind == 6) {
        uint32_t rnd = ae_MonkeyRand(r) & 0xFFFFu;
        uint32_t q = mulu_hi(rnd, 0xAAAAAAABu) >> 1;
        uint32_t target = (rnd - (q * 2u + q)) & 0xFFFFu;
        monkey = cur_monkey(r);
        AE_MK_HOOK(r, h, ProjectileSpawn, sp + 16, monkey, 10, target);
    } else {
        uint32_t target = RD32(AE_G_MONKEY_TARGET);
        WR16(sp + 24, RD16(target + AE_MK_POS) - RD16(monkey + AE_MK_POS));
        WR16(sp + 24, RD16(target + AE_MK_POS + 4) - RD16(monkey + AE_MK_POS + 4));
        WR16(sp + 24, RD16(target + AE_MK_POS + 8) - RD16(monkey + AE_MK_POS + 8));
        AE_MK_HOOK(r, h, ProjectileSpawn, sp + 16, monkey, 5, sp + 24);
    }
    ae_MonkeyQueueVoice(r, h, 73, 0);
    uint32_t monkey_after_shot = cur_monkey(r);
    uint32_t shots_left = RD8(monkey_after_shot + AE_MK_PATROL_COUNTER);
    WR16(monkey_after_shot + AE_MK_SHOT_COOLDOWN, 10);
    WR8(monkey_after_shot + AE_MK_PATROL_COUNTER, shots_left - 1u);
}

/* MonkeyFireProjectile8 @8003440C: type 8, 200 above the monkey (the same single offset slot as above). */
/* Fire projectile eight above the monkey. */
void ae_MonkeyFireProjectile8(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_FIRE_8);
    uint32_t monkey = cur_monkey(r);
    WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 1u);
    WR16(sp + 16, RD16(monkey + AE_MK_POS));
    WR16(sp + 18, RD16(monkey + AE_MK_POS + 4) + 200u);
    WR16(sp + 20, RD16(monkey + AE_MK_POS + 8));
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    WR16(sp + 24, RD16(target + AE_MK_POS) - RD16(monkey + AE_MK_POS));
    WR16(sp + 24, RD16(target + AE_MK_POS + 4) - RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 24, RD16(target + AE_MK_POS + 8) - RD16(monkey + AE_MK_POS + 8));
    AE_MK_HOOK(r, h, ProjectileSpawn, sp + 16, monkey, 8, sp + 24);
    ae_MonkeyQueueVoice(r, h, 73, 0);
    decrement_shots_left(r);
}

/* MonkeyAngleToTarget @8003455C: angle between the move direction (x, z) and the direction to the target, 1..4096 */
/* Measure the target angle relative to the movement direction. */
int32_t ae_MonkeyAngleToTarget(AeRam *r) {
    uint32_t sp = AE_STK(AE_STK_ANGLE_TO_TARGET);
    uint32_t monkey = cur_monkey(r);
    int32_t move_x = RDS16(monkey + AE_MK_MOVE_DIR);
    WR32(sp + 20, 0);
    WR32(sp + 16, move_x);
    int32_t move_z = RDS16(monkey + AE_MK_MOVE_DIR + 4);
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    WR32(sp + 24, move_z);
    int32_t target_x = (int32_t)RD32(target + AE_MK_POS);
    int32_t monkey_x = (int32_t)RD32(monkey + AE_MK_POS);
    WR32(sp + 36, 0);
    WR32(sp + 32, (uint32_t)sub32(target_x, monkey_x));
    int32_t monkey_z = (int32_t)RD32(monkey + AE_MK_POS + 8);
    int32_t target_z = (int32_t)RD32(target + AE_MK_POS + 8);
    WR32(sp + 40, (uint32_t)sub32(target_z, monkey_z));
    int32_t angle = ae_AngleBetweenXZ(r, sp + 16, sp + 32);
    if (!(angle > 0)) angle = add32(angle, 4096);
    return angle;
}

/* MonkeyApplyMoveDir @80034834: derives the move direction; outside the jump / hurt states moveDir = normalise(wishDir.x, 0, wishDir.z) */
/* Derive horizontal movement facing unless the state preserves its direction. */
void ae_MonkeyApplyMoveDir(AeRam *r) {
    ae_MonkeyCalcMoveDir(r);
    if (ae_ram_failed(r)) return;
    if (ae_MonkeyIsJumpArcState(r)) return;
    if (RD8(cur_monkey(r) + AE_MK_AI_STATE) == 9) return;
    if (ae_MonkeyIsAltAnimState(r)) return;
    uint32_t monkey = cur_monkey(r);
    uint32_t st = RD8(monkey + AE_MK_AI_STATE);
    if (st == 41 || st == 43) return;
    uint32_t wish_x = RD16(monkey + AE_MK_WISH_DIR);
    uint32_t wish_z = RD16(monkey + AE_MK_WISH_DIR + 4);
    WR16(monkey + AE_MK_MOVE_DIR + 2, 0);
    WR16(monkey + AE_MK_MOVE_DIR, wish_x);
    WR16(monkey + AE_MK_MOVE_DIR + 4, wish_z);
    ae_sdk_VectorNormalSS(r, monkey + AE_MK_MOVE_DIR, monkey + AE_MK_MOVE_DIR);
}

/* NavNodeGetPos @80034EF8: world position of a nav node into out[3] (s32): bone position for nodes with bit 10 (node
   ids from the link fields), a moving reference + offset for nodes with bit 0, else the node's own position. */
/* Resolve a plain, moving-reference or object-bone node position. */
void ae_NavNodeGetPos(AeRam *r, const AeMonkeyHooks *h, int32_t node, uint32_t out) {
    uint32_t sp = AE_STK(AE_STK_NAV_NODE_POS);
    uint32_t node_index = (uint32_t)node;
    uint32_t node_addr = node_of(r, node_index & 0xFFFFu);
    uint32_t node_flags = RD32(node_addr);
    uint32_t position_out = out;
    if (node_flags & 0x400u) {
        ae_NavNodeGetLinkIds(r, s16(node_index), sp + 24, sp + 26, sp + 28);
        if (ae_ram_failed(r)) return;
        int32_t object_slot = RDS16(sp + 24);
        uint32_t matrix_index = RD16(sp + 26);
        uint32_t bone_index = RD16(sp + 28);
        int32_t ok = ae_ObjGetBonePos(r, h, object_slot, matrix_index, bone_index, sp + 16);
        if (ok != 0) {
            WR32(position_out, RDS16(sp + 16));
            WR32(position_out + 4, RDS16(sp + 18));
            WR32(position_out + 8, RDS16(sp + 20));
        } else {
            WR32(position_out, 0);
            WR32(position_out + 4, 0);
            WR32(position_out + 8, 0);
        }
        return;
    }
    if (node_flags & 1u) {
        int32_t v = RDS16(node_addr + AE_NAV_LINK_CODE);
        uint32_t rec = (uint32_t)v * 56u + RD32(AE_G_NAV_GROUND_REF);
        int32_t x = RDS16(node_addr + AE_NAV_POS) + RDS16(rec + 0);
        WR32(position_out, x);
        v = RDS16(node_addr + AE_NAV_LINK_CODE);
        rec = (uint32_t)v * 56u + RD32(AE_G_NAV_GROUND_REF);
        int32_t y1 = RDS16(node_addr + AE_NAV_POS + 2);
        int32_t y2 = RDS16(rec + 2);
        int32_t y3 = RDS16(rec + AE_NAV_REF_Y_OFFSET);
        WR32(position_out + 4, (uint32_t)sub32(add32(y1, y2), y3));
        v = RDS16(node_addr + AE_NAV_LINK_CODE);
        rec = (uint32_t)v * 56u + RD32(AE_G_NAV_GROUND_REF);
        int32_t z1 = RDS16(node_addr + AE_NAV_POS + 4);
        int32_t z2 = RDS16(rec + 4);
        WR32(position_out + 8, (uint32_t)add32(z1, z2));
        return;
    }
    WR32(position_out, RDS16(node_addr + AE_NAV_POS));
    WR32(position_out + 4, RDS16(node_addr + AE_NAV_POS + 2));
    WR32(position_out + 8, RDS16(node_addr + AE_NAV_POS + 4));
}

/* MonkeyTargetInSightRange @800361B4: is the target close enough for the monkey to notice it? */
/* Test target distance against the applicable sight range. */
int32_t ae_MonkeyTargetInSightRange(AeRam *r) {
    if (ae_AreaIsBossArena(r)) return 0;
    if (ae_MonkeyIsIdleActivityState(r)) {
        if ((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 19) & 1u) return 0;
    }
    uint32_t target_is_grounded = (RD32(RD32(AE_G_MONKEY_TARGET) + AE_MK_TARGET_STATE_FLAGS) & 0xFu) < 2u;
    if (target_is_grounded) {
        uint32_t monkey = cur_monkey(r);
        uint32_t info = info_of(r, monkey) + (target_is_grounded << 1);
        int32_t range_limit = RDS16(info + AE_MK_INFO_SIGHT_RANGES);
        int32_t distance = RDS32(monkey + AE_MK_DIST_TO_TARGET);
        if (distance < range_limit) return 1;
    }
    uint32_t monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == 51) {
        int32_t sight_range = RDS16(info_of(r, monkey) + AE_MK_INFO_SIGHT_RANGE);
        int32_t distance = RDS32(monkey + AE_MK_DIST_TO_TARGET);
        int32_t expanded_range = add32(shift_left32(sight_range, 1), sight_range);
        if (distance < expanded_range) return 1;
    }
    monkey = cur_monkey(r);
    int32_t range_limit = RDS16(info_of(r, monkey) + AE_MK_INFO_SIGHT_RANGE);
    int32_t distance = RDS32(monkey + AE_MK_DIST_TO_TARGET);
    return distance < range_limit;
}

/* MonkeyBodyOff @80036B24: flags: +0x40000000, -bits 16/17, +bit 17 ("body off"); body unregistered; puff */
/* Disable the collision body and spawn its transition puff. */
void ae_MonkeyBodyOff(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t flags = RD32(monkey + AE_MK_FLAGS);
    WR16(monkey + AE_MK_NOTICE_COUNT, 0);
    flags = ((flags | 0x40000000u) & 0xFFFCFFFFu) | 0x20000u;
    WR32(monkey + AE_MK_FLAGS, flags);
    ae_MonkeyBodyUnregister(r, h);
    ae_MonkeyPuff5(r, h);
}

/* MonkeyBodyOn @80036B78 */
/* Enable the collision body and spawn its transition puff. */
void ae_MonkeyBodyOn(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t flags = RD32(monkey + AE_MK_FLAGS);
    WR16(monkey + AE_MK_NOTICE_COUNT, 0);
    flags = ((flags & 0xBFFFFFFFu) & 0xFFFCFFFFu) | 0x10000u;
    WR32(monkey + AE_MK_FLAGS, flags);
    ae_MonkeyBodyRegister(r, h);
    ae_MonkeyPuff5(r, h);
}

/* MonkeyDefeatBurst @80036C84 */
/* Spawn the defeat burst and queue its two sounds. */
void ae_MonkeyDefeatBurst(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    int32_t x = RDS16(monkey + AE_MK_POS);
    int32_t y = RDS16(monkey + AE_MK_POS + 4);
    int32_t z = RDS16(monkey + AE_MK_POS + 8);
    AE_MK_HOOK(r, h, EffectBigBurst, x, y, z, 0);
    ae_MonkeyQueueVoice(r, h, 121, 0);
    ae_MonkeyQueueVoice(r, h, 121, 2);
}

/* ================================================================================================
 * Particles
 * ============================================================================================== */

/* MonkeyFootDust @80038F00: every third frame a dust particle under a moving monkey with a "dusty" info record */
/* Spawn foot dust on the eligible staggered frames. */
void ae_MonkeyFootDust(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_FOOT_DUST);
    uint32_t frame = RD32(AE_G_FRAME_COUNTER);
    uint32_t quotient = mulu_hi(frame, 0xAAAAAAABu) >> 1;
    if (frame != quotient + (quotient << 1)) return;
    uint32_t monkey = cur_monkey(r);
    if (RDS16(info_of(r, monkey) + AE_MK_INFO_ALERT_SPEED) == 0) return;
    uint32_t st = RD8(monkey + AE_MK_AI_STATE);
    if (st == 26 || st == 53) return;
    if (ae_MonkeyIsState0C_or_24(r)) return;
    uint32_t m = cur_monkey(r);
    WR16(sp + 16, RD16(m + AE_MK_POS));
    WR16(sp + 18, RD16(m + AE_MK_POS + 4) - 32u);
    WR16(sp + 20, RD16(m + AE_MK_POS + 8));
    AE_MK_HOOK(r, h, SpawnParticle, 9, sp + 16, 0, 0);
}

/* one debris particle: SpawnParticle(7, position - rand offsets (-100..99, -50..149, -100..99)); sp holds the monkey position
   at +16 and receives the particle position at +24 */
/* Spawn one debris particle with random position offsets. */
static void debris_once(AeRam *r, const AeMonkeyHooks *h, uint32_t sp) {
    uint32_t rnd = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t q = mulu_hi(rnd, 0x51EB851Fu) >> 6;
    uint32_t scaled_quotient = q * 200u;
    uint32_t random_offset = rnd - scaled_quotient - 100u;
    WR16(sp + 24, RD16(sp + 16) - random_offset);
    rnd = ae_MonkeyRand(r) & 0xFFFFu;
    q = mulu_hi(rnd, 0x51EB851Fu) >> 6;
    scaled_quotient = q * 200u;
    random_offset = rnd - scaled_quotient - 50u;
    WR16(sp + 26, RD16(sp + 18) - random_offset);
    rnd = ae_MonkeyRand(r) & 0xFFFFu;
    q = mulu_hi(rnd, 0x51EB851Fu) >> 6;
    scaled_quotient = q * 200u;
    random_offset = rnd - scaled_quotient - 100u;
    WR16(sp + 28, RD16(sp + 20) - random_offset);
    AE_MK_HOOK(r, h, SpawnParticle, 7, sp + 24, 0, 0);
}

/* MonkeyDebrisBurst6 @80038FD8: six debris particles around the monkey */
/* Spawn six random debris particles. */
void ae_MonkeyDebrisBurst6(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_DEBRIS6);
    uint32_t monkey = cur_monkey(r);
    WR16(sp + 16, RD16(monkey + AE_MK_POS));
    WR16(sp + 18, RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 20, RD16(monkey + AE_MK_POS + 8));
    int32_t particles_spawned = 0;
    do {
        ++particles_spawned;
        debris_once(r, h, sp);
    } while (particles_spawned < 6);
}

/* MonkeyDebrisBurst1 @800390F8 */
/* Spawn one random debris particle. */
void ae_MonkeyDebrisBurst1(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_DEBRIS1);
    uint32_t monkey = cur_monkey(r);
    WR16(sp + 16, RD16(monkey + AE_MK_POS));
    WR16(sp + 18, RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 20, RD16(monkey + AE_MK_POS + 8));
    debris_once(r, h, sp);
}

/* MonkeyAlertPuff @80039248: particle 256 at the monkey and voice 114 */
/* Spawn the alert puff and queue its voice. */
void ae_MonkeyAlertPuff(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_ALERT_PUFF);
    uint32_t monkey = cur_monkey(r);
    WR16(sp + 16, RD16(monkey + AE_MK_POS));
    WR16(sp + 18, RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 20, RD16(monkey + AE_MK_POS + 8));
    AE_MK_HOOK(r, h, SpawnParticle, 256, sp + 16, 0, 0);
    ae_MonkeyQueueVoice(r, h, 114, 1);
}

/* ================================================================================================
 * Contact / screen / LOD
 * ============================================================================================== */

/* MonkeyUpdateContact @800392F0: reads what touched the monkey (BodyContactType = top nibble of +0x50: 8 = the Monkey Net,
   1 / 10 / 11 = other gadget hits, 3 = a body contact that counts under some conditions).  Stores the type at +0x126
   (not for the net on a monkey in state 30 / 32, which clears it) and returns 1 when the contact makes the monkey react. */
/* Decode contact and decide whether the monkey should react. */
int32_t ae_MonkeyUpdateContact(AeRam *r) {
    uint32_t current_monkey_global = AE_G_CUR_MONKEY;
    uint32_t type = ae_BodyContactType(r, RD32(current_monkey_global));
    uint32_t contact_type = type;
    uint32_t contact_monkey = RD32(current_monkey_global);
    uint32_t state = RD8(contact_monkey + AE_MK_AI_STATE);
    if (state == 30 || state == 32) {
        if (contact_type == 8) {
            WR16(contact_monkey + AE_MK_KIND, 0);
            return 0;
        }
    }
    uint32_t contact_monkey_after_test = RD32(current_monkey_global);
    WR16(contact_monkey_after_test + AE_MK_KIND, contact_type);
    if (contact_type == 8 || contact_type == 1 || contact_type == 11 || contact_type == 10) return 1;
    if (contact_type != 3) return 0;
    if (RD8(contact_monkey_after_test + AE_MK_AI_STATE) != 2) {
        if (!ae_MonkeyIsIdleActivityState(r)) {
            if ((RD32(RD32(current_monkey_global) + AE_MK_FLAGS2) & 0x8000u) == 0) return 0;
        }
    }
    return RDS16(cur_monkey(r) + AE_MK_TIMER_124) <= 0;
}

/* MonkeyProjectToScreen @800393E4: 1 when the monkey (a point at its feet and one 400 above) is off screen / too far / behind
   the camera, else 0; stores the screen x / y (+0x80 / +0x82).  The GTE calls are hooks (they need a GTE port). */
/* Project the monkey bounds and report whether drawing should be skipped. */
int32_t ae_MonkeyProjectToScreen(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_PROJECT);
    uint32_t monkey = cur_monkey(r);
    uint32_t act = RD8(monkey + AE_MK_ACTIVITY);
    if (act == 0 || act == 2) return 1;
    int32_t d = ae_DistanceIntToShort3D(r, monkey + AE_MK_POS, AE_G_CAMERA_SVEC);
    if (!(d < 6001)) return 1;
    AE_MK_HOOK(r, h, SetRotMatrix, AE_G_VIEW_MATRIX, 0, 0, 0);
    AE_MK_HOOK(r, h, SetTransMatrix, AE_G_VIEW_MATRIX, 0, 0, 0);
    uint32_t screen_xy_out = sp + 24, depth_out = sp + 28, projection_flags_out = sp + 32;
    uint32_t reloaded_monkey = cur_monkey(r);
    WR16(sp + 16, RD16(reloaded_monkey + AE_MK_POS));
    WR16(sp + 18, RD16(reloaded_monkey + AE_MK_POS + 4));
    WR16(sp + 20, RD16(reloaded_monkey + AE_MK_POS + 8));
    int32_t otz = AE_MK_HOOK(r, h, RotTransPers, sp + 16, screen_xy_out, depth_out, projection_flags_out);
    uint32_t projected_monkey = cur_monkey(r);
    uint32_t sx = RD16(sp + 24);
    uint32_t t = RD16(projected_monkey + AE_MK_POS);
    int32_t feet_depth = otz;
    WR16(projected_monkey + AE_MK_SCREEN_X, sx);
    WR16(sp + 16, t);
    t = RD16(projected_monkey + AE_MK_POS + 4);
    int32_t sy = RDS16(sp + 26);
    WR16(sp + 18, t - 400u);
    WR16(sp + 20, RD16(projected_monkey + AE_MK_POS + 8));
    AE_MK_HOOK(r, h, RotTransPers, sp + 16, screen_xy_out, depth_out, projection_flags_out);
    uint32_t in_x = (sx + 50u) & 0xFFFFu;
    reloaded_monkey = cur_monkey(r);
    uint32_t head_screen_y = RD16(sp + 26);
    int32_t inside = in_x < 485u;
    WR16(reloaded_monkey + AE_MK_SCREEN_Y, head_screen_y);
    if (!inside) return 1;
    if (sy < -25) return 1;
    if (!(sy < 266)) return 1;
    if (feet_depth > 0) return 0;
    return 1;
}

/* MonkeyAlertIconKind @80039B90: 0 none, 448 / 896 / byte: which alert icon to show */
/* Choose the alert icon from awareness and sight history. */
int32_t ae_MonkeyAlertIconKind(AeRam *r) {
    uint32_t alerted = RD8(cur_monkey(r) + AE_MK_ALERTED);
    if (alerted == 1) {
        int32_t n = ae_MonkeyNop(r);
        uint32_t lost_sight_count = RD8(cur_monkey(r) + AE_MK_LOST_SIGHT_COUNT);
        if (sub32(n, (int32_t)lost_sight_count) < 2) return 448;
    }
    uint32_t monkey = cur_monkey(r);
    if ((RD32(monkey + AE_MK_FLAGS) >> 19) & 1u) return 448;
    alerted = RD8(monkey + AE_MK_ALERTED);
    if (alerted == 0) return 0;
    if (alerted != 1) return (int32_t)alerted;
    return 896;
}

/* MonkeySelectLod @8003A33C: LOD 0..2 by distance to the view (3 for states 30 / 32); a change re-inits the model. */
/* Choose a model detail level and initialize it when it changes. */
void ae_MonkeySelectLod(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t state = RD8(monkey + AE_MK_AI_STATE);
    uint32_t old_lod = RD8(monkey + AE_MK_MODEL_LOD);
    int32_t new_lod;
    if (state == 30 || state == 32) {
        new_lod = 3;
        WR16(monkey + AE_MK_BODY_HEIGHT, 256);
    } else {
        int32_t dist = ae_DistanceIntToShort3D(r, monkey + AE_MK_POS, AE_G_CAMERA_SVEC);
        uint32_t force_high_detail = RD8(AE_G_LOD_SWITCH);
        if (force_high_detail == 1 || dist < 1000) new_lod = 0;
        else if (dist < 2000) new_lod = 1;
        else new_lod = 2;
    }
    if ((old_lod & 0xFFu) == (uint32_t)new_lod) return;
    uint32_t model = RD32(AE_G_LOD_MODELS + (uint32_t)new_lod * 4u);
    AE_MK_HOOK(r, h, ModelInit, cur_monkey(r), model, 0, 0);
    WR8(cur_monkey(r) + AE_MK_MODEL_LOD, new_lod);
}

/* ================================================================================================
 * Navigation / movement
 * ============================================================================================== */

/* MonkeyPickGoalNode @8003AD9C: next goal node, either along a per-monkey route (+0xD7 routes, +0xD8 current, +0xD9 step,
   +0xDA.. route lengths, +0xE8 visited mask) or random in the node graph. */
/* Choose an unvisited route node or a random graph node. */
void ae_MonkeyPickGoalNode(AeRam *r, const AeMonkeyHooks *h) {
    if (RD8(AE_G_DEBUG_PRINT) != 0) {
        uint32_t monkey = cur_monkey(r);
        AE_MK_HOOK(r, h, Printf, AE_G_MONKEY_GOAL_TEXT, RD8(monkey + AE_MK_ID), RDS16(monkey + AE_MK_GOAL_NODE), RD8(monkey + AE_MK_GROUP));
    }
    ae_MonkeyResetNavVisits(r);
    uint32_t monkey = cur_monkey(r);
    int picked_from_route = 0;
    if (RD8(monkey + AE_MK_GROUP_COUNT) != 0) {
        /* advance along the current group: step + 1, or the next group when the current one is done */
        uint32_t group = RD8(monkey + AE_MK_GROUP);
        uint32_t step = RD8(monkey + AE_MK_GROUP + 1);
        uint32_t group_length = RD8(monkey + group + AE_MK_GROUP_SIZES);
        if (step < group_length) {
            WR8(monkey + AE_MK_GROUP + 1, step + 1u);
        } else {
            WR8(monkey + AE_MK_GROUP + 1, 1);
            monkey = cur_monkey(r);
            int32_t next_group = (int32_t)RD8(monkey + AE_MK_GROUP) + 1;
            int32_t group_count = (int32_t)RD8(monkey + AE_MK_GROUP_COUNT);
            int32_t wrapped = rem_trap(r, next_group, group_count);
            if (ae_ram_failed(r)) return;
            WR8(monkey + AE_MK_GROUP, wrapped);
            WR16(cur_monkey(r) + AE_MK_GROUP_VISIT_MASK, 0);              /* the visited mask restarts */
        }
        monkey = cur_monkey(r);
        group = RD8(monkey + AE_MK_GROUP);
        if (RD8(monkey + group + AE_MK_GROUP_SIZES) != 0) {
            /* a random unvisited node of the group (the first unvisited one when the roll hits a visited one) */
            uint32_t roll = ae_MonkeyRand(r);
            monkey = cur_monkey(r);
            group = RD8(monkey + AE_MK_GROUP);
            int32_t length = (int32_t)RD8(monkey + group + AE_MK_GROUP_SIZES);
            int32_t pick = rem_trap(r, (int32_t)(roll & 0xFFFFu), length);
            if (ae_ram_failed(r)) return;
            int32_t visited = RDS16(monkey + AE_MK_GROUP_VISIT_MASK);
            if ((sra32(visited, (uint32_t)pick) & 1) != 0) {
                int32_t first_unvisited = 0;
                if (length != 0) {
                    for (;;) {
                        if ((sra32(visited, (uint32_t)first_unvisited) & 1) == 0) break;
                        ++first_unvisited;
                        if (!(first_unvisited < length)) break;
                    }
                }
                uint32_t m = cur_monkey(r);
                uint32_t current_group = RD8(m + AE_MK_GROUP);
                uint32_t current_length = RD8(m + current_group + AE_MK_GROUP_SIZES);
                pick = first_unvisited;
                if ((uint32_t)first_unvisited == current_length) pick = 0;
            }
            pick = s16((uint32_t)pick);
            monkey = cur_monkey(r);
            uint32_t route_table = RD32(AE_G_ROUTE_TABLE);
            uint32_t offset = ((uint32_t)pick << 1) + (RD8(monkey + AE_MK_GROUP) << 4) + (RD8(monkey + AE_MK_ID) << 7);
            uint32_t bit = 1u << ((uint32_t)pick & 31u);
            uint32_t visited_mask = RD16(monkey + AE_MK_GROUP_VISIT_MASK);
            uint32_t goal = RD16(route_table + offset + AE_MK_ROUTE_NODES);
            WR16(monkey + AE_MK_GROUP_VISIT_MASK, visited_mask | bit);
            WR16(monkey + AE_MK_GOAL_NODE, goal);
            picked_from_route = 1;
        }
    }
    if (!picked_from_route) {
        /* a random node of the graph */
        uint32_t roll = ae_MonkeyRand(r);
        int32_t count = RDS16(AE_G_NAV_NODE_COUNT);
        int32_t goal = rem_trap(r, (int32_t)(roll & 0xFFFFu), count);
        if (ae_ram_failed(r)) return;
        WR16(cur_monkey(r) + AE_MK_GOAL_NODE, goal);
    }

    /* mark the goal node as visited by this monkey's slot */
    monkey = cur_monkey(r);
    uint32_t goal_node = (uint32_t)RDS16(monkey + AE_MK_GOAL_NODE);
    uint32_t node = goal_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + RD8(monkey + AE_MK_SLOT);
    WR8(node + AE_NAV_NODE_VISIT_MARKS, 1);
    monkey = cur_monkey(r);
    uint32_t debug = RD8(AE_G_DEBUG_PRINT);
    WR16(monkey + AE_MK_NAV_MARK_COUNT, 1);
    if (debug != 0) {
        AE_MK_HOOK(r, h, Printf, AE_G_MONKEY_ROUTE_TEXT, RDS16(monkey + AE_MK_GOAL_NODE), RD8(monkey + AE_MK_GROUP), 0);
    }
}


/* MonkeyLoadLinkExtra @8003B674: loads the extra per-link data (link flags word, wall flags, ...) when the monkey enters
   a new nav link (from -> to), looked up in the sorted table at 0x800B4D1C. */
/* Apply the current link extra flags and collision settings. */
void ae_MonkeyLoadLinkExtra(AeRam *r, int32_t from, int32_t to) {
    int32_t from_node = s16((uint32_t)from), to_node = s16((uint32_t)to);
    uint32_t from_bits = (uint32_t)from, to_bits = (uint32_t)to;
    if (ae_NavNodeLinkFlag(r, from_node, to_node) == 0) return;
    int32_t low_node, high_node;
    if (from_node < to_node) { low_node = from_node; high_node = to_node; } else { high_node = from_node; low_node = to_node; }
    uint32_t monkey = cur_monkey(r);
    if (RDS16(monkey + AE_MK_EXTRA_LINK_FROM) == s16(from_bits) && RDS16(monkey + AE_MK_EXTRA_LINK_TO) == s16(to_bits)) return;
    int32_t count = RDS16(AE_G_NAV_EXTRA_COUNT);
    WR32(monkey + AE_MK_FLAGS2, 0);
    WR16(monkey + AE_MK_EXTRA_LINK_FROM, from_bits);
    WR16(monkey + AE_MK_EXTRA_LINK_TO, to_bits);
    if (count > 0) {
        uint32_t extra_entry = RD32(AE_G_NAV_EXTRA_LINKS);
        int32_t entries_seen = 0;
        for (;;) {
            int32_t entry_node = RDS16(extra_entry);
            if (low_node < entry_node) break;
            if (entry_node == low_node) {
                entry_node = RDS16(extra_entry + AE_NAV_EXTRA_HIGH_NODE);
                if (high_node < entry_node) break;
                if (entry_node == high_node) {
                    WR32(cur_monkey(r) + AE_MK_FLAGS2, RD32(extra_entry + AE_NAV_EXTRA_FLAGS));
                    break;
                }
            }
            ++entries_seen;
            if (!(entries_seen < count)) break;
            extra_entry += 12;
        }
    }
    uint32_t cm = cur_monkey(r);
    uint32_t flags2;
    if (ae_AreaIsBossArena(r) || (RD32(cm + AE_MK_FLAGS2) & 0x02000000u)) {
        cm = cur_monkey(r);
        WR16(cm + AE_MK_BODY_FLAGS, 0xC000);
    } else {
        WR16(cm + AE_MK_BODY_FLAGS, 0x8000);
    }
    cm = cur_monkey(r);
    flags2 = RD32(cm + AE_MK_FLAGS2);
    if (flags2 & 0x200u) WR16(cm + AE_MK_PREV_NODE, from_bits);
    cm = cur_monkey(r);
    uint32_t f = RD32(cm + AE_MK_FLAGS);
    if (RD32(cm + AE_MK_FLAGS2) & 0x2000u) f &= 0xFF7FFFFFu; else f |= 0x800000u;
    WR32(cm + AE_MK_FLAGS, f);
    cm = cur_monkey(r);
    f = RD32(cm + AE_MK_FLAGS);
    if (RD32(cm + AE_MK_FLAGS2) & 0x04000000u) f &= 0xFFFBFFFFu; else f |= 0x40000u;
    WR32(cm + AE_MK_FLAGS, f);
}

/* NavLinkLookupExtra @8003B880: the extra record of a link (from -> to): writes entry[+8] to *out, returns entry[+4]; 0 if none */
/* Find a sorted link record and return its flags and extra value. */
int32_t ae_NavLinkLookupExtra(AeRam *r, int32_t from, int32_t to, uint32_t out) {
    uint32_t from_bits = (uint32_t)from, to_bits = (uint32_t)to;
    uint32_t low_bits = from_bits, high_bits = to_bits;
    int32_t from_node = s16(from_bits), to_node = s16(to_bits);
    if (ae_NavNodeLinkFlag(r, from_node, to_node) == 0) return 0;
    if (to_node < from_node) { high_bits = from_bits; low_bits = to_bits; }
    int32_t count = RDS16(AE_G_NAV_EXTRA_COUNT);
    if (count <= 0) return 0;
    int32_t low_node = s16(low_bits), high_node = s16(high_bits);
    uint32_t extra_entry = RD32(AE_G_NAV_EXTRA_LINKS);
    uint32_t entry_offset = 0;
    for (int32_t i = 0; i < count; ++i) {
        int32_t entry_node = RDS16(extra_entry);
        if (low_node < entry_node) return 0;
        if (entry_node == low_node) {
            entry_node = RDS16(extra_entry + AE_NAV_EXTRA_HIGH_NODE);
            if (high_node < entry_node) return 0;
            if (entry_node == high_node) {
                WR32(out, RD32(extra_entry + AE_NAV_EXTRA_VALUE));
                return (int32_t)RD32(entry_offset + RD32(AE_G_NAV_EXTRA_LINKS) + AE_NAV_EXTRA_FLAGS);
            }
        }
        extra_entry += 12;
        entry_offset += 12;
    }
    return 0;
}

/* the spark burst of a hop step: position at +16, velocity = -moveDir / 16 (x, z) at +24 */
/* Spawn sparks opposite the current movement direction. */
static void move_step_spark_burst(AeRam *r, const AeMonkeyHooks *h, uint32_t sp) {
    uint32_t velocity = sp + 24, position = sp + 16;
    for (uint32_t i = 0; i < 8; ++i) WR8(velocity + i, 0);                         /* memset(sp + 24, 0, 8) */
    uint32_t monkey = cur_monkey(r);
    WR16(sp + 24, RD16(monkey + AE_MK_POS));
    WR16(sp + 26, RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 28, RD16(monkey + AE_MK_POS + 8));
    uint32_t pos_xy = RD32(sp + 24), pos_z = RD32(sp + 28);
    WR32(sp + 16, pos_xy);
    WR32(sp + 20, pos_z);
    for (uint32_t i = 0; i < 8; ++i) WR8(sp + 32 + i, 0);                    /* memset(sp + 32, 0, 8) */
    monkey = cur_monkey(r);
    WR16(sp + 32, neg32(RDS16(monkey + AE_MK_MOVE_DIR)) / 16);
    WR16(sp + 36, neg32(RDS16(monkey + AE_MK_MOVE_DIR + 4)) / 16);
    uint32_t vel_xy = RD32(sp + 32);
    uint32_t vel_z = RD32(sp + 36);
    WR32(sp + 24, vel_xy);
    WR32(sp + 28, vel_z);
    AE_MK_HOOK(r, h, SparkAddRandom, position, velocity, 0, 0);
}

/* the speed wobble of the states 0x1E / 0x20: 240 + 50 sin(6 * frame counter) / 4096 */
/* Compute the periodic speed variation for flying states. */
static int32_t move_step_wobble_speed(AeRam *r) {
    uint32_t frame = RD32(AE_G_FRAME_COUNTER);
    int32_t sine = ae_sdk_rsin(r, (int32_t)(frame * 6u));
    return add32(mul_lo(sine, 50) / 4096, 240);
}

/* the speed of the walking states (outside hops and states 61 / 62), before the final scaling */
/* Choose and scale movement speed for the current walking state. */
static int32_t move_step_walk_speed(AeRam *r) {
    int32_t speed;
    if (ae_AreaIsBossArena(r)) {
        speed = RDS16(info_of(r, cur_monkey(r)) + AE_MK_INFO_ALERT_SPEED);
    } else {
        uint32_t monkey = cur_monkey(r);
        uint32_t state = RD8(monkey + AE_MK_AI_STATE);
        if (state == 30) {
            speed = 360;
            if ((RD32(monkey + AE_MK_FLAGS3) >> 2) & 1u) {
                /* stays 360 */
            } else if (RD8(monkey + AE_MK_FLAGS + 3) & 1u) {
                int32_t base = RDS16(info_of(r, monkey) + AE_MK_INFO_ALERT_SPEED);
                speed = mul_lo(base, 120) / 100;
            } else if (RDS32(monkey + AE_MK_STATE_TIMER) < 180) {
                speed = RDS16(info_of(r, monkey) + AE_MK_INFO_ALERT_SPEED) / 10;
            } else {
                speed = move_step_wobble_speed(r);
            }
        } else if (state == 32) {
            speed = move_step_wobble_speed(r);
        } else if (state == 26 || state == 53) {
            if (ae_AreaIsBossArena(r)) {
                speed = shift_left32(RDS16(info_of(r, cur_monkey(r)) + AE_MK_INFO_ALERT_SPEED), 1);
            } else {
                speed = mul_lo(RDS16(info_of(r, cur_monkey(r)) + AE_MK_INFO_WALK_SPEED), 80) / 100;
            }
        } else if (RD8(monkey + AE_MK_ALERTED) == 1) {
            /* alerted: info speed (+4) + half of it * cos of the distance to the target (0 .. 600 -> 0 .. a quarter turn) */
            int32_t dist = RDS32(monkey + AE_MK_DIST_TO_TARGET);
            if (!(dist < 601)) dist = 600;
            int32_t angle = shift_left32(dist, 12) / 2400;
            int32_t cosine = ae_sdk_rcos(r, angle);
            if (cosine < 0) cosine = 0;
            int32_t base = s16(RD16(info_of(r, cur_monkey(r)) + AE_MK_INFO_ALERT_SPEED));
            speed = add32(base, mul_lo(base / 2, cosine) / 4096);
        } else if (RD8(monkey + AE_MK_ALERTED) != 0) {
            speed = 0;
        } else {
            speed = RDS16(info_of(r, monkey) + AE_MK_INFO_WALK_SPEED);
        }
    }

    /* fast ground states double the info speed, state 9 halves it; the counters at +0x120 / +0x94 scale it */
    if (ae_MonkeyIsState0C_or_24(r)) {
        speed = shift_left32(RDS16(info_of(r, cur_monkey(r)) + AE_MK_INFO_WALK_SPEED), 1);
    } else {
        uint32_t monkey = cur_monkey(r);
        if (RD8(monkey + AE_MK_AI_STATE) == 9) speed = s16(RD16(info_of(r, monkey) + AE_MK_INFO_ALERT_SPEED)) / 2;
    }
    uint32_t monkey = cur_monkey(r);
    if (RDS16(monkey + AE_MK_LINK_FRAME) > 0) {
        speed = mul_lo(speed, 40) / 100;
    } else if (RDS16(monkey + AE_MK_ACTION_COOLDOWN) > 0) {
        if (RD8(monkey + AE_MK_ALERTED) == 1) speed = mul_lo(speed, 85) / 100;
        else speed = speed / 5;
    }
    return speed;
}

/* MonkeyMoveStep @8003B9C0: the speed of this frame (from the MonkeyInfo record, the state and some flags) scales the move
   direction (moveDir.xyz = moveDir.xyz * speed / 32768, rounded toward zero) and is stored at +0x220 (scaled by 74 / 100 for the
   ground states 3 / 8 / 12 / 36).  With flags3 bit 0 set (a hop) the speed is a function of the step counter and a spark
   burst is spawned. */
/* Compute state-dependent speed and scale the wish direction into velocity. */
void ae_MonkeyMoveStep(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_MOVE_STEP);
    int32_t speed = 0;
    if (ae_MonkeyIsJumpArcState(r)) {
        WR16(cur_monkey(r) + AE_MK_REACH_RADIUS, 50);
        return;
    }
    uint32_t monkey = cur_monkey(r);
    if (RD32(monkey + AE_MK_FLAGS3) & 1u) {
        /* hop: the speed follows the step counter */
        uint32_t counter = RD16(monkey + AE_MK_COUNTER96) + 1u;
        uint32_t duration = RD16(monkey + AE_MK_ANIM_LENGTH);
        WR16(monkey + AE_MK_COUNTER96, counter);
        int32_t step = s16(counter);
        if (step < (int32_t)duration) {
            int32_t remaining = sub32((int32_t)duration, step);
            int32_t remaining_sq = mul_lo(remaining, remaining);
            int32_t duration_sq = mul_lo((int32_t)duration, (int32_t)duration);
            int32_t base = RDS16(info_of(r, monkey) + AE_MK_INFO_ALERT_SPEED);
            int32_t boost = div_trap(r, mul_lo(base, shift_left32(remaining_sq, 2)), duration_sq);
            if (ae_ram_failed(r)) return;
            speed = add32(base, boost);
        } else {
            speed = RDS16(info_of(r, monkey) + AE_MK_INFO_ALERT_SPEED);
            uint32_t flags3 = RD32(monkey + AE_MK_FLAGS3);
            WR16(monkey + AE_MK_ANIM_LENGTH, 0);
            WR16(monkey + AE_MK_COUNTER96, 0);
            WR32(monkey + AE_MK_FLAGS3, flags3 & 0xFFFFFFFEu);
        }
        move_step_spark_burst(r, h, sp);
    } else {
        uint32_t state = RD8(monkey + AE_MK_AI_STATE);
        if (state == 61) {
            int32_t base = RDS16(info_of(r, monkey) + AE_MK_INFO_ALERT_SPEED);
            int32_t counter = RDS16(monkey + AE_MK_COUNTER96);
            int32_t limit = shift_left32(add32(shift_left32(base, 1), base), 1);                    /* 6 * base */
            speed = limit;
            int32_t ramp = add32(shift_left32(base, 1), add32(shift_left32(counter, 1), counter));  /* 2 * base + 3 * counter */
            if (ramp < speed) speed = ramp;
        } else if (state == 62) {
            int32_t elapsed = RDS16(monkey + AE_MK_COUNTER96) - 10;
            int32_t base = RDS16(info_of(r, monkey) + AE_MK_INFO_ALERT_SPEED);
            uint32_t duration = RD16(monkey + AE_MK_ANIM_LENGTH);
            int32_t remaining = sub32((int32_t)duration, elapsed);
            speed = div_trap(r, mul_lo(remaining, base), (int32_t)duration);
            if (ae_ram_failed(r)) return;
        } else {
            speed = move_step_walk_speed(r);
        }
    }

    /* the move direction takes the wish direction (+0x68 / +0x6C), state 9 stands still; then moveDir * speed / 32768 */
    monkey = cur_monkey(r);
    uint32_t wish_xy = RD32(monkey + AE_MK_WISH_DIR), wish_z = RD32(monkey + AE_MK_WISH_DIR + 4);
    WR32(monkey + AE_MK_VEL, wish_xy);
    WR32(monkey + AE_MK_VEL + 4, wish_z);
    if (RD8(monkey + AE_MK_AI_STATE) == 9) {
        WR16(monkey + AE_MK_VEL + 4, 0);
        WR16(monkey + AE_MK_VEL, 0);
    }
    monkey = cur_monkey(r);
    int32_t scaled_x = mul_lo(RDS16(monkey + AE_MK_VEL), speed);
    int32_t scaled_y = mul_lo(RDS16(monkey + AE_MK_VEL + 2), speed);
    WR16(monkey + AE_MK_VEL, scaled_x / 32768);
    int32_t scaled_z = mul_lo(RDS16(monkey + AE_MK_VEL + 4), speed);
    WR16(monkey + AE_MK_VEL + 2, scaled_y / 32768);
    uint32_t state = RD8(monkey + AE_MK_AI_STATE);
    WR16(monkey + AE_MK_VEL + 4, scaled_z / 32768);
    if (state == 3 || state == 8 || state == 12 || state == 36) {
        WR16(cur_monkey(r) + AE_MK_REACH_RADIUS, mul_lo(speed, 74) / 100);
    } else {
        WR16(monkey + AE_MK_REACH_RADIUS, speed);
    }
}

/* MonkeyTurnTowardTarget @8003CC3C: if the move direction is more than rate + 45 (in 1/4096 turns) off the direction to the
   target, rotate it by +-rate around Y. */
/* Rotate movement toward the target when the angular difference warrants it. */
void ae_MonkeyTurnTowardTarget(AeRam *r, int32_t rate) {
    uint32_t sp = AE_STK(AE_STK_TURN);
    int32_t turn_rate = rate;
    uint32_t monkey = cur_monkey(r);
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    uint32_t flags = RD32(monkey + AE_MK_FLAGS);
    int32_t delta_z = (int32_t)RD32(monkey + AE_MK_POS + 8);
    WR32(monkey + AE_MK_FLAGS, flags | 0x02000000u);
    int32_t target_z = (int32_t)RD32(target + AE_MK_POS + 8);
    int32_t tx = (int32_t)RD32(target + AE_MK_POS);
    int32_t cx = (int32_t)RD32(monkey + AE_MK_POS);
    delta_z = sub32(target_z, delta_z);
    int32_t ang1 = ae_Atan2Angle4096(r, delta_z, sub32(tx, cx));
    if (ae_ram_failed(r)) return;
    int32_t target_angle = neg32(ang1);
    monkey = cur_monkey(r);
    int32_t ang2 = ae_Atan2Angle4096(r, RDS16(monkey + AE_MK_MOVE_DIR + 4), RDS16(monkey + AE_MK_MOVE_DIR));
    if (ae_ram_failed(r)) return;
    int32_t d = sub32(neg32(ang2), target_angle);
    int32_t ad = d < 0 ? neg32(d) : d;
    if (ad < add32(turn_rate, 45)) return;
    if (d >= 0) {
        WR16(sp + 16, 0);
        WR16(sp + 18, neg32(turn_rate));
    } else {
        WR16(sp + 16, 0);
        WR16(sp + 18, turn_rate);
    }
    WR16(sp + 20, 0);
    ae_sdk_RotMatrixYXZ(r, sp + 16, sp + 24);
    uint32_t pm = cur_monkey(r) + AE_MK_MOVE_DIR;
    ae_sdk_ApplyMatrixSV(r, sp + 24, pm, pm);
}

/* MonkeyLineOfSight @800401F4: samples the segment monkey -> target (probe position of the target) in 362 unit steps; 0 when
   the ground drops away along it (the sampled floor is > 200 / 50 below the line), 1 otherwise or when closer than 250.
   mode == 1 re-uses the cached result in flags bit 28 on alternate frames. */
/* Probe ground height along the target segment to test visibility. */
int32_t ae_MonkeyLineOfSight(AeRam *r, const AeMonkeyHooks *h, int32_t mode) {
    uint32_t sp = AE_STK(AE_STK_LINE_OF_SIGHT);
    uint32_t monkey = cur_monkey(r);
    if (RDS32(monkey + AE_MK_DIST_TO_TARGET) < 250) return 1;
    if (mode == 1) {
        uint32_t monkey_id = RD8(monkey + AE_MK_ID);
        uint32_t sight_frame = RD32(AE_G_FRAME_COUNTER) + monkey_id;
        if ((sight_frame & 1u) == 0) return (int32_t)((RD32(monkey + AE_MK_FLAGS) >> 28) & 1u);
    }
    if (RD8(AE_G_NAV_AREA_FLAG) == 0) {
        copy_svec_at_1c(r, RD32(AE_G_MONKEY_TARGET), sp + 16);
    } else {
        uint32_t target = RD32(AE_G_MONKEY_TARGET);
        WR16(sp + 16, RD16(target + AE_MK_POS));
        WR16(sp + 18, RD16(target + AE_MK_POS + 4));
        WR16(sp + 20, RD16(target + AE_MK_POS + 8));
    }
    AE_MK_HOOK(r, h, SetRotMatrix, AE_G_VIEW_MATRIX, 0, 0, 0);
    AE_MK_HOOK(r, h, SetTransMatrix, AE_G_VIEW_MATRIX, 0, 0, 0);
    int32_t distance = ae_DistanceIntToShort3D(r, cur_monkey(r) + AE_MK_POS, sp + 16);
    if (distance == 0) return 1;
    uint32_t origin_monkey = cur_monkey(r);
    int32_t target_x = RDS16(sp + 16);
    int32_t target_y = RDS16(sp + 18);
    int32_t origin_y = sub32((int32_t)RD32(origin_monkey + AE_MK_POS + 4), 125);
    WR32(sp + 40, origin_y);
    int32_t target_z = RDS16(sp + 20);
    int32_t origin_x = (int32_t)RD32(origin_monkey + AE_MK_POS);
    WR32(sp + 36, origin_x);
    int32_t monkey_z = (int32_t)RD32(origin_monkey + AE_MK_POS + 8);
    if (target_x == origin_x && target_z == monkey_z) return 1;
    monkey = cur_monkey(r);
    uint32_t target_component = (uint32_t)RDS16(sp + 20);
    int32_t scaled_component = sub32((int32_t)RD32(monkey + AE_MK_POS + 8), (int32_t)target_component);
    scaled_component = (int32_t)((uint32_t)scaled_component << 12);
    int32_t step_component = div_trap(r, scaled_component, distance);
    WR32(sp + 28, step_component);
    int32_t origin_component = (int32_t)RD32(sp + 40);
    int32_t delta_y = sub32(origin_component, target_y);
    scaled_component = delta_y < 0 ? neg32(delta_y) : delta_y;
    WR8(sp + 32, scaled_component < 100);
    scaled_component = sub32(monkey_z, target_z);
    origin_component = (int32_t)RD32(sp + 36);
    if (scaled_component < 0) scaled_component = neg32(scaled_component);
    uint32_t abs_delta_x = (uint32_t)sub32(origin_component, target_x);
    if ((int32_t)abs_delta_x < 0) abs_delta_x = (uint32_t)neg32((int32_t)abs_delta_x);
    WR8(sp + 33, scaled_component < (int32_t)abs_delta_x);
    target_component = (uint32_t)RDS16(sp + 16);
    int32_t sample_index = 1;
    scaled_component = (int32_t)RD32(monkey + AE_MK_POS);
    int32_t height_delta = delta_y;
    scaled_component = sub32(scaled_component, (int32_t)target_component);
    scaled_component = (int32_t)((uint32_t)scaled_component << 12);
    step_component = div_trap(r, scaled_component, distance);
    WR32(sp + 24, step_component);
    if (ae_ram_failed(r)) return 0;
    for (;;) {
        int32_t sample = s16((uint32_t)sample_index);
        int32_t q = ae_sdk_SquareRoot0(r, 0x20000);
        int32_t lim = div_trap(r, distance, q);
        if (ae_ram_failed(r)) return 0;
        if (!(sample < lim)) return 1;
        int32_t sq = ae_sdk_SquareRoot0(r, 0x20000);
        int32_t t = mul_lo(sq, sample);
        int32_t step_x = (int32_t)RD32(sp + 24);
        scaled_component = mul_lo(t, step_x);
        if (scaled_component < 0) scaled_component = add32(scaled_component, 4095);
        scaled_component = sra32(scaled_component, 12);
        int32_t sample_x = add32(target_x, scaled_component);
        sq = ae_sdk_SquareRoot0(r, 0x20000);
        t = mul_lo(sq, sample);
        int32_t step_z = (int32_t)RD32(sp + 28);
        scaled_component = mul_lo(t, step_z);
        if (scaled_component < 0) scaled_component = add32(scaled_component, 4095);
        scaled_component = sra32(scaled_component, 12);
        int32_t sample_z = add32(target_z, scaled_component);
        if (RD8(sp + 32) != 0) {
            int32_t y = (int32_t)RD32(sp + 40);
            int32_t ground_height = AE_MK_HOOK(r, h, GetGroundHeight, sample_x, y, sample_z, 0);
            if (ae_ram_failed(r)) return 0;
            if (sub32(ground_height, target_y) < -200) return 0;
            ++sample_index;
            continue;
        }
        int32_t height_numerator, axis_span;
        if (RD8(sp + 33) != 0) {
            scaled_component = sub32(sample_x, target_x);
            t = mul_lo(height_delta, scaled_component);
            int32_t mx = (int32_t)RD32(sp + 36);
            axis_span = sub32(mx, target_x);
            height_numerator = t;
        } else {
            scaled_component = sub32(sample_z, target_z);
            t = mul_lo(height_delta, scaled_component);
            axis_span = sub32(monkey_z, target_z);
            height_numerator = t;
        }
        int32_t interpolated_height = div_trap(r, height_numerator, axis_span);
        if (ae_ram_failed(r)) return 0;
        int32_t sample_y = s16((uint32_t)add32(target_y, interpolated_height));
        int32_t ground_height = AE_MK_HOOK(r, h, GetGroundHeight, sample_x, sample_y, sample_z, 0);
        if (ae_ram_failed(r)) return 0;
        if (sub32(ground_height, sample_y) < -50) return 0;
        ++sample_index;
    }
}
