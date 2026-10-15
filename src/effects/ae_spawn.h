#ifndef AE_SPAWN_H
#define AE_SPAWN_H
#include "../core/ae_ram.h"
#include "../world/ae_world.h"

/* Original RAM layouts; the particle offsets match effect_vm.c's 52-byte state. */
enum {
    AE_PARTICLE_SIZE=0x34, AE_PARTICLE_COUNT=100,
    AE_P_KIND=0, AE_P_TIMER=1, AE_P_PRIORITY=2, AE_P_LOOP_DEPTH=3,
    AE_P_SCRIPT=4, AE_P_FADE=0x12, AE_P_MODE=0x13, AE_P_POS=0x14,
    AE_P_VEL_X=0x1A, AE_P_SIZE=0x22, AE_P_TPAGE=0x2A, AE_P_RGB=0x30,
    AE_EFFECT_SIZE=0x54, AE_E_POS=0, AE_E_ANGLE=8, AE_E_TIMER=0xA,
    AE_E_FLAGS=0xC, AE_E_HEADER=0xE, AE_E_SPIN=0x14,
    AE_E_MATRIX=0x20, AE_E_DELAY=0x4E,
    AE_TRAIL_SIZE=0x10, AE_T_POS=0, AE_T_COLOR=8, AE_T_LIFE=0xA,
    AE_T_MODE=0xB, AE_T_STEP=0xC, AE_T_WIDTH=0xE,
    AE_SPARK_SIZE=12, AE_S_POS=0, AE_S_LIFE=6, AE_S_RGB=8,
    AE_PROJECTILE_SIZE=0x30, AE_J_HEADER=0, AE_J_OWNER=4, AE_J_RECORD=8,
    AE_J_KIND=0xC, AE_J_TIMER=0xD, AE_J_VARIANT=0xE, AE_J_MISC=0xF,
    AE_J_BEHAVIOR=0x10, AE_J_FLAG=0x11, AE_J_DRAW=0x12, AE_J_AGE=0x13,
    AE_J_U=0x16, AE_J_V=0x17, AE_J_CLUT=0x18, AE_J_TPAGE=0x1A,
    AE_J_POS=0x1C, AE_J_POS_PAD=0x22, AE_J_VEL=0x24, AE_J_GROUND=0x2A,
    AE_J_TAIL=0x2E,
    AE_RECORD_SIZE=0x34, AE_R_STATE=0, AE_R_KIND=1, AE_R_BYTE2=2,
    AE_R_AGE=3, AE_R_UPDATE=4, AE_R_OWNER=8, AE_R_MISC=0xC,
    AE_R_ORIGIN=0x10, AE_R_POS=0x1C, AE_R_SIZE=0x22, AE_R_RGB=0x23,
    AE_R_PARAM_B=0x26, AE_R_PARAM_C=0x27, AE_R_FLAGS=0x2A,
    AE_R_SCALE=0x2C, AE_R_FIELD30=0x30, AE_R_PARAM_A=0x32
};
#define AE_PARTICLES 0x800B9900u
#define AE_PARTICLE_CURSOR 0x800BAD50u
#define AE_PARTICLE_SCRIPTS 0x800B175Cu
#define AE_EFFECT_GROUPS 0x800B1828u
#define AE_EFFECT_DEFS 0x800B1BD4u
#define AE_EFFECTS 0x800BAF00u
#define AE_EFFECT_CURSOR 0x800B1BD0u
#define AE_EFFECT_MATRIX 0x800B1BB0u
#define AE_TRAILS 0x800F5010u
#define AE_TRAIL_CURSOR 0x800B1B74u
#define AE_SPARKS 0x800BAEA0u
#define AE_SPARK_CURSOR 0x800B1B41u
#define AE_PROJECTILES 0x800B8D30u
#define AE_PROJECTILE_RECORDS 0x800F5380u
#define AE_PROJECTILE_COLORS 0x800B2950u
#define AE_PROJECTILE_VARIANTS 0x800AF800u
#define AE_SPAWN_RAND_SEED 0x800AE8F0u
#define AE_SHAKE 0x800E6950u

typedef int32_t (*AeSpawnHookFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t);
typedef struct AeSpawnHooks {
    void *user;
    const AeWorldHooks *world;
    AeSpawnHookFn PlaySfx;          /* 800877F8: one sound id */
    AeSpawnHookFn MatrixLookDir;    /* 8001484C: direction -> 32-byte matrix */
    AeSpawnHookFn MatrixFromDirXZ;  /* 80014804: direction -> 32-byte matrix */
    /* Original group/burst position locals have an unwritten fourth halfword.
     * These callbacks provide that caller-owned value, independently per call.
     * It is copied by the original eight-byte position copies. */
    uint16_t (*position_padding)(void *, AeRam *, uint32_t original_function);
} AeSpawnHooks;

uint32_t ae_ParticleAlloc(AeRam *r);
void ae_SpawnOneParticle(AeRam *r, int32_t type, uint32_t pos, uint32_t table);
void ae_SpawnEffectGroup(AeRam *r, const AeSpawnHooks *h, int32_t type, uint32_t pos, uint32_t particles, uint32_t groups);
void ae_SpawnParticle(AeRam *r, const AeSpawnHooks *h, int32_t type, uint32_t pos);
void ae_TrailSpawn(AeRam *r, uint32_t pos, int32_t color, int32_t width, int32_t mode);
uint32_t ae_EffectObjAlloc(AeRam *r);
void ae_EffectObjSpawn(AeRam *r, const AeSpawnHooks *h, uint32_t pos, uint32_t def, int32_t count, int32_t delay, int32_t spacing, uint32_t direction);
void ae_EffectSpawnFromDef(AeRam *r, const AeSpawnHooks *h, uint32_t pos, uint32_t def, int32_t count, int32_t delay, uint32_t direction);
uint32_t ae_ProjectileAllocSlot(AeRam *r);
void ae_ProjectileInit(AeRam *r, uint32_t record, uint32_t origin, uint32_t pos, uint32_t kind);
void ae_ProjectileSetByte2(AeRam *r, uint32_t record, uint32_t value);
void ae_ProjectileSetColor(AeRam *r, uint32_t record, uint32_t index);
void ae_ProjectileSetUpdate(AeRam *r, uint32_t record, uint32_t a, uint32_t b, uint32_t c);
int32_t ae_ProjectileSpawn(AeRam *r, const AeSpawnHooks *h, uint32_t pos, uint32_t owner, int32_t kind, uint32_t velocity);
void ae_SpawnPuff(AeRam *r, const AeSpawnHooks *h, uint32_t pos);
void ae_EffectBigBurst(AeRam *r, const AeSpawnHooks *h, int32_t x, int32_t y, int32_t z);
void ae_SparkAdd(AeRam *r, uint32_t pos, uint32_t color);
void ae_SparkAddRandom(AeRam *r, uint32_t pos, uint32_t color, int32_t mode);
void ae_CameraShake(AeRam *r, int32_t level);
#endif
