/* Area CAM region loader and CameraRegionTest (see native_camera_regions.md). */
#include "native_camera_regions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AREA_CAM_OFFSET_FIELD 0x4D8u
#define AREA_MIN_SIZE 0x4DCu
#define CAM_HEADER_SIZE 16u
#define CAM_NAME_TABLE_HEADER 8u
#define CAM_NAME_ENTRY_SIZE 0x28u
#define CAM_NAME_ID_OFFSET 8u
#define CAM_ID_BYTES 32u
#define REGION_ID_OFFSET 0x10u

static char g_error[128];

static void set_error(const char *msg)
{
    snprintf(g_error, sizeof g_error, "%s", msg);
}

static void set_error_index(const char *msg, unsigned index)
{
    snprintf(g_error, sizeof g_error, "%s (region %u)", msg, index);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((unsigned)p[0] | ((unsigned)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

void ae_camera_regions_free(AeCameraRegions *r)
{
    if (!r)
        return;
    free(r->records);
    r->records = NULL;
    r->count = 0;
    r->named_count = 0;
    r->cam_offset = 0;
}

const char *ae_camera_regions_error(void)
{
    return g_error;
}

int ae_camera_regions_load(AeCameraRegions *r, const uint8_t *area_blob,
                           size_t size)
{
    uint32_t off32;
    size_t off, count, rec_bytes, tbl, names = 0, i, j;
    uint8_t *rec;

    if (!r || !area_blob) {
        set_error("null argument");
        return 0;
    }
    if (size < AREA_MIN_SIZE) {
        set_error("area blob smaller than 0x4DC bytes");
        return 0;
    }
    off32 = rd32(area_blob + AREA_CAM_OFFSET_FIELD);
    if (off32 == 0) {
        ae_camera_regions_free(r);
        g_error[0] = '\0';
        return 1;
    }
    off = (size_t)off32;
    if (off32 != (uint32_t)off || off > size || size - off < CAM_HEADER_SIZE) {
        set_error("CAM offset leaves no room for the 16 byte header");
        return 0;
    }
    if (area_blob[off] != 'C' || area_blob[off + 1] != 'A' ||
        area_blob[off + 2] != 'M') {
        set_error("CAM magic missing");
        return 0;
    }
    count = rd16(area_blob + off + 4);
    if (count != 0) {
        size_t avail = size - off - CAM_HEADER_SIZE;
        if (count > avail / AE_CAMERA_REGION_STRIDE) {
            set_error("CAM region records extend past the blob");
            return 0;
        }
        tbl = (size_t)rd16(area_blob + off + 0xE);
        if (tbl > size - off || size - off - tbl < CAM_NAME_TABLE_HEADER) {
            set_error("CAM name table header outside the blob");
            return 0;
        }
        tbl += off;
        names = rd16(area_blob + tbl);
        if (names > (size - tbl - CAM_NAME_TABLE_HEADER) / CAM_NAME_ENTRY_SIZE) {
            set_error("CAM name table entries extend past the blob");
            return 0;
        }
    } else {
        tbl = 0;
    }

    rec_bytes = count * AE_CAMERA_REGION_STRIDE;
    rec = NULL;
    if (count != 0) {
        const uint8_t *src = area_blob + off + CAM_HEADER_SIZE;
        const uint8_t *entries = area_blob + tbl + CAM_NAME_TABLE_HEADER;

        rec = (uint8_t *)malloc(rec_bytes);
        if (!rec) {
            set_error("out of memory");
            return 0;
        }
        memcpy(rec, src, rec_bytes);
        if (names != 0) {
            for (i = 0; i < count; i++) {
                uint8_t *rr = rec + i * AE_CAMERA_REGION_STRIDE;
                for (j = 0; j < names; j++) {
                    const uint8_t *e = entries + j * CAM_NAME_ENTRY_SIZE;
                    if (memcmp(rr + REGION_ID_OFFSET, e + CAM_NAME_ID_OFFSET,
                               CAM_ID_BYTES) == 0) {
                        wr16(rr + 0, (uint16_t)j);
                        wr16(rr + 2, rd16(e));
                        break;
                    }
                }
                if (j == names) {
                    free(rec);
                    set_error_index("camera name not found in CAM name table",
                                    (unsigned)i);
                    return 0;
                }
            }
        }
    }

    ae_camera_regions_free(r);
    r->records = rec;
    r->count = (uint16_t)count;
    r->named_count = (uint16_t)names;
    r->cam_offset = off32;
    g_error[0] = '\0';
    return 1;
}

/* Center +/- half wrapped to 16 bits, then sign extended. */
static int32_t wrap16(uint32_t v)
{
    uint32_t lo = v & 0xFFFFu;
    return lo >= 0x8000u ? (int32_t)lo - 65536 : (int32_t)lo;
}

/* Strict inside test on one axis. half_raw is the stored u16 width. */
static int axis_inside(uint16_t center, uint16_t half_raw, int16_t p)
{
    int32_t w = half_raw >= 0x8000u ? (int32_t)half_raw - 65536
                                    : (int32_t)half_raw;
    int32_t half = w / 2; /* truncates toward zero, like the sra + sign fixup */
    int32_t lo = wrap16((uint32_t)center - (uint32_t)half);
    int32_t hi = wrap16((uint32_t)center + (uint32_t)half);
    return lo < (int32_t)p && (int32_t)p < hi;
}

int32_t ae_camera_region_test(const uint8_t *records, size_t bytes,
                              uint16_t count, const int16_t point[3])
{
    uint16_t i;

    if (!point)
        return -2;
    if (count == 0)
        return -1;
    if (!records || count > bytes / AE_CAMERA_REGION_STRIDE)
        return -2;

    for (i = 0; i < count; i++) {
        const uint8_t *r = records + (size_t)i * AE_CAMERA_REGION_STRIDE;
        uint16_t flags = rd16(r + 2);

        if ((flags & 0x6000u) == 0x6000u || (flags & 0x8300u) == 0x8100u)
            continue;
        /* X (+4 / +10), then Z (+8 / +14), then Y (+6 / +12). */
        if (axis_inside(rd16(r + 4), rd16(r + 10), point[0]) &&
            axis_inside(rd16(r + 8), rd16(r + 14), point[2]) &&
            axis_inside(rd16(r + 6), rd16(r + 12), point[1]))
            return (int32_t)i;
    }
    return -1;
}
