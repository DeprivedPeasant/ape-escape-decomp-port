/* Game host: wires the hook tables of the ported RAM-image modules to each other (see ae_game.h). */
#include "ae_game.h"
#include "ae_calls.h"
#include "../core/ae_pose.h"
#include "../core/ae_sdk_matrix.h"
#include "../bodies/ae_probe.h"
#include "../bodies/ae_bodymove.h"

#include <stdio.h>
#include <string.h>

/* Scratch area for arguments the original passes by stack address (for example PlaySfxAt's position vector) when the
   caller hands the hook a host array. It lies in the stack window every differential checker excludes. */
#define AE_GAME_ARG_SCRATCH 0x801FFE00u

static AeGame *game_of(void *user) { return (AeGame *)user; }

int32_t ae_game_missing_call(AeGame *g, const char *name, uint32_t address,
                             uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    /* The host models each invocation; the default diagnostic reports each address once. */
    if (g->host.missing)
        return g->host.missing(g->host.user, g->ram, name, address, a0, a1, a2, a3);
    for (size_t i = 0; i < g->missing_address_count; ++i)
        if (g->missing_addresses[i] == address && strcmp(g->missing_names[i], name) == 0) return 0;
    if (g->missing_address_count < sizeof(g->missing_addresses) / sizeof(g->missing_addresses[0])) {
        size_t index = g->missing_address_count++;
        g->missing_addresses[index] = address;
        g->missing_names[index] = name;
    }
    fprintf(stderr, "ae_game: original function not ported yet: %s @%08X (returns 0)\n", name, address);
    return 0;
}

int32_t ae_game_missing_words(AeGame *g, const char *name, uint32_t address,
                               uint32_t count, const uint32_t *words) {
    if (g->host.missing_words)
        return g->host.missing_words(g->host.user, g->ram, name, address, count, words);
    return ae_game_missing_call(g, name, address, words[0], words[1], words[2], words[3]);
}

/* ---- original game logic that is not ported yet ------------------------------------------------------------- */

/* Every unported function a hook can reach, in a fixed order: the index is the bit in AeGame.missing_seen. */
static const struct { const char *name; uint32_t address; } MISSING[] = {
    {"ModelInit", 0x80015AC0u},           {"PlaySfx", 0x800877F8u},           {"PlayPlayerVoice", 0x8006896Cu},
    {"SoundLoopStart", 0x800878E0u},
    {"CollisionRayTest", 0x80078C9Cu},    {"GimmickTypeToSfx", 0x8005BB98u},  {"PickupCollect", 0x80078A18u},
    {"CameraNudge", 0x80047690u},         {"EventCamStart", 0x8004D39Cu},     {"FUN_8013d358 (area 0x29 overlay)", 0x8013D358u},
    {"FUN_801414a0 (gimmick overlay)", 0x801414A0u}, {"PlayerEventCallback", 0x800B027Cu},
    {"XaPlay", 0x80011F6Cu},
};
enum { M_ModelInit, M_PlaySfx, M_PlayPlayerVoice, M_SoundLoopStart, M_CollisionRayTest,
       M_GimmickTypeToSfx, M_PickupCollect, M_CameraNudge, M_EventCamStart, M_Ovl8013d358, M_Ovl801414a0,
       M_PlayerEventCallback, M_XaPlay, M_COUNT };

static int32_t missing(AeGame *g, int which, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (g->host.missing)
        return g->host.missing(g->host.user, g->ram, MISSING[which].name, MISSING[which].address, a0, a1, a2, a3);
    uint32_t bit = 1u << (which & 31);
    if (!(g->missing_seen[which >> 5] & bit)) {
        g->missing_seen[which >> 5] |= bit;
        fprintf(stderr, "ae_game: original function not ported yet: %s @%08X (returns 0)\n",
                MISSING[which].name, MISSING[which].address);
    }
    return 0;
}

static int32_t render(AeGame *g, const char *name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return g->host.render ? g->host.render(g->host.user, g->ram, name, a0, a1, a2, a3) : 0;
}

static void debug(AeGame *g, const char *name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (g->host.debug) g->host.debug(g->host.user, g->ram, name, a0, a1, a2, a3);
}

/* ---- monkey hooks ---------------------------------------------------------------------------------------------- */

#define MONKEY_HOOK(NAME) static int32_t mk_##NAME(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
#define UNUSED_ARGS (void)a0; (void)a1; (void)a2; (void)a3

MONKEY_HOOK(Printf)           { (void)r; debug(game_of(user), "Printf", a0, a1, a2, a3); return 0; }
MONKEY_HOOK(GimmickGetInfo)   { (void)user; (void)a2; (void)a3; return ae_GimmickGetInfo(r, (int32_t)a0, a1); }
MONKEY_HOOK(PlaySfxAt)        { (void)a2; (void)a3; return ae_PlaySfxAt(r, &game_of(user)->host.sound_hw, (int32_t)a0, a1); }
MONKEY_HOOK(ProjectileSpawn)  { return ae_ProjectileSpawn(r, &game_of(user)->spawn, a0, a1, (int32_t)a2, a3); }
MONKEY_HOOK(SpawnParticle)    { (void)a2; (void)a3; ae_SpawnParticle(r, &game_of(user)->spawn, (int32_t)a0, a1); return 0; }
MONKEY_HOOK(TargetListAdd)    { (void)user; (void)a1; (void)a2; (void)a3; return ae_TargetListAdd(r, a0); }
MONKEY_HOOK(TargetListRemove) { (void)user; (void)a1; (void)a2; (void)a3; ae_TargetListRemove(r, a0); return 0; }
MONKEY_HOOK(MatrixFromDirXZ)  { (void)user; (void)a2; (void)a3; return (int32_t)ae_sdk_MatrixFromDirXZ(r, a0, a1); }
MONKEY_HOOK(ModelPoseBones)   { (void)user; (void)a2; (void)a3; ae_ModelPoseBones(r, a0, a1); return 0; }
MONKEY_HOOK(EffectBigBurst)   { (void)a3; ae_EffectBigBurst(r, &game_of(user)->spawn, (int32_t)a0, (int32_t)a1, (int32_t)a2); return 0; }
MONKEY_HOOK(SparkAddRandom)   { (void)user; (void)a3; ae_SparkAddRandom(r, a0, a1, (int32_t)a2); return 0; }
MONKEY_HOOK(ModelInit)        { (void)r; return missing(game_of(user), M_ModelInit, a0, a1, a2, a3); }
MONKEY_HOOK(GetGroundHeight)  { (void)a3; return ae_GetGroundHeight(r, &game_of(user)->world, (int32_t)a0, (int32_t)a1, (int32_t)a2); }
MONKEY_HOOK(ObjSlotGetAnimIndex) { (void)user; (void)a1; (void)a2; (void)a3; return ae_ObjSlotGetAnimIndex(r, (int32_t)a0); }
MONKEY_HOOK(ObjSlotSetField_CF)  { (void)user; (void)a2; (void)a3; ae_ObjSlotSetField_CF(r, (int32_t)a0, a1); return 0; }
MONKEY_HOOK(ObjSlotGetTarget)    { (void)user; (void)a2; (void)a3; return ae_ObjSlotGetTarget(r, a0, a1); }
MONKEY_HOOK(SoundLoopStartEx)  { (void)a3; return ae_SoundLoopStartEx(r, &game_of(user)->host.sound_hw, (int32_t)a0, (int32_t)a1, (int32_t)a2); }
MONKEY_HOOK(SoundLoopStopAndFree) { (void)a2; (void)a3; return ae_SoundLoopStopAndFree(r, &game_of(user)->host.sound_hw, (int32_t)a0, (int32_t)a1); }
MONKEY_HOOK(SpawnPuff)        { (void)a1; (void)a2; (void)a3; ae_SpawnPuff(r, &game_of(user)->spawn, a0); return 0; }
MONKEY_HOOK(MatrixLookDir)    { (void)user; (void)a2; (void)a3; return (int32_t)ae_sdk_MatrixLookDir(r, a0, a1); }
MONKEY_HOOK(ScaleMatrix)      { (void)user; (void)a2; (void)a3; return (int32_t)ae_sdk_ScaleMatrix(r, a0, a1); }
MONKEY_HOOK(ModelBuildBoneMatrices) { (void)user; (void)a2; (void)a3; ae_ModelBuildBoneMatrices(r, a0, a1); return 0; }
MONKEY_HOOK(GimmickEvent)     { (void)a3; return ae_GimmickEvent(r, &game_of(user)->world, a0, (int32_t)a1, a2); }
MONKEY_HOOK(ActorAnimDecode)  { (void)user; (void)a1; (void)a2; (void)a3; return (int32_t)ae_ActorAnimDecode(r, a0); }
MONKEY_HOOK(ClearWords)       { (void)user; (void)a2; (void)a3; ae_ClearWords(r, a0, (int32_t)a1); return 0; }
MONKEY_HOOK(StageExitIsActive){ (void)user; UNUSED_ARGS; return ae_StageExitIsActive(r); }
MONKEY_HOOK(VSync) {
    (void)r; UNUSED_ARGS;
    AeGame *g = game_of(user);
    return g->host.vblank_count ? g->host.vblank_count(g->host.user) : 0;
}

/* Native-system notifications: rendering and the debug menu. */
#define RENDER_HOOK(NAME) MONKEY_HOOK(NAME) { (void)r; return render(game_of(user), #NAME, a0, a1, a2, a3); }
#define DEBUG_HOOK(NAME)  MONKEY_HOOK(NAME) { (void)r; debug(game_of(user), #NAME, a0, a1, a2, a3); return 0; }
RENDER_HOOK(ModelTransformRigid)
RENDER_HOOK(ModelDraw)
RENDER_HOOK(SetRotMatrix)
RENDER_HOOK(SetTransMatrix)
RENDER_HOOK(RotTransPers)
RENDER_HOOK(AlertIconDraw)
RENDER_HOOK(ModelTransformSkinned)
RENDER_HOOK(ModelDrawWithOt)
RENDER_HOOK(DrawShadow)
RENDER_HOOK(ColorFadeSet)
DEBUG_HOOK(MenuItemInitBound)
DEBUG_HOOK(MenuItemSetRange)
DEBUG_HOOK(MenuItemSetCallback)
DEBUG_HOOK(MenuItemSetStep)
DEBUG_HOOK(DebugTextPrintfWhite)

/* ---- world, gimmick and spawn hooks ---------------------------------------------------------------------------- */

#define HOOK6(PREFIX, NAME) static int32_t PREFIX##_##NAME(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)

HOOK6(world, GetSolidHeight)  { (void)a3; return ae_GetSolidHeight(r, &game_of(user)->solid, (int32_t)a0, (int32_t)a1, (int32_t)a2); }
HOOK6(world, PlaySfxAt)       { (void)a2; (void)a3; return ae_PlaySfxAt(r, &game_of(user)->host.sound_hw, (int32_t)a0, a1); }
HOOK6(world, GimmickActivate) { (void)a2; (void)a3; return ae_GimmickActivate(r, &game_of(user)->gimmick, (int32_t)a0, a1); }
HOOK6(world, MeshInitFromAreaTable) { (void)r; return render(game_of(user), "MeshInitFromAreaTable", a0, a1, a2, a3); }
HOOK6(world, Ovl8013d358)     { (void)r; return missing(game_of(user), M_Ovl8013d358, a0, a1, a2, a3); }
HOOK6(world, CameraShake)     { (void)user; (void)a1; (void)a2; (void)a3; ae_CameraShake(r, (int32_t)a0); return 0; }
HOOK6(world, EventCamStart)   { (void)r; return missing(game_of(user), M_EventCamStart, a0, a1, a2, a3); }

HOOK6(gimmick, PlaySfx)       { (void)a1; (void)a2; (void)a3; return ae_PlaySfx(r, &game_of(user)->host.sound_hw, (int32_t)a0); }
HOOK6(gimmick, PropSetMesh)   { (void)r; return render(game_of(user), "PropSetMesh", a0, a1, a2, a3); }
HOOK6(gimmick, Ovl801414a0)   { (void)r; return missing(game_of(user), M_Ovl801414a0, a0, a1, a2, a3); }

HOOK6(spawn, PlaySfx)         { (void)a1; (void)a2; (void)a3; return ae_PlaySfx(r, &game_of(user)->host.sound_hw, (int32_t)a0); }
HOOK6(spawn, MatrixLookDir)   { (void)user; (void)a2; (void)a3; return (int32_t)ae_sdk_MatrixLookDir(r, a0, a1); }
HOOK6(spawn, MatrixFromDirXZ) { (void)user; (void)a2; (void)a3; return (int32_t)ae_sdk_MatrixFromDirXZ(r, a0, a1); }

/* The original's group / burst position locals (SpawnEffectGroup 800696EC, EffectBigBurst 8006B3F0) are s16[3] in an
   eight-byte local whose fourth halfword is never written, and the eight-byte position copies take it along into the
   particle record. The original leaves leftover stack there; the native build supplies zero. */
static uint16_t spawn_position_padding(void *user, AeRam *r, uint32_t original_function) {
    (void)user; (void)r; (void)original_function;
    return 0;
}

/* ---- collision-body hooks -------------------------------------------------------------------------------------- */

static void body_printf_error(void *user, AeRam *r, uint32_t fmt, int32_t type) {
    (void)r; debug(game_of(user), "Printf", fmt, (uint32_t)type, 0, 0);
}
static int32_t body_sound_loop_start(void *user, AeRam *r, int32_t a, int32_t id) {
    return ae_SoundLoopStart(r, &game_of(user)->host.sound_hw, a, id);
}
static void body_sound_loop_stop(void *user, AeRam *r, int32_t id, int32_t handle) {
    AeGame *g = game_of(user);
    (void)ae_SoundLoopStop(r, &g->host.sound_hw, id, handle);
}
static void body_sfx_voice_set_pitch(void *user, AeRam *r, int32_t voice, int32_t pitch) {
    ae_SfxVoiceSetPitch(r, &game_of(user)->missb, voice, pitch);
}
static void body_xa_play(void *user, AeRam *r, int32_t index) {
    (void)r; ae_game_call(game_of(user), 0x80011F6Cu, (uint32_t)index, 0, 0, 0);
}
static void body_build_probe_points(void *user, AeRam *r, int32_t type, uint32_t body) {
    (void)user; ae_CollisionBuildProbePoints(r, type, body);
}
static int32_t body_collision_ray_test(void *user, AeRam *r, uint32_t arg, uint32_t params, int16_t out[2]) {
    (void)r; out[0] = out[1] = 0;
    return missing(game_of(user), M_CollisionRayTest, arg, params, 0, 0);
}
static int32_t body_gimmick_type_to_sfx(void *user, AeRam *r, int32_t collider_type) {
    (void)r; return missing(game_of(user), M_GimmickTypeToSfx, (uint32_t)collider_type, 0, 0, 0);
}
static void body_play_sfx_at(void *user, AeRam *r, int32_t sfx, const int16_t pos[3]) {
    /* the original passes the position by stack address */
    for (uint32_t axis = 0; axis < 3; ++axis) ae_wr16(r, AE_GAME_ARG_SCRATCH + 2 * axis, (uint16_t)pos[axis]);
    ae_PlaySfxAt(r, &game_of(user)->host.sound_hw, sfx, AE_GAME_ARG_SCRATCH);
}
static void body_pickup_collect(void *user, AeRam *r, int32_t type) {
    (void)r; missing(game_of(user), M_PickupCollect, (uint32_t)type, 0, 0, 0);
}
static void body_camera_nudge(void *user, AeRam *r, int32_t dx, int32_t dy, int32_t dz) {
    (void)r; missing(game_of(user), M_CameraNudge, (uint32_t)dx, (uint32_t)dy, (uint32_t)dz, 0);
}

/* ---- player animation-event hooks ------------------------------------------------------------------------------ */

static void anim_play_sfx(void *user, AeRam *r, int32_t id) { (void)ae_PlaySfx(r, &game_of(user)->host.sound_hw, id); }
static void anim_play_player_voice(void *user, AeRam *r, int32_t id) {
    (void)r; missing(game_of(user), M_PlayPlayerVoice, (uint32_t)id, 0, 0, 0);
}
static void anim_event_callback(void *user, AeRam *r, uint32_t function) {
    (void)r; missing(game_of(user), M_PlayerEventCallback, function, 0, 0, 0);
}

/* ---- wiring ---------------------------------------------------------------------------------------------------- */

void ae_game_wire_modules(AeGame *g);
void ae_game_wire_body(AeGame *g);
static void legacy_select_gadget(void *user, AeRam *r, int32_t gadget, int32_t old) {
    (void)r;
    ae_game_call(game_of(user), 0x8007AC64u, (uint32_t)gadget, (uint32_t)old, 0, 0);
}
static void legacy_hud_icon(void *user, AeRam *r) {
    (void)r;
    ae_game_call(game_of(user), 0x8008197Cu, 0, 0, 0, 0);
}

void ae_game_init_legacy(AeGame *g, AeRam *ram, const AeGameHost *host) {
    memset(g, 0, sizeof(*g));
    g->ram = ram;
    g->call_stack_pointer = 0x801FFF00u;
    if (host) g->host = *host;
    ae_game_wire_modules(g);
    g->legacy_gadget.user = g;
    g->legacy_gadget.select_gadget = legacy_select_gadget;
    g->legacy_gadget.hud_find_icon_pos = legacy_hud_icon;

    AeWorldHooks *w = &g->world;
    w->user = g;
    w->GetSolidHeight = world_GetSolidHeight;
    w->PlaySfxAt = world_PlaySfxAt;
    w->GimmickActivate = world_GimmickActivate;
    w->MeshInitFromAreaTable = world_MeshInitFromAreaTable;
    w->FUN_8013d358 = world_Ovl8013d358;
    w->CameraShake = world_CameraShake;
    w->EventCamStart = world_EventCamStart;
    /* GetGroundHeight's no-heightfield path and GetSolidHeight's edge cases read stack words the original leaves
       uninitialised (notes/port/world.md, solid.md); the native build supplies zero for them. */

    AeGimmickHooks *gm = &g->gimmick;
    gm->user = g;
    gm->PlaySfx = gimmick_PlaySfx;
    gm->PlaySfxAt = world_PlaySfxAt;
    gm->PropSetMesh = gimmick_PropSetMesh;
    gm->FUN_801414a0 = gimmick_Ovl801414a0;

    AeSpawnHooks *sp = &g->spawn;
    sp->user = g;
    sp->world = w;
    sp->PlaySfx = spawn_PlaySfx;
    sp->MatrixLookDir = spawn_MatrixLookDir;
    sp->MatrixFromDirXZ = spawn_MatrixFromDirXZ;
    sp->position_padding = spawn_position_padding;

    AeBodyHooks *b = &g->body;
    b->user = g;
    b->printf_error = body_printf_error;
    b->sound_loop_start = body_sound_loop_start;
    b->sound_loop_stop = body_sound_loop_stop;
    b->build_probe_points = body_build_probe_points;
    b->collision_ray_test = body_collision_ray_test;
    b->gimmick_type_to_sfx = body_gimmick_type_to_sfx;
    b->play_sfx_at = body_play_sfx_at;
    b->pickup_collect = body_pickup_collect;
    b->camera_nudge = body_camera_nudge;
    g->misc.PlaySfx = spawn_PlaySfx;
    b->move = g->body_move;
    b->move.stack_pointer = g->call_stack_pointer - 56u;
    b->move.user = g;
    b->move.SfxVoiceSetPitch = body_sfx_voice_set_pitch;
    b->move.XaPlay = body_xa_play;

    AeAnimEventHooks *a = &g->anim;
    a->user = g;
    a->play_sfx = anim_play_sfx;
    a->play_player_voice = anim_play_player_voice;
    a->event_callback = anim_event_callback;

    AeMonkeyHooks *m = &g->monkey;
    m->user = g;
    m->body = b;
#define WIRE(NAME) m->NAME = mk_##NAME
    WIRE(Printf); WIRE(GimmickGetInfo); WIRE(PlaySfxAt); WIRE(ProjectileSpawn); WIRE(SpawnParticle);
    WIRE(TargetListAdd); WIRE(TargetListRemove); WIRE(MatrixFromDirXZ); WIRE(ModelPoseBones);
    WIRE(ModelTransformRigid); WIRE(ModelDraw); WIRE(MenuItemInitBound); WIRE(MenuItemSetRange);
    WIRE(MenuItemSetCallback); WIRE(MenuItemSetStep); WIRE(EffectBigBurst); WIRE(SparkAddRandom); WIRE(ModelInit);
    WIRE(SetRotMatrix); WIRE(SetTransMatrix); WIRE(RotTransPers); WIRE(GetGroundHeight); WIRE(ObjSlotGetAnimIndex);
    WIRE(AlertIconDraw); WIRE(ObjSlotSetField_CF); WIRE(ObjSlotGetTarget); WIRE(SoundLoopStartEx);
    WIRE(SoundLoopStopAndFree); WIRE(SpawnPuff); WIRE(MatrixLookDir); WIRE(ScaleMatrix); WIRE(ModelBuildBoneMatrices);
    WIRE(ModelTransformSkinned); WIRE(ModelDrawWithOt); WIRE(GimmickEvent); WIRE(ActorAnimDecode); WIRE(DrawShadow);
    WIRE(ColorFadeSet); WIRE(DebugTextPrintfWhite); WIRE(ClearWords); WIRE(VSync); WIRE(StageExitIsActive);
#undef WIRE
    (void)M_COUNT;
}

void ae_game_init(AeGame *g, AeRam *ram, const AeGameHost *host) {
    ae_game_init_legacy(g, ram, host);
    ae_game_wire_modules(g);
    ae_game_wire_body(g);
}

int ae_game_logic_frame(AeGame *g) {
    /* Mode_Stage order (8005C1E0 .. 8005C26C): ... MonkeyUpdateAll ... GimmickUpdateAll ... CollisionUpdate. */
    ae_MonkeyUpdateAll(g->ram, &g->monkey);
    if (ae_ram_failed(g->ram)) return 0;
    ae_CollisionUpdate(g->ram, &g->body);
    return !ae_ram_failed(g->ram);
}
