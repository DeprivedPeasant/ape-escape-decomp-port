/* Stage camera composition. See native_camera_stage.md. */
#include "native_camera_stage.h"
#include "native_camera_bank.h"
#include "native_camera_blend.h"
#include "native_camera_focus.h"
#include "native_camera_motion.h"
#include "native_camera_molten.h"
#include "native_camera_script.h"
#include "native_camera_select.h"

#include <stdlib.h>
#include <string.h>

#define AREA_BASE_TAG 0x80154000u  /* g_AreaData */
#define ACTIVATION_TAG 0x80130000u /* opaque ActivateSlot tag */
#define CAM_SCRIPT_OFFSET 0x0Eu    /* CAM header u16: script table offset */
#define CAM_HEADER_BYTES 0x10u

enum {
    CTL_REGION_COUNT = 0x00,
    CTL_FOLLOW = 0x18,
    CTL_BLEND_LIMIT = 0x2C,
    CTL_NO_REGION = 0x3E,
    CTL_FORCE_FOLLOW = 0x42,
    CTL_GUARD = 0x47
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

static int16_t s16_from_u16(uint16_t v)
{
    return (int16_t)(v < 0x8000u ? (int32_t)v : (int32_t)v - 0x10000);
}

static void facing_s16(const AeCameraPlayerSnapshot *s, int16_t facing[3])
{
    facing[0] = s16_from_u16(rd16(s->facing8));
    facing[1] = s16_from_u16(rd16(s->facing8 + 2));
    facing[2] = s16_from_u16(rd16(s->facing8 + 4));
}

static int host_usable(const AeCameraHost *h)
{
    return h && h->ready && h->math.loaded && h->queries.user == (const void *)h &&
           h->queries.normalize && h->queries.sqrt && h->queries.ground;
}

static int has_free_slot(const AeCameraSlot slots[12])
{
    uint32_t i;

    for (i = 0; i < AE_CAMERA_BANK_SLOTS; i++)
        if (slots[i].raw[0] == 0)
            return 1;
    return 0;
}

/* The original takes the first free slot and returns 0 when the bank is full;
 * the bank service reports that as success with index 0, so fail closed here. */
static int create_slot(AeCameraSlot slots[12], uint8_t controls[0x50], int32_t kind,
                       const uint8_t initial8[8], const uint8_t eye8[8],
                       const uint8_t target8[8], const AeCameraPlayerSnapshot *snap,
                       const int16_t facing[3], const AeCameraMathTables *tables,
                       uint32_t *index)
{
    if (!has_free_slot(slots))
        return 0;
    if (!ae_camera_slot_create(slots, controls, kind, initial8, eye8, target8, snap->pos,
                               facing, tables, index))
        return 0;
    return *index < AE_CAMERA_BANK_SLOTS;
}

int ae_camera_stage_init(AeCameraStage *stage, AeCameraHost *host, const uint8_t *blob,
                         size_t size, const AeCameraPlayerSnapshot *snapshot)
{
    AeCameraRegionFrame work;
    AeCameraRegions regions;
    uint8_t eye8[8], target8[8];
    const uint8_t *table = NULL;
    size_t table_bytes = 0;
    uint32_t table_base = 0;
    int16_t facing[3];
    uint32_t bits = 0, idx = 0, kind, i, count;
    uint8_t *c;

    if (!stage)
        return 0;
    if (stage->ready || !blob || !snapshot || !host_usable(host)) {
        stage->status |= AE_CAMERA_STAGE_MISSING;
        return 0;
    }

    memset(&work, 0, sizeof work);
    memset(&regions, 0, sizeof regions);
    c = work.controls;

    if (!ae_camera_regions_load(&regions, blob, size)) {
        bits = AE_CAMERA_STAGE_INVALID;
        goto fail;
    }
    count = regions.count;
    if (count > 255u) {
        bits = AE_CAMERA_STAGE_INVALID;
        goto fail;
    }

    /* Seed the global eye / target from the host's existing follow slot. */
    memcpy(eye8, host->slot.raw + AE_CAM_EYE, 8);
    memcpy(target8, host->slot.raw + AE_CAM_TARGET, 8);
    facing_s16(snapshot, facing);

    if (count > 0) {
        size_t cam = regions.cam_offset, off;

        if (cam > size || size - cam < CAM_HEADER_BYTES) {
            bits = AE_CAMERA_STAGE_INVALID;
            goto fail;
        }
        off = cam + rd16(blob + cam + CAM_SCRIPT_OFFSET);
        if (off > size || size - off < AE_CAMERA_SCRIPT_TABLE_HEADER ||
            off > 0xFFFFFFFFu - AREA_BASE_TAG) {
            bits = AE_CAMERA_STAGE_INVALID;
            goto fail;
        }
        table = blob + off;
        table_bytes = size - off;
        table_base = AREA_BASE_TAG + (uint32_t)off;
    }

    c[AE_CAMCTL_ACTIVE_1B] = 0xFF;
    c[AE_CAMCTL_CLEAR_D] = 0xFF;
    wr16(c + CTL_BLEND_LIMIT, 0x148);
    wr16(c + CTL_REGION_COUNT, count);

    for (kind = 1; kind <= 3; kind++) {
        if (!create_slot(work.slots, c, (int32_t)kind, eye8, eye8, target8, snapshot, facing,
                         &host->math, &idx)) {
            bits = AE_CAMERA_STAGE_INVALID;
            goto fail;
        }
        c[CTL_FOLLOW + kind - 1] = (uint8_t)idx;
    }

    for (i = 0; i < count; i++) {
        uint8_t *rec = regions.records + (size_t)i * AE_CAMERA_REGION_STRIDE;
        uint16_t flags = rd16(rec + 2);

        wr16(rec + 2, flags & 0xFFF0u);
        if ((flags & 0x6000u) != 0x2000u)
            continue;
        /* Persistent kind 5: the region center is the initial eye. The original
         * does not store the region index in slot byte 0x93 here. */
        if (!create_slot(work.slots, c, 5, rec + 4, eye8, target8, snapshot, facing,
                         &host->math, &idx)) {
            bits = AE_CAMERA_STAGE_INVALID;
            goto fail;
        }
        wr16(rec + 2, (flags & 0xFFF0u) | (idx & 0xFu));
        if (s16_from_u16(rd16(rec)) < 0 ||
            !ae_camera_set_script(&work.slots[idx], table, table_bytes, table_base,
                                  (uint32_t)s16_from_u16(rd16(rec)),
                                  (uint8_t)((flags >> 13) & 3u))) {
            bits = AE_CAMERA_STAGE_INVALID;
            goto fail;
        }
    }

    c[AE_CAMCTL_LAST_E] = c[CTL_FOLLOW];
    if (!ae_camera_activate_slot(c, work.slots, AE_CAMERA_BANK_SLOTS, c[CTL_FOLLOW],
                                 ACTIVATION_TAG)) {
        bits = AE_CAMERA_STAGE_INVALID;
        goto fail;
    }

    stage->frame = work;
    stage->regions = regions;
    stage->blob = blob;
    stage->blob_size = size;
    stage->script_table = table;
    stage->script_table_bytes = table_bytes;
    stage->original_table_base = table_base;
    memcpy(stage->eye8, eye8, 8);
    memcpy(stage->target8, target8, 8);
    stage->status = 0;
    stage->ready = 1;
    return 1;

fail:
    ae_camera_regions_free(&regions);
    stage->status |= bits;
    return 0;
}

void ae_camera_stage_free(AeCameraStage *stage)
{
    if (!stage)
        return;
    ae_camera_regions_free(&stage->regions);
    memset(stage, 0, sizeof *stage);
}

typedef struct StageCtx {
    AeCameraHost *host;
    uint8_t *controls;
    const AeCameraPlayerSnapshot *snapshot;
    const uint8_t *region_focus8;
    uint32_t bits;
} StageCtx;

/* CameraSlotGetFocusPoint: the output is zeroed first so every byte is defined
 * (native policy, not proof of the original stack contents). */
static int ctx_focus(void *user, const AeCameraSlot *slot, uint8_t mode, uint8_t out8[8])
{
    StageCtx *x = (StageCtx *)user;

    memset(out8, 0, 8);
    if (!ae_camera_get_focus_point(slot, (int32_t)mode, x->snapshot->pos, x->snapshot->facing8,
                                   x->region_focus8, out8)) {
        x->bits |= AE_CAMERA_STAGE_MISSING;
        return 0;
    }
    return 1;
}

/* Indexed ids are area-overlay-dependent. Handler 11 is only bound for area 05. */
static int ctx_indexed(void *user, AeCameraSlot *slot, uint8_t id)
{
    StageCtx *x = (StageCtx *)user;

    if (id == 11 && x->host->frame.current_area == 0x05) {
        if (!ae_camera_molten_update(slot, &x->host->math, &x->host->queries,
                                     x->snapshot->pos, x->region_focus8,
                                     x->host->frame.scratch)) {
            x->bits |= AE_CAMERA_STAGE_MATH_DOMAIN;
            return 0;
        }
        return 1;
    }
    if (id != 3) {
        x->bits |= AE_CAMERA_STAGE_UNSUPPORTED_HANDLER;
        return 0;
    }
    x->controls[CTL_FORCE_FOLLOW] = 1;
    return 1;
}

int ae_camera_stage_step(AeCameraStage *stage, AeCameraHost *host,
                         const AeCameraPlayerSnapshot *snapshot, int is_flight_area)
{
    AeCameraRegionFrame frame_backup;
    AeCameraSlot host_slot_backup;
    AeCameraFollowFrame host_frame_backup;
    uint8_t shadow_backup[16], eye_backup[8], target_backup[8];
    uint32_t host_status_backup;
    uint8_t *regions_backup = NULL;
    uint8_t *c;
    AeCameraSelection ev;
    AeCameraMotionQueries mq;
    StageCtx ctx;
    int16_t facing[3];
    uint32_t bits = 0, i, active;
    uint16_t count;
    size_t region_bytes;

    if (!stage)
        return 0;
    if (!stage->ready || !snapshot || !host_usable(host)) {
        stage->status |= AE_CAMERA_STAGE_MISSING;
        return 0;
    }
    count = stage->regions.count;
    region_bytes = (size_t)count * AE_CAMERA_REGION_STRIDE;
    if (count > 255u || (count > 0 && !stage->regions.records)) {
        stage->status |= AE_CAMERA_STAGE_INVALID;
        return 0;
    }
    if (count > 0) {
        regions_backup = (uint8_t *)malloc(region_bytes);
        if (!regions_backup) {
            stage->status |= AE_CAMERA_STAGE_MISSING;
            return 0;
        }
        memcpy(regions_backup, stage->regions.records, region_bytes);
    }
    frame_backup = stage->frame;
    memcpy(eye_backup, stage->eye8, 8);
    memcpy(target_backup, stage->target8, 8);
    host_slot_backup = host->slot;
    host_frame_backup = host->frame;
    memcpy(shadow_backup, host->snapshot_shadow, sizeof shadow_backup);
    host_status_backup = host->status;

    c = stage->frame.controls;
    host->status = 0;
    host->frame.status = 0;
    ae_camera_host_snapshot(host, snapshot);
    facing_s16(snapshot, facing);

    /* CameraRegionUpdate unless control 0x3E is set. */
    if (c[CTL_NO_REGION] == 0 &&
        !ae_camera_region_update(&stage->frame, stage->regions.records, region_bytes, count,
                                 snapshot->pos, facing, stage->eye8, stage->target8,
                                 &host->math, stage->script_table, stage->script_table_bytes,
                                 stage->original_table_base, is_flight_area, ACTIVATION_TAG)) {
        bits = AE_CAMERA_STAGE_INVALID;
        goto fail;
    }

    /* CameraSelectMode and its ordered snapshot / activate requests. */
    if (!ae_camera_select_mode(c, stage->frame.slots, AE_CAMERA_BANK_SLOTS, snapshot->pos,
                               stage->eye8, stage->target8, is_flight_area, &host->math, &ev)) {
        bits = AE_CAMERA_STAGE_INVALID;
        goto fail;
    }
    if (ev.snapshot_follow) {
        uint32_t follow = c[CTL_FOLLOW];

        if (follow >= AE_CAMERA_BANK_SLOTS ||
            !ae_camera_snapshot_eye(&stage->frame.slots[follow], snapshot->pos, stage->eye8,
                                    &host->math)) {
            bits = AE_CAMERA_STAGE_INVALID;
            goto fail;
        }
    }
    if (ev.activate) {
        if (ev.slot < 0 || (uint32_t)ev.slot >= AE_CAMERA_BANK_SLOTS ||
            !ae_camera_activate_slot(c, stage->frame.slots, AE_CAMERA_BANK_SLOTS,
                                     (uint32_t)ev.slot, ACTIVATION_TAG)) {
            bits = AE_CAMERA_STAGE_INVALID;
            goto fail;
        }
    }
    c[CTL_GUARD] = 0; /* DAT_800E6917, cleared once per frame */

    ctx.host = host;
    ctx.controls = c;
    ctx.snapshot = snapshot;
    ctx.region_focus8 = stage->frame.focus;
    ctx.bits = 0;
    mq.user = &ctx;
    mq.focus = ctx_focus;
    mq.indexed = ctx_indexed;

    /* First loop: kind 4 / 5 script run, bank remove request, motion update. The
     * original tests the kind once, so the motion update still runs for a slot
     * whose script removed it. Stereo (byte 0x800AF131) is not supplied: 0. */
    for (i = 0; i < AE_CAMERA_BANK_SLOTS; i++) {
        AeCameraSlot *s = &stage->frame.slots[i];
        AeCameraScriptResult res;

        if ((uint8_t)(s->raw[0] - 4u) >= 2u)
            continue;
        if (!ae_camera_script_run(s, c, stage->script_table, stage->script_table_bytes,
                                  stage->original_table_base, &res)) {
            bits = AE_CAMERA_STAGE_INVALID;
            goto fail;
        }
        if (res.remove_slot != UINT32_MAX &&
            !ae_camera_slot_remove(stage->frame.slots, c, stage->regions.records, region_bytes,
                                   count, res.remove_slot)) {
            bits = AE_CAMERA_STAGE_INVALID;
            goto fail;
        }
        if (!ae_camera_slot_update(s, snapshot->pos, 0, &host->math, &mq)) {
            bits = ctx.bits ? ctx.bits : AE_CAMERA_STAGE_MATH_DOMAIN;
            goto fail;
        }
    }

    active = rd16(c + AE_CAMCTL_CURRENT);
    if (active >= AE_CAMERA_BANK_SLOTS) {
        bits = AE_CAMERA_STAGE_INVALID;
        goto fail;
    }

    /* Second loop: only kind 1 follow; kinds 0, 4, 5 and the unselected reserved
     * kinds 2 / 3 do nothing; a selected kind 2 / 3 or any other kind fails. */
    for (i = 0; i < AE_CAMERA_BANK_SLOTS; i++) {
        AeCameraSlot *s = &stage->frame.slots[i];
        uint8_t kind = s->raw[0];
        int ok;

        if (kind == 0 || kind == 4 || kind == 5)
            continue;
        if (kind == 2 || kind == 3) {
            if (i == active) {
                bits = AE_CAMERA_STAGE_UNSUPPORTED_MODE;
                goto fail;
            }
            continue;
        }
        if (kind != 1) {
            bits = AE_CAMERA_STAGE_UNSUPPORTED_MODE;
            goto fail;
        }
        memcpy(host->frame.controls, c, sizeof host->frame.controls);
        host->frame.status = 0;
        ok = ae_camera_follow_update(s, &host->frame, &host->math, &host->queries, 0);
        if (!ok || (host->status & AE_CAMERA_HOST_MATH_DOMAIN)) {
            bits = host->frame.status & 0xFu;
            if (host->status & AE_CAMERA_HOST_MATH_DOMAIN)
                bits |= AE_CAMERA_STAGE_MATH_DOMAIN;
            if (!bits)
                bits = AE_CAMERA_STAGE_MATH_DOMAIN;
            goto fail;
        }
        memcpy(c, host->frame.controls, sizeof host->frame.controls);
    }

    if (!ae_camera_apply_slot(c, &stage->frame.slots[active], stage->eye8, stage->target8)) {
        bits = AE_CAMERA_STAGE_INVALID;
        goto fail;
    }

    /* Display slot for the renderer; the bank slot keeps the real follow state. */
    host->slot = stage->frame.slots[active];
    memcpy(host->slot.raw + AE_CAM_EYE, stage->eye8, 8);
    memcpy(host->slot.raw + AE_CAM_TARGET, stage->target8, 8);
    memcpy(host->frame.controls, c, sizeof host->frame.controls);
    host->status = 0;
    host->frame.status = 0;
    stage->status = 0;
    free(regions_backup);
    return 1;

fail:
    stage->frame = frame_backup;
    if (regions_backup)
        memcpy(stage->regions.records, regions_backup, region_bytes);
    memcpy(stage->eye8, eye_backup, 8);
    memcpy(stage->target8, target_backup, 8);
    host->slot = host_slot_backup;
    host->frame = host_frame_backup;
    memcpy(host->snapshot_shadow, shadow_backup, sizeof shadow_backup);
    host->status = host_status_backup;
    stage->status |= bits ? bits : AE_CAMERA_STAGE_INVALID;
    free(regions_backup);
    return 0;
}
