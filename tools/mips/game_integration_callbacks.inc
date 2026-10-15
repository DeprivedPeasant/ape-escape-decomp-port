/* Generated test callbacks: preserve host argument words. */
static int32_t test_platform_CdControl(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "CdControl", 0x8002408Cu, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_CdIntToPos(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "CdIntToPos", 0x80025D20u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_CdPlayXa(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "CdPlayXa", 0x800242FCu, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_CdPosToInt(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "CdPosToInt", 0x80025E24u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_CdReady(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "CdReady", 0x8002408Cu, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_CdReadyCallback(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "CdReadyCallback", 0x80024078u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_CdSync(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "CdSync", 0x80026A50u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_ClearImage(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ClearImage", 0x80028850u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_platform_ClearImage2(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ClearImage2", 0x800288E0u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_platform_ClearOTagR(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "ClearOTagR", 0x80028BB8u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_DR_MOVE(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DR_MOVE", 0x8002B380u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_platform_DrawOTag(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "DrawOTag", 0x80028CC0u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_DrawPrim(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "DrawPrim", 0x80028C64u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_DrawSync(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "DrawSync", 0x800286CCu, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_Exit(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "Exit", 0x800A3700u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_GetVideoMode(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "GetVideoMode", 0x8002EE04u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_InitDisplay(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "InitDisplay", 0x8005CFECu, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_InitSystem(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "InitSystem", 0x8005CD50u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_LoadImage(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "LoadImage", 0x80028978u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_LoadTimList(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "LoadTimList", 0x80068D10u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_MatrixNormal(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "MatrixNormal", 0x800302D0u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_MemCardOp(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "MemCardOp", 0x80010000u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_MoveImage(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "MoveImage", 0x80028A38u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_MulMatrix(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "MulMatrix", 0x80031470u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_MulMatrix0(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "MulMatrix0", 0x80031830u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_PadGetState(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "PadGetState", 0x8002B52Cu, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_PadInfoMode(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "PadInfoMode", 0x8002B5F8u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_Printf(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "Printf", 0x800A3720u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_platform_PutDispEnv(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "PutDispEnv", 0x80028EFCu, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_PutDrawEnv(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "PutDrawEnv", 0x80028D30u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_ReadGeomScreen(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "ReadGeomScreen", 0x800315D0u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_ResetGraph(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "ResetGraph", 0x80028350u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_RotTrans(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "RotTrans", 0x80031940u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_RotTransPers(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RotTransPers", 0x80031330u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_platform_RotTransSV(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "RotTransSV", 0x80031230u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_RunMinigameExe(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "RunMinigameExe", 0x80011A2Cu, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_ScaleMatrixL(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "ScaleMatrixL", 0x800305B0u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_SeqPlayer_SetupDisplay(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "SeqPlayer_SetupDisplay", 0x80136BD4u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SetBackColor(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "SetBackColor", 0x800315E0u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_SetColorMatrix(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetColorMatrix", 0x80031600u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SetDefDispEnv(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "SetDefDispEnv", 0x8002B340u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_platform_SetDefDrawEnv(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "SetDefDrawEnv", 0x8002B280u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_platform_SetDispMask(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetDispMask", 0x80028634u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SetDrawStp(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "SetDrawStp", 0x8002B3E0u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_SetDrawTPage(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "SetDrawTPage", 0x8002B410u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_platform_SetFarColor(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "SetFarColor", 0x80031630u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_SetGeomOffset(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "SetGeomOffset", 0x8002FAD0u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_SetGeomScreen(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetGeomScreen", 0x8002FAF0u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SetGraphDebug(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetGraphDebug", 0x800284C4u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SetRotMatrix(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetRotMatrix", 0x8002FB00u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SetTransMatrix(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetTransMatrix", 0x8002FB30u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SetVideoMode(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetVideoMode", 0x8002EDF0u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SoundRestoreVolume(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "SoundRestoreVolume", 0x80086170u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SoundStopAndRestoreVolume(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SoundStopAndRestoreVolume", 0x80086248u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SpuWaitIdle(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "SpuWaitIdle", 0x800A1398u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_StFree(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "StFree", 0x80025EB0u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_StRingStatus(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "StRingStatus", 0x80025F60u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_StSetRing(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "StSetRing", 0x80025C60u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_StSetStream(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "StSetStream", 0x80025C90u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_platform_StopCallback(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "StopCallback", 0x8002E824u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_StoreImage(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "StoreImage", 0x800289D8u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_TextLineWidth(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "TextLineWidth", 0x8008DE1Cu, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_VSync(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "VSync", 0x8002E520u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_VSyncCallback(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "VSyncCallback", 0x8002E7C0u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_memcpy(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "memcpy", 0x800A3760u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_memset(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "memset", 0x800A3740u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_srand(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "srand", 0x800A3750u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_renderer_AlertIconDraw(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "AlertIconDraw", 0x8006ABF0u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_CameraLookAt(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "CameraLookAt", 0x80046670u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_DrawDebugGlyph(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) {
    (void)r;
    return trace_call(user, "DrawDebugGlyph", 0x00000000u, 6, a0, a1, a2, a3, a4, a5);
}
static int32_t test_renderer_DrawMenuChrome(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawMenuChrome", 0x8007D71Cu, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_DrawMenuCursor(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawMenuCursor", 0x8007F148u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_DrawMenuHighlight(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawMenuHighlight", 0x8007E030u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_DrawMenuItem(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawMenuItem", 0x8007E1B8u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_DrawMenuRows(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawMenuRows", 0x8007D9F0u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_DrawMenuSelection(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawMenuSelection", 0x8007E3F8u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_DrawParticleQuad(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "DrawParticleQuad", 0x00000000u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_DrawProjectileSprite(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) {
    (void)r;
    return trace_call(user, "DrawProjectileSprite", 0x00000000u, 6, a0, a1, a2, a3, a4, a5);
}
static int32_t test_renderer_DrawRibbonBetween(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "DrawRibbonBetween", 0x8002020Cu, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_DrawRibbonStrip(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "DrawRibbonStrip", 0x00000000u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_DrawShadow(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawShadow", 0x8001F604u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_Draw_Nop(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "Draw_Nop", 0x8005E000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_EffectRingDraw(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "EffectRingDraw", 0x8006E5B4u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_FinishDebugText(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) {
    (void)r;
    return trace_call(user, "FinishDebugText", 0x00000000u, 6, a0, a1, a2, a3, a4, a5);
}
static int32_t test_renderer_GpuDrawEffectObj(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "GpuDrawEffectObj", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_GpuDrawFxPlayerState6(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "GpuDrawFxPlayerState6", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_GpuDrawGadget3AimLine(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "GpuDrawGadget3AimLine", 0x00000000u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_GpuDrawGadget4Trail(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "GpuDrawGadget4Trail", 0x00000000u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_GpuDrawGadget7Trail(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "GpuDrawGadget7Trail", 0x00000000u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_GpuDrawGroundSpark(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "GpuDrawGroundSpark", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_GpuDrawMesh(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "GpuDrawMesh", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_GpuDrawSprite(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "GpuDrawSprite", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_GpuDrawTrailPoint(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "GpuDrawTrailPoint", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_GpuProjectGadget3Aim(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "GpuProjectGadget3Aim", 0x00000000u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_GpuProjectMesh(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "GpuProjectMesh", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_HudDrawFrameBox(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "HudDrawFrameBox", 0x80060334u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_HudDrawStatPips(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "HudDrawStatPips", 0x80060224u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_HudDrawTimeRankIcon(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "HudDrawTimeRankIcon", 0x8005F70Cu, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ItemDrawModel(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ItemDrawModel", 0x800845E0u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_MeshInit(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "MeshInit", 0x80015C34u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_MeshInitFromAreaTable(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "MeshInitFromAreaTable", 0x80015CC8u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_ModelCalcEnvUVs(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelCalcEnvUVs", 0x80016B94u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ModelDraw(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "ModelDraw", 0x80017CD4u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_ModelDrawEnvMap(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelDrawEnvMap", 0x8001CE54u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ModelDrawScreenSpace(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelDrawScreenSpace", 0x800207B4u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ModelEmitPrims(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelEmitPrims", 0x800180B8u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ModelEmitPrimsCycleColors(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelEmitPrimsCycleColors", 0x8001D530u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ModelEmitPrimsScratchColors(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelEmitPrimsScratchColors", 0x8001C918u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ModelFillVertexColors(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "ModelFillVertexColors", 0x80020A20u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_ModelProjectVertices(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelProjectVertices", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ModelTransformRigid(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "ModelTransformRigid", 0x800161C0u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_ModelTransformSkinned(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelTransformSkinned", 0x80016A60u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ProjectEffectPoint(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "ProjectEffectPoint", 0x00000000u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_PropSetMesh(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "PropSetMesh", 0x80090E54u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RenderCameraTransition(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RenderCameraTransition", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RenderEnvUV(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RenderEnvUV", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RenderFadeSpiral(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RenderFadeSpiral", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RenderHudPopup(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RenderHudPopup", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RenderIndicator(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RenderIndicator", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RenderScreenFlash(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RenderScreenFlash", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RenderTrailRingSegment(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "RenderTrailRingSegment", 0x00000000u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_RendererGetProjectionDistance(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererGetProjectionDistance", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RendererHudDrawWindowFrame(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererHudDrawWindowFrame", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RendererHudMonkeyBadge(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererHudMonkeyBadge", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RendererIrgbToIr123(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererIrgbToIr123", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RendererPackIrgb(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererPackIrgb", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RendererPauseMenuDrawButtons(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererPauseMenuDrawButtons", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RendererPauseMenuDrawGadgetScreen(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererPauseMenuDrawGadgetScreen", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RendererPrimitiveBlock(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererPrimitiveBlock", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RendererSetProjectionDistance(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererSetProjectionDistance", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RendererWobbleGte(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererWobbleGte", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ScreenBandDraw(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ScreenBandDraw", 0x8009B9C8u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_VertexWobble(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "VertexWobble", 0x8001F434u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ZakoDebugDrawPrimitive(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ZakoDebugDrawPrimitive", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_Ovl185_DrawText(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "Ovl185_DrawText", 0x8014666Cu, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_SaveUi_DrawPortrait(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "SaveUi_DrawPortrait", 0x80139570u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_renderer_SaveUi_DrawCursorAnim(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "SaveUi_DrawCursorAnim", 0x80139CF0u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_renderer_SaveUi_DrawFileIcons(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "SaveUi_DrawFileIcons", 0x8013A478u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_renderer_SaveUi_FadeDraw(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "SaveUi_FadeDraw", 0x8013A70Cu, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_renderer_SaveUi_DrawModel(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "SaveUi_DrawModel", 0x8013A804u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_SaveUi_DrawIcon(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "SaveUi_DrawIcon", 0x8013946Cu, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_renderer_SeqPlayer_CopyScreenToBack(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "SeqPlayer_CopyScreenToBack", 0x801367DCu, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_renderer_SeqPlayer_Present(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SeqPlayer_Present", 0x80136FF0u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_renderer_SeqPlayer_LoadSavedImage(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "SeqPlayer_LoadSavedImage", 0x8013AA70u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_renderer_SeqPlayer_SaveRestoreVram(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SeqPlayer_SaveRestoreVram", 0x8013BA80u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_renderer_DebugTextSetCursor(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "DebugTextSetCursor", 0x8008CC60u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_renderer_DrawMenuArrows(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "DrawMenuArrows", 0x8008211Cu, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_renderer_ModelEmitPrimsAlt(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "ModelEmitPrimsAlt", 0x8001A414u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_renderer_MatrixBuild(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "MatrixBuild", 0x80030450u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_renderer_ModelTransform(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "ModelTransform", 0x800160ECu, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_renderer_ModelDrawTransformed(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelDrawTransformed", 0x80017EFCu, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_archive_ArcGetMemberPos(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "ArcGetMemberPos", 0x80012C0Cu, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_archive_CdMgrQueueCommand(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "CdMgrQueueCommand", 0x80012360u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_archive_CdMgrUpdate(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "CdMgrUpdate", 0x800125B8u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_archive_LoadListStep(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "LoadListStep", 0x800116C0u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_archive_FUN_80025E24(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "FUN_80025E24", 0x80025E24u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_archive_ArcLoadMember(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "ArcLoadMember", 0x800112E4u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_archive_CdMgrWaitIdle(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "CdMgrWaitIdle", 0x8001256Cu, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_archive_LoadListInit(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "LoadListInit", 0x80011104u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_archive_ArcLoadEntry(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "ArcLoadEntry", 0x80011194u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_archive_CdMgrStopStream(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "CdMgrStopStream", 0x8001231Cu, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_archive_Ovl181_CdCommand(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "Ovl181_CdCommand", 0x80136AECu, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_archive_SeqPlayer_CdReadSeek(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "SeqPlayer_CdReadSeek", 0x80136454u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_archive_SeqPlayer_CdPlayXaRange(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "SeqPlayer_CdPlayXaRange", 0x801364B0u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_archive_SeqPlayer_LoadFiles(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SeqPlayer_LoadFiles", 0x80136884u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_archive_SeqPlayer_CdSetFilter(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SeqPlayer_CdSetFilter", 0x80136E68u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_archive_SeqPlayer_StGetNextWait(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "SeqPlayer_StGetNextWait", 0x80136E00u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_frame_BeginFrame(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "BeginFrame", 0x8005DF48u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_DrawTerrain(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawTerrain", 0x80043A8Cu, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_DrawAreaProps(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawAreaProps", 0x80016F34u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_QueueFrameFlip(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "QueueFrameFlip", 0x8005DEE0u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_WaitFrameFlip(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "WaitFrameFlip", 0x8005DE8Cu, 4, a0, a1, a2, a3, 0, 0);
}
static void integration_callbacks(AeGameHost *host) {
    host->platform.CdControl = test_platform_CdControl;
    host->platform.CdIntToPos = test_platform_CdIntToPos;
    host->platform.CdPlayXa = test_platform_CdPlayXa;
    host->platform.CdPosToInt = test_platform_CdPosToInt;
    host->platform.CdReady = test_platform_CdReady;
    host->platform.CdReadyCallback = test_platform_CdReadyCallback;
    host->platform.CdSync = test_platform_CdSync;
    host->platform.ClearImage = test_platform_ClearImage;
    host->platform.ClearImage2 = test_platform_ClearImage2;
    host->platform.ClearOTagR = test_platform_ClearOTagR;
    host->platform.DR_MOVE = test_platform_DR_MOVE;
    host->platform.DrawOTag = test_platform_DrawOTag;
    host->platform.DrawPrim = test_platform_DrawPrim;
    host->platform.DrawSync = test_platform_DrawSync;
    host->platform.Exit = test_platform_Exit;
    host->platform.GetVideoMode = test_platform_GetVideoMode;
    host->platform.InitDisplay = test_platform_InitDisplay;
    host->platform.InitSystem = test_platform_InitSystem;
    host->platform.LoadImage = test_platform_LoadImage;
    host->platform.LoadTimList = test_platform_LoadTimList;
    host->platform.MatrixNormal = test_platform_MatrixNormal;
    host->platform.MemCardOp = test_platform_MemCardOp;
    host->platform.MoveImage = test_platform_MoveImage;
    host->platform.MulMatrix = test_platform_MulMatrix;
    host->platform.MulMatrix0 = test_platform_MulMatrix0;
    host->platform.PadGetState = test_platform_PadGetState;
    host->platform.PadInfoMode = test_platform_PadInfoMode;
    host->platform.Printf = test_platform_Printf;
    host->platform.PutDispEnv = test_platform_PutDispEnv;
    host->platform.PutDrawEnv = test_platform_PutDrawEnv;
    host->platform.ReadGeomScreen = test_platform_ReadGeomScreen;
    host->platform.ResetGraph = test_platform_ResetGraph;
    host->platform.RotTrans = test_platform_RotTrans;
    host->platform.RotTransPers = test_platform_RotTransPers;
    host->platform.RotTransSV = test_platform_RotTransSV;
    host->platform.RunMinigameExe = test_platform_RunMinigameExe;
    host->platform.ScaleMatrixL = test_platform_ScaleMatrixL;
    host->platform.SeqPlayer_SetupDisplay = test_platform_SeqPlayer_SetupDisplay;
    host->platform.SetBackColor = test_platform_SetBackColor;
    host->platform.SetColorMatrix = test_platform_SetColorMatrix;
    host->platform.SetDefDispEnv = test_platform_SetDefDispEnv;
    host->platform.SetDefDrawEnv = test_platform_SetDefDrawEnv;
    host->platform.SetDispMask = test_platform_SetDispMask;
    host->platform.SetDrawStp = test_platform_SetDrawStp;
    host->platform.SetDrawTPage = test_platform_SetDrawTPage;
    host->platform.SetFarColor = test_platform_SetFarColor;
    host->platform.SetGeomOffset = test_platform_SetGeomOffset;
    host->platform.SetGeomScreen = test_platform_SetGeomScreen;
    host->platform.SetGraphDebug = test_platform_SetGraphDebug;
    host->platform.SetRotMatrix = test_platform_SetRotMatrix;
    host->platform.SetTransMatrix = test_platform_SetTransMatrix;
    host->platform.SetVideoMode = test_platform_SetVideoMode;
    host->platform.SoundRestoreVolume = test_platform_SoundRestoreVolume;
    host->platform.SoundStopAndRestoreVolume = test_platform_SoundStopAndRestoreVolume;
    host->platform.SpuWaitIdle = test_platform_SpuWaitIdle;
    host->platform.StFree = test_platform_StFree;
    host->platform.StRingStatus = test_platform_StRingStatus;
    host->platform.StSetRing = test_platform_StSetRing;
    host->platform.StSetStream = test_platform_StSetStream;
    host->platform.StopCallback = test_platform_StopCallback;
    host->platform.StoreImage = test_platform_StoreImage;
    host->platform.TextLineWidth = test_platform_TextLineWidth;
    host->platform.VSync = test_platform_VSync;
    host->platform.VSyncCallback = test_platform_VSyncCallback;
    host->platform.memcpy = test_platform_memcpy;
    host->platform.memset = test_platform_memset;
    host->platform.srand = test_platform_srand;
    host->renderer.AlertIconDraw = test_renderer_AlertIconDraw;
    host->renderer.CameraLookAt = test_renderer_CameraLookAt;
    host->renderer.DrawDebugGlyph = test_renderer_DrawDebugGlyph;
    host->renderer.DrawMenuChrome = test_renderer_DrawMenuChrome;
    host->renderer.DrawMenuCursor = test_renderer_DrawMenuCursor;
    host->renderer.DrawMenuHighlight = test_renderer_DrawMenuHighlight;
    host->renderer.DrawMenuItem = test_renderer_DrawMenuItem;
    host->renderer.DrawMenuRows = test_renderer_DrawMenuRows;
    host->renderer.DrawMenuSelection = test_renderer_DrawMenuSelection;
    host->renderer.DrawParticleQuad = test_renderer_DrawParticleQuad;
    host->renderer.DrawProjectileSprite = test_renderer_DrawProjectileSprite;
    host->renderer.DrawRibbonBetween = test_renderer_DrawRibbonBetween;
    host->renderer.DrawRibbonStrip = test_renderer_DrawRibbonStrip;
    host->renderer.DrawShadow = test_renderer_DrawShadow;
    host->renderer.Draw_Nop = test_renderer_Draw_Nop;
    host->renderer.EffectRingDraw = test_renderer_EffectRingDraw;
    host->renderer.FinishDebugText = test_renderer_FinishDebugText;
    host->renderer.GpuDrawEffectObj = test_renderer_GpuDrawEffectObj;
    host->renderer.GpuDrawFxPlayerState6 = test_renderer_GpuDrawFxPlayerState6;
    host->renderer.GpuDrawGadget3AimLine = test_renderer_GpuDrawGadget3AimLine;
    host->renderer.GpuDrawGadget4Trail = test_renderer_GpuDrawGadget4Trail;
    host->renderer.GpuDrawGadget7Trail = test_renderer_GpuDrawGadget7Trail;
    host->renderer.GpuDrawGroundSpark = test_renderer_GpuDrawGroundSpark;
    host->renderer.GpuDrawMesh = test_renderer_GpuDrawMesh;
    host->renderer.GpuDrawSprite = test_renderer_GpuDrawSprite;
    host->renderer.GpuDrawTrailPoint = test_renderer_GpuDrawTrailPoint;
    host->renderer.GpuProjectGadget3Aim = test_renderer_GpuProjectGadget3Aim;
    host->renderer.GpuProjectMesh = test_renderer_GpuProjectMesh;
    host->renderer.HudDrawFrameBox = test_renderer_HudDrawFrameBox;
    host->renderer.HudDrawStatPips = test_renderer_HudDrawStatPips;
    host->renderer.HudDrawTimeRankIcon = test_renderer_HudDrawTimeRankIcon;
    host->renderer.ItemDrawModel = test_renderer_ItemDrawModel;
    host->renderer.MeshInit = test_renderer_MeshInit;
    host->renderer.MeshInitFromAreaTable = test_renderer_MeshInitFromAreaTable;
    host->renderer.ModelCalcEnvUVs = test_renderer_ModelCalcEnvUVs;
    host->renderer.ModelDraw = test_renderer_ModelDraw;
    host->renderer.ModelDrawEnvMap = test_renderer_ModelDrawEnvMap;
    host->renderer.ModelDrawScreenSpace = test_renderer_ModelDrawScreenSpace;
    host->renderer.ModelEmitPrims = test_renderer_ModelEmitPrims;
    host->renderer.ModelEmitPrimsCycleColors = test_renderer_ModelEmitPrimsCycleColors;
    host->renderer.ModelEmitPrimsScratchColors = test_renderer_ModelEmitPrimsScratchColors;
    host->renderer.ModelFillVertexColors = test_renderer_ModelFillVertexColors;
    host->renderer.ModelProjectVertices = test_renderer_ModelProjectVertices;
    host->renderer.ModelTransformRigid = test_renderer_ModelTransformRigid;
    host->renderer.ModelTransformSkinned = test_renderer_ModelTransformSkinned;
    host->renderer.ProjectEffectPoint = test_renderer_ProjectEffectPoint;
    host->renderer.PropSetMesh = test_renderer_PropSetMesh;
    host->renderer.RenderCameraTransition = test_renderer_RenderCameraTransition;
    host->renderer.RenderEnvUV = test_renderer_RenderEnvUV;
    host->renderer.RenderFadeSpiral = test_renderer_RenderFadeSpiral;
    host->renderer.RenderHudPopup = test_renderer_RenderHudPopup;
    host->renderer.RenderIndicator = test_renderer_RenderIndicator;
    host->renderer.RenderScreenFlash = test_renderer_RenderScreenFlash;
    host->renderer.RenderTrailRingSegment = test_renderer_RenderTrailRingSegment;
    host->renderer.RendererGetProjectionDistance = test_renderer_RendererGetProjectionDistance;
    host->renderer.RendererHudDrawWindowFrame = test_renderer_RendererHudDrawWindowFrame;
    host->renderer.RendererHudMonkeyBadge = test_renderer_RendererHudMonkeyBadge;
    host->renderer.RendererIrgbToIr123 = test_renderer_RendererIrgbToIr123;
    host->renderer.RendererPackIrgb = test_renderer_RendererPackIrgb;
    host->renderer.RendererPauseMenuDrawButtons = test_renderer_RendererPauseMenuDrawButtons;
    host->renderer.RendererPauseMenuDrawGadgetScreen = test_renderer_RendererPauseMenuDrawGadgetScreen;
    host->renderer.RendererPrimitiveBlock = test_renderer_RendererPrimitiveBlock;
    host->renderer.RendererSetProjectionDistance = test_renderer_RendererSetProjectionDistance;
    host->renderer.RendererWobbleGte = test_renderer_RendererWobbleGte;
    host->renderer.ScreenBandDraw = test_renderer_ScreenBandDraw;
    host->renderer.VertexWobble = test_renderer_VertexWobble;
    host->renderer.ZakoDebugDrawPrimitive = test_renderer_ZakoDebugDrawPrimitive;
    host->renderer.Ovl185_DrawText = test_renderer_Ovl185_DrawText;
    host->renderer.SaveUi_DrawPortrait = test_renderer_SaveUi_DrawPortrait;
    host->renderer.SaveUi_DrawCursorAnim = test_renderer_SaveUi_DrawCursorAnim;
    host->renderer.SaveUi_DrawFileIcons = test_renderer_SaveUi_DrawFileIcons;
    host->renderer.SaveUi_FadeDraw = test_renderer_SaveUi_FadeDraw;
    host->renderer.SaveUi_DrawModel = test_renderer_SaveUi_DrawModel;
    host->renderer.SaveUi_DrawIcon = test_renderer_SaveUi_DrawIcon;
    host->renderer.SeqPlayer_CopyScreenToBack = test_renderer_SeqPlayer_CopyScreenToBack;
    host->renderer.SeqPlayer_Present = test_renderer_SeqPlayer_Present;
    host->renderer.SeqPlayer_LoadSavedImage = test_renderer_SeqPlayer_LoadSavedImage;
    host->renderer.SeqPlayer_SaveRestoreVram = test_renderer_SeqPlayer_SaveRestoreVram;
    host->renderer.DebugTextSetCursor = test_renderer_DebugTextSetCursor;
    host->renderer.DrawMenuArrows = test_renderer_DrawMenuArrows;
    host->renderer.ModelEmitPrimsAlt = test_renderer_ModelEmitPrimsAlt;
    host->renderer.MatrixBuild = test_renderer_MatrixBuild;
    host->renderer.ModelTransform = test_renderer_ModelTransform;
    host->renderer.ModelDrawTransformed = test_renderer_ModelDrawTransformed;
    host->archive.ArcGetMemberPos = test_archive_ArcGetMemberPos;
    host->archive.CdMgrQueueCommand = test_archive_CdMgrQueueCommand;
    host->archive.CdMgrUpdate = test_archive_CdMgrUpdate;
    host->archive.LoadListStep = test_archive_LoadListStep;
    host->archive.FUN_80025E24 = test_archive_FUN_80025E24;
    host->archive.ArcLoadMember = test_archive_ArcLoadMember;
    host->archive.CdMgrWaitIdle = test_archive_CdMgrWaitIdle;
    host->archive.LoadListInit = test_archive_LoadListInit;
    host->archive.ArcLoadEntry = test_archive_ArcLoadEntry;
    host->archive.CdMgrStopStream = test_archive_CdMgrStopStream;
    host->archive.Ovl181_CdCommand = test_archive_Ovl181_CdCommand;
    host->archive.SeqPlayer_CdReadSeek = test_archive_SeqPlayer_CdReadSeek;
    host->archive.SeqPlayer_CdPlayXaRange = test_archive_SeqPlayer_CdPlayXaRange;
    host->archive.SeqPlayer_LoadFiles = test_archive_SeqPlayer_LoadFiles;
    host->archive.SeqPlayer_CdSetFilter = test_archive_SeqPlayer_CdSetFilter;
    host->archive.SeqPlayer_StGetNextWait = test_archive_SeqPlayer_StGetNextWait;
    host->frame.BeginFrame = test_frame_BeginFrame;
    host->frame.DrawTerrain = test_frame_DrawTerrain;
    host->frame.DrawAreaProps = test_frame_DrawAreaProps;
    host->frame.QueueFrameFlip = test_frame_QueueFrameFlip;
    host->frame.WaitFrameFlip = test_frame_WaitFrameFlip;
}
