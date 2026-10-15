/* Bounded portable subset of CameraScriptRun (0x8004C318); see native_camera_script.md.
 * Only the LOCAL opcodes 1, 2, 3, 4, 20, 23 and 25 are implemented. Anything else
 * fails transactionally. No dependencies beyond native_camera.h. */
#ifndef AE_NATIVE_CAMERA_SCRIPT_H
#define AE_NATIVE_CAMERA_SCRIPT_H

#include <stddef.h>
#include <stdint.h>

#include "native_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AeCameraScriptResult {
    uint32_t remove_slot; /* UINT32_MAX = none, else the slot byte at slot+2 (opcode 2) */
} AeCameraScriptResult;

/* Runs one CameraScriptRun call on slot (script address at +0x84, timer at +0x88).
 * controls is the 0x50-byte control block based at 0x800E68D0 (written: +0x34,
 * +0x36, +0x3F, +0x40). data[0..size) is the script blob mapped at original_base.
 * Returns 1 on success (result->remove_slot initialised) and 0 on invalid
 * input, unsupported opcode, malformed or out-of-bounds script; on 0 slot,
 * controls and result are all unchanged. If the script address is 0 or the signed
 * timer is nonzero the call succeeds without changes (data is not examined). */
int ae_camera_script_run(AeCameraSlot *slot, uint8_t controls[0x50],
                         const uint8_t *data, size_t size, uint32_t original_base,
                         AeCameraScriptResult *result);

#ifdef __cplusplus
}
#endif

#endif
