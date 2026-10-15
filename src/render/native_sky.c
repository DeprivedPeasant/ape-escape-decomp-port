#include "native_sky.h"

#include <stddef.h>

/* Store a 32-bit value as a signed 16-bit one with wraparound, using only
 * defined conversions (int32 -> uint32 is modular; the final value is in range). */
static int16_t wrap16(int32_t v)
{
    uint32_t u = (uint32_t)v & 0xFFFFu;
    return (int16_t)(u >= 0x8000u ? (int32_t)u - 0x10000 : (int32_t)u);
}

int ae_sky_policy(int type, AeSkyPolicy *policy)
{
    if ((type != AE_SKY_TYPE_FOLLOW_PLAYER_Y && type != AE_SKY_TYPE_FIXED_Y) || policy == NULL)
        return 0;
    policy->uses_placement = 0;
    policy->zero_camera_translation = 1;
    policy->fog = 0;
    policy->background_bucket = 1;
    return 1;
}

int ae_sky_quarter_turn(uint16_t rotation)
{
    return (int)((rotation >> 10) & 3u);
}

int ae_sky_vertex(int type, uint16_t rotation, int16_t player_y, const int16_t in[3], int16_t out[3])
{
    int32_t x, y, z, rx, rz, ry;

    if ((type != AE_SKY_TYPE_FOLLOW_PLAYER_Y && type != AE_SKY_TYPE_FIXED_Y) || in == NULL || out == NULL)
        return 0;
    x = in[0];
    y = in[1];
    z = in[2];
    switch (ae_sky_quarter_turn(rotation)) {
    case 0: rx = x;  rz = z;  break;
    case 1: rx = z;  rz = -x; break;
    case 2: rx = -x; rz = -z; break;
    default: rx = -z; rz = x; break;
    }
    /* Type 4 offset is the 16-bit store of -playerY (wraps for -32768); the
     * sum is stored as 16 bits again. Type 5 offset is 0. */
    ry = (type == AE_SKY_TYPE_FOLLOW_PLAYER_Y) ? y + (int32_t)wrap16(-(int32_t)player_y) : y;
    out[0] = wrap16(rx);
    out[1] = wrap16(ry);
    out[2] = wrap16(rz);
    return 1;
}
