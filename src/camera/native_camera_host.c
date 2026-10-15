/* Portable camera host adapter. See native_camera_host.md. */
#include "native_camera_host.h"

#include <string.h>

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v);
    put16(p + 2, v >> 16);
}

static int32_t to_s32(uint32_t v)
{
    return v > 0x7FFFFFFFu ? (int32_t)(v - 0x80000000u) - 0x7FFFFFFF - 1 : (int32_t)v;
}

static int32_t sext16(uint32_t v)
{
    v &= 0xFFFFu;
    return v >= 0x8000u ? (int32_t)v - 0x10000 : (int32_t)v;
}

static int32_t get_s16(const uint8_t *p)
{
    return sext16((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

static void mark_missing(AeCameraHost *h)
{
    h->status |= AE_CAMERA_FOLLOW_MISSING_CALLBACK;
    h->frame.status |= AE_CAMERA_FOLLOW_MISSING_CALLBACK;
}

static uint32_t host_normalize(void *user, int16_t io[3])
{
    AeCameraHost *h = (AeCameraHost *)user;
    uint32_t sq = 0;
    if (!ae_camera_normalize(&h->math, io, &sq)) {
        h->status |= AE_CAMERA_HOST_MATH_DOMAIN;
        return 0;
    }
    return sq;
}

static int32_t host_sqrt(void *user, uint32_t value)
{
    (void)user;
    return ae_collision_sqrt(to_s32(value), 1);
}

static int32_t host_ground(void *user, int32_t x, int32_t y, int32_t z)
{
    AeCameraHost *h = (AeCameraHost *)user;
    AeGroundHit hit;
    AeSolidHit solid;
    memset(&hit, 0, sizeof hit);
    memset(&solid, 0, sizeof solid);
    if (!h->collision || !h->ground)
        return 0;
    /* The bool result is not used: the no-floor sentinel in hit.height is meaningful. */
    (void)ae_collision_ground_height(h->collision, h->ground, x, y, z, 0, -1, 0, &hit, &solid);
    return sext16((uint32_t)hit.height);
}

static void bind_queries(AeCameraHost *h)
{
    h->queries.user = h;
    h->queries.normalize = host_normalize;
    h->queries.sqrt = host_sqrt;
    h->queries.ground = host_ground;
}

void ae_camera_host_snapshot(AeCameraHost *h, const AeCameraPlayerSnapshot *s)
{
    uint8_t *sp;
    if (!h || !s)
        return;
    sp = h->frame.scratch;
    memcpy(sp + 0x10, s->render8, 8);
    memcpy(sp + 0x40, s->facing8, 8);
    put16(sp + 0x18, (uint32_t)s->pos[0]);
    put16(sp + 0x1A, (uint32_t)s->pos[1]);
    put16(sp + 0x1C, (uint32_t)s->pos[2]);
    put32(sp + 0x68, (uint32_t)s->vel[0]);
    put32(sp + 0x6C, (uint32_t)s->vel[1]);
    put32(sp + 0x70, (uint32_t)s->vel[2]);
    put32(sp + 0x2C, (uint32_t)s->ground_y);
    memcpy(h->snapshot_shadow, s->render8, 8);
    memcpy(h->snapshot_shadow + 8, s->facing8, 8);
}

int ae_camera_host_reset(AeCameraHost *h, const AeCameraPlayerSnapshot *s)
{
    static const uint8_t zero8[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int16_t facing[3];
    int32_t yaw;
    if (!h)
        return 0;
    if (!h->ready || !s) {
        mark_missing(h);
        return 0;
    }
    facing[0] = (int16_t)get_s16(s->facing8);
    facing[1] = (int16_t)get_s16(s->facing8 + 2);
    facing[2] = (int16_t)get_s16(s->facing8 + 4);
    if (!ae_camera_atan2_safe(&h->math, -(int32_t)facing[0], -(int32_t)facing[2], &yaw)) {
        h->status |= AE_CAMERA_HOST_MATH_DOMAIN;
        h->frame.status |= AE_CAMERA_FOLLOW_MATH_DOMAIN;
        return 0;
    }
    bind_queries(h);
    memset(&h->slot, 0, sizeof h->slot);
    ae_camera_init_follow(&h->slot, 1, zero8, zero8, s->pos, facing, yaw);
    memset(h->frame.scratch, 0, sizeof h->frame.scratch);
    memset(h->frame.controls, 0, sizeof h->frame.controls);
    ae_camera_host_snapshot(h, s);
    h->status = 0;
    h->frame.status = 0;
    return 1;
}

int ae_camera_host_init(AeCameraHost *h, const uint8_t *exe, size_t exe_size,
                        AeGround *ground, AeCollision *collision,
                        const AeCameraPlayerSnapshot *s)
{
    if (!h)
        return 0;
    memset(h, 0, sizeof *h);
    if (!exe || !ground || !collision || !s)
        return 0;
    if (!ae_camera_math_load(&h->math, exe, exe_size))
        return 0;
    h->ground = ground;
    h->collision = collision;
    bind_queries(h);
    h->ready = 1;
    if (!ae_camera_host_reset(h, s)) {
        h->ready = 0;
        return 0;
    }
    return 1;
}

int ae_camera_host_step(AeCameraHost *h, const AeCameraPlayerSnapshot *s, int mode)
{
    AeCameraSlot slot_backup;
    AeCameraFollowFrame frame_backup;
    uint8_t shadow_backup[16];
    uint32_t bits;
    int ok;
    if (!h)
        return 0;
    if (!h->ready || !s) {
        mark_missing(h);
        return 0;
    }
    bind_queries(h);
    slot_backup = h->slot;
    frame_backup = h->frame;
    memcpy(shadow_backup, h->snapshot_shadow, sizeof shadow_backup);
    h->status = 0;
    h->frame.status = 0;
    ae_camera_host_snapshot(h, s);
    h->frame.controls[0x47] = 0;
    ok = ae_camera_follow_update(&h->slot, &h->frame, &h->math, &h->queries, mode);
    if (ok && !(h->status & AE_CAMERA_HOST_MATH_DOMAIN))
        return 1;
    bits = h->status | h->frame.status;
    if (ok)
        bits |= AE_CAMERA_FOLLOW_MATH_DOMAIN;
    h->slot = slot_backup;
    h->frame = frame_backup;
    memcpy(h->snapshot_shadow, shadow_backup, sizeof shadow_backup);
    h->status = bits;
    h->frame.status |= bits;
    return 0;
}

int ae_camera_can_exit_stage(uint32_t stage_exit_code, uint32_t state, uint32_t state_flags)
{
    uint32_t code_off = (stage_exit_code & 0xFFu) - 3u; /* wraps like addiu + sltiu */
    state &= 0xFFFFu;
    if (code_off >= 3u)
        return 0;
    if (state == 0x23u || state == 0x4Au || state == 0x2Du)
        return 0;
    return (state_flags & 0xF0u) == 0xE0u ? 0 : 1;
}

int ae_camera_player_submerged(int water_active, int32_t water_y, int32_t player_y)
{
    int32_t limit = to_s32((uint32_t)player_y - 256u);
    if ((water_active & 0xFF) == 0)
        return 0;
    return sext16((uint32_t)water_y) < limit ? 1 : 0;
}
