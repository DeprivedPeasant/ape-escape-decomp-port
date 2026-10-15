#include "../core/ae_missa.h"

#define AE_MISSA_TRAIL_RING_CURSOR       0x800B8AB0u
#define AE_MISSA_TRAIL_RING_RECORDS      (AE_MISSA_TRAIL_RING_CURSOR + 4u)
#define AE_MISSA_TRAIL_RING_STRIDE       20u
#define AE_MISSA_TRAIL_RING_CAPACITY     12u
#define AE_MISSA_TRAIL_RING_LIFE_OFFSET  16u
#define AE_MISSA_TRAIL_MATRIX            0x800E78C0u
#define AE_MISSA_TRAIL_OT_POINTER        0x800EC354u
#define AE_MISSA_TRAIL_OT_LINK_POINTER   0x800EC358u
#define AE_MISSA_TRAIL_PACKET_OFFSET     36u
#define AE_MISSA_TRAIL_PACKET_STRIDE     40u

static int32_t missa_renderer_s32(uint32_t value)
{
    if ((value & 0x80000000u) == 0u) return (int32_t)value;
    return (int32_t)((int64_t)value - 0x100000000LL);
}

static uint32_t missa_trail_record(uint32_t index)
{
    return AE_MISSA_TRAIL_RING_RECORDS + index * AE_MISSA_TRAIL_RING_STRIDE;
}

static uint32_t missa_trail_previous_index(uint32_t index)
{
    int32_t wrapped = missa_renderer_s32(index + 11u);
    return (uint32_t)(wrapped % (int32_t)AE_MISSA_TRAIL_RING_CAPACITY);
}

static int32_t missa_trail_call(AeRam *r, const AeMissaHooks *hooks,
                                AeMissaHookFn callback, const char *name,
                                uint32_t matrix)
{
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "TrailRingDraw: %s renderer hook missing", name);
        return 0;
    }
    return callback(hooks->user, r, matrix, 0u, 0u, 0u);
}

void ae_TrailRingDraw(AeRam *r, const AeMissaHooks *hooks,
                      uint32_t a0, uint32_t a1, uint32_t a2,
                      uint32_t a3, uint32_t a4)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;

    uint32_t current_index = ae_rd32(r, AE_MISSA_TRAIL_RING_CURSOR);
    uint32_t current_record = missa_trail_record(current_index);
    int32_t current_life = missa_renderer_s32(
        ae_rd32(r, current_record + AE_MISSA_TRAIL_RING_LIFE_OFFSET));
    if (current_life <= 0) return;

    (void)missa_trail_call(r, hooks,
        hooks != NULL ? hooks->SetRotMatrix : NULL, "SetRotMatrix",
        AE_MISSA_TRAIL_MATRIX);
    if (ae_ram_failed(r)) return;
    (void)missa_trail_call(r, hooks,
        hooks != NULL ? hooks->SetTransMatrix : NULL, "SetTransMatrix",
        AE_MISSA_TRAIL_MATRIX);
    if (ae_ram_failed(r)) return;

    uint32_t next_packet = ae_rd32(r, AE_MISSA_TRAIL_OT_POINTER);
    uint32_t order_table_entry = ae_rd32(r, AE_MISSA_TRAIL_OT_LINK_POINTER);
    uint32_t packet = next_packet + AE_MISSA_TRAIL_PACKET_OFFSET;

    while (current_life > 0) {
        uint32_t previous_index = missa_trail_previous_index(current_index);
        uint32_t previous_record = missa_trail_record(previous_index);
        int32_t previous_life = missa_renderer_s32(
            ae_rd32(r, previous_record + AE_MISSA_TRAIL_RING_LIFE_OFFSET));

        if (previous_life <= 0) {
            if (current_life < 64) {
                ae_wr32(r, current_record + AE_MISSA_TRAIL_RING_LIFE_OFFSET, 0u);
            }
            break;
        }

        if (hooks == NULL || hooks->TrailRingDraw == NULL) {
            ae_ram_fail(r, "TrailRingDraw: segment renderer hook missing");
            return;
        }
        (void)hooks->TrailRingDraw(hooks->user, r, current_record,
            previous_record, packet, order_table_entry,
            (uint32_t)current_life);

        current_life -= 32;
        ae_wr32(r, current_record + AE_MISSA_TRAIL_RING_LIFE_OFFSET,
                current_life > 0 ? (uint32_t)current_life : 0u);
        next_packet += AE_MISSA_TRAIL_PACKET_STRIDE;
        packet += AE_MISSA_TRAIL_PACKET_STRIDE;
        current_index = previous_index;
        current_record = previous_record;
        current_life = missa_renderer_s32(
            ae_rd32(r, current_record + AE_MISSA_TRAIL_RING_LIFE_OFFSET));
    }

    /* The renderer owns the packet bytes; this game-side cursor update follows each segment. */
    ae_wr32(r, AE_MISSA_TRAIL_OT_POINTER, next_packet);
}
