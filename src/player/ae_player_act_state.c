#include "ae_player_act.h"

#include <stdint.h>

#define AE_PA_PLAYER_STATE_SCRATCH 0x1F800048u
#define AE_PA_EYE_DMA_RECT 0x801FFEE8u
#define AE_PA_EYE_DMA_QUEUE_INDEX 0x800EC35Cu
#define AE_PA_EYE_DMA_QUEUE_PTR 0x800EC354u
#define AE_PA_EYE_DMA_COMMANDS 0x800F03CCu
#define AE_PA_EYE_DMA_COMMAND_STRIDE 0x4078u /* one 16,504-byte command buffer per double-buffer index */
#define AE_PA_EYE_DEST_X 0x03C0
#define AE_PA_EYE_DEST_Y 0
#define AE_PA_EYE_RECT_X 0x03D0
#define AE_PA_EYE_RECT_WIDTH 0x10
#define AE_PA_EYE_RECT_HEIGHT 0x40

static uint32_t pa_addr(uint32_t base, uint32_t offset)
{
    return base + offset;
}

static uint32_t pa_state_set(AeRam *r, int32_t state)
{
    int32_t set_index = 0;
    if (state >= 0x80) {
        set_index = ae_rds8(r, pa_addr(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_CURRENT_GADGET)) + 1;
    }
    return AE_PLAYER_ACT_STATE_SETS + (uint32_t)set_index * AE_PA_SET_SIZE;
}

static uint32_t pa_state_table(AeRam *r, int32_t state)
{
    return ae_rd32(r, pa_addr(pa_state_set(r, state), AE_PA_SET_STATES));
}

static uint32_t pa_state_entry(uint32_t table, uint32_t state)
{
    return table + (state & 0x7Fu) * AE_PA_STATE_ENTRY_SIZE;
}

static int32_t pa_state_frame_index(int32_t state)
{
    int32_t adjusted = state < 0 ? state + 0x7F : state;
    int32_t quotient = adjusted >= 0
            ? adjusted / 0x80
            : (int32_t)(-((-(int64_t)adjusted + 0x7F) / 0x80));
    return state - quotient * 0x80;
}

void ae_PlayerSetState(AeRam *r, const AePlayerActHooks *hooks, uint32_t requested_state)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const int32_t state = (int32_t)requested_state;
    const uint32_t old_state = ae_rd16(r, pa_addr(player, AE_PA_PLAYER_STATE));
    const uint32_t old_table = pa_state_table(r, old_state);
    const uint32_t old_entry = pa_state_entry(old_table, old_state);
    const uint32_t old_frame = ae_rd16(r, pa_addr(player, AE_PA_PLAYER_ANIM_FRAME));
    int blend = 0;

    ae_wr32(r, AE_PA_PLAYER_STATE_SCRATCH, old_table);

    if (state == 0 && old_state != 0) {
        uint32_t last_frame = ae_rd16(r, old_entry + 2u);
        if (old_frame != last_frame + 1u) blend = 1;
    }

    if (state == 0x24) {
        uint32_t mode_state = ae_rd16(r, AE_PLAYER_ACT_STATE_CHANGE_MODE);
        if (mode_state != 0x24 && mode_state != 0x0E && mode_state != 0x8D) {
            blend = 1;
        }
    }

    if ((old_state == 0x46 && state == 0x47) || (old_state == 0x47 && state == 0x46)) {
        blend = 1;
    }

    ae_wr16(r, pa_addr(player, AE_PA_PLAYER_ANIM_LERP), 0);
    ae_wr16(r, pa_addr(player, AE_PA_PLAYER_ANIM_SPEED), 0x1000u);

    if (blend) {
        uint32_t frame_address;
        ae_wr16(r, pa_addr(player, AE_PA_PLAYER_ANIM_BLEND), 5);
        if (hooks == NULL || hooks->AnimFramePtr == NULL) {
            ae_ram_fail(r, "AnimFramePtr hook not provided");
            frame_address = 0;
        } else {
            frame_address = hooks->AnimFramePtr(hooks->user, r, player, (int32_t)old_frame);
        }
        ae_wr32(r, pa_addr(player, AE_PA_PLAYER_ANIM_BLEND_FROM), frame_address);
    } else if (!(state == 0x24 && ae_rd16(r, AE_PLAYER_ACT_STATE_CHANGE_MODE) == 0x24)) {
        ae_wr16(r, pa_addr(player, AE_PA_PLAYER_ANIM_BLEND), 0);
    }

    {
        uint32_t new_set = pa_state_set(r, state);
        uint32_t new_table = ae_rd32(r, pa_addr(new_set, AE_PA_SET_STATES));
        uint32_t frame_entry = new_table + (uint32_t)(pa_state_frame_index(state) * AE_PA_STATE_ENTRY_SIZE);
        uint32_t mode_entry = pa_state_entry(new_table, requested_state);
        uint32_t packed_modes = ae_rd32(r, mode_entry + AE_PA_STATE_PACKED_MODES);
        uint32_t flags = ((packed_modes >> 28) << 4) | ((packed_modes >> 24) & 0x0Fu);

        ae_wr32(r, AE_PA_PLAYER_STATE_SCRATCH, new_table);
        ae_wr16(r, pa_addr(player, AE_PA_PLAYER_STATE), requested_state);
        ae_wr16(r, pa_addr(player, AE_PA_PLAYER_ANIM_FRAME), ae_rd16(r, frame_entry));
        ae_wr32(r, pa_addr(player, AE_PA_PLAYER_ANIM_SET),
                ae_rd32(r, pa_addr(new_set, AE_PA_SET_ANIM)));
        ae_wr32(r, pa_addr(player, AE_PA_PLAYER_STATE_FLAGS), flags);
    }
}

void ae_PlayerSetClimbTarget(AeRam *r, int32_t x, int32_t y, int32_t z, int32_t target_flags,
                             int32_t y_offset, int32_t target_height)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    uint32_t attach_flags;
    ae_wr16(r, pa_addr(player, AE_PA_PLAYER_CLIMB_TARGET), (uint32_t)x);
    ae_wr16(r, pa_addr(player, AE_PA_PLAYER_CLIMB_TARGET + 2u), (uint32_t)y);
    ae_wr16(r, pa_addr(player, AE_PA_PLAYER_CLIMB_TARGET + 4u), (uint32_t)z);
    ae_wr16(r, pa_addr(player, AE_PA_PLAYER_CLIMB_TARGET + 8u), (uint32_t)target_flags);
    ae_wr16(r, pa_addr(player, AE_PA_PLAYER_CLIMB_TARGET + 10u),
            (uint32_t)(y_offset - 0x100));
    ae_wr16(r, pa_addr(player, AE_PA_PLAYER_CLIMB_TARGET + 6u), (uint32_t)target_height);

    if (ae_rd16(r, AE_PLAYER_ACT_MODE) == 5) return;
    attach_flags = ae_rd32(r, pa_addr(player, AE_PA_PLAYER_ATTACH_FLAGS));
    ae_wr32(r, pa_addr(player, AE_PA_PLAYER_ATTACH_FLAGS), attach_flags | 2u);
}

void ae_PlayerSetRope(AeRam *r, uint32_t first, uint32_t second)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    uint32_t rope = pa_addr(player, AE_PA_PLAYER_ROPE);
    uint32_t flags;
    ae_wr16(r, rope, ae_rd16(r, first));
    ae_wr16(r, rope + 2u, ae_rd16(r, first + 2u) + 0x100u);
    ae_wr16(r, rope + 4u, ae_rd16(r, first + 4u));
    ae_wr16(r, rope + 6u, ae_rd16(r, second));
    ae_wr16(r, rope + 8u, ae_rd16(r, second + 2u) + 0x100u);
    ae_wr16(r, rope + 10u, ae_rd16(r, second + 4u));
    flags = ae_rd32(r, pa_addr(player, AE_PA_PLAYER_ATTACH_FLAGS));
    ae_wr32(r, pa_addr(player, AE_PA_PLAYER_ATTACH_FLAGS), flags | 4u);
}

void ae_PlayerSetTintEffect(AeRam *r, int32_t mode)
{
    ae_wr8(r, AE_PLAYER_ACT_TINT_EFFECT + 2u, (uint32_t)mode);
    ae_wr16(r, AE_PLAYER_ACT_TINT_EFFECT + 6u, 0x32u);
    if (mode == 1) {
        ae_wr32(r, AE_PLAYER_ACT_PLAYER + AE_PA_PLAYER_TINT_TARGET, 0x00FF0000u);
    } else if (mode > 0 && mode < 4) {
        ae_wr32(r, AE_PLAYER_ACT_PLAYER + AE_PA_PLAYER_TINT_TARGET, 0);
    }
}

static uint32_t pa_lwl(AeRam *r, uint32_t address, uint32_t old_value)
{
    uint32_t shift = (address & 3u) * 8u;
    uint32_t mask = (1u << (24u - shift)) - 1u;
    uint32_t word = ae_rd32(r, address & ~3u);
    return (old_value & mask) | (word << (24u - shift));
}

static void pa_swl(AeRam *r, uint32_t address, uint32_t value)
{
    uint32_t offset = address & 3u;
    uint32_t aligned = address & ~3u;
    uint32_t i;
    for (i = 0; i <= offset; i++) {
        ae_wr8(r, aligned + i, value >> ((3u - offset + i) * 8u));
    }
}

void ae_PlayerSetEyeFrame(AeRam *r, const AePlayerActHooks *hooks, int32_t frame)
{
    uint32_t queue = ae_rd32(r, AE_PA_EYE_DMA_QUEUE_PTR);
    uint32_t command_index = ae_rd32(r, AE_PA_EYE_DMA_QUEUE_INDEX);
    uint32_t command = AE_PA_EYE_DMA_COMMANDS + command_index * AE_PA_EYE_DMA_COMMAND_STRIDE;
    uint32_t old_command_word;
    uint32_t rect_x = (uint32_t)frame * 0x10u + AE_PA_EYE_RECT_X;
    uint32_t command_count = command_index;

    ae_wr16(r, AE_PA_EYE_DMA_RECT, rect_x);
    ae_wr16(r, AE_PA_EYE_DMA_RECT + 2u, 0);
    ae_wr16(r, AE_PA_EYE_DMA_RECT + 4u, AE_PA_EYE_RECT_WIDTH);
    ae_wr16(r, AE_PA_EYE_DMA_RECT + 6u, AE_PA_EYE_RECT_HEIGHT);
    if (hooks == NULL || hooks->DR_MOVE == NULL) {
        ae_ram_fail(r, "DR_MOVE hook not provided");
        return;
    }
    hooks->DR_MOVE(hooks->user, r, queue, AE_PA_EYE_DMA_RECT,
                   AE_PA_EYE_DEST_X, AE_PA_EYE_DEST_Y);

    old_command_word = pa_lwl(r, command + 2u, command_count);
    pa_swl(r, command + 2u, queue << 8);
    pa_swl(r, queue + 2u, old_command_word);
    ae_wr32(r, AE_PA_EYE_DMA_QUEUE_PTR, queue + 0x18u);
}

