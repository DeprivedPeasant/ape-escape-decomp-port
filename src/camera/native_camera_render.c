/* Host-side render adapter for the native camera slot. See native_camera_render.md. */
#include "native_camera_render.h"

#include <math.h>

#define RENDER_PI 3.14159265358979323846
#define RENDER_EPS 1e-6
#define RENDER_MIN_DEPTH 1e-3

int ae_camera_render_orbit(const AeCameraSlot *slot, float target[3], float *yaw,
                           float *pitch, float *distance)
{
    double t[3], d[3], dist, horiz, y, p;
    float out_target[3], out_yaw, out_pitch, out_dist;
    int i;

    if (!slot || !target || !yaw || !pitch || !distance)
        return 0;
    for (i = 0; i < 3; i++) {
        double ts = ae_camera_i16(slot, AE_CAM_TARGET + 2 * (size_t)i);
        double es = ae_camera_i16(slot, AE_CAM_EYE + 2 * (size_t)i);
        if (i == 1) {
            ts = -ts;
            es = -es;
        }
        t[i] = ts / 256.0;
        d[i] = es / 256.0 - t[i];
    }
    dist = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    horiz = sqrt(d[0] * d[0] + d[2] * d[2]);
    if (!(dist > RENDER_EPS) || !(horiz > RENDER_EPS))
        return 0;
    y = atan2(-d[0], d[2]) * 180.0 / RENDER_PI;
    p = atan2(d[1], horiz) * 180.0 / RENDER_PI;
    for (i = 0; i < 3; i++)
        out_target[i] = (float)t[i];
    out_yaw = (float)y;
    out_pitch = (float)p;
    out_dist = (float)dist;
    if (!isfinite(out_target[0]) || !isfinite(out_target[1]) || !isfinite(out_target[2]) ||
        !isfinite(out_yaw) || !isfinite(out_pitch) || !isfinite(out_dist))
        return 0;
    for (i = 0; i < 3; i++)
        target[i] = out_target[i];
    *yaw = out_yaw;
    *pitch = out_pitch;
    *distance = out_dist;
    return 1;
}

int ae_camera_project_approx(const AeCameraSlot *slot, const int16_t point[3], float focal,
                             float offset_x, float offset_y, uint32_t *sxy)
{
    double eye[3], d[3], hx, hz, hn, len, vy, ux, uz, h, row[3][3], rel[3], view[3];
    double sx, sy;
    int xi, yi, i, j;

    if (!slot || !point || !sxy || !isfinite(focal) || focal <= 0.0f ||
        !isfinite(offset_x) || !isfinite(offset_y))
        return 0;
    for (i = 0; i < 3; i++) {
        eye[i] = ae_camera_i16(slot, AE_CAM_EYE + 2 * (size_t)i);
        d[i] = (double)ae_camera_i16(slot, AE_CAM_TARGET + 2 * (size_t)i) - eye[i];
    }
    /* Original: a zero horizontal delta becomes (0, 0, 0x1000) before both normalisations. */
    if (d[0] == 0.0 && d[2] == 0.0)
        d[2] = 4096.0;
    hx = d[0];
    hz = d[2];
    hn = sqrt(hx * hx + hz * hz);
    len = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(hn > 0.0) || !(len > 0.0))
        return 0;
    hx /= hn;
    hz /= hn;
    ux = d[0] / len;
    vy = d[1] / len;
    uz = d[2] / len;
    h = sqrt(ux * ux + uz * uz);
    row[0][0] = hz;       row[0][1] = 0.0; row[0][2] = -hx;
    row[1][0] = -hx * vy; row[1][1] = h;   row[1][2] = -vy * hz;
    row[2][0] = h * hx;   row[2][1] = vy;  row[2][2] = h * hz;
    for (j = 0; j < 3; j++)
        rel[j] = (double)point[j] - eye[j];
    for (i = 0; i < 3; i++)
        view[i] = row[i][0] * rel[0] + row[i][1] * rel[1] + row[i][2] * rel[2];
    if (!(view[2] > RENDER_MIN_DEPTH))
        return 0;
    sx = (double)offset_x + (double)focal * view[0] / view[2];
    sy = (double)offset_y + (double)focal * view[1] / view[2];
    if (!isfinite(sx) || !isfinite(sy))
        return 0;
    sx = trunc(sx);
    sy = trunc(sy);
    xi = (int)fmax(-1024.0, fmin(1023.0, sx));
    yi = (int)fmax(-1024.0, fmin(1023.0, sy));
    *sxy = ((uint32_t)xi & 0xFFFFu) | (((uint32_t)yi & 0xFFFFu) << 16);
    return 1;
}
