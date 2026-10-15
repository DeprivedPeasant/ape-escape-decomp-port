/* Camera slot ground clamp helpers. See native_camera_collision.h / .md. */
#include "native_camera_collision.h"

static int32_t i32_from_u32(uint32_t v)
{
    if (v & 0x80000000u)
        return (int32_t)(v & 0x7FFFFFFFu) - 0x7FFFFFFF - 1;
    return (int32_t)v;
}

static int16_t wrap_s16(int32_t v)
{
    uint32_t u = (uint32_t)v & 0xFFFFu;
    return u >= 0x8000u ? (int16_t)((int32_t)u - 0x10000) : (int16_t)u;
}

/* Arithmetic right shift (floor) without implementation-defined behaviour. */
static int32_t floor_shr(int32_t v, int n)
{
    return v >= 0 ? (v >> n) : (-1 - ((-1 - v) >> n));
}

/* Wrapped 32 bit a + b. */
static int32_t add32(int32_t a, int32_t b)
{
    return i32_from_u32((uint32_t)a + (uint32_t)b);
}

/* Low 32 bits of a * b, as signed. */
static int32_t mul32(int32_t a, int32_t b)
{
    return i32_from_u32((uint32_t)a * (uint32_t)b);
}

/* GetGroundHeight with the result truncated to signed 16 bit (the original's
 * sll / sra by 16). */
static int32_t ground16(const AeCameraQueries *q, int32_t x, int32_t y, int32_t z)
{
    return wrap_s16(q->ground(q->user, x, y, z));
}

int ae_camera_ground_clamp(AeCameraSlot *s, const int16_t focus[3],
                           const AeCameraQueries *q)
{
    int16_t v[3];
    int32_t ex, ey, ez, g, focus_y;

    if (!s || !focus || !q || !q->normalize || !q->ground)
        return 0;

    ex = ae_camera_i16(s, AE_CAM_EYE);
    ey = ae_camera_i16(s, AE_CAM_EYE + 2);
    ez = ae_camera_i16(s, AE_CAM_EYE + 4);

    v[0] = wrap_s16((int32_t)focus[0] - ex);
    v[1] = (int16_t)ey; /* the original stores the current eye Y, not 0 */
    v[2] = wrap_s16((int32_t)focus[2] - ez);
    (void)q->normalize(q->user, v);

    g = ground16(q, add32(ex, floor_shr(v[0], 5)), ey,
                 add32(ez, floor_shr(v[2], 5)));
    focus_y = focus[1];

    if (g - 0x80 < ey && ey < g + 0x110) {
        if (focus_y < g + 0x100)
            ae_camera_set16(s, AE_CAM_EYE + 2, g - 0x80);
        if (ae_camera_i16(s, AE_CAM_EYE + 2) < focus_y - 0x400)
            ae_camera_set16(s, AE_CAM_EYE + 2, focus_y - 0x400);
    }
    return 1;
}

int ae_camera_ground_clamp_near(AeCameraSlot *s, const int16_t desired_eye[3],
                                int near, const AeCameraQueries *q)
{
    int16_t v[3];
    int32_t ex, ey, ez, root, g1, g2, desired_y;
    uint32_t len_sq;
    int32_t px, pz;

    if (!s || !desired_eye || !q || !q->normalize || !q->sqrt || !q->ground)
        return 0;

    ex = ae_camera_i16(s, AE_CAM_EYE);
    ey = ae_camera_i16(s, AE_CAM_EYE + 2);
    ez = ae_camera_i16(s, AE_CAM_EYE + 4);

    v[0] = wrap_s16((int32_t)desired_eye[0] - ex);
    v[1] = 0;
    v[2] = wrap_s16((int32_t)desired_eye[2] - ez);
    len_sq = q->normalize(q->user, v);

    root = q->sqrt(q->user, len_sq);
    if (root > 0x40)
        root = 0x40;

    px = mul32(v[0], root);
    pz = mul32(v[2], root);
    g1 = ground16(q, add32(ex, floor_shr(px, 10)), ey,
                  add32(ez, floor_shr(pz, 10)));
    g2 = ground16(q, add32(ex, floor_shr(px, 11)), ey,
                  add32(ez, floor_shr(pz, 11)));
    desired_y = desired_eye[1];

    if (g1 < desired_y - 0x100 || g2 < desired_y - 0x100) {
        if (near != 0 && s->raw[AE_CAM_CLAMP_STATE] == 0)
            ae_camera_set32(s, AE_CAM_YAW, ae_camera_i32(s, AE_CAM_SAVED_YAW));
        s->raw[AE_CAM_CLAMP_FLAG] = 0;
        if (s->raw[AE_CAM_CLAMP_COUNT] < 255u)
            s->raw[AE_CAM_CLAMP_COUNT]++;
        s->raw[AE_CAM_CLAMP_TIMER] = 6;
    } else {
        if (s->raw[AE_CAM_CLAMP_TIMER] == 0)
            s->raw[AE_CAM_CLAMP_COUNT] = 0;
        if (s->raw[AE_CAM_STILL_FRAMES] == 0 && s->raw[AE_CAM_CLAMP_FLAG] != 255u)
            s->raw[AE_CAM_CLAMP_FLAG]++;
    }
    return 1;
}
