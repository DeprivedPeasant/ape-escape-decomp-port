#ifndef AE_PLAYER2_H
#define AE_PLAYER2_H

#include "../core/ae_ram.h"
#include "../bodies/ae_bodies.h"
#include "../effects/ae_spawn.h"
#include "../sound/ae_sound.h"
#include "ae_player_act.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t (*AePlayer2PositionPaddingFn)(void *user, AeRam *r,
                                                uint32_t original_function);
typedef void (*AePlayer2PlaySfxFn)(void *user, AeRam *r, uint32_t sound_id);
typedef void (*AePlayer2SimpleHookFn)(void *user, AeRam *r);
typedef void (*AePlayer2StopGadgetSoundsFn)(void *user, AeRam *r, int32_t gadget);
typedef int32_t (*AePlayer2CanActivateGadgetFn)(void *user, AeRam *r, int32_t gadget);
typedef int32_t (*AePlayer2ConditionFn)(void *user, AeRam *r, uint32_t function);
typedef void (*AePlayer2ActionFn)(void *user, AeRam *r, uint32_t function);
typedef void (*AePlayer2AddressHookFn)(void *user, AeRam *r,
                                       uint32_t a0, uint32_t a1,
                                       uint32_t a2, uint32_t a3);
typedef void (*AePlayer2RumbleFn)(void *user, AeRam *r, int32_t motor,
                                  int32_t strength, int32_t frames);
typedef struct AePlayer2Hooks {
    void *user;
    AePlayer2PositionPaddingFn position_padding;
    AePlayer2PlaySfxFn PlaySfx;
    AePlayer2SimpleHookFn AimModeLeave;
    AePlayer2StopGadgetSoundsFn PlayerStopGadgetSounds;
    AePlayer2CanActivateGadgetFn PlayerCanActivateGadget;
    AePlayer2ConditionFn PlayerCondition;
    AePlayer2ActionFn PlayerAction;
    AePlayer2AddressHookFn AnimDecodeFrame;
    AePlayer2AddressHookFn MatrixFromFrontUp;
    AePlayer2AddressHookFn ScaleMatrixL;
    AePlayer2AddressHookFn ModelTransformSkinned;
    AePlayer2AddressHookFn ModelPrepCycleColors;
    AePlayer2AddressHookFn ModelEmitPrimsCycleColors;
    AePlayer2AddressHookFn ModelDraw;
    AePlayer2AddressHookFn DrawRibbonBetween;
    AePlayer2SimpleHookFn AimModeEnter;
    AePlayer2SimpleHookFn TrailRingReset;
    AePlayer2SimpleHookFn CameraFixedModeOff;
    AePlayer2SimpleHookFn AimRequestClear;
    AePlayer2AddressHookFn TurnTowardsY;
    AePlayer2RumbleFn PadRumble;
    AePlayer2AddressHookFn MatrixLookDir;
    AePlayer2AddressHookFn ModelTransformRigid;
    AePlayer2AddressHookFn ModelDrawWithOt;
    AePlayer2AddressHookFn ActorAnimDecodeEntry;
    AePlayer2AddressHookFn SetRotMatrix;
    AePlayer2AddressHookFn SetTransMatrix;
    AePlayer2AddressHookFn RotTransSV;
    AePlayer2AddressHookFn RotTransPers;
    AePlayer2AddressHookFn RotMatrixY;
    AePlayer2SimpleHookFn EventCamStartRandom;
} AePlayer2Hooks;

/* Small leaf routines from the second half of PlayerUpdate. */
int32_t ae_PlayerIsOnGround(AeRam *r);                    /* 800652E4 */
void ae_PlayerRunTransitions(AeRam *r, const AePlayer2Hooks *hooks,
                             const AePlayerActHooks *act, uint32_t transitions); /* 800649D8 */
void ae_PlayerUpdateHeldByObject(AeRam *r, const AePlayer2Hooks *hooks,
                                 const AePlayerActHooks *act); /* 80068634 */
int32_t ae_PlayerCheckWaterEntry(AeRam *r, const AePlayer2Hooks *hooks,
                                 const AeSpawnHooks *spawn); /* 800657F0 */
void ae_PlayerSplashStart(AeRam *r, const AeSpawnHooks *spawn,
                          const AePlayer2Hooks *hooks); /* 8006B848 */
int32_t ae_PlayerAccelXZ(AeRam *r, int32_t max_step);      /* 8006699C */
int32_t ae_PlayerAccelXYZ(AeRam *r, int32_t max_step);     /* 80066ADC */
void ae_PlayerStartHurtBlink(AeRam *r);                    /* 80066018 */
void ae_PlayerEnterSwimIfInWater(AeRam *r);                /* 80066094 */
void ae_PlayerApplyGadgetStance(AeRam *r, const AePlayerActHooks *act); /* 80067A1C */
void ae_PlayerResetAir(AeRam *r);                          /* 800665E0 */
void ae_PlayerFxResetBlink(AeRam *r);                      /* 8006CFA4 */
void ae_PlayerSpawnDustBehind(AeRam *r, const AePlayer2Hooks *hooks); /* 8006BF10 */
void ae_PlayerSpawnDust(AeRam *r, const AePlayer2Hooks *hooks);       /* 8006B70C */
void ae_FUN_8013f0c0(AeRam *r, const AePlayer2Hooks *hooks);          /* 8013F0C0, area overlay 18C */
void ae_PlayerFxSpawn10C(AeRam *r, const AeSpawnHooks *hooks);       /* 8006C2C0 */
void ae_EffectGroundImpact(AeRam *r, const AeWorldHooks *world, const AeSpawnHooks *spawn,
                           const AePlayer2Hooks *hooks, int32_t x, int32_t y, int32_t z); /* 8006B570 */
void ae_EffectGroundImpactAtPlayer(AeRam *r, const AeWorldHooks *world, const AeSpawnHooks *spawn,
                                   const AePlayer2Hooks *hooks); /* 8006BF84 */
void ae_PlayerSparkleBurst(AeRam *r, const AePlayer2Hooks *hooks);   /* 8006C204 */
void ae_PlayPlayerVoice(AeRam *r, const AePlayer2Hooks *hooks,
                       uint32_t voice_id, uint32_t fallback_sound); /* 8006896C */
void ae_PlayerOnWallBump(AeRam *r, const AePlayer2Hooks *hooks,
                         uint32_t normal); /* 800668A0 */
void ae_AimTableClear3(AeRam *r);                          /* 8007A304 */
void ae_PlayerFxClearDAT_800F5144(AeRam *r);               /* 8007AA64 */
int32_t ae_IsPauseMenuOpen(AeRam *r);                      /* 8007F5D4 */
void ae_RadarResetState(AeRam *r);                         /* 80099D20 */
void ae_RcCarSpawn(AeRam *r, const AePlayer2Hooks *player_hooks, const AeBodyHooks *body_hooks,
                   uint32_t x, uint32_t y, uint32_t z); /* 80099360 */
void ae_SpawnRandomParticle(AeRam *r, uint32_t position, uint32_t variant,
                            uint32_t kind, int32_t speed); /* 80070B70 */
void ae_PlayerFxSquashParticles(AeRam *r, const AeSpawnHooks *spawn,
                                const AePlayer2Hooks *hooks); /* 8006C92C */
void ae_PlayerFxFlattenedParticles(AeRam *r, const AeSpawnHooks *spawn,
                                   const AePlayer2Hooks *hooks); /* 8006CC28 */
void ae_PlayerDrawSquashed(AeRam *r, const AePlayer2Hooks *hooks); /* 800689F0 */
void ae_PlayerDrawSquashedWithFx(AeRam *r, const AePlayer2Hooks *hooks,
                                 const AeSpawnHooks *spawn); /* 80068ADC */
void ae_PlayerDrawSquashedFlattened(AeRam *r, const AePlayer2Hooks *hooks,
                                    const AeSpawnHooks *spawn); /* 80068B1C */
void ae_PlayerUpdateIdleAnim(AeRam *r, const AePlayer2Hooks *hooks,
                             const AePlayerActHooks *act); /* 800687D8 */
int32_t ae_PlayerSelectGadget(AeRam *r, const AePlayer2Hooks *hooks,
                              const AePlayerActHooks *act, const AeSpawnHooks *spawn,
                              const AeBodyHooks *bodies, const AeSoundHw *sound,
                              int32_t gadget, int32_t previous_gadget); /* 8007AC64 */
void ae_PlayerUpdateClimb(AeRam *r, const AePlayer2Hooks *hooks,
                          const AePlayerActHooks *act); /* 80067CAC */
void ae_PlayerUpdateRope(AeRam *r, const AePlayer2Hooks *hooks,
                         const AePlayerActHooks *act); /* 800681E4 */
int32_t ae_PlayerOnContact(AeRam *r, const AePlayer2Hooks *hooks,
                           const AePlayerActHooks *act, const AeWorldHooks *world,
                           const AeSpawnHooks *spawn, const AeSoundHw *sound); /* 80066FC8 */
void ae_MonkeyNetCatchDraw(AeRam *r, const AePlayer2Hooks *hooks); /* 8007BD38 */
void ae_MonkeyNetCatchUpdate(AeRam *r, const AePlayer2Hooks *hooks,
                             const AePlayerActHooks *act); /* 8007C2A0 */
int32_t ae_Max16(AeRam *r, int32_t left, int32_t right);  /* 800A1174 */
void ae_SpuSetMainVolume(AeRam *r, int32_t left, int32_t right); /* 800A2A14 */
int32_t ae_SoundSetVolumeMode(AeRam *r, int32_t mode);    /* 80086248 */

#ifdef __cplusplus
}
#endif
#endif
