/* CameraRegionUpdate (0x8004BF20). See native_camera_lifecycle.h / .md. */
#include "native_camera_lifecycle.h"
#include "native_camera_bank.h"
#include "native_camera_regions.h"
#include "native_camera_select.h"

#include <stdlib.h>
#include <string.h>

enum {
    CTL_TIMER = 0x0C,
    CTL_PREV = 0x0D,
    CTL_FOLLOW = 0x18,
    CTL_ACTIVE = 0x1B,
    CTL_3F = 0x3F,
    CTL_40 = 0x40,
    CTL_42 = 0x42
};

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static void wr16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static int16_t s16_from_u32(uint32_t v)
{
    v &= 0xFFFFu;
    return (int16_t)(v < 0x8000u ? (int32_t)v : (int32_t)v - 0x10000);
}

/* (s16 + (s16 < 0)) >> 1 == truncation toward zero. */
static uint16_t half_s16(const uint8_t *p)
{
    int32_t v = s16_from_u32(rd16(p));
    return (uint16_t)((uint32_t)(v / 2) & 0xFFFFu);
}

/* CameraSlotRemove(slot) as called by CameraRegionUpdate. */
static int remove_slot(AeCameraRegionFrame *f, uint8_t *regions, size_t regions_bytes,
                       uint16_t count, uint32_t index)
{
    return ae_camera_slot_remove(f->slots, f->controls, regions, regions_bytes, count, index);
}

static void clear_selection_controls(uint8_t *c)
{
    c[CTL_ACTIVE] = 0xFF;
    c[CTL_3F] = 0;
    c[CTL_40] = 0;
    c[CTL_42] = 0;
}

static int run(AeCameraRegionFrame *f, uint8_t *regions, size_t regions_bytes,
               uint16_t count, const int32_t player[3], const int16_t facing[3],
               const uint8_t eye8[8], const uint8_t target8[8],
               const AeCameraMathTables *tables, const uint8_t *script_table,
               size_t script_table_bytes, uint32_t script_table_base,
               int is_flight_area, uint32_t slot_base)
{
    uint8_t *c = f->controls;
    int16_t point[3];
    int32_t hit;
    uint32_t query, prev;

    point[0] = s16_from_u32((uint32_t)player[0]);
    point[1] = s16_from_u32((uint32_t)player[1] - 0x80u);
    point[2] = s16_from_u32((uint32_t)player[2]);

    if (c[CTL_TIMER] != 0)
        c[CTL_TIMER] = (uint8_t)(c[CTL_TIMER] - 1u);

    hit = ae_camera_region_test(regions, regions_bytes, count, point);
    if (hit == -2)
        return 0;
    query = hit == -1 ? 0xFFu : (uint32_t)hit;

    prev = c[CTL_PREV];
    if (query == prev)
        return 1;
    if (c[CTL_TIMER] == 0)
        c[CTL_TIMER] = 1;
    if (c[CTL_TIMER] != 1)
        return 1;

    c[CTL_TIMER] = 0;

    if (prev != 0xFFu && prev >= count)
        return 0;

    if (query == 0xFFu) {
        /* prev != 0xFF here (query != prev). */
        uint16_t oflags = rd16(regions + (size_t)prev * AE_CAMERA_REGION_STRIDE + 2);
        uint32_t nib = oflags & 0xFu, mode = oflags & 0x6000u;
        AeCameraSelection ev;

        if (nib != 0 && (mode == 0 || mode == 0x6000u) &&
            !remove_slot(f, regions, regions_bytes, count, nib))
            return 0;
        clear_selection_controls(c);
        if (!ae_camera_select_mode(c, f->slots, AE_CAMERA_BANK_SLOTS, player, eye8, target8,
                                   is_flight_area, tables, &ev))
            return 0;
        if (ev.activate) {
            if (ev.slot < 0 || (uint32_t)ev.slot >= AE_CAMERA_BANK_SLOTS)
                return 0;
            if (ev.snapshot_follow) {
                if (c[CTL_FOLLOW] >= AE_CAMERA_BANK_SLOTS ||
                    !ae_camera_snapshot_eye(&f->slots[c[CTL_FOLLOW]], player, eye8, tables))
                    return 0;
            }
            if (!ae_camera_activate_slot(c, f->slots, AE_CAMERA_BANK_SLOTS,
                                         (uint32_t)ev.slot, slot_base))
                return 0;
        }
    } else {
        uint8_t *rec = regions + (size_t)query * AE_CAMERA_REGION_STRIDE;
        uint16_t flags;
        uint32_t nib;

        if (prev != 0xFFu) {
            uint16_t oflags = rd16(regions + (size_t)prev * AE_CAMERA_REGION_STRIDE + 2);

            if ((oflags & 0xFu) != 0 && (oflags & 0x6000u) == 0 &&
                !remove_slot(f, regions, regions_bytes, count, oflags & 0xFu))
                return 0;
        }
        clear_selection_controls(c);

        memcpy(f->focus, rec + 4, 6);
        wr16(f->focus + 8, half_s16(rec + 10));
        wr16(f->focus + 10, half_s16(rec + 12));
        wr16(f->focus + 12, half_s16(rec + 14));

        flags = rd16(rec + 2);
        if ((flags & 0x8000u) == 0) {
            flags = (uint16_t)(flags | 0x8000u);
            wr16(rec + 2, flags);
        }
        nib = flags & 0xFu;

        if (nib == 0) {
            int16_t script_id = s16_from_u32(rd16(rec));
            int param2 = (flags & 0x6000u) == 0x4000u;
            uint32_t idx = 0;

            if (script_id < 0)
                return 0;
            /* EventCamCreate: GetEye, CameraSlotCreate(4), EventCamSetScript. */
            if (!ae_camera_slot_create(f->slots, c, 4, eye8, eye8, target8, player, facing,
                                       tables, &idx))
                return 0;
            if (idx >= AE_CAMERA_BANK_SLOTS ||
                !ae_camera_set_script(&f->slots[idx], script_table, script_table_bytes,
                                      script_table_base, (uint32_t)script_id, 0))
                return 0;
            if (param2 == 0) {
                if (!ae_camera_activate_slot(c, f->slots, AE_CAMERA_BANK_SLOTS, idx, slot_base))
                    return 0;
                c[CTL_ACTIVE] = (uint8_t)idx;
            }
            rec = regions + (size_t)query * AE_CAMERA_REGION_STRIDE;
            wr16(rec + 2, (uint32_t)(rd16(rec + 2) & 0xFFF0u) | (idx & 0xFu));
            f->slots[idx].raw[0x93] = (uint8_t)query;
        } else {
            AeCameraSlot *s;

            if (nib >= AE_CAMERA_BANK_SLOTS)
                return 0;
            s = &f->slots[nib];
            c[CTL_ACTIVE] = (uint8_t)nib;
            s->raw[0x88] = s->raw[0x8A];
            s->raw[0x89] = s->raw[0x8B];
            if ((flags & 0x6000u) == 0) {
                memcpy(s->raw + AE_CAM_EYE, eye8, 8);
                memcpy(s->raw + AE_CAM_TARGET, target8, 8);
            }
        }
    }

    /* Remove may have overwritten these; the original stores them last. */
    c[CTL_TIMER] = 0;
    c[CTL_PREV] = (uint8_t)query;
    return 1;
}

int ae_camera_region_update(AeCameraRegionFrame *frame, uint8_t *regions,
                            size_t regions_bytes, uint16_t region_count,
                            const int32_t player[3], const int16_t facing[3],
                            const uint8_t eye8[8], const uint8_t target8[8],
                            const AeCameraMathTables *tables,
                            const uint8_t *script_table, size_t script_table_bytes,
                            uint32_t script_table_base, int is_flight_area,
                            uint32_t original_slot_base)
{
    AeCameraRegionFrame work;
    uint8_t *copy = NULL;
    size_t need = (size_t)region_count * AE_CAMERA_REGION_STRIDE;
    int ok;

    if (!frame || !player || !eye8 || !target8 || region_count > 255u)
        return 0;
    if (region_count > 0 && (!regions || regions_bytes < need))
        return 0;

    work = *frame;
    if (region_count > 0) {
        copy = (uint8_t *)malloc(need);
        if (!copy)
            return 0;
        memcpy(copy, regions, need);
    }

    ok = run(&work, copy, need, region_count, player, facing, eye8, target8, tables,
             script_table, script_table_bytes, script_table_base, is_flight_area,
             original_slot_base);
    if (ok) {
        *frame = work;
        if (region_count > 0)
            memcpy(regions, copy, need);
    }
    free(copy);
    return ok;
}
