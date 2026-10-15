#ifndef AE_PLAYERUPDATE_FRAME_H
#define AE_PLAYERUPDATE_FRAME_H

#include "ae_playerupdate.h"
#include "ae_player1.h"
#include "ae_player2.h"
#include "ae_player_spawn.h"
#include "ae_anim_events.h"
#include "../world/ae_stage.h"
#include "../world/ae_areaset.h"
#include "../world/ae_solid.h"

/* Services used by the enclosing update. Game logic calls the existing ports
 * directly; only renderer, archive, SDK and loaded callbacks are boundaries. */
typedef struct AePlayerUpdateFrameHooks {
    void *user;
    const AePlayerUpdateHooks *turn;
    const AePlayer1Hooks *player1;
    const AePlayer2Hooks *player2;
    const AePlayerActHooks *act;
    const AePlayerSpawnHooks *player_spawn;
    const AeAnimEventHooks *anim;
    const AeWorldHooks *world;
    const AeSolidHooks *solid;
    const AeStageHooks *stage;
    const AeAreaSetHooks *area_set;
    const AeSpawnHooks *spawn;
    const AeBodyHooks *body;
    const AeSoundHw *sound;
    int32_t (*XaPlay)(void *, AeRam *, uint32_t); /* 80011F6C */
    int32_t (*CdMgrQueueCommand)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t); /* 80012360 */
    int32_t (*CdIntToPos)(void *, AeRam *, uint32_t, uint32_t); /* 80025D20 */
    int32_t (*CdPosToInt)(void *, AeRam *, uint32_t); /* 80025E24 */
    int32_t (*memset)(void *, AeRam *, uint32_t, uint32_t, uint32_t); /* 800A3740 */
    void (*PlayerAnimateAndDraw)(void *, AeRam *); /* 80063628 */
    void (*FUN_80142808)(void *, AeRam *); /* 80142808 */
    void (*PlayerModeCallback)(void *, AeRam *, uint32_t);
    void (*PlayerAction)(void *, AeRam *, uint32_t);
    int32_t (*PlayerCheckGadgetSelect)(void *, AeRam *); /* 80067974: existing port verifies its decision only */
    /* Preserve observable leftover a1 in dependency tests. Voice ID 10 in the
     * real callee selects SFX 0x8A unconditionally, so this value is unused. */
    uint32_t (*PlayerVoiceFallback)(void *, AeRam *);
    /* PlayerSetMode(1) consumes a caller-held camera-script table through
     * AePlayerActHooks.incoming_a2; this is a logical table input, not a CPU
     * register frame. Hosts must supply it when entering actor mode. */
    uint32_t (*PlayerActorCameraTable)(void *, AeRam *);
    /* Original local addresses are observable by pointer-taking callees. The
     * host supplies storage whose initial bytes represent unwritten locals. */
    uint32_t (*PlayerUpdateLocalFrame)(void *, AeRam *);
} AePlayerUpdateFrameHooks;

void ae_PlayerUpdate(AeRam *r, const AePlayerUpdateFrameHooks *hooks); /* 80060780 */

#endif
