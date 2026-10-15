/*
 * Collision-body system, translated from the original MIPS code (see ae_bodies.h).
 * Each function carries its original name and address.  Where the Ghidra pseudocode and the
 * instruction listing differ (hidden arguments, register reuse), the listing was followed.
 * Integer widths follow the load/store instructions: lh / lhu / lw, sh / sw.
 */
#include "ae_bodies.h"
#include "ae_bodymove.h"
#include "ae_probe.h"

#include "../player/native_player.h"   /* ae_player_vec_length3 (VecLength3 @80046618) */

#include <stddef.h>

/* sticky error + sanity cap for list walks (the slot table has 256 entries) */
#define WALK_CAP 4096
#define PUSH_CAP 4000000

/* CollisionUpdate body and player fields used by its inline type-1/type-2 movement cases. */
#define AE_CU_PLAYER_POSITION_X       0x24u
#define AE_CU_PLAYER_POSITION_Y       0x28u
#define AE_CU_PLAYER_POSITION_Z       0x2Cu
#define AE_CU_PLAYER_VELOCITY_X       0x30u
#define AE_CU_PLAYER_VELOCITY_Y       0x34u
#define AE_CU_PLAYER_VELOCITY_Z       0x38u
#define AE_CU_PLAYER_STATE            0x5Eu
#define AE_CU_PLAYER_HEALTH           0xE8u
#define AE_CU_BODY_POSITION_X         0x00u
#define AE_CU_BODY_POSITION_Y         0x04u
#define AE_CU_BODY_POSITION_Z         0x08u
#define AE_CU_BODY_VELOCITY_X         0x0Cu
#define AE_CU_BODY_VELOCITY_Y         0x10u
#define AE_CU_BODY_VELOCITY_Z         0x14u
#define AE_CU_BODY_SOLID_COUNT        0x2Au
#define AE_CU_BODY_CONTACT_COLLIDER   0x34u
#define AE_CU_COLLIDER_SIZE           0x38u
#define AE_CU_COLLIDER_CONTACT_INDEX  0x1Eu
#define AE_CU_SNAPSHOT_TAIL_SIZE      0x08u
#define AE_CU_SNAPSHOT_PREFIX_SIZE    (AE_BODY_SIZE - AE_CU_SNAPSHOT_TAIL_SIZE)
#define AE_CU_SNAPSHOT_TAIL_OFFSET    AE_CU_SNAPSHOT_PREFIX_SIZE

/* ---- field access helpers --------------------------------------------------------------------- */
#define SLOT_TYPE(s) ae_rds16(r, (s) + 0)
#define SLOT_TYPE_U(s) ae_rd16(r, (s) + 0)
#define SLOT_IDX(s) ae_rds16(r, (s) + 2)
#define SLOT_IDX_U(s) ae_rd16(r, (s) + 2)
#define SLOT_BODY(s) ae_rd32(r, (s) + 4)
#define SLOT_OWNER(s) ae_rd32(r, (s) + 8)
#define SLOT_NEXT(s) ae_rd32(r, (s) + 0xC)
#define SLOT_PREV(s) ae_rd32(r, (s) + 0x10)

#define BODY_CONTACT(b) ((b) + 0x2C)
#define OWNER_CONTACT(o) ((o) + 0x50)

#define SCR_V 0x1F8000F0u    /* difference vector (3 x s32) */
#define SCR_N 0x1F800100u    /* normal / second centre (3 x s16) */
#define SCR_C_A 0x1F800108u  /* centre of body A / ray parameters */
#define SCR_C_B 0x1F800110u  /* centre of body B */

static uint32_t slot_addr(uint32_t i) { return AE_COLLISION_SLOTS + i * AE_COLLISION_SLOT_SIZE; }

/* "halve with round toward zero then abs", the (h + (h < 0)) >> 1 idiom on an s16 height */
static int32_t half(int32_t h16) { return (h16 + (h16 < 0 ? 1 : 0)) >> 1; }
static int32_t half_abs(int32_t h16) { int32_t v = half(h16); return v < 0 ? -v : v; }

/* MIPS addu / subu semantics (wraparound) for 32-bit positions */
static int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

static int32_t vec_length3(int32_t x, int32_t y, int32_t z) { return ae_player_vec_length3(x, y, z); }

static int missing_hook(AeRam *r, const char *name) {
    ae_ram_fail(r, "AeBodyHooks.%s not provided", name);
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * CollisionBodiesReset @80071D3C
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_CollisionBodiesReset(AeRam *r) {
    ae_wr32(r, AE_G_COLLISION_BODIES, 0);
    ae_wr32(r, AE_G_COLLISION_TAIL, 0);
    for (uint32_t i = 0; i < AE_COLLISION_SLOT_COUNT; ++i) {
        uint32_t s = slot_addr(i);
        ae_wr16(r, s + 0, 0xFFFF);
        ae_wr16(r, s + 2, 0xFFFF);
        ae_wr32(r, s + 4, 0);
        ae_wr32(r, s + 8, 0);
        ae_wr32(r, s + 0xC, 0);
        ae_wr32(r, s + 0x10, 0);
    }
    for (uint32_t i = 0; i < AE_BODY_POOL_COUNT; ++i) {
        ae_wr16(r, AE_BODY_POOL_FREE + i * 2, 0xFFFF);
        ae_CollisionBodyClear(r, AE_BODY_POOL + i * AE_BODY_SIZE);
    }
    for (uint32_t i = 0; i < AE_LOOP_SLOT_COUNT; ++i) {
        uint32_t e = AE_LOOP_SLOTS + i * AE_LOOP_SLOT_SIZE;
        ae_wr16(r, e + 0, 0xFFFF);
        ae_wr16(r, e + 2, 0);
        ae_wr16(r, e + 8, 0);
        ae_wr32(r, e + 4, 0);
    }
    ae_wr16(r, AE_G_COLL_WALL_PROBE, 0);
    ae_wr16(r, AE_G_COLL_PROBE_E8, 0);
    ae_wr16(r, AE_G_IGNORE_COLLIDER, 0xFFFF);
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * CollisionBodyRegister @80071E44
 * Returns 0, or -1 when no slot is free (after printf(" Collision_set Error %d\n", type)).
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_CollisionBodyRegister(AeRam *r, const AeBodyHooks *h, int32_t type, uint32_t body, uint32_t owner) {
    uint32_t s = ae_CollisionBodyAllocSlot(r);
    if (s == 0) {
        if (h && h->printf_error) h->printf_error(h->user, r, 0x800A5D20u, type);
        else missing_hook(r, "printf_error");
        return -1;
    }
    ae_wr16(r, s + 0, (uint32_t)type);
    int32_t t = SLOT_TYPE(s);
    ae_wr32(r, s + 8, owner);
    ae_wr32(r, s + 4, body);
    if ((t == 1 || t == 3 || t == 4 || t == 5) && owner != 0)
        ae_wr16(r, SLOT_OWNER(s) + 0x4C, SLOT_IDX_U(s));
    t = SLOT_TYPE(s);
    if (t == 3) {
        uint32_t b = SLOT_BODY(s);
        ae_wr16(r, b + 0x26, ae_rd16(r, b + 0x26) | 0x8000);
        t = SLOT_TYPE(s);
    }
    if (t == 2) {
        if (owner != 0) ae_wr16(r, SLOT_OWNER(s) + 0x4C, SLOT_IDX_U(s));
        int32_t handle = 0;
        if (h && h->sound_loop_start) handle = h->sound_loop_start(h->user, r, 1, 0x16);
        else missing_hook(r, "sound_loop_start");
        ae_wr16(r, AE_G_LOOP_SOUND, (uint32_t)handle);
        ae_wr16(r, AE_G_LOOP_SOUND + 2, 1);
    }
    ae_wr16(r, SLOT_BODY(s) + 0x2A, 0);
    uint32_t tu = SLOT_TYPE_U(s);
    if ((uint32_t)(tu - 6) < 3u || (uint32_t)(tu - 12) < 6u) ae_wr16(r, SLOT_BODY(s) + 0x2A, 1);
    uint32_t tail = ae_rd32(r, AE_G_COLLISION_TAIL);
    if (ae_rd32(r, AE_G_COLLISION_BODIES) == 0) {
        ae_wr32(r, AE_G_COLLISION_BODIES, s);
        ae_wr32(r, AE_G_COLLISION_TAIL, s);
        ae_wr32(r, s + 0xC, 0);
        ae_wr32(r, s + 0x10, 0);
    } else {
        ae_wr32(r, AE_G_COLLISION_TAIL, s);
        ae_wr32(r, tail + 0xC, s);
        ae_wr32(r, s + 0xC, 0);
        ae_wr32(r, s + 0x10, tail);
    }
    return 0;
}

/* CollisionBodyAllocSlot @80071FDC: first slot with type >= 0 clear (type < 0 = free); stores its index at +2. */
uint32_t ae_CollisionBodyAllocSlot(AeRam *r) {
    for (uint32_t i = 0; i < AE_COLLISION_SLOT_COUNT; ++i) {
        uint32_t s = slot_addr(i);
        if (SLOT_TYPE(s) < 0) {
            ae_wr16(r, s + 2, i);
            return s;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * CollisionBodyUnregister @80072018 (returns 0, or -1 when the body has no slot)
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_CollisionBodyUnregister(AeRam *r, const AeBodyHooks *h, uint32_t body) {
    uint32_t s = 0;
    for (uint32_t i = 0; i < AE_COLLISION_SLOT_COUNT; ++i) {
        uint32_t c = slot_addr(i);
        if (SLOT_BODY(c) == body) { s = c; break; }
    }
    if (s == 0) return -1;
    if (SLOT_TYPE(s) == 2) {
        ae_wr16(r, AE_G_RC_LOOP_FLAG, 0xFFFF);
        if (ae_rds16(r, AE_G_LOOP_SOUND + 2) != 0) {
            int32_t handle = ae_rds16(r, AE_G_LOOP_SOUND);
            if (h && h->sound_loop_stop) h->sound_loop_stop(h->user, r, 0x16, handle);
            else missing_hook(r, "sound_loop_stop");
            ae_wr16(r, AE_G_LOOP_SOUND + 2, 0);
        }
    }
    uint32_t head = ae_rd32(r, AE_G_COLLISION_BODIES);
    ae_wr16(r, s + 0, 0xFFFF);
    ae_wr16(r, s + 2, 0xFFFF);
    ae_wr32(r, s + 8, 0);
    ae_wr32(r, s + 4, 0);
    if (head == s) {
        uint32_t next = SLOT_NEXT(s);
        if (next == 0) {
            ae_wr32(r, AE_G_COLLISION_BODIES, 0);
            ae_wr32(r, AE_G_COLLISION_TAIL, 0);
        } else {
            ae_wr32(r, AE_G_COLLISION_BODIES, next);
            ae_wr32(r, SLOT_NEXT(s) + 0x10, 0);
        }
    } else {
        if (ae_rd32(r, AE_G_COLLISION_TAIL) == s) ae_wr32(r, AE_G_COLLISION_TAIL, SLOT_PREV(s));
        uint32_t next = SLOT_NEXT(s);
        if (next == 0) {
            ae_wr32(r, SLOT_PREV(s) + 0xC, 0);
        } else {
            ae_wr32(r, SLOT_PREV(s) + 0xC, next);
            ae_wr32(r, SLOT_NEXT(s) + 0x10, SLOT_PREV(s));
        }
    }
    ae_wr32(r, s + 0xC, 0);
    ae_wr32(r, s + 0x10, 0);
    return 0;
}

/* GetObjectByIndex @80072170: slot[(s16)index].body for index < 0x101 (u16 compare), else 0. */
uint32_t ae_GetObjectByIndex(AeRam *r, uint32_t index) {
    if ((index & 0xFFFF) < 0x101u) {
        int32_t i = (int16_t)(uint16_t)index;
        return ae_rd32(r, AE_COLLISION_SLOTS + (uint32_t)(i * 20) + 4);
    }
    return 0;
}

/* CollisionBodyIndexOf @800721B4: owner+0x4C of the slot (when it has an owner) used as the slot index; type or -2. */
int32_t ae_CollisionBodyIndexOf(AeRam *r, int32_t slot) {
    int32_t i = (int16_t)slot;
    uint32_t owner = ae_rd32(r, AE_COLLISION_SLOTS + (uint32_t)(i * 20) + 8);
    if (owner != 0) i = ae_rds16(r, owner + 0x4C);
    if ((uint32_t)i < 0x101u) return ae_rds16(r, AE_COLLISION_SLOTS + (uint32_t)(i * 20));
    return -2;
}

/* CollisionBodyAlloc @80072220: first free pool body (flag < 0): flag = 1, body cleared. 0 when none. */
uint32_t ae_CollisionBodyAlloc(AeRam *r) {
    for (uint32_t i = 0; i < AE_BODY_POOL_COUNT; ++i) {
        uint32_t flag = AE_BODY_POOL_FREE + i * 2;
        if (ae_rds16(r, flag) < 0) {
            ae_wr16(r, flag, 1);
            uint32_t body = AE_BODY_POOL + i * AE_BODY_SIZE;
            ae_CollisionBodyClear(r, body);
            return body;
        }
    }
    return 0;
}

/* CollisionBodyClear @8007228C: zeroes pos, vel, +0x20/22/24/26/2A, contact, +0x30 and facing. Returns 0. */
int32_t ae_CollisionBodyClear(AeRam *r, uint32_t b) {
    ae_wr32(r, b + 0x00, 0);
    ae_wr32(r, b + 0x04, 0);
    ae_wr32(r, b + 0x08, 0);
    ae_wr32(r, b + 0x0C, 0);
    ae_wr32(r, b + 0x10, 0);
    ae_wr32(r, b + 0x14, 0);
    ae_wr16(r, b + 0x20, 0);
    ae_wr16(r, b + 0x22, 0);
    ae_wr16(r, b + 0x24, 0);
    ae_wr16(r, b + 0x26, 0);
    ae_wr16(r, b + 0x2A, 0);
    ae_wr32(r, b + 0x2C, 0);
    ae_wr32(r, b + 0x30, 0);
    ae_wr16(r, b + 0x1C, 0);
    ae_wr16(r, b + 0x1A, 0);
    ae_wr16(r, b + 0x18, 0);
    return 0;
}

/* CollisionBodyFree @800722D4: marks the pool body free (0xFFFF) and clears it. 0 found, -1 not a pool body. */
int32_t ae_CollisionBodyFree(AeRam *r, uint32_t body) {
    for (uint32_t i = 0; i < AE_BODY_POOL_COUNT; ++i) {
        uint32_t b = AE_BODY_POOL + i * AE_BODY_SIZE;
        if (b == body) {
            ae_wr16(r, AE_BODY_POOL_FREE + i * 2, 0xFFFF);
            ae_CollisionBodyClear(r, b);
            return 0;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------------------------------------
 * Loop slot table @0x800BEEA0: 16 entries {s16 id (<0 free), s16 age, u32 key, s16 refs}
 * ---------------------------------------------------------------------------------------------- */
/* LoopSlotAdd @80072334 */
int32_t ae_LoopSlotAdd(AeRam *r, int32_t id, uint32_t key) {
    for (uint32_t i = 0; i < AE_LOOP_SLOT_COUNT; ++i) {
        uint32_t e = AE_LOOP_SLOTS + i * AE_LOOP_SLOT_SIZE;
        if (ae_rds16(r, e) < 0) {
            ae_wr16(r, e + 0, (uint32_t)id);
            ae_wr16(r, e + 2, 0);
            ae_wr16(r, e + 8, 1);
            ae_wr32(r, e + 4, key);
            return 0;
        }
    }
    return -1;
}

/* LoopSlotRef @80072380 (id is an s16 argument) */
int32_t ae_LoopSlotRef(AeRam *r, int32_t id, uint32_t key) {
    int32_t a0 = (int16_t)id;
    for (uint32_t i = 0; i < AE_LOOP_SLOT_COUNT; ++i) {
        uint32_t e = AE_LOOP_SLOTS + i * AE_LOOP_SLOT_SIZE;
        int32_t v = ae_rds16(r, e);
        if (v < 0 || a0 != v) continue;
        if (key != ae_rd32(r, e + 4)) continue;
        ae_wr16(r, e + 8, ae_rd16(r, e + 8) + 1);
        return 0;
    }
    return 1;
}

/* LoopSlotSweep @800723E8 */
int32_t ae_LoopSlotSweep(AeRam *r) {
    for (uint32_t i = 0; i < AE_LOOP_SLOT_COUNT; ++i) {
        uint32_t e = AE_LOOP_SLOTS + i * AE_LOOP_SLOT_SIZE;
        if (ae_rds16(r, e) >= 0) {
            if (ae_rds16(r, e + 8) == 0) {
                ae_wr16(r, e + 0, 0xFFFF);
                ae_wr16(r, e + 2, 0);
                ae_wr16(r, e + 8, 0);
                ae_wr32(r, e + 4, 0);
            } else {
                ae_wr16(r, e + 2, ae_rd16(r, e + 2) + 1);
            }
        }
    }
    return 0;
}

/* LoopSlotClearAll @80072454: clears the refcount of all 16 entries; returns -1 (as the original). */
int32_t ae_LoopSlotClearAll(AeRam *r) {
    for (int32_t i = 15; i >= 0; --i) ae_wr16(r, AE_LOOP_SLOTS + (uint32_t)i * AE_LOOP_SLOT_SIZE + 8, 0);
    return -1;
}

/* ------------------------------------------------------------------------------------------------
 * VectorNormalS @8003019C (core at 0x80030210).  GTE: SQR (IR = low 16 bits of each input), LZCR,
 * table lookup, GPF.  v0 on return = sum of squares.
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_VectorNormalS(AeRam *r, uint32_t in, uint32_t out) {
    uint32_t x = ae_rd32(r, in), y = ae_rd32(r, in + 4), z = ae_rd32(r, in + 8);
    int32_t ir[3] = {(int16_t)x, (int16_t)y, (int16_t)z};
    int64_t s64 = (int64_t)(ir[0] * ir[0]) + (ir[1] * ir[1]);
    if (s64 > INT32_MAX) { ae_ram_fail(r, "VectorNormalS: add overflow (the original traps)"); return 0; }
    s64 += (int64_t)(ir[2] * ir[2]);
    if (s64 > INT32_MAX) { ae_ram_fail(r, "VectorNormalS: add overflow (the original traps)"); return 0; }
    int32_t s = (int32_t)s64;                                   /* sum >= 0 */
    int32_t lz = 32;
    if (s != 0) { lz = 0; for (uint32_t v = (uint32_t)s; !(v & 0x80000000u); v <<= 1) ++lz; }
    int32_t v1 = lz & ~1;
    int32_t t6 = (31 - v1) >> 1;
    int32_t t3 = v1 - 24;
    int32_t t4;
    if (t3 < 0) { t3 = 24 - v1; t4 = s >> (t3 & 31); }
    else t4 = (int32_t)((uint32_t)s << (t3 & 31));
    t4 = (int32_t)((uint32_t)t4 - 64u) * 2;
    int32_t ir0 = ae_rds16(r, (uint32_t)(0x800B0000u + (uint32_t)t4 - 25580u));
    for (int i = 0; i < 3; ++i) {
        int32_t mac = ir0 * ir[i];
        ae_wr16(r, out + (uint32_t)i * 2, (uint32_t)(mac >> (t6 & 31)));
    }
    return s;
}

/* ------------------------------------------------------------------------------------------------
 * CollisionBodyGetCenter @80077C28: out = three s16 {x, y, z} (low 16 bits of the body position).
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_CollisionBodyGetCenter(AeRam *r, int32_t type, uint32_t body, uint32_t out) {
    int32_t t = (int16_t)type;
    if ((((uint32_t)type - 6u) & 0xFFFFu) < 4u || t == 12 || t == 13 || t == 16 || t == 17) {
        ae_wr16(r, out + 0, ae_rd16(r, body + 0));
        ae_wr16(r, out + 2, ae_rd16(r, body + 4));
        ae_wr16(r, out + 4, ae_rd16(r, body + 8));
        return 0;
    }
    ae_wr16(r, out + 0, ae_rd16(r, body + 0));
    int32_t hh = half(ae_rds16(r, body + 0x22));
    ae_wr16(r, out + 2, ae_rd16(r, body + 4) - (uint32_t)hh);
    ae_wr16(r, out + 4, ae_rd16(r, body + 8));
    if (t == 1 && (ae_rd32(r, AE_G_PLAYER + 0x70) & 0xF0) == 0x60) {
        int32_t h2 = half(ae_rds16(r, body + 0x22));
        ae_wr16(r, out + 2, ae_rd16(r, body + 4) + (uint32_t)h2);
    }
    return 0;
}

/* BodyContactType @800392E4 */
uint32_t ae_BodyContactType(AeRam *r, uint32_t actor) {
    return ae_rd32(r, OWNER_CONTACT(actor)) >> 28;
}

/* BodyTypeOfActor @80078864: type of the slot whose index equals actor+0x4C, or -1 */
int32_t ae_BodyTypeOfActor(AeRam *r, uint32_t actor) {
    if (actor == 0) return -1;
    uint32_t n = ae_rd32(r, AE_G_COLLISION_BODIES);
    for (int cap = 0; n != 0 && cap < WALK_CAP; ++cap) {
        int32_t a0 = ae_rds16(r, actor + 0x4C);
        uint32_t next = SLOT_NEXT(n);
        if (a0 == SLOT_IDX(n)) return SLOT_TYPE(n);
        n = next;
        if (ae_ram_failed(r)) return -1;
    }
    return -1;
}

/* BodyContactSfx @80078808 */
int32_t ae_BodyContactSfx(AeRam *r, const AeBodyHooks *h, int32_t body_type, int32_t collider_type) {
    if ((int16_t)body_type == 3) return 0x29;
    int32_t v;
    if (h && h->gimmick_type_to_sfx) v = h->gimmick_type_to_sfx(h->user, r, (int16_t)collider_type);
    else { missing_hook(r, "gimmick_type_to_sfx"); return 0x29; }
    if (v < 0 || v == 0x80 || v == 0x82 || v == 0x81) return 0x29;
    return v;
}

/* ------------------------------------------------------------------------------------------------
 * BodyCollideWithBodies @800777AC: pushes apart bodies of types 1..5 (slot = body slot A)
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_BodyCollideWithBodies(AeRam *r, const AeBodyHooks *h, uint32_t a) {
    if (ae_rd32(r, BODY_CONTACT(SLOT_BODY(a))) != 0) return 0;
    uint32_t b = ae_rd32(r, AE_G_COLLISION_BODIES);
    int walk = 0, pushes = 0;
    for (; b != 0; b = SLOT_NEXT(b)) {
        if (ae_ram_failed(r) || ++walk > WALK_CAP) { ae_ram_fail(r, "BodyCollideWithBodies: list walk cap"); return 0; }
        if (a == b) continue;
        int32_t bt = SLOT_TYPE(b);
        if (bt <= 0 || bt >= 6) continue;
        if (ae_rd32(r, BODY_CONTACT(SLOT_BODY(b))) != 0) continue;
        int32_t at = SLOT_TYPE(a);
        if (at == 3 && !(bt == 1 || bt == 3 || bt == 2)) continue;
        at = SLOT_TYPE(a);
        if (at == 4 && (SLOT_TYPE(b) == 3 || SLOT_TYPE(b) == 4)) continue;
        if (at == 5 && (SLOT_TYPE(b) == 5 || SLOT_TYPE(b) == 3)) continue;
        uint32_t bb = SLOT_BODY(b);                              /* s7 */
        ae_CollisionBodyGetCenter(r, SLOT_TYPE(b), bb, SCR_C_B);
        for (;;) {
            if (ae_ram_failed(r) || ++pushes > PUSH_CAP) { ae_ram_fail(r, "BodyCollideWithBodies: push loop cap"); return 0; }
            ae_CollisionBodyGetCenter(r, SLOT_TYPE(a), SLOT_BODY(a), SCR_C_A);
            int32_t dz = ae_rds16(r, SCR_C_B + 4) - ae_rds16(r, SCR_C_A + 4);
            int32_t dx = ae_rds16(r, SCR_C_B + 0) - ae_rds16(r, SCR_C_A + 0);
            int32_t dy = ae_rds16(r, SCR_C_B + 2) - ae_rds16(r, SCR_C_A + 2);
            ae_wr32(r, SCR_V + 8, (uint32_t)dz);
            ae_wr32(r, SCR_V + 0, (uint32_t)dx);
            ae_wr32(r, SCR_V + 4, (uint32_t)dy);
            int32_t len = vec_length3(dx, 0, dz);                /* a2 still holds dz */
            uint32_t abody = SLOT_BODY(a);
            if (!(len < ae_rds16(r, bb + 0x20) + ae_rds16(r, abody + 0x20))) break;
            int32_t ha = half_abs(ae_rds16(r, abody + 0x22));
            int32_t hb = half_abs(ae_rds16(r, bb + 0x22));
            int32_t cdy = ae_rds16(r, SCR_C_B + 2) - ae_rds16(r, SCR_C_A + 2);
            int32_t sum = ha + hb;
            if (cdy >= 0) { if (!(cdy < sum)) break; }
            else if (!((ae_rds16(r, SCR_C_A + 2) - ae_rds16(r, SCR_C_B + 2)) < sum)) break;
            int32_t btype = SLOT_TYPE(b);
            if ((btype == 5 && SLOT_TYPE(a) == 4) || (btype == 4 && SLOT_TYPE(a) == 5) || btype == 1)
                ae_wr32(r, BODY_CONTACT(SLOT_BODY(b)), ((uint32_t)SLOT_TYPE(a) << 28) | (uint32_t)SLOT_IDX(a));
            ae_wr32(r, BODY_CONTACT(SLOT_BODY(a)), ((uint32_t)SLOT_TYPE(b) << 28) | (uint32_t)SLOT_IDX(b));
            if (SLOT_TYPE(a) == 3 && SLOT_TYPE(b) == 3) break;
            ae_VectorNormalS(r, SCR_V, SCR_N);
            int32_t nx = (int32_t)((uint32_t)ae_rd16(r, SCR_N + 0) << 16) >> 26;
            int32_t nz = (int32_t)((uint32_t)ae_rd16(r, SCR_N + 4) << 16) >> 26;
            int32_t px = nx, pz = nz;
            if (((uint32_t)(nx + 3) & 0xFFFF) < 7 && ((uint32_t)(nz + 3) & 0xFFFF) < 7) {
                px = nx >= 0 ? 4 : -4;
                pz = nz >= 0 ? 4 : -4;
            }
            int32_t sx = (int16_t)px, sz = (int16_t)pz;
            int pushb = (((int32_t)ae_rds16(r, SLOT_BODY(a) + 0x26) & 0x8000) != 0 && (uint32_t)(SLOT_TYPE_U(b) - 1) < 2u) ||
                        (SLOT_TYPE(a) == 5 && SLOT_TYPE(b) == 4);
            if (pushb) {
                uint32_t p = SLOT_BODY(b);
                ae_wr32(r, p + 0, ae_rd32(r, p + 0) + (uint32_t)sx);
                p = SLOT_BODY(b);
                ae_wr32(r, p + 8, ae_rd32(r, p + 8) + (uint32_t)sz);
                ae_CollisionBodyGetCenter(r, SLOT_TYPE(b), SLOT_BODY(b), SCR_C_B);
                if (SLOT_TYPE(b) == 1) {
                    if (h && h->camera_nudge) h->camera_nudge(h->user, r, sx, 0, sz);
                    else missing_hook(r, "camera_nudge");
                }
            } else {
                uint32_t p = SLOT_BODY(a);
                ae_wr32(r, p + 0, ae_rd32(r, p + 0) - (uint32_t)sx);
                p = SLOT_BODY(a);
                ae_wr32(r, p + 8, ae_rd32(r, p + 8) - (uint32_t)sz);
            }
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * BodyFindOverlapType1 @80077D18 (type argument unused by the original)
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_BodyFindOverlapType1(AeRam *r, const AeBodyHooks *h, int32_t type, uint32_t a) {
    (void)type;
    int walk = 0;
    for (uint32_t b = ae_rd32(r, AE_G_COLLISION_BODIES); b != 0; b = SLOT_NEXT(b)) {
        if (ae_ram_failed(r) || ++walk > WALK_CAP) { ae_ram_fail(r, "BodyFindOverlapType1: list walk cap"); return 0; }
        if (SLOT_TYPE(b) != 1) continue;
        uint32_t c = ae_rd32(r, BODY_CONTACT(SLOT_BODY(b)));
        if (c != 0 && (c >> 28) != 10) continue;
        uint32_t s0 = SLOT_BODY(b);
        ae_CollisionBodyGetCenter(r, SLOT_TYPE(b), s0, SCR_V);
        uint32_t abody = SLOT_BODY(a);
        int32_t dx = sub32(ae_rds16(r, SCR_V + 0), ae_rds32(r, abody + 0));
        int32_t dz = sub32(ae_rds16(r, SCR_V + 4), ae_rds32(r, abody + 8));
        int32_t len = vec_length3(dx, 0, dz);
        abody = SLOT_BODY(a);
        int32_t rad = ae_rds16(r, abody + 0x20), hgt = ae_rds16(r, abody + 0x22);
        int32_t d = len - rad;
        if (d < 0) d = rad - len;
        if (!(d < hgt)) continue;
        int32_t bh = ae_rds16(r, s0 + 0x22);
        int32_t by = ae_rds32(r, s0 + 4);
        int32_t ay = ae_rds32(r, SLOT_BODY(a) + 4);
        if (!(sub32(sub32(by, bh), 64) < ay)) continue;
        if (!(ay < add32(by, 64))) continue;
        uint32_t shifted = (uint32_t)ae_rds16(r, SLOT_BODY(a) + 0x24) << 12;
        int32_t stub = 0;
        if (h && h->body_stub_return_zero) stub = h->body_stub_return_zero(h->user, r);
        uint32_t v1 = shifted & 0x0FFFF000u;
        if (stub == 0) {
            ae_wr32(r, BODY_CONTACT(SLOT_BODY(b)), v1 | ((SLOT_IDX_U(a) & 0xFFFu) | 0xB0000000u));
            ae_wr32(r, BODY_CONTACT(SLOT_BODY(a)), 0x60000000u);
            uint32_t owner = SLOT_OWNER(a);
            if (owner != 0) ae_wr32(r, OWNER_CONTACT(owner), 0x60000000u);
        } else {
            ae_wr32(r, BODY_CONTACT(SLOT_BODY(a)), v1 | ((SLOT_IDX_U(b) & 0xFFFu) | 0xB0000000u));
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * BodyFindOverlapType3to5 @80077EE8
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_BodyFindOverlapType3to5(AeRam *r, const AeBodyHooks *h, int32_t type, uint32_t a) {
    (void)h; (void)type;
    int walk = 0;
    for (uint32_t b = ae_rd32(r, AE_G_COLLISION_BODIES); b != 0; b = SLOT_NEXT(b)) {
        if (ae_ram_failed(r) || ++walk > WALK_CAP) { ae_ram_fail(r, "BodyFindOverlapType3to5: list walk cap"); return 0; }
        if (!((uint32_t)(SLOT_TYPE_U(b) - 3) < 3u)) continue;
        uint32_t c = ae_rd32(r, BODY_CONTACT(SLOT_BODY(b)));
        if (c != 0 && (c >> 28) != 10) continue;
        uint32_t s1 = SLOT_BODY(b);
        ae_CollisionBodyGetCenter(r, SLOT_TYPE(b), s1, SCR_V);
        uint32_t abody = SLOT_BODY(a);
        int32_t dx = sub32(ae_rds16(r, SCR_V + 0), ae_rds32(r, abody + 0));
        int32_t dz = sub32(ae_rds16(r, SCR_V + 4), ae_rds32(r, abody + 8));
        int32_t len = vec_length3(dx, 0, dz);
        abody = SLOT_BODY(a);
        int32_t ra = ae_rds16(r, abody + 0x20);
        if (!(len < ae_rds16(r, s1 + 0x20) + ra)) continue;
        int32_t hh = half_abs(ae_rds16(r, s1 + 0x22));
        int32_t lim = ra + hh;
        int32_t cy = ae_rds16(r, SCR_V + 2);
        int32_t ay = ae_rds32(r, abody + 4);
        int32_t dy = sub32(cy, ay);
        if (dy >= 0) { if (!(dy < lim)) continue; }
        else if (!(sub32(ay, cy) < lim)) continue;
        uint32_t v1 = ((uint32_t)ae_rds16(r, SLOT_BODY(a) + 0x24) << 12) & 0x0FFFF000u;
        ae_wr32(r, BODY_CONTACT(SLOT_BODY(b)), v1 | ((SLOT_IDX_U(a) & 0xFFFu) | 0xB0000000u));
        ae_wr32(r, BODY_CONTACT(SLOT_BODY(a)), 0x60000000u);
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * BodyCheckPickupContact @800788B8 (type argument unused)
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_BodyCheckPickupContact(AeRam *r, const AeBodyHooks *h, int32_t type, uint32_t a) {
    (void)type;
    if (ae_rd32(r, BODY_CONTACT(SLOT_BODY(a))) != 0) return 0;
    int walk = 0;
    for (uint32_t b = ae_rd32(r, AE_G_COLLISION_BODIES); b != 0; b = SLOT_NEXT(b)) {
        if (ae_ram_failed(r) || ++walk > WALK_CAP) { ae_ram_fail(r, "BodyCheckPickupContact: list walk cap"); return 0; }
        uint32_t tu = SLOT_TYPE_U(b);
        int32_t ts = (int16_t)tu;
        if (!((uint32_t)(tu - 1) < 2u || ts == 8 || ts == 13)) continue;
        uint32_t s0 = SLOT_BODY(b);
        ae_CollisionBodyGetCenter(r, SLOT_TYPE(b), s0, SCR_V);
        uint32_t abody = SLOT_BODY(a);
        int32_t dx = sub32(ae_rds16(r, SCR_V + 0), ae_rds32(r, abody + 0));
        int32_t dy = sub32(ae_rds16(r, SCR_V + 2), ae_rds32(r, abody + 4));
        int32_t dz = sub32(ae_rds16(r, SCR_V + 4), ae_rds32(r, abody + 8));
        int32_t len = vec_length3(dx, dy, dz);
        abody = SLOT_BODY(a);
        if (len < ae_rds16(r, s0 + 0x20) + ae_rds16(r, abody + 0x20)) {
            if (h && h->pickup_collect) h->pickup_collect(h->user, r, ae_rds16(r, SLOT_BODY(a) + 0x24));
            else missing_hook(r, "pickup_collect");
            ae_wr32(r, BODY_CONTACT(SLOT_BODY(a)), ((uint32_t)SLOT_TYPE(b) << 28) | (SLOT_IDX_U(b) & 0xFFFu));
            return 0;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * BodyCollideBodiesDetailed @80078088
 * p1 = body type (a0, used raw), a = body slot (a1)
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_BodyCollideBodiesDetailed(AeRam *r, const AeBodyHooks *h, int32_t p1, uint32_t a) {
    if (ae_rd32(r, BODY_CONTACT(SLOT_BODY(a))) != 0) return 0;
    uint32_t s2 = SLOT_OWNER(a);
    int32_t s7 = ae_BodyTypeOfActor(r, s2);
    int walk = 0;
    for (uint32_t b = ae_rd32(r, AE_G_COLLISION_BODIES); b != 0; b = SLOT_NEXT(b)) {
        if (ae_ram_failed(r) || ++walk > WALK_CAP) { ae_ram_fail(r, "BodyCollideBodiesDetailed: list walk cap"); return 0; }
        if (a == b) continue;
        int32_t bt = SLOT_TYPE(b);
        if (bt <= 0 || bt >= 6) continue;
        uint32_t c = ae_rd32(r, BODY_CONTACT(SLOT_BODY(b)));
        if (c != 0) {
            uint32_t k = c >> 28;
            if (!(k < 6u) && k != 10) continue;
        }
        if (SLOT_TYPE(b) == 1 && s2 != 0 && (ae_rd32(r, OWNER_CONTACT(s2)) >> 28) == 8) continue;
        if (SLOT_OWNER(b) == AE_G_PLAYER && s2 == AE_G_RC_CAR) continue;
        if (SLOT_TYPE(b) == 3 && s2 != AE_G_PLAYER && s2 != AE_G_RC_CAR) continue;
        if (SLOT_TYPE(b) == 4 && !(s7 == 5 || s2 == AE_G_PLAYER || s2 == AE_G_RC_CAR)) continue;
        int cond;
        if (s2 == 0) cond = 1;
        else if (ae_rds16(r, s2 + 0x4C) != SLOT_IDX(b) && SLOT_OWNER(b) == 0) cond = 1;
        else cond = (SLOT_OWNER(b) != 0 && s2 != SLOT_OWNER(b));
        if (!cond) continue;

        uint32_t bb = SLOT_BODY(b);                             /* s1 */
        ae_CollisionBodyGetCenter(r, SLOT_TYPE(b), bb, SCR_N);
        int32_t vx = sub32(ae_rds16(r, SCR_N + 0), ae_rds32(r, SLOT_BODY(a) + 0));
        ae_wr32(r, SCR_V + 0, (uint32_t)vx);
        int32_t vy = sub32(ae_rds16(r, SCR_N + 2), ae_rds32(r, SLOT_BODY(a) + 4));
        ae_wr32(r, SCR_V + 4, (uint32_t)vy);
        int32_t vz = sub32(ae_rds16(r, SCR_N + 4), ae_rds32(r, SLOT_BODY(a) + 8));
        ae_wr32(r, SCR_V + 8, (uint32_t)vz);
        uint32_t tu = SLOT_TYPE_U(b);
        int32_t ts = (int16_t)tu;
        if ((uint32_t)(tu - 6) < 2u || ts == 12 || ts == 13) {
            int32_t len = vec_length3(vx, vy, vz);
            if (len < ae_rds16(r, bb + 0x20) + ae_rds16(r, SLOT_BODY(a) + 0x20)) {
                ae_wr32(r, BODY_CONTACT(SLOT_BODY(b)), 0xB0000000u);
                ae_wr32(r, BODY_CONTACT(SLOT_BODY(a)), 0xB0000000u);
            }
            continue;
        }
        int32_t len = vec_length3(vx, 0, vz);
        int32_t a2 = (int32_t)ae_rd16(r, bb + 0x22);              /* lhu */
        if (SLOT_TYPE(b) == 1 && (ae_rd32(r, AE_G_PLAYER + 0x70) & 0xF0) == 0x80) a2 = (int16_t)a2 / 3;
        uint32_t abody = SLOT_BODY(a);
        int32_t sumr = ae_rds16(r, bb + 0x20) + ae_rds16(r, abody + 0x20);
        if (!(len < sumr)) continue;
        int32_t hh = half_abs((int16_t)a2);
        int32_t lim = ae_rds16(r, abody + 0x20) + hh;
        int32_t cy = ae_rds16(r, SCR_N + 2);
        int32_t ay = ae_rds32(r, abody + 4);
        int32_t dy = sub32(cy, ay);
        if (dy >= 0) { if (!(dy < lim)) continue; }
        else if (!(sub32(ay, cy) < lim)) continue;

        if (((uint32_t)p1 - 6u) < 2u || p1 == 12 || p1 == 13) {
            ae_wr32(r, BODY_CONTACT(SLOT_BODY(a)), 0x60000000u);
            uint32_t sh12 = (uint32_t)ae_rds16(r, SLOT_BODY(a) + 0x24) << 12;
            int stub = 0;
            if (SLOT_TYPE(b) == 1) {
                if (h && h->body_stub_return_zero) stub = h->body_stub_return_zero(h->user, r) != 0;
                if (stub) {
                    ae_wr32(r, BODY_CONTACT(SLOT_BODY(a)),
                            (sh12 & 0x0FFFF000u) | ((SLOT_IDX_U(b) & 0xFFFu) | 0xB0000000u));
                    uint32_t owner = SLOT_OWNER(a);
                    if (owner != 0) ae_wr32(r, OWNER_CONTACT(owner), ae_rd32(r, BODY_CONTACT(SLOT_BODY(a))));
                    continue;
                }
            }
            ae_wr32(r, BODY_CONTACT(SLOT_BODY(b)), (sh12 & 0x0FFFF000u) | ((SLOT_IDX_U(a) & 0xFFFu) | 0xB0000000u));
            uint32_t owner = SLOT_OWNER(a);
            if (owner != 0) ae_wr32(r, OWNER_CONTACT(owner), 0x60000000u);
        } else if (p1 == 14) {
            ae_wr32(r, BODY_CONTACT(SLOT_BODY(b)), 0xE0000000u);
        } else if (p1 == 15) {
            ae_wr32(r, BODY_CONTACT(SLOT_BODY(b)), 0xF0000000u);
        } else if (p1 == 8) {
            if (SLOT_TYPE(b) == 3 && (ae_rd16(r, SLOT_BODY(b) + 0x26) & 0x4000u) != 0) continue;
            uint32_t owner = SLOT_OWNER(a);
            if (owner != 0 && (ae_rd32(r, OWNER_CONTACT(owner)) >> 28) == 8) continue;
            if (SLOT_TYPE(b) == 3) {
                ae_wr32(r, BODY_CONTACT(SLOT_BODY(b)), (SLOT_IDX_U(a) & 0xFFFu) | 0x80000000u);
                ae_wr32(r, BODY_CONTACT(SLOT_BODY(a)), (SLOT_IDX_U(b) & 0xFFFu) | 0x80000000u);
                owner = SLOT_OWNER(a);
                if (owner != 0) ae_wr32(r, OWNER_CONTACT(owner), (uint32_t)SLOT_IDX(b) | 0x80000000u);
            }
        }
    }

    /* after the list: ray test against the colliders */
    if (!((s7 == 5 && p1 == 6) || ae_rd32(r, BODY_CONTACT(SLOT_BODY(a))) == 0)) return 0;
    int32_t t0 = 0;
    if (s7 == 1 || s7 == 5) {
        if (((uint32_t)p1 - 6u) < 2u || p1 == 12 || p1 == 13) t0 = 1;
    }
    if (t0 == 0) {
        if (s7 == 3 && p1 == 7) t0 = 2;
        else if (s7 == 4 && (ae_rd16(r, SLOT_BODY(a) + 0x24) & 0xC00u) != 0) t0 = 3;
    }
    if (t0 == 0) return 0;
    ae_wr16(r, AE_G_COLL_WALL_PROBE, p1 == 7 ? 2 : 1);
    int16_t pos[3];
    uint32_t ab = SLOT_BODY(a);
    pos[0] = (int16_t)ae_rd16(r, ab + 0);
    uint32_t y16 = (ae_rd16(r, SLOT_BODY(a) + 4) + ae_rd16(r, SLOT_BODY(a) + 0x20)) & 0xFFFFu;
    pos[1] = (int16_t)y16;
    ae_wr32(r, SCR_C_A + 4, (uint32_t)(int32_t)(int16_t)y16);
    pos[2] = (int16_t)ae_rd16(r, SLOT_BODY(a) + 8);
    ae_wr32(r, SCR_C_A + 8, (uint32_t)(int32_t)pos[2]);
    ae_wr32(r, SCR_C_A + 12, (uint32_t)ae_rds16(r, SLOT_BODY(a) + 0x20));
    ae_wr32(r, SCR_C_A + 16, (uint32_t)ae_rds16(r, SLOT_BODY(a) + 0x20) << 1);
    ae_wr16(r, AE_G_BODY_CUR_TYPE, 6);
    ae_wr32(r, SCR_C_A + 0, (uint32_t)(int32_t)pos[0]);
    uint32_t arg = ((uint32_t)t0 << 16) | (uint32_t)ae_rds16(r, SLOT_BODY(a) + 0x24);
    int16_t out[2] = {0, 0};
    int32_t res = 0;
    if (h && h->collision_ray_test) res = h->collision_ray_test(h->user, r, arg, SCR_C_A, out);
    else missing_hook(r, "collision_ray_test");
    if (res >= 0) {
        uint32_t body = SLOT_BODY(a);
        if (ae_rd32(r, BODY_CONTACT(body)) == 0) ae_wr32(r, BODY_CONTACT(body), 0xA0000000u);
        if (out[0] != 0) {
            int32_t sfx = ae_BodyContactSfx(r, h, (int16_t)s7, (int16_t)res);
            if (sfx != 0x29) {
                if (h && h->play_sfx_at) h->play_sfx_at(h->user, r, sfx, pos);
                else missing_hook(r, "play_sfx_at");
            }
        }
    }
    ae_wr16(r, AE_G_COLL_WALL_PROBE, 0);
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * CollisionUpdate @8007247C
 * Types 1/2 execute the inline player / RC-car collision pass.  Types 6/7/12/13 also
 * build probe points before their body-collision checks.
 * ---------------------------------------------------------------------------------------------- */
static void collision_snapshot_body(AeRam *r, uint32_t body) {
    uint32_t snapshot = ae_rd32(r, AE_G_BODY_SNAPSHOT_PTR);
    for (uint32_t offset = 0; offset < AE_CU_SNAPSHOT_PREFIX_SIZE; offset += 16u) {
        uint32_t word0 = ae_rd32(r, body + offset);
        uint32_t word1 = ae_rd32(r, body + offset + 4u);
        uint32_t word2 = ae_rd32(r, body + offset + 8u);
        uint32_t word3 = ae_rd32(r, body + offset + 12u);
        ae_wr32(r, snapshot + offset, word0);
        ae_wr32(r, snapshot + offset + 4u, word1);
        ae_wr32(r, snapshot + offset + 8u, word2);
        ae_wr32(r, snapshot + offset + 12u, word3);
    }
    uint32_t tail = body + AE_CU_SNAPSHOT_TAIL_OFFSET;
    uint32_t snapshot_tail = snapshot + AE_CU_SNAPSHOT_TAIL_OFFSET;
    for (uint32_t offset = 0; offset < AE_CU_SNAPSHOT_TAIL_SIZE; offset += sizeof(uint32_t)) {
        uint32_t word = ae_rd32(r, tail + offset);
        ae_wr32(r, snapshot_tail + offset, word);
    }
}

static void collision_update_body_type_1(AeRam *r, const AeBodyHooks *h,
                                         uint32_t body) {
    collision_snapshot_body(r, body);
    uint32_t mode = ae_rd16(r, AE_G_PLAYER_MODE);
    ae_wr16(r, AE_G_BODY_MODE5_COLLIDER, 0xFFFFu);
    ae_wr16(r, AE_G_BODY_MODE6_COLLIDER, 0xFFFFu);
    if (mode == 2u) {
        (void)ae_CollisionBuildProbePoints(r, 1, body);
        return;
    }
    if (mode == 6u) {
        ae_wr16(r, AE_G_BODY_MODE6_COLLIDER,
                ae_rd16(r, body + AE_CU_BODY_CONTACT_COLLIDER));
    } else if (mode == 5u) {
        ae_wr16(r, AE_G_BODY_MODE5_COLLIDER,
                ae_rd16(r, body + AE_CU_BODY_CONTACT_COLLIDER));
        ae_wr32(r, body + AE_CU_BODY_VELOCITY_X, 0);
        ae_wr32(r, body + AE_CU_BODY_VELOCITY_Z, 0);
    }
    ae_wr16(r, body + AE_CU_BODY_CONTACT_COLLIDER, 0xFFFFu);

    AeBodyHooks nested = h ? *h : (AeBodyHooks){0};
    if (nested.move.user == NULL && h != NULL) nested.move.user = h->user;
    if (nested.move.stack_pointer == 0u) nested.move.stack_pointer = 0x801FFF00u - 56u;
    (void)ae_PlayerMoveRumbleSoundUpdate(r, &nested, body);
    (void)ae_BodyProbeGround(r, nested.move.world, body);
    (void)ae_CollisionStub_ReturnZero(r, body);
    (void)ae_CollisionBuildProbePoints(r, 1, body);
    int32_t ceiling = ae_CollisionResolveProbes(r, &nested.move, 1, body);
    (void)ae_BodyLandOnGround(r, &nested.move, body, 1, ceiling);
    (void)ae_PlayerTryLedgeGrab(r, &nested.move);
    (void)ae_BodyRecordColliderContact(r, &nested.move, body, 1);
}

static void collision_update_body_type_2(AeRam *r, const AeBodyHooks *h,
                                         uint32_t body) {
    collision_snapshot_body(r, body);
    ae_wr16(r, body + AE_CU_BODY_CONTACT_COLLIDER, 0xFFFFu);
    AeBodyHooks nested = h ? *h : (AeBodyHooks){0};
    if (nested.move.user == NULL && h != NULL) nested.move.user = h->user;
    if (nested.move.stack_pointer == 0u) nested.move.stack_pointer = 0x801FFF00u - 56u;
    (void)ae_PlayerVehicleSoundUpdate(r, &nested, body);
    (void)ae_BodyProbeGround(r, nested.move.world, body);
    (void)ae_CollisionStub_ReturnZero(r, body);
    (void)ae_CollisionBuildProbePoints(r, 2, body);
    int32_t ceiling = ae_CollisionResolveProbes(r, &nested.move, 2, body);
    (void)ae_BodyLandOnGround(r, &nested.move, body, 2, ceiling);
    (void)ae_BodyRecordColliderContact(r, &nested.move, body, 2);
}

int32_t ae_CollisionUpdate(AeRam *r, const AeBodyHooks *h) {
    if (ae_rd16(r, AE_G_PLAYER + AE_CU_PLAYER_STATE) == 0x2D) {
        ae_wr32(r, AE_G_PLAYER + AE_CU_PLAYER_POSITION_X,
                ae_rd32(r, AE_G_PLAYER + AE_CU_PLAYER_POSITION_X) +
                ae_rd32(r, AE_G_PLAYER + AE_CU_PLAYER_VELOCITY_X));
        uint32_t y = ae_rd32(r, AE_G_PLAYER + AE_CU_PLAYER_POSITION_Y) +
                     ae_rd32(r, AE_G_PLAYER + AE_CU_PLAYER_VELOCITY_Y);
        ae_wr32(r, AE_G_PLAYER + AE_CU_PLAYER_POSITION_Y, y);
        ae_wr32(r, AE_G_PLAYER + AE_CU_PLAYER_POSITION_Z,
                ae_rd32(r, AE_G_PLAYER + AE_CU_PLAYER_POSITION_Z) +
                ae_rd32(r, AE_G_PLAYER + AE_CU_PLAYER_VELOCITY_Z));
        return 0;
    }
    uint32_t s1 = ae_rd32(r, AE_G_COLLISION_BODIES);
    ae_wr16(r, AE_G_COLL_COUNTER, 0);
    int32_t count = ae_rds16(r, AE_G_COLLIDER_COUNT);
    ae_wr16(r, AE_G_IGNORE_COLLIDER, 0xFFFF);
    for (int32_t idx = count - 1; idx >= 0; --idx) {
        uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
        ae_wr16(r, colliders + (uint32_t)idx * AE_CU_COLLIDER_SIZE +
                AE_CU_COLLIDER_CONTACT_INDEX, 0xFFFF);
    }
    ae_LoopSlotClearAll(r);
    int walk = 0;
    while (s1 != 0) {
        if (ae_ram_failed(r) || ++walk > WALK_CAP) { ae_ram_fail(r, "CollisionUpdate: list walk cap"); return 0; }
        uint32_t body = SLOT_BODY(s1);
        int32_t type = SLOT_TYPE(s1);
        uint32_t next = SLOT_NEXT(s1);
        int release = 0;
        if (type >= 6) {
            if (type < 9) release = 1;
            else if (type < 18 && type >= 12) release = 1;
            if (release && ae_rd16(r, body + AE_CU_BODY_SOLID_COUNT) != 0) release = 0;
        }
        if (release) {
            ae_CollisionBodyFree(r, body);
            ae_CollisionBodyUnregister(r, h, body);
        }
        s1 = next;
        ae_wr32(r, BODY_CONTACT(body), 0);
    }
    s1 = ae_rd32(r, AE_G_COLLISION_BODIES);
    walk = 0;
    while (s1 != 0) {
        if (ae_ram_failed(r) || ++walk > WALK_CAP) { ae_ram_fail(r, "CollisionUpdate: body walk cap"); return 0; }
        uint32_t body = SLOT_BODY(s1);
        uint32_t tu = SLOT_TYPE_U(s1);
        uint32_t next = SLOT_NEXT(s1);
        ae_wr16(r, AE_G_BODY_CUR_TYPE, tu);
        int32_t sel = (int16_t)(tu - 1);
        int dec = 0;
        if ((uint32_t)sel < 17u) {
            switch (sel + 1) {
            case 1:
                collision_update_body_type_1(r, h, body);
                break;
            case 2:
                collision_update_body_type_2(r, h, body);
                break;
            case 3: case 4: case 5:
                ae_BodyCollideWithBodies(r, h, s1);
                break;
            case 6: case 7: case 12: case 13:
                if (h && h->build_probe_points) h->build_probe_points(h->user, r, SLOT_TYPE(s1), body);
                else missing_hook(r, "build_probe_points");
                /* fall through */
            case 8: case 14: case 15:
                ae_BodyCollideBodiesDetailed(r, h, SLOT_TYPE(s1), s1);
                dec = 1;
                break;
            case 9:
            if (ae_rd32(r, AE_G_STAGE_ENTRY_FLAGS) == 0 &&
                ae_rds16(r, AE_G_PLAYER + AE_CU_PLAYER_HEALTH) > 0)
                    ae_BodyCheckPickupContact(r, h, SLOT_TYPE(s1), s1);
                break;
            case 16:
                ae_BodyFindOverlapType1(r, h, SLOT_TYPE(s1), s1);
                dec = 1;
                break;
            case 17:
                ae_BodyFindOverlapType3to5(r, h, SLOT_TYPE(s1), s1);
                dec = 1;
                break;
            default:
                break;
            }
            if (dec)
                ae_wr16(r, body + AE_CU_BODY_SOLID_COUNT,
                        ae_rd16(r, body + AE_CU_BODY_SOLID_COUNT) - 1);
        }
        /* clamp the position to the level grid */
        int32_t a1 = (int32_t)((uint32_t)(ae_rds16(r, AE_G_LEVEL_GRID_WIDTH) - 1) << 8);
        int32_t x = ae_rds32(r, body + AE_CU_BODY_POSITION_X);
        int32_t nx = x;
        if (x < -a1) nx = -a1;
        else if (a1 < x) nx = a1;
        ae_wr32(r, body + AE_CU_BODY_POSITION_X, (uint32_t)nx);
        a1 = (int32_t)((uint32_t)(ae_rds16(r, AE_G_LEVEL_GRID_DEPTH) - 1) << 8);
        int32_t z = ae_rds32(r, body + AE_CU_BODY_POSITION_Z);
        int32_t nz = z;
        if (z < -a1) nz = -a1;
        else if (a1 < z) nz = a1;
        ae_wr32(r, body + AE_CU_BODY_POSITION_Z, (uint32_t)nz);
        s1 = next;
    }
    ae_LoopSlotSweep(r);
    ae_wr32(r, AE_G_COLL_2130, 0);
    return 0;
}
