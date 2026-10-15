/* Native boundary for the MCX accessory driver in member 186. */
#include "ae_ovl186d.h"

static const AeOvl186dNativeApi NATIVE_APIS[] = {
    {"McxSync", AE_OVL186D_FN_MCX_SYNC},
    {"McxGetApl", AE_OVL186D_FN_MCX_GET_APL},
    {"McxExecApl", AE_OVL186D_FN_MCX_EXEC_APL},
    {"McxGetTime", AE_OVL186D_FN_MCX_GET_TIME},
    {"McxGetMem", AE_OVL186D_FN_MCX_GET_MEM},
    {"McxSetMem", AE_OVL186D_FN_MCX_SET_MEM},
    {"McxReadDev", AE_OVL186D_FN_MCX_READ_DEV},
    {"McxWriteDev", AE_OVL186D_FN_MCX_WRITE_DEV},
    {"McxSetTime", AE_OVL186D_FN_MCX_SET_TIME},
    {"McxGetUIFS", AE_OVL186D_FN_MCX_GET_UIFS},
    {"McxSetUIFS", AE_OVL186D_FN_MCX_SET_UIFS},
    {"McxShowTrans", AE_OVL186D_FN_MCX_SHOW_TRANS},
    {"McxHideTrans", AE_OVL186D_FN_MCX_HIDE_TRANS},
    {"McxCurrCtrl", AE_OVL186D_FN_MCX_CURR_CTRL},
    {"McxFlashAcs", AE_OVL186D_FN_MCX_FLASH_ACS},
    {"McxGetSerial", AE_OVL186D_FN_MCX_GET_SERIAL},
    {"McxSetLED", AE_OVL186D_FN_MCX_SET_LED},
    {"McxCardType", AE_OVL186D_FN_MCX_CARD_TYPE},
    {"McxGetInfo", AE_OVL186D_FN_MCX_GET_INFO},
    {"McxGetMcxInfo", AE_OVL186D_FN_MCX_GET_MCX_INFO},
    {"McxAllInfo", AE_OVL186D_FN_MCX_ALL_INFO},
    {"McxExecFlag", AE_OVL186D_FN_MCX_EXEC_FLAG},
};

const AeOvl186dNativeApi *ovl186d_native_api(uint32_t address)
{
    for (unsigned i = 0; i < sizeof(NATIVE_APIS) / sizeof(NATIVE_APIS[0]); ++i) {
        if (NATIVE_APIS[i].address == address) {
            return &NATIVE_APIS[i];
        }
    }
    return 0;
}

int32_t ovl186d_call_native(AeRam *ram, const AeOvl186dHooks *hooks,
                            uint32_t address, uint32_t argc,
                            const uint32_t *arguments)
{
    const AeOvl186dNativeApi *api = ovl186d_native_api(address);
    if (api == 0) {
        ae_ram_fail(ram, "unknown member 186 native API at %08X", address);
        return 0;
    }
    if (argc > AE_OVL186D_MAX_NATIVE_ARGS ||
        (argc != 0u && arguments == 0)) {
        ae_ram_fail(ram, "%s: invalid native argument vector (%u words)",
                    api->name, argc);
        return 0;
    }
    if (hooks == 0 || hooks->native_call == 0) {
        ae_ram_fail(ram, "%s native hook not provided", api->name);
        return 0;
    }
    return hooks->native_call(hooks->user, ram, api->name, api->address,
                              argc, arguments);
}
