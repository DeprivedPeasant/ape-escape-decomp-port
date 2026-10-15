#ifndef AE_OVL186D_H
#define AE_OVL186D_H

#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Member 186's final code section is the MCX serial/accessory driver.  These
 * are its public entry points; the native card backend owns their protocol,
 * SIO, and interrupt work. */
#define AE_OVL186D_FN_MCX_SYNC       0x8014236Cu
#define AE_OVL186D_FN_MCX_GET_APL    0x8014275Cu
#define AE_OVL186D_FN_MCX_EXEC_APL   0x801428E0u
#define AE_OVL186D_FN_MCX_GET_TIME   0x80142A90u
#define AE_OVL186D_FN_MCX_GET_MEM    0x80142BE4u
#define AE_OVL186D_FN_MCX_SET_MEM    0x80142E7Cu
#define AE_OVL186D_FN_MCX_READ_DEV   0x8014311Cu
#define AE_OVL186D_FN_MCX_WRITE_DEV  0x801434CCu
#define AE_OVL186D_FN_MCX_SET_TIME   0x80143884u
#define AE_OVL186D_FN_MCX_GET_UIFS   0x801439A4u
#define AE_OVL186D_FN_MCX_SET_UIFS   0x80143A64u
#define AE_OVL186D_FN_MCX_SHOW_TRANS 0x80143F54u
#define AE_OVL186D_FN_MCX_HIDE_TRANS 0x80144084u
#define AE_OVL186D_FN_MCX_CURR_CTRL  0x80144128u
#define AE_OVL186D_FN_MCX_FLASH_ACS  0x80144264u
#define AE_OVL186D_FN_MCX_GET_SERIAL 0x80144344u
#define AE_OVL186D_FN_MCX_SET_LED    0x80144400u
#define AE_OVL186D_FN_MCX_CARD_TYPE  0x801444D4u
#define AE_OVL186D_FN_MCX_GET_INFO   0x80144DF4u
#define AE_OVL186D_FN_MCX_GET_MCX_INFO 0x80144500u
#define AE_OVL186D_FN_MCX_ALL_INFO   0x8014464Cu
#define AE_OVL186D_FN_MCX_EXEC_FLAG  0x80144850u

#define AE_OVL186D_MAX_NATIVE_ARGS 7u

typedef struct AeOvl186dNativeApi {
    const char *name;
    uint32_t address;
} AeOvl186dNativeApi;

/* `arguments` contains the original o32 argument words.  The platform hook
 * may update AeRam through `ram`, as the card APIs write result buffers. */
typedef int32_t (*AeOvl186dNativeCallFn)(void *user, AeRam *ram,
                                         const char *name, uint32_t address,
                                         uint32_t argc,
                                         const uint32_t *arguments);

typedef struct AeOvl186dHooks {
    void *user;
    AeOvl186dNativeCallFn native_call;
} AeOvl186dHooks;

/* Returns NULL for addresses that are not public MCX APIs in this section. */
const AeOvl186dNativeApi *ovl186d_native_api(uint32_t address);

/* Dispatches one public MCX API to the platform.  An unknown address, invalid
 * argument vector, or missing native backend sets AeRam's sticky error flag. */
int32_t ovl186d_call_native(AeRam *ram, const AeOvl186dHooks *hooks,
                            uint32_t address, uint32_t argc,
                            const uint32_t *arguments);

#ifdef __cplusplus
}
#endif

#endif
