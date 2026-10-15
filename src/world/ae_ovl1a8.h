#ifndef AE_OVL1A8_H
#define AE_OVL1A8_H

#include "../core/ae_ram.h"

/* Area 0x57 boss overlay, member 1A8.bin, loaded at 0x8013D000. */
#define AE_OVL1A8_BASE                 0x8013D000u
#define AE_OVL1A8_BOSS                 0x80142320u
#define AE_OVL1A8_ACTORS               0x80142338u
#define AE_OVL1A8_MODEL_HEADERS        0x80142330u
#define AE_OVL1A8_MODEL_HEADER_PTRS    0x80142334u
#define AE_OVL1A8_DEBRIS               0x8014278Cu
#define AE_OVL1A8_LOOP_HANDLE           0x80142798u
#define AE_OVL1A8_ENTRIES               0x801427A0u
#define AE_OVL1A8_SPARKS                0x801427C0u
#define AE_OVL1A8_WAVES                 0x80142940u
#define AE_OVL1A8_RIPPLES_LAST          0x80142B34u
#define AE_OVL1A8_DEBRIS_COUNT          80u
#define AE_OVL1A8_DEBRIS_SIZE           12u
#define AE_OVL1A8_ENTRY_COUNT           2u
#define AE_OVL1A8_ENTRY_SIZE            28u
#define AE_OVL1A8_SPARK_COUNT           32u
#define AE_OVL1A8_SPARK_SIZE            12u
#define AE_OVL1A8_WAVE_COUNT            4u
#define AE_OVL1A8_WAVE_SIZE             32u
#define AE_OVL1A8_RIPPLE_COUNT          32u
#define AE_OVL1A8_RIPPLE_SIZE           12u

typedef struct AeBodyHooks AeBodyHooks;
typedef struct AeCamupdHooks AeCamupdHooks;
typedef struct AeCameraHooks AeCameraHooks;
typedef struct AeSpawnHooks AeSpawnHooks;
typedef struct AeWorldHooks AeWorldHooks;
typedef struct AeZako2Hooks AeZako2Hooks;
typedef struct AeSoundHw AeSoundHw;
typedef struct AeStageHooks AeStageHooks;
typedef struct AePlayer2Hooks AePlayer2Hooks;
typedef struct AePlayer1Hooks AePlayer1Hooks;

typedef int32_t (*AeOvl1a8CallFn)(void *user, AeRam *r, const char *name,
                                  uint32_t address, uint32_t count,
                                  const uint32_t *args);
typedef int32_t (*AeOvl1a8RenderFn)(void *user, AeRam *r, const char *name,
                                    uint32_t address, uint32_t count,
                                    const uint32_t *args);

typedef struct AeOvl1a8Hooks {
    void *user;
    const AeBodyHooks *body;
    const AeCamupdHooks *camupd;
    const AeCameraHooks *camera;
    const AeSpawnHooks *spawn;
    const AeWorldHooks *world;
    const AeZako2Hooks *zako2;
    const AePlayer1Hooks *player1;
    const AePlayer2Hooks *player2;
    const AeSoundHw *sound_hw;
    const AeStageHooks *stage;
    AeOvl1a8CallFn original_call;
    AeOvl1a8RenderFn renderer;
} AeOvl1a8Hooks;

void ae_ovl1a8_BossModelInit(AeRam *r, const AeOvl1a8Hooks *h,
                             uint32_t model_index, uint32_t actor,
                             uint32_t model_out);                    /* 8013D000 */
void ae_ovl1a8_BossSpawnEffectNear(AeRam *r, const AeOvl1a8Hooks *h,
                                   uint32_t record, uint32_t owner,
                                   uint32_t body_value, uint32_t flags,
                                   uint32_t body_flags);              /* 8013D1FC */
void ae_ovl1a8_BossSpawnEffectNearWrap(AeRam *r, const AeOvl1a8Hooks *h,
                                       uint32_t record, uint32_t owner,
                                       uint32_t body_value,
                                       int32_t body_flags);            /* 8013D310 */
void ae_ovl1a8_BossSpawnDebris(AeRam *r, uint32_t position);         /* 8013D364 */
void ae_ovl1a8_BossUpdateDebris(AeRam *r, const AeOvl1a8Hooks *h);   /* 8013D4CC */
void ae_ovl1a8_BossRumbleLong(AeRam *r, const AeOvl1a8Hooks *h);     /* 8013D89C */
void ae_ovl1a8_BossRumbleShort(AeRam *r, const AeOvl1a8Hooks *h);    /* 8013D8C4 */
void ae_ovl1a8_BossRumbleMedium(AeRam *r, const AeOvl1a8Hooks *h);   /* 8013D8EC */
void ae_ovl1a8_BossSpawnDropPickups(AeRam *r, const AeOvl1a8Hooks *h,
                                    uint32_t position);               /* 8013D914 */
void ae_ovl1a8_BossResetLoopHandle(AeRam *r);                         /* 8013D9A8 */
void ae_ovl1a8_BossClearDebris(AeRam *r);                              /* 8013D33C */
void ae_ovl1a8_BossLoopStart(AeRam *r, const AeOvl1a8Hooks *h,
                              int32_t sound_id);                     /* 8013D9B8 */
void ae_ovl1a8_BossLoopStop(AeRam *r, const AeOvl1a8Hooks *h,
                             int32_t sound_id);                      /* 8013D9E0 */
void ae_ovl1a8_BossInit(AeRam *r, const AeOvl1a8Hooks *h);           /* 8013DA10 */
void ae_ovl1a8_BossDrawScaledModel(AeRam *r, const AeOvl1a8Hooks *h,
                                   uint32_t actor);                    /* 8013DDC4 */
void ae_ovl1a8_BossSpawnEffectGroupsN(AeRam *r, const AeOvl1a8Hooks *h,
                                      uint32_t position);              /* 8013DFB0 */
void ae_ovl1a8_BossAddEntry(AeRam *r, uint32_t source);               /* 8013E044 */
void ae_ovl1a8_BossClearEntries(AeRam *r);                             /* 8013E020 */
void ae_ovl1a8_BossUpdateEntries(AeRam *r, const AeOvl1a8Hooks *h);   /* 8013E120 */
int32_t ae_ovl1a8_BossUpdate(AeRam *r, const AeOvl1a8Hooks *h,
                             uint32_t camera_slot);                    /* 8013E4B4 */
void ae_ovl1a8_BossUpdateCameraSequence(AeRam *r,
                                        const AeOvl1a8Hooks *h,
                                        uint32_t camera_slot);         /* 8013E8A8 */
void ae_ovl1a8_BossWake(AeRam *r, const AeOvl1a8Hooks *h);            /* 8013EA24 */
void ae_ovl1a8_BossUpdateWakeSequence(AeRam *r,
                                      const AeOvl1a8Hooks *h,
                                      uint32_t camera_slot);           /* 8013EA9C */
void ae_ovl1a8_BossClearSparks(AeRam *r);                              /* 8013EE9C */
void ae_ovl1a8_BossAddSparks(AeRam *r, uint32_t actor,
                             uint32_t count);                         /* 8013EEC0 */
void ae_ovl1a8_BossUpdateSparks(AeRam *r, const AeOvl1a8Hooks *h,
                                uint32_t actor);                      /* 8013F06C */
void ae_ovl1a8_BossResetFlag(AeRam *r);                               /* 8013F370 */
void ae_ovl1a8_BossClearFlag(AeRam *r, uint32_t value);              /* 8013F398 */
void ae_ovl1a8_BossSetAnim(AeRam *r, uint32_t actor,
                           uint32_t animation);                       /* 8013F3A4 */
void ae_ovl1a8_BossStepAnim(AeRam *r, uint32_t actor);                /* 8013F3DC */
void ae_ovl1a8_BossUpdateSequence(AeRam *r,
                                  const AeOvl1a8Hooks *h);             /* 8013F464 */
void ae_ovl1a8_BossClearRipples(AeRam *r);                             /* 801407BC */
void ae_ovl1a8_BossClearTable8(AeRam *r);                              /* 8013FFFC */
int32_t ae_ovl1a8_BossAddWaveEntry(AeRam *r, const AeOvl1a8Hooks *h); /* 80140020 */
void ae_ovl1a8_BossUpdateWaves(AeRam *r, const AeOvl1a8Hooks *h);    /* 801400F4 */
void ae_ovl1a8_BossRumbleBig(AeRam *r, const AeOvl1a8Hooks *h);      /* 80140794 */
void ae_ovl1a8_BossSpawnGroundRipples(AeRam *r,
                                      const AeOvl1a8Hooks *h,
                                      uint32_t position,
                                      uint32_t max_ripples);            /* 801407E4 */
void ae_ovl1a8_BossUpdateRipples(AeRam *r, const AeOvl1a8Hooks *h);  /* 801409D4 */
void ae_ovl1a8_BossStart(AeRam *r, const AeOvl1a8Hooks *h);           /* 80140E70 */
void ae_ovl1a8_BossSlamImpact(AeRam *r, const AeOvl1a8Hooks *h,
                              uint32_t actor, uint32_t position);     /* 80140F08 */
void ae_ovl1a8_BossDeathBurst(AeRam *r, const AeOvl1a8Hooks *h,
                              uint32_t actor, uint32_t position);     /* 80140F68 */
void ae_ovl1a8_BossUpdateLateSequence(AeRam *r,
                                      const AeOvl1a8Hooks *h);         /* 80140FFC */

#endif
