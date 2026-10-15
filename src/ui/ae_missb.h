#ifndef AE_MISSB_H
#define AE_MISSB_H

#include "../core/ae_ram.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The task batch uses a name-dispatched call boundary so native platform,
 * renderer, and out-of-batch game calls keep their original identity and word
 * arguments.  The callback receives at most the four MIPS argument registers. */
typedef int32_t (*AeMissbCallFn)(void *user, AeRam *ram, const char *name,
                                 uint32_t address, const uint32_t args[4],
                                 uint32_t argc);

struct AeMiscHooks;
struct AeProgressHooks;
struct AePauseMenuHooks;
struct AeHudHooks;
struct AeSoundHw;
struct AeStageFadeHooks;
struct AeCamupdHooks;
struct AePlayerActHooks;
struct AeAreaSetHooks;
struct AeGimmickAHooks;
struct AeZonesHooks;
struct AeWorldHooks;
struct AePlayer1Hooks;
struct AeStageHooks;

typedef struct AeMissbHooks {
    void *user;
    uint32_t stack_pointer;
    uint32_t caller_a3;
    int32_t caller_s1;
    int32_t caller_s2;
    AeMissbCallFn call;
    const struct AeMiscHooks *misc;
    const struct AeProgressHooks *progress;
    const struct AePauseMenuHooks *pause_menu;
    const struct AeHudHooks *hud;
    const struct AeSoundHw *sound_hw;
    const struct AeStageFadeHooks *stage_fade;
    const struct AeCamupdHooks *camupd;
    const struct AePlayerActHooks *player_act;
    const struct AeAreaSetHooks *area_set;
    const struct AeGimmickAHooks *gimmick_a;
    const struct AeZonesHooks *zones;
    const struct AeWorldHooks *world;
    const struct AePlayer1Hooks *player1;
    const struct AeStageHooks *stage;
} AeMissbHooks;

static inline int32_t ae_missb_call(AeRam *ram, const AeMissbHooks *hooks,
                                    const char *name, uint32_t address,
                                    uint32_t argc, uint32_t a0, uint32_t a1,
                                    uint32_t a2, uint32_t a3)
{
    uint32_t args[4] = {a0, a1, a2, a3};
    if (hooks == NULL || hooks->call == NULL) {
        ae_ram_fail(ram, "AeMissbHooks.%s not provided", name);
        return 0;
    }
    return hooks->call(hooks->user, ram, name, address, args, argc);
}

/* Pause menu / HUD functions from the task batch. */
int32_t ae_HudTextWidth(AeRam *ram, uint32_t text); /* 80082230 */
void ae_PauseDrawCaptureGrid(AeRam *ram, const AeMissbHooks *hooks); /* 80082398 */
void ae_HudSetGeomOffsetThenDraw(AeRam *ram, const AeMissbHooks *hooks,
                                 int32_t x, int32_t y); /* 800828F8 */
void ae_PauseDrawCounter(AeRam *ram, const AeMissbHooks *hooks); /* 80082A04 */
void ae_PauseMenuInitVolumes(AeRam *ram); /* 80082D40 */
int32_t ae_PauseMenuDrawSubScreen(AeRam *ram, const AeMissbHooks *hooks); /* 80082D6C */
uint32_t ae_StringTableIndex_Main(AeRam *ram, uint32_t strings, int32_t index); /* 800832FC */
void ae_ItemsClearFlags(AeRam *ram); /* 800847C4 */
void ae_ItemSaveCollectedMask(AeRam *ram, const AeMissbHooks *hooks); /* 800847EC */
void ae_MessageBoxOpen(AeRam *ram); /* 8008DEEC */
int32_t ae_BodyInsideBox(AeRam *ram, uint32_t point, uint32_t collider,
                         int32_t unused, uint32_t half_sizes); /* 8008EC70 */
int32_t ae_ColliderBoxContainsPoint(AeRam *ram, uint32_t collider,
                                    int32_t x, int32_t y, int32_t z); /* 8009091C */
int32_t ae_GimmickTouchCheck(AeRam *ram, int32_t include_monkeys,
                             uint32_t collider, int32_t unused2,
                             int32_t unused3); /* 80090754 */
int32_t ae_SwingDoorBodyDelta(AeRam *ram, uint32_t door, int32_t prop_index); /* 8008F308 */
int32_t ae_SlideDoorMove(AeRam *ram, uint32_t door, int32_t direction,
                         int32_t delta); /* 8008FAD0 */
int32_t ae_ColliderScriptRiseToGround(AeRam *ram,
                                      const AeMissbHooks *hooks,
                                      uint32_t collider, int32_t unused);
int32_t ae_ColliderScriptLiftUpdate(AeRam *ram,
                                    const AeMissbHooks *hooks,
                                    uint32_t collider, int32_t index);
int32_t ae_ColliderScriptRotateUpdate(AeRam *ram,
                                      const AeMissbHooks *hooks,
                                      uint32_t collider, int32_t index);
int32_t ae_ColliderScriptOrbitPlayer(AeRam *ram,
                                     const AeMissbHooks *hooks,
                                     uint32_t collider, int32_t index);
int32_t ae_ColliderScriptPropTransformUpdate(AeRam *ram,
                                             const AeMissbHooks *hooks,
                                             uint32_t collider, int32_t index);
int32_t ae_ColliderScriptNoOpA(AeRam *ram, uint32_t collider,
                               int32_t index);
int32_t ae_ColliderScriptNoOpB(AeRam *ram, uint32_t collider,
                               int32_t index);

/* Stage lifecycle, quota, save and actor helpers. */
int32_t ae_StageType_Clear_Init(AeRam *ram, const AeMissbHooks *hooks);
int32_t ae_StageType_Clear_Update(AeRam *ram, const AeMissbHooks *hooks);
int32_t ae_StageVictoryStart(AeRam *ram, const AeMissbHooks *hooks);
int32_t ae_StageIntroStart(AeRam *ram, const AeMissbHooks *hooks);
int32_t ae_StageIntroUpdate(AeRam *ram, const AeMissbHooks *hooks);
int32_t ae_PlayerEnterCutsceneMode(AeRam *ram, const AeMissbHooks *hooks);
int32_t ae_AreaMonkeyQuotaMet(AeRam *ram, const AeMissbHooks *hooks,
                              uint32_t out_count);
int32_t ae_StageGoalReached(AeRam *ram, const AeMissbHooks *hooks);
int32_t ae_StageGradeUpdate(AeRam *ram);
void ae_HudPopupStart(AeRam *ram, int32_t kind); /* 80098330 */
void ae_PlayerAuxModelInit(AeRam *ram, const AeMissbHooks *hooks,
                           uint32_t model); /* 80098FB0 */
void ae_SpecialActorPlaceAtCollider(AeRam *ram, const AeMissbHooks *hooks,
                                    int32_t kind, int32_t collider_index,
                                    int32_t angle); /* 8009B578 */
int32_t ae_AreaAggregateActiveMonkeyCount(AeRam *ram,
                                          const AeMissbHooks *hooks); /* 80097504 */
void ae_GimmickCylinderSetPlayerFlag(AeRam *ram, int32_t unused); /* 8009B3B0 */
int32_t ae_GimmickBreakableFloorAdjust(AeRam *ram, const AeMissbHooks *hooks,
                                       uint32_t collider, uint32_t gimmick); /* 80091598 */
int32_t ae_GimmickApplyPropScale(AeRam *ram, const AeMissbHooks *hooks,
                                 uint32_t collider, uint32_t prop); /* 80091AF0 */
int32_t ae_GimmickSetPropRotation(AeRam *ram, uint32_t direction,
                                  int32_t prop_index, int32_t amount); /* 80091C04 */
int32_t ae_GimmickGetGrabOffset(AeRam *ram, uint32_t mode,
                                uint32_t collider, uint32_t out_vector); /* 80093848 */
int32_t ae_GimmickPointWithinExpandedBox(AeRam *ram, uint32_t collider,
                                         uint32_t point, int32_t x_offset,
                                         int32_t z_offset); /* 80094330 */
int32_t ae_GimmickLinkedAreaActive(AeRam *ram, int32_t index); /* 80094A48 */
int32_t ae_GimmickMonkeyTouchingBox(AeRam *ram, uint32_t collider); /* 80094E5C */
int32_t ae_GimmickColliderContactUpdate(AeRam *ram,
                                        const AeMissbHooks *hooks,
                                        uint32_t gimmick, uint32_t collider); /* 80092324 */

/* Sound and sequencer helpers from the task batch. */
int32_t ae_SoundSilence(AeRam *ram, const AeMissbHooks *hooks); /* 80086148 */
int32_t ae_SoundStopAndRestoreVolume(AeRam *ram, const AeMissbHooks *hooks); /* 80086170 */
int32_t ae_BgmFadeOut(AeRam *ram, const AeMissbHooks *hooks); /* 8008760C */
int32_t ae_SoundEmitterVolume(AeRam *ram, const AeMissbHooks *hooks,
                              uint32_t emitter, int32_t voice,
                              int32_t volume, int32_t divisor); /* 80087BA4 */
int32_t ae_SoundSetStereo(AeRam *ram, int32_t stereo); /* 80087C64 */
void ae_SeqSetMonoFlag(AeRam *ram, int32_t mono); /* 800A2294 */
int32_t ae_SoundBankSetVolume(AeRam *ram, const AeMissbHooks *hooks,
                              int32_t bank, int32_t volume); /* 800A2640 */
void ae_SeqTrackSetPlayState(AeRam *ram, const AeMissbHooks *hooks,
                             int32_t track, int32_t state); /* 800A2B6C */
int32_t ae_SeqFadeTrackOut(AeRam *ram, const AeMissbHooks *hooks,
                           int32_t track, int32_t frames,
                           int32_t volume); /* 800A2F54 */
void ae_SoundStopAllVoices(AeRam *ram, const AeMissbHooks *hooks,
                           int32_t mode); /* 800A342C */
void ae_SfxVoiceSetPitch(AeRam *ram, const AeMissbHooks *hooks,
                         int32_t voice, int32_t pitch); /* 800A35FC */

/* Progress / save functions from the task batch. */
int32_t ae_ProgressChecksum(AeRam *ram, uint32_t bytes); /* 800962B0 */
int32_t ae_ProgressFlagSet(AeRam *ram, int32_t bit); /* 800962F4 */
int32_t ae_ProgressSetByte(AeRam *ram, int32_t index, int32_t value); /* 80096360 */
int32_t ae_ProgressGetStageResult(AeRam *ram, int32_t area); /* 800964B0 */
int32_t ae_LevelMonkeyCountGet(AeRam *ram, int32_t level); /* 800965EC */
int32_t ae_LevelProgressRecordExit(AeRam *ram, int32_t level); /* 80096638 */
int32_t ae_GetDAT_800B38B8(AeRam *ram); /* 80097620 */
uint32_t ae_LevelTableGet(AeRam *ram, int32_t level); /* 80097E60 */
int32_t ae_LevelCaughtCount(AeRam *ram, int32_t level); /* 80097FB0 */
int32_t ae_SaveBuildImage(AeRam *ram); /* 80098088 */
int32_t ae_SaveComputeChecksum(AeRam *ram, uint32_t buffer); /* 8009816C */

#ifdef __cplusplus
}
#endif
#endif
