/* Generated named native API routes. */
if (address == 0x8002408Cu) {
    if (g->host.platform.CdControl) return g->host.platform.CdControl(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native CdControl boundary"); return 0;
}
if (address == 0x80025D20u) {
    if (g->host.platform.CdIntToPos) return g->host.platform.CdIntToPos(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native CdIntToPos boundary"); return 0;
}
if (address == 0x800242FCu) {
    if (g->host.platform.CdPlayXa) return g->host.platform.CdPlayXa(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native CdPlayXa boundary"); return 0;
}
if (address == 0x80025E24u) {
    if (g->host.platform.CdPosToInt) return g->host.platform.CdPosToInt(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native CdPosToInt boundary"); return 0;
}
if (address == 0x8002408Cu) {
    if (g->host.platform.CdReady) return g->host.platform.CdReady(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native CdReady boundary"); return 0;
}
if (address == 0x80024078u) {
    if (g->host.platform.CdReadyCallback) return g->host.platform.CdReadyCallback(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native CdReadyCallback boundary"); return 0;
}
if (address == 0x80026A50u) {
    if (g->host.platform.CdSync) return g->host.platform.CdSync(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native CdSync boundary"); return 0;
}
if (address == 0x80028850u) {
    if (g->host.platform.ClearImage) return g->host.platform.ClearImage(g->host.user, r, args[0], args[1], args[2], args[3]);
    ae_ram_fail(r, "Mode: missing native ClearImage boundary"); return 0;
}
if (address == 0x800288E0u) {
    if (g->host.platform.ClearImage2) return g->host.platform.ClearImage2(g->host.user, r, args[0], args[1], args[2], args[3]);
    ae_ram_fail(r, "Mode: missing native ClearImage2 boundary"); return 0;
}
if (address == 0x80028BB8u) {
    if (g->host.platform.ClearOTagR) return g->host.platform.ClearOTagR(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native ClearOTagR boundary"); return 0;
}
if (address == 0x8002B380u) {
    if (g->host.platform.DR_MOVE) return g->host.platform.DR_MOVE(g->host.user, r, args[0], args[1], args[2], args[3]);
    ae_ram_fail(r, "Mode: missing native DR_MOVE boundary"); return 0;
}
if (address == 0x80028CC0u) {
    if (g->host.platform.DrawOTag) return g->host.platform.DrawOTag(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native DrawOTag boundary"); return 0;
}
if (address == 0x80028C64u) {
    if (g->host.platform.DrawPrim) return g->host.platform.DrawPrim(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native DrawPrim boundary"); return 0;
}
if (address == 0x800286CCu) {
    if (g->host.platform.DrawSync) return g->host.platform.DrawSync(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native DrawSync boundary"); return 0;
}
if (address == 0x800A3700u || (address == 0x000000B0u && !strcmp(name, "Exit"))) {
    if (g->host.platform.Exit) return g->host.platform.Exit(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native Exit boundary"); return 0;
}
if (address == 0x8002EE04u) {
    if (g->host.platform.GetVideoMode) return g->host.platform.GetVideoMode(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native GetVideoMode boundary"); return 0;
}
if (address == 0x8005CFECu) {
    if (g->host.platform.InitDisplay) return g->host.platform.InitDisplay(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native InitDisplay boundary"); return 0;
}
if (address == 0x8005CD50u) {
    if (g->host.platform.InitSystem) return g->host.platform.InitSystem(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native InitSystem boundary"); return 0;
}
if (address == 0x80028978u) {
    if (g->host.platform.LoadImage) return g->host.platform.LoadImage(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native LoadImage boundary"); return 0;
}
if (address == 0x80068D10u) {
    if (g->host.platform.LoadTimList) return g->host.platform.LoadTimList(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native LoadTimList boundary"); return 0;
}
if (address == 0x800302D0u) {
    if (g->host.platform.MatrixNormal) return g->host.platform.MatrixNormal(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native MatrixNormal boundary"); return 0;
}
if (address == 0x80010000u) {
    if (g->host.platform.MemCardOp) return g->host.platform.MemCardOp(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native MemCardOp boundary"); return 0;
}
if (address == 0x80028A38u) {
    if (g->host.platform.MoveImage) return g->host.platform.MoveImage(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native MoveImage boundary"); return 0;
}
if (address == 0x80031470u) {
    if (g->host.platform.MulMatrix) return g->host.platform.MulMatrix(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native MulMatrix boundary"); return 0;
}
if (address == 0x80031830u) {
    if (g->host.platform.MulMatrix0) return g->host.platform.MulMatrix0(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native MulMatrix0 boundary"); return 0;
}
if (address == 0x8002B52Cu) {
    if (g->host.platform.PadGetState) return g->host.platform.PadGetState(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native PadGetState boundary"); return 0;
}
if (address == 0x8002B5F8u) {
    if (g->host.platform.PadInfoMode) return g->host.platform.PadInfoMode(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native PadInfoMode boundary"); return 0;
}
if (address == 0x800A3720u) {
    if (g->host.platform.Printf) return g->host.platform.Printf(g->host.user, r, args[0], args[1], args[2], args[3]);
    ae_ram_fail(r, "Mode: missing native Printf boundary"); return 0;
}
if (address == 0x80028EFCu) {
    if (g->host.platform.PutDispEnv) return g->host.platform.PutDispEnv(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native PutDispEnv boundary"); return 0;
}
if (address == 0x80028D30u) {
    if (g->host.platform.PutDrawEnv) return g->host.platform.PutDrawEnv(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native PutDrawEnv boundary"); return 0;
}
if (address == 0x800315D0u) {
    if (g->host.platform.ReadGeomScreen) return g->host.platform.ReadGeomScreen(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native ReadGeomScreen boundary"); return 0;
}
if (address == 0x80028350u) {
    if (g->host.platform.ResetGraph) return g->host.platform.ResetGraph(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native ResetGraph boundary"); return 0;
}
if (address == 0x80031940u) {
    if (g->host.platform.RotTrans) return g->host.platform.RotTrans(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native RotTrans boundary"); return 0;
}
if (address == 0x80031330u) {
    if (g->host.platform.RotTransPers) return g->host.platform.RotTransPers(g->host.user, r, args[0], args[1], args[2], args[3]);
    ae_ram_fail(r, "Mode: missing native RotTransPers boundary"); return 0;
}
if (address == 0x80031230u) {
    if (g->host.platform.RotTransSV) return g->host.platform.RotTransSV(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native RotTransSV boundary"); return 0;
}
if (address == 0x80011A2Cu) {
    if (g->host.platform.RunMinigameExe) return g->host.platform.RunMinigameExe(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native RunMinigameExe boundary"); return 0;
}
if (address == 0x800305B0u) {
    if (g->host.platform.ScaleMatrixL) return g->host.platform.ScaleMatrixL(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native ScaleMatrixL boundary"); return 0;
}
if (address == 0x80136BD4u && g->loaded_overlay == 0x180u) {
    if (g->host.platform.SeqPlayer_SetupDisplay) return g->host.platform.SeqPlayer_SetupDisplay(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native SeqPlayer_SetupDisplay boundary"); return 0;
}
if (address == 0x800315E0u) {
    if (g->host.platform.SetBackColor) return g->host.platform.SetBackColor(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native SetBackColor boundary"); return 0;
}
if (address == 0x80031600u) {
    if (g->host.platform.SetColorMatrix) return g->host.platform.SetColorMatrix(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native SetColorMatrix boundary"); return 0;
}
if (address == 0x8002B340u) {
    if (g->host.platform.SetDefDispEnv) return g->host.platform.SetDefDispEnv(g->host.user, r, args[0], args[1], args[2], args[3], args[4]);
    ae_ram_fail(r, "Mode: missing native SetDefDispEnv boundary"); return 0;
}
if (address == 0x8002B280u) {
    if (g->host.platform.SetDefDrawEnv) return g->host.platform.SetDefDrawEnv(g->host.user, r, args[0], args[1], args[2], args[3], args[4]);
    ae_ram_fail(r, "Mode: missing native SetDefDrawEnv boundary"); return 0;
}
if (address == 0x80028634u) {
    if (g->host.platform.SetDispMask) return g->host.platform.SetDispMask(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native SetDispMask boundary"); return 0;
}
if (address == 0x8002B3E0u) {
    if (g->host.platform.SetDrawStp) return g->host.platform.SetDrawStp(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native SetDrawStp boundary"); return 0;
}
if (address == 0x8002B410u) {
    if (g->host.platform.SetDrawTPage) return g->host.platform.SetDrawTPage(g->host.user, r, args[0], args[1], args[2], args[3]);
    ae_ram_fail(r, "Mode: missing native SetDrawTPage boundary"); return 0;
}
if (address == 0x80031630u) {
    if (g->host.platform.SetFarColor) return g->host.platform.SetFarColor(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native SetFarColor boundary"); return 0;
}
if (address == 0x8002FAD0u) {
    if (g->host.platform.SetGeomOffset) return g->host.platform.SetGeomOffset(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native SetGeomOffset boundary"); return 0;
}
if (address == 0x8002FAF0u) {
    if (g->host.platform.SetGeomScreen) return g->host.platform.SetGeomScreen(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native SetGeomScreen boundary"); return 0;
}
if (address == 0x800284C4u) {
    if (g->host.platform.SetGraphDebug) return g->host.platform.SetGraphDebug(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native SetGraphDebug boundary"); return 0;
}
if (address == 0x8002FB00u) {
    if (g->host.platform.SetRotMatrix) return g->host.platform.SetRotMatrix(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native SetRotMatrix boundary"); return 0;
}
if (address == 0x8002FB30u) {
    if (g->host.platform.SetTransMatrix) return g->host.platform.SetTransMatrix(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native SetTransMatrix boundary"); return 0;
}
if (address == 0x8002EDF0u) {
    if (g->host.platform.SetVideoMode) return g->host.platform.SetVideoMode(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native SetVideoMode boundary"); return 0;
}
if (address == 0x80086170u) {
    if (g->host.platform.SoundRestoreVolume) return g->host.platform.SoundRestoreVolume(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native SoundRestoreVolume boundary"); return 0;
}
if (address == 0x80086248u) {
    if (g->host.platform.SoundStopAndRestoreVolume) return g->host.platform.SoundStopAndRestoreVolume(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native SoundStopAndRestoreVolume boundary"); return 0;
}
if (address == 0x800A1398u) {
    if (g->host.platform.SpuWaitIdle) return g->host.platform.SpuWaitIdle(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native SpuWaitIdle boundary"); return 0;
}
if (address == 0x80025EB0u) {
    if (g->host.platform.StFree) return g->host.platform.StFree(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native StFree boundary"); return 0;
}
if (address == 0x80025F60u) {
    if (g->host.platform.StRingStatus) return g->host.platform.StRingStatus(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native StRingStatus boundary"); return 0;
}
if (address == 0x80025C60u) {
    if (g->host.platform.StSetRing) return g->host.platform.StSetRing(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native StSetRing boundary"); return 0;
}
if (address == 0x80025C90u) {
    if (g->host.platform.StSetStream) return g->host.platform.StSetStream(g->host.user, r, args[0], args[1], args[2], args[3], args[4]);
    ae_ram_fail(r, "Mode: missing native StSetStream boundary"); return 0;
}
if (address == 0x8002E824u) {
    if (g->host.platform.StopCallback) return g->host.platform.StopCallback(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native StopCallback boundary"); return 0;
}
if (address == 0x800289D8u) {
    if (g->host.platform.StoreImage) return g->host.platform.StoreImage(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native StoreImage boundary"); return 0;
}
if (address == 0x8008DE1Cu) {
    if (g->host.platform.TextLineWidth) return g->host.platform.TextLineWidth(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native TextLineWidth boundary"); return 0;
}
if (address == 0x8002E520u) {
    if (g->host.platform.VSync) return g->host.platform.VSync(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native VSync boundary"); return 0;
}
if (address == 0x8002E7C0u) {
    if (g->host.platform.VSyncCallback) return g->host.platform.VSyncCallback(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native VSyncCallback boundary"); return 0;
}
if (address == 0x800A3760u) {
    if (g->host.platform.memcpy) return g->host.platform.memcpy(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native memcpy boundary"); return 0;
}
if (address == 0x800A3740u) {
    if (g->host.platform.memset) return g->host.platform.memset(g->host.user, r, args[0], args[1], args[2]);
    ae_ram_fail(r, "Mode: missing native memset boundary"); return 0;
}
if (address == 0x800A3750u) {
    if (g->host.platform.srand) return g->host.platform.srand(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native srand boundary"); return 0;
}
if (address == 0x80012C0Cu) {
    if (g->host.archive.ArcGetMemberPos) return g->host.archive.ArcGetMemberPos(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native ArcGetMemberPos boundary"); return 0;
}
if (address == 0x80012360u) {
    if (g->host.archive.CdMgrQueueCommand) return g->host.archive.CdMgrQueueCommand(g->host.user, r, args[0], args[1], args[2], args[3]);
    ae_ram_fail(r, "Mode: missing native CdMgrQueueCommand boundary"); return 0;
}
if (address == 0x800125B8u) {
    if (g->host.archive.CdMgrUpdate) return g->host.archive.CdMgrUpdate(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native CdMgrUpdate boundary"); return 0;
}
if (address == 0x800116C0u) {
    if (g->host.archive.LoadListStep) return g->host.archive.LoadListStep(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native LoadListStep boundary"); return 0;
}
if (address == 0x80025E24u) {
    if (g->host.archive.FUN_80025E24) return g->host.archive.FUN_80025E24(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native FUN_80025E24 boundary"); return 0;
}
if (address == 0x800112E4u) {
    if (g->host.archive.ArcLoadMember) return g->host.archive.ArcLoadMember(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native ArcLoadMember boundary"); return 0;
}
if (address == 0x8001256Cu) {
    if (g->host.archive.CdMgrWaitIdle) return g->host.archive.CdMgrWaitIdle(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native CdMgrWaitIdle boundary"); return 0;
}
if (address == 0x80011104u) {
    if (g->host.archive.LoadListInit) return g->host.archive.LoadListInit(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native LoadListInit boundary"); return 0;
}
if (address == 0x80011194u) {
    if (g->host.archive.ArcLoadEntry) return g->host.archive.ArcLoadEntry(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native ArcLoadEntry boundary"); return 0;
}
if (address == 0x8001231Cu) {
    if (g->host.archive.CdMgrStopStream) return g->host.archive.CdMgrStopStream(g->host.user, r);
    ae_ram_fail(r, "Mode: missing native CdMgrStopStream boundary"); return 0;
}
if (address == 0x80136AECu && g->loaded_overlay == 0x181u) {
    if (g->host.archive.Ovl181_CdCommand) return g->host.archive.Ovl181_CdCommand(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native Ovl181_CdCommand boundary"); return 0;
}
if (address == 0x80136454u && g->loaded_overlay == 0x180u) {
    if (g->host.archive.SeqPlayer_CdReadSeek) return g->host.archive.SeqPlayer_CdReadSeek(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native SeqPlayer_CdReadSeek boundary"); return 0;
}
if (address == 0x801364B0u && g->loaded_overlay == 0x180u) {
    if (g->host.archive.SeqPlayer_CdPlayXaRange) return g->host.archive.SeqPlayer_CdPlayXaRange(g->host.user, r, args[0], args[1], args[2], args[3]);
    ae_ram_fail(r, "Mode: missing native SeqPlayer_CdPlayXaRange boundary"); return 0;
}
if (address == 0x80136884u && g->loaded_overlay == 0x180u) {
    if (g->host.archive.SeqPlayer_LoadFiles) return g->host.archive.SeqPlayer_LoadFiles(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native SeqPlayer_LoadFiles boundary"); return 0;
}
if (address == 0x80136E68u && g->loaded_overlay == 0x180u) {
    if (g->host.archive.SeqPlayer_CdSetFilter) return g->host.archive.SeqPlayer_CdSetFilter(g->host.user, r, args[0]);
    ae_ram_fail(r, "Mode: missing native SeqPlayer_CdSetFilter boundary"); return 0;
}
if (address == 0x80136E00u && g->loaded_overlay == 0x180u) {
    if (g->host.archive.SeqPlayer_StGetNextWait) return g->host.archive.SeqPlayer_StGetNextWait(g->host.user, r, args[0], args[1]);
    ae_ram_fail(r, "Mode: missing native SeqPlayer_StGetNextWait boundary"); return 0;
}
if (address == 0x8006ABF0u) {
    if (g->host.renderer.AlertIconDraw) return g->host.renderer.AlertIconDraw(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80046670u) {
    if (g->host.renderer.CameraLookAt) return g->host.renderer.CameraLookAt(g->host.user, r, args[0], args[1], args[2], args[3], args[4]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8007D71Cu) {
    if (g->host.renderer.DrawMenuChrome) return g->host.renderer.DrawMenuChrome(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8007F148u) {
    if (g->host.renderer.DrawMenuCursor) return g->host.renderer.DrawMenuCursor(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8007E030u) {
    if (g->host.renderer.DrawMenuHighlight) return g->host.renderer.DrawMenuHighlight(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8007E1B8u) {
    if (g->host.renderer.DrawMenuItem) return g->host.renderer.DrawMenuItem(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8007D9F0u) {
    if (g->host.renderer.DrawMenuRows) return g->host.renderer.DrawMenuRows(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8007E3F8u) {
    if (g->host.renderer.DrawMenuSelection) return g->host.renderer.DrawMenuSelection(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8002020Cu) {
    if (g->host.renderer.DrawRibbonBetween) return g->host.renderer.DrawRibbonBetween(g->host.user, r, args[0], args[1], args[2], args[3], args[4]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8001F604u) {
    if (g->host.renderer.DrawShadow) return g->host.renderer.DrawShadow(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8005E000u) {
    if (g->host.renderer.Draw_Nop) return g->host.renderer.Draw_Nop(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8006E5B4u) {
    if (g->host.renderer.EffectRingDraw) return g->host.renderer.EffectRingDraw(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80060334u) {
    if (g->host.renderer.HudDrawFrameBox) return g->host.renderer.HudDrawFrameBox(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80060224u) {
    if (g->host.renderer.HudDrawStatPips) return g->host.renderer.HudDrawStatPips(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8005F70Cu) {
    if (g->host.renderer.HudDrawTimeRankIcon) return g->host.renderer.HudDrawTimeRankIcon(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x800845E0u) {
    if (g->host.renderer.ItemDrawModel) return g->host.renderer.ItemDrawModel(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80015C34u) {
    if (g->host.renderer.MeshInit) return g->host.renderer.MeshInit(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80015CC8u) {
    if (g->host.renderer.MeshInitFromAreaTable) return g->host.renderer.MeshInitFromAreaTable(g->host.user, r, args[0], args[1], args[2], args[3], args[4]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80016B94u) {
    if (g->host.renderer.ModelCalcEnvUVs) return g->host.renderer.ModelCalcEnvUVs(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80017CD4u) {
    if (g->host.renderer.ModelDraw) return g->host.renderer.ModelDraw(g->host.user, r, args[0], args[1], args[2], args[3], args[4]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8001CE54u) {
    if (g->host.renderer.ModelDrawEnvMap) return g->host.renderer.ModelDrawEnvMap(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x800207B4u) {
    if (g->host.renderer.ModelDrawScreenSpace) return g->host.renderer.ModelDrawScreenSpace(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x800180B8u) {
    if (g->host.renderer.ModelEmitPrims) return g->host.renderer.ModelEmitPrims(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8001D530u) {
    if (g->host.renderer.ModelEmitPrimsCycleColors) return g->host.renderer.ModelEmitPrimsCycleColors(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8001C918u) {
    if (g->host.renderer.ModelEmitPrimsScratchColors) return g->host.renderer.ModelEmitPrimsScratchColors(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80020A20u) {
    if (g->host.renderer.ModelFillVertexColors) return g->host.renderer.ModelFillVertexColors(g->host.user, r, args[0], args[1], args[2], args[3], args[4]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x800161C0u) {
    if (g->host.renderer.ModelTransformRigid) return g->host.renderer.ModelTransformRigid(g->host.user, r, args[0], args[1], args[2], args[3], args[4]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80016A60u) {
    if (g->host.renderer.ModelTransformSkinned) return g->host.renderer.ModelTransformSkinned(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80090E54u) {
    if (g->host.renderer.PropSetMesh) return g->host.renderer.PropSetMesh(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8009B9C8u) {
    if (g->host.renderer.ScreenBandDraw) return g->host.renderer.ScreenBandDraw(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8001F434u) {
    if (g->host.renderer.VertexWobble) return g->host.renderer.VertexWobble(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8014666Cu && g->loaded_overlay == 0x185u) {
    if (g->host.renderer.Ovl185_DrawText) return g->host.renderer.Ovl185_DrawText(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80139570u && g->loaded_overlay == 0x186u) {
    if (g->host.renderer.SaveUi_DrawPortrait) return g->host.renderer.SaveUi_DrawPortrait(g->host.user, r, args[0], args[1]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80139CF0u && g->loaded_overlay == 0x186u) {
    if (g->host.renderer.SaveUi_DrawCursorAnim) return g->host.renderer.SaveUi_DrawCursorAnim(g->host.user, r);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8013A478u && g->loaded_overlay == 0x186u) {
    if (g->host.renderer.SaveUi_DrawFileIcons) return g->host.renderer.SaveUi_DrawFileIcons(g->host.user, r);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8013A70Cu && g->loaded_overlay == 0x186u) {
    if (g->host.renderer.SaveUi_FadeDraw) return g->host.renderer.SaveUi_FadeDraw(g->host.user, r);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8013A804u && g->loaded_overlay == 0x186u) {
    if (g->host.renderer.SaveUi_DrawModel) return g->host.renderer.SaveUi_DrawModel(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8013946Cu && g->loaded_overlay == 0x186u) {
    if (g->host.renderer.SaveUi_DrawIcon) return g->host.renderer.SaveUi_DrawIcon(g->host.user, r, args[0], args[1], args[2]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x801367DCu && g->loaded_overlay == 0x180u) {
    if (g->host.renderer.SeqPlayer_CopyScreenToBack) return g->host.renderer.SeqPlayer_CopyScreenToBack(g->host.user, r);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80136FF0u && g->loaded_overlay == 0x180u) {
    if (g->host.renderer.SeqPlayer_Present) return g->host.renderer.SeqPlayer_Present(g->host.user, r, args[0]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8013AA70u && g->loaded_overlay == 0x180u) {
    if (g->host.renderer.SeqPlayer_LoadSavedImage) return g->host.renderer.SeqPlayer_LoadSavedImage(g->host.user, r);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8013BA80u && g->loaded_overlay == 0x180u) {
    if (g->host.renderer.SeqPlayer_SaveRestoreVram) return g->host.renderer.SeqPlayer_SaveRestoreVram(g->host.user, r, args[0]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8008CC60u) {
    if (g->host.renderer.DebugTextSetCursor) return g->host.renderer.DebugTextSetCursor(g->host.user, r, args[0], args[1]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8008211Cu) {
    if (g->host.renderer.DrawMenuArrows) return g->host.renderer.DrawMenuArrows(g->host.user, r, args[0], args[1], args[2]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x8001A414u) {
    if (g->host.renderer.ModelEmitPrimsAlt) return g->host.renderer.ModelEmitPrimsAlt(g->host.user, r, args[0], args[1], args[2]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80030450u) {
    if (g->host.renderer.MatrixBuild) return g->host.renderer.MatrixBuild(g->host.user, r, args[0], args[1], args[2]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x800160ECu) {
    if (g->host.renderer.ModelTransform) return g->host.renderer.ModelTransform(g->host.user, r, args[0], args[1], args[2]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
if (address == 0x80017EFCu) {
    if (g->host.renderer.ModelDrawTransformed) return g->host.renderer.ModelDrawTransformed(g->host.user, r, args[0], args[1], args[2], args[3]);
    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;
}
