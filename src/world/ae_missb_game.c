#include "../ui/ae_missb.h"

#include "ae_areaset.h"
#include "ae_progress.h"
#include "ae_zako_init.h"
#include "ae_zako2.h"
#include "ae_stage.h"
#include "ae_stagefade.h"
#include "ae_misc.h"
#include "../player/ae_player1.h"
#include "../player/ae_player_spawn.h"
#include "../camera/ae_camupd.h"
#include "../player/ae_player_act.h"
#include "../core/ae_sdk.h"

#define MB_STAGE_STATE             0x800F4350u
#define MB_STAGE_RESULT_WORDS      0x800DFDD8u
#define MB_FRAME_COUNTER           0x800EC350u
#define MB_XA_ARGUMENT_POINTER     0x800F0000u
#define MB_LEVEL_GRADE_TARGETS     0x800A67F0u
#define MB_LEVEL_GRADE_CAUGHT      0x80097C68u
#define MB_LEVEL_GRADE_THRESHOLDS  0x80097CC0u
#define MB_LEVEL_GRADE_ALWAYS_TWO  0x80097CA4u
#define MB_STAGE_LEVEL             0x124u
#define MB_STAGE_AREA              0x126u
#define MB_STAGE_KIND              0x120u
#define MB_STAGE_SUBKIND           0x122u
#define MB_STAGE_CAUGHT            0x166u
#define MB_STAGE_GOAL              0x188u
#define MB_STAGE_ALT_GOAL          0x186u
#define MB_STAGE_CLEAR_STATE       0x800B37E4u
#define MB_STAGE_INTRO_TIMER       0x800B37E0u
#define MB_STAGE_CLEAR_TIMER       0x800B37E6u
#define MB_STAGE_EXIT_INFO         0x12Cu
#define MB_STAGE_EXIT_AREA         0x12Au
#define MB_STAGE_EXIT_MODE         0x12Eu
#define MB_PLAYER                  0x800EC1E0u
#define MB_PLAYER_ATTACH_FLAGS     0xE0u
#define MB_PLAYER_TIMER            0x100u
#define MB_ACTOR_STATE_PTR         0x800F455Cu
#define MB_SPECIAL_ACTOR_PTR       0x800B4DC4u
#define MB_SPECIAL_FLAGS           0x800F5120u
#define MB_POPUP                   0x800E07C0u
#define MB_SPECIAL_COLLIDER_BASE   0x20Cu
#define MB_SPECIAL_COLLIDER_STRIDE 56u
#define MB_LEVEL_QUOTA_TABLE       0x800DFFE0u
#define MB_MONKEY_BASE             0x800E5500u
#define MB_MONKEY_STRIDE           0x250u
#define MB_MONKEY_ACTIVITY         0x7Au
#define MB_SPECIAL_ACTOR_SCALE     0x80u
#define MB_STAGE_REQUEST_EXIT      0x12Cu
#define MB_STAGE_REQUEST_AREA      0x126u
#define MB_STAGE_REQUEST_ARG       0x12Au
#define MB_STAGE_AREA_TYPE          0x122u
#define MB_CLEAR_SUBSTATE           0x800B37E4u
#define MB_CLEAR_DELAY              0x800B37E6u
#define MB_STAGE_INTRO_SUBSTATE     0x02u
#define MB_POPUP_VALUE              0x01u
#define MB_POPUP_TIMER              0x02u
#define MB_POPUP_KIND               0x04u
#define MB_POPUP_COLOR               0x05u
#define MB_POPUP_ICON               0x06u
#define MB_POPUP_ICON_ALT           0x07u
#define MB_POPUP_GLYPH              0x08u
#define MB_POPUP_POSITION           0x0Au
#define MB_AUX_MODEL_INSTANCE       0x800E07D0u
#define MB_AUX_MODEL_INSTANCE_DATA  0x14u
#define MB_AUX_MODEL_DATA           0x800E9C60u
#define MB_ACTOR_POSITION_X         0x24u
#define MB_ACTOR_ROTATION_SIN_X     0x60u
#define MB_ACTOR_ROTATION_COS_Z     0x64u
#define MB_ACTOR_ROTATION_SIN_X_ALT 0x80u
#define MB_ACTOR_ROTATION_COS_Z_ALT 0x84u
#define MB_ACTOR_COLLIDER           0x70u
#define MB_ACTOR_COLLIDER_INDEX     0x74u
#define MB_ACTOR_KIND               0x88u
#define MB_ACTOR_ANGLE_Y            0x78u
#define MB_COLLIDER_HALF_X          0x06u
#define MB_COLLIDER_HALF_Y          0x08u
#define MB_COLLIDER_HALF_Z          0x0Au

static int32_t mb_stage_result_byte(AeRam *r, uint32_t index)
{
    uint32_t word = ae_rd32(r, MB_STAGE_RESULT_WORDS +
                            (index & 0xFFFFFFFCu));
    return (int32_t)((word >> ((index & 3u) * 8u)) & 0xFFu);
}

static int32_t mb_area_get_level(AeRam *r, const AeMissbHooks *h, int32_t area)
{
    if (h == NULL || h->area_set == NULL) {
        ae_ram_fail(r, "AeMissbHooks.area_set not provided for AreaGetLevel");
        return 0;
    }
    return ae_AreaGetLevel(r, h->area_set, area);
}

/* 80085E08: the body builds only GPU packets for an aim-circle overlay. */
/* 8009543C */
int32_t ae_StageVictoryStart(AeRam *r, const AeMissbHooks *h)
{
    (void)ae_missb_call(r, h, "EventCamStartVictory", 0x8004BBF0u, 0u,
                        0u, 0u, 0u, 0u);
    (void)ae_BgmFadeOut(r, h);
    return 0;
}

/* 80094F48 */
int32_t ae_StageType_Clear_Init(AeRam *r, const AeMissbHooks *h)
{
    uint32_t state = MB_STAGE_STATE;
    ae_wr16(r, MB_CLEAR_DELAY, 90u);
    if (ae_IsTimeAttack(r) == 0) {
        ae_wr32(r, state + MB_PLAYER_TIMER, 0u);
        ae_wr16(r, MB_CLEAR_SUBSTATE, 0u);
    } else {
        ae_wr32(r, state + MB_PLAYER_TIMER, 0u);
        ae_wr16(r, MB_CLEAR_SUBSTATE,
                ae_rd16(r, state + MB_STAGE_SUBKIND) == 3u ? 1u : 2u);
    }

    uint32_t level = ae_rd16(r, state + MB_STAGE_LEVEL);
    uint32_t area = ae_rd16(r, state + MB_STAGE_AREA);
    if (level != 31u && area != 78u && area != 86u && level != 33u) {
        uint32_t frame = ae_rd32(r, MB_FRAME_COUNTER);
        uint32_t variant = frame % 5u;
        uint32_t caller_a2 = (uint32_t)(((uint64_t)frame * 0xCCCCCCCDull) >> 32);
        uint32_t xa_args[4] = {
            variant + 4u, MB_XA_ARGUMENT_POINTER, caller_a2,
            h != NULL ? h->caller_a3 : 0u
        };
        (void)ae_missb_call(r, h, "XaPlay", 0x80011F6Cu, 4u,
                            xa_args[0], xa_args[1], xa_args[2], xa_args[3]);
    }
    return 0;
}

/* 80095030 */
int32_t ae_StageType_Clear_Update(AeRam *r, const AeMissbHooks *h)
{
    uint32_t substate_addr = MB_CLEAR_SUBSTATE;
    int32_t substate = ae_rds16(r, substate_addr);
    uint32_t player = MB_PLAYER;

    if (substate == 0) {
        int32_t timer = ae_rds16(r, player + MB_PLAYER_TIMER) - 1;
        ae_wr16(r, player + MB_PLAYER_TIMER, (uint32_t)timer);
        if (timer <= 0) ae_wr16(r, substate_addr, 3u);
        return 0;
    }

    if (substate == 2) {
        int32_t timer = ae_rds16(r, player + MB_PLAYER_TIMER);
        if (timer > 0) {
            timer--;
            if (timer == 0) {
                ae_PlayerSetMode(r, h != NULL ? h->player_act : NULL, 0);
                ae_PlayerSetMode(r, h != NULL ? h->player_act : NULL, 11);
            }
            ae_wr16(r, player + MB_PLAYER_TIMER, (uint32_t)timer);
        }
    } else if (substate == 3) {
        ae_CutsceneModeOn(r);
        uint32_t state = MB_STAGE_STATE;
        uint32_t code = (ae_rd16(r, state + MB_STAGE_REQUEST_EXIT) & 0x6000u) | 3u;
        (void)ae_StageRequest(r, h != NULL ? h->stage : NULL,
                              (int32_t)code,
                              ae_rds16(r, state + MB_STAGE_REQUEST_AREA),
                              ae_rds16(r, state + MB_STAGE_REQUEST_ARG));
        ae_PlayerSetMode(r, h != NULL ? h->player_act : NULL, 0);
        if (ae_GetDAT_800B38B8(r) != 0) {
            (void)ae_HudPanelB_Start(r, 0, 0);
            (void)ae_HudPanelA_Start(r, 0);
            ae_HudShowHealth(r, 0);
        }
        ae_wr16(r, substate_addr, 4u);
        return 0;
    } else if (substate != 1) {
        return 0;
    }

    int32_t clear_delay = ae_rds16(r, MB_CLEAR_DELAY);
    if (clear_delay == 0) {
        (void)ae_PlaySfx(r, h != NULL ? h->sound_hw : NULL, 7);
        ae_HudPopupStart(r, 1);
        ae_wr16(r, substate_addr, 3u);
        ae_wr32(r, MB_STAGE_STATE + MB_PLAYER_TIMER, 0u);
        ae_wr16(r, MB_CLEAR_DELAY, 0xFFFFu);
    } else {
        int32_t whole = clear_delay / 30;
        int32_t remainder = clear_delay - whole * 30;
        if (remainder == 0) {
            (void)ae_PlaySfx(r, h != NULL ? h->sound_hw : NULL, 6);
            ae_HudPopupStart(r, whole + 1);
        }
        ae_wr16(r, MB_CLEAR_DELAY, (uint32_t)(clear_delay - 1));
    }

    return 0;
}

/* 8009546C */
int32_t ae_StageIntroStart(AeRam *r, const AeMissbHooks *h)
{
    ae_wr16(r, MB_STAGE_INTRO_TIMER, 0x50u);
    ae_wr16(r, MB_STAGE_INTRO_TIMER + MB_STAGE_INTRO_SUBSTATE, 0u);

    uint32_t state = MB_STAGE_STATE;
    int32_t area = ae_rds16(r, state + MB_STAGE_AREA);
    ae_wr32(r, state + MB_PLAYER_TIMER, 0u);
    (void)ae_missb_call(r, h, "PlayerPlaceAtEntry", 0x80032D38u, 3u,
                        1u, (uint32_t)area, 1u, 0u);
    (void)ae_missb_call(r, h, "EventCamStartDefault", 0x8004BC5Cu, 0u,
                        0u, 0u, 0u, 0u);
    return 0;
}

/* 80095588 */
int32_t ae_PlayerEnterCutsceneMode(AeRam *r, const AeMissbHooks *h)
{
    ae_PlayerSetMode(r, h != NULL ? h->player_act : NULL, 11);
    ae_PlayerSetState(r, h != NULL ? h->player_act : NULL, 0u);
    ae_wr32(r, MB_PLAYER + MB_PLAYER_ATTACH_FLAGS,
            ae_rd32(r, MB_PLAYER + MB_PLAYER_ATTACH_FLAGS) | 0x20u);
    return 0;
}

/* 800954C4 */
int32_t ae_StageIntroUpdate(AeRam *r, const AeMissbHooks *h)
{
    uint32_t state = MB_STAGE_STATE;
    int32_t area = ae_rds16(r, state + MB_STAGE_AREA);
    (void)ae_missb_call(r, h, "PlayerPlaceAtEntry", 0x80032D38u, 3u,
                        1u, (uint32_t)area, 1u, 0u);

    int32_t substate = ae_rds16(r, MB_STAGE_INTRO_TIMER + MB_STAGE_INTRO_SUBSTATE);
    if (substate == 2) {
        int32_t timer = ae_rds16(r, MB_STAGE_INTRO_TIMER) - 1;
        ae_wr16(r, MB_STAGE_INTRO_TIMER, (uint32_t)timer);
        if (timer < 0)
            ae_wr16(r, MB_STAGE_INTRO_TIMER + MB_STAGE_INTRO_SUBSTATE, 4u);
    } else if (substate == 0) {
        ae_wr16(r, MB_STAGE_INTRO_TIMER + MB_STAGE_INTRO_SUBSTATE, 2u);
        ae_wr16(r, MB_STAGE_INTRO_TIMER, 0x50u);
    } else if (substate == 4) {
        (void)ae_StageRequest(r, h != NULL ? h->stage : NULL,
                              0x14, area,
                              ae_rds16(r, state + MB_STAGE_EXIT_INFO));
    }
    return 0;
}

/* 80097454 */
int32_t ae_AreaMonkeyQuotaMet(AeRam *r, const AeMissbHooks *h,
                              uint32_t out_count)
{
    uint32_t count = 0u;
    uint32_t monkey = MB_MONKEY_BASE;
    for (uint32_t i = 0; i < 8u; ++i, monkey += MB_MONKEY_STRIDE) {
        uint8_t activity = ae_rd8(r, monkey + MB_MONKEY_ACTIVITY);
        if (activity == 2u || activity == 3u) count++;
    }
    ae_wr32(r, out_count, count);

    int32_t area = ae_rds16(r, MB_STAGE_STATE + MB_STAGE_AREA);
    int32_t level = mb_area_get_level(r, h, area);
    int32_t first_area = ae_GetLevelFirstArea(r, level);
    uint32_t area_offset = (uint32_t)(area - first_area);
    uint32_t required = ae_rd16(r, MB_LEVEL_QUOTA_TABLE + area_offset * 2u);
    return count >= required;
}

/* 80097A60 */
int32_t ae_StageGoalReached(AeRam *r, const AeMissbHooks *h)
{
    uint32_t state = MB_STAGE_STATE;
    int32_t caught = ae_rds16(r, state + MB_STAGE_CAUGHT);
    int32_t level = (int32_t)ae_rd16(r, state + MB_STAGE_LEVEL);
    int32_t goal = ae_rds16(r, state + MB_STAGE_GOAL);

    if (ae_rd16(r, state + MB_STAGE_KIND) == 19u) return caught >= goal;
    if (level == 22 || level == 17 || (level >= 24 && level < 30)) {
        uint32_t result_index = level == 22 ? 0u : (level == 17 ? 1u : 3u);
        if (mb_stage_result_byte(r, result_index) != 5) return 0;
        return caught >= goal;
    }

    int32_t exit_mode = ae_rds16(r, state + MB_STAGE_EXIT_MODE);
    if (exit_mode == 0) return caught >= goal;
    if (exit_mode == 1 || exit_mode == 2) {
        return caught >= ae_rds16(r, state + MB_STAGE_ALT_GOAL);
    }
    (void)h;
    return 0;
}

/* 80097BFC */
int32_t ae_StageGradeUpdate(AeRam *r)
{
    uint32_t state = MB_STAGE_STATE;
    if (ae_rd16(r, state + MB_STAGE_KIND) != 11u ||
        ae_rd16(r, state + MB_STAGE_SUBKIND) == 3u) return 0;

    uint32_t level = ae_rd16(r, state + MB_STAGE_LEVEL);
    int32_t caught = ae_rds16(r, state + MB_STAGE_CAUGHT);
    int32_t goal = ae_rds16(r, state + MB_STAGE_GOAL);
    int32_t alternate_goal = ae_rds16(r, state + MB_STAGE_ALT_GOAL);
    uint32_t grade_address = state + MB_STAGE_EXIT_MODE;
    if (level >= 17u && level < 31u) {
        uint32_t target = ae_rd32(r, MB_LEVEL_GRADE_TARGETS +
                                  (level - 17u) * 4u);
        if (target == MB_LEVEL_GRADE_CAUGHT) {
            ae_wr16(r, grade_address, caught < goal ? 1u : 2u);
        } else if (target == MB_LEVEL_GRADE_THRESHOLDS) {
            if (caught < goal) return 0;
            ae_wr16(r, grade_address, 1u);
            if (caught >= alternate_goal) ae_wr16(r, grade_address, 2u);
        } else if (target == MB_LEVEL_GRADE_ALWAYS_TWO) {
            ae_wr16(r, grade_address, 2u);
        }
    } else if (caught >= goal) {
        ae_wr16(r, grade_address, 1u);
        if (caught >= alternate_goal) ae_wr16(r, grade_address, 2u);
    }
    return 0;
}

/* 80098330 */
void ae_HudPopupStart(AeRam *r, int32_t kind)
{
    ae_wr8(r, MB_POPUP, (uint32_t)kind);
    ae_wr8(r, MB_POPUP + MB_POPUP_VALUE, 16u);
    ae_wr16(r, MB_POPUP + MB_POPUP_TIMER, 250u);
    ae_wr8(r, MB_POPUP + MB_POPUP_KIND, ((uint32_t)kind - 2u) << 5);
    ae_wr8(r, MB_POPUP + MB_POPUP_COLOR, 96u);
    ae_wr8(r, MB_POPUP + MB_POPUP_ICON, 31u);
    ae_wr8(r, MB_POPUP + MB_POPUP_ICON_ALT, 31u);
    ae_wr16(r, MB_POPUP + MB_POPUP_GLYPH, 0x7DADu);
    ae_wr16(r, MB_POPUP + MB_POPUP_POSITION, 59u);
    if (kind == 1) {
        ae_wr8(r, MB_POPUP + MB_POPUP_KIND, 96u);
        ae_wr8(r, MB_POPUP + MB_POPUP_ICON, 159u);
    }
}

/* 80098FB0 */
void ae_PlayerAuxModelInit(AeRam *r, const AeMissbHooks *h, uint32_t model)
{
    ae_ModelInit(r, h != NULL ? h->area_set : NULL,
                 MB_AUX_MODEL_INSTANCE, model);
    ae_wr32(r, MB_AUX_MODEL_INSTANCE + MB_AUX_MODEL_INSTANCE_DATA,
            MB_AUX_MODEL_DATA);
}

/* 8009B578 */
void ae_SpecialActorPlaceAtCollider(AeRam *r, const AeMissbHooks *h,
                                    int32_t kind, int32_t collider_index,
                                    int32_t angle)
{
    (void)h;
    uint32_t actor = ae_rd32(r, MB_SPECIAL_ACTOR_PTR);
    uint32_t collider = ae_rd32(r, MB_ACTOR_STATE_PTR) +
                        (uint32_t)collider_index * MB_SPECIAL_COLLIDER_STRIDE;
    ae_wr32(r, actor + MB_ACTOR_COLLIDER, collider);
    ae_wr32(r, actor + MB_ACTOR_COLLIDER_INDEX, (uint32_t)collider_index);
    ae_wr32(r, actor + MB_ACTOR_KIND, (uint32_t)kind);

    int32_t sine = ae_sdk_rsin(r, angle);
    int32_t cosine = ae_sdk_rcos(r, angle);
    ae_wr16(r, actor + MB_ACTOR_ROTATION_SIN_X, (uint32_t)sine);
    ae_wr16(r, actor + MB_ACTOR_ROTATION_SIN_X + 2u, 0u);
    ae_wr16(r, actor + MB_ACTOR_ROTATION_COS_Z, (uint32_t)cosine);
    ae_wr16(r, actor + MB_ACTOR_ROTATION_SIN_X_ALT, (uint32_t)sine);
    ae_wr16(r, actor + MB_ACTOR_ROTATION_SIN_X_ALT + 2u, 0u);
    ae_wr16(r, actor + MB_ACTOR_ROTATION_COS_Z_ALT, (uint32_t)cosine);

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t value = (uint32_t)(int32_t)ae_rds16(r, collider + axis * 2u);
        ae_wr32(r, actor + MB_ACTOR_POSITION_X + axis * 4u, value);
        ae_wr16(r, actor + MB_ACTOR_ANGLE_Y + axis * 2u, value);
    }

    if (kind == 1) {
        ae_wr32(r, MB_SPECIAL_FLAGS, ae_rd32(r, MB_SPECIAL_FLAGS) | 4u);
        ae_wr16(r, collider + MB_COLLIDER_HALF_X, 0x80u);
        ae_wr16(r, collider + MB_COLLIDER_HALF_Y, 0xC0u);
        ae_wr16(r, collider + MB_COLLIDER_HALF_Z, 0x80u);
    } else if (kind == 2) {
        ae_wr32(r, MB_SPECIAL_FLAGS, ae_rd32(r, MB_SPECIAL_FLAGS) | 8u);
        ae_wr16(r, collider + MB_COLLIDER_HALF_X, 0x80u);
        ae_wr16(r, collider + MB_COLLIDER_HALF_Y, 0x80u);
        ae_wr16(r, collider + MB_COLLIDER_HALF_Z, 0x80u);
    }
}
