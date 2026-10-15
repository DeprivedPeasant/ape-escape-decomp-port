/* Generated test callbacks: preserve host argument words. */
static int32_t test_platform_CdControl(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "CdControl", 0x8002408Cu, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_CdIntToPos(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "CdIntToPos", 0x80025D20u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_CdPosToInt(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "CdPosToInt", 0x80025E24u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_CdReadyCallback(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "CdReadyCallback", 0x80024078u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_ClearImage(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ClearImage", 0x80028850u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_platform_DR_MOVE(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DR_MOVE", 0x8002B380u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_platform_DrawOTag(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "DrawOTag", 0x80028CC0u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_DrawSync(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "DrawSync", 0x800286CCu, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_Exit(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "Exit", 0x800A3700u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_LoadImage(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "LoadImage", 0x80028978u, 2, a0, a1, 0, 0, 0, 0);
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
static int32_t test_platform_RotMatrixX(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "RotMatrixX", 0x80030A30u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_RotMatrixY(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "RotMatrixY", 0x80030890u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_RotTransPers(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RotTransPers", 0x80031330u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_platform_RotTransSV(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "RotTransSV", 0x80031230u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_ScaleMatrixL(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "ScaleMatrixL", 0x800305B0u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_SetBackColor(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "SetBackColor", 0x800315E0u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_SetColorMatrix(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetColorMatrix", 0x80031600u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SetFarColor(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "SetFarColor", 0x80031630u, 3, a0, a1, a2, 0, 0, 0);
}
static int32_t test_platform_SetGeomScreen(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetGeomScreen", 0x8002FAF0u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SetRotMatrix(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetRotMatrix", 0x8002FB00u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SetTransMatrix(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "SetTransMatrix", 0x8002FB30u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_SpuWaitIdle(void *user, AeRam *r) {
    (void)r;
    return trace_call(user, "SpuWaitIdle", 0x800A1398u, 0, 0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_StoreImage(void *user, AeRam *r, uint32_t a0, uint32_t a1) {
    (void)r;
    return trace_call(user, "StoreImage", 0x800289D8u, 2, a0, a1, 0, 0, 0, 0);
}
static int32_t test_platform_VSync(void *user, AeRam *r, uint32_t a0) {
    (void)r;
    return trace_call(user, "VSync", 0x8002E520u, 1, a0, 0, 0, 0, 0, 0);
}
static int32_t test_platform_memset(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2) {
    (void)r;
    return trace_call(user, "memset", 0x800A3740u, 3, a0, a1, a2, 0, 0, 0);
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
static int32_t test_renderer_ModelDraw(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)r;
    return trace_call(user, "ModelDraw", 0x80017CD4u, 5, a0, a1, a2, a3, a4, 0);
}
static int32_t test_renderer_ModelDrawEnvMap(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelDrawEnvMap", 0x8001CE54u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ModelEmitPrimsCycleColors(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ModelEmitPrimsCycleColors", 0x8001D530u, 4, a0, a1, a2, a3, 0, 0);
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
static int32_t test_renderer_RendererPrimitiveBlock(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererPrimitiveBlock", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_RendererWobbleGte(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RendererWobbleGte", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ScreenBandDraw(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ScreenBandDraw", 0x8009B9C8u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_renderer_ZakoDebugDrawPrimitive(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "ZakoDebugDrawPrimitive", 0x00000000u, 4, a0, a1, a2, a3, 0, 0);
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
static int32_t test_frame_PauseMenuUpdate(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "PauseMenuUpdate", 0x800821D4u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_BeginFrame(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "BeginFrame", 0x8005DF48u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_MenuWindowsRun(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "MenuWindowsRun", 0x8007D530u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_MessageBoxUpdateDraw(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "MessageBoxUpdateDraw", 0x8008DF4Cu, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_DrawTextSprites(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawTextSprites", 0x80082288u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_PlayerAnimateAndDraw(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "PlayerAnimateAndDraw", 0x80063628u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_PlayerUpdate(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "PlayerUpdate", 0x80060780u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_RcCarDraw(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "RcCarDraw", 0x80099AE0u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_DrawTerrain(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawTerrain", 0x80043A8Cu, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_DrawAreaProps(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "DrawAreaProps", 0x80016F34u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_StageDrawScreenOverlay(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "StageDrawScreenOverlay", 0x8005E0F0u, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_SoundLoopsPause(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "SoundLoopsPause", 0x8007292Cu, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t test_frame_OpenPauseMenu(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, "OpenPauseMenu", 0x8007F56Cu, 4, a0, a1, a2, a3, 0, 0);
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
    host->platform.CdPosToInt = test_platform_CdPosToInt;
    host->platform.CdReadyCallback = test_platform_CdReadyCallback;
    host->platform.ClearImage = test_platform_ClearImage;
    host->platform.DR_MOVE = test_platform_DR_MOVE;
    host->platform.DrawOTag = test_platform_DrawOTag;
    host->platform.DrawSync = test_platform_DrawSync;
    host->platform.Exit = test_platform_Exit;
    host->platform.LoadImage = test_platform_LoadImage;
    host->platform.MoveImage = test_platform_MoveImage;
    host->platform.MulMatrix = test_platform_MulMatrix;
    host->platform.MulMatrix0 = test_platform_MulMatrix0;
    host->platform.PadGetState = test_platform_PadGetState;
    host->platform.PadInfoMode = test_platform_PadInfoMode;
    host->platform.Printf = test_platform_Printf;
    host->platform.PutDispEnv = test_platform_PutDispEnv;
    host->platform.PutDrawEnv = test_platform_PutDrawEnv;
    host->platform.RotMatrixX = test_platform_RotMatrixX;
    host->platform.RotMatrixY = test_platform_RotMatrixY;
    host->platform.RotTransPers = test_platform_RotTransPers;
    host->platform.RotTransSV = test_platform_RotTransSV;
    host->platform.ScaleMatrixL = test_platform_ScaleMatrixL;
    host->platform.SetBackColor = test_platform_SetBackColor;
    host->platform.SetColorMatrix = test_platform_SetColorMatrix;
    host->platform.SetFarColor = test_platform_SetFarColor;
    host->platform.SetGeomScreen = test_platform_SetGeomScreen;
    host->platform.SetRotMatrix = test_platform_SetRotMatrix;
    host->platform.SetTransMatrix = test_platform_SetTransMatrix;
    host->platform.SpuWaitIdle = test_platform_SpuWaitIdle;
    host->platform.StoreImage = test_platform_StoreImage;
    host->platform.VSync = test_platform_VSync;
    host->platform.memset = test_platform_memset;
    host->renderer.AlertIconDraw = test_renderer_AlertIconDraw;
    host->renderer.CameraLookAt = test_renderer_CameraLookAt;
    host->renderer.DrawDebugGlyph = test_renderer_DrawDebugGlyph;
    host->renderer.DrawParticleQuad = test_renderer_DrawParticleQuad;
    host->renderer.DrawProjectileSprite = test_renderer_DrawProjectileSprite;
    host->renderer.DrawRibbonBetween = test_renderer_DrawRibbonBetween;
    host->renderer.DrawRibbonStrip = test_renderer_DrawRibbonStrip;
    host->renderer.DrawShadow = test_renderer_DrawShadow;
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
    host->renderer.ItemDrawModel = test_renderer_ItemDrawModel;
    host->renderer.MeshInit = test_renderer_MeshInit;
    host->renderer.MeshInitFromAreaTable = test_renderer_MeshInitFromAreaTable;
    host->renderer.ModelDraw = test_renderer_ModelDraw;
    host->renderer.ModelDrawEnvMap = test_renderer_ModelDrawEnvMap;
    host->renderer.ModelEmitPrimsCycleColors = test_renderer_ModelEmitPrimsCycleColors;
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
    host->renderer.RendererPrimitiveBlock = test_renderer_RendererPrimitiveBlock;
    host->renderer.RendererWobbleGte = test_renderer_RendererWobbleGte;
    host->renderer.ScreenBandDraw = test_renderer_ScreenBandDraw;
    host->renderer.ZakoDebugDrawPrimitive = test_renderer_ZakoDebugDrawPrimitive;
    host->archive.ArcGetMemberPos = test_archive_ArcGetMemberPos;
    host->archive.CdMgrQueueCommand = test_archive_CdMgrQueueCommand;
    host->archive.CdMgrUpdate = test_archive_CdMgrUpdate;
    host->archive.LoadListStep = test_archive_LoadListStep;
    host->archive.FUN_80025E24 = test_archive_FUN_80025E24;
    host->frame.PauseMenuUpdate = test_frame_PauseMenuUpdate;
    host->frame.BeginFrame = test_frame_BeginFrame;
    host->frame.MenuWindowsRun = test_frame_MenuWindowsRun;
    host->frame.MessageBoxUpdateDraw = test_frame_MessageBoxUpdateDraw;
    host->frame.DrawTextSprites = test_frame_DrawTextSprites;
    host->frame.PlayerAnimateAndDraw = test_frame_PlayerAnimateAndDraw;
    host->frame.PlayerUpdate = test_frame_PlayerUpdate;
    host->frame.RcCarDraw = test_frame_RcCarDraw;
    host->frame.DrawTerrain = test_frame_DrawTerrain;
    host->frame.DrawAreaProps = test_frame_DrawAreaProps;
    host->frame.StageDrawScreenOverlay = test_frame_StageDrawScreenOverlay;
    host->frame.SoundLoopsPause = test_frame_SoundLoopsPause;
    host->frame.OpenPauseMenu = test_frame_OpenPauseMenu;
    host->frame.QueueFrameFlip = test_frame_QueueFrameFlip;
    host->frame.WaitFrameFlip = test_frame_WaitFrameFlip;
}
