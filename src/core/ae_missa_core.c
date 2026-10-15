#include "ae_missa.h"

#include "../world/ae_stagefade.h"

#define AE_MISSA_G_STAGE_STATE          0x800F4350u
#define AE_MISSA_STAGE_MEMBER_MODE      0x012Cu
#define AE_MISSA_ALT_AREA_LIST          0x800A851Cu
#define AE_MISSA_G_STICK_HISTORY        0x800E5320u
#define AE_MISSA_G_STICK_HISTORY_POS    0x800E5490u
#define AE_MISSA_G_PLAY_TIME            0x800F4490u
#define AE_MISSA_G_DEMO_BUFFER          0x800A8A50u
#define AE_MISSA_G_DEMO_FRAME           0x800A8A54u
#define AE_MISSA_G_DEMO_ACTIVE          0x800A8A58u
#define AE_MISSA_G_PAD_PACKET            0x800E52D0u
#define AE_MISSA_G_PAD_HELD              0x800E5478u
#define AE_MISSA_G_PAD_CHANGED           0x800E5480u
#define AE_MISSA_SCR_STICK_GESTURE      0x1F80005Eu
#define AE_MISSA_G_FADE_COUNTER         0x800E690Au

#define AE_MISSA_STICK_HISTORY_STRIDE   4u
#define AE_MISSA_STICK_HISTORY_COUNT    40u
#define AE_MISSA_DEMO_RECORD_STRIDE     8u
#define AE_MISSA_DEMO_RECORD_STICK     0x00u
#define AE_MISSA_DEMO_RECORD_BUTTONS   0x04u
#define AE_MISSA_PAD_STATUS             0x00u
#define AE_MISSA_PAD_BUTTONS            0x02u
#define AE_MISSA_DEMO_ABORT_BUTTONS     0x09F0u
#define AE_MISSA_STAGE_PLAY_TIME        0x0140u

#define AE_MISSA_MODEL_RESOURCE         0x00u
#define AE_MISSA_MODEL_VERTEX_COUNT     0x04u
#define AE_MISSA_MODEL_NORMAL_COUNT     0x06u
#define AE_MISSA_MODEL_POLYGON_COUNT    0x08u
#define AE_MISSA_MODEL_ARRAYS           0x0Cu
#define AE_MISSA_MODEL_STATE_RESOURCE   0x00u
#define AE_MISSA_MODEL_STATE_VERTICES   0x04u
#define AE_MISSA_MODEL_STATE_NORMALS    0x08u
#define AE_MISSA_MODEL_STATE_POLYGONS   0x0Cu
#define AE_MISSA_MODEL_STATE_VERTEX_COLORS 0x10u
#define AE_MISSA_MODEL_STATE_COLOR_TABLE 0x14u
#define AE_MISSA_MODEL_VERTEX_BYTES     4u
#define AE_MISSA_MODEL_NORMAL_BYTES     16u
#define AE_MISSA_G_MODEL_NORMAL_TABLE  0x800E9C60u

static int32_t missa_s32(uint32_t value)
{
    return (int32_t)value;
}

static int missa_alt_list_contains(AeRam *r, int32_t value)
{
    uint32_t cursor = AE_MISSA_ALT_AREA_LIST;
    int32_t needle = (int16_t)(uint16_t)value;

    for (;;) {
        uint32_t entry = ae_rd8(r, cursor);
        if (entry == 0xFFu) {
            return 0;
        }
        if ((int32_t)(uint8_t)entry == needle) {
            return 1;
        }
        ++cursor;
    }
}

int32_t ae_AreaUsesAltMember(AeRam *r, int32_t area, int32_t list_index)
{
    if (ae_rd8(r, AE_MISSA_G_STAGE_STATE + AE_MISSA_STAGE_MEMBER_MODE) == 11u) {
        return 0;
    }

    if (!missa_alt_list_contains(r, area)) {
        return 0;
    }
    return missa_alt_list_contains(r, list_index);
}

int32_t ae_PadDemoPlaybackStep(AeRam *r)
{
    uint32_t frame_index = ae_rd32(r, AE_MISSA_G_DEMO_FRAME);
    uint32_t next_frame = frame_index + 1u;
    uint32_t history_pos = (ae_rd32(r, AE_MISSA_G_STICK_HISTORY_POS) + 1u) %
                           AE_MISSA_STICK_HISTORY_COUNT;
    uint32_t history_sample = AE_MISSA_G_STICK_HISTORY +
                              history_pos * AE_MISSA_STICK_HISTORY_STRIDE;
    uint32_t pad_status = ae_rd8(r, AE_MISSA_G_PAD_PACKET + AE_MISSA_PAD_STATUS);

    if (pad_status != 0xFFu) {
        uint32_t packet_buttons = ae_rd8(r, AE_MISSA_G_PAD_PACKET + 3u) |
                                  (ae_rd8(r, AE_MISSA_G_PAD_PACKET + 2u) << 8);
        if (((~packet_buttons) & AE_MISSA_DEMO_ABORT_BUTTONS) != 0u) {
            ae_wr32(r, AE_MISSA_G_DEMO_ACTIVE, 0u);
            ae_wr32(r, AE_MISSA_G_DEMO_FRAME, 0u);
            return 34;
        }
    }

    uint32_t demo_buffer = ae_rd32(r, AE_MISSA_G_DEMO_BUFFER);
    uint32_t record = demo_buffer + frame_index * AE_MISSA_DEMO_RECORD_STRIDE;
    uint32_t buttons_word = ae_rd32(r, record + AE_MISSA_DEMO_RECORD_BUTTONS);
    if (buttons_word == 0u) {
        ae_wr32(r, AE_MISSA_G_DEMO_ACTIVE, 0u);
        ae_wr32(r, AE_MISSA_G_DEMO_FRAME, 0u);
        return 32;
    }

    ae_wr32(r, history_sample,
            ae_rd32(r, record + AE_MISSA_DEMO_RECORD_STICK));
    ae_wr16(r, AE_MISSA_G_PAD_PACKET + AE_MISSA_PAD_BUTTONS,
            ae_rd16(r, record + AE_MISSA_DEMO_RECORD_BUTTONS));
    uint32_t active_buttons = ~((buttons_word >> 8) |
                                ((buttons_word & 0xFFu) << 8));
    uint32_t previous_buttons = ae_rd32(r, AE_MISSA_G_PAD_HELD);
    ae_wr32(r, AE_MISSA_G_PAD_HELD, active_buttons);
    ae_wr32(r, AE_MISSA_G_PAD_CHANGED, previous_buttons ^ active_buttons);
    ae_wr16(r, AE_MISSA_G_STICK_HISTORY + history_pos * 2u + 0xA4u, 0u);
    ae_wr32(r, AE_MISSA_G_STICK_HISTORY_POS, history_pos);

    if (frame_index >= 3599u) {
        ae_wr32(r, AE_MISSA_G_DEMO_ACTIVE, 0u);
        ae_wr32(r, AE_MISSA_G_DEMO_FRAME, 0u);
        return 32;
    }
    ae_wr32(r, AE_MISSA_G_DEMO_FRAME, next_frame);
    return 0;
}

uint32_t ae_StickToButtons(AeRam *r, uint32_t buttons)
{
    uint32_t history_pos = ae_rd32(r, AE_MISSA_G_STICK_HISTORY_POS);
    uint32_t sample = AE_MISSA_G_STICK_HISTORY + history_pos * 4u;
    int32_t stick_x = ae_rds8(r, sample + 2u);
    int32_t stick_y = ae_rds8(r, sample + 3u);

    if (stick_x >= 97) {
        buttons |= 0x2000u;
    } else if (stick_x < -96) {
        buttons |= 0x8000u;
    }
    if (stick_y >= 97) {
        buttons |= 0x1000u;
    } else if (stick_y < -96) {
        buttons |= 0x4000u;
    }
    return buttons;
}

uint32_t ae_PlayerInput_StickGesture(AeRam *r)
{
    return ae_rd16(r, AE_MISSA_SCR_STICK_GESTURE) & 0x30u;
}

void ae_ModelInitNormals(AeRam *r, uint32_t model_state, uint32_t model)
{
    uint32_t vertex_count = ae_rd16(r, model + AE_MISSA_MODEL_VERTEX_COUNT);
    uint32_t normal_count = ae_rd16(r, model + AE_MISSA_MODEL_NORMAL_COUNT);
    uint32_t polygon_count = ae_rd16(r, model + AE_MISSA_MODEL_POLYGON_COUNT);
    uint32_t arrays = model + AE_MISSA_MODEL_ARRAYS;
    uint32_t polygons = arrays + polygon_count * AE_MISSA_MODEL_VERTEX_BYTES;
    uint32_t colors = arrays + (vertex_count * 3u + polygon_count) *
                                AE_MISSA_MODEL_VERTEX_BYTES;

    ae_wr32(r, model_state + AE_MISSA_MODEL_STATE_RESOURCE, model);
    ae_wr32(r, model_state + AE_MISSA_MODEL_STATE_VERTICES, arrays);
    ae_wr32(r, model_state + AE_MISSA_MODEL_STATE_NORMALS,
            colors + normal_count * AE_MISSA_MODEL_NORMAL_BYTES);
    ae_wr32(r, model_state + AE_MISSA_MODEL_STATE_POLYGONS, polygons);
    ae_wr32(r, model_state + AE_MISSA_MODEL_STATE_VERTEX_COLORS, colors);
    ae_wr32(r, model_state + AE_MISSA_MODEL_STATE_COLOR_TABLE,
            AE_MISSA_G_MODEL_NORMAL_TABLE);
}

void ae_ScreenFadeSet(AeRam *r, uint32_t count, uint32_t start, uint32_t step)
{
    ae_wr8(r, AE_MISSA_G_FADE_COUNTER + 0u, count);
    ae_wr8(r, AE_MISSA_G_FADE_COUNTER + 1u, start);
    ae_wr8(r, AE_MISSA_G_FADE_COUNTER + 2u, step);
}

void ae_PlayTimeAdd(AeRam *r, int32_t seconds)
{
    int32_t truncated_seconds = (int16_t)(uint16_t)seconds;
    uint32_t frames = (uint32_t)truncated_seconds * 60u;
    uint32_t time_address = AE_MISSA_G_STAGE_STATE + AE_MISSA_STAGE_PLAY_TIME;
    uint32_t total = ae_rd32(r, time_address) + frames;
    ae_wr32(r, time_address, total);
}

int32_t ae_PointInBoxXZ(AeRam *r, uint32_t position, uint32_t box)
{
    int32_t center_x = ae_rds16(r, box + 0x00u);
    int32_t center_z = ae_rds16(r, box + 0x04u);
    int32_t half_x = ae_rds16(r, box + 0x06u);
    int32_t half_z = ae_rds16(r, box + 0x0Au);
    int32_t x = ae_rds32(r, position + 0x00u);
    int32_t z = ae_rds32(r, position + 0x08u);
    int32_t upper_x = missa_s32((uint32_t)center_x + (uint32_t)half_x);
    int32_t lower_x = missa_s32((uint32_t)center_x - (uint32_t)half_x);
    int32_t upper_z = missa_s32((uint32_t)center_z + (uint32_t)half_z);
    int32_t lower_z = missa_s32((uint32_t)center_z - (uint32_t)half_z);

    if (upper_x < x || x < lower_x) {
        return 0;
    }
    if (upper_z < z || z < lower_z) {
        return 0;
    }
    return 1;
}

int32_t ae_DirIndexFromFlags(AeRam *r, uint32_t flags)
{
    (void)r;
    switch (flags & 0x0F00u) {
    case 0x0400u: return 1;
    case 0x0800u: return 2;
    case 0x0C00u: return 3;
    default: return 0;
    }
}

int32_t ae_FadeFrame(AeRam *r, const AeMissaHooks *hooks)
{
    if (!hooks || !hooks->BeginFrame) {
        ae_ram_fail(r, "FadeFrame: BeginFrame hook not provided");
        return 0;
    }
    (void)hooks->BeginFrame(hooks->user, r, 0, 0, 0, 0);
    if (hooks->stage_fade == NULL) {
        ae_ram_fail(r, "FadeFrame: stage fade hooks not provided");
        return 0;
    }
    ae_FadeSpiralDraw(r, hooks->stage_fade);
    return 0;
}
