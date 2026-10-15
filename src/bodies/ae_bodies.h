#ifndef AE_BODIES_H
#define AE_BODIES_H
/*
 * Collision-body system of Ape Escape (PS1), ported 1:1 from the original instructions and run on a
 * byte-exact image of PSX main RAM (AeRam).  Function names are the original names with an ae_ prefix;
 * the original address is given at each function.  Every ported function is diffed against the original
 * MIPS code by tools/mips/check_bodies.py (see notes/port/bodies.md).
 *
 * Data (all at their original addresses):
 *   g_CollisionBodies 0x800B20D8   head of the linked list of body slots, tail at 0x800B20DC
 *   slot table        0x800BBDA0   256 slots x 0x14 bytes
 *       +0 s16 type (-1 free)  +2 s16 slot index  +4 body*  +8 owner*  +0xC next slot  +0x10 prev slot
 *   body pool         0x800BD1A0   128 standalone CollisionBody x 0x38, free flags (s16, <0 free) 0x800BEDA0
 *   loop slot table   0x800BEEA0   16 entries x 12 bytes
 *   CollisionBody (0x38): +0 s32[3] pos, +0xC s32[3] vel, +0x18 SVECTOR facing, +0x20 s16 radius,
 *       +0x22 s16 height, +0x24 u16 param, +0x26 u16 flags, +0x2A u16 solid, +0x2C u32 contact,
 *       +0x34 s16 contactCollider.  The bodies of monkeys / objects are embedded at owner+0x24.
 *
 * Calls to functions outside this module go through AeBodyHooks.  A NULL hook (other than
 * body_stub_return_zero) sets the AeRam error flag and returns 0 (fail closed).
 * Scratchpad addresses used by the original (0x1F8000F0..0x1F800118) are written exactly as the
 * original does, because later code (and the host's hooks) read them.
 */
#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- global addresses ------------------------------------------------------------------------ */
#define AE_G_COLLISION_BODIES 0x800B20D8u   /* head */
#define AE_G_COLLISION_TAIL 0x800B20DCu
#define AE_COLLISION_SLOTS 0x800BBDA0u
#define AE_COLLISION_SLOT_COUNT 256
#define AE_COLLISION_SLOT_SIZE 0x14u
#define AE_BODY_POOL 0x800BD1A0u
#define AE_BODY_POOL_FREE 0x800BEDA0u
#define AE_BODY_POOL_COUNT 128
#define AE_BODY_SIZE 0x38u
#define AE_LOOP_SLOTS 0x800BEEA0u
#define AE_LOOP_SLOT_COUNT 16
#define AE_LOOP_SLOT_SIZE 12u
#define AE_G_PLAYER 0x800EC1E0u
#define AE_G_RC_CAR 0x800F5A20u
#define AE_G_STAGE_ENTRY_FLAGS 0x800F4450u
#define AE_G_COLLIDERS 0x800F455Cu          /* Collider* (0x38 each) */
#define AE_G_COLLIDER_COUNT 0x800F45D4u     /* s16 */
#define AE_G_IGNORE_COLLIDER 0x800F45E0u    /* u16 */
#define AE_G_COLL_WALL_PROBE 0x800F45E6u    /* u16 */
#define AE_G_COLL_PROBE_E8 0x800F45E8u      /* DAT_800F45E8, cleared by CollisionBodiesReset */
#define AE_G_LEVEL_GRID_WIDTH 0x800EBFA0u   /* s16 */
#define AE_G_LEVEL_GRID_DEPTH 0x800EBFA2u   /* s16 */
#define AE_G_BODY_CUR_TYPE 0x800B20CCu      /* DAT_800B20CC: type of the body being updated */
#define AE_G_COLL_COUNTER 0x800B20C4u       /* DAT_800B20C4 */
#define AE_G_BODY_SNAPSHOT_PTR 0x800B20B8u  /* pointer to CollisionUpdate's 0x38-byte body snapshot */
#define AE_G_PLAYER_MODE 0x800F44A0u       /* player control mode used by CollisionUpdate */
#define AE_G_BODY_MODE6_COLLIDER 0x800B20C8u /* previous collider index saved by player mode 6 */
#define AE_G_BODY_MODE5_COLLIDER 0x800B20CAu /* previous collider index saved by player mode 5 */
#define AE_G_LOOP_SOUND 0x800B20D0u         /* s16 handle of loop sound 0x16, +2 s16 active flag */
#define AE_G_RC_LOOP_FLAG 0x800F5A78u       /* DAT_800F5A78 */
#define AE_G_COLL_2130 0x800B2130u          /* DAT_800B2130 */

/* Forward declarations for the verified modules used by the player / RC-car body path. */
typedef struct AeWorldHooks AeWorldHooks;
typedef struct AeSolidHooks AeSolidHooks;
typedef struct AeMiscHooks AeMiscHooks;
typedef struct AePlayerActHooks AePlayerActHooks;
typedef struct AeSpawnHooks AeSpawnHooks;
typedef struct AeCamupdHooks AeCamupdHooks;
typedef struct AeAreaSetHooks AeAreaSetHooks;
typedef struct AeSoundHw AeSoundHw;
typedef struct AeCollProbeHooks AeCollProbeHooks;

/* Additional ports and native-system calls needed by CollisionUpdate's inline cases 1 and 2. */
typedef struct AeBodyMoveHooks {
    void *user;
    const AeWorldHooks *world;
    const AeSolidHooks *solid;
    const AeMiscHooks *misc;
    const AePlayerActHooks *player_act;
    const AeSpawnHooks *spawn;
    const AeCamupdHooks *camupd;
    const AeAreaSetHooks *area_set;
    const AeSoundHw *sound_hw;
    const AeCollProbeHooks *coll_probe;
    uint32_t stack_pointer;
    /* Unwritten SVECTOR padding copied by PlayerTryLedgeGrab. Native hosts use
     * zero; differential hosts can supply the original caller's stale word. */
    uint32_t (*LocalDirectionPadding)(void *user, AeRam *r);
    void (*SfxVoiceSetPitch)(void *user, AeRam *r, int32_t voice, int32_t pitch); /* 800A35FC */
    void (*XaPlay)(void *user, AeRam *r, int32_t index); /* 80011F6C: native CD / XA boundary */
} AeBodyMoveHooks;

/* ---- hook table ------------------------------------------------------------------------------ */
typedef struct AeBodyHooks {
    void *user;
    /* printf @800A3720: only called as printf(" Collision_set Error %d\n", type); fmt = address of the string */
    void (*printf_error)(void *user, AeRam *r, uint32_t fmt, int32_t type);
    /* SoundLoopStart @800878E0 (CollisionBodyRegister type 2: (1, 0x16)); the result is stored at 0x800B20D0 */
    int32_t (*sound_loop_start)(void *user, AeRam *r, int32_t a, int32_t id);
    /* SoundLoopStop @8008806C (CollisionBodyUnregister type 2: (0x16, handle)) */
    void (*sound_loop_stop)(void *user, AeRam *r, int32_t id, int32_t handle);
    /* Reserved ABI slot from the retired body-movement callback; CollisionUpdate has inline player/car movement. */
    void (*reserved_body_move)(void *user, AeRam *r, int32_t type, uint32_t body, uint32_t slot);
    /* Legacy ABI slot. CollisionUpdate calls ae_CollisionBuildProbePoints directly. */
    void (*build_probe_points)(void *user, AeRam *r, int32_t type, uint32_t body);
    /* BodyStub_ReturnZero @80098320: the original returns 0.  NULL = the original (returns 0). */
    int32_t (*body_stub_return_zero)(void *user, AeRam *r);
    /* CollisionRayTest @80078C9C (arg, params = scratchpad 0x1F800108, out = two s16 results (out[0] is read)) */
    int32_t (*collision_ray_test)(void *user, AeRam *r, uint32_t arg, uint32_t params, int16_t out[2]);
    /* GimmickTypeToSfx @8005BB98 */
    int32_t (*gimmick_type_to_sfx)(void *user, AeRam *r, int32_t collider_type);
    /* PlaySfxAt @80087A20 (sfx, pos = three s16 {x, y, z} that the original passes by stack address) */
    void (*play_sfx_at)(void *user, AeRam *r, int32_t sfx, const int16_t pos[3]);
    /* PickupCollect @80078A18 */
    void (*pickup_collect)(void *user, AeRam *r, int32_t type);
    /* CameraNudge @80047690 */
    void (*camera_nudge)(void *user, AeRam *r, int32_t dx, int32_t dy, int32_t dz);
    /* Context for direct calls to the verified player / collision / audio ports used by the cases above. */
    AeBodyMoveHooks move;
} AeBodyHooks;

/* ---- list / slot management ------------------------------------------------------------------ */
int32_t ae_CollisionBodiesReset(AeRam *r);                                    /* 80071D3C */
int32_t ae_CollisionBodyRegister(AeRam *r, const AeBodyHooks *h, int32_t type, uint32_t body, uint32_t owner); /* 80071E44 */
uint32_t ae_CollisionBodyAllocSlot(AeRam *r);                                 /* 80071FDC: slot address or 0 */
int32_t ae_CollisionBodyUnregister(AeRam *r, const AeBodyHooks *h, uint32_t body); /* 80072018 */
uint32_t ae_GetObjectByIndex(AeRam *r, uint32_t index);                       /* 80072170 (a0 is a u16) */
int32_t ae_CollisionBodyIndexOf(AeRam *r, int32_t slot);                      /* 800721B4 (a0 is an s16) */
uint32_t ae_CollisionBodyAlloc(AeRam *r);                                     /* 80072220: body address or 0 */
int32_t ae_CollisionBodyClear(AeRam *r, uint32_t body);                       /* 8007228C */
int32_t ae_CollisionBodyFree(AeRam *r, uint32_t body);                        /* 800722D4 */
int32_t ae_LoopSlotAdd(AeRam *r, int32_t id, uint32_t key);                   /* 80072334 */
int32_t ae_LoopSlotRef(AeRam *r, int32_t id, uint32_t key);                   /* 80072380 */
int32_t ae_LoopSlotSweep(AeRam *r);                                           /* 800723E8 */
int32_t ae_LoopSlotClearAll(AeRam *r);                                        /* 80072454 */

/* ---- per-frame update and body-vs-body tests ------------------------------------------------- */
int32_t ae_CollisionUpdate(AeRam *r, const AeBodyHooks *h);                   /* 8007247C (types 1 / 2: inline movement cases) */
int32_t ae_CollisionBodyGetCenter(AeRam *r, int32_t type, uint32_t body, uint32_t out); /* 80077C28 */
int32_t ae_BodyCollideWithBodies(AeRam *r, const AeBodyHooks *h, uint32_t slot);        /* 800777AC */
int32_t ae_BodyFindOverlapType1(AeRam *r, const AeBodyHooks *h, int32_t type, uint32_t slot);   /* 80077D18 */
int32_t ae_BodyFindOverlapType3to5(AeRam *r, const AeBodyHooks *h, int32_t type, uint32_t slot);/* 80077EE8 */
int32_t ae_BodyCollideBodiesDetailed(AeRam *r, const AeBodyHooks *h, int32_t type, uint32_t slot); /* 80078088 */
int32_t ae_BodyCheckPickupContact(AeRam *r, const AeBodyHooks *h, int32_t type, uint32_t slot);    /* 800788B8 */
uint32_t ae_BodyContactType(AeRam *r, uint32_t actor);                        /* 800392E4: actor+0x50 >> 28 */
int32_t ae_BodyTypeOfActor(AeRam *r, uint32_t actor);                         /* 80078864 */
int32_t ae_BodyContactSfx(AeRam *r, const AeBodyHooks *h, int32_t body_type, int32_t collider_type); /* 80078808 */

/* ---- SDK helpers used by the above (translated from their listings) --------------------------- */
/* VectorNormalS @8003019C: normalises the three s32 at `in` (read as GTE IR, i.e. low 16 bits) into three s16 at
   `out`, using the 192-entry s16 table at 0x800A9C14 of the RAM image. Returns what the original leaves in $v0
   (the sum of squares). The original `add` traps on signed overflow of the sum; that sets the error flag. */
int32_t ae_VectorNormalS(AeRam *r, uint32_t in, uint32_t out);

#ifdef __cplusplus
}
#endif
#endif
