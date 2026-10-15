/* Save-menu unit dispatcher; public entries also serve the central ABI proof. */
#include "../../src/modes/ae_ovl186b.h"

int32_t o186_call(AeRam *r, const AeOvl186bHooks *h, uint32_t address,
                  uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a1; (void)a2; (void)a3;
    switch (address) {
    case 0x801391B0u: ae_SaveUi_DrawDetail(r,h); return 0;
    case 0x801399E4u: return ae_SaveUi_LastUsedEntry(r,(int32_t)a0);
    case 0x80139A3Cu: return ae_SaveUi_DrawMainWindow(r,h);
    case 0x80139A68u: ae_SaveUi_DrawBackground3D(r,h); return 0;
    case 0x80139F08u: return ae_SaveUi_YesNoUpdate(r,h);
    case 0x80139FC8u: return ae_SaveUi_FileSelectUpdate(r,h);
    case 0x8013A400u: return ae_SaveUi_ConfirmPressed(r);
    case 0x8013A43Cu: return ae_SaveUi_CancelPressed(r);
    case 0x8013A634u: return ae_SaveUi_FadeTick(r,h);
    case 0x8013B400u: return ae_SaveUi_OnCardResult_7(r,h);
    case 0x8013B47Cu: return ae_SaveUi_ConfirmDialogUpdate(r,h);
    case 0x8013B550u: return ae_SaveUi_NameEntryInit(r,h);
    case 0x8013B6C4u: return ae_SaveUi_NameEntryUpdate(r,h);
    case 0x8013BC64u: ae_SaveUi_NameEntryDraw(r,h); return 0;
    case 0x8013C7F0u: return ae_CardMgr_Run(r,h);
    case 0x8013C8E8u: return ae_CardMgr_Init(r,h);
    case 0x8013C954u: return ae_CardMgr_RunFileList(r,h);
    case 0x8013CB30u: return ae_CardMgr_RunFileDetail(r,h);
    default: ae_ram_fail(r,"unknown 186b test entry %08X",address); return 0;
    }
}
