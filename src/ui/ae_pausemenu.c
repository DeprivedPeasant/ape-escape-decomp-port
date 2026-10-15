#include "ae_pausemenu.h"

#include "../camera/ae_camupd.h"
#include "../core/ae_pad.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../render/ae_tex_copy.h"
#include "../player/native_player.h"

#include <limits.h>
#include <string.h>

/* Absolute globals used by this batch. */
#define AE_PM_G_PAUSE_MODE                 0x800BEF74u
#define AE_PM_G_WIPE_DONE                  0x800BEF78u
#define AE_PM_G_WIPE_ROW                   0x800BEF7Cu
#define AE_PM_G_PAUSE_MENU_VALUE           0x800BEF80u
#define AE_PM_G_PAUSE_MENU_TABLE           0x800B264Cu
#define AE_PM_G_BLUR_KERNEL                0x800B2704u
#define AE_PM_G_GADGET_SOUND_STATE         0x800F5120u
#define AE_PM_G_GADGET_SOUND_IDS           0x800F51D0u
#define AE_PM_G_STAGE_KIND                 0x800F4474u
#define AE_PM_G_PAUSE_ITEM_LEVEL           0x800F4472u
#define AE_PM_G_CURRENT_AREA               0x800F4476u
#define AE_PM_G_PAUSE_INPUT                 0x800E5320u
#define AE_PM_G_CURRENT_GADGET              0x800EC2D2u
#define AE_PM_G_FRAME_COUNTER              0x800EC350u
#define AE_PM_G_PRIMITIVE_ALLOC            0x800EC354u
#define AE_PM_G_ORDERING_TABLE             0x800EC358u
#define AE_PM_G_RENDER_BUFFER_INDEX         0x800EC35Cu
#define AE_PM_G_OT_POINTER                 0x800EC354u
#define AE_PM_G_DRAW_MODEL                 0x800FC358u
#define AE_PM_G_MODEL_MATRIX_A             0x800EC0B0u
#define AE_PM_G_MODEL_MATRIX_B             0x800EC100u
#define AE_PM_G_SCREEN_BUTTON_MODELS       0x800F4CA0u
#define AE_PM_G_SCREEN_BUTTON_MODELS_STRIDE 16u
#define AE_PM_G_RADAR_MODEL_MATRIX_A       0x800EC080u
#define AE_PM_G_RADAR_MODEL_MATRIX_B       0x800EC160u
#define AE_PM_G_RADAR_MODEL_MATRIX_DIRECTION 0x800EC150u
#define AE_PM_G_RADAR_MODEL_MATRIX_FINAL   0x800EC170u
#define AE_PM_G_STATIC_MODEL_MATRIX        0x800E78C0u
#define AE_PM_G_STATIC_MODEL               0x800E7D00u
#define AE_PM_G_SOUND_LOOPS                0x800BF370u
#define AE_PM_G_SOUND_LOOP_COUNT           24u
#define AE_PM_G_SOUND_LOOP_STRIDE          28u
#define AE_PM_G_SCREEN_SNAPSHOT            0x801A8000u
#define AE_PM_G_HUD_FLAGS                  0x800F4DD0u
#define AE_PM_G_PAD_BUTTONS_CHANGED         0x800E5480u
#define AE_PM_G_PAD_BUTTONS_HELD            0x800E5478u
#define AE_PM_G_PLAYER_MODE                 0x800F44A0u
#define AE_PM_G_PLAYER_STATE                0x800EC23Eu
#define AE_PM_G_GADGET_SELECT_MASK          0x800F51C4u
#define AE_PM_G_GADGET_SLOT_BASE            0x800F51A8u
#define AE_PM_G_SCORE_COUNTER               0x800F448Cu
#define AE_PM_G_COUNTER_STATE               0x800F4DB0u
#define AE_PM_G_COUNTER_MODE                0x800F4DB4u
#define AE_PM_G_COUNTER_HEALTH_WINDOW       0x800F4DCCu
#define AE_PM_G_TIME_COUNTER                0x800F4490u
#define AE_PM_G_DISP_ENV_BASE               0x800EC3BCu
#define AE_PM_G_DRAW_ENV_BASE               0x800EC360u
#define AE_PM_G_OT_BUFFER_HEAD_OFFSET       16384u
#define AE_PM_G_OT_LAST_TAG_OFFSET          16380u
#define AE_PM_G_SECOND_OT_OFFSET            16512u

/* Menu state / rectangle fields. */
#define AE_PM_MODE_TRANSITION              0x01u
#define AE_PM_MODE_ACTIVE                  0x01u
#define AE_PM_MODE_MENU                    0x02u
#define AE_PM_MODE_SUBMENU_A               0x02u
#define AE_PM_MODE_SUBMENU_B               0x03u
#define AE_PM_MODE_SIZE                    0x00u
#define AE_PM_MODE_VALUE                   0x02u
#define AE_PM_MENU_RECORD_SIZE             12u
#define AE_PM_MENU_ITEM_X                  0x00u
#define AE_PM_MENU_ITEM_Y                  0x02u
#define AE_PM_MENU_ITEM_WIDTH              0x04u
#define AE_PM_MENU_ITEM_HEIGHT             0x06u
#define AE_PM_RECT_X                       0x00u
#define AE_PM_RECT_Y                       0x02u
#define AE_PM_RECT_WIDTH                   0x04u
#define AE_PM_RECT_HEIGHT                  0x06u
#define AE_PM_SCREEN_WIDTH                 384
#define AE_PM_SCREEN_HEIGHT                240
#define AE_PM_FRAMEBUFFER_HEIGHT           256
#define AE_PM_FRAMEBUFFER_ROW_BYTES        768u

/* Player and gadget sound layout, expressed as field offsets. */
#define AE_PM_GADGET_LOOP_FLAGS            0x08u
#define AE_PM_GADGET_LOOP_A                0x04u
#define AE_PM_GADGET_LOOP_B                0x06u
#define AE_PM_GADGET4_LOOP_A               0x14u
#define AE_PM_GADGET4_LOOP_B               0x16u
#define AE_PM_GADGET6_LOOP_A               0x20u
#define AE_PM_GADGET6_LOOP_B               0x22u
#define AE_PM_GADGET0_LOOP                 0x48u
#define AE_PM_GADGET_SOUND_HANDLE          0x18u
#define AE_PM_GADGET_SOUND_ID              0x1Au
#define AE_PM_GADGET3_FRAME_FIELD          0x16u
#define AE_PM_SOUND_LOOP_ID_A              29u
#define AE_PM_SOUND_LOOP_ID_B              30u
#define AE_PM_SOUND_LOOP_ID_GADGET4        37u
#define AE_PM_SOUND_LOOP_ID_GADGET4_B      38u
#define AE_PM_SOUND_LOOP_ID_GADGET6        44u
#define AE_PM_SOUND_LOOP_ID_GADGET0        61u

/* Function-local stack layouts. Stack arguments and buffers are kept in AeRam so native hooks
   receive the same addresses and bytes as the original code. */
#define AE_PM_DEFAULT_STACK_TOP            0x801FFF00u
#define AE_PM_RUN_FRAME_SIZE                232u
#define AE_PM_RUN_OT_ALLOC0_OFFSET           88u
#define AE_PM_RUN_OT_ALLOC1_OFFSET           92u
#define AE_PM_RUN_SNAPSHOT_RECT_OFFSET       96u
#define AE_PM_RUN_PROJECTION_SAVE_OFFSET    172u
#define AE_PM_RUN_MESSAGE_FRAME_OFFSET       24u
#define AE_PM_RUN_MODEL_MATRIX_OFFSET        24u
#define AE_PM_SNAPSHOT_FRAME_SIZE          32u
#define AE_PM_WIPE_STEP_FRAME_SIZE          32u
#define AE_PM_UPDATE_FRAME_SIZE             24u
#define AE_PM_SNAPSHOT_RECT_OFFSET         16u
#define AE_PM_BLUR_VECTOR_SIZE             16u
#define AE_PM_BLUR_VECTOR_OFFSET           0x801FFEE0u
#define AE_PM_TEXT_FRAME_SIZE               32u
#define AE_PM_TEXT_GLYPH_OFFSET             16u
#define AE_PM_WIDTH_FRAME_SIZE              40u
#define AE_PM_WIDTH_GLYPH_OFFSET            16u
#define AE_PM_SNAPSHOT_FIRST_SIZE           16u
#define AE_PM_SNAPSHOT_SCALE_OFFSET         16u
#define AE_PM_BUTTON_FRAME_SIZE             80u
#define AE_PM_BUTTON_POSITION_OFFSET        16u
#define AE_PM_BUTTON_ROTATION_OFFSET        24u
#define AE_PM_BUTTON_SCALE_OFFSET           32u
#define AE_PM_SCALED_MODEL_FRAME_SIZE       136u
#define AE_PM_SCALED_MODEL_MATRIX_OFFSET    40u
#define AE_PM_SCALED_MODEL_POSITION_OFFSET  32u
#define AE_PM_SCALED_MODEL_RESULT_OFFSET    92u
#define AE_PM_SCALED_MODEL_FLAGS_OFFSET     104u
#define AE_PM_SCALED_MODEL_VECTOR_OFFSET    16u
#define AE_PM_SCALED_MODEL_SCALE_MATRIX     72u
#define AE_PM_SCALED_MODEL_SCALE_VECTOR     16u
#define AE_PM_SCALED_MODEL_SCALE_MAX        4096
#define AE_PM_SCALED_MODEL_SCALE_LIMIT      4097
#define AE_PM_SCALED_MODEL_SCALE_NUMERATOR  0x01000000
#define AE_PM_PLAYER_MATRIX_TRANSLATION     0x800F512Cu
#define AE_PM_BUTTON_SIZE                   328u
#define AE_PM_BUTTON_CENTER_X               192
#define AE_PM_BUTTON_CENTER_Y               120
#define AE_PM_BUTTON_SCRATCH_VISIBLE        0x1F8003C8u
#define AE_PM_RADAR_FRAME_SIZE              136u
#define AE_PM_RADAR_MATRIX_OFFSET           24u
#define AE_PM_RADAR_ROTTRANS_INPUT          16u
#define AE_PM_RADAR_ROTTRANS_OUTPUT         88u
#define AE_PM_RADAR_ROTATION_SCALE          44u
#define AE_PM_RADAR_SCALE_VECTOR            56u
#define AE_PM_RADAR_SCALE_COPY              72u
#define AE_PM_VECTOR_Y                      2u
#define AE_PM_RIGID_MODEL_BASE_FIELD        12u
#define AE_PM_RENDER_BUFFER_ENV_BASE         0x800EC3D0u
#define AE_PM_RENDER_BUFFER_ENV_STRIDE       16504u
#define AE_PM_MODEL_TRANSFORM_BASE_FIELD     12u
#define AE_PM_MODEL_ROTTRANS_VECTOR_OFFSET   0x140u

static int32_t pm_bits_to_s32(uint32_t value)
{
    if ((value & 0x80000000u) != 0u) {
        return -1 - (int32_t)(~value);
    }
    return (int32_t)value;
}

static int32_t pm_sra32(uint32_t value, uint32_t amount)
{
    if (amount == 0u) return pm_bits_to_s32(value);
    uint32_t shifted = value >> amount;
    if ((value & 0x80000000u) != 0u) {
        shifted |= ~(0xFFFFFFFFu >> amount);
    }
    return pm_bits_to_s32(shifted);
}

static uint32_t pm_sp(const AePauseMenuHooks *hooks)
{
    return hooks != NULL && hooks->stack_pointer != 0u
        ? hooks->stack_pointer : AE_PM_DEFAULT_STACK_TOP;
}

static uint32_t pm_frame(const AePauseMenuHooks *hooks, uint32_t bytes)
{
    return pm_sp(hooks) - bytes;
}

static void pm_child_hooks(AePauseMenuHooks *child, const AePauseMenuHooks *parent,
                           uint32_t parent_frame_size)
{
    if (parent != NULL) {
        *child = *parent;
    } else {
        memset(child, 0, sizeof(*child));
    }
    child->stack_pointer = pm_frame(parent, parent_frame_size);
}

static uint32_t pm_hook(AeRam *ram, const AePauseMenuHooks *hooks,
                        AePauseMenuHookFn function, const char *name,
                        uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    if (hooks == NULL || function == NULL) {
        ae_ram_fail(ram, "AePauseMenuHooks.%s not provided", name);
        return 0u;
    }
    return (uint32_t)function(hooks->user, ram, a0, a1, a2, a3);
}

#define PM_CALL(ram, hooks, name, a0, a1, a2, a3) \
    pm_hook((ram), (hooks), (hooks) != NULL ? (hooks)->name : NULL, #name, \
            (uint32_t)(a0), (uint32_t)(a1), (uint32_t)(a2), (uint32_t)(a3))

static void pm_call_sound_loop_stop(AeRam *ram, const AePauseMenuHooks *hooks,
                                   uint32_t sound_id, int32_t handle)
{
    (void)PM_CALL(ram, hooks, SoundLoopStop, sound_id, (uint32_t)handle, 0u, 0u);
}

static void pm_stop_gadget4_loops(AeRam *ram, const AePauseMenuHooks *hooks,
                                 uint32_t first_offset, uint32_t first_id,
                                 uint32_t second_offset, uint32_t second_id)
{
    int32_t first = ae_rds16(ram, AE_PM_G_GADGET_SOUND_STATE + first_offset);
    if (first != -1) {
        pm_call_sound_loop_stop(ram, hooks, first_id, first);
        ae_wr16(ram, AE_PM_G_GADGET_SOUND_STATE + first_offset, 0xFFFFu);
    }

    int32_t second = ae_rds16(ram, AE_PM_G_GADGET_SOUND_STATE + second_offset);
    if (second != -1) {
        pm_call_sound_loop_stop(ram, hooks, second_id, second);
        ae_wr16(ram, AE_PM_G_GADGET_SOUND_STATE + second_offset, 0xFFFFu);
    }
}

static void pm_stop_flagged_gadget_loops(AeRam *ram, const AePauseMenuHooks *hooks)
{
    uint32_t state = AE_PM_G_GADGET_SOUND_STATE;
    uint32_t flags = ae_rd32(ram, state + AE_PM_GADGET_LOOP_FLAGS);
    if ((flags & 1u) != 0u) {
        pm_call_sound_loop_stop(ram, hooks, AE_PM_SOUND_LOOP_ID_A,
                                ae_rds16(ram, state + AE_PM_GADGET_LOOP_A));
    }
    if ((flags & 2u) != 0u) {
        pm_call_sound_loop_stop(ram, hooks, AE_PM_SOUND_LOOP_ID_B,
                                ae_rds16(ram, state + AE_PM_GADGET_LOOP_B));
    }
    ae_wr32(ram, state + AE_PM_GADGET_LOOP_FLAGS, 0u);
}

void ae_PlayerStopGadgetSounds(AeRam *ram, const AePauseMenuHooks *hooks, int32_t gadget)
{
    if (gadget == 2) {
        pm_stop_gadget4_loops(ram, hooks,
                              AE_PM_GADGET4_LOOP_A, AE_PM_SOUND_LOOP_ID_GADGET4,
                              AE_PM_GADGET4_LOOP_B, AE_PM_SOUND_LOOP_ID_GADGET4_B);
    } else if (gadget == 4) {
        pm_stop_gadget4_loops(ram, hooks,
                              AE_PM_GADGET6_LOOP_A, AE_PM_SOUND_LOOP_ID_GADGET6,
                              AE_PM_GADGET6_LOOP_B, AE_PM_SOUND_LOOP_ID_GADGET6);
    } else if (gadget == 6) {
        pm_stop_flagged_gadget_loops(ram, hooks);
    } else if (gadget == 0) {
        int32_t handle = ae_rds16(ram, AE_PM_G_GADGET_SOUND_STATE + AE_PM_GADGET0_LOOP);
        if (handle != -1) {
            pm_call_sound_loop_stop(ram, hooks, AE_PM_SOUND_LOOP_ID_GADGET0, handle);
            ae_wr16(ram, AE_PM_G_GADGET_SOUND_STATE + AE_PM_GADGET0_LOOP, 0xFFFFu);
        }
    }

    uint32_t sound_state = AE_PM_G_GADGET_SOUND_IDS;
    int32_t handle = ae_rds16(ram, sound_state + AE_PM_GADGET_SOUND_HANDLE);
    if (handle != -1) {
        int32_t sound_id = ae_rds16(ram, sound_state + AE_PM_GADGET_SOUND_ID);
        pm_call_sound_loop_stop(ram, hooks, (uint32_t)sound_id, handle);
        ae_wr16(ram, sound_state + AE_PM_GADGET_SOUND_HANDLE, 0xFFFFu);
        ae_wr16(ram, sound_state + AE_PM_GADGET_SOUND_ID, 0xFFFFu);
    }
}

int32_t ae_SoundLoopsSetPan(AeRam *ram, const AePauseMenuHooks *hooks, int32_t resume)
{
    uint32_t entry = AE_PM_G_SOUND_LOOPS;
    for (uint32_t index = 0; index < AE_PM_G_SOUND_LOOP_COUNT; ++index) {
        int32_t voice = ae_rds16(ram, entry);
        int32_t handle = ae_rds16(ram, entry + 2u);
        if (voice >= 0 && handle >= 0) {
            int32_t stored_pan = (int16_t)(ae_rd16(ram, entry) & 0xF000u);
            if (resume == 0) {
                if (stored_pan < 0x6000 && stored_pan == 0) {
                    (void)PM_CALL(ram, hooks, SfxVoiceSetPanVol,
                                  (uint32_t)handle, 0u, 0u, 0u);
                }
            } else if (stored_pan == 0 &&
                       ae_rds16(ram, entry + AE_PM_GADGET3_FRAME_FIELD) != 1) {
                (void)PM_CALL(ram, hooks, SfxVoiceSetPanVol,
                              (uint32_t)handle, 0x1000u, 0x1000u, 0u);
            }
        }
        entry += AE_PM_G_SOUND_LOOP_STRIDE;
    }
    return 0;
}

void ae_SoundLoopsResume(AeRam *ram, const AePauseMenuHooks *hooks)
{
    if (ae_rd16(ram, AE_PM_G_STAGE_KIND) == 0x1Bu) {
        (void)PM_CALL(ram, hooks, PauseMenuOverlay1B, 1u, 0u, 0u, 0u);
    }
    ae_SoundLoopsSetPan(ram, hooks, 1);
}

void ae_ScreenWipeReset(AeRam *ram)
{
    ae_wr32(ram, AE_PM_G_WIPE_DONE, 0u);
    ae_wr32(ram, AE_PM_G_WIPE_ROW, 239u);
}

static int32_t pm_load_image(AeRam *ram, const AePauseMenuHooks *hooks,
                             uint32_t rectangle, uint32_t image)
{
    return (int32_t)PM_CALL(ram, hooks, LoadImage, rectangle, image, 0u, 0u);
}

void ae_ScreenSnapshotRestore(AeRam *ram, const AePauseMenuHooks *hooks)
{
    uint32_t frame = pm_frame(hooks, AE_PM_SNAPSHOT_FRAME_SIZE);
    uint32_t rectangle = frame + AE_PM_SNAPSHOT_RECT_OFFSET;
    int32_t row = ae_rds32(ram, AE_PM_G_WIPE_ROW);
    ae_wr16(ram, rectangle + AE_PM_RECT_X, 0u);
    ae_wr16(ram, rectangle + AE_PM_RECT_WIDTH, AE_PM_SCREEN_WIDTH);

    if (row > 0) {
        uint32_t draw_buffer = ae_rd32(ram, AE_PM_G_RENDER_BUFFER_INDEX);
        ae_wr16(ram, rectangle + AE_PM_RECT_Y,
                (draw_buffer != 0u) ? AE_PM_FRAMEBUFFER_HEIGHT : 0u);
        ae_wr16(ram, rectangle + AE_PM_RECT_HEIGHT, (uint32_t)row);
        (void)pm_load_image(ram, hooks, rectangle, AE_PM_G_SCREEN_SNAPSHOT);
    }

    if (row < AE_PM_SCREEN_HEIGHT) {
        uint32_t draw_buffer = ae_rd32(ram, AE_PM_G_RENDER_BUFFER_INDEX);
        uint32_t source_y = (uint32_t)row +
                            (draw_buffer != 0u ? AE_PM_FRAMEBUFFER_HEIGHT : 0u);
        ae_wr16(ram, rectangle + AE_PM_RECT_Y, source_y);
        ae_wr16(ram, rectangle + AE_PM_RECT_HEIGHT,
                AE_PM_SCREEN_HEIGHT - (uint32_t)row);
        uint32_t source_offset = ((uint32_t)row + 2u) *
                                 AE_PM_FRAMEBUFFER_ROW_BYTES;
        (void)pm_load_image(ram, hooks, rectangle,
                            AE_PM_G_SCREEN_SNAPSHOT + source_offset);
    }
}

static int32_t pm_call_irgb_to_ir123(AeRam *ram, const AePauseMenuHooks *hooks,
                                     uint32_t color, uint32_t out_vector)
{
    return (int32_t)PM_CALL(ram, hooks, RendererIrgbToIr123, color, out_vector, 0u, 0u);
}

static int32_t pm_call_pack_irgb(AeRam *ram, const AePauseMenuHooks *hooks,
                                 int32_t red, int32_t green, int32_t blue,
                                 uint32_t destination)
{
    return (int32_t)PM_CALL(ram, hooks, RendererPackIrgb,
                            (uint32_t)red, (uint32_t)green, (uint32_t)blue, destination);
}

void ae_ScreenWipeBlurBlock(AeRam *ram, const AePauseMenuHooks *hooks, uint32_t block)
{
    uint32_t block_start = AE_PM_G_SCREEN_SNAPSHOT + 2u + block * 0x300u;
    uint32_t destination = block_start + 0x600u;
    uint32_t temp = pm_sp(hooks) - AE_PM_BLUR_VECTOR_SIZE;

    for (uint32_t pixel = 0; pixel < 382u; ++pixel) {
        uint32_t red = 0u;
        uint32_t green = 0u;
        uint32_t blue = 0u;
        uint32_t kernel = AE_PM_G_BLUR_KERNEL;

        for (uint32_t sample = 0; sample < 9u; ++sample) {
            int32_t source_delta = ae_rds16(ram, kernel);
            uint32_t shift = ae_rd16(ram, kernel + 2u) & 31u;
            uint32_t source_pixel = block_start + (pixel + (uint32_t)source_delta) * 2u;
            uint32_t color = ae_rd16(ram, source_pixel);
            (void)pm_call_irgb_to_ir123(ram, hooks, color, temp);
            red += (uint32_t)ae_rd32(ram, temp) << shift;
            green += (uint32_t)ae_rd32(ram, temp + 4u) << shift;
            blue += (uint32_t)ae_rd32(ram, temp + 8u) << shift;
            kernel += 4u;
        }

        int32_t out_red = pm_sra32(red, 4u);
        int32_t out_green = pm_sra32(green, 4u);
        int32_t out_blue = pm_sra32(blue, 4u);
        (void)pm_call_pack_irgb(ram, hooks, out_red, out_green, out_blue,
                                destination + pixel * 2u);
    }
}

void ae_ScreenWipeStep(AeRam *ram, const AePauseMenuHooks *hooks)
{
    if (ae_rd32(ram, AE_PM_G_WIPE_DONE) != 0u) return;

    int32_t row = ae_rds32(ram, AE_PM_G_WIPE_ROW);
    int32_t stop_row = row < 4 ? 0 : row - 4;
    AePauseMenuHooks child;
    pm_child_hooks(&child, hooks, AE_PM_WIPE_STEP_FRAME_SIZE);

    while (row >= stop_row) {
        ae_ScreenWipeBlurBlock(ram, &child, (uint32_t)row);
        --row;
    }
    ae_wr32(ram, AE_PM_G_WIPE_ROW, (uint32_t)stop_row);
    if (stop_row == 0) ae_wr32(ram, AE_PM_G_WIPE_DONE, 1u);
}

void ae_SetDAT_800BEF80(AeRam *ram, uint32_t value)
{
    ae_wr32(ram, AE_PM_G_PAUSE_MENU_VALUE, value);
}

int32_t ae_PauseItemEnabled(AeRam *ram, int32_t item)
{
    uint32_t current_level = ae_rd16(ram, AE_PM_G_PAUSE_ITEM_LEVEL);
    switch (item) {
    case 0: {
        uint32_t area = ae_rd16(ram, AE_PM_G_CURRENT_AREA);
        if (area == 19u || area == 36u) return 0;
        return area < 92u || area >= 99u;
    }
    case 1:
        return current_level != 0x0Du;
    case 2:
        return current_level != 0x0Fu;
    case 3:
        return 1;
    default:
        /* These are the values left in $v0 by the original signed range branches. */
        return item < 0 ? 1 : 3;
    }
}

static uint32_t pm_stack_glyph(const AePauseMenuHooks *hooks, uint32_t frame_size,
                               uint32_t glyph_offset)
{
    return pm_frame(hooks, frame_size) + glyph_offset;
}

static uint32_t pm_next_glyph(AeRam *ram, const AePauseMenuHooks *hooks,
                              uint32_t text, uint32_t glyph_out)
{
    uint32_t next = PM_CALL(ram, hooks, TextNextGlyph, text, glyph_out, 0u, 0u);
    return ae_ram_failed(ram) ? 0u : next;
}

void ae_MessageBoxDrawString(AeRam *ram, const AePauseMenuHooks *hooks, uint32_t text)
{
    uint32_t glyph_address = pm_stack_glyph(hooks, AE_PM_TEXT_FRAME_SIZE,
                                             AE_PM_TEXT_GLYPH_OFFSET);
    for (;;) {
        text = pm_next_glyph(ram, hooks, text, glyph_address);
        if (ae_ram_failed(ram)) return;
        uint32_t glyph = ae_rd16(ram, glyph_address);
        if (glyph == 0u) return;
        (void)PM_CALL(ram, hooks, FontDrawGlyph, glyph, 496u, 0u, 0u);
    }
}

int32_t ae_TextWidthUntilGlyph(AeRam *ram, const AePauseMenuHooks *hooks,
                               uint32_t text, uint32_t glyph)
{
    uint32_t glyph_address = pm_stack_glyph(hooks, AE_PM_WIDTH_FRAME_SIZE,
                                             AE_PM_WIDTH_GLYPH_OFFSET);
    uint32_t wanted = glyph & 0xFFFFu;
    uint32_t widths = 0x800B3640u;
    uint32_t width = 0u;

    for (;;) {
        text = pm_next_glyph(ram, hooks, text, glyph_address);
        if (ae_ram_failed(ram)) return 0;
        uint32_t current = ae_rd16(ram, glyph_address);
        if (current == wanted) return (int32_t)width;
        width += ae_rd8(ram, widths + current);
    }
}

static void pm_layout_value(AeRam *ram, uint32_t offset, int32_t value)
{
    ae_wr16(ram, AE_PM_G_PAUSE_MENU_TABLE + offset, (uint32_t)value);
}

#define AE_PM_LAYOUT_BUTTON0_X       0x00u
#define AE_PM_LAYOUT_BUTTON0_Y       0x02u
#define AE_PM_LAYOUT_BUTTON0_WIDTH   0x04u
#define AE_PM_LAYOUT_BUTTON0_HEIGHT  0x06u
#define AE_PM_LAYOUT_BUTTON1_X       0x0Cu
#define AE_PM_LAYOUT_BUTTON1_Y       0x0Eu
#define AE_PM_LAYOUT_BUTTON1_WIDTH   0x10u
#define AE_PM_LAYOUT_BUTTON1_HEIGHT  0x12u
#define AE_PM_LAYOUT_BUTTON2_X       0x18u
#define AE_PM_LAYOUT_BUTTON2_Y       0x1Au
#define AE_PM_LAYOUT_BUTTON2_WIDTH   0x1Cu
#define AE_PM_LAYOUT_BUTTON2_HEIGHT  0x1Eu
#define AE_PM_LAYOUT_BUTTON3_X       0x24u
#define AE_PM_LAYOUT_BUTTON3_Y       0x26u
#define AE_PM_LAYOUT_BUTTON3_WIDTH   0x28u
#define AE_PM_LAYOUT_BUTTON3_HEIGHT  0x2Au

void ae_PauseMenuSetLayout(AeRam *ram, uint32_t layout)
{
    switch (layout) {
    case 0:
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_HEIGHT, -32);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_WIDTH, -128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_WIDTH, 512);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_WIDTH, 128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_HEIGHT, 110);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_HEIGHT, 110);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_WIDTH, 128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_HEIGHT, 288);
        break;
    case 1:
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_X, 128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_Y, 60);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_X, 40);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_X, 220);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_X, 128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_Y, 110);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_Y, 110);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_Y, 170);
        break;
    case 2:
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_X, 120);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_Y, 30);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_X, -128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_X, 512);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_X, 128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_Y, 110);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_Y, 110);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_Y, 288);
        break;
    case 3:
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_X, 128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_Y, -32);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_X, -128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_Y, 110);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_X, 120);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_Y, 30);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_X, 128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_Y, 288);
        break;
    case 4:
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_X, 128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_Y, -32);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_X, 120);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_Y, 30);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_X, 512);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_Y, 110);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_X, 128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_Y, 288);
        break;
    case 5:
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_X, 128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON0_Y, -32);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_X, -128);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON1_Y, 110);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_X, 512);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON2_Y, 110);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_X, 120);
        pm_layout_value(ram, AE_PM_LAYOUT_BUTTON3_Y, 30);
        break;
    default:
        break;
    }
}

void ae_HudDrawWindowFrame(AeRam *ram, const AePauseMenuHooks *hooks,
                           int32_t x, int32_t y, int32_t width, int32_t height)
{
    (void)PM_CALL(ram, hooks, RendererHudDrawWindowFrame,
                  (uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height);
}

void ae_PauseMenuDrawButtons(AeRam *ram, const AePauseMenuHooks *hooks)
{
    uint32_t time_attack = PM_CALL(ram, hooks, IsTimeAttack, 0u, 0u, 0u, 0u);
    if (time_attack != 0u && ae_rds8(ram, AE_PM_G_PAUSE_MODE) != AE_PM_MODE_SUBMENU_A) return;

    uint32_t enabled_mask = 0u;
    for (uint32_t item = 0u; item < 4u; ++item) {
        uint32_t record = AE_PM_G_PAUSE_MENU_TABLE + item * AE_PM_MENU_RECORD_SIZE;
        if (ae_PauseItemEnabled(ram, (int32_t)item) != 0) enabled_mask |= 1u << item;

        int32_t x = ae_rds16(ram, record + AE_PM_MENU_ITEM_X);
        int32_t y = ae_rds16(ram, record + AE_PM_MENU_ITEM_Y);
        int32_t width = ae_rds16(ram, record + AE_PM_MENU_ITEM_WIDTH);
        int32_t height = ae_rds16(ram, record + AE_PM_MENU_ITEM_HEIGHT);
        int32_t scaled_width = pm_bits_to_s32((uint32_t)((int64_t)(x + width * 2) / 3));
        int32_t scaled_height = pm_bits_to_s32((uint32_t)((int64_t)(y + height * 2) / 3));
        ae_wr16(ram, record + AE_PM_MENU_ITEM_WIDTH, (uint32_t)scaled_width);
        ae_wr16(ram, record + AE_PM_MENU_ITEM_HEIGHT, (uint32_t)scaled_height);
    }
    uint32_t current_ot = ae_rd32(ram, AE_PM_G_PRIMITIVE_ALLOC);
    (void)PM_CALL(ram, hooks, RendererPauseMenuDrawButtons,
                  AE_PM_G_PAUSE_MENU_TABLE, enabled_mask,
                  ae_rd8(ram, AE_PM_G_PAUSE_MODE), current_ot);
    ae_wr32(ram, AE_PM_G_PRIMITIVE_ALLOC, current_ot + 88u);
}

int32_t ae_PauseMenuUpdate(AeRam *ram, const AePauseMenuHooks *hooks)
{
    int32_t mode = ae_rds8(ram, AE_PM_G_PAUSE_MODE);
    if (mode == 0) return 0;
    if (mode != AE_PM_MODE_ACTIVE) return 0;

    (void)PM_CALL(ram, hooks, WaitFrameFlip, 0u, 0u, 0u, 0u);
    if (ae_ram_failed(ram)) return 0;
    int32_t gadget = ae_rds8(ram, AE_PM_G_CURRENT_GADGET);
    ae_PlayerStopGadgetSounds(ram, hooks, gadget);
    if (ae_ram_failed(ram)) return 0;
    AePauseMenuHooks child;
    pm_child_hooks(&child, hooks, AE_PM_UPDATE_FRAME_SIZE);
    return ae_PauseMenuRun(ram, &child);
}

static int32_t pm_div_s32(AeRam *ram, int32_t numerator, int32_t denominator,
                          const char *name)
{
    if (denominator == 0 || (numerator == INT32_MIN && denominator == -1)) {
        ae_ram_fail(ram, "%s: signed divide trap", name);
        return 0;
    }
    int64_t quotient = (int64_t)numerator / (int64_t)denominator;
    return (int32_t)quotient;
}

static uint32_t pm_mul32(uint32_t left, uint32_t right)
{
    return (uint32_t)((uint64_t)left * (uint64_t)right);
}

static uint32_t pm_lwl(AeRam *ram, uint32_t address, uint32_t old_value)
{
    uint32_t shift = (address & 3u) * 8u;
    uint32_t word = ae_rd32(ram, address & ~3u);
    uint32_t keep = shift == 24u ? 0u : (1u << (24u - shift)) - 1u;
    return (old_value & keep) | (word << (24u - shift));
}

static void pm_swl(AeRam *ram, uint32_t address, uint32_t value)
{
    uint32_t offset = address & 3u;
    uint32_t aligned = address & ~3u;
    for (uint32_t byte = 0u; byte <= offset; ++byte) {
        uint32_t shift = (3u - offset + byte) * 8u;
        ae_wr8(ram, aligned + byte, value >> shift);
    }
}

static int32_t pm_div13_magic(int32_t value)
{
    int64_t product = (int64_t)value * (int64_t)0x4EC4EC4F;
    int32_t high = (int32_t)(product >> 32);
    return pm_sra32((uint32_t)high, 4u) - pm_sra32((uint32_t)value, 31u);
}

static int32_t pm_div74_magic(int32_t value)
{
    int64_t product = (int64_t)value * (int32_t)0xDD67C8A7u;
    int32_t high = (int32_t)(product >> 32);
    uint32_t sum = (uint32_t)high + (uint32_t)value;
    return pm_sra32(sum, 6u) - pm_sra32((uint32_t)value, 31u);
}

static uint32_t pm_run_env(uint32_t base, uint32_t draw_buffer)
{
    return base + draw_buffer * AE_PM_RENDER_BUFFER_ENV_STRIDE;
}

static void pm_draw_button_ring(AeRam *ram, const AePauseMenuHooks *hooks,
                                int32_t selected, uint32_t ot_pointer,
                                int32_t expanding_ring)
{
    int32_t progress = ae_rds16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE);
    uint32_t current_ot = ot_pointer;
    for (uint32_t item = 0u; item < 4u; ++item) {
        int32_t angle = (int32_t)(item << 10);
        int32_t cosine = ae_sdk_rcos(ram, angle);
        int32_t sine = ae_sdk_rsin(ram, angle);
        int32_t x;
        int32_t y;
        if (expanding_ring != 0) {
            x = pm_div_s32(ram, cosine, 52 - progress * 2, "PauseMenuRun ring x");
            y = pm_div_s32(ram, sine, 74 - progress * 2, "PauseMenuRun ring y");
        } else {
            x = pm_div13_magic(cosine);
            y = pm_div74_magic(sine);
        }
        if (ae_ram_failed(ram)) return;
        current_ot = ae_PauseMenuDrawButton(ram, hooks, x, y, (int32_t)item,
                                             selected, current_ot);
        if (ae_ram_failed(ram)) return;
    }
}

static void pm_draw_pause_hud(AeRam *ram, const AePauseMenuHooks *hooks,
                              int32_t mode)
{
    uint32_t time_attack = PM_CALL(ram, hooks, IsTimeAttack, 0u, 0u, 0u, 0u);
    if (ae_ram_failed(ram)) return;

    if (time_attack != 0u) {
        uint32_t timer = ae_rd32(ram, AE_PM_G_TIME_COUNTER);
        (void)PM_CALL(ram, hooks, HudDrawIcon16, 144u, 32u,
                      timer, 0u);
        (void)PM_CALL(ram, hooks, HudDrawTime, 160u, 32u,
                      timer, 0u);
        uint32_t gauge = PM_CALL(ram, hooks, GetDAT_800B01C2, 0u, 0u, 0u, 0u);
        if (gauge != 0u) {
            (void)PM_CALL(ram, hooks, HudDrawTimerGauge, gauge, 0u, 0u, 0u);
        }
    }

    if (mode == 4 || (mode >= 10 && mode <= 12)) {
        (void)PM_CALL(ram, hooks, SetPriority,
                      ae_rd32(ram, AE_PM_G_PRIMITIVE_ALLOC), 0u, 0u, 0u);
        return;
    }

    (void)PM_CALL(ram, hooks, HudPanelA_DrawIcon, 310u, 192u, 0u, 0u);
    ae_wr16(ram, AE_PM_G_COUNTER_HEALTH_WINDOW, 20u);
    (void)PM_CALL(ram, hooks, HudDrawHealth, 310u, 0u, 0u, 0u);
    (void)PM_CALL(ram, hooks, HudPanelB_DrawSprite, 20u, 192u, 0u, 0u);
    (void)PM_CALL(ram, hooks, HudDrawDigit, 52u, 200u, 0xFFFFFFFFu, 0u);
    int32_t score = (int32_t)ae_rd16(ram, AE_PM_G_SCORE_COUNTER) - 1;
    (void)PM_CALL(ram, hooks, HudDrawNumber, 76u, 200u,
                  (uint32_t)score, 0u);
    if (PM_CALL(ram, hooks, IsTimeAttack, 0u, 0u, 0u, 0u) == 0u) {
        ae_wr16(ram, AE_PM_G_COUNTER_STATE, 81u);
        (void)PM_CALL(ram, hooks, HudCounterC_Draw, 76u, 200u,
                      (uint32_t)score, 0u);
    }
    (void)PM_CALL(ram, hooks, HudDrawGadgetIcons, 0u, 0u, 0u, 0u);
    ae_wr16(ram, AE_PM_G_COUNTER_MODE, (uint32_t)-28);
    (void)PM_CALL(ram, hooks, SetPriority,
                  ae_rd32(ram, AE_PM_G_PRIMITIVE_ALLOC), 0u, 0u, 0u);
}

static void pm_draw_gadget_screen(AeRam *ram, const AePauseMenuHooks *hooks,
                                  int32_t cursor)
{
    (void)PM_CALL(ram, hooks, CutsceneModeOn, 0u, 0u, 0u, 0u);
    (void)PM_CALL(ram, hooks, RendererPauseMenuDrawGadgetScreen,
                  (uint32_t)cursor,
                  ae_rd8(ram, AE_PM_G_CURRENT_GADGET),
                  ae_rd32(ram, AE_PM_G_GADGET_SELECT_MASK),
                  ae_rd32(ram, AE_PM_G_RENDER_BUFFER_INDEX));
}

static void pm_draw_centered_text(AeRam *ram, const AePauseMenuHooks *hooks,
                                  uint32_t text, int32_t y)
{
    int32_t width = (int32_t)PM_CALL(ram, hooks, HudTextWidth,
                                    text, 0u, 0u, 0u);
    int32_t x = pm_sra32((uint32_t)(384 - width), 1u);
    (void)PM_CALL(ram, hooks, DrawTextSprites,
                  text, (uint32_t)x, (uint32_t)y, 2u);
}

static int32_t pm_wrap_gadget_cursor(int32_t cursor)
{
    if (cursor >= 8) {
        int32_t rounded = cursor < 0 ? cursor + 7 : cursor;
        int32_t quotient = pm_sra32((uint32_t)rounded, 3u);
        cursor -= quotient * 8;
    }
    return cursor;
}

static void pm_update_gadget_slot(AeRam *ram, const AePauseMenuHooks *hooks,
                                  uint32_t buttons, int32_t selected_gadget)
{
    if ((buttons & 0xF0u) == 0u) return;
    int can_switch = ae_player_can_switch_gadget(
        ae_rd16(ram, AE_PM_G_PLAYER_MODE),
        ae_rd16(ram, AE_PM_G_PLAYER_STATE));
    if (!can_switch) {
        (void)PM_CALL(ram, hooks, PlaySfx, 48u, 0u, 0u, 0u);
        return;
    }

    uint32_t slot_offset;
    if ((buttons & 0x10u) != 0u) slot_offset = 0x88u;
    else if ((buttons & 0x80u) != 0u) slot_offset = 0x89u;
    else if ((buttons & 0x20u) != 0u) slot_offset = 0x8Au;
    else slot_offset = 0x8Bu;

    uint32_t enabled = ae_rd32(ram, AE_PM_G_GADGET_SELECT_MASK);
    uint32_t bit = 1u << ((uint32_t)selected_gadget & 31u);
    if ((enabled & bit) == 0u) {
        (void)PM_CALL(ram, hooks, PlaySfx, 48u, 0u, 0u, 0u);
        return;
    }

    uint32_t slot = AE_PM_G_GADGET_SOUND_STATE + slot_offset;
    int32_t old_gadget = ae_rds8(ram, slot);
    int32_t current_gadget = ae_rds8(ram, AE_PM_G_CURRENT_GADGET);
    if (current_gadget == old_gadget) {
        (void)ae_PlayerSelectGadget(ram,
                                    hooks != NULL ? hooks->player2 : NULL,
                                    hooks != NULL ? hooks->player_act : NULL,
                                    hooks != NULL ? hooks->spawn : NULL,
                                    hooks != NULL ? hooks->bodies : NULL,
                                    hooks != NULL ? hooks->sound_hw : NULL,
                                    selected_gadget, current_gadget);
    }
    (void)PM_CALL(ram, hooks, GadgetSlotRemap,
                  (uint32_t)selected_gadget, (uint32_t)old_gadget, 0u, 0u);
    ae_wr8(ram, slot, (uint32_t)selected_gadget);
    (void)PM_CALL(ram, hooks, PlaySfx, 36u, 0u, 0u, 0u);
}

int32_t ae_PauseMenuRun(AeRam *ram, const AePauseMenuHooks *hooks)
{
    AePauseMenuHooks frame_hooks;
    pm_child_hooks(&frame_hooks, hooks, AE_PM_RUN_FRAME_SIZE);
    uint32_t frame = pm_frame(hooks, AE_PM_RUN_FRAME_SIZE);
    uint32_t draw_buffer = ae_rd32(ram, AE_PM_G_RENDER_BUFFER_INDEX) ^ 1u;
    uint32_t display_environment = pm_run_env(AE_PM_G_DISP_ENV_BASE, draw_buffer);
    uint32_t draw_environment = pm_run_env(AE_PM_G_DRAW_ENV_BASE, draw_buffer);

    (void)PM_CALL(ram, &frame_hooks, DrawSync, 0u, 0u, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, VSync, 0u, 0u, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, PutDispEnv, display_environment, 0u, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, PutDrawEnv, draw_environment, 0u, 0u, 0u);

    uint32_t projection_distance = PM_CALL(ram, &frame_hooks,
                                            RendererGetProjectionDistance,
                                            0u, 0u, 0u, 0u);
    ae_wr32(ram, frame + AE_PM_RUN_PROJECTION_SAVE_OFFSET, projection_distance);
    (void)PM_CALL(ram, &frame_hooks, RendererSetProjectionDistance, 328u, 0u, 0u, 0u);

    uint32_t snapshot_rect = frame + AE_PM_RUN_SNAPSHOT_RECT_OFFSET;
    uint32_t initial_buffer = ae_rd32(ram, AE_PM_G_RENDER_BUFFER_INDEX);
    ae_wr16(ram, snapshot_rect + AE_PM_RECT_X, 0u);
    ae_wr16(ram, snapshot_rect + AE_PM_RECT_Y,
            initial_buffer != 0u ? AE_PM_FRAMEBUFFER_HEIGHT : 0u);
    ae_wr16(ram, snapshot_rect + AE_PM_RECT_WIDTH, AE_PM_SCREEN_WIDTH);
    ae_wr16(ram, snapshot_rect + AE_PM_RECT_HEIGHT, AE_PM_SCREEN_HEIGHT);
    (void)PM_CALL(ram, &frame_hooks, StoreImage,
                  snapshot_rect, AE_PM_G_SCREEN_SNAPSHOT, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, DrawSync, 0u, 0u, 0u, 0u);
    ae_ScreenWipeReset(ram);

    uint32_t first_allocation = PM_CALL(ram, &frame_hooks, HeapAlloc,
                                        0x9000u, 0u, 0u, 0u);
    uint32_t second_allocation = PM_CALL(ram, &frame_hooks, HeapAlloc,
                                         0x9000u, 0u, 0u, 0u);
    ae_wr32(ram, frame + AE_PM_RUN_OT_ALLOC0_OFFSET, first_allocation);
    ae_wr32(ram, frame + AE_PM_RUN_OT_ALLOC1_OFFSET, second_allocation);

    uint32_t hud_flags = ae_rd32(ram, AE_PM_G_HUD_FLAGS);
    ae_wr32(ram, AE_PM_G_HUD_FLAGS, hud_flags | 0x10u);
    (void)PM_CALL(ram, &frame_hooks, HudPanelA_Start, 1u, 0u, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, HudPanelB_Start, 1u, 0u, 0u, 0u);
    if (PM_CALL(ram, &frame_hooks, IsTimeAttack, 0u, 0u, 0u, 0u) == 0u) {
        (void)PM_CALL(ram, &frame_hooks, HudCounterC_Start, 1u, 0u, 0u, 0u);
    }
    ae_PauseMenuSetLayout(ram, 0u);
    ae_PauseMenuSetLayout(ram, 1u);
    ae_SetDAT_800BEF80(ram, 1u);
    (void)PM_CALL(ram, &frame_hooks, DrawSync, 0u, 0u, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, VSync, 2u, 0u, 0u, 0u);

    int32_t menu_cursor = 0;
    int32_t gadget_cursor = 0;
    int32_t return_value = hooks != NULL ? (int32_t)hooks->caller_s5 : 0;
    for (;;) {
        if (hooks == NULL || hooks->pad == NULL) {
            ae_ram_fail(ram, "AePauseMenuHooks.pad not provided");
            break;
        }
        ae_pad_PadUpdate(ram, hooks->pad, AE_PADUPDATE_STICK_BUTTONS);
        if (ae_ram_failed(ram)) break;

        uint32_t buffer_index = ae_rd32(ram, AE_PM_G_RENDER_BUFFER_INDEX) ^ 1u;
        ae_wr32(ram, AE_PM_G_RENDER_BUFFER_INDEX, buffer_index);
        ae_wr32(ram, AE_PM_G_FRAME_COUNTER,
                ae_rd32(ram, AE_PM_G_FRAME_COUNTER) + 1u);
        uint32_t ordering_table = pm_run_env(AE_PM_RENDER_BUFFER_ENV_BASE, buffer_index);
        uint32_t allocation = ae_rd32(ram, frame + AE_PM_RUN_OT_ALLOC0_OFFSET + buffer_index * 4u);
        uint32_t clear_pointer = ordering_table;
        (void)PM_CALL(ram, &frame_hooks, ClearOTagR,
                      clear_pointer, 0x1000u, 0u, 0u);
        ae_wr32(ram, ordering_table + AE_PM_G_OT_BUFFER_HEAD_OFFSET, 0x00FFFFFFu);
        ae_wr32(ram, AE_PM_G_PRIMITIVE_ALLOC, allocation);
        ae_wr32(ram, AE_PM_G_ORDERING_TABLE, ordering_table);
        ae_wr32(ram, frame + 168u, ordering_table);
        (void)PM_CALL(ram, &frame_hooks, ColorFadeReset, 0u, 0u, 0u, 0u);

        if (ae_rd8(ram, AE_PM_G_PAUSE_MODE + 1u) != 0u) {
            ae_wr8(ram, AE_PM_G_PAUSE_MODE + 1u, 0u);
            if (ae_PauseItemEnabled(ram, 0) != 0) {
                return_value = 0;
                gadget_cursor = 0;
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 10u);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 0u);
                ae_PauseMenuSetLayout(ram, 2u);
                (void)PM_CALL(ram, &frame_hooks, CutsceneModeOn, 0u, 0u, 0u, 0u);
            } else {
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 15u);
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, AE_PM_MODE_ACTIVE);
            }
        }

        uint32_t buttons = ae_rd32(ram, AE_PM_G_PAD_BUTTONS_CHANGED) &
                           ae_rd32(ram, AE_PM_G_PAD_BUTTONS_HELD);
        int32_t mode = ae_rds8(ram, AE_PM_G_PAUSE_MODE);
        if (mode == 1) {
            uint16_t progress = ae_rd16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE);
            pm_draw_button_ring(ram, &frame_hooks, (int16_t)progress,
                                ae_rd32(ram, AE_PM_G_OT_POINTER), 1);
            if (ae_ram_failed(ram)) return return_value;
            progress = (uint16_t)(progress - 1u);
            ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, progress);
            if (progress == 0u) ae_wr8(ram, AE_PM_G_PAUSE_MODE, 2u);
            if ((buttons & 0x0800u) != 0u) {
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 3u);
                return_value = 0;
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 16u, 0u, 0u, 0u);
            }
        } else if (mode == 2) {
            (void)PM_CALL(ram, &frame_hooks, PadDirectionCode, 0u, 0u, 0u, 0u);
            int16_t progress = ae_rds16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE);
            if (progress < 256) {
                progress = (int16_t)(progress + 1);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE,
                        (uint16_t)progress);
            }
            (void)PM_CALL(ram, &frame_hooks, TextSetCursor, 256u, 170u, 0u, 0u);
            ae_MessageBoxDrawString(ram, &frame_hooks, 0x800B269Cu);
            ae_PauseMenuDrawButtons(ram, &frame_hooks);
            pm_draw_button_ring(ram, &frame_hooks, progress,
                                ae_rd32(ram, AE_PM_G_OT_POINTER), 0);
            if (ae_ram_failed(ram)) return return_value;

            if ((buttons & 0x0C00u) != 0u) {
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 3u);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 0u);
                return_value = 0;
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 1u, 0u, 0u, 0u);
            } else if ((buttons & 0x0020u) != 0u && ae_PauseItemEnabled(ram, 2) != 0) {
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 11u);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 0u);
                ae_PauseMenuSetLayout(ram, 3u);
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 3u, 0u, 0u, 0u);
            } else if ((buttons & 0x0080u) != 0u && ae_PauseItemEnabled(ram, 1) != 0) {
                menu_cursor = 1;
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 4u);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 0u);
                ae_PauseMenuSetLayout(ram, 4u);
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 3u, 0u, 0u, 0u);
            } else if ((buttons & 0x0010u) != 0u && ae_PauseItemEnabled(ram, 0) != 0) {
                return_value = 0;
                gadget_cursor = 0;
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 10u);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 0u);
                ae_PauseMenuSetLayout(ram, 2u);
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 3u, 0u, 0u, 0u);
            } else if ((buttons & 0x0040u) != 0u && ae_PauseItemEnabled(ram, 3) != 0) {
                return_value = 0;
                gadget_cursor = 0;
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 12u);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 0u);
                ae_PauseMenuSetLayout(ram, 5u);
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 3u, 0u, 0u, 0u);
                (void)PM_CALL(ram, &frame_hooks, PauseMenuInitVolumes,
                              0u, 0u, 0u, 0u);
            }
        } else if (mode == 3) {
            pm_draw_button_ring(ram, &frame_hooks, 32,
                                ae_rd32(ram, AE_PM_G_OT_POINTER), 1);
            if (ae_ram_failed(ram)) return return_value;
            uint16_t progress = (uint16_t)(ae_rd16(ram, AE_PM_G_PAUSE_MODE +
                                                    AE_PM_MODE_VALUE) + 1u);
            ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, progress);
            if ((int16_t)progress >= 16) {
                if (return_value == 1) ae_wr8(ram, AE_PM_G_PAUSE_MODE, 11u);
                else if (return_value == 4) ae_wr8(ram, AE_PM_G_PAUSE_MODE, 10u);
                else ae_wr8(ram, AE_PM_G_PAUSE_MODE, 0u);
                menu_cursor = 0;
            }
        } else if (mode == 4) {
            static const uint32_t labels[] = {
                0x800B267Cu, 0x800B2690u, 0x800B2694u, 0x800B2698u
            };
            static const int32_t label_y[] = {100, 120, 140, 160};
            for (uint32_t i = 0u; i < 4u; ++i) {
                pm_draw_centered_text(ram, &frame_hooks, labels[i], label_y[i]);
                if (ae_ram_failed(ram)) break;
            }
            int32_t time_attack = PM_CALL(ram, &frame_hooks, IsTimeAttack,
                                          0u, 0u, 0u, 0u) != 0u;
            if (time_attack != 0) {
                pm_draw_centered_text(ram, &frame_hooks, 0x800B26BCu, 180);
            }
            if (ae_ram_failed(ram)) break;
            if ((buttons & 0x5000u) != 0u) {
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 0u, 0u, 0u, 0u);
                menu_cursor += (buttons & 0x1000u) != 0u ? -1 : 1;
            }
            time_attack = PM_CALL(ram, &frame_hooks, IsTimeAttack,
                                  0u, 0u, 0u, 0u) != 0u;
            if (time_attack != 0) {
                int32_t shifted = menu_cursor + 3;
                menu_cursor = shifted - pm_div_s32(ram, shifted, 3,
                                                    "PauseMenuRun submenu cursor") * 3;
            } else {
                menu_cursor = (menu_cursor + 2) & 1;
            }
            uint32_t selected_text = menu_cursor == 0 ? labels[2] :
                                     menu_cursor == 1 ? labels[3] : 0x800B26BCu;
            int32_t width = (int32_t)PM_CALL(ram, &frame_hooks, HudTextWidth,
                                              selected_text, 0u, 0u, 0u);
            (void)PM_CALL(ram, &frame_hooks, DrawMenuArrows,
                          192u, (uint32_t)(140 + menu_cursor * 20),
                          (uint32_t)(width + 48), 0u);
            time_attack = PM_CALL(ram, &frame_hooks, IsTimeAttack,
                                  0u, 0u, 0u, 0u) != 0u;
            (void)PM_CALL(ram, &frame_hooks, RendererHudDrawWindowFrame,
                          92u, 80u, 192u, time_attack != 0 ? 128u : 112u);
            ae_PauseMenuDrawButtons(ram, &frame_hooks);
            if ((buttons & 0x08F0u) != 0u) {
                if (menu_cursor == 0) {
                    ae_wr8(ram, AE_PM_G_PAUSE_MODE, 3u);
                    return_value = 3;
                    (void)PM_CALL(ram, &frame_hooks, PlaySfx, 3u, 0u, 0u, 0u);
                } else if (menu_cursor == 1) {
                    ae_wr8(ram, AE_PM_G_PAUSE_MODE, 2u);
                    ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 256u);
                    (void)PM_CALL(ram, &frame_hooks, PlaySfx, 1u, 0u, 0u, 0u);
                } else {
                    ae_wr8(ram, AE_PM_G_PAUSE_MODE, 3u);
                    return_value = 2;
                    (void)PM_CALL(ram, &frame_hooks, PlaySfx, 3u, 0u, 0u, 0u);
                }
                ae_PauseMenuSetLayout(ram, 1u);
            }
        } else if (mode == 10) {
            pm_draw_gadget_screen(ram, &frame_hooks, gadget_cursor);
            (void)PM_CALL(ram, &frame_hooks, TextSetCursor, 276u, 138u, 0u, 0u);
            ae_MessageBoxDrawString(ram, &frame_hooks, 0x800B269Cu);

            if ((buttons & 0xF000u) != 0u) {
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 0u, 0u, 0u, 0u);
                if ((buttons & 0x8000u) != 0u) {
                    gadget_cursor += (gadget_cursor & 3) == 0 ? 3 : -1;
                } else if ((buttons & 0x2000u) != 0u) {
                    gadget_cursor += (gadget_cursor & 3) == 3 ? -3 : 1;
                } else if ((buttons & 0x1000u) != 0u) {
                    gadget_cursor += (gadget_cursor / 4) != 0 ? -4 : 4;
                } else if ((buttons & 0x4000u) != 0u) {
                    gadget_cursor += (gadget_cursor / 4) != 0 ? -4 : 4;
                }
            } else {
                pm_update_gadget_slot(ram, &frame_hooks, buttons, return_value);
            }
            gadget_cursor = pm_wrap_gadget_cursor(gadget_cursor);

            (void)PM_CALL(ram, &frame_hooks, HudDrawGadgetSlots, 0u, 0u, 0u, 0u);
            ae_PauseMenuDrawButtons(ram, &frame_hooks);
            /* PauseMenuRun stores the normalized gadget cursor in $s5 here. */
            return_value = gadget_cursor;
            (void)PM_CALL(ram, &frame_hooks, RendererHudDrawWindowFrame,
                          8u, 56u, 256u, 104u);
            (void)PM_CALL(ram, &frame_hooks, RendererHudDrawWindowFrame,
                          8u, 162u, 368u, 64u);
            (void)PM_CALL(ram, &frame_hooks, SetPriority,
                          ae_rd32(ram, AE_PM_G_PRIMITIVE_ALLOC), 0u, 0u, 0u);

            uint32_t ordering_table = ae_rd32(ram, AE_PM_G_ORDERING_TABLE);
            uint32_t primitive = ae_rd32(ram, AE_PM_G_PRIMITIVE_ALLOC);
            uint32_t gadget_tag = ordering_table + AE_PM_G_OT_LAST_TAG_OFFSET - 4u;
            uint32_t old_tag = pm_lwl(ram, gadget_tag + 2u, primitive);
            pm_swl(ram, gadget_tag + 2u, primitive << 8);
            pm_swl(ram, primitive + 2u, old_tag);
            ae_wr32(ram, AE_PM_G_PRIMITIVE_ALLOC, primitive + 12u);

            if ((buttons & 0x0800u) != 0u) {
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 2u);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 256u);
                ae_PauseMenuSetLayout(ram, 1u);
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 1u, 0u, 0u, 0u);
                (void)PM_CALL(ram, &frame_hooks, CutsceneModeOffCaller,
                              0u, 0u, 0u, 0u);
            } else if ((buttons & 0x0100u) != 0u) {
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 3u);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 0u);
                return_value = 0;
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 1u, 0u, 0u, 0u);
                (void)PM_CALL(ram, &frame_hooks, CutsceneModeOffCaller,
                              0u, 0u, 0u, 0u);
            }
        } else if (mode == 11) {
            if (ae_rd16(ram, AE_PM_G_PAUSE_ITEM_LEVEL) == 13u) {
                (void)PM_CALL(ram, &frame_hooks, PauseDrawCounter,
                              0u, 0u, 0u, 0u);
            } else {
                (void)PM_CALL(ram, &frame_hooks, PauseDrawCaptureGrid,
                              0u, 0u, 0u, 0u);
            }
            ae_PauseMenuDrawButtons(ram, &frame_hooks);
            if ((buttons & 0x08F0u) != 0u) {
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 2u);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 256u);
                ae_PauseMenuSetLayout(ram, 1u);
                (void)PM_CALL(ram, &frame_hooks, PlaySfx, 1u, 0u, 0u, 0u);
            }
        } else if (mode == 12) {
            int32_t result = (int32_t)PM_CALL(ram, &frame_hooks,
                                              PauseMenuDrawSubScreen,
                                              0u, 0u, 0u, 0u);
            if (result == -1) {
                ae_wr8(ram, AE_PM_G_PAUSE_MODE, 2u);
                ae_wr16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE, 256u);
                ae_PauseMenuSetLayout(ram, 1u);
            }
        }
        if (ae_ram_failed(ram)) break;

        int32_t frame_mode = ae_rds8(ram, AE_PM_G_PAUSE_MODE);
        pm_draw_pause_hud(ram, &frame_hooks, frame_mode);
        uint32_t end_primitive = ae_rd32(ram, AE_PM_G_PRIMITIVE_ALLOC);
        uint32_t target_primitive = ae_rd32(ram, frame + AE_PM_RUN_OT_ALLOC0_OFFSET +
                                           buffer_index * 4u) + 0x9000u;
        if (target_primitive < end_primitive) {
            (void)PM_CALL(ram, &frame_hooks, Printf, 0x800A60E4u, 0u, 0u, 0u);
        }
        (void)PM_CALL(ram, &frame_hooks, CdMgrUpdate, 0u, 0u, 0u, 0u);
        ae_ScreenWipeStep(ram, &frame_hooks);
        (void)PM_CALL(ram, &frame_hooks, DrawSync, 0u, 0u, 0u, 0u);
        (void)PM_CALL(ram, &frame_hooks, VSync, 2u, 0u, 0u, 0u);
        display_environment = pm_run_env(AE_PM_G_DISP_ENV_BASE, buffer_index);
        draw_environment = pm_run_env(AE_PM_G_DRAW_ENV_BASE, buffer_index);
        (void)PM_CALL(ram, &frame_hooks, PutDispEnv, display_environment, 0u, 0u, 0u);
        (void)PM_CALL(ram, &frame_hooks, PutDrawEnv, draw_environment, 0u, 0u, 0u);
        ae_ScreenSnapshotRestore(ram, &frame_hooks);

        uint32_t ot_last_tag = ordering_table + AE_PM_G_OT_LAST_TAG_OFFSET;
        uint32_t primitive = ae_rd32(ram, AE_PM_G_PRIMITIVE_ALLOC);
        uint32_t old_tag = pm_lwl(ram, ot_last_tag + 2u, primitive);
        pm_swl(ram, ot_last_tag + 2u, primitive << 8);
        pm_swl(ram, primitive + 2u, old_tag);
        (void)PM_CALL(ram, &frame_hooks, DrawOTag,
                      ot_last_tag, 0u, 0u, 0u);
        (void)PM_CALL(ram, &frame_hooks, DrawOTag,
                      ordering_table + 0x4000u, 0u, 0u, 0u);

        if (ae_rd8(ram, AE_PM_G_PAUSE_MODE) == 0u) break;
    }

    ae_wr8(ram, AE_PM_G_PAUSE_MODE, 0u);
    uint32_t saved_projection = ae_rd32(ram, frame + AE_PM_RUN_PROJECTION_SAVE_OFFSET);
    (void)PM_CALL(ram, &frame_hooks, RendererSetProjectionDistance,
                  saved_projection, 0u, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, DrawSync, 0u, 0u, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, VSync, 0u, 0u, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, HeapFree,
                  ae_rd32(ram, frame + AE_PM_RUN_OT_ALLOC0_OFFSET), 0u, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, HeapFree,
                  ae_rd32(ram, frame + AE_PM_RUN_OT_ALLOC1_OFFSET), 0u, 0u, 0u);
    (void)PM_CALL(ram, &frame_hooks, CutsceneModeOn, 0u, 0u, 0u, 0u);
    ae_SoundLoopsResume(ram, &frame_hooks);
    return return_value;
}

void ae_DrawScaledModelAt(AeRam *ram, const AePauseMenuHooks *hooks,
                          uint32_t matrix, int32_t scale, int32_t update_player_matrix)
{
    uint32_t frame = pm_frame(hooks, AE_PM_SCALED_MODEL_FRAME_SIZE);
    uint32_t local_matrix = frame + AE_PM_SCALED_MODEL_MATRIX_OFFSET;
    uint32_t scale_vector = frame + AE_PM_SCALED_MODEL_SCALE_VECTOR;
    (void)ae_sdk_MatrixIdentity(ram, local_matrix);
    (void)ae_sdk_CompMatrix(ram, matrix, local_matrix, local_matrix);

    int32_t quotient = pm_div_s32(ram, AE_PM_SCALED_MODEL_SCALE_NUMERATOR, scale,
                                  "DrawScaledModelAt");
    if (ae_ram_failed(ram)) return;
    if (quotient >= AE_PM_SCALED_MODEL_SCALE_LIMIT) {
        quotient = AE_PM_SCALED_MODEL_SCALE_MAX;
    }

    ae_wr32(ram, scale_vector, AE_PM_SCALED_MODEL_SCALE_MAX);
    ae_wr32(ram, scale_vector + 8u, (uint32_t)scale);
    uint32_t scaled_matrix = frame + AE_PM_SCALED_MODEL_SCALE_MATRIX;
    for (uint32_t word = 0u; word < 8u; ++word) {
        ae_wr32(ram, scaled_matrix + word * 4u,
                ae_rd32(ram, local_matrix + word * 4u));
    }
    ae_wr32(ram, scale_vector, (uint32_t)quotient);
    (void)ae_sdk_ScaleMatrix(ram, local_matrix, scale_vector);
    if (ae_ram_failed(ram)) return;

    (void)PM_CALL(ram, hooks, SetRotMatrix, local_matrix, 0u, 0u, 0u);
    (void)PM_CALL(ram, hooks, SetTransMatrix, local_matrix, 0u, 0u, 0u);

    uint32_t position = frame + AE_PM_SCALED_MODEL_POSITION_OFFSET;
    uint32_t result = frame + AE_PM_SCALED_MODEL_RESULT_OFFSET;
    ae_wr16(ram, position, 0u);
    ae_wr16(ram, position + 2u, 0u);
    ae_wr16(ram, position + 4u, (uint32_t)-430);
    (void)PM_CALL(ram, hooks, RotTrans, position, result,
                  frame + AE_PM_SCALED_MODEL_FLAGS_OFFSET, 0u);
    if (ae_ram_failed(ram)) return;
    if (update_player_matrix != 0) {
        ae_wr16(ram, AE_PM_PLAYER_MATRIX_TRANSLATION,
                ae_rd16(ram, result));
        ae_wr16(ram, AE_PM_PLAYER_MATRIX_TRANSLATION + 2u,
                ae_rd16(ram, result + 4u));
        ae_wr16(ram, AE_PM_PLAYER_MATRIX_TRANSLATION + 4u,
                ae_rd16(ram, result + 8u));
    }

    uint32_t first_transform = AE_PM_G_MODEL_MATRIX_A;
    (void)ae_sdk_CompMatrix(ram, AE_PM_G_STATIC_MODEL_MATRIX,
                            local_matrix, local_matrix);
    (void)PM_CALL(ram, hooks, ModelTransformRigid, first_transform,
                  local_matrix, AE_PM_G_STATIC_MODEL, 0u);
    uint32_t first_draw_result = PM_CALL(ram, hooks, ModelDraw,
                                         ae_rd32(ram, AE_PM_G_DRAW_MODEL),
                                         AE_PM_G_STATIC_MODEL, first_transform, 0u);

    uint32_t second_transform = AE_PM_G_MODEL_MATRIX_B;
    int32_t secondary_scale;
    uint32_t triple_scale = pm_mul32((uint32_t)scale, 3u);
    int32_t signed_triple_scale = pm_bits_to_s32(triple_scale);
    if (signed_triple_scale < 0x1000) {
        secondary_scale = 0x1000;
    } else if (signed_triple_scale >= 0x2001) {
        secondary_scale = 0x2000;
    } else {
        secondary_scale = signed_triple_scale;
    }
    ae_wr32(ram, scale_vector, (uint32_t)secondary_scale);
    ae_wr32(ram, scale_vector + 4u, (uint32_t)secondary_scale);
    ae_wr32(ram, scale_vector + 8u, (uint32_t)secondary_scale);
    (void)ae_sdk_CompMatrix(ram, AE_PM_G_STATIC_MODEL_MATRIX, scaled_matrix,
                            scaled_matrix);
    (void)ae_sdk_ScaleMatrix(ram, scaled_matrix, scale_vector);
    (void)PM_CALL(ram, hooks, ModelTransformRigid, second_transform,
                  scaled_matrix,
                  AE_PM_G_STATIC_MODEL, 0u);
    uint32_t second_draw_result = PM_CALL(ram, hooks, ModelDraw,
                                          ae_rd32(ram, AE_PM_G_DRAW_MODEL),
                                          AE_PM_G_STATIC_MODEL, second_transform, 0u);
    (void)first_draw_result;
    (void)second_draw_result;
}

static uint32_t pm_button_model_index(uint32_t item)
{
    static const uint8_t model_index[4] = {2u, 3u, 1u, 0u};
    return item < 4u ? model_index[item] : 0u;
}

uint32_t ae_PauseMenuDrawButton(AeRam *ram, const AePauseMenuHooks *hooks,
                                int32_t x_offset, int32_t y_offset, int32_t item,
                                int32_t selected, uint32_t ot_pointer)
{
    uint32_t frame = pm_frame(hooks, AE_PM_BUTTON_FRAME_SIZE);
    uint32_t position = frame + AE_PM_BUTTON_POSITION_OFFSET;
    uint32_t rotation = frame + AE_PM_BUTTON_ROTATION_OFFSET;
    uint32_t scale_vector = frame + AE_PM_BUTTON_SCALE_OFFSET;

    if (selected >= 0) {
        uint32_t angle = ae_rd32(ram, AE_PM_G_FRAME_COUNTER) << 6;
        int32_t cosine = ae_sdk_rcos(ram, (int32_t)angle);
        int32_t sine = ae_sdk_rsin(ram, (int32_t)angle);
        ae_wr16(ram, rotation, (uint32_t)pm_sra32((uint32_t)cosine, 4u));
        ae_wr16(ram, rotation + 2u, (uint32_t)pm_sra32((uint32_t)sine, 4u));
    } else {
        ae_wr16(ram, rotation, 0u);
        ae_wr16(ram, rotation + 2u, 0u);
    }
    ae_wr16(ram, rotation + 4u, 0u);

    int32_t mode = ae_rds8(ram, AE_PM_G_PAUSE_MODE);
    int32_t progress = ae_rds16(ram, AE_PM_G_PAUSE_MODE + AE_PM_MODE_VALUE);
    if (mode == AE_PM_MODE_SUBMENU_A) {
        int32_t transition_scale = pm_bits_to_s32(0x1000u - ((uint32_t)progress << 6));
        if (transition_scale < 0x800) transition_scale = 0x800;
        x_offset = pm_sra32(pm_mul32((uint32_t)x_offset,
                                     (uint32_t)transition_scale), 12u);
        y_offset = pm_sra32(pm_mul32((uint32_t)y_offset,
                                     (uint32_t)transition_scale), 12u);
    }

    int32_t scale;
    if (selected == -1) {
        scale = pm_bits_to_s32(pm_mul32(pm_mul32((uint32_t)progress, 3u), 512u));
    } else {
        scale = pm_bits_to_s32(0x1000u - ((uint32_t)progress << 8));
    }
    ae_wr32(ram, scale_vector, (uint32_t)scale);
    ae_wr32(ram, scale_vector + 4u, (uint32_t)scale);
    ae_wr32(ram, scale_vector + 8u, (uint32_t)scale);

    ae_wr16(ram, position + AE_PM_RECT_X,
            (uint32_t)(AE_PM_BUTTON_CENTER_X + x_offset));
    ae_wr16(ram, position + AE_PM_RECT_Y,
            (uint32_t)(AE_PM_BUTTON_CENTER_Y + y_offset));
    ae_wr16(ram, position + AE_PM_RECT_WIDTH, AE_PM_BUTTON_SIZE);
    ae_wr16(ram, position + AE_PM_RECT_HEIGHT, AE_PM_BUTTON_SIZE);
    ae_wr32(ram, AE_PM_G_OT_POINTER, ot_pointer);

    uint32_t model_index = pm_button_model_index((uint32_t)item);
    uint32_t enabled = (uint32_t)ae_PauseItemEnabled(ram, model_index);
    if (enabled == 0u) {
        ae_wr32(ram, AE_PM_BUTTON_SCRATCH_VISIBLE, 1u);
        ae_wr16(ram, rotation, 0u);
        ae_wr16(ram, rotation + 2u, 0u);
        ae_wr16(ram, rotation + 4u, 0u);
    } else {
        ae_wr32(ram, AE_PM_BUTTON_SCRATCH_VISIBLE, 0u);
    }
    uint32_t model = AE_PM_G_SCREEN_BUTTON_MODELS +
                     model_index * AE_PM_G_SCREEN_BUTTON_MODELS_STRIDE;
    (void)PM_CALL(ram, hooks, ModelDrawScreenSpace,
                  model, position, rotation, scale_vector);
    return ae_rd32(ram, AE_PM_G_OT_POINTER);
}

int32_t ae_RadarDrawModel(AeRam *ram, const AePauseMenuHooks *hooks,
                          uint32_t model, uint32_t matrix, uint32_t direction, uint32_t angle_x)
{
    uint32_t frame = pm_frame(hooks, AE_PM_RADAR_FRAME_SIZE);
    uint32_t local_matrix = frame + AE_PM_RADAR_MATRIX_OFFSET;
    uint32_t draw_buffer = ae_rd32(ram, AE_PM_G_FRAME_COUNTER + 12u);
    uint32_t draw_environment = AE_PM_RENDER_BUFFER_ENV_BASE +
                               pm_mul32(draw_buffer, AE_PM_RENDER_BUFFER_ENV_STRIDE);
    uint32_t first_transform = AE_PM_G_RADAR_MODEL_MATRIX_A;

    (void)PM_CALL(ram, hooks, ModelTransformRigid, first_transform,
                  model, AE_PM_G_STATIC_MODEL, 0u);
    (void)PM_CALL(ram, hooks, ModelDraw,
                  draw_environment, AE_PM_G_STATIC_MODEL,
                  first_transform, 0u);
    (void)PM_CALL(ram, hooks, SetRotMatrix, matrix, 0u, 0u, 0u);
    (void)PM_CALL(ram, hooks, SetTransMatrix, matrix, 0u, 0u, 0u);
    (void)PM_CALL(ram, hooks, RotTransSV,
                  ae_rd32(ram, AE_PM_G_RADAR_MODEL_MATRIX_A +
                          AE_PM_MODEL_TRANSFORM_BASE_FIELD) +
                      AE_PM_MODEL_ROTTRANS_VECTOR_OFFSET,
                  frame + AE_PM_RADAR_ROTTRANS_INPUT,
                  frame + AE_PM_RADAR_ROTTRANS_OUTPUT, 0u);

    (void)ae_sdk_MatrixIdentity(ram, local_matrix);
    ae_wr32(ram, frame + AE_PM_RADAR_ROTATION_SCALE, 4u);
    ae_wr32(ram, frame + AE_PM_RADAR_ROTATION_SCALE + 4u, 0x80u);
    ae_wr32(ram, frame + AE_PM_RADAR_ROTATION_SCALE + 8u, 4u);
    (void)PM_CALL(ram, hooks, RotMatrixX, angle_x, local_matrix, 0u, 0u);
    (void)PM_CALL(ram, hooks, RotMatrixY, 0x400u, local_matrix, 0u, 0u);
    (void)ae_sdk_CompMatrix(ram, model, local_matrix, local_matrix);

    uint32_t second_transform = AE_PM_G_RADAR_MODEL_MATRIX_B;
    (void)PM_CALL(ram, hooks, ModelTransformRigid, second_transform,
                  local_matrix, AE_PM_G_STATIC_MODEL, 0u);
    (void)PM_CALL(ram, hooks, ModelDraw,
                  draw_environment, AE_PM_G_STATIC_MODEL,
                  second_transform, 0u);

    (void)ae_sdk_MatrixIdentity(ram, local_matrix);
    (void)PM_CALL(ram, hooks, RotMatrixX, angle_x, local_matrix, 0u, 0u);
    (void)PM_CALL(ram, hooks, RotMatrixY, (uint32_t)-0x400, local_matrix, 0u, 0u);
    (void)ae_sdk_CompMatrix(ram, model, local_matrix, local_matrix);
    (void)PM_CALL(ram, hooks, ModelTransformRigid, second_transform,
                  local_matrix, AE_PM_G_STATIC_MODEL, 0u);
    (void)PM_CALL(ram, hooks, ModelDraw,
                  draw_environment, AE_PM_G_STATIC_MODEL,
                  second_transform, 0u);

    (void)ae_sdk_MatrixIdentity(ram, local_matrix);
    (void)ae_sdk_MatrixFromDirXZ(ram, direction, local_matrix);
    int32_t height = ae_rds16(ram, direction + AE_PM_VECTOR_Y);
    if (height > 0) {
        uint32_t scale_copy = frame + AE_PM_RADAR_SCALE_COPY;
        (void)PM_CALL(ram, hooks, Memset, scale_copy, 0u, 16u, 0u);
        ae_wr32(ram, scale_copy, (uint32_t)height);
        ae_wr32(ram, scale_copy + 4u, (uint32_t)height);
        ae_wr32(ram, scale_copy + 8u, (uint32_t)height);
        for (uint32_t word = 0u; word < 4u; ++word) {
            ae_wr32(ram, frame + AE_PM_RADAR_SCALE_VECTOR + word * 4u,
                    ae_rd32(ram, scale_copy + word * 4u));
        }
        (void)ae_sdk_ScaleMatrix(ram, local_matrix, frame + AE_PM_RADAR_SCALE_VECTOR);
    }

    (void)ae_sdk_CompMatrix(ram, AE_PM_G_STATIC_MODEL_MATRIX,
                            local_matrix, local_matrix);
    uint32_t direction_transform = AE_PM_G_RADAR_MODEL_MATRIX_DIRECTION;
    (void)PM_CALL(ram, hooks, ModelTransformRigid, direction_transform,
                  local_matrix, AE_PM_G_STATIC_MODEL, 0u);
    (void)PM_CALL(ram, hooks, ModelDraw, draw_environment,
                  AE_PM_G_STATIC_MODEL, direction_transform, 0u);

    return (int32_t)PM_CALL(ram, hooks, ModelTransformRigid,
                            AE_PM_G_RADAR_MODEL_MATRIX_FINAL, local_matrix,
                            AE_PM_G_STATIC_MODEL, 0u);
}
