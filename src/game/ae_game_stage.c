/* Native file/presentation glue for the resident modes in ae_modes.c. */
#include "ae_calls.h"

#define FRAME_CALL(NAME, ADDRESS, A0, A1, A2, A3) \
    frame_##NAME(g, ADDRESS, A0, A1, A2, A3)
#define FRAME_ADAPTER(NAME) \
static int32_t frame_##NAME(AeGame *g, uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { \
    if (ae_game_find_call(address)) return ae_game_call(g, address, a0, a1, a2, a3); \
    if (g->host.frame.NAME) return g->host.frame.NAME(g->host.user, g->ram, a0, a1, a2, a3); \
    return ae_game_missing_call(g, #NAME, address, a0, a1, a2, a3); \
}
FRAME_ADAPTER(BeginFrame)
FRAME_ADAPTER(DrawTerrain)
FRAME_ADAPTER(DrawAreaProps)
FRAME_ADAPTER(QueueFrameFlip)
FRAME_ADAPTER(WaitFrameFlip)


int32_t ae_game_load_area(AeGame *g, int32_t area, int32_t entry) {
    if (!g->host.area_member) {
        ae_ram_fail(g->ram, "ae_game_load_area: area_member callback not provided");
        return 0;
    }
    const uint8_t *bytes = NULL;
    size_t size = 0;
    if (!g->host.area_member(g->host.user, area, &bytes, &size) || !bytes || !size) {
        ae_ram_fail(g->ram, "ae_game_load_area: no member for area %d", area);
        return 0;
    }
    if (!ae_ram_load(g->ram, AE_AREA_BASE, bytes, size)) return 0;
    g->loaded_overlay = 0;
    if (g->host.area_overlay) {
        uint32_t member = 0, address = 0;
        const uint8_t *overlay = NULL;
        size_t overlay_size = 0;
        int found = g->host.area_overlay(g->host.user, area, &member, &address, &overlay, &overlay_size);
        if (found < 0 || (found > 0 && (!member || !overlay || !overlay_size))) {
            ae_ram_fail(g->ram, "ae_game_load_area: code-overlay member unavailable for area %d", area);
            return 0;
        }
        if (found > 0) {
            if (!ae_ram_load(g->ram, address, overlay, overlay_size)) return 0;
            g->loaded_overlay = member;
        }
    }
    ae_wr16(g->ram, 0x800F4476u, (uint32_t)area);
    AeModeHooks hooks = g->mode;
    hooks.stack_pointer = g->call_stack_pointer;
    /* Native file I/O has supplied the member; enter through the original
     * render-init -> StageRequest(1, area, entry) -> flip-reset setup. */
    ae_StageRenderInit(g->ram, &hooks);
    ae_game_call(g, 0x8005D100u, 1u, (uint32_t)area, (uint32_t)entry, 0);
    ae_ResetFrameFlip(g->ram);
    return 0;
}

int ae_game_frame(AeGame *g) {
    int running = ae_mode_stage_frame(g->ram, &g->mode);
    if (running && ae_rd8(g->ram, 0x800F447Cu) == 6u && g->host.transition_present)
        g->host.transition_present(g->host.user, g->ram);
    return running;
}

/* Platform calls retain their named native interfaces. Every other call goes
 * through the central table; missing gameplay remains a visible named gap. */
int32_t ae_game_mode_call(void *user, AeRam *r, const char *name, uint32_t address,
                          uint32_t count, const uint32_t *args) {
    AeGame *g = user;
    uint32_t a[4] = {0};
    if (count > 4u) return ae_game_missing_words(g, name, address, count, args);
    for (uint32_t i = 0; i < count; ++i) a[i] = args[i];
    switch (address) {
    case 0x8005DF48u: return FRAME_CALL(BeginFrame, address, a[0], a[1], a[2], a[3]);
    case 0x80043A8Cu: return FRAME_CALL(DrawTerrain, address, a[0], a[1], a[2], a[3]);
    case 0x80016F34u: return FRAME_CALL(DrawAreaProps, address, a[0], a[1], a[2], a[3]);
    case 0x8005DEE0u: return FRAME_CALL(QueueFrameFlip, address, a[0], a[1], a[2], a[3]);
    case 0x8005DE8Cu: return FRAME_CALL(WaitFrameFlip, address, a[0], a[1], a[2], a[3]);
    case 0x8002E520u:
        if (g->host.platform.VSync) return g->host.platform.VSync(g->host.user, r, a[0]);
        ae_ram_fail(r, "Mode: missing native VSync boundary"); return 0;
    case 0x800286CCu:
        if (g->host.platform.DrawSync) return g->host.platform.DrawSync(g->host.user, r, a[0]);
        ae_ram_fail(r, "Mode: missing native DrawSync boundary"); return 0;
    case 0x80028850u:
        if (g->host.platform.ClearImage) return g->host.platform.ClearImage(g->host.user, r, a[0], a[1], a[2], a[3]);
        ae_ram_fail(r, "Mode: missing native ClearImage boundary"); return 0;
    case 0x80028EFCu:
        if (g->host.platform.PutDispEnv) return g->host.platform.PutDispEnv(g->host.user, r, a[0]);
        ae_ram_fail(r, "Mode: missing native PutDispEnv boundary"); return 0;
    case 0x80028D30u:
        if (g->host.platform.PutDrawEnv) return g->host.platform.PutDrawEnv(g->host.user, r, a[0]);
        ae_ram_fail(r, "Mode: missing native PutDrawEnv boundary"); return 0;
    case 0x80028CC0u:
        if (g->host.platform.DrawOTag) return g->host.platform.DrawOTag(g->host.user, r, a[0]);
        ae_ram_fail(r, "Mode: missing native DrawOTag boundary"); return 0;
    case 0x8005CFECu:
        if (g->host.platform.InitDisplay) return g->host.platform.InitDisplay(g->host.user, r);
        ae_ram_fail(r, "Mode: missing native InitDisplay boundary"); return 0;
    case 0x8005CD50u:
        if (g->host.platform.InitSystem) return g->host.platform.InitSystem(g->host.user, r);
        ae_ram_fail(r, "Mode: missing native InitSystem boundary"); return 0;
    case 0x8002EDF0u:
        if (g->host.platform.SetVideoMode) return g->host.platform.SetVideoMode(g->host.user, r, a[0]);
        ae_ram_fail(r, "Mode: missing native SetVideoMode boundary"); return 0;
    case 0x80028350u:
        if (g->host.platform.ResetGraph) return g->host.platform.ResetGraph(g->host.user, r, a[0]);
        ae_ram_fail(r, "Mode: missing native ResetGraph boundary"); return 0;
    case 0x80011A2Cu:
        if (g->host.platform.RunMinigameExe) return g->host.platform.RunMinigameExe(g->host.user, r, a[0]);
        ae_ram_fail(r, "Mode: missing native RunMinigameExe boundary"); return 0;
    case 0x800112E4u:
        if (g->host.archive.ArcLoadMember) return g->host.archive.ArcLoadMember(g->host.user, r, a[0], a[1]);
        ae_ram_fail(r, "Mode: missing native ArcLoadMember boundary"); return 0;
    case 0x8001256Cu:
        if (g->host.archive.CdMgrWaitIdle) return g->host.archive.CdMgrWaitIdle(g->host.user, r, a[0]);
        ae_ram_fail(r, "Mode: missing native CdMgrWaitIdle boundary"); return 0;
    case 0x80011104u:
        if (g->host.archive.LoadListInit) return g->host.archive.LoadListInit(g->host.user, r, a[0], a[1]);
        ae_ram_fail(r, "Mode: missing native LoadListInit boundary"); return 0;
    case 0x800116C0u:
        if (g->host.archive.LoadListStep) return g->host.archive.LoadListStep(g->host.user, r);
        ae_ram_fail(r, "Mode: missing native LoadListStep boundary"); return 0;
    case 0x800125B8u:
        if (g->host.archive.CdMgrUpdate) return g->host.archive.CdMgrUpdate(g->host.user, r);
        ae_ram_fail(r, "Mode: missing native CdMgrUpdate boundary"); return 0;
    default:
        if (ae_game_find_call(address)) return ae_game_call(g, address, a[0], a[1], a[2], a[3]);
        return ae_game_missing_words(g, name, address, count, args);
    }
}
