#include "ae_ovl186b.h"

#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../player/ae_animdraw.h"
#include "../world/ae_progress.h"

#include <limits.h>
#include <string.h>

#define OVL186B_API_PLAY_SFX       0x800877F8u
#define OVL186B_API_BEGIN_FRAME    0x8005DF48u
#define OVL186B_API_DRAW_SYNC      0x800286CCu
#define OVL186B_API_VSYNC          0x8002E520u
#define OVL186B_API_INIT_DISPLAY   0x8005CFECu
#define OVL186B_API_RESET_GRAPH    0x80028350u
#define OVL186B_API_CLEAR_IMAGE    0x80028850u
#define OVL186B_API_PUT_DISP_ENV   0x80028EFCu
#define OVL186B_API_PUT_DRAW_ENV   0x80028D30u
#define OVL186B_API_DRAW_OTAG      0x80028CC0u
#define OVL186B_API_SAVE_HEADER    0x80095D44u
#define OVL186B_API_MEMCARD_OP     0x80010000u
#define OVL186B_API_PROGRESS_CHECKSUM 0x800962B0u
#define OVL186B_API_DRAW_MENU_ARROWS 0x8008211Cu
#define OVL186B_API_SET_STATE       0x8013784Cu
#define OVL186B_THREAD_FILE_LIST    0x8013D688u
#define OVL186B_THREAD_FILE_DETAIL  0x8013D764u
#define OVL186B_CARDMGR_BUSY_DETAIL 0x8013D218u
#define OVL186B_DISPLAY_ENV_TEMPLATE 0x8014EEE0u
#define OVL186B_SAVE_UI_DATA_BASE  0x800E3880u
#define OVL186B_SAVE_RECORD_BASE   0x800E1880u

#define OVL186B_SAVE_DATA_SELECTED_SLOT 0x06u
#define OVL186B_SAVE_DATA_CARD_RESULT   0x00u
#define OVL186B_SAVE_DATA_CARD_TYPE     0x02u
#define OVL186B_SAVE_DATA_CARD_BLOCKS   0x04u
#define OVL186B_SAVE_DATA_CARD_COUNT    0x06u
#define OVL186B_SAVE_DATA_CARD_STATUS   0x08u
#define OVL186B_SAVE_DATA_CARD_FLAGS    0x0Au
#define OVL186B_NAME_GRID_CURSOR        0x04u
#define OVL186B_NAME_GRID_ROW           0x06u
#define OVL186B_NAME_GRID_COLUMN        0x08u
#define OVL186B_NAME_GRID_VISIBLE       0x0Au
#define OVL186B_VECTOR_X                0x00u
#define OVL186B_VECTOR_Y                0x02u
#define OVL186B_VECTOR_Z                0x04u
#define OVL186B_CARDMGR_STATS_CHECKSUM  0x00u
#define OVL186B_CARDMGR_STATS_CARD_TYPE 0x02u
#define OVL186B_CARDMGR_STATS_BLOCKS    0x04u
#define OVL186B_CARDMGR_STATS_CARD_COUNT 0x06u
#define OVL186B_CARDMGR_STATS_STATUS    0x08u
#define OVL186B_CARDMGR_STATS_FLAGS     0x0Au

static uint32_t stack_base(const AeOvl186bHooks *h)
{
    return h != NULL && h->stack_pointer != 0u ? h->stack_pointer : 0x801FFF00u;
}

static uint32_t stack_local(const AeOvl186bHooks *h, uint32_t frame_size,
                            uint32_t local_offset)
{
    return stack_base(h) - frame_size + local_offset;
}

static int32_t sign16(uint32_t value)
{
    uint32_t low = value & 0xFFFFu;
    return (low & 0x8000u) != 0u ? (int32_t)(low | 0xFFFF0000u) : (int32_t)low;
}

static int32_t arithmetic_shift_right(int32_t value, uint32_t amount)
{
    amount &= 31u;
    if (amount == 0u) return value;
    if (value >= 0) return (int32_t)((uint32_t)value >> amount);
    return (int32_t)~((~(uint32_t)value) >> amount);
}

static int32_t call_named(AeRam *r, const AeOvl186bHooks *h,
                          AeOvl186bCallFn callback, const char *name,
                          uint32_t address, uint32_t argc,
                          const uint32_t *argv)
{
    if (callback == NULL) {
        ae_ram_fail(r, "AeOvl186b %s hook not provided", name);
        return 0;
    }
    return callback(h != NULL ? h->user : NULL, r, name, address, argc, argv);
}

static int32_t overlay_call(AeRam *r, const AeOvl186bHooks *h,
                            const char *name, uint32_t address,
                            uint32_t argc, const uint32_t *argv)
{
    return call_named(r, h, h != NULL ? h->overlay_call : NULL,
                      name, address, argc, argv);
}

static int32_t native_call(AeRam *r, const AeOvl186bHooks *h,
                           const char *name, uint32_t address,
                           uint32_t argc, const uint32_t *argv)
{
    return call_named(r, h, h != NULL ? h->native_call : NULL,
                      name, address, argc, argv);
}

static int32_t render_call(AeRam *r, const AeOvl186bHooks *h,
                           const char *name, uint32_t address,
                           uint32_t argc, const uint32_t *argv)
{
    return call_named(r, h, h != NULL ? h->renderer : NULL,
                      name, address, argc, argv);
}

#define OVERLAY0(r,h,n,a) overlay_call((r),(h),(n),(a),0u,NULL)
#define OVERLAY1(r,h,n,a,x0) overlay_call((r),(h),(n),(a),1u,(uint32_t[]){(uint32_t)(x0)})
#define OVERLAY2(r,h,n,a,x0,x1) overlay_call((r),(h),(n),(a),2u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1)})
#define OVERLAY3(r,h,n,a,x0,x1,x2) overlay_call((r),(h),(n),(a),3u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2)})
#define OVERLAY4(r,h,n,a,x0,x1,x2,x3) overlay_call((r),(h),(n),(a),4u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2),(uint32_t)(x3)})
#define NATIVE0(r,h,n,a) native_call((r),(h),(n),(a),0u,NULL)
#define NATIVE1(r,h,n,a,x0) native_call((r),(h),(n),(a),1u,(uint32_t[]){(uint32_t)(x0)})
#define NATIVE2(r,h,n,a,x0,x1) native_call((r),(h),(n),(a),2u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1)})
#define NATIVE3(r,h,n,a,x0,x1,x2) native_call((r),(h),(n),(a),3u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2)})
#define NATIVE4(r,h,n,a,x0,x1,x2,x3) native_call((r),(h),(n),(a),4u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2),(uint32_t)(x3)})
#define RENDER0(r,h,n,a) render_call((r),(h),(n),(a),0u,NULL)
#define RENDER1(r,h,n,a,x0) render_call((r),(h),(n),(a),1u,(uint32_t[]){(uint32_t)(x0)})
#define RENDER2(r,h,n,a,x0,x1) render_call((r),(h),(n),(a),2u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1)})
#define RENDER3(r,h,n,a,x0,x1,x2) render_call((r),(h),(n),(a),3u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2)})
#define RENDER4(r,h,n,a,x0,x1,x2,x3) render_call((r),(h),(n),(a),4u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2),(uint32_t)(x3)})

static AeOvl186bHooks nested_hooks(const AeOvl186bHooks *h,
                                  uint32_t caller_frame_size)
{
    AeOvl186bHooks next;
    if (h != NULL) {
        next = *h;
    } else {
        memset(&next, 0, sizeof(next));
    }
    next.stack_pointer = stack_base(h) - caller_frame_size;
    return next;
}

static void play_sfx(AeRam *r, const AeOvl186bHooks *h, int32_t id)
{
    (void)NATIVE1(r, h, "PlaySfx", OVL186B_API_PLAY_SFX, id);
}

static uint32_t save_slot_record(int32_t slot)
{
    return OVL186B_SAVE_RECORD_BASE + (uint32_t)slot * AE_OVL186B_SAVE_SLOT_STRIDE;
}

void ae_SaveUi_DrawDetail(AeRam *r, const AeOvl186bHooks *h)
{
    uint32_t changed = ae_rd32(r, AE_OVL186B_G_PAD_CHANGED);
    uint32_t held = ae_rd32(r, AE_OVL186B_G_PAD_HELD);
    if ((changed & 4u) != 0u) {
        ae_wr16(r, AE_OVL186B_G_SAVE_UI_CHOICE, 0u);
        if ((held & changed & 4u) != 0u) {
            ae_wr16(r, AE_OVL186B_G_SAVE_UI_CHOICE, 1u);
            play_sfx(r, h, 4);
        }
    } else if ((changed & 8u) != 0u) {
        ae_wr16(r, AE_OVL186B_G_SAVE_UI_CHOICE, 2u);
        if ((held & changed & 8u) != 0u) play_sfx(r, h, 4);
    }

    int32_t selected_slot = ae_rds16(r, AE_OVL186B_G_SAVE_UI_DATA +
                                     OVL186B_SAVE_DATA_SELECTED_SLOT);
    (void)OVERLAY2(r, h, "SaveUi_DrawSlot", 0x80138DC0u, 1u,
                   (uint32_t)selected_slot);
    (void)RENDER2(r, h, "SaveUi_DrawPortrait", 0x80139570u, 56, 72);
    (void)RENDER2(r, h, "SaveUi_DrawDetailPanel", 0x80139240u,
                  (uint32_t)selected_slot,
                  ae_rd16(r, AE_OVL186B_G_SAVE_UI_CHOICE));
}

int32_t ae_SaveUi_LastUsedEntry(AeRam *r, int32_t slot)
{
    uint32_t record = save_slot_record(slot) + AE_OVL186B_SAVE_ENTRY_USED;
    int32_t last = 1;
    for (int32_t entry = 0; entry < 25; ++entry) {
        if (ae_rd8(r, record + (uint32_t)entry) != 0u) {
            last = entry;
        }
    }
    return last;
}

int32_t ae_SaveUi_DrawMainWindow(AeRam *r, const AeOvl186bHooks *h)
{
    (void)OVERLAY4(r, h, "SaveUi_DrawWindow", 0x80136160u,
                   0x0Cu, 0x14u, 0x160u, 0xC4u);
    return 0;
}

void ae_SaveUi_DrawBackground3D(AeRam *r, const AeOvl186bHooks *h)
{
    uint32_t eye = stack_local(h, 112u, 48u);
    uint32_t target = stack_local(h, 112u, 56u);
    uint32_t matrix = stack_local(h, 112u, 16u);
    uint32_t position = stack_local(h, 112u, 64u);
    uint32_t rotation = stack_local(h, 112u, 72u);
    uint32_t model = ae_rd32(r, AE_OVL186B_G_VIEW_MODEL);
    int32_t angle = ae_rds32(r, AE_OVL186B_G_SAVE_UI_ANGLE);
    if (ae_ram_failed(r)) return;

    ae_wr16(r, eye, 0u);
    ae_wr16(r, eye + OVL186B_VECTOR_Y, 0u);
    ae_wr16(r, eye + OVL186B_VECTOR_Z, 0x0A40u);
    ae_wr16(r, target, 0u);
    ae_wr16(r, target + OVL186B_VECTOR_Y, 0u);
    ae_wr16(r, target + OVL186B_VECTOR_Z, 0u);
    (void)OVERLAY2(r, h, "SaveUi_LookAt", 0x801368E8u, eye, target);
    (void)NATIVE1(r, h, "SetGeomScreen", 0x8002FAF0u, 164u);

    (void)ae_sdk_MatrixIdentity(r, matrix);
    int32_t matrix_angle = angle < 0 ? angle + 7 : angle;
    matrix_angle = arithmetic_shift_right(matrix_angle, 3u);
    (void)ae_sdk_RotMatrixZ(r, matrix_angle, matrix);
    (void)ae_sdk_RotMatrixY(r, matrix_angle, matrix);
    (void)ae_sdk_RotMatrixX(r, matrix_angle, matrix);

    int32_t display_index = ae_rds32(r, AE_OVL186B_G_DISPLAY_INDEX);
    uint32_t display_record = AE_OVL186B_G_DISPLAY_STATE +
                              (uint32_t)display_index * AE_OVL186B_DISPLAY_RECORD_STRIDE;
    uint32_t screen_vertices = AE_OVL186B_G_SCREEN_VERTICES;
    (void)RENDER3(r, h, "MeshTransform", 0x800160ECu,
                  model, matrix, screen_vertices);
    (void)RENDER3(r, h, "ModelEmitPrimsAlt", 0x8001A414u,
                  display_record + AE_OVL186B_DISPLAY_OTAG_OFFSET,
                  screen_vertices, model);

    if (ae_rd16(r, AE_OVL186B_G_MENU_STATE_BLOCK) ==
        AE_OVL186B_MENU_STATE_RENDER) {
    int32_t selected = ae_rds16(r, AE_OVL186B_G_SAVE_UI_SELECTION);
        int32_t target_angle = (int32_t)(0u - (uint32_t)selected * 512u);
        int32_t current_angle = ae_rds32(r, AE_OVL186B_G_SAVE_UI_MODEL_ANGLE);
        int32_t lower_bound = (int32_t)((uint32_t)target_angle - 128u);
        int32_t upper_bound = (int32_t)((uint32_t)target_angle + 128u);
        if (current_angle < lower_bound) {
            current_angle = (int32_t)((uint32_t)current_angle + 128u);
        } else if (current_angle > upper_bound) {
            current_angle = (int32_t)((uint32_t)current_angle - 128u);
        } else {
            current_angle = target_angle;
        }
        ae_wr32(r, AE_OVL186B_G_SAVE_UI_MODEL_ANGLE, (uint32_t)current_angle);

        for (int32_t part = 0; part < 3; ++part) {
            int32_t part_angle = (int32_t)((uint32_t)current_angle +
                                           (uint32_t)part * 512u);
            int32_t sine = arithmetic_shift_right(ae_sdk_rsin(r, part_angle), 1u);
            int32_t cosine_y = arithmetic_shift_right(ae_sdk_rcos(r, part_angle), 2u) - 200;
            int32_t cosine_z = 3800 - arithmetic_shift_right(ae_sdk_rcos(r, part_angle), 1u);
            ae_wr16(r, position + OVL186B_VECTOR_X, (uint32_t)sine);
            ae_wr16(r, position + OVL186B_VECTOR_Y, (uint32_t)cosine_y);
            ae_wr16(r, position + OVL186B_VECTOR_Z, (uint32_t)cosine_z);
            ae_wr16(r, rotation + OVL186B_VECTOR_X, 128u);
            ae_wr16(r, rotation + OVL186B_VECTOR_Y,
                    (uint32_t)(0u - ((uint32_t)ae_rd16(r,
                    AE_OVL186B_G_MENU_MODEL_BASE_ROTATION) << 3)));
            ae_wr16(r, rotation + OVL186B_VECTOR_Z, 0u);
            if (part != 0 || ae_rds16(r, AE_OVL186B_G_SAVE_UI_MODEL_DRAW_FIRST) != 0) {
                (void)RENDER4(r, h, "SaveUi_DrawModel", 0x8013A804u,
                              position, rotation,
                              model + (uint32_t)(part + 1) * AE_OVL186B_MENU_MODEL_STRIDE,
                              0x1000u);
            }
        }
    }

    int32_t model_step = ae_StepToward(r,
                                      ae_rds32(r, AE_OVL186B_G_SAVE_UI_MODEL_STEP),
                                      16, 32);
    int32_t updated_angle = (int32_t)((uint32_t)ae_rds32(r,
                               AE_OVL186B_G_SAVE_UI_ANGLE) + (uint32_t)model_step);
    ae_wr32(r, AE_OVL186B_G_SAVE_UI_MODEL_STEP, (uint32_t)model_step);
    ae_wr32(r, AE_OVL186B_G_SAVE_UI_ANGLE, (uint32_t)updated_angle);
}

int32_t ae_SaveUi_ConfirmPressed(AeRam *r)
{
    if (ae_rds16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE) != 1) return 0;
    uint32_t pressed = ae_rd32(r, AE_OVL186B_G_PAD_HELD) &
                       ae_rd32(r, AE_OVL186B_G_PAD_CHANGED) & 0x60u;
    return pressed != 0u;
}

int32_t ae_SaveUi_CancelPressed(AeRam *r)
{
    if (ae_rds16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE) != 1) return 0;
    uint32_t pressed = ae_rd32(r, AE_OVL186B_G_PAD_HELD) &
                       ae_rd32(r, AE_OVL186B_G_PAD_CHANGED) & 0x90u;
    return pressed != 0u;
}

int32_t ae_SaveUi_FadeTick(AeRam *r, const AeOvl186bHooks *h)
{
    int32_t state = ae_rds16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE);
    if (state == 1) return 0;

    if (state == 0) {
        (void)RENDER0(r, h, "SaveUi_FadeDraw", 0x8013A70Cu);
        uint32_t timer = (ae_rd16(r, AE_OVL186B_G_SAVE_UI_FADE_TIMER) - 1u) & 0xFFFFu;
        ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_TIMER, timer);
        if (sign16(timer) < 0) {
            ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE, 1u);
            ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_TIMER, 0u);
        }
        return 0;
    }

    if (state == 2) {
        (void)RENDER0(r, h, "SaveUi_FadeDraw", 0x8013A70Cu);
        uint32_t timer = (ae_rd16(r, AE_OVL186B_G_SAVE_UI_FADE_TIMER) + 1u) & 0xFFFFu;
        ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_TIMER, timer);
        if (sign16(timer) >= 31) {
            ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE, 1u);
            ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_TIMER, 0u);
            return -1;
        }
    }
    return 0;
}

int32_t ae_SaveUi_YesNoUpdate(AeRam *r, const AeOvl186bHooks *h)
{
    uint32_t selection_address = AE_OVL186B_G_SAVE_UI_YES_NO;
    (void)OVERLAY1(r, h, "SaveUi_DrawYesNo", 0x8013FFECu,
                   (uint32_t)ae_rds16(r, selection_address));
    if (ae_rds16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE) != 1) return 0;

    uint32_t repeat = ae_rd32(r, AE_OVL186B_G_PAD_REPEAT);
    if (repeat == 0x1000u || repeat == 0x4000u) {
        play_sfx(r, h, 0);
        uint32_t current = ae_rd16(r, selection_address);
        ae_wr16(r, selection_address, current == 0u ? 1u : 0u);
    }
    if (ae_SaveUi_ConfirmPressed(r)) {
        play_sfx(r, h, 3);
        return ae_rds16(r, selection_address) + 1;
    }
    return 0;
}

static void save_ui_set_state(AeRam *r, const AeOvl186bHooks *h,
                              int32_t state, int32_t argument)
{
    (void)OVERLAY2(r, h, "SaveUi_SetState", OVL186B_API_SET_STATE,
                   state, argument);
}

static void file_select_move(AeRam *r, int32_t direction)
{
    uint32_t selection_address = AE_OVL186B_G_SAVE_UI_SELECTION;
    uint32_t table = AE_OVL186B_G_FILE_SELECT_OPTIONS;
    int32_t selection = ae_rds16(r, selection_address);

    if (direction < 0) {
        selection = (int16_t)((uint16_t)selection - 1u);
        ae_wr16(r, selection_address, (uint16_t)selection);
        if (selection < 0) {
            selection = 4;
            ae_wr16(r, selection_address, 4u);
        }
    } else {
        selection = (int16_t)((uint16_t)selection + 1u);
        ae_wr16(r, selection_address, (uint16_t)selection);
        if (selection >= 5) {
            selection = 0;
            ae_wr16(r, selection_address, 0u);
        }
    }

    while (ae_rds16(r, table + (uint32_t)selection * 2u) != 1) {
        if (direction < 0) {
            --selection;
            if (selection < 0) selection = 4;
        } else {
            ++selection;
            if (selection >= 5) selection = 0;
        }
        ae_wr16(r, selection_address, (uint32_t)selection);
    }
}

int32_t ae_SaveUi_FileSelectUpdate(AeRam *r, const AeOvl186bHooks *h)
{
    if (ae_rds16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE) != 1) return 0;

    uint32_t changed = ae_rd32(r, AE_OVL186B_G_PAD_CHANGED);
    uint32_t held = ae_rd32(r, AE_OVL186B_G_PAD_HELD);
    if ((changed & 4u) != 0u) {
        ae_wr16(r, AE_OVL186B_G_SAVE_UI_CHOICE, 0u);
        if ((held & changed & 4u) != 0u) {
            ae_wr16(r, AE_OVL186B_G_SAVE_UI_CHOICE,
                    (uint32_t)ae_rds16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE));
            play_sfx(r, h, 11);
        }
    } else if ((changed & 8u) != 0u) {
        ae_wr16(r, AE_OVL186B_G_SAVE_UI_CHOICE, 2u);
        if ((held & changed & 8u) != 0u) play_sfx(r, h, 11);
    }

    uint32_t selection_address = AE_OVL186B_G_SAVE_UI_SELECTION;
    if (ae_rds16(r, AE_OVL186B_G_SAVE_UI_CONFIRM_MODE) == 0) {
        uint32_t repeat = ae_rd32(r, AE_OVL186B_G_PAD_REPEAT);
        if (repeat == 0x1000u || repeat == 0x4000u) {
            play_sfx(r, h, 0);
            file_select_move(r, repeat == 0x1000u ? -1 : 1);
        }

        int32_t selection = ae_rds16(r, selection_address);
        if (selection == 0) {
            if (ae_SaveUi_ConfirmPressed(r)) {
                play_sfx(r, h, 3);
                return -4;
            }
            return 0;
        }
        if (selection < 0 || selection >= 5) return 0;
        if (!ae_SaveUi_ConfirmPressed(r)) return 0;

        ae_wr16(r, AE_OVL186B_G_SAVE_UI_CONFIRM_MODE, 1u);
        ae_wr16(r, AE_OVL186B_G_SAVE_UI_SELECTED_SLOT, 0u);
        play_sfx(r, h, 3);
        return 0;
    }

    if (ae_SaveUi_CancelPressed(r)) {
        ae_wr16(r, AE_OVL186B_G_SAVE_UI_CONFIRM_MODE, 0u);
        play_sfx(r, h, 1);
        return 0;
    }
    if (!ae_SaveUi_ConfirmPressed(r)) {
        uint32_t repeat = ae_rd32(r, AE_OVL186B_G_PAD_REPEAT);
        uint32_t slot_address = AE_OVL186B_G_SAVE_UI_SELECTED_SLOT;
        if (repeat == 0x1000u) {
            play_sfx(r, h, 0);
            int32_t slot = (int16_t)((uint16_t)ae_rd16(r, slot_address) - 1u);
            if (slot < 0) slot = 3;
            ae_wr16(r, slot_address, (uint32_t)slot);
        } else if (repeat == 0x4000u) {
            play_sfx(r, h, 0);
            int32_t slot = (int16_t)((uint16_t)ae_rd16(r, slot_address) + 1u);
            if (slot >= 4) slot = 0;
            ae_wr16(r, slot_address, (uint32_t)slot);
        }
        return 0;
    }

    int32_t selection = ae_rds16(r, AE_OVL186B_G_SAVE_UI_SELECTION);
    if (selection < 1 || selection >= 5) return 0;
    uint32_t record = save_slot_record(
        ae_rds16(r, AE_OVL186B_G_SAVE_UI_SELECTED_SLOT));
    int32_t entry_type = ae_rds16(r, record + AE_OVL186B_SAVE_ENTRY_TYPE);

    if (selection == 1) {
        play_sfx(r, h, 3);
        if (entry_type >= 0) {
            save_ui_set_state(r, h, 13, 0);
        } else if (ae_rds16(r, AE_OVL186B_G_SAVE_UI_CARD_AVAILABLE) == 0) {
            (void)NATIVE2(r, h, "MemCardOp", OVL186B_API_MEMCARD_OP, 0u, 0u);
            save_ui_set_state(r, h, 6, 0);
        } else {
            save_ui_set_state(r, h, 4, 0);
        }
    } else if (selection == 2) {
        if (entry_type < 0) {
            play_sfx(r, h, 48);
        } else {
            play_sfx(r, h, 3);
            save_ui_set_state(r, h, 5, 0);
        }
    } else if (selection == 3) {
        if (entry_type < 0) {
            play_sfx(r, h, 48);
        } else {
            play_sfx(r, h, 3);
            save_ui_set_state(r, h, 15, 0);
        }
    } else {
        if (entry_type < 0) {
            play_sfx(r, h, 48);
        } else {
            play_sfx(r, h, 3);
            save_ui_set_state(r, h, 7, 0);
        }
    }
    return 0;
}

int32_t ae_SaveUi_OnCardResult_7(AeRam *r, const AeOvl186bHooks *h)
{
    uint32_t raw_result = (ae_rd16(r, OVL186B_SAVE_UI_DATA_BASE +
                                   OVL186B_SAVE_DATA_CARD_RESULT) + 8u) & 0xFFFFu;
    int32_t result = sign16(raw_result);
    if ((uint32_t)result >= 10u) return 0;
    switch (result) {
    case 1:
        (void)OVERLAY2(r, h, "SaveUi_SetState", 0x8013784Cu, 2u, 0u);
        break;
    case 2:
        (void)OVERLAY2(r, h, "SaveUi_SetState", 0x8013784Cu, 1u,
                       ae_rds16(r, AE_OVL186B_G_SAVE_NAME_LENGTH));
        break;
    case 3:
        (void)OVERLAY2(r, h, "SaveUi_SetState", 0x8013784Cu, 44u, 0u);
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_SaveUi_ConfirmDialogUpdate(AeRam *r, const AeOvl186bHooks *h)
{
    AeOvl186bHooks nested = nested_hooks(h, 24u);
    (void)OVERLAY0(r, h, "SaveUi_DrawStateOverlay", 0x8014016Cu);
    int32_t choice = ae_SaveUi_YesNoUpdate(r, h);
    (void)ae_SaveUi_DrawMainWindow(r, h);
    ae_SaveUi_DrawBackground3D(r, &nested);
    if (choice == 0) return 0;

    if (choice == 1) {
        int32_t slot = ae_rds16(r, AE_OVL186B_G_SAVE_UI_DATA +
                                OVL186B_SAVE_DATA_SELECTED_SLOT);
        uint32_t from = AE_OVL186B_G_SAVE_NAME_BUFFER;
        uint32_t to = save_slot_record(slot) + AE_OVL186B_SAVE_SLOT_NAME;
        for (uint32_t i = 0; i < AE_OVL186B_SAVE_NAME_LENGTH; ++i) {
            ae_wr8(r, to + i, ae_rd8(r, from + i));
        }
        (void)NATIVE2(r, h, "MemCardOp", OVL186B_API_MEMCARD_OP, 2u, 0u);
        (void)OVERLAY2(r, h, "SaveUi_SetState", 0x8013784Cu, 17u, 0u);
    } else {
        (void)OVERLAY2(r, h, "SaveUi_SetState", 0x8013784Cu, 1u,
                       ae_rds16(r, AE_OVL186B_G_SAVE_NAME_LENGTH));
    }
    return 0;
}

int32_t ae_SaveUi_NameEntryInit(AeRam *r, const AeOvl186bHooks *h)
{
    static const uint32_t reset_words[] = {
        AE_OVL186B_G_SAVE_NAME_CURSOR_X,
        AE_OVL186B_G_SAVE_NAME_CURSOR_Y,
        AE_OVL186B_G_SAVE_NAME_FIELD_5AE
    };
    for (size_t i = 0; i < sizeof(reset_words) / sizeof(reset_words[0]); ++i) {
        ae_wr16(r, reset_words[i], 0u);
    }
    ae_wr16(r, AE_OVL186B_G_SAVE_NAME_PAGE, 4u);
    ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B0, 44u);
    ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B2, 11u);
    ae_wr16(r, AE_OVL186B_G_SAVE_NAME_GRID + OVL186B_NAME_GRID_CURSOR, 1u);
    ae_wr16(r, AE_OVL186B_G_SAVE_NAME_GRID + OVL186B_NAME_GRID_ROW, 0xFFFFu);
    ae_wr16(r, AE_OVL186B_G_SAVE_NAME_GRID + OVL186B_NAME_GRID_COLUMN, 0xFFFFu);
    ae_wr16(r, AE_OVL186B_G_SAVE_NAME_LENGTH,
            ae_rd16(r, AE_OVL186B_G_SAVE_UI_NAME_LENGTH));

    int32_t selected = ae_rds16(r, AE_OVL186B_G_SAVE_UI_DATA +
                                OVL186B_SAVE_DATA_SELECTED_SLOT);
    uint32_t record = save_slot_record(selected);
    uint32_t source;
    uint32_t name = AE_OVL186B_G_SAVE_NAME_BUFFER;
    if (ae_rd8(r, record + AE_OVL186B_SAVE_SLOT_NAME) == 0u) {
        uint32_t default_name = (uint32_t)OVERLAY0(r, h, "SaveUi_GetBuffer2",
                                                   0x8013FD44u);
        uint32_t default_character = (h != NULL ? h->caller_s0 : 0u) +
                                     AE_OVL186B_SAVE_DEFAULT_CHAR_BIAS;
        ae_wr8(r, default_name + 4u, default_character);
        source = default_name;
    } else {
        source = record + AE_OVL186B_SAVE_SLOT_NAME;
    }
    for (uint32_t i = 0; i < AE_OVL186B_SAVE_NAME_LENGTH; ++i) {
        ae_wr8(r, name + i, ae_rd8(r, source + i));
    }

    for (uint32_t i = 0; i < 5u; ++i) {
        ae_wr16(r, AE_OVL186B_G_SAVE_NAME_TIMERS + i * 2u, 0u);
    }
    return 0;
}

static void set_name_grid_page(AeRam *r, int32_t current_page, int32_t direction)
{
    uint32_t grid = AE_OVL186B_G_SAVE_NAME_GRID;
    uint32_t page = AE_OVL186B_G_SAVE_NAME_PAGE;
    uint32_t cursor = AE_OVL186B_G_SAVE_NAME_CURSOR_X;
    switch (current_page) {
    case 0:
        if (direction == 8) {
            ae_wr16(r, page, 1u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5AE, 90u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B0, 90u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B2, 15u);
            ae_wr16(r, grid + OVL186B_NAME_GRID_CURSOR, 0xFFFFu);
            ae_wr16(r, grid + OVL186B_NAME_GRID_ROW, 1u);
        } else {
            ae_wr16(r, page, 2u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5AE, 180u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B0, 44u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B2, 11u);
            ae_wr16(r, grid + OVL186B_NAME_GRID_CURSOR, 0xFFFFu);
            ae_wr16(r, grid + OVL186B_NAME_GRID_COLUMN, 1u);
        }
        ae_wr16(r, cursor, 0u);
        break;
    case 1:
        if (direction == 8) {
            ae_wr16(r, page, 2u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5AE, 180u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B0, 44u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B2, 11u);
            ae_wr16(r, grid + OVL186B_NAME_GRID_ROW, 0xFFFFu);
            ae_wr16(r, grid + OVL186B_NAME_GRID_COLUMN, 1u);
        } else {
            ae_wr16(r, page, 0u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5AE, 0u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B0, 90u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B2, 15u);
            ae_wr16(r, grid + OVL186B_NAME_GRID_ROW, 0xFFFFu);
            ae_wr16(r, grid + OVL186B_NAME_GRID_CURSOR, 1u);
        }
        ae_wr16(r, cursor, 0u);
        break;
    case 2:
        if (direction == 8) {
            ae_wr16(r, page, 0u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5AE, 0u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B0, 90u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B2, 15u);
        } else {
            ae_wr16(r, page, 1u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5AE, 90u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B0, 90u);
            ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B2, 15u);
            ae_wr16(r, grid + OVL186B_NAME_GRID_COLUMN, 0xFFFFu);
            ae_wr16(r, grid + OVL186B_NAME_GRID_ROW, 1u);
        }
        if (direction == 8) {
            ae_wr16(r, grid + OVL186B_NAME_GRID_COLUMN, 0xFFFFu);
            ae_wr16(r, grid + OVL186B_NAME_GRID_CURSOR, 1u);
        }
        ae_wr16(r, cursor, 0u);
        break;
    case 3:
        ae_wr16(r, page, 4u);
        ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5AE, 0u);
        ae_wr16(r, grid + OVL186B_NAME_GRID_CURSOR, 1u);
        ae_wr16(r, grid + OVL186B_NAME_GRID_ROW, 0xFFFFu);
        break;
    case 4:
        ae_wr16(r, page, 3u);
        ae_wr16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5AE, 44u);
        ae_wr16(r, grid + OVL186B_NAME_GRID_CURSOR, 0xFFFFu);
        ae_wr16(r, grid + OVL186B_NAME_GRID_ROW, 1u);
        break;
    default:
        break;
    }
}

int32_t ae_SaveUi_NameEntryUpdate(AeRam *r, const AeOvl186bHooks *h)
{
    for (uint32_t i = 0; i < 5u; ++i) {
        uint32_t address = AE_OVL186B_G_SAVE_NAME_TIMERS + i * 2u;
        int32_t timer = ae_rds16(r, address);
        ae_wr16(r, address, timer > 0 ? (uint32_t)(timer - 1) : 0u);
    }

    uint32_t buttons = ae_rd32(r, AE_OVL186B_G_PAD_HELD) &
                       ae_rd32(r, AE_OVL186B_G_PAD_CHANGED);
    if ((buttons & 0x800u) != 0u) {
        play_sfx(r, h, 2);
        return 1;
    }

    uint32_t cursor_x_address = AE_OVL186B_G_SAVE_NAME_CURSOR_X;
    uint32_t cursor_y_address = AE_OVL186B_G_SAVE_NAME_CURSOR_Y;
    if ((buttons & 0x60u) != 0u) {
        play_sfx(r, h, 3);
        int32_t cell = ae_rds16(r, cursor_x_address);
        int32_t table_offset = ae_rds16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5AE);
        int32_t name_offset = ae_rds16(r, cursor_y_address);
        uint8_t character = (uint8_t)ae_rd8(r, 0x8013C550u +
                                            (uint32_t)(cell + table_offset));
        ae_wr8(r, AE_OVL186B_G_SAVE_NAME_BUFFER + (uint32_t)name_offset, character);
        ++name_offset;
        if (name_offset >= 5) name_offset = 4;
        ae_wr16(r, cursor_y_address, (uint32_t)name_offset);
        ae_wr16(r, AE_OVL186B_G_SAVE_NAME_TIMERS +
                (((buttons & 0x20u) != 0u) ? 2u : 0u), 4u);
    } else if ((buttons & 0x80u) != 0u) {
        play_sfx(r, h, 1);
        int32_t name_offset = ae_rds16(r, cursor_y_address);
        ae_wr8(r, AE_OVL186B_G_SAVE_NAME_BUFFER + (uint32_t)name_offset, 0xEFu);
        --name_offset;
        if (name_offset < 0) name_offset = 0;
        ae_wr16(r, cursor_y_address, (uint32_t)name_offset);
        ae_wr16(r, AE_OVL186B_G_SAVE_NAME_TIMERS + 8u, 4u);
    } else if ((buttons & 0x08u) != 0u) {
        play_sfx(r, h, 11);
        ae_wr16(r, AE_OVL186B_G_SAVE_NAME_TIMERS + 6u, 4u);
        set_name_grid_page(r, ae_rds16(r, AE_OVL186B_G_SAVE_NAME_PAGE), 8);
    } else if ((buttons & 0x04u) != 0u) {
        play_sfx(r, h, 11);
        ae_wr16(r, AE_OVL186B_G_SAVE_NAME_TIMERS + 4u, 4u);
        set_name_grid_page(r, ae_rds16(r, AE_OVL186B_G_SAVE_NAME_PAGE), 4);
    }

    uint32_t repeat = ae_rd32(r, AE_OVL186B_G_PAD_REPEAT);
    int32_t cursor = ae_rds16(r, cursor_x_address);
    int32_t cell_count = ae_rds16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B0);
    int32_t columns = ae_rds16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B2);
    if (repeat == 0x1000u) {
        play_sfx(r, h, 0);
        cursor -= columns;
        if (cursor < 0) cursor += cell_count;
        ae_wr16(r, cursor_x_address, (uint32_t)cursor);
    } else if (repeat == 0x4000u) {
        play_sfx(r, h, 0);
        cursor += columns;
        if (cursor >= cell_count) cursor -= cell_count;
        ae_wr16(r, cursor_x_address, (uint32_t)cursor);
    } else if (repeat == 0x2000u) {
        play_sfx(r, h, 0);
        ++cursor;
        if (cursor >= cell_count) cursor = 0;
        ae_wr16(r, cursor_x_address, (uint32_t)cursor);
    } else if (repeat == 0x8000u) {
        play_sfx(r, h, 0);
        --cursor;
        if (cursor < 0) cursor = cell_count - 1;
        ae_wr16(r, cursor_x_address, (uint32_t)cursor);
    }
    return 0;
}

int32_t ae_CardMgr_Init(AeRam *r, const AeOvl186bHooks *h)
{
    int32_t checksum = OVERLAY1(r, h, "ProgressChecksum",
                                OVL186B_API_PROGRESS_CHECKSUM, 0u);
    uint32_t card_display = AE_OVL186B_G_CARDMGR_DISPLAY;
    uint32_t card_information = AE_OVL186B_CARDMGR_STATS_BASE;
    uint32_t memory_card_header = AE_OVL186B_CARDMGR_HEADER_BASE;

    ae_wr16(r, card_information + OVL186B_CARDMGR_STATS_CHECKSUM,
            (uint32_t)checksum);
    ae_wr16(r, card_information + OVL186B_CARDMGR_STATS_CARD_TYPE,
            ae_rd16(r, card_display + AE_OVL186B_CARDMGR_DISPLAY_BUFFER2));
    ae_wr16(r, card_information + OVL186B_CARDMGR_STATS_BLOCKS,
            ae_rd16(r, card_display + AE_OVL186B_CARDMGR_DISPLAY_FIELD3));
    ae_wr16(r, card_information + OVL186B_CARDMGR_STATS_CARD_COUNT,
            ae_rd16(r, card_display + AE_OVL186B_CARDMGR_DISPLAY_FIELD4));
    ae_wr16(r, card_information + OVL186B_CARDMGR_STATS_STATUS,
            ae_rd8(r, memory_card_header + AE_OVL186B_CARDMGR_HEADER_STATUS));
    ae_wr16(r, card_information + OVL186B_CARDMGR_STATS_FLAGS,
            ae_rd8(r, memory_card_header + AE_OVL186B_CARDMGR_HEADER_FLAGS));
    return 0;
}

static void cardmgr_copy_display_env(AeRam *r, uint32_t destination)
{
    for (uint32_t byte = 0; byte < 8u; ++byte) {
        ae_wr8(r, destination + byte,
               ae_rd8(r, AE_OVL186B_DISPLAY_ENV_TEMPLATE + byte));
    }
}

static uint32_t cardmgr_display_record(AeRam *r)
{
    uint32_t frame_index = ae_rd32(r, AE_OVL186B_G_DISPLAY_INDEX);
    return AE_OVL186B_G_DISPLAY_STATE + frame_index * AE_OVL186B_DISPLAY_RECORD_STRIDE;
}

static void cardmgr_render_frame(AeRam *r, const AeOvl186bHooks *h,
                                 uint32_t frame_stack)
{
    uint32_t display_record = cardmgr_display_record(r);
    (void)NATIVE1(r, h, "DrawSync", OVL186B_API_DRAW_SYNC, 0u);
    (void)NATIVE1(r, h, "VSync", OVL186B_API_VSYNC, 0u);
    (void)NATIVE1(r, h, "PutDispEnv", OVL186B_API_PUT_DISP_ENV,
                  display_record + AE_OVL186B_DISPLAY_ENV_OFFSET);
    ae_wr32(r, AE_OVL186B_G_DISPLAY_STATE,
            ae_rd32(r, AE_OVL186B_G_DISPLAY_STATE) + 1u);
    display_record = cardmgr_display_record(r);
    (void)NATIVE1(r, h, "PutDrawEnv", OVL186B_API_PUT_DRAW_ENV,
                  display_record + AE_OVL186B_DISPLAY_DRAWENV_OFFSET);
    (void)NATIVE1(r, h, "DrawOTag", OVL186B_API_DRAW_OTAG,
                  display_record + AE_OVL186B_DISPLAY_OTAG_OFFSET);
    (void)frame_stack;
}

static void cardmgr_close_file_thread(AeRam *r, const AeOvl186bHooks *h,
                                      uint32_t frame_stack)
{
    (void)OVERLAY0(r, h, "SaveUi_ThreadClose", 0x80141AB4u);
    (void)NATIVE2(r, h, "MemCardOp", OVL186B_API_MEMCARD_OP, 9u, 0u);
    (void)NATIVE4(r, h, "ClearImage", OVL186B_API_CLEAR_IMAGE,
                  frame_stack, 0u, 0u, 0u);
    (void)NATIVE1(r, h, "DrawSync", OVL186B_API_DRAW_SYNC, 0u);
}

int32_t ae_SaveUi_FileSelectUpdate(AeRam *r, const AeOvl186bHooks *h);

int32_t ae_CardMgr_RunFileList(AeRam *r, const AeOvl186bHooks *h)
{
    AeOvl186bHooks nested = nested_hooks(h, 48u);
    uint32_t frame_stack = stack_local(h, 48u, 16u);
    cardmgr_copy_display_env(r, frame_stack);
    ae_wr8(r, AE_OVL186B_G_DISPLAY_STATE + AE_OVL186B_CARDMGR_DISPLAY_ACTIVE, 1u);
    ae_wr8(r, AE_OVL186B_G_DISPLAY_STATE + AE_OVL186B_CARDMGR_OT_ACTIVE, 1u);
    for (uint32_t offset = AE_OVL186B_CARDMGR_OT_CLEAR_START;
         offset <= AE_OVL186B_CARDMGR_OT_CLEAR_END; ++offset) {
        ae_wr8(r, AE_OVL186B_G_DISPLAY_STATE + offset, 0u);
    }
    for (uint32_t offset = AE_OVL186B_CARDMGR_OT_TAGS;
         offset <= AE_OVL186B_CARDMGR_OT_TAGS_END; ++offset) {
        ae_wr8(r, AE_OVL186B_G_DISPLAY_STATE + offset, 0u);
    }
    ae_wr32(r, AE_OVL186B_G_DISPLAY_INDEX, 0u);
    (void)OVERLAY0(r, h, "CardMgr_StartOps", 0x8013D19Cu);
    (void)OVERLAY1(r, h, "SaveUi_ThreadSetHook", 0x80141A10u,
                   OVL186B_THREAD_FILE_LIST);

    ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE, 0u);
    ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_TIMER, 30u);
    for (;;) {
        ae_pad_PadUpdate(r, h != NULL ? h->pad : NULL, AE_PADUPDATE_STICK_BUTTONS);
        (void)NATIVE0(r, h, "BeginFrame", OVL186B_API_BEGIN_FRAME);
        if (ae_SaveUi_FadeTick(r, &nested) < 0) break;
        if (OVERLAY0(r, h, "CardMgr_IsBusy", OVL186B_CARDMGR_BUSY_DETAIL) != 0) {
            ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE, 2u);
        }
        (void)OVERLAY0(r, h, "SaveUi_ThreadSwitch", 0x80141B00u);
        cardmgr_render_frame(r, h, frame_stack);
    }

    cardmgr_close_file_thread(r, h, frame_stack);
    ae_wr32(r, AE_OVL186B_G_CARDMGR_STATE, 0u);
    return 0;
}

int32_t ae_CardMgr_RunFileDetail(AeRam *r, const AeOvl186bHooks *h)
{
    AeOvl186bHooks nested = nested_hooks(h, 48u);
    uint32_t frame_stack = stack_local(h, 48u, 16u);
    cardmgr_copy_display_env(r, frame_stack);
    ae_wr8(r, AE_OVL186B_G_DISPLAY_STATE + AE_OVL186B_CARDMGR_DISPLAY_ACTIVE, 1u);
    ae_wr8(r, AE_OVL186B_G_DISPLAY_STATE + AE_OVL186B_CARDMGR_OT_ACTIVE, 1u);
    for (uint32_t offset = AE_OVL186B_CARDMGR_OT_CLEAR_START;
         offset <= AE_OVL186B_CARDMGR_OT_CLEAR_END; ++offset) {
        ae_wr8(r, AE_OVL186B_G_DISPLAY_STATE + offset, 0u);
    }
    for (uint32_t offset = AE_OVL186B_CARDMGR_OT_TAGS;
         offset <= AE_OVL186B_CARDMGR_OT_TAGS_END; ++offset) {
        ae_wr8(r, AE_OVL186B_G_DISPLAY_STATE + offset, 0u);
    }
    ae_wr32(r, AE_OVL186B_G_DISPLAY_INDEX, 0u);
    (void)OVERLAY0(r, h, "CardMgr_StartOps", 0x8013D19Cu);
    (void)OVERLAY1(r, h, "SaveUi_ThreadSetHook", 0x80141A10u,
                   OVL186B_THREAD_FILE_DETAIL);

    ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_TIMER, 30u);
    ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE, 0u);
    ae_wr32(r, AE_OVL186B_G_CARDMGR_TIMER, 0u);
    for (;;) {
        ae_pad_PadUpdate(r, h != NULL ? h->pad : NULL, AE_PADUPDATE_STICK_BUTTONS);
        (void)NATIVE0(r, h, "BeginFrame", OVL186B_API_BEGIN_FRAME);
        if (ae_SaveUi_FadeTick(r, &nested) < 0) break;
        if (OVERLAY0(r, h, "CardMgr_IsBusy", 0x8013D1F8u) != 0) {
            ae_wr16(r, AE_OVL186B_G_SAVE_UI_FADE_STATE, 2u);
        }
        (void)OVERLAY0(r, h, "SaveUi_ThreadSwitch", 0x80141B00u);
        cardmgr_render_frame(r, h, frame_stack);
    }

    cardmgr_close_file_thread(r, h, frame_stack);
    (void)ae_SaveHeaderFill(r);
    ae_wr32(r, AE_OVL186B_G_CARDMGR_STATE, 0u);
    return 0;
}

int32_t ae_CardMgr_Run(AeRam *r, const AeOvl186bHooks *h)
{
    AeOvl186bHooks nested = nested_hooks(h, 32u);
    (void)NATIVE0(r, h, "InitDisplay", OVL186B_API_INIT_DISPLAY);
    (void)NATIVE1(r, h, "ResetGraph", OVL186B_API_RESET_GRAPH, 1u);
    ae_wr16(r, AE_OVL186B_G_SAVE_UI_STATE, 14u);
    ae_wr32(r, AE_OVL186B_G_CARDMGR_FLAGS, 1u);
    ae_wr32(r, AE_OVL186B_G_CARDMGR_STATE, 0u);
    ae_wr32(r, AE_OVL186B_G_CARDMGR_TIMER, 0u);
    (void)ae_CardMgr_Init(r, &nested);

    for (;;) {
        int32_t state = ae_rds32(r, AE_OVL186B_G_CARDMGR_STATE);
        if (state == 0) {
            if (OVERLAY0(r, h, "CardMgr_RunMessage", 0x8013F630u) < 0) {
                ae_wr16(r, AE_OVL186B_CARDMGR_RESULT_FIELD,
                        AE_OVL186B_MENU_STATE_RENDER);
                break;
            }
        } else if (state == 1) {
            (void)ae_CardMgr_RunFileList(r, &nested);
        } else if (state == 2) {
            (void)ae_CardMgr_RunFileDetail(r, &nested);
        }
    }
    return 0;
}

static void draw_name_char(AeRam *r, const AeOvl186bHooks *h,
                           uint32_t local_string, int32_t x, int32_t y,
                           uint32_t flags, uint8_t character)
{
    ae_wr8(r, local_string, character);
    ae_wr8(r, local_string + 1u, 0u);
    (void)OVERLAY4(r, h, "SaveUi_DrawText", 0x80136000u,
                   local_string, (uint32_t)x, (uint32_t)y, flags);
}

void ae_SaveUi_NameEntryDraw(AeRam *r, const AeOvl186bHooks *h)
{
    int32_t page = ae_rds16(r, AE_OVL186B_G_SAVE_NAME_PAGE);
    int32_t cursor = ae_rds16(r, AE_OVL186B_G_SAVE_NAME_CURSOR_X);
    int32_t cursor_y = ae_rds16(r, AE_OVL186B_G_SAVE_NAME_CURSOR_Y);
    uint32_t local_string = stack_local(h, 56u, 16u);
    AeOvl186bHooks nested = nested_hooks(h, 56u);

    int32_t cursor_column;
    int32_t cursor_row;
    int32_t cursor_x;
    int32_t cursor_screen_y;
    if (page < 2) {
        cursor_row = cursor / 15;
        cursor_column = cursor - cursor_row * 15;
        cursor_x = 50 + cursor_column * 16 + (cursor_column / 5) * 20;
        cursor_screen_y = 62 + cursor_row * 20;
    } else {
        cursor_row = cursor / 11;
        cursor_column = cursor - cursor_row * 11;
        cursor_x = 100 + cursor_column * 16;
        cursor_screen_y = 82 + cursor_row * 20;
    }
    (void)NATIVE3(r, h, "DrawMenuArrows", OVL186B_API_DRAW_MENU_ARROWS,
                  (uint32_t)(cursor_x + 8), (uint32_t)(cursor_screen_y - 2), 32u);
    (void)OVERLAY1(r, h, "CardMgr_DrawMenuTexts", 0x8013FD50u,
                   AE_OVL186B_G_SAVE_NAME_GRID);

    for (uint32_t i = 0; i < 5u; ++i) {
        uint8_t character = (uint8_t)ae_rd8(r, AE_OVL186B_G_SAVE_NAME_BUFFER + i);
        draw_name_char(r, &nested, local_string, 174 + (int32_t)i * 16,
                       32, 1u, character);
    }

    int32_t cell_count = ae_rds16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5B0);
    uint32_t character_table = AE_OVL186B_SAVE_NAME_CHAR_TABLE;
    int32_t table_offset = ae_rds16(r, AE_OVL186B_G_SAVE_NAME_FIELD_5AE);
    for (int32_t cell = 0; cell < cell_count; ++cell) {
        int32_t row;
        int32_t column;
        int32_t x;
        int32_t y;
        if (page < 2) {
            row = cell / 15;
            column = cell - row * 15;
            x = 50 + column * 16 + (column / 5) * 20;
            y = 62 + row * 20;
        } else {
            row = cell / 11;
            column = cell - row * 11;
            x = 100 + column * 16;
            y = 82 + row * 20;
        }
        uint8_t character = (uint8_t)ae_rd8(r, character_table +
                                            (uint32_t)table_offset + (uint32_t)cell);
        uint32_t flags = 1u;
        if (cell == cursor) {
            y -= 2;
            flags = 0u;
        }
        draw_name_char(r, &nested, local_string, x, y, flags, character);
    }

    (void)RENDER4(r, h, "SaveUi_NameEntryPanel", 0x8013BF44u,
                  (uint32_t)page, (uint32_t)cursor,
                  (uint32_t)cursor_y, AE_OVL186B_G_SAVE_NAME_BUFFER);

    static const struct {
        uint32_t icon;
        int32_t x;
        uint32_t timer_offset;
        int32_t y_bias;
    } icons[] = {
        {0u, 268, 0u, 32}, {1u, 288, 2u, 32}, {3u, 194, 8u, 192},
        {4u, 34, 4u, 192}, {5u, 156, 6u, 192}, {6u, 271, 0u, 192}
    };
    for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); ++i) {
        int32_t y = icons[i].y_bias;
        if (i != 5u) y += ae_rds16(r, AE_OVL186B_G_SAVE_NAME_TIMERS + icons[i].timer_offset);
        (void)RENDER3(r, &nested, "SaveUi_DrawIcon", 0x8013946Cu,
                      icons[i].icon, icons[i].x, y);
    }

    (void)RENDER0(r, h, "SaveUi_NameEntryFooter", 0x8013C43Cu);
    ae_wr32(r, AE_OVL186B_G_PACKET_CURSOR,
            ae_rd32(r, AE_OVL186B_G_PACKET_CURSOR) + 8u);
    (void)ae_SaveUi_DrawMainWindow(r, &nested);
    ae_SaveUi_DrawBackground3D(r, &nested);
}

