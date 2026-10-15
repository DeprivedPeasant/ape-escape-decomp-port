/* Bounded portable subset of CameraScriptRun. See native_camera_script.h / .md. */
#include "native_camera_script.h"

#include <string.h>

#define MAX_COMMANDS 1024u
#define ADDR_LIMIT   0x100000000ull

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static int16_t s16_of(uint16_t v)
{
    return v >= 0x8000u ? (int16_t)((int32_t)v - 0x10000) : (int16_t)v;
}

/* Operand count (halfwords after the opcode) of the supported opcodes, -1 otherwise. */
static int operand_words(uint16_t op)
{
    switch (op) {
    case 1: case 3: case 4: return 1;
    case 2: return 0;
    case 20: case 25: return 7;
    case 23: return 2;
    default: return -1;
    }
}

int ae_camera_script_run(AeCameraSlot *slot, uint8_t controls[0x50],
                         const uint8_t *data, size_t size, uint32_t original_base,
                         AeCameraScriptResult *result)
{
    AeCameraSlot tmp;
    uint8_t ctl[0x50];
    uint32_t remove = UINT32_MAX;
    uint32_t pc, next = 0;
    uint64_t end;
    unsigned count;
    int yielded = 0;

    if (!slot || !controls || !result)
        return 0;

    pc = (uint32_t)ae_camera_i32(slot, AE_CAM_SCRIPT);
    if (pc == 0 || ae_camera_i16(slot, AE_CAM_SCRIPT_TIMER) != 0) {
        result->remove_slot = UINT32_MAX;
        return 1;
    }

    if (!data || (original_base & 1u) || (uint64_t)original_base + size > ADDR_LIMIT)
        return 0;
    end = (uint64_t)original_base + size;

    tmp = *slot;
    memcpy(ctl, controls, sizeof ctl);

    for (count = 0; count < MAX_COMMANDS && !yielded; count++) {
        const uint8_t *p;
        uint16_t op;
        int words;
        uint64_t after;

        if ((pc & 1u) || pc < original_base || (uint64_t)pc + 2 > end)
            return 0;
        p = data + (pc - original_base);
        op = rd16(p);
        words = operand_words(op);
        if (words < 0)
            return 0;
        after = (uint64_t)pc + 2 + 2u * (unsigned)words;
        if (after > end || after >= ADDR_LIMIT)
            return 0;
        next = (uint32_t)after;

        switch (op) {
        case 1: { /* relative jump from the operand address */
            uint32_t target = (uint32_t)(pc + 2u) + (uint32_t)(int32_t)s16_of(rd16(p + 2));
            if ((target & 1u) || target < original_base || (uint64_t)target + 2 > end)
                return 0;
            next = target;
            yielded = 1;
            break;
        }
        case 2:
            remove = tmp.raw[2];
            yielded = 1;
            break;
        case 3:
            ctl[0x34] = p[2]; ctl[0x35] = p[3];
            ctl[0x3F] = 1;
            break;
        case 4:
            ctl[0x36] = p[2]; ctl[0x37] = p[3];
            ctl[0x40] = 1;
            break;
        case 20:
        case 25:
            ae_camera_set16(&tmp, AE_CAM_SCRIPT_TIMER, rd16(p + 2));
            ae_camera_set16(&tmp, AE_CAM_SCRIPT_STEP, rd16(p + 2));
            tmp.raw[AE_CAM_SCRIPT_STATE] = (uint8_t)(op == 20 ? 8 : 9);
            ae_camera_set16(&tmp, AE_CAM_EYE, rd16(p + 4));
            ae_camera_set16(&tmp, AE_CAM_EYE + 2, rd16(p + 6));
            ae_camera_set16(&tmp, AE_CAM_EYE + 4, rd16(p + 8));
            ae_camera_set16(&tmp, 0x94, rd16(p + 10));
            ae_camera_set16(&tmp, 0x96, rd16(p + 12));
            ae_camera_set16(&tmp, 0x98, rd16(p + 14));
            memcpy(&tmp.raw[0xA8], &tmp.raw[AE_CAM_EYE], 8);
            yielded = 1;
            break;
        case 23:
            ae_camera_set16(&tmp, AE_CAM_SCRIPT_TIMER, rd16(p + 2));
            ae_camera_set16(&tmp, AE_CAM_SCRIPT_STEP, rd16(p + 2));
            tmp.raw[AE_CAM_SCRIPT_ACTIVE] = p[4];
            yielded = 1;
            break;
        default:
            return 0;
        }
        pc = next;
    }
    if (!yielded)
        return 0;

    ae_camera_set32(&tmp, AE_CAM_SCRIPT, (int32_t)(next >= 0x80000000u
                    ? (int32_t)(next - 0x80000000u) - 0x7FFFFFFF - 1 : (int32_t)next));
    *slot = tmp;
    memcpy(controls, ctl, sizeof ctl);
    result->remove_slot = remove;
    return 1;
}
