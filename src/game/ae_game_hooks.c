/* Generated typed module wiring; see tools/generate_game_hooks.py. */
#include "ae_calls.h"

static int32_t wire_original_words(void *user, AeRam *ram, const char *name, uint32_t address, uint32_t count, const uint32_t *args) {
    AeGame *g = user; uint32_t words[7] = {0}; (void)ram;
    if (count > 7) return ae_game_missing_words(g, name, address, count, args);
    for (uint32_t i = 0; i < count; ++i) words[i] = args[i];
    if (!ae_game_find_call(address)) return ae_game_missing_words(g, name, address, count, args);
    return ae_game_call7(g, address, words[0], words[1], words[2], words[3], words[4], words[5], words[6]);
}
static int32_t wire_renderer_words(void *user, AeRam *ram, const char *name, uint32_t address, uint32_t count, const uint32_t *args) {
    AeGame *g = user;
    return g->host.render_words ? g->host.render_words(g->host.user, ram, name, address, count, args) : 0;
}

static int32_t wire_anim_draw_SoundLoopStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800878E0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_anim_draw_SoundLoopStop(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008806Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_anim_draw_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_anim_draw_PadRumble(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80014168u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_anim_draw_ModelTransformRigid(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelTransformRigid ? g->host.renderer.ModelTransformRigid(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformRigid", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_ModelTransformSkinned(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelTransformSkinned ? g->host.renderer.ModelTransformSkinned(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformSkinned", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_ModelCalcEnvUVs(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelCalcEnvUVs ? g->host.renderer.ModelCalcEnvUVs(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelCalcEnvUVs", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_ModelDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelDraw ? g->host.renderer.ModelDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_ModelEmitPrims(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelEmitPrims ? g->host.renderer.ModelEmitPrims(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelEmitPrims", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_ModelFillVertexColors(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.ModelFillVertexColors ? g->host.renderer.ModelFillVertexColors(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelFillVertexColors", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_ModelDrawEnvMap(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelDrawEnvMap ? g->host.renderer.ModelDrawEnvMap(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDrawEnvMap", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_ModelEmitPrimsCycleColors(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelEmitPrimsCycleColors ? g->host.renderer.ModelEmitPrimsCycleColors(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelEmitPrimsCycleColors", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_DrawShadow(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.DrawShadow ? g->host.renderer.DrawShadow(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawShadow", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_Draw_Nop(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.Draw_Nop ? g->host.renderer.Draw_Nop(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "Draw_Nop", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_EffectRingDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.EffectRingDraw ? g->host.renderer.EffectRingDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "EffectRingDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_VertexWobble(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.VertexWobble ? g->host.renderer.VertexWobble(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "VertexWobble", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_SfxVoiceSetPitch(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SfxVoiceSetPitch", 0x800A35FCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_anim_draw_SetRotMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetRotMatrix ? g->host.platform.SetRotMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_anim_draw_SetTransMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetTransMatrix ? g->host.platform.SetTransMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_anim_draw_SDK_80031230(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.RotTransSV ? g->host.platform.RotTransSV(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_anim_draw_SDK_800302D0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.MatrixNormal ? g->host.platform.MatrixNormal(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_anim_draw_TurnTowardsY(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80066C90u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_anim_draw_SoundEmitterPitch(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80087C28u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_anim_draw_IsPlayerSwimming(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80068B7Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_anim_draw_PlayerStopGadgetSounds(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007A180u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_anim_draw_RandomRange(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "RandomRange", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_anim_draw_RendererPrimitiveBlock(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RendererPrimitiveBlock ? g->host.renderer.RendererPrimitiveBlock(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RendererPrimitiveBlock", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_anim_draw_RendererWobbleGte(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RendererWobbleGte ? g->host.renderer.RendererWobbleGte(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RendererWobbleGte", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_area_set_Printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_area_set_Exit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Exit ? g->host.platform.Exit(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_area_set_ProgressGetByte(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800963ACu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013d1f0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013d1f0", 0x8013D1F0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_Area44_Boss_SetAnim(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "Area44_Boss_SetAnim", 0x80140948u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013f68c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013f68c", 0x8013F68Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013f130(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013f130", 0x8013F130u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013ecf4(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013ecf4", 0x8013ECF4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013d52c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013d52c", 0x8013D52Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013db98(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013db98", 0x8013DB98u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013dc00(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013dc00", 0x8013DC00u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013d440(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013d440", 0x8013D440u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_Area46_Boss_SetAnim(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "Area46_Boss_SetAnim", 0x8013EF0Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_Area46_Boss_ClearFlag(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "Area46_Boss_ClearFlag", 0x8013EF00u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013f544(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013f544", 0x8013F544u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013e14c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013e14c", 0x8013E14Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013ec5c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013ec5c", 0x8013EC5Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013d8bc(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013d8bc", 0x8013D8BCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013df28(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013df28", 0x8013DF28u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013d10c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013d10c", 0x8013D10Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013f4f0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013f4f0", 0x8013F4F0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013e8c0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013e8c0", 0x8013E8C0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013de14(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013de14", 0x8013DE14u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013e4d8(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013e4d8", 0x8013E4D8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013d804(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013d804", 0x8013D804u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013d000(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8013D000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_Area56_Boss_SetCameraObjAnim(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "Area56_Boss_SetCameraObjAnim", 0x801406F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013f86c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013f86c", 0x8013F86Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013e4a8(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013e4a8", 0x8013E4A8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013d47c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013d47c", 0x8013D47Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013e070(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013e070", 0x8013E070u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013e7f8(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013e7f8", 0x8013E7F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013f080(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013f080", 0x8013F080u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_Area57_Boss_SetAnim(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "Area57_Boss_SetAnim", 0x8013F3A4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_Area57_Boss_ClearFlag(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "Area57_Boss_ClearFlag", 0x8013F398u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013e020(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013e020", 0x8013E020u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013d33c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013d33c", 0x8013D33Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_801407bc(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_801407bc", 0x801407BCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013d9a8(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013d9a8", 0x8013D9A8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_area_set_FUN_8013ee9c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013ee9c", 0x8013EE9Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_body_move_LocalDirectionPadding(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)g;
    return (uint32_t)(g->host.body_direction_padding ? g->host.body_direction_padding(g->host.user, g->ram) : 0);
}

static void wire_body_move_SfxVoiceSetPitch(void * p0, AeRam * p1, int32_t p2, int32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)(ae_game_missing_call(g, "SfxVoiceSetPitch", 0x800A35FCu, (uint32_t)p2, (uint32_t)p3, 0, 0));
}

static void wire_body_move_XaPlay(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, 0x80011F6Cu, (uint32_t)p2, 0, 0, 0));
}

static int32_t wire_camera_Atan2Safe(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80046340u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_VectorNormalSS(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800301E0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_SquareRoot0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800303C0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_GetGroundHeight(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80043400u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_rsin(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8002FC20u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_rcos(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8002FB50u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_RotTransPers(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.RotTransPers ? g->host.platform.RotTransPers(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_camera_SetRotMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetRotMatrix ? g->host.platform.SetRotMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_camera_SetTransMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetTransMatrix ? g->host.platform.SetTransMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_camera_memset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.memset ? g->host.platform.memset(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_camera_PlayerCanExitStage(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005DC78u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_PlayerIsSubmerged(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8006B9F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_CameraRcCarActive(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004A978u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_CameraRcCarFollow(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CameraRcCarFollow", 0x8004AA58u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_IsFlightArea(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004A940u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_FUN_8013d000(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8013D000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_BossUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005AC28u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_CameraAreaScriptDispatch(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CameraAreaScriptDispatch", 0x8004B590u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_CallIndexedHandler(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CallIndexedHandler", 0x8004D880u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_RotMatrixY(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80030890u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_ApplyMatrixSV(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800300B0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_MatrixIdentity(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004637Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_CameraGetPlayerDir(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004B880u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_AimRequestStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "AimRequestStart", 0x80048398u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_AimRequestClear(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80048430u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_AimModeEnter(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004B4B8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_AimModeLeave(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004B510u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_CameraFixedModeOff(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80048748u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_StageExitPending(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004D104u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_BossAimSfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "BossAimSfx", 0x8004898Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_BossKindForArea(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005AB00u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_ScreenFadeSet(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "ScreenFadeSet", 0x80046978u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_HudDrawMonkeyInfo(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "HudDrawMonkeyInfo", 0x80060448u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_RadarUsable(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800468ECu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_MonkeyFlag248Set(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8009A17Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_CameraRadarFocusUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CameraRadarFocusUpdate", 0x80047EE0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camera_CameraRadarFocusSelect(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CameraRadarFocusSelect", 0x80047E8Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camupd_PadGetState(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.PadGetState ? g->host.platform.PadGetState(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_camupd_SpawnParticle(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800698B8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camupd_Printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_camupd_memset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.memset ? g->host.platform.memset(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_camupd_CameraAreaScriptDispatch(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CameraAreaScriptDispatch", 0x8004B590u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_camupd_CameraLookAt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.CameraLookAt ? g->host.renderer.CameraLookAt(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "CameraLookAt", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_camupd_RenderScreenFlash(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RenderScreenFlash ? g->host.renderer.RenderScreenFlash(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RenderScreenFlash", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_camupd_RenderCameraTransition(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RenderCameraTransition ? g->host.renderer.RenderCameraTransition(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RenderCameraTransition", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_coll_probe_PointInBoxXZ(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PointInBoxXZ", 0x80073B64u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_coll_probe_printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_coll_probe_HalfPipeRangeTest(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80050490u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_coll_probe_ColliderSlopeContactDir(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004F884u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_coll_probe_DirIndexFromFlags(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "DirIndexFromFlags", 0x8007774Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_coll_probe_PlayerSetClimbTarget(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
    return (int32_t)(ae_game_call6(g, 0x80066E28u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6, (uint32_t)p7));
}

static int32_t wire_coll_probe_PlayerSetRope(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80066E8Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_effects_EffectRingDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.EffectRingDraw ? g->host.renderer.EffectRingDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "EffectRingDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_effects_SpawnRandomParticle(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80070B70u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_effects_GpuProjectMesh(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.GpuProjectMesh ? g->host.renderer.GpuProjectMesh(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuProjectMesh", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_effects_GpuDrawMesh(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.GpuDrawMesh ? g->host.renderer.GpuDrawMesh(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuDrawMesh", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_effects_Gadget4TrailUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8006DAF8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_effects_GadgetFlashUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8006C6D4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint16_t wire_effects_LocalPositionPadding(void * p0, AeRam * p1, uint32_t p2, int32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)g;
    return (uint16_t)(0);
}

static int32_t wire_effects_GpuDrawSprite(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.GpuDrawSprite ? g->host.renderer.GpuDrawSprite(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuDrawSprite", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_effects_RibbonDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8006F3D0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_effects_SetRotMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetRotMatrix ? g->host.platform.SetRotMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_effects_SetTransMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetTransMatrix ? g->host.platform.SetTransMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_effects_GpuDrawTrailPoint(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.GpuDrawTrailPoint ? g->host.renderer.GpuDrawTrailPoint(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuDrawTrailPoint", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_effects_GpuDrawEffectObj(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.GpuDrawEffectObj ? g->host.renderer.GpuDrawEffectObj(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuDrawEffectObj", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_effects_SparkAddRandom(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8006E0DCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_effects_GpuDrawFxPlayerState6(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.GpuDrawFxPlayerState6 ? g->host.renderer.GpuDrawFxPlayerState6(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuDrawFxPlayerState6", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_effects_GpuDrawGroundSpark(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.GpuDrawGroundSpark ? g->host.renderer.GpuDrawGroundSpark(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuDrawGroundSpark", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static uint16_t wire_fxscript_position_padding(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)g;
    return (uint16_t)(0);
}

static int32_t wire_fxscript_SetRotMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.SetRotMatrix ? g->host.platform.SetRotMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_fxscript_SetTransMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.SetTransMatrix ? g->host.platform.SetTransMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_fxscript_ClearMemory(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.memset ? g->host.platform.memset(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_fxscript_ProjectEffectPoint(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.ProjectEffectPoint ? g->host.renderer.ProjectEffectPoint(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "ProjectEffectPoint", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_fxscript_DrawParticleQuad(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.DrawParticleQuad ? g->host.renderer.DrawParticleQuad(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawParticleQuad", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_fxscript_DrawRibbonStrip(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.DrawRibbonStrip ? g->host.renderer.DrawRibbonStrip(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawRibbonStrip", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_fxscript_Printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_fxscript_SoundLoopStop(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8008806Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_fxscript_CameraShake(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80048A00u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_fxscript_PadRumble(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80014168u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_fxscript_PlayerCanActivateGadget(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8007CA48u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_fxscript_TurnTowardsY(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80066C90u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_fxscript_StepToward(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x800463A4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_fxscript_SpawnRandomParticle(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80070B70u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_fxscript_ProjectileExpireCallback(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call(g, (uint32_t)p4, (uint32_t)p2, (uint32_t)p3, 0, 0));
}

static int32_t wire_fxscript_FUN_8014022c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "FUN_8014022c", 0x8014022Cu, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_fxscript_FUN_8014263c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "FUN_8014263c", 0x8014263Cu, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_fxscript_GetSolidHeight(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8004D8C0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_fxscript_GimmickActivate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8008FC30u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_fxscript_MeshInitFromAreaTable(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.MeshInitFromAreaTable ? g->host.renderer.MeshInitFromAreaTable(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "MeshInitFromAreaTable", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_fxscript_FUN_8013d358(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "FUN_8013d358", 0x8013D358u, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_fxscript_EventCamStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8004D39Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_fxscript_ModelTransformRigid(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.ModelTransformRigid ? g->host.renderer.ModelTransformRigid(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformRigid", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_fxscript_ModelDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.ModelDraw ? g->host.renderer.ModelDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gadget_PlayerCanExitStage(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8005DC78u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_IsPlayerSwimming(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80068B7Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_RibbonDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8006F3D0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_EffectSpawnFromDef(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80070284u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_SpawnParticle(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x800698B8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_TrailSpawn(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8006ED78u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_CameraGetSlotTarget(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "CameraGetSlotTarget", 0x8004B54Cu, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_gadget_TargetListFindBest(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "TargetListFindBest", 0x8007CDFCu, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_gadget_SdkVectorHelper_80031330(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.RotTransPers ? g->host.platform.RotTransPers(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_gadget_AimReticleDrawCircle(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "AimReticleDrawCircle", 0x80085E08u, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_gadget_PlayerSetState(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80064208u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_PadRumble(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80014168u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_DrawScaledModelAt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8007CAE0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_SdkVectorHelper_80030A30(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80030A30u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_SdkVectorHelper_80031830(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.MulMatrix0 ? g->host.platform.MulMatrix0(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_gadget_ModelTransformRigid(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.ModelTransformRigid ? g->host.renderer.ModelTransformRigid(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformRigid", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gadget_SetColorMatrixMode(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80015D58u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_ModelDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.ModelDraw ? g->host.renderer.ModelDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gadget_PlayerCanActivateGadget(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8007CA48u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_SoundLoopStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x800878E0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_SoundLoopStop(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8008806Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_SoundEmitterVolume(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "SoundEmitterVolume", 0x80087BA4u, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_gadget_MatrixRotYMul(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80014750u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_SdkVectorHelper_800305B0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.ScaleMatrixL ? g->host.platform.ScaleMatrixL(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_gadget_PlayerFxSetMatrixPos(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "PlayerFxSetMatrixPos", 0x8006CFBCu, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_gadget_RadarDrawModel(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8009A574u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_SdkVectorHelper_8002FB00(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.SetRotMatrix ? g->host.platform.SetRotMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_gadget_SdkVectorHelper_8002FB30(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.SetTransMatrix ? g->host.platform.SetTransMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_gadget_SdkVectorHelper_80031230(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.RotTransSV ? g->host.platform.RotTransSV(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_gadget_TrailRingPush(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "TrailRingPush", 0x8001F8B0u, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_gadget_TrailRingDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "TrailRingDraw", 0x8001F954u, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_gadget_DrawRibbonBetween(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.DrawRibbonBetween ? g->host.renderer.DrawRibbonBetween(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawRibbonBetween", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gadget_ProjectileGroupSpawnSpread(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80085630u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_PlayerSelectGadget(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8007AC64u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_AimTableClear3(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8007A304u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_GetSolidHeight(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8004D8C0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_PlaySfxAt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80087A20u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_GimmickActivate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8008FC30u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_MeshInitFromAreaTable(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.MeshInitFromAreaTable ? g->host.renderer.MeshInitFromAreaTable(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "MeshInitFromAreaTable", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gadget_AreaEffect(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_missing_words(g, "AreaEffect", 0x8013D358u, 5, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6}));
}

static int32_t wire_gadget_CameraShake(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80048A00u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_EventCamStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8004D39Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_PrintfError(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_gadget_GimmickTypeToSfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x8005BB98u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_PickupCollect(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80078A18u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_CameraNudge(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call5(g, 0x80047690u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_gadget_GpuDrawGadget7Trail(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.GpuDrawGadget7Trail ? g->host.renderer.GpuDrawGadget7Trail(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuDrawGadget7Trail", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gadget_GpuDrawGadget4Trail(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.GpuDrawGadget4Trail ? g->host.renderer.GpuDrawGadget4Trail(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuDrawGadget4Trail", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gadget_GpuProjectGadget3Aim(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.GpuProjectGadget3Aim ? g->host.renderer.GpuProjectGadget3Aim(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuProjectGadget3Aim", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gadget_GpuDrawGadget3AimLine(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(g->host.renderer.GpuDrawGadget3AimLine ? g->host.renderer.GpuDrawGadget3AimLine(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "GpuDrawGadget3AimLine", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gimmick_a_CameraNudge(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80047690u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_BodyInsideBox(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "BodyInsideBox", 0x8008EC70u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_GimmickTouchCheck(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "GimmickTouchCheck", 0x80090754u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_MeshInitFromAreaTable(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.MeshInitFromAreaTable ? g->host.renderer.MeshInitFromAreaTable(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "MeshInitFromAreaTable", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gimmick_a_PropSetMesh(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.PropSetMesh ? g->host.renderer.PropSetMesh(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "PropSetMesh", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gimmick_a_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_PlaySfxAt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80087A20u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_SoundLoopStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800878E0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_SoundEmitterPan(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80087B34u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_SoundLoopStop(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008806Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_ProgressFlagGet(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8009632Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_ProgressFlagSet(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "ProgressFlagSet", 0x800962F4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_ProgressGetByte(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800963ACu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_ProgressSetByte(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "ProgressSetByte", 0x80096360u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_GetSolidHeight(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004D8C0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_SwingDoorBodyDelta(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SwingDoorBodyDelta", 0x8008F308u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_SlideDoorMove(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SlideDoorMove", 0x8008FAD0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_a_GimmickHandler(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    return (int32_t)(ae_game_call(g, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0, 0));
}

static int32_t wire_gimmick_b_AreaMonkeyQuotaMet(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "AreaMonkeyQuotaMet", 0x80097454u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_80097504(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80097504", 0x80097504u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_SpecialActorPlaceAtCollider(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SpecialActorPlaceAtCollider", 0x8009B578u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_8009B3B0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8009B3B0", 0x8009B3B0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_IsTimeAttack(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80097678u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_Indicator_Register(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800716ECu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_Indicator_Activate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "Indicator_Activate", 0x800717B4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_SpawnEffectGroupAt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SpawnEffectGroupAt", 0x800698E4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_PadRumble(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80014168u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_8006AA60(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8006AA60", 0x8006AA60u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_MeshInitFromAreaTable(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.MeshInitFromAreaTable ? g->host.renderer.MeshInitFromAreaTable(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "MeshInitFromAreaTable", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gimmick_b_FUN_80091598(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80091598", 0x80091598u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_80091AF0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80091AF0", 0x80091AF0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_SoundLoopStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800878E0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_SoundEmitterPan(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80087B34u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_SoundLoopStop(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008806Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_80092324(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80092324", 0x80092324u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_80091C04(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80091C04", 0x80091C04u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_EventCamStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004D39Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_80093848(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80093848", 0x80093848u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_PlayerAttachSet(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PlayerAttachSet", 0x80066EECu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_80066F34(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80066F34", 0x80066F34u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_SvecToDiagMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800147D0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_80094330(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80094330", 0x80094330u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_DiagonalSignedDistance(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800774B8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_80094A48(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80094A48", 0x80094A48u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_FUN_80094E5C(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80094E5C", 0x80094E5Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_ColliderBoxContainsPoint(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "ColliderBoxContainsPoint", 0x8009091Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_b_CameraNudge(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80047690u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_PlaySfxAt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80087A20u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_gimmick_PropSetMesh(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.PropSetMesh ? g->host.renderer.PropSetMesh(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "PropSetMesh", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_gimmick_FUN_801414a0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_801414a0", 0x801414A0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static void wire_hud_draw_sprite(void * p0, AeRam * p1, const AeHudDraw * p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    if (g->host.hud_draw) g->host.hud_draw(g->host.user, g->ram, p2);
}

static void wire_hud_draw_quad(void * p0, AeRam * p1, const AeHudDraw * p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    if (g->host.hud_draw) g->host.hud_draw(g->host.user, g->ram, p2);
}

static void wire_hud_draw_glyph(void * p0, AeRam * p1, const AeHudDraw * p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    if (g->host.hud_draw) g->host.hud_draw(g->host.user, g->ram, p2);
}

static void wire_hud_draw_stage_overlay(void * p0, AeRam * p1, const AeHudDraw * p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    if (g->host.hud_draw) g->host.hud_draw(g->host.user, g->ram, p2);
}

static void wire_hud_draw_gauge(void * p0, AeRam * p1, const AeHudDraw * p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    if (g->host.hud_draw) g->host.hud_draw(g->host.user, g->ram, p2);
}

static void wire_hud_draw_message_frame(void * p0, AeRam * p1, const AeHudDraw * p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    if (g->host.hud_draw) g->host.hud_draw(g->host.user, g->ram, p2);
}

static int32_t wire_hud_Ovl_8013D424(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (int32_t)(ae_game_missing_call(g, "Ovl_8013D424", 0x8013D424u, (uint32_t)p2, 0, 0, 0));
}

static void wire_hud_DR_MOVE(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.platform.DR_MOVE ? g->host.platform.DR_MOVE(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static void wire_hud_ClearMemory(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)(g->host.platform.memset ? g->host.platform.memset(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_hud_ModelDrawScreenSpace(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelDrawScreenSpace ? g->host.renderer.ModelDrawScreenSpace(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDrawScreenSpace", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_items_init_GetLevelFirstArea(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8009764Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_items_init_printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static void wire_items_init_SetBackColor(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)(g->host.platform.SetBackColor ? g->host.platform.SetBackColor(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static void wire_items_init_SetFarColor(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)(g->host.platform.SetFarColor ? g->host.platform.SetFarColor(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static void wire_items_init_SetColorMatrix(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(g->host.platform.SetColorMatrix ? g->host.platform.SetColorMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_items_init_rsin(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (int32_t)(ae_game_call(g, 0x8002FB50u, (uint32_t)p2, 0, 0, 0));
}

static int32_t wire_items_init_rcos(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (int32_t)(ae_game_call(g, 0x8002FC20u, (uint32_t)p2, 0, 0, 0));
}

static uint32_t wire_menu_win_SetDrawMove(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.platform.DR_MOVE ? g->host.platform.DR_MOVE(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static uint32_t wire_menu_win_DrawPrim(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (uint32_t)(g->host.platform.DrawPrim ? g->host.platform.DrawPrim(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static uint32_t wire_menu_win_SetDrawArea(void * p0, AeRam * p1, uint32_t p2, uint32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    return (uint32_t)(ae_game_missing_call(g, "SetDrawArea", 0x80029458u, (uint32_t)p2, (uint32_t)p3, 0, 0));
}

static uint32_t wire_menu_win_SetDrawTPage(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.platform.SetDrawTPage ? g->host.platform.SetDrawTPage(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static uint32_t wire_menu_win_BeginFrame(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (uint32_t)(ae_game_missing_call(g, "BeginFrame", 0x8005DF48u, 0, 0, 0, 0));
}

static uint32_t wire_menu_win_ColorFadeReset(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (uint32_t)(ae_game_call(g, 0x8008C390u, 0, 0, 0, 0));
}

static uint32_t wire_menu_win_OtLinkSkipEmpty(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (uint32_t)(ae_game_missing_call(g, "OtLinkSkipEmpty", 0x8001F3A0u, (uint32_t)p2, 0, 0, 0));
}

static uint32_t wire_menu_win_DrawSync(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (uint32_t)(g->host.platform.DrawSync ? g->host.platform.DrawSync(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static uint32_t wire_menu_win_VSync(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (uint32_t)(g->host.platform.VSync ? g->host.platform.VSync(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static uint32_t wire_menu_win_PutDrawEnv(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (uint32_t)(g->host.platform.PutDrawEnv ? g->host.platform.PutDrawEnv(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static uint32_t wire_menu_win_PutDispEnv(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (uint32_t)(g->host.platform.PutDispEnv ? g->host.platform.PutDispEnv(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static uint32_t wire_menu_win_DrawOTag(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (uint32_t)(g->host.platform.DrawOTag ? g->host.platform.DrawOTag(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static uint32_t wire_menu_win_ModelTransformRigid(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    return (uint32_t)(g->host.renderer.ModelTransformRigid ? g->host.renderer.ModelTransformRigid(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformRigid", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0) : 0));
}

static uint32_t wire_menu_win_ModelDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    return (uint32_t)(g->host.renderer.ModelDraw ? g->host.renderer.ModelDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0) : 0));
}

static uint32_t wire_menu_win_DrawMenuChrome(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(ae_game_missing_call(g, "DrawMenuChrome", 0x8007D71Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_menu_win_DrawMenuRows(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(ae_game_missing_call(g, "DrawMenuRows", 0x8007D9F0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_menu_win_DrawMenuHighlight(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(ae_game_missing_call(g, "DrawMenuHighlight", 0x8007E030u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_menu_win_DrawMenuItem(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(ae_game_missing_call(g, "DrawMenuItem", 0x8007E1B8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_menu_win_DrawMenuSelection(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(ae_game_missing_call(g, "DrawMenuSelection", 0x8007E3F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_menu_win_DrawMenuCursor(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(ae_game_missing_call(g, "DrawMenuCursor", 0x8007F148u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_menu_win_item_callback(void * p0, AeRam * p1, uint32_t p2, uint32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    return (uint32_t)(ae_game_missing_call(g, "item_callback", 0x00000000u, (uint32_t)p2, (uint32_t)p3, 0, 0));
}

static int32_t wire_misc_MulMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.MulMatrix ? g->host.platform.MulMatrix(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_misc_SetRotMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetRotMatrix ? g->host.platform.SetRotMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_misc_SetTransMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetTransMatrix ? g->host.platform.SetTransMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_misc_RotTransPers(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.RotTransPers ? g->host.platform.RotTransPers(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_misc_ModelDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ItemDrawModel ? g->host.renderer.ItemDrawModel(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ItemDrawModel", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_misc_ModelDrawWithOt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80017D94u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_ModelTransformRigid(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelTransformRigid ? g->host.renderer.ModelTransformRigid(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformRigid", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_misc_ModelTransformSkinned(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelTransformSkinned ? g->host.renderer.ModelTransformSkinned(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformSkinned", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_misc_ModelDrawEnvMap(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelDrawEnvMap ? g->host.renderer.ModelDrawEnvMap(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDrawEnvMap", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_misc_DrawShadow(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.DrawShadow ? g->host.renderer.DrawShadow(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawShadow", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_misc_RenderEnvUV(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RenderEnvUV ? g->host.renderer.RenderEnvUV(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RenderEnvUV", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_misc_RenderIndicator(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RenderIndicator ? g->host.renderer.RenderIndicator(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RenderIndicator", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_misc_RenderHudPopup(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RenderHudPopup ? g->host.renderer.RenderHudPopup(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RenderHudPopup", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_misc_CopyPlayerEyeTexture(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80067C08u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_HudShowHealth(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005E42Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_IndicatorDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "IndicatorDraw", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_IndicatorRegister(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "IndicatorRegister", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_IndicatorActivate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "IndicatorActivate", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_SpecialActorDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SpecialActorDraw", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_SpawnParticle(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800698B8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_TrailSpawn(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8006ED78u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_RotTransPersResult(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.RotTransPers ? g->host.platform.RotTransPers(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_misc_FUN_8013f464(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013f464", 0x8013F464u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_8013fa2c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013fa2c", 0x8013FA2Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_8013fd74(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013fd74", 0x8013FD74u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_80140a60(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80140a60", 0x80140A60u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_80140d60(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80140d60", 0x80140D60u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_GetGroundHeight(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80043400u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_ColliderTypeActive(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80079A28u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_IsTimeAttack(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80097678u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_HeapFree(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008C064u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_Printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_misc_SoundLoopStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800878E0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_SoundLoopStop(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008806Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_SpuWaitIdle(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SpuWaitIdle ? g->host.platform.SpuWaitIdle(g->host.user, g->ram) : 0);
}

static int32_t wire_misc_SpuVolumeClamp(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800A119Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_ColliderScriptMotion(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008B980u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_ColliderScriptUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, ae_rd32(g->ram, 0x800B3344u + ae_rd8(g->ram, (uint32_t)p2 + 0x12u) * 4u), (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_PadRumbleReset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80014208u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_ScreenBandDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ScreenBandDraw ? g->host.renderer.ScreenBandDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ScreenBandDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_misc_SetDrawMove(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.DR_MOVE ? g->host.platform.DR_MOVE(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_misc_exit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Exit ? g->host.platform.Exit(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_misc_FUN_801402d8(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_801402d8", 0x801402D8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_Area53_BossUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "Area53_BossUpdate", 0x8013D57Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_80143854(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80143854", 0x80143854u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_Area46_BossUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "Area46_BossUpdate", 0x8013F1ACu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_Area57_BossUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "Area57_BossUpdate", 0x8013E4B4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_80137248(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80137248", 0x80137248u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_8013fc18(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013fc18", 0x8013FC18u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_8013fe4c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013fe4c", 0x8013FE4Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_80088d78(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80088d78", 0x80088D78u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_80088f2c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80088f2c", 0x80088F2Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_800895f0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_800895f0", 0x800895F0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_80089b10(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80089b10", 0x80089B10u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_80089d64(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80089d64", 0x80089D64u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_80089d6c(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80089d6c", 0x80089D6Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_FUN_8008aa64(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8008aa64", 0x8008AA64u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_GetSolidHeight(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004D8C0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_GetColliderQuadrantMaskXZ(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800889B4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_CdIntToPos(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.CdIntToPos ? g->host.platform.CdIntToPos(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_misc_CdControl(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.CdControl ? g->host.platform.CdControl(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_misc_CdReadyCallback(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.CdReadyCallback ? g->host.platform.CdReadyCallback(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_misc_VSync(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.VSync ? g->host.platform.VSync(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_misc_AnimFramePtr(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80015240u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_CollisionProbe0(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CollisionProbe0", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_CollisionProbe1(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CollisionProbe1", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_CollisionProbe2(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CollisionProbe2", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_CollisionProbe3(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CollisionProbe3", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_CollisionProbe4(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CollisionProbe4", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_CollisionProbe5(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CollisionProbe5", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_CollisionProbe6(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CollisionProbe6", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_CollisionProbe7(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "CollisionProbe7", 0x00000000u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_PointInBoxXZ(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PointInBoxXZ", 0x80073B64u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_HalfPipeRangeTest(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80050490u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_ColliderSlopeContactDir(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004F884u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_DirIndexFromFlags(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "DirIndexFromFlags", 0x8007774Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_misc_PlayerSetClimbTarget(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
    return (int32_t)(ae_game_call6(g, 0x80066E28u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6, (uint32_t)p7));
}

static int32_t wire_misc_PlayerSetRope(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80066E8Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_Printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_monkey_GimmickGetInfo(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005B7BCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_PlaySfxAt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80087A20u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ProjectileSpawn(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80059304u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_SpawnParticle(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800698B8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_TargetListAdd(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007CD88u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_TargetListRemove(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007CDC4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_MatrixFromDirXZ(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80014804u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ModelPoseBones(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80015780u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ModelTransformRigid(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelTransformRigid ? g->host.renderer.ModelTransformRigid(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformRigid", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_monkey_ModelDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelDraw ? g->host.renderer.ModelDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_monkey_MenuItemInitBound(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007D1E0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_MenuItemSetRange(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007D290u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_MenuItemSetCallback(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007D29Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_MenuItemSetStep(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007D2A4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_EffectBigBurst(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8006B3F0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_SparkAddRandom(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8006E0DCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ModelInit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80015AC0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_SetRotMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetRotMatrix ? g->host.platform.SetRotMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_monkey_SetTransMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetTransMatrix ? g->host.platform.SetTransMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_monkey_RotTransPers(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.RotTransPers ? g->host.platform.RotTransPers(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_monkey_GetGroundHeight(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80043400u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ObjSlotGetAnimIndex(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80052C4Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_AlertIconDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.AlertIconDraw ? g->host.renderer.AlertIconDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "AlertIconDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_monkey_ObjSlotSetField_CF(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80052CA0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ObjSlotGetTarget(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80052D04u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_SoundLoopStartEx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800882C4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_SoundLoopStopAndFree(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800880D0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_SpawnPuff(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8006B3CCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_MatrixLookDir(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8001484Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ScaleMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80030E60u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ModelBuildBoneMatrices(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80015504u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ModelTransformSkinned(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelTransformSkinned ? g->host.renderer.ModelTransformSkinned(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformSkinned", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_monkey_ModelDrawWithOt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80017D94u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_GimmickEvent(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005B880u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ActorAnimDecode(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80014E34u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_DrawShadow(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.DrawShadow ? g->host.renderer.DrawShadow(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawShadow", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_monkey_ColorFadeSet(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008C3C0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_DebugTextPrintfWhite(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008CBFCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_ClearWords(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80032708u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_monkey_VSync(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.VSync ? g->host.platform.VSync(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_monkey_StageExitIsActive(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005DCE4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pad_pad_get_state(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (int32_t)(g->host.platform.PadGetState ? g->host.platform.PadGetState(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_pad_pad_info_mode(void * p0, AeRam * p1, int32_t p2, int32_t p3, int32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    return (int32_t)(g->host.platform.PadInfoMode ? g->host.platform.PadInfoMode(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static void wire_pad_pad_setup_dualshock(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_missing_call(g, "PadSetupDualShock", 0x80012CA0u, (uint32_t)p2, 0, 0, 0));
}

static int32_t wire_pad_pad_demo_step(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (int32_t)(ae_game_missing_call(g, "PadDemoPlaybackStep", 0x80013128u, 0, 0, 0, 0));
}

static void wire_pad_stage_request(void * p0, AeRam * p1, int32_t p2, int32_t p3, int32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)(ae_game_call(g, 0x8005D100u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0));
}

static uint32_t wire_pad_stick_to_buttons(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (uint32_t)(ae_game_missing_call(g, "StickToButtons", 0x800140B4u, 0, 0, 0, 0));
}

static void wire_pad_pad_rumble_reset(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, 0x80014208u, (uint32_t)p2, 0, 0, 0));
}

static int32_t wire_pause_menu_WaitFrameFlip(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "WaitFrameFlip", 0x8005DE8Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_PauseMenuOverlay1B(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PauseMenuOverlay1B", 0x8013FB20u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_DrawSync(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.DrawSync ? g->host.platform.DrawSync(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_pause_menu_VSync(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.VSync ? g->host.platform.VSync(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_pause_menu_PutDispEnv(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.PutDispEnv ? g->host.platform.PutDispEnv(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_pause_menu_PutDrawEnv(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.PutDrawEnv ? g->host.platform.PutDrawEnv(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_pause_menu_StoreImage(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.StoreImage ? g->host.platform.StoreImage(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_pause_menu_LoadImage(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.LoadImage ? g->host.platform.LoadImage(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_pause_menu_ClearOTagR(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.ClearOTagR ? g->host.platform.ClearOTagR(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_pause_menu_DrawOTag(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.DrawOTag ? g->host.platform.DrawOTag(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_pause_menu_HeapAlloc(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008C09Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HeapFree(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008C064u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_ColorFadeReset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008C390u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_PadDirectionCode(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PadDirectionCode", 0x8007F40Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_TextSetCursor(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008DDB0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_PauseMenuInitVolumes(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PauseMenuInitVolumes", 0x80082D40u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudTextWidth(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "HudTextWidth", 0x80082230u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_DrawTextSprites(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80082288u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_DrawMenuArrows(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "DrawMenuArrows", 0x8008211Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_IsTimeAttack(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80097678u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudPanelA_Start(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80095244u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudPanelB_Start(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80095600u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudCounterC_Start(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800958C0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudDrawGadgetSlots(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80081664u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_GadgetSlotRemap(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80081144u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudDrawGadgetSlotsFrame(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80081180u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_SetPriority(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SetPriority", 0x80012C70u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_CutsceneModeOffCaller(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005DD28u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_CutsceneModeOn(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005DD18u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_PauseDrawCounter(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PauseDrawCounter", 0x80082A04u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_PauseDrawCaptureGrid(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PauseDrawCaptureGrid", 0x80082398u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_PauseMenuDrawSubScreen(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PauseMenuDrawSubScreen", 0x80082D6Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudDrawIcon16(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005FCA4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudDrawTime(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005F978u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_GetDAT_800B01C2(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005DD34u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudDrawTimerGauge(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005FFD4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudPanelA_DrawIcon(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005F2E0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudDrawHealth(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005E4A8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudPanelB_DrawSprite(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005F5FCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudDrawDigit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005F770u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudDrawNumber(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005F87Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudCounterC_Draw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005FD80u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_HudDrawGadgetIcons(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80046BC4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_CdMgrUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.archive.CdMgrUpdate ? g->host.archive.CdMgrUpdate(g->host.user, g->ram) : 0);
}

static int32_t wire_pause_menu_Printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_pause_menu_SoundLoopStop(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008806Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_SfxVoiceSetPanVol(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SfxVoiceSetPanVol ? g->host.platform.SfxVoiceSetPanVol(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_pause_menu_TextNextGlyph(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008DD68u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_FontDrawGlyph(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008D9D0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_RotMatrixZ(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800306F0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_RotMatrixY(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80030890u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_RotMatrixX(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80030A30u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_SetRotMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetRotMatrix ? g->host.platform.SetRotMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_pause_menu_SetTransMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetTransMatrix ? g->host.platform.SetTransMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_pause_menu_RotTrans(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.RotTrans ? g->host.platform.RotTrans(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_pause_menu_RotTransSV(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.RotTransSV ? g->host.platform.RotTransSV(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_pause_menu_Memset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.memset ? g->host.platform.memset(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_pause_menu_ModelTransformRigid(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelTransformRigid ? g->host.renderer.ModelTransformRigid(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformRigid", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_pause_menu_ModelDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelDraw ? g->host.renderer.ModelDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_pause_menu_ModelEmitPrimsScratchColors(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelEmitPrimsScratchColors ? g->host.renderer.ModelEmitPrimsScratchColors(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelEmitPrimsScratchColors", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_pause_menu_ModelDrawScreenSpace(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelDrawScreenSpace ? g->host.renderer.ModelDrawScreenSpace(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDrawScreenSpace", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_pause_menu_GadgetDrawSlot0Attachment(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8009ABF4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_pause_menu_RendererIrgbToIr123(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RendererIrgbToIr123 ? g->host.renderer.RendererIrgbToIr123(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RendererIrgbToIr123", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_pause_menu_RendererPackIrgb(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RendererPackIrgb ? g->host.renderer.RendererPackIrgb(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RendererPackIrgb", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_pause_menu_RendererGetProjectionDistance(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RendererGetProjectionDistance ? g->host.renderer.RendererGetProjectionDistance(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RendererGetProjectionDistance", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_pause_menu_RendererSetProjectionDistance(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RendererSetProjectionDistance ? g->host.renderer.RendererSetProjectionDistance(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RendererSetProjectionDistance", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_pause_menu_RendererHudDrawWindowFrame(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RendererHudDrawWindowFrame ? g->host.renderer.RendererHudDrawWindowFrame(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RendererHudDrawWindowFrame", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_pause_menu_RendererPauseMenuDrawButtons(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RendererPauseMenuDrawButtons ? g->host.renderer.RendererPauseMenuDrawButtons(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RendererPauseMenuDrawButtons", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_pause_menu_RendererPauseMenuDrawGadgetScreen(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RendererPauseMenuDrawGadgetScreen ? g->host.renderer.RendererPauseMenuDrawGadgetScreen(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RendererPauseMenuDrawGadgetScreen", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static uint32_t wire_player1_CdIntToPos(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.platform.CdIntToPos ? g->host.platform.CdIntToPos(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static uint32_t wire_player1_CdPosToInt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.platform.CdPosToInt ? g->host.platform.CdPosToInt(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static uint32_t wire_player1_CdControl(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.platform.CdControl ? g->host.platform.CdControl(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static uint32_t wire_player1_CdReadyCallback(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.platform.CdReadyCallback ? g->host.platform.CdReadyCallback(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static uint32_t wire_player1_VSync(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.platform.VSync ? g->host.platform.VSync(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static uint32_t wire_player1_printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static uint32_t wire_player1_SoundSetVolumeMode(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(ae_game_call(g, 0x80086248u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_player1_EventCamCreate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(ae_game_call(g, 0x8004BB68u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_player1_ModelEmitPrimsCycleColors(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.renderer.ModelEmitPrimsCycleColors ? g->host.renderer.ModelEmitPrimsCycleColors(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelEmitPrimsCycleColors", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static uint32_t wire_player1_DrawRibbonBetween(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.renderer.DrawRibbonBetween ? g->host.renderer.DrawRibbonBetween(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawRibbonBetween", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static uint32_t wire_player1_ModelProjectVertices(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.renderer.ModelProjectVertices ? g->host.renderer.ModelProjectVertices(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelProjectVertices", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static uint32_t wire_player1_ModelEmitPrims(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(g->host.renderer.ModelEmitPrims ? g->host.renderer.ModelEmitPrims(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelEmitPrims", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static uint16_t wire_player2_position_padding(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)g;
    return (uint16_t)(0);
}

static void wire_player2_PlaySfx(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, 0, 0, 0));
}

static void wire_player2_AimModeLeave(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x8004B510u, 0, 0, 0, 0));
}

static void wire_player2_PlayerStopGadgetSounds(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, 0x8007A180u, (uint32_t)p2, 0, 0, 0));
}

static int32_t wire_player2_PlayerCanActivateGadget(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (int32_t)(ae_game_call(g, 0x8007CA48u, (uint32_t)p2, 0, 0, 0));
}

static int32_t wire_player2_PlayerCondition(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (int32_t)(ae_game_call(g, (uint32_t)p2, 0, 0, 0, 0));
}

static void wire_player2_PlayerAction(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, (uint32_t)p2, 0, 0, 0, 0));
}

static void wire_player2_AnimDecodeFrame(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(ae_game_call(g, 0x80014D70u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static void wire_player2_MatrixFromFrontUp(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(ae_game_call(g, 0x80014A84u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static void wire_player2_ScaleMatrixL(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.platform.ScaleMatrixL ? g->host.platform.ScaleMatrixL(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static void wire_player2_ModelTransformSkinned(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.renderer.ModelTransformSkinned ? g->host.renderer.ModelTransformSkinned(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformSkinned", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static void wire_player2_ModelPrepCycleColors(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(ae_game_call(g, 0x8001D4A8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static void wire_player2_ModelEmitPrimsCycleColors(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.renderer.ModelEmitPrimsCycleColors ? g->host.renderer.ModelEmitPrimsCycleColors(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelEmitPrimsCycleColors", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static void wire_player2_ModelDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.renderer.ModelDraw ? g->host.renderer.ModelDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static void wire_player2_DrawRibbonBetween(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.renderer.DrawRibbonBetween ? g->host.renderer.DrawRibbonBetween(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawRibbonBetween", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static void wire_player2_AimModeEnter(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x8004B4B8u, 0, 0, 0, 0));
}

static void wire_player2_TrailRingReset(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x8001F884u, 0, 0, 0, 0));
}

static void wire_player2_CameraFixedModeOff(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x80048748u, 0, 0, 0, 0));
}

static void wire_player2_AimRequestClear(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x80048430u, 0, 0, 0, 0));
}

static void wire_player2_TurnTowardsY(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(ae_game_call(g, 0x80066C90u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static void wire_player2_PadRumble(void * p0, AeRam * p1, int32_t p2, int32_t p3, int32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)(ae_game_call(g, 0x80014168u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0));
}

static void wire_player2_MatrixLookDir(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(ae_game_call(g, 0x8001484Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static void wire_player2_ModelTransformRigid(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.renderer.ModelTransformRigid ? g->host.renderer.ModelTransformRigid(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformRigid", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static void wire_player2_ModelDrawWithOt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(ae_game_call(g, 0x80017D94u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static void wire_player2_ActorAnimDecodeEntry(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(ae_game_call(g, 0x80014EF0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static void wire_player2_SetRotMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.platform.SetRotMatrix ? g->host.platform.SetRotMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static void wire_player2_SetTransMatrix(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.platform.SetTransMatrix ? g->host.platform.SetTransMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static void wire_player2_RotTransSV(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.platform.RotTransSV ? g->host.platform.RotTransSV(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static void wire_player2_RotTransPers(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.platform.RotTransPers ? g->host.platform.RotTransPers(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static void wire_player2_RotMatrixY(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(ae_game_call(g, 0x80030890u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static void wire_player2_EventCamStartRandom(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x8004BC9Cu, 0, 0, 0, 0));
}

static void wire_player_act_AimRequestClear(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x80048430u, 0, 0, 0, 0));
}

static int32_t wire_player_act_PlayerCond_JumpHeld(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (int32_t)(ae_game_call(g, 0x80064AF8u, 0, 0, 0, 0));
}

static int32_t wire_player_act_PlayerInput_StickGesture(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (int32_t)(ae_game_missing_call(g, "PlayerInput_StickGesture", 0x800650D8u, 0, 0, 0, 0));
}

static void wire_player_act_PadRumble(void * p0, AeRam * p1, int32_t p2, int32_t p3, int32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)(ae_game_call(g, 0x80014168u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0));
}

static void wire_player_act_Gadget3AimUpdateAmmo(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x8007AA98u, 0, 0, 0, 0));
}

static void wire_player_act_AimModeEnter(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x8004B4B8u, 0, 0, 0, 0));
}

static void wire_player_act_AimModeLeave(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x8004B510u, 0, 0, 0, 0));
}

static void wire_player_act_PlayPlayerVoice(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, 0x8006896Cu, (uint32_t)p2, 0, 0, 0));
}

static void wire_player_act_EffectGroundImpactAtPlayer(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x8006BF84u, 0, 0, 0, 0));
}

static void wire_player_act_PlayerDamage(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, 0x8005E6F0u, (uint32_t)p2, 0, 0, 0));
}

static void wire_player_act_PlaySfx(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, 0, 0, 0));
}

static void wire_player_act_StageRequest(void * p0, AeRam * p1, int32_t p2, int32_t p3, int32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)(ae_game_call(g, 0x8005D100u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0));
}

static void wire_player_act_EffectBigBurst(void * p0, AeRam * p1, int32_t p2, int32_t p3, int32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)(ae_game_call(g, 0x8006B3F0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0));
}

static int32_t wire_player_act_StageGoalReached(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (int32_t)(ae_game_missing_call(g, "StageGoalReached", 0x80097A60u, 0, 0, 0, 0));
}

static void wire_player_act_HudPanelA_Start(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, 0x80095244u, (uint32_t)p2, 0, 0, 0));
}

static void wire_player_act_HeapFree(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, 0x8008C064u, (uint32_t)p2, 0, 0, 0));
}

static void wire_player_act_CameraAreaScriptDispatch(void * p0, AeRam * p1, int32_t p2, int32_t p3, uint32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)(ae_game_missing_call(g, "CameraAreaScriptDispatch", 0x8004B590u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0));
}

static void wire_player_act_PlayerSelectGadget(void * p0, AeRam * p1, int32_t p2, int32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)(ae_game_call(g, 0x8007AC64u, (uint32_t)p2, (uint32_t)p3, 0, 0));
}

static void wire_player_act_PlayerIdleModelSetup(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, 0x80032ADCu, (uint32_t)p2, 0, 0, 0));
}

static uint32_t wire_player_act_AnimFramePtr(void * p0, AeRam * p1, uint32_t p2, int32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    return (uint32_t)(ae_game_call(g, 0x80015240u, (uint32_t)p2, (uint32_t)p3, 0, 0));
}

static void wire_player_act_DR_MOVE(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, int32_t p4, int32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)(g->host.platform.DR_MOVE ? g->host.platform.DR_MOVE(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static void wire_player_act_PlayerModeCallback(void * p0, AeRam * p1, uint32_t p2, int32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)(ae_game_call(g, (uint32_t)p2, (uint32_t)p3, 0, 0, 0));
}

static int32_t wire_player_cond_PlayerIsOnGround(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (int32_t)(ae_game_call(g, 0x800652E4u, 0, 0, 0, 0));
}

static void wire_player_cond_PlayerSplash(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x8006B950u, 0, 0, 0, 0));
}

static void wire_player_cond_PlayerFxJumpBurst(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_missing_call(g, "PlayerFxJumpBurst", 0x8006C58Cu, 0, 0, 0, 0));
}

static uint32_t wire_player_spawn_AreaBumpAlloc(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (uint32_t)(ae_game_call(g, 0x8008C1B8u, (uint32_t)p2, 0, 0, 0));
}

static int32_t wire_player_spawn_printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static void wire_player_spawn_CameraLookAt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, int32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)(g->host.renderer.CameraLookAt ? g->host.renderer.CameraLookAt(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6) : (g->host.render ? g->host.render(g->host.user, g->ram, "CameraLookAt", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static void wire_player_spawn_SetGeomScreen(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(g->host.platform.SetGeomScreen ? g->host.platform.SetGeomScreen(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_player_update_frame_XaPlay(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (int32_t)(ae_game_call(g, 0x80011F6Cu, (uint32_t)p2, 0, 0, 0));
}

static int32_t wire_player_update_frame_CdMgrQueueCommand(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.archive.CdMgrQueueCommand ? g->host.archive.CdMgrQueueCommand(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_player_update_frame_CdIntToPos(void * p0, AeRam * p1, uint32_t p2, uint32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    return (int32_t)(g->host.platform.CdIntToPos ? g->host.platform.CdIntToPos(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_player_update_frame_CdPosToInt(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (int32_t)(g->host.platform.CdPosToInt ? g->host.platform.CdPosToInt(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_player_update_frame_memset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    return (int32_t)(g->host.platform.memset ? g->host.platform.memset(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static void wire_player_update_frame_PlayerAnimateAndDraw(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_call(g, 0x80063628u, 0, 0, 0, 0));
}

static void wire_player_update_frame_FUN_80142808(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)(ae_game_missing_call(g, "FUN_80142808", 0x80142808u, 0, 0, 0, 0));
}

static void wire_player_update_frame_PlayerModeCallback(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, (uint32_t)p2, 0, 0, 0, 0));
}

static void wire_player_update_frame_PlayerAction(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_call(g, (uint32_t)p2, 0, 0, 0, 0));
}

static int32_t wire_player_update_frame_PlayerCheckGadgetSelect(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (int32_t)(ae_game_call(g, 0x80067974u, 0, 0, 0, 0));
}

static uint32_t wire_player_update_frame_PlayerVoiceFallback(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (uint32_t)(g->host.player_voice_fallback ? g->host.player_voice_fallback(g->host.user, g->ram) : 0x8Au);
}

static uint32_t wire_player_update_frame_PlayerUpdateLocalFrame(void * p0, AeRam * p1) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    return (uint32_t)(g->host.player_update_locals ? g->host.player_update_locals(g->host.user, g->ram) : g->call_stack_pointer - 120u);
}

static int32_t wire_player_update_RotMatrixY(void * p0, AeRam * p1, uint32_t p2, uint32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    return (int32_t)(ae_game_call(g, 0x80030890u, (uint32_t)p2, (uint32_t)p3, 0, 0));
}

static int32_t wire_progress_SoundLoopsSetPan(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80086788u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_progress_FUN_8013FB20(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013FB20", 0x8013FB20u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_progress_SoundBankSetVolume(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SoundBankSetVolume ? g->host.platform.SoundBankSetVolume(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_progress_SetDrawTPage(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetDrawTPage ? g->host.platform.SetDrawTPage(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_spawn_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_spawn_MatrixLookDir(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8001484Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_spawn_MatrixFromDirXZ(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80014804u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint16_t wire_spawn_position_padding(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)g;
    return (uint16_t)(0);
}

static int32_t wire_stage_fade_DebugTextPrintf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008C6F0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_fade_SeqTrackClose(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SeqTrackClose", 0x800A1F3Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_fade_SeqTrackSetPlayState(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SeqTrackSetPlayState", 0x800A2B6Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_fade_SoundBankFree(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SoundBankFree", 0x800A1CCCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_fade_SoundStopAllVoices(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SoundStopAllVoices", 0x800A342Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_fade_RenderScreenFlash(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RenderScreenFlash ? g->host.renderer.RenderScreenFlash(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RenderScreenFlash", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_stage_fade_RenderFadeSpiral(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.RenderFadeSpiral ? g->host.renderer.RenderFadeSpiral(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "RenderFadeSpiral", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_stage_AreaUsesAltMember(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "AreaUsesAltMember", 0x800115F4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ArcGetMemberPos(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80012C0Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_FUN_80025E24(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.archive.FUN_80025E24 ? g->host.archive.FUN_80025E24(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_stage_CdMgrQueueCommand(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.archive.CdMgrQueueCommand ? g->host.archive.CdMgrQueueCommand(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_stage_StageExitStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "StageExitStart", 0x8004D73Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_IsTimeAttack(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80097678u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_GetLevelFirstArea(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8009764Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_GimmickSaveStates(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005ADF8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_GimmickSaveApplyProgress(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005B480u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ItemSaveCollectedMask(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "ItemSaveCollectedMask", 0x800847ECu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ZakoSaveDefeatedMask(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "ZakoSaveDefeatedMask", 0x800509CCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaGetKind(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80010CA4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaGetLevel(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80010CC0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_GimmickSaveTableReset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005AD90u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ItemsClearFlags(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "ItemsClearFlags", 0x800847C4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ZakoClearSlotTable(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800509A4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_SoundSilence(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "SoundSilence", 0x80086148u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_LevelMonkeyCountGet(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "LevelMonkeyCountGet", 0x800965ECu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaBumpAlloc(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008C1B8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_MeshInit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.MeshInit ? g->host.renderer.MeshInit(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "MeshInit", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_stage_GetZoneMask(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004626Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_FUN_80030890(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80030890u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_FUN_800A3700(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Exit ? g->host.platform.Exit(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_stage_HeapInit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008C01Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaBumpAllocSet(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008C058u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaSpecialFlagsClear(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007AFB8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaSetVramAnimList(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800690E8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaSetZoneBoxes(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80043380u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_IndicatorsReset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800715A0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaSetFeature30(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8009B7C8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaSetSpecialActor(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8009B3CCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaSetIndicatorModelA(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007163Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaSetIndicatorModelB(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80071694u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaSetFeature31(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8009B7E4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaSetFeature32(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8009B7F0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_SoundAreaKeepsMusic(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80086F00u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_BgmStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80087388u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_BgmUpdateVolume(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008745Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ApplyLevelFog(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800323F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_SetColorMatrixMode(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80015D58u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_SoundLoopsStopAll(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80086540u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_BuildTerrainBlockMask(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800438F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_TableClearDAT_800BEF60(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007D0F0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_DebugMenuInitPlayerParams(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005CB1Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_DebugMenuInitCounters(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005CC58u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_DebugMenuInitCounters2(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005CCDCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_MessageBoxClose(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008DEE0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_PlayerSpawnAtEntry(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80032BBCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_TargetListClear(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007CD60u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ZakoInit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800505C8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_MonkeyLodModelsInit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007BCB4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_FxSlotsReset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80084A60u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ProjectilesReset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800592E0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_EffectsReset(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80069500u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ColliderZoneMasksUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80088670u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_GimmickSpawnAll(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005B538u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_BossInit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005AB90u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ItemsLoadForArea(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80083470u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaTexCopyInit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005E7ECu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_RcCarDespawn(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8009946Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_PlayerSelectGadget(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007AC64u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_HudFindGadgetIconPos(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008197Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_MonkeyClearWords(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80032708u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_VSync(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.VSync ? g->host.platform.VSync(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_stage_Printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_stage_CutsceneModeOff(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005DD28u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_CutsceneModeOn(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8005DD18u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_PlayerInit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PlayerInit", 0x8003272Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_PlayerSetMode(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007B374u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_FUN_80028850(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.ClearImage ? g->host.platform.ClearImage(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_stage_FUN_800286CC(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.DrawSync ? g->host.platform.DrawSync(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_stage_WaitFrameFlip(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "WaitFrameFlip", 0x8005DE8Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_BgmFadeOut(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "BgmFadeOut", 0x8008760Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_SoundStop(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80086FD0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_ProgressSetByte(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "ProgressSetByte", 0x80096360u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_PlayTimeAdd(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PlayTimeAdd", 0x8005DD44u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_StageGradeUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "StageGradeUpdate", 0x80097BFCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_StageVictoryStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "StageVictoryStart", 0x8009543Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_StageTypeClearInit(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "StageTypeClearInit", 0x80094F48u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_StageTypeClearUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "StageTypeClearUpdate", 0x80095030u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_StageIntroStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "StageIntroStart", 0x8009546Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_PlayerEnterCutsceneMode(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "PlayerEnterCutsceneMode", 0x80095588u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_StageIntroUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "StageIntroUpdate", 0x800954C4u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_GimmickNopUpdate3(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80094F40u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_GimmickNopUpdate4(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80095464u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_XaPlay(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80011F6Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AreaCheckExits(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "AreaCheckExits", 0x80010D80u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_CdMgrUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.archive.CdMgrUpdate ? g->host.archive.CdMgrUpdate(g->host.user, g->ram) : 0);
}

static int32_t wire_stage_FadeFrame(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FadeFrame", 0x8007A14Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_LoadListStep(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.archive.LoadListStep ? g->host.archive.LoadListStep(g->host.user, g->ram) : 0);
}

static int32_t wire_stage_FUN_80096638(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_80096638", 0x80096638u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_FUN_8013EA24(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013EA24", 0x8013EA24u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AimRequestClear(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80048430u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_stage_AimModeLeave(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004B510u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_tex_copy_HeapAlloc(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (uint32_t)(ae_game_call(g, 0x8008C09Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_tex_copy_DrawSync(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (int32_t)(g->host.platform.DrawSync ? g->host.platform.DrawSync(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_tex_copy_LoadImage(void * p0, AeRam * p1, uint32_t p2, uint32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    return (int32_t)(g->host.platform.LoadImage ? g->host.platform.LoadImage(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_tex_copy_StoreImage(void * p0, AeRam * p1, uint32_t p2, uint32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    return (int32_t)(g->host.platform.StoreImage ? g->host.platform.StoreImage(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3) : 0);
}

static int32_t wire_vram_anim_MoveImage(void * p0, AeRam * p1, uint32_t p2, int32_t p3, int32_t p4) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    return (int32_t)(g->host.platform.MoveImage ? g->host.platform.MoveImage(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_world_GetSolidHeight(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004D8C0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_world_PlaySfxAt(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80087A20u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_world_GimmickActivate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008FC30u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_world_MeshInitFromAreaTable(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.MeshInitFromAreaTable ? g->host.renderer.MeshInitFromAreaTable(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "MeshInitFromAreaTable", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_world_FUN_8013d358(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013d358", 0x8013D358u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_world_CameraShake(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80048A00u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_world_EventCamStart(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8004D39Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako1_ProjectilesUpdateDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80059B08u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako1_Draw_Nop(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.Draw_Nop ? g->host.renderer.Draw_Nop(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "Draw_Nop", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako1_FUN_8002FB00(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetRotMatrix ? g->host.platform.SetRotMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_zako1_FUN_8002FB30(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.SetTransMatrix ? g->host.platform.SetTransMatrix(g->host.user, g->ram, (uint32_t)p2) : 0);
}

static int32_t wire_zako1_FUN_80031330(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.RotTransPers ? g->host.platform.RotTransPers(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_zako1_Atan2Safe(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80046340u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako1_FUN_800A3740(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.memset ? g->host.platform.memset(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4) : 0);
}

static int32_t wire_zako1_SpawnPickups(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x80083AA8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako1_FUN_8013F660(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013F660", 0x8013F660u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako1_ZakoTypeUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call(g, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_zako1_AreaModeUpdate(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call(g, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_zako1_ZakoFrameHook(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    return (int32_t)(ae_game_call(g, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6));
}

static int32_t wire_zako1_DrawShadow(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.DrawShadow ? g->host.renderer.DrawShadow(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawShadow", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako1_ModelTransformSkinned(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelTransformSkinned ? g->host.renderer.ModelTransformSkinned(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformSkinned", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako1_ModelPrepCycleColors(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8001D4A8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako1_ModelEmitPrims(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelEmitPrims ? g->host.renderer.ModelEmitPrims(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelEmitPrims", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako1_ModelEmitPrimsCycleColors(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelEmitPrimsCycleColors ? g->host.renderer.ModelEmitPrimsCycleColors(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelEmitPrimsCycleColors", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako1_DebugTextPrintfWhite(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008CBFCu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako1_ColorFadeSet(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8008C3C0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako1_ZakoDebugDrawPrimitive(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ZakoDebugDrawPrimitive ? g->host.renderer.ZakoDebugDrawPrimitive(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "ZakoDebugDrawPrimitive", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako2_PlaySfx(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x800877F8u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako2_DrawShadow(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.DrawShadow ? g->host.renderer.DrawShadow(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawShadow", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako2_ModelTransformRigid(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelTransformRigid ? g->host.renderer.ModelTransformRigid(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelTransformRigid", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako2_ModelDraw(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.renderer.ModelDraw ? g->host.renderer.ModelDraw(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, 0) : (g->host.render ? g->host.render(g->host.user, g->ram, "ModelDraw", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako2_FUN_8013f660(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_missing_call(g, "FUN_8013f660", 0x8013F660u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako2_DrawProjectileSprite(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
    return (int32_t)(g->host.renderer.DrawProjectileSprite ? g->host.renderer.DrawProjectileSprite(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6, (uint32_t)p7) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawProjectileSprite", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako2_DrawDebugGlyph(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
    return (int32_t)(g->host.renderer.DrawDebugGlyph ? g->host.renderer.DrawDebugGlyph(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6, (uint32_t)p7) : (g->host.render ? g->host.render(g->host.user, g->ram, "DrawDebugGlyph", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako2_FinishDebugText(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
    return (int32_t)(g->host.renderer.FinishDebugText ? g->host.renderer.FinishDebugText(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6, (uint32_t)p7) : (g->host.render ? g->host.render(g->host.user, g->ram, "FinishDebugText", (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0));
}

static int32_t wire_zako_ai_FUN_8013DF54(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
    (void)p8;
    return (int32_t)(ae_game_call7(g, 0x8013DF54u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6, (uint32_t)p7, (uint32_t)p8));
}

static int32_t wire_zako_ai_FUN_80140880(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
    (void)p8;
    return (int32_t)(ae_game_missing_words(g, "FUN_80140880", 0x80140880u, 7, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6, (uint32_t)p7, (uint32_t)p8}));
}

static int32_t wire_zako_ai_FUN_8013F504(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
    (void)p8;
    return (int32_t)(ae_game_missing_words(g, "FUN_8013F504", 0x8013F504u, 7, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6, (uint32_t)p7, (uint32_t)p8}));
}

static int32_t wire_zako_ai_FUN_8014116C(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
    (void)p8;
    return (int32_t)(ae_game_missing_words(g, "FUN_8014116C", 0x8014116Cu, 7, (const uint32_t[]){(uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5, (uint32_t)p6, (uint32_t)p7, (uint32_t)p8}));
}

static int32_t wire_zako_Printf(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(g->host.platform.Printf ? g->host.platform.Printf(g->host.user, g->ram, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5) : 0);
}

static int32_t wire_zako_MenuItemInitBound(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007D1E0u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako_MenuItemSetCallback(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007D29Cu, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static int32_t wire_zako_ReadSizedValue(void * p0, AeRam * p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    return (int32_t)(ae_game_call(g, 0x8007D118u, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5));
}

static uint32_t wire_zones_area_bump_alloc(void * p0, AeRam * p1, uint32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    return (uint32_t)(ae_game_call(g, 0x8008C1B8u, (uint32_t)p2, 0, 0, 0));
}

static int32_t wire_zones_get_collider_quadrant_mask_xz(void * p0, AeRam * p1, int32_t p2, int32_t p3) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    return (int32_t)(ae_game_call(g, 0x800889B4u, (uint32_t)p2, (uint32_t)p3, 0, 0));
}

static void wire_zones_FUN_8013fc00(void * p0, AeRam * p1, int32_t p2) {
    AeGame *g = (AeGame *)p0;
    (void)p1;
    (void)p2;
    (void)(ae_game_missing_call(g, "FUN_8013fc00", 0x8013FC00u, (uint32_t)p2, 0, 0, 0));
}

static int32_t nested_camera_Atan2Safe(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_Atan2Safe(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_VectorNormalSS(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_VectorNormalSS(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_SquareRoot0(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_SquareRoot0(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_GetGroundHeight(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_GetGroundHeight(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_rsin(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_rsin(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_rcos(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_rcos(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_RotTransPers(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_RotTransPers(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_SetRotMatrix(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_SetRotMatrix(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_SetTransMatrix(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_SetTransMatrix(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_memset(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_memset(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_PlayerCanExitStage(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_PlayerCanExitStage(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_PlayerIsSubmerged(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_PlayerIsSubmerged(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_CameraRcCarActive(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_CameraRcCarActive(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_CameraRcCarFollow(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_CameraRcCarFollow(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_IsFlightArea(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_IsFlightArea(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_FUN_8013d000(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_FUN_8013d000(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_BossUpdate(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_BossUpdate(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_CameraAreaScriptDispatch(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_CameraAreaScriptDispatch(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_CallIndexedHandler(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_CallIndexedHandler(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_RotMatrixY(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_RotMatrixY(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_ApplyMatrixSV(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_ApplyMatrixSV(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_MatrixIdentity(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_MatrixIdentity(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_CameraGetPlayerDir(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_CameraGetPlayerDir(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_AimRequestStart(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_AimRequestStart(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_AimRequestClear(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_AimRequestClear(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_AimModeEnter(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_AimModeEnter(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_AimModeLeave(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_AimModeLeave(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_CameraFixedModeOff(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_CameraFixedModeOff(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_StageExitPending(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_StageExitPending(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_BossAimSfx(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_BossAimSfx(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_BossKindForArea(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_BossKindForArea(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_ScreenFadeSet(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_ScreenFadeSet(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_PlaySfx(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_PlaySfx(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_HudDrawMonkeyInfo(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_HudDrawMonkeyInfo(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_RadarUsable(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_RadarUsable(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_MonkeyFlag248Set(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_MonkeyFlag248Set(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_CameraRadarFocusUpdate(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_CameraRadarFocusUpdate(owner->user, ram, a0, a1, a2, a3);
}
static int32_t nested_camera_CameraRadarFocusSelect(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeCamupdHooks *owner = user;
    return wire_camera_CameraRadarFocusSelect(owner->user, ram, a0, a1, a2, a3);
}
void ae_game_wire_modules(AeGame *g) {
    g->anim_draw.user = g;
    g->anim_draw.area_hooks = &g->area_set;
    g->anim_draw.gadget_hooks = &g->gadget;
    g->anim_draw.player1_hooks = &g->player1;
    g->anim_draw.player2_hooks = &g->player2;
    g->anim_draw.items_hooks = &g->items_init;
    g->area_set.user = g;
    g->area_set.body_hooks = &g->body;
    g->body_move.user = g;
    g->body_move.stack_pointer = g->call_stack_pointer;
    g->body_move.world = &g->world;
    g->body_move.solid = &g->solid;
    g->body_move.misc = &g->misc;
    g->body_move.player_act = &g->player_act;
    g->body_move.spawn = &g->spawn;
    g->body_move.camupd = &g->camupd;
    g->body_move.area_set = &g->area_set;
    g->body_move.sound_hw = &g->host.sound_hw;
    g->body_move.coll_probe = &g->coll_probe;
    g->camera.user = g;
    g->camupd.user = g;
    g->camupd.stage = &g->stage;
    g->coll_probe.user = g;
    g->effects.user = g;
    g->effects.sound = &g->host.sound_hw;
    g->fxscript.user = g;
    g->fxscript.sound = &g->host.sound_hw;
    g->gadget.user = g;
    g->gimmick_a.user = g;
    g->gimmick_b.user = g;
    g->gimmick_b.gimmick_hooks = &g->gimmick;
    g->gimmick_b.world_hooks = &g->world;
    g->gimmick_b.body_hooks = &g->body;
    g->gimmick_b.sound_hw = &g->host.sound_hw;
    g->gimmick.user = g;
    g->hud.user = g;
    g->hud.stack_pointer = g->call_stack_pointer;
    g->hud.sound_hw = &g->host.sound_hw;
    g->items_init.user = g;
    g->menu_win.user = g;
    g->menu_win.pad = &g->pad;
    g->menu_win.stage = &g->stage;
    g->menu_win.sound = &g->host.sound_hw;
    g->menu_win.debug_text = &g->zako2;
    g->misc.user = g;
    g->misc.stack_pointer = g->call_stack_pointer;
    g->misc.sound_hw = &g->host.sound_hw;
    g->mode.user = g;
    g->mode.original_call = ae_game_mode_call;
    g->mode.stack_pointer = g->call_stack_pointer;
    g->monkey.user = g;
    g->monkey.body = &g->body;
    g->ovl18b.user = g;
    g->ovl18b.stack_pointer = g->call_stack_pointer;
    g->ovl18b.area = &g->area_set;
    g->ovl18b.pad = &g->pad;
    g->ovl18b.items = &g->items_init;
    g->ovl18b.stage_fade = &g->stage_fade;
    g->ovl18b.pause_menu = &g->pause_menu;
    g->ovl18c.user = g;
    g->ovl18c.original_call = wire_original_words;
    g->ovl18c.renderer = wire_renderer_words;
    g->ovl18c.zako_hooks = &g->zako;
    g->ovl18c.zako1_hooks = &g->zako1;
    g->ovl18c.spawn_hooks = &g->spawn;
    g->ovl18c.world_hooks = &g->world;
    g->pad.user = g;
    g->pause_menu.user = g;
    g->pause_menu.stack_pointer = g->call_stack_pointer;
    g->pause_menu.caller_s5 = g->host.pause_menu_caller_s5;
    g->pause_menu.pad = &g->pad;
    g->pause_menu.player2 = &g->player2;
    g->pause_menu.player_act = &g->player_act;
    g->pause_menu.spawn = &g->spawn;
    g->pause_menu.bodies = &g->body;
    g->pause_menu.sound_hw = &g->host.sound_hw;
    g->player1.user = g;
    g->player1.sound_hw = &g->host.sound_hw;
    g->player2.user = g;
    g->player_act.user = g;
    g->player_cond.user = g;
    g->player_cond.world = &g->world;
    g->player_spawn.user = g;
    g->player_spawn.stack_pointer = g->call_stack_pointer;
    g->player_spawn.world = &g->world;
    g->player_spawn.bodies = &g->body;
    g->player_spawn.camera = &g->camera;
    g->player_spawn.sound = &g->host.sound_hw;
    g->player_update_frame.user = g;
    g->player_update_frame.turn = &g->player_update;
    g->player_update_frame.player1 = &g->player1;
    g->player_update_frame.player2 = &g->player2;
    g->player_update_frame.act = &g->player_act;
    g->player_update_frame.player_spawn = &g->player_spawn;
    g->player_update_frame.anim = &g->anim;
    g->player_update_frame.world = &g->world;
    g->player_update_frame.solid = &g->solid;
    g->player_update_frame.stage = &g->stage;
    g->player_update_frame.area_set = &g->area_set;
    g->player_update_frame.spawn = &g->spawn;
    g->player_update_frame.body = &g->body;
    g->player_update_frame.sound = &g->host.sound_hw;
    g->player_update.user = g;
    g->progress.user = g;
    g->progress.sound_hw = &g->host.sound_hw;
    g->progress.gimmick_a = &g->gimmick_a;
    g->progress.area_set = &g->area_set;
    g->spawn.user = g;
    g->spawn.world = &g->world;
    g->stage_fade.user = g;
    g->stage_fade.stack_pointer = g->call_stack_pointer;
    g->stage_fade.stage = &g->stage;
    g->stage_fade.sound_hw = &g->host.sound_hw;
    g->stage.user = g;
    g->stage.stack_pointer = g->call_stack_pointer;
    g->stage.monkey_hooks = &g->monkey;
    /* ae_stage.c consumes the historical two-callback slot layout. */
    g->stage.gadget_hooks = (const AeGadgetHooks *)(const void *)&g->legacy_gadget;
    g->tex_copy.user = g;
    g->tex_copy.stack_pointer = g->call_stack_pointer;
    g->vram_anim.user = g;
    g->world.user = g;
    g->zako1.user = g;
    g->zako1.init_hooks = &g->zako;
    g->zako1.body_hooks = &g->body;
    g->zako1.world_hooks = &g->world;
    g->zako1.spawn_hooks = &g->spawn;
    g->zako1.sound_hw = &g->host.sound_hw;
    g->zako2.user = g;
    g->zako2.body_hooks = &g->body;
    g->zako2.world_hooks = &g->world;
    g->zako2.spawn_hooks = &g->spawn;
    g->zako2.items_hooks = &g->items_init;
    g->zako_ai.user = g;
    g->zako_ai.zako1_hooks = &g->zako1;
    g->zako.user = g;
    g->zako.body_hooks = &g->body;
    g->zones.user = g;
    g->anim_draw.SoundLoopStart = wire_anim_draw_SoundLoopStart;
    g->anim_draw.SoundLoopStop = wire_anim_draw_SoundLoopStop;
    g->anim_draw.PlaySfx = wire_anim_draw_PlaySfx;
    g->anim_draw.PadRumble = wire_anim_draw_PadRumble;
    g->anim_draw.ModelTransformRigid = wire_anim_draw_ModelTransformRigid;
    g->anim_draw.ModelTransformSkinned = wire_anim_draw_ModelTransformSkinned;
    g->anim_draw.ModelCalcEnvUVs = wire_anim_draw_ModelCalcEnvUVs;
    g->anim_draw.ModelDraw = wire_anim_draw_ModelDraw;
    g->anim_draw.ModelEmitPrims = wire_anim_draw_ModelEmitPrims;
    g->anim_draw.ModelFillVertexColors = wire_anim_draw_ModelFillVertexColors;
    g->anim_draw.ModelDrawEnvMap = wire_anim_draw_ModelDrawEnvMap;
    g->anim_draw.ModelEmitPrimsCycleColors = wire_anim_draw_ModelEmitPrimsCycleColors;
    g->anim_draw.DrawShadow = wire_anim_draw_DrawShadow;
    g->anim_draw.Draw_Nop = wire_anim_draw_Draw_Nop;
    g->anim_draw.EffectRingDraw = wire_anim_draw_EffectRingDraw;
    g->anim_draw.VertexWobble = wire_anim_draw_VertexWobble;
    g->anim_draw.SfxVoiceSetPitch = wire_anim_draw_SfxVoiceSetPitch;
    g->anim_draw.SetRotMatrix = wire_anim_draw_SetRotMatrix;
    g->anim_draw.SetTransMatrix = wire_anim_draw_SetTransMatrix;
    g->anim_draw.SDK_80031230 = wire_anim_draw_SDK_80031230;
    g->anim_draw.SDK_800302D0 = wire_anim_draw_SDK_800302D0;
    g->anim_draw.TurnTowardsY = wire_anim_draw_TurnTowardsY;
    g->anim_draw.SoundEmitterPitch = wire_anim_draw_SoundEmitterPitch;
    g->anim_draw.IsPlayerSwimming = wire_anim_draw_IsPlayerSwimming;
    g->anim_draw.PlayerStopGadgetSounds = wire_anim_draw_PlayerStopGadgetSounds;
    g->anim_draw.RandomRange = wire_anim_draw_RandomRange;
    g->anim_draw.RendererPrimitiveBlock = wire_anim_draw_RendererPrimitiveBlock;
    g->anim_draw.RendererWobbleGte = wire_anim_draw_RendererWobbleGte;
    g->area_set.Printf = wire_area_set_Printf;
    g->area_set.Exit = wire_area_set_Exit;
    g->area_set.ProgressGetByte = wire_area_set_ProgressGetByte;
    g->area_set.FUN_8013d1f0 = wire_area_set_FUN_8013d1f0;
    g->area_set.Area44_Boss_SetAnim = wire_area_set_Area44_Boss_SetAnim;
    g->area_set.FUN_8013f68c = wire_area_set_FUN_8013f68c;
    g->area_set.FUN_8013f130 = wire_area_set_FUN_8013f130;
    g->area_set.FUN_8013ecf4 = wire_area_set_FUN_8013ecf4;
    g->area_set.FUN_8013d52c = wire_area_set_FUN_8013d52c;
    g->area_set.FUN_8013db98 = wire_area_set_FUN_8013db98;
    g->area_set.FUN_8013dc00 = wire_area_set_FUN_8013dc00;
    g->area_set.FUN_8013d440 = wire_area_set_FUN_8013d440;
    g->area_set.Area46_Boss_SetAnim = wire_area_set_Area46_Boss_SetAnim;
    g->area_set.Area46_Boss_ClearFlag = wire_area_set_Area46_Boss_ClearFlag;
    g->area_set.FUN_8013f544 = wire_area_set_FUN_8013f544;
    g->area_set.FUN_8013e14c = wire_area_set_FUN_8013e14c;
    g->area_set.FUN_8013ec5c = wire_area_set_FUN_8013ec5c;
    g->area_set.FUN_8013d8bc = wire_area_set_FUN_8013d8bc;
    g->area_set.FUN_8013df28 = wire_area_set_FUN_8013df28;
    g->area_set.FUN_8013d10c = wire_area_set_FUN_8013d10c;
    g->area_set.FUN_8013f4f0 = wire_area_set_FUN_8013f4f0;
    g->area_set.FUN_8013e8c0 = wire_area_set_FUN_8013e8c0;
    g->area_set.FUN_8013de14 = wire_area_set_FUN_8013de14;
    g->area_set.FUN_8013e4d8 = wire_area_set_FUN_8013e4d8;
    g->area_set.FUN_8013d804 = wire_area_set_FUN_8013d804;
    g->area_set.FUN_8013d000 = wire_area_set_FUN_8013d000;
    g->area_set.Area56_Boss_SetCameraObjAnim = wire_area_set_Area56_Boss_SetCameraObjAnim;
    g->area_set.FUN_8013f86c = wire_area_set_FUN_8013f86c;
    g->area_set.FUN_8013e4a8 = wire_area_set_FUN_8013e4a8;
    g->area_set.FUN_8013d47c = wire_area_set_FUN_8013d47c;
    g->area_set.FUN_8013e070 = wire_area_set_FUN_8013e070;
    g->area_set.FUN_8013e7f8 = wire_area_set_FUN_8013e7f8;
    g->area_set.FUN_8013f080 = wire_area_set_FUN_8013f080;
    g->area_set.Area57_Boss_SetAnim = wire_area_set_Area57_Boss_SetAnim;
    g->area_set.Area57_Boss_ClearFlag = wire_area_set_Area57_Boss_ClearFlag;
    g->area_set.FUN_8013e020 = wire_area_set_FUN_8013e020;
    g->area_set.FUN_8013d33c = wire_area_set_FUN_8013d33c;
    g->area_set.FUN_801407bc = wire_area_set_FUN_801407bc;
    g->area_set.FUN_8013d9a8 = wire_area_set_FUN_8013d9a8;
    g->area_set.FUN_8013ee9c = wire_area_set_FUN_8013ee9c;
    g->body_move.LocalDirectionPadding = wire_body_move_LocalDirectionPadding;
    g->body_move.SfxVoiceSetPitch = wire_body_move_SfxVoiceSetPitch;
    g->body_move.XaPlay = wire_body_move_XaPlay;
    g->camera.Atan2Safe = wire_camera_Atan2Safe;
    g->camera.VectorNormalSS = wire_camera_VectorNormalSS;
    g->camera.SquareRoot0 = wire_camera_SquareRoot0;
    g->camera.GetGroundHeight = wire_camera_GetGroundHeight;
    g->camera.rsin = wire_camera_rsin;
    g->camera.rcos = wire_camera_rcos;
    g->camera.RotTransPers = wire_camera_RotTransPers;
    g->camera.SetRotMatrix = wire_camera_SetRotMatrix;
    g->camera.SetTransMatrix = wire_camera_SetTransMatrix;
    g->camera.memset = wire_camera_memset;
    g->camera.PlayerCanExitStage = wire_camera_PlayerCanExitStage;
    g->camera.PlayerIsSubmerged = wire_camera_PlayerIsSubmerged;
    g->camera.CameraRcCarActive = wire_camera_CameraRcCarActive;
    g->camera.CameraRcCarFollow = wire_camera_CameraRcCarFollow;
    g->camera.IsFlightArea = wire_camera_IsFlightArea;
    g->camera.FUN_8013d000 = wire_camera_FUN_8013d000;
    g->camera.BossUpdate = wire_camera_BossUpdate;
    g->camera.CameraAreaScriptDispatch = wire_camera_CameraAreaScriptDispatch;
    g->camera.CallIndexedHandler = wire_camera_CallIndexedHandler;
    g->camera.RotMatrixY = wire_camera_RotMatrixY;
    g->camera.ApplyMatrixSV = wire_camera_ApplyMatrixSV;
    g->camera.MatrixIdentity = wire_camera_MatrixIdentity;
    g->camera.CameraGetPlayerDir = wire_camera_CameraGetPlayerDir;
    g->camera.AimRequestStart = wire_camera_AimRequestStart;
    g->camera.AimRequestClear = wire_camera_AimRequestClear;
    g->camera.AimModeEnter = wire_camera_AimModeEnter;
    g->camera.AimModeLeave = wire_camera_AimModeLeave;
    g->camera.CameraFixedModeOff = wire_camera_CameraFixedModeOff;
    g->camera.StageExitPending = wire_camera_StageExitPending;
    g->camera.BossAimSfx = wire_camera_BossAimSfx;
    g->camera.BossKindForArea = wire_camera_BossKindForArea;
    g->camera.ScreenFadeSet = wire_camera_ScreenFadeSet;
    g->camera.PlaySfx = wire_camera_PlaySfx;
    g->camera.HudDrawMonkeyInfo = wire_camera_HudDrawMonkeyInfo;
    g->camera.RadarUsable = wire_camera_RadarUsable;
    g->camera.MonkeyFlag248Set = wire_camera_MonkeyFlag248Set;
    g->camera.CameraRadarFocusUpdate = wire_camera_CameraRadarFocusUpdate;
    g->camera.CameraRadarFocusSelect = wire_camera_CameraRadarFocusSelect;
    g->camupd.PadGetState = wire_camupd_PadGetState;
    g->camupd.SpawnParticle = wire_camupd_SpawnParticle;
    g->camupd.Printf = wire_camupd_Printf;
    g->camupd.memset = wire_camupd_memset;
    g->camupd.CameraAreaScriptDispatch = wire_camupd_CameraAreaScriptDispatch;
    g->camupd.CameraLookAt = wire_camupd_CameraLookAt;
    g->camupd.RenderScreenFlash = wire_camupd_RenderScreenFlash;
    g->camupd.RenderCameraTransition = wire_camupd_RenderCameraTransition;
    g->coll_probe.PointInBoxXZ = wire_coll_probe_PointInBoxXZ;
    g->coll_probe.printf = wire_coll_probe_printf;
    g->coll_probe.HalfPipeRangeTest = wire_coll_probe_HalfPipeRangeTest;
    g->coll_probe.ColliderSlopeContactDir = wire_coll_probe_ColliderSlopeContactDir;
    g->coll_probe.DirIndexFromFlags = wire_coll_probe_DirIndexFromFlags;
    g->coll_probe.PlayerSetClimbTarget = wire_coll_probe_PlayerSetClimbTarget;
    g->coll_probe.PlayerSetRope = wire_coll_probe_PlayerSetRope;
    g->effects.EffectRingDraw = wire_effects_EffectRingDraw;
    g->effects.SpawnRandomParticle = wire_effects_SpawnRandomParticle;
    g->effects.GpuProjectMesh = wire_effects_GpuProjectMesh;
    g->effects.GpuDrawMesh = wire_effects_GpuDrawMesh;
    g->effects.Gadget4TrailUpdate = wire_effects_Gadget4TrailUpdate;
    g->effects.GadgetFlashUpdate = wire_effects_GadgetFlashUpdate;
    g->effects.LocalPositionPadding = wire_effects_LocalPositionPadding;
    g->effects.GpuDrawSprite = wire_effects_GpuDrawSprite;
    g->effects.RibbonDraw = wire_effects_RibbonDraw;
    g->effects.SetRotMatrix = wire_effects_SetRotMatrix;
    g->effects.SetTransMatrix = wire_effects_SetTransMatrix;
    g->effects.GpuDrawTrailPoint = wire_effects_GpuDrawTrailPoint;
    g->effects.GpuDrawEffectObj = wire_effects_GpuDrawEffectObj;
    g->effects.SparkAddRandom = wire_effects_SparkAddRandom;
    g->effects.GpuDrawFxPlayerState6 = wire_effects_GpuDrawFxPlayerState6;
    g->effects.GpuDrawGroundSpark = wire_effects_GpuDrawGroundSpark;
    g->fxscript.position_padding = wire_fxscript_position_padding;
    g->fxscript.SetRotMatrix = wire_fxscript_SetRotMatrix;
    g->fxscript.SetTransMatrix = wire_fxscript_SetTransMatrix;
    g->fxscript.ClearMemory = wire_fxscript_ClearMemory;
    g->fxscript.ProjectEffectPoint = wire_fxscript_ProjectEffectPoint;
    g->fxscript.DrawParticleQuad = wire_fxscript_DrawParticleQuad;
    g->fxscript.DrawRibbonStrip = wire_fxscript_DrawRibbonStrip;
    g->fxscript.Printf = wire_fxscript_Printf;
    g->fxscript.SoundLoopStop = wire_fxscript_SoundLoopStop;
    g->fxscript.CameraShake = wire_fxscript_CameraShake;
    g->fxscript.PadRumble = wire_fxscript_PadRumble;
    g->fxscript.PlayerCanActivateGadget = wire_fxscript_PlayerCanActivateGadget;
    g->fxscript.TurnTowardsY = wire_fxscript_TurnTowardsY;
    g->fxscript.StepToward = wire_fxscript_StepToward;
    g->fxscript.SpawnRandomParticle = wire_fxscript_SpawnRandomParticle;
    g->fxscript.ProjectileExpireCallback = wire_fxscript_ProjectileExpireCallback;
    g->fxscript.FUN_8014022c = wire_fxscript_FUN_8014022c;
    g->fxscript.FUN_8014263c = wire_fxscript_FUN_8014263c;
    g->fxscript.GetSolidHeight = wire_fxscript_GetSolidHeight;
    g->fxscript.GimmickActivate = wire_fxscript_GimmickActivate;
    g->fxscript.MeshInitFromAreaTable = wire_fxscript_MeshInitFromAreaTable;
    g->fxscript.FUN_8013d358 = wire_fxscript_FUN_8013d358;
    g->fxscript.EventCamStart = wire_fxscript_EventCamStart;
    g->fxscript.ModelTransformRigid = wire_fxscript_ModelTransformRigid;
    g->fxscript.ModelDraw = wire_fxscript_ModelDraw;
    g->gadget.PlayerCanExitStage = wire_gadget_PlayerCanExitStage;
    g->gadget.IsPlayerSwimming = wire_gadget_IsPlayerSwimming;
    g->gadget.RibbonDraw = wire_gadget_RibbonDraw;
    g->gadget.EffectSpawnFromDef = wire_gadget_EffectSpawnFromDef;
    g->gadget.SpawnParticle = wire_gadget_SpawnParticle;
    g->gadget.TrailSpawn = wire_gadget_TrailSpawn;
    g->gadget.PlaySfx = wire_gadget_PlaySfx;
    g->gadget.CameraGetSlotTarget = wire_gadget_CameraGetSlotTarget;
    g->gadget.TargetListFindBest = wire_gadget_TargetListFindBest;
    g->gadget.SdkVectorHelper_80031330 = wire_gadget_SdkVectorHelper_80031330;
    g->gadget.AimReticleDrawCircle = wire_gadget_AimReticleDrawCircle;
    g->gadget.PlayerSetState = wire_gadget_PlayerSetState;
    g->gadget.PadRumble = wire_gadget_PadRumble;
    g->gadget.DrawScaledModelAt = wire_gadget_DrawScaledModelAt;
    g->gadget.SdkVectorHelper_80030A30 = wire_gadget_SdkVectorHelper_80030A30;
    g->gadget.SdkVectorHelper_80031830 = wire_gadget_SdkVectorHelper_80031830;
    g->gadget.ModelTransformRigid = wire_gadget_ModelTransformRigid;
    g->gadget.SetColorMatrixMode = wire_gadget_SetColorMatrixMode;
    g->gadget.ModelDraw = wire_gadget_ModelDraw;
    g->gadget.PlayerCanActivateGadget = wire_gadget_PlayerCanActivateGadget;
    g->gadget.SoundLoopStart = wire_gadget_SoundLoopStart;
    g->gadget.SoundLoopStop = wire_gadget_SoundLoopStop;
    g->gadget.SoundEmitterVolume = wire_gadget_SoundEmitterVolume;
    g->gadget.MatrixRotYMul = wire_gadget_MatrixRotYMul;
    g->gadget.SdkVectorHelper_800305B0 = wire_gadget_SdkVectorHelper_800305B0;
    g->gadget.PlayerFxSetMatrixPos = wire_gadget_PlayerFxSetMatrixPos;
    g->gadget.RadarDrawModel = wire_gadget_RadarDrawModel;
    g->gadget.SdkVectorHelper_8002FB00 = wire_gadget_SdkVectorHelper_8002FB00;
    g->gadget.SdkVectorHelper_8002FB30 = wire_gadget_SdkVectorHelper_8002FB30;
    g->gadget.SdkVectorHelper_80031230 = wire_gadget_SdkVectorHelper_80031230;
    g->gadget.TrailRingPush = wire_gadget_TrailRingPush;
    g->gadget.TrailRingDraw = wire_gadget_TrailRingDraw;
    g->gadget.DrawRibbonBetween = wire_gadget_DrawRibbonBetween;
    g->gadget.ProjectileGroupSpawnSpread = wire_gadget_ProjectileGroupSpawnSpread;
    g->gadget.PlayerSelectGadget = wire_gadget_PlayerSelectGadget;
    g->gadget.AimTableClear3 = wire_gadget_AimTableClear3;
    g->gadget.GetSolidHeight = wire_gadget_GetSolidHeight;
    g->gadget.PlaySfxAt = wire_gadget_PlaySfxAt;
    g->gadget.GimmickActivate = wire_gadget_GimmickActivate;
    g->gadget.MeshInitFromAreaTable = wire_gadget_MeshInitFromAreaTable;
    g->gadget.AreaEffect = wire_gadget_AreaEffect;
    g->gadget.CameraShake = wire_gadget_CameraShake;
    g->gadget.EventCamStart = wire_gadget_EventCamStart;
    g->gadget.PrintfError = wire_gadget_PrintfError;
    g->gadget.GimmickTypeToSfx = wire_gadget_GimmickTypeToSfx;
    g->gadget.PickupCollect = wire_gadget_PickupCollect;
    g->gadget.CameraNudge = wire_gadget_CameraNudge;
    g->gadget.GpuDrawGadget7Trail = wire_gadget_GpuDrawGadget7Trail;
    g->gadget.GpuDrawGadget4Trail = wire_gadget_GpuDrawGadget4Trail;
    g->gadget.GpuProjectGadget3Aim = wire_gadget_GpuProjectGadget3Aim;
    g->gadget.GpuDrawGadget3AimLine = wire_gadget_GpuDrawGadget3AimLine;
    g->gimmick_a.CameraNudge = wire_gimmick_a_CameraNudge;
    g->gimmick_a.BodyInsideBox = wire_gimmick_a_BodyInsideBox;
    g->gimmick_a.GimmickTouchCheck = wire_gimmick_a_GimmickTouchCheck;
    g->gimmick_a.MeshInitFromAreaTable = wire_gimmick_a_MeshInitFromAreaTable;
    g->gimmick_a.PropSetMesh = wire_gimmick_a_PropSetMesh;
    g->gimmick_a.PlaySfx = wire_gimmick_a_PlaySfx;
    g->gimmick_a.PlaySfxAt = wire_gimmick_a_PlaySfxAt;
    g->gimmick_a.SoundLoopStart = wire_gimmick_a_SoundLoopStart;
    g->gimmick_a.SoundEmitterPan = wire_gimmick_a_SoundEmitterPan;
    g->gimmick_a.SoundLoopStop = wire_gimmick_a_SoundLoopStop;
    g->gimmick_a.ProgressFlagGet = wire_gimmick_a_ProgressFlagGet;
    g->gimmick_a.ProgressFlagSet = wire_gimmick_a_ProgressFlagSet;
    g->gimmick_a.ProgressGetByte = wire_gimmick_a_ProgressGetByte;
    g->gimmick_a.ProgressSetByte = wire_gimmick_a_ProgressSetByte;
    g->gimmick_a.GetSolidHeight = wire_gimmick_a_GetSolidHeight;
    g->gimmick_a.SwingDoorBodyDelta = wire_gimmick_a_SwingDoorBodyDelta;
    g->gimmick_a.SlideDoorMove = wire_gimmick_a_SlideDoorMove;
    g->gimmick_a.GimmickHandler = wire_gimmick_a_GimmickHandler;
    g->gimmick_b.AreaMonkeyQuotaMet = wire_gimmick_b_AreaMonkeyQuotaMet;
    g->gimmick_b.FUN_80097504 = wire_gimmick_b_FUN_80097504;
    g->gimmick_b.SpecialActorPlaceAtCollider = wire_gimmick_b_SpecialActorPlaceAtCollider;
    g->gimmick_b.FUN_8009B3B0 = wire_gimmick_b_FUN_8009B3B0;
    g->gimmick_b.IsTimeAttack = wire_gimmick_b_IsTimeAttack;
    g->gimmick_b.Indicator_Register = wire_gimmick_b_Indicator_Register;
    g->gimmick_b.Indicator_Activate = wire_gimmick_b_Indicator_Activate;
    g->gimmick_b.SpawnEffectGroupAt = wire_gimmick_b_SpawnEffectGroupAt;
    g->gimmick_b.PadRumble = wire_gimmick_b_PadRumble;
    g->gimmick_b.FUN_8006AA60 = wire_gimmick_b_FUN_8006AA60;
    g->gimmick_b.MeshInitFromAreaTable = wire_gimmick_b_MeshInitFromAreaTable;
    g->gimmick_b.FUN_80091598 = wire_gimmick_b_FUN_80091598;
    g->gimmick_b.FUN_80091AF0 = wire_gimmick_b_FUN_80091AF0;
    g->gimmick_b.SoundLoopStart = wire_gimmick_b_SoundLoopStart;
    g->gimmick_b.SoundEmitterPan = wire_gimmick_b_SoundEmitterPan;
    g->gimmick_b.SoundLoopStop = wire_gimmick_b_SoundLoopStop;
    g->gimmick_b.FUN_80092324 = wire_gimmick_b_FUN_80092324;
    g->gimmick_b.FUN_80091C04 = wire_gimmick_b_FUN_80091C04;
    g->gimmick_b.EventCamStart = wire_gimmick_b_EventCamStart;
    g->gimmick_b.FUN_80093848 = wire_gimmick_b_FUN_80093848;
    g->gimmick_b.PlayerAttachSet = wire_gimmick_b_PlayerAttachSet;
    g->gimmick_b.PlaySfx = wire_gimmick_b_PlaySfx;
    g->gimmick_b.FUN_80066F34 = wire_gimmick_b_FUN_80066F34;
    g->gimmick_b.SvecToDiagMatrix = wire_gimmick_b_SvecToDiagMatrix;
    g->gimmick_b.FUN_80094330 = wire_gimmick_b_FUN_80094330;
    g->gimmick_b.DiagonalSignedDistance = wire_gimmick_b_DiagonalSignedDistance;
    g->gimmick_b.FUN_80094A48 = wire_gimmick_b_FUN_80094A48;
    g->gimmick_b.FUN_80094E5C = wire_gimmick_b_FUN_80094E5C;
    g->gimmick_b.ColliderBoxContainsPoint = wire_gimmick_b_ColliderBoxContainsPoint;
    g->gimmick_b.CameraNudge = wire_gimmick_b_CameraNudge;
    g->gimmick.PlaySfx = wire_gimmick_PlaySfx;
    g->gimmick.PlaySfxAt = wire_gimmick_PlaySfxAt;
    g->gimmick.PropSetMesh = wire_gimmick_PropSetMesh;
    g->gimmick.FUN_801414a0 = wire_gimmick_FUN_801414a0;
    g->hud.draw_sprite = wire_hud_draw_sprite;
    g->hud.draw_quad = wire_hud_draw_quad;
    g->hud.draw_glyph = wire_hud_draw_glyph;
    g->hud.draw_stage_overlay = wire_hud_draw_stage_overlay;
    g->hud.draw_gauge = wire_hud_draw_gauge;
    g->hud.draw_message_frame = wire_hud_draw_message_frame;
    g->hud.Ovl_8013D424 = wire_hud_Ovl_8013D424;
    g->hud.DR_MOVE = wire_hud_DR_MOVE;
    g->hud.ClearMemory = wire_hud_ClearMemory;
    g->hud.ModelDrawScreenSpace = wire_hud_ModelDrawScreenSpace;
    g->items_init.GetLevelFirstArea = wire_items_init_GetLevelFirstArea;
    g->items_init.printf = wire_items_init_printf;
    g->items_init.SetBackColor = wire_items_init_SetBackColor;
    g->items_init.SetFarColor = wire_items_init_SetFarColor;
    g->items_init.SetColorMatrix = wire_items_init_SetColorMatrix;
    g->items_init.rsin = wire_items_init_rsin;
    g->items_init.rcos = wire_items_init_rcos;
    g->menu_win.SetDrawMove = wire_menu_win_SetDrawMove;
    g->menu_win.DrawPrim = wire_menu_win_DrawPrim;
    g->menu_win.SetDrawArea = wire_menu_win_SetDrawArea;
    g->menu_win.SetDrawTPage = wire_menu_win_SetDrawTPage;
    g->menu_win.BeginFrame = wire_menu_win_BeginFrame;
    g->menu_win.ColorFadeReset = wire_menu_win_ColorFadeReset;
    g->menu_win.OtLinkSkipEmpty = wire_menu_win_OtLinkSkipEmpty;
    g->menu_win.DrawSync = wire_menu_win_DrawSync;
    g->menu_win.VSync = wire_menu_win_VSync;
    g->menu_win.PutDrawEnv = wire_menu_win_PutDrawEnv;
    g->menu_win.PutDispEnv = wire_menu_win_PutDispEnv;
    g->menu_win.DrawOTag = wire_menu_win_DrawOTag;
    g->menu_win.ModelTransformRigid = wire_menu_win_ModelTransformRigid;
    g->menu_win.ModelDraw = wire_menu_win_ModelDraw;
    g->menu_win.DrawMenuChrome = wire_menu_win_DrawMenuChrome;
    g->menu_win.DrawMenuRows = wire_menu_win_DrawMenuRows;
    g->menu_win.DrawMenuHighlight = wire_menu_win_DrawMenuHighlight;
    g->menu_win.DrawMenuItem = wire_menu_win_DrawMenuItem;
    g->menu_win.DrawMenuSelection = wire_menu_win_DrawMenuSelection;
    g->menu_win.DrawMenuCursor = wire_menu_win_DrawMenuCursor;
    g->menu_win.item_callback = wire_menu_win_item_callback;
    g->misc.MulMatrix = wire_misc_MulMatrix;
    g->misc.SetRotMatrix = wire_misc_SetRotMatrix;
    g->misc.SetTransMatrix = wire_misc_SetTransMatrix;
    g->misc.RotTransPers = wire_misc_RotTransPers;
    g->misc.ModelDraw = wire_misc_ModelDraw;
    g->misc.ModelDrawWithOt = wire_misc_ModelDrawWithOt;
    g->misc.ModelTransformRigid = wire_misc_ModelTransformRigid;
    g->misc.ModelTransformSkinned = wire_misc_ModelTransformSkinned;
    g->misc.ModelDrawEnvMap = wire_misc_ModelDrawEnvMap;
    g->misc.DrawShadow = wire_misc_DrawShadow;
    g->misc.RenderEnvUV = wire_misc_RenderEnvUV;
    g->misc.RenderIndicator = wire_misc_RenderIndicator;
    g->misc.RenderHudPopup = wire_misc_RenderHudPopup;
    g->misc.CopyPlayerEyeTexture = wire_misc_CopyPlayerEyeTexture;
    g->misc.HudShowHealth = wire_misc_HudShowHealth;
    g->misc.PlaySfx = wire_misc_PlaySfx;
    g->misc.IndicatorDraw = wire_misc_IndicatorDraw;
    g->misc.IndicatorRegister = wire_misc_IndicatorRegister;
    g->misc.IndicatorActivate = wire_misc_IndicatorActivate;
    g->misc.SpecialActorDraw = wire_misc_SpecialActorDraw;
    g->misc.SpawnParticle = wire_misc_SpawnParticle;
    g->misc.TrailSpawn = wire_misc_TrailSpawn;
    g->misc.RotTransPersResult = wire_misc_RotTransPersResult;
    g->misc.FUN_8013f464 = wire_misc_FUN_8013f464;
    g->misc.FUN_8013fa2c = wire_misc_FUN_8013fa2c;
    g->misc.FUN_8013fd74 = wire_misc_FUN_8013fd74;
    g->misc.FUN_80140a60 = wire_misc_FUN_80140a60;
    g->misc.FUN_80140d60 = wire_misc_FUN_80140d60;
    g->misc.GetGroundHeight = wire_misc_GetGroundHeight;
    g->misc.ColliderTypeActive = wire_misc_ColliderTypeActive;
    g->misc.IsTimeAttack = wire_misc_IsTimeAttack;
    g->misc.HeapFree = wire_misc_HeapFree;
    g->misc.Printf = wire_misc_Printf;
    g->misc.SoundLoopStart = wire_misc_SoundLoopStart;
    g->misc.SoundLoopStop = wire_misc_SoundLoopStop;
    g->misc.SpuWaitIdle = wire_misc_SpuWaitIdle;
    g->misc.SpuVolumeClamp = wire_misc_SpuVolumeClamp;
    g->misc.ColliderScriptMotion = wire_misc_ColliderScriptMotion;
    g->misc.ColliderScriptUpdate = wire_misc_ColliderScriptUpdate;
    g->misc.PadRumbleReset = wire_misc_PadRumbleReset;
    g->misc.ScreenBandDraw = wire_misc_ScreenBandDraw;
    g->misc.SetDrawMove = wire_misc_SetDrawMove;
    g->misc.exit = wire_misc_exit;
    g->misc.FUN_801402d8 = wire_misc_FUN_801402d8;
    g->misc.Area53_BossUpdate = wire_misc_Area53_BossUpdate;
    g->misc.FUN_80143854 = wire_misc_FUN_80143854;
    g->misc.Area46_BossUpdate = wire_misc_Area46_BossUpdate;
    g->misc.Area57_BossUpdate = wire_misc_Area57_BossUpdate;
    g->misc.FUN_80137248 = wire_misc_FUN_80137248;
    g->misc.FUN_8013fc18 = wire_misc_FUN_8013fc18;
    g->misc.FUN_8013fe4c = wire_misc_FUN_8013fe4c;
    g->misc.FUN_80088d78 = wire_misc_FUN_80088d78;
    g->misc.FUN_80088f2c = wire_misc_FUN_80088f2c;
    g->misc.FUN_800895f0 = wire_misc_FUN_800895f0;
    g->misc.FUN_80089b10 = wire_misc_FUN_80089b10;
    g->misc.FUN_80089d64 = wire_misc_FUN_80089d64;
    g->misc.FUN_80089d6c = wire_misc_FUN_80089d6c;
    g->misc.FUN_8008aa64 = wire_misc_FUN_8008aa64;
    g->misc.GetSolidHeight = wire_misc_GetSolidHeight;
    g->misc.GetColliderQuadrantMaskXZ = wire_misc_GetColliderQuadrantMaskXZ;
    g->misc.CdIntToPos = wire_misc_CdIntToPos;
    g->misc.CdControl = wire_misc_CdControl;
    g->misc.CdReadyCallback = wire_misc_CdReadyCallback;
    g->misc.VSync = wire_misc_VSync;
    g->misc.AnimFramePtr = wire_misc_AnimFramePtr;
    g->misc.CollisionProbe0 = wire_misc_CollisionProbe0;
    g->misc.CollisionProbe1 = wire_misc_CollisionProbe1;
    g->misc.CollisionProbe2 = wire_misc_CollisionProbe2;
    g->misc.CollisionProbe3 = wire_misc_CollisionProbe3;
    g->misc.CollisionProbe4 = wire_misc_CollisionProbe4;
    g->misc.CollisionProbe5 = wire_misc_CollisionProbe5;
    g->misc.CollisionProbe6 = wire_misc_CollisionProbe6;
    g->misc.CollisionProbe7 = wire_misc_CollisionProbe7;
    g->misc.PointInBoxXZ = wire_misc_PointInBoxXZ;
    g->misc.HalfPipeRangeTest = wire_misc_HalfPipeRangeTest;
    g->misc.ColliderSlopeContactDir = wire_misc_ColliderSlopeContactDir;
    g->misc.DirIndexFromFlags = wire_misc_DirIndexFromFlags;
    g->misc.PlayerSetClimbTarget = wire_misc_PlayerSetClimbTarget;
    g->misc.PlayerSetRope = wire_misc_PlayerSetRope;
    g->monkey.Printf = wire_monkey_Printf;
    g->monkey.GimmickGetInfo = wire_monkey_GimmickGetInfo;
    g->monkey.PlaySfxAt = wire_monkey_PlaySfxAt;
    g->monkey.ProjectileSpawn = wire_monkey_ProjectileSpawn;
    g->monkey.SpawnParticle = wire_monkey_SpawnParticle;
    g->monkey.TargetListAdd = wire_monkey_TargetListAdd;
    g->monkey.TargetListRemove = wire_monkey_TargetListRemove;
    g->monkey.MatrixFromDirXZ = wire_monkey_MatrixFromDirXZ;
    g->monkey.ModelPoseBones = wire_monkey_ModelPoseBones;
    g->monkey.ModelTransformRigid = wire_monkey_ModelTransformRigid;
    g->monkey.ModelDraw = wire_monkey_ModelDraw;
    g->monkey.MenuItemInitBound = wire_monkey_MenuItemInitBound;
    g->monkey.MenuItemSetRange = wire_monkey_MenuItemSetRange;
    g->monkey.MenuItemSetCallback = wire_monkey_MenuItemSetCallback;
    g->monkey.MenuItemSetStep = wire_monkey_MenuItemSetStep;
    g->monkey.EffectBigBurst = wire_monkey_EffectBigBurst;
    g->monkey.SparkAddRandom = wire_monkey_SparkAddRandom;
    g->monkey.ModelInit = wire_monkey_ModelInit;
    g->monkey.SetRotMatrix = wire_monkey_SetRotMatrix;
    g->monkey.SetTransMatrix = wire_monkey_SetTransMatrix;
    g->monkey.RotTransPers = wire_monkey_RotTransPers;
    g->monkey.GetGroundHeight = wire_monkey_GetGroundHeight;
    g->monkey.ObjSlotGetAnimIndex = wire_monkey_ObjSlotGetAnimIndex;
    g->monkey.AlertIconDraw = wire_monkey_AlertIconDraw;
    g->monkey.ObjSlotSetField_CF = wire_monkey_ObjSlotSetField_CF;
    g->monkey.ObjSlotGetTarget = wire_monkey_ObjSlotGetTarget;
    g->monkey.SoundLoopStartEx = wire_monkey_SoundLoopStartEx;
    g->monkey.SoundLoopStopAndFree = wire_monkey_SoundLoopStopAndFree;
    g->monkey.SpawnPuff = wire_monkey_SpawnPuff;
    g->monkey.MatrixLookDir = wire_monkey_MatrixLookDir;
    g->monkey.ScaleMatrix = wire_monkey_ScaleMatrix;
    g->monkey.ModelBuildBoneMatrices = wire_monkey_ModelBuildBoneMatrices;
    g->monkey.ModelTransformSkinned = wire_monkey_ModelTransformSkinned;
    g->monkey.ModelDrawWithOt = wire_monkey_ModelDrawWithOt;
    g->monkey.GimmickEvent = wire_monkey_GimmickEvent;
    g->monkey.ActorAnimDecode = wire_monkey_ActorAnimDecode;
    g->monkey.DrawShadow = wire_monkey_DrawShadow;
    g->monkey.ColorFadeSet = wire_monkey_ColorFadeSet;
    g->monkey.DebugTextPrintfWhite = wire_monkey_DebugTextPrintfWhite;
    g->monkey.ClearWords = wire_monkey_ClearWords;
    g->monkey.VSync = wire_monkey_VSync;
    g->monkey.StageExitIsActive = wire_monkey_StageExitIsActive;
    g->pad.pad_get_state = wire_pad_pad_get_state;
    g->pad.pad_info_mode = wire_pad_pad_info_mode;
    g->pad.pad_setup_dualshock = wire_pad_pad_setup_dualshock;
    g->pad.pad_demo_step = wire_pad_pad_demo_step;
    g->pad.stage_request = wire_pad_stage_request;
    g->pad.stick_to_buttons = wire_pad_stick_to_buttons;
    g->pad.pad_rumble_reset = wire_pad_pad_rumble_reset;
    g->pause_menu.WaitFrameFlip = wire_pause_menu_WaitFrameFlip;
    g->pause_menu.PauseMenuOverlay1B = wire_pause_menu_PauseMenuOverlay1B;
    g->pause_menu.DrawSync = wire_pause_menu_DrawSync;
    g->pause_menu.VSync = wire_pause_menu_VSync;
    g->pause_menu.PutDispEnv = wire_pause_menu_PutDispEnv;
    g->pause_menu.PutDrawEnv = wire_pause_menu_PutDrawEnv;
    g->pause_menu.StoreImage = wire_pause_menu_StoreImage;
    g->pause_menu.LoadImage = wire_pause_menu_LoadImage;
    g->pause_menu.ClearOTagR = wire_pause_menu_ClearOTagR;
    g->pause_menu.DrawOTag = wire_pause_menu_DrawOTag;
    g->pause_menu.HeapAlloc = wire_pause_menu_HeapAlloc;
    g->pause_menu.HeapFree = wire_pause_menu_HeapFree;
    g->pause_menu.ColorFadeReset = wire_pause_menu_ColorFadeReset;
    g->pause_menu.PadDirectionCode = wire_pause_menu_PadDirectionCode;
    g->pause_menu.TextSetCursor = wire_pause_menu_TextSetCursor;
    g->pause_menu.PlaySfx = wire_pause_menu_PlaySfx;
    g->pause_menu.PauseMenuInitVolumes = wire_pause_menu_PauseMenuInitVolumes;
    g->pause_menu.HudTextWidth = wire_pause_menu_HudTextWidth;
    g->pause_menu.DrawTextSprites = wire_pause_menu_DrawTextSprites;
    g->pause_menu.DrawMenuArrows = wire_pause_menu_DrawMenuArrows;
    g->pause_menu.IsTimeAttack = wire_pause_menu_IsTimeAttack;
    g->pause_menu.HudPanelA_Start = wire_pause_menu_HudPanelA_Start;
    g->pause_menu.HudPanelB_Start = wire_pause_menu_HudPanelB_Start;
    g->pause_menu.HudCounterC_Start = wire_pause_menu_HudCounterC_Start;
    g->pause_menu.HudDrawGadgetSlots = wire_pause_menu_HudDrawGadgetSlots;
    g->pause_menu.GadgetSlotRemap = wire_pause_menu_GadgetSlotRemap;
    g->pause_menu.HudDrawGadgetSlotsFrame = wire_pause_menu_HudDrawGadgetSlotsFrame;
    g->pause_menu.SetPriority = wire_pause_menu_SetPriority;
    g->pause_menu.CutsceneModeOffCaller = wire_pause_menu_CutsceneModeOffCaller;
    g->pause_menu.CutsceneModeOn = wire_pause_menu_CutsceneModeOn;
    g->pause_menu.PauseDrawCounter = wire_pause_menu_PauseDrawCounter;
    g->pause_menu.PauseDrawCaptureGrid = wire_pause_menu_PauseDrawCaptureGrid;
    g->pause_menu.PauseMenuDrawSubScreen = wire_pause_menu_PauseMenuDrawSubScreen;
    g->pause_menu.HudDrawIcon16 = wire_pause_menu_HudDrawIcon16;
    g->pause_menu.HudDrawTime = wire_pause_menu_HudDrawTime;
    g->pause_menu.GetDAT_800B01C2 = wire_pause_menu_GetDAT_800B01C2;
    g->pause_menu.HudDrawTimerGauge = wire_pause_menu_HudDrawTimerGauge;
    g->pause_menu.HudPanelA_DrawIcon = wire_pause_menu_HudPanelA_DrawIcon;
    g->pause_menu.HudDrawHealth = wire_pause_menu_HudDrawHealth;
    g->pause_menu.HudPanelB_DrawSprite = wire_pause_menu_HudPanelB_DrawSprite;
    g->pause_menu.HudDrawDigit = wire_pause_menu_HudDrawDigit;
    g->pause_menu.HudDrawNumber = wire_pause_menu_HudDrawNumber;
    g->pause_menu.HudCounterC_Draw = wire_pause_menu_HudCounterC_Draw;
    g->pause_menu.HudDrawGadgetIcons = wire_pause_menu_HudDrawGadgetIcons;
    g->pause_menu.CdMgrUpdate = wire_pause_menu_CdMgrUpdate;
    g->pause_menu.Printf = wire_pause_menu_Printf;
    g->pause_menu.SoundLoopStop = wire_pause_menu_SoundLoopStop;
    g->pause_menu.SfxVoiceSetPanVol = wire_pause_menu_SfxVoiceSetPanVol;
    g->pause_menu.TextNextGlyph = wire_pause_menu_TextNextGlyph;
    g->pause_menu.FontDrawGlyph = wire_pause_menu_FontDrawGlyph;
    g->pause_menu.RotMatrixZ = wire_pause_menu_RotMatrixZ;
    g->pause_menu.RotMatrixY = wire_pause_menu_RotMatrixY;
    g->pause_menu.RotMatrixX = wire_pause_menu_RotMatrixX;
    g->pause_menu.SetRotMatrix = wire_pause_menu_SetRotMatrix;
    g->pause_menu.SetTransMatrix = wire_pause_menu_SetTransMatrix;
    g->pause_menu.RotTrans = wire_pause_menu_RotTrans;
    g->pause_menu.RotTransSV = wire_pause_menu_RotTransSV;
    g->pause_menu.Memset = wire_pause_menu_Memset;
    g->pause_menu.ModelTransformRigid = wire_pause_menu_ModelTransformRigid;
    g->pause_menu.ModelDraw = wire_pause_menu_ModelDraw;
    g->pause_menu.ModelEmitPrimsScratchColors = wire_pause_menu_ModelEmitPrimsScratchColors;
    g->pause_menu.ModelDrawScreenSpace = wire_pause_menu_ModelDrawScreenSpace;
    g->pause_menu.GadgetDrawSlot0Attachment = wire_pause_menu_GadgetDrawSlot0Attachment;
    g->pause_menu.RendererIrgbToIr123 = wire_pause_menu_RendererIrgbToIr123;
    g->pause_menu.RendererPackIrgb = wire_pause_menu_RendererPackIrgb;
    g->pause_menu.RendererGetProjectionDistance = wire_pause_menu_RendererGetProjectionDistance;
    g->pause_menu.RendererSetProjectionDistance = wire_pause_menu_RendererSetProjectionDistance;
    g->pause_menu.RendererHudDrawWindowFrame = wire_pause_menu_RendererHudDrawWindowFrame;
    g->pause_menu.RendererPauseMenuDrawButtons = wire_pause_menu_RendererPauseMenuDrawButtons;
    g->pause_menu.RendererPauseMenuDrawGadgetScreen = wire_pause_menu_RendererPauseMenuDrawGadgetScreen;
    g->player1.CdIntToPos = wire_player1_CdIntToPos;
    g->player1.CdPosToInt = wire_player1_CdPosToInt;
    g->player1.CdControl = wire_player1_CdControl;
    g->player1.CdReadyCallback = wire_player1_CdReadyCallback;
    g->player1.VSync = wire_player1_VSync;
    g->player1.printf = wire_player1_printf;
    g->player1.SoundSetVolumeMode = wire_player1_SoundSetVolumeMode;
    g->player1.EventCamCreate = wire_player1_EventCamCreate;
    g->player1.ModelEmitPrimsCycleColors = wire_player1_ModelEmitPrimsCycleColors;
    g->player1.DrawRibbonBetween = wire_player1_DrawRibbonBetween;
    g->player1.ModelProjectVertices = wire_player1_ModelProjectVertices;
    g->player1.ModelEmitPrims = wire_player1_ModelEmitPrims;
    g->player2.position_padding = wire_player2_position_padding;
    g->player2.PlaySfx = wire_player2_PlaySfx;
    g->player2.AimModeLeave = wire_player2_AimModeLeave;
    g->player2.PlayerStopGadgetSounds = wire_player2_PlayerStopGadgetSounds;
    g->player2.PlayerCanActivateGadget = wire_player2_PlayerCanActivateGadget;
    g->player2.PlayerCondition = wire_player2_PlayerCondition;
    g->player2.PlayerAction = wire_player2_PlayerAction;
    g->player2.AnimDecodeFrame = wire_player2_AnimDecodeFrame;
    g->player2.MatrixFromFrontUp = wire_player2_MatrixFromFrontUp;
    g->player2.ScaleMatrixL = wire_player2_ScaleMatrixL;
    g->player2.ModelTransformSkinned = wire_player2_ModelTransformSkinned;
    g->player2.ModelPrepCycleColors = wire_player2_ModelPrepCycleColors;
    g->player2.ModelEmitPrimsCycleColors = wire_player2_ModelEmitPrimsCycleColors;
    g->player2.ModelDraw = wire_player2_ModelDraw;
    g->player2.DrawRibbonBetween = wire_player2_DrawRibbonBetween;
    g->player2.AimModeEnter = wire_player2_AimModeEnter;
    g->player2.TrailRingReset = wire_player2_TrailRingReset;
    g->player2.CameraFixedModeOff = wire_player2_CameraFixedModeOff;
    g->player2.AimRequestClear = wire_player2_AimRequestClear;
    g->player2.TurnTowardsY = wire_player2_TurnTowardsY;
    g->player2.PadRumble = wire_player2_PadRumble;
    g->player2.MatrixLookDir = wire_player2_MatrixLookDir;
    g->player2.ModelTransformRigid = wire_player2_ModelTransformRigid;
    g->player2.ModelDrawWithOt = wire_player2_ModelDrawWithOt;
    g->player2.ActorAnimDecodeEntry = wire_player2_ActorAnimDecodeEntry;
    g->player2.SetRotMatrix = wire_player2_SetRotMatrix;
    g->player2.SetTransMatrix = wire_player2_SetTransMatrix;
    g->player2.RotTransSV = wire_player2_RotTransSV;
    g->player2.RotTransPers = wire_player2_RotTransPers;
    g->player2.RotMatrixY = wire_player2_RotMatrixY;
    g->player2.EventCamStartRandom = wire_player2_EventCamStartRandom;
    g->player_act.AimRequestClear = wire_player_act_AimRequestClear;
    g->player_act.PlayerCond_JumpHeld = wire_player_act_PlayerCond_JumpHeld;
    g->player_act.PlayerInput_StickGesture = wire_player_act_PlayerInput_StickGesture;
    g->player_act.PadRumble = wire_player_act_PadRumble;
    g->player_act.Gadget3AimUpdateAmmo = wire_player_act_Gadget3AimUpdateAmmo;
    g->player_act.AimModeEnter = wire_player_act_AimModeEnter;
    g->player_act.AimModeLeave = wire_player_act_AimModeLeave;
    g->player_act.PlayPlayerVoice = wire_player_act_PlayPlayerVoice;
    g->player_act.EffectGroundImpactAtPlayer = wire_player_act_EffectGroundImpactAtPlayer;
    g->player_act.PlayerDamage = wire_player_act_PlayerDamage;
    g->player_act.PlaySfx = wire_player_act_PlaySfx;
    g->player_act.StageRequest = wire_player_act_StageRequest;
    g->player_act.EffectBigBurst = wire_player_act_EffectBigBurst;
    g->player_act.StageGoalReached = wire_player_act_StageGoalReached;
    g->player_act.HudPanelA_Start = wire_player_act_HudPanelA_Start;
    g->player_act.HeapFree = wire_player_act_HeapFree;
    g->player_act.CameraAreaScriptDispatch = wire_player_act_CameraAreaScriptDispatch;
    g->player_act.PlayerSelectGadget = wire_player_act_PlayerSelectGadget;
    g->player_act.PlayerIdleModelSetup = wire_player_act_PlayerIdleModelSetup;
    g->player_act.AnimFramePtr = wire_player_act_AnimFramePtr;
    g->player_act.DR_MOVE = wire_player_act_DR_MOVE;
    g->player_act.PlayerModeCallback = wire_player_act_PlayerModeCallback;
    g->player_cond.PlayerIsOnGround = wire_player_cond_PlayerIsOnGround;
    g->player_cond.PlayerSplash = wire_player_cond_PlayerSplash;
    g->player_cond.PlayerFxJumpBurst = wire_player_cond_PlayerFxJumpBurst;
    g->player_spawn.AreaBumpAlloc = wire_player_spawn_AreaBumpAlloc;
    g->player_spawn.printf = wire_player_spawn_printf;
    g->player_spawn.CameraLookAt = wire_player_spawn_CameraLookAt;
    g->player_spawn.SetGeomScreen = wire_player_spawn_SetGeomScreen;
    g->player_update_frame.XaPlay = wire_player_update_frame_XaPlay;
    g->player_update_frame.CdMgrQueueCommand = wire_player_update_frame_CdMgrQueueCommand;
    g->player_update_frame.CdIntToPos = wire_player_update_frame_CdIntToPos;
    g->player_update_frame.CdPosToInt = wire_player_update_frame_CdPosToInt;
    g->player_update_frame.memset = wire_player_update_frame_memset;
    g->player_update_frame.PlayerAnimateAndDraw = wire_player_update_frame_PlayerAnimateAndDraw;
    g->player_update_frame.FUN_80142808 = wire_player_update_frame_FUN_80142808;
    g->player_update_frame.PlayerModeCallback = wire_player_update_frame_PlayerModeCallback;
    g->player_update_frame.PlayerAction = wire_player_update_frame_PlayerAction;
    g->player_update_frame.PlayerCheckGadgetSelect = wire_player_update_frame_PlayerCheckGadgetSelect;
    g->player_update_frame.PlayerVoiceFallback = wire_player_update_frame_PlayerVoiceFallback;
    g->player_update_frame.PlayerUpdateLocalFrame = wire_player_update_frame_PlayerUpdateLocalFrame;
    g->player_update.RotMatrixY = wire_player_update_RotMatrixY;
    g->progress.SoundLoopsSetPan = wire_progress_SoundLoopsSetPan;
    g->progress.FUN_8013FB20 = wire_progress_FUN_8013FB20;
    g->progress.SoundBankSetVolume = wire_progress_SoundBankSetVolume;
    g->progress.SetDrawTPage = wire_progress_SetDrawTPage;
    g->spawn.PlaySfx = wire_spawn_PlaySfx;
    g->spawn.MatrixLookDir = wire_spawn_MatrixLookDir;
    g->spawn.MatrixFromDirXZ = wire_spawn_MatrixFromDirXZ;
    g->spawn.position_padding = wire_spawn_position_padding;
    g->stage_fade.DebugTextPrintf = wire_stage_fade_DebugTextPrintf;
    g->stage_fade.SeqTrackClose = wire_stage_fade_SeqTrackClose;
    g->stage_fade.SeqTrackSetPlayState = wire_stage_fade_SeqTrackSetPlayState;
    g->stage_fade.SoundBankFree = wire_stage_fade_SoundBankFree;
    g->stage_fade.SoundStopAllVoices = wire_stage_fade_SoundStopAllVoices;
    g->stage_fade.RenderScreenFlash = wire_stage_fade_RenderScreenFlash;
    g->stage_fade.RenderFadeSpiral = wire_stage_fade_RenderFadeSpiral;
    g->stage.AreaUsesAltMember = wire_stage_AreaUsesAltMember;
    g->stage.ArcGetMemberPos = wire_stage_ArcGetMemberPos;
    g->stage.FUN_80025E24 = wire_stage_FUN_80025E24;
    g->stage.CdMgrQueueCommand = wire_stage_CdMgrQueueCommand;
    g->stage.StageExitStart = wire_stage_StageExitStart;
    g->stage.IsTimeAttack = wire_stage_IsTimeAttack;
    g->stage.GetLevelFirstArea = wire_stage_GetLevelFirstArea;
    g->stage.GimmickSaveStates = wire_stage_GimmickSaveStates;
    g->stage.GimmickSaveApplyProgress = wire_stage_GimmickSaveApplyProgress;
    g->stage.ItemSaveCollectedMask = wire_stage_ItemSaveCollectedMask;
    g->stage.ZakoSaveDefeatedMask = wire_stage_ZakoSaveDefeatedMask;
    g->stage.AreaGetKind = wire_stage_AreaGetKind;
    g->stage.AreaGetLevel = wire_stage_AreaGetLevel;
    g->stage.GimmickSaveTableReset = wire_stage_GimmickSaveTableReset;
    g->stage.ItemsClearFlags = wire_stage_ItemsClearFlags;
    g->stage.ZakoClearSlotTable = wire_stage_ZakoClearSlotTable;
    g->stage.SoundSilence = wire_stage_SoundSilence;
    g->stage.LevelMonkeyCountGet = wire_stage_LevelMonkeyCountGet;
    g->stage.AreaBumpAlloc = wire_stage_AreaBumpAlloc;
    g->stage.MeshInit = wire_stage_MeshInit;
    g->stage.GetZoneMask = wire_stage_GetZoneMask;
    g->stage.FUN_80030890 = wire_stage_FUN_80030890;
    g->stage.FUN_800A3700 = wire_stage_FUN_800A3700;
    g->stage.HeapInit = wire_stage_HeapInit;
    g->stage.AreaBumpAllocSet = wire_stage_AreaBumpAllocSet;
    g->stage.AreaSpecialFlagsClear = wire_stage_AreaSpecialFlagsClear;
    g->stage.AreaSetVramAnimList = wire_stage_AreaSetVramAnimList;
    g->stage.AreaSetZoneBoxes = wire_stage_AreaSetZoneBoxes;
    g->stage.IndicatorsReset = wire_stage_IndicatorsReset;
    g->stage.AreaSetFeature30 = wire_stage_AreaSetFeature30;
    g->stage.AreaSetSpecialActor = wire_stage_AreaSetSpecialActor;
    g->stage.AreaSetIndicatorModelA = wire_stage_AreaSetIndicatorModelA;
    g->stage.AreaSetIndicatorModelB = wire_stage_AreaSetIndicatorModelB;
    g->stage.AreaSetFeature31 = wire_stage_AreaSetFeature31;
    g->stage.AreaSetFeature32 = wire_stage_AreaSetFeature32;
    g->stage.SoundAreaKeepsMusic = wire_stage_SoundAreaKeepsMusic;
    g->stage.BgmStart = wire_stage_BgmStart;
    g->stage.BgmUpdateVolume = wire_stage_BgmUpdateVolume;
    g->stage.ApplyLevelFog = wire_stage_ApplyLevelFog;
    g->stage.SetColorMatrixMode = wire_stage_SetColorMatrixMode;
    g->stage.SoundLoopsStopAll = wire_stage_SoundLoopsStopAll;
    g->stage.BuildTerrainBlockMask = wire_stage_BuildTerrainBlockMask;
    g->stage.TableClearDAT_800BEF60 = wire_stage_TableClearDAT_800BEF60;
    g->stage.DebugMenuInitPlayerParams = wire_stage_DebugMenuInitPlayerParams;
    g->stage.DebugMenuInitCounters = wire_stage_DebugMenuInitCounters;
    g->stage.DebugMenuInitCounters2 = wire_stage_DebugMenuInitCounters2;
    g->stage.MessageBoxClose = wire_stage_MessageBoxClose;
    g->stage.PlayerSpawnAtEntry = wire_stage_PlayerSpawnAtEntry;
    g->stage.TargetListClear = wire_stage_TargetListClear;
    g->stage.ZakoInit = wire_stage_ZakoInit;
    g->stage.MonkeyLodModelsInit = wire_stage_MonkeyLodModelsInit;
    g->stage.FxSlotsReset = wire_stage_FxSlotsReset;
    g->stage.ProjectilesReset = wire_stage_ProjectilesReset;
    g->stage.EffectsReset = wire_stage_EffectsReset;
    g->stage.ColliderZoneMasksUpdate = wire_stage_ColliderZoneMasksUpdate;
    g->stage.GimmickSpawnAll = wire_stage_GimmickSpawnAll;
    g->stage.BossInit = wire_stage_BossInit;
    g->stage.ItemsLoadForArea = wire_stage_ItemsLoadForArea;
    g->stage.AreaTexCopyInit = wire_stage_AreaTexCopyInit;
    g->stage.RcCarDespawn = wire_stage_RcCarDespawn;
    g->stage.PlayerSelectGadget = wire_stage_PlayerSelectGadget;
    g->stage.HudFindGadgetIconPos = wire_stage_HudFindGadgetIconPos;
    g->stage.MonkeyClearWords = wire_stage_MonkeyClearWords;
    g->stage.VSync = wire_stage_VSync;
    g->stage.Printf = wire_stage_Printf;
    g->stage.CutsceneModeOff = wire_stage_CutsceneModeOff;
    g->stage.CutsceneModeOn = wire_stage_CutsceneModeOn;
    g->stage.PlayerInit = wire_stage_PlayerInit;
    g->stage.PlayerSetMode = wire_stage_PlayerSetMode;
    g->stage.FUN_80028850 = wire_stage_FUN_80028850;
    g->stage.FUN_800286CC = wire_stage_FUN_800286CC;
    g->stage.WaitFrameFlip = wire_stage_WaitFrameFlip;
    g->stage.BgmFadeOut = wire_stage_BgmFadeOut;
    g->stage.SoundStop = wire_stage_SoundStop;
    g->stage.PlaySfx = wire_stage_PlaySfx;
    g->stage.ProgressSetByte = wire_stage_ProgressSetByte;
    g->stage.PlayTimeAdd = wire_stage_PlayTimeAdd;
    g->stage.StageGradeUpdate = wire_stage_StageGradeUpdate;
    g->stage.StageVictoryStart = wire_stage_StageVictoryStart;
    g->stage.StageTypeClearInit = wire_stage_StageTypeClearInit;
    g->stage.StageTypeClearUpdate = wire_stage_StageTypeClearUpdate;
    g->stage.StageIntroStart = wire_stage_StageIntroStart;
    g->stage.PlayerEnterCutsceneMode = wire_stage_PlayerEnterCutsceneMode;
    g->stage.StageIntroUpdate = wire_stage_StageIntroUpdate;
    g->stage.GimmickNopUpdate3 = wire_stage_GimmickNopUpdate3;
    g->stage.GimmickNopUpdate4 = wire_stage_GimmickNopUpdate4;
    g->stage.XaPlay = wire_stage_XaPlay;
    g->stage.AreaCheckExits = wire_stage_AreaCheckExits;
    g->stage.CdMgrUpdate = wire_stage_CdMgrUpdate;
    g->stage.FadeFrame = wire_stage_FadeFrame;
    g->stage.LoadListStep = wire_stage_LoadListStep;
    g->stage.FUN_80096638 = wire_stage_FUN_80096638;
    g->stage.FUN_8013EA24 = wire_stage_FUN_8013EA24;
    g->stage.AimRequestClear = wire_stage_AimRequestClear;
    g->stage.AimModeLeave = wire_stage_AimModeLeave;
    g->tex_copy.HeapAlloc = wire_tex_copy_HeapAlloc;
    g->tex_copy.DrawSync = wire_tex_copy_DrawSync;
    g->tex_copy.LoadImage = wire_tex_copy_LoadImage;
    g->tex_copy.StoreImage = wire_tex_copy_StoreImage;
    g->vram_anim.MoveImage = wire_vram_anim_MoveImage;
    g->world.GetSolidHeight = wire_world_GetSolidHeight;
    g->world.PlaySfxAt = wire_world_PlaySfxAt;
    g->world.GimmickActivate = wire_world_GimmickActivate;
    g->world.MeshInitFromAreaTable = wire_world_MeshInitFromAreaTable;
    g->world.FUN_8013d358 = wire_world_FUN_8013d358;
    g->world.CameraShake = wire_world_CameraShake;
    g->world.EventCamStart = wire_world_EventCamStart;
    g->zako1.ProjectilesUpdateDraw = wire_zako1_ProjectilesUpdateDraw;
    g->zako1.Draw_Nop = wire_zako1_Draw_Nop;
    g->zako1.FUN_8002FB00 = wire_zako1_FUN_8002FB00;
    g->zako1.FUN_8002FB30 = wire_zako1_FUN_8002FB30;
    g->zako1.FUN_80031330 = wire_zako1_FUN_80031330;
    g->zako1.Atan2Safe = wire_zako1_Atan2Safe;
    g->zako1.FUN_800A3740 = wire_zako1_FUN_800A3740;
    g->zako1.SpawnPickups = wire_zako1_SpawnPickups;
    g->zako1.FUN_8013F660 = wire_zako1_FUN_8013F660;
    g->zako1.ZakoTypeUpdate = wire_zako1_ZakoTypeUpdate;
    g->zako1.AreaModeUpdate = wire_zako1_AreaModeUpdate;
    g->zako1.ZakoFrameHook = wire_zako1_ZakoFrameHook;
    g->zako1.DrawShadow = wire_zako1_DrawShadow;
    g->zako1.ModelTransformSkinned = wire_zako1_ModelTransformSkinned;
    g->zako1.ModelPrepCycleColors = wire_zako1_ModelPrepCycleColors;
    g->zako1.ModelEmitPrims = wire_zako1_ModelEmitPrims;
    g->zako1.ModelEmitPrimsCycleColors = wire_zako1_ModelEmitPrimsCycleColors;
    g->zako1.DebugTextPrintfWhite = wire_zako1_DebugTextPrintfWhite;
    g->zako1.ColorFadeSet = wire_zako1_ColorFadeSet;
    g->zako1.ZakoDebugDrawPrimitive = wire_zako1_ZakoDebugDrawPrimitive;
    g->zako2.PlaySfx = wire_zako2_PlaySfx;
    g->zako2.DrawShadow = wire_zako2_DrawShadow;
    g->zako2.ModelTransformRigid = wire_zako2_ModelTransformRigid;
    g->zako2.ModelDraw = wire_zako2_ModelDraw;
    g->zako2.FUN_8013f660 = wire_zako2_FUN_8013f660;
    g->zako2.DrawProjectileSprite = wire_zako2_DrawProjectileSprite;
    g->zako2.DrawDebugGlyph = wire_zako2_DrawDebugGlyph;
    g->zako2.FinishDebugText = wire_zako2_FinishDebugText;
    g->zako_ai.FUN_8013DF54 = wire_zako_ai_FUN_8013DF54;
    g->zako_ai.FUN_80140880 = wire_zako_ai_FUN_80140880;
    g->zako_ai.FUN_8013F504 = wire_zako_ai_FUN_8013F504;
    g->zako_ai.FUN_8014116C = wire_zako_ai_FUN_8014116C;
    g->zako.Printf = wire_zako_Printf;
    g->zako.MenuItemInitBound = wire_zako_MenuItemInitBound;
    g->zako.MenuItemSetCallback = wire_zako_MenuItemSetCallback;
    g->zako.ReadSizedValue = wire_zako_ReadSizedValue;
    g->zones.area_bump_alloc = wire_zones_area_bump_alloc;
    g->zones.get_collider_quadrant_mask_xz = wire_zones_get_collider_quadrant_mask_xz;
    g->zones.FUN_8013fc00 = wire_zones_FUN_8013fc00;
    g->body.move = g->body_move;
    g->body.move.stack_pointer = g->call_stack_pointer - 56u;
    g->camupd.camera = g->camera;
    g->camupd.camera.Atan2Safe = nested_camera_Atan2Safe;
    g->camupd.camera.VectorNormalSS = nested_camera_VectorNormalSS;
    g->camupd.camera.SquareRoot0 = nested_camera_SquareRoot0;
    g->camupd.camera.GetGroundHeight = nested_camera_GetGroundHeight;
    g->camupd.camera.rsin = nested_camera_rsin;
    g->camupd.camera.rcos = nested_camera_rcos;
    g->camupd.camera.RotTransPers = nested_camera_RotTransPers;
    g->camupd.camera.SetRotMatrix = nested_camera_SetRotMatrix;
    g->camupd.camera.SetTransMatrix = nested_camera_SetTransMatrix;
    g->camupd.camera.memset = nested_camera_memset;
    g->camupd.camera.PlayerCanExitStage = nested_camera_PlayerCanExitStage;
    g->camupd.camera.PlayerIsSubmerged = nested_camera_PlayerIsSubmerged;
    g->camupd.camera.CameraRcCarActive = nested_camera_CameraRcCarActive;
    g->camupd.camera.CameraRcCarFollow = nested_camera_CameraRcCarFollow;
    g->camupd.camera.IsFlightArea = nested_camera_IsFlightArea;
    g->camupd.camera.FUN_8013d000 = nested_camera_FUN_8013d000;
    g->camupd.camera.BossUpdate = nested_camera_BossUpdate;
    g->camupd.camera.CameraAreaScriptDispatch = nested_camera_CameraAreaScriptDispatch;
    g->camupd.camera.CallIndexedHandler = nested_camera_CallIndexedHandler;
    g->camupd.camera.RotMatrixY = nested_camera_RotMatrixY;
    g->camupd.camera.ApplyMatrixSV = nested_camera_ApplyMatrixSV;
    g->camupd.camera.MatrixIdentity = nested_camera_MatrixIdentity;
    g->camupd.camera.CameraGetPlayerDir = nested_camera_CameraGetPlayerDir;
    g->camupd.camera.AimRequestStart = nested_camera_AimRequestStart;
    g->camupd.camera.AimRequestClear = nested_camera_AimRequestClear;
    g->camupd.camera.AimModeEnter = nested_camera_AimModeEnter;
    g->camupd.camera.AimModeLeave = nested_camera_AimModeLeave;
    g->camupd.camera.CameraFixedModeOff = nested_camera_CameraFixedModeOff;
    g->camupd.camera.StageExitPending = nested_camera_StageExitPending;
    g->camupd.camera.BossAimSfx = nested_camera_BossAimSfx;
    g->camupd.camera.BossKindForArea = nested_camera_BossKindForArea;
    g->camupd.camera.ScreenFadeSet = nested_camera_ScreenFadeSet;
    g->camupd.camera.PlaySfx = nested_camera_PlaySfx;
    g->camupd.camera.HudDrawMonkeyInfo = nested_camera_HudDrawMonkeyInfo;
    g->camupd.camera.RadarUsable = nested_camera_RadarUsable;
    g->camupd.camera.MonkeyFlag248Set = nested_camera_MonkeyFlag248Set;
    g->camupd.camera.CameraRadarFocusUpdate = nested_camera_CameraRadarFocusUpdate;
    g->camupd.camera.CameraRadarFocusSelect = nested_camera_CameraRadarFocusSelect;
}
