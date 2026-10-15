/* Game host: one PSX RAM image with every ported RAM-image module wired to the others.
 *
 * Each ported module (src/monkeys, src/world, src/bodies, src/effects, src/sound, src/core pose / SDK maths,
 * src/player/ae_anim_events) reaches code outside itself through a hook table, so that its differential checker can
 * stand in for the rest of the game. This module fills those tables for the real game: a hook whose original function
 * is ported calls the port directly; a hook that belongs to a native system (rendering, the sound chip, debug menus,
 * the vertical-blank counter) goes to the AeGameHost callbacks; a hook whose original game logic is NOT ported yet
 * goes to `missing`, which reports it (by original name and address) and returns 0, so gaps are visible rather than
 * silently approximated.
 *
 * Frame order follows Mode_Stage (8005BF70): ... PlayerUpdate ... MonkeyUpdateAll ... GimmickUpdateAll ...
 * CollisionUpdate. Only the ported systems run; see ae_game_frame. */
#ifndef AE_GAME_H
#define AE_GAME_H

#include "../core/ae_ram.h"
#include "../monkeys/ae_monkey.h"
#include "../bodies/ae_bodies.h"
#include "../world/ae_world.h"
#include "../world/ae_solid.h"
#include "../world/ae_gimmick.h"
#include "../effects/ae_spawn.h"
#include "../sound/ae_sound.h"
#include "../player/ae_anim_events.h"
#include "ae_modes.h"
#include "ae_game_modules.h"
#include "ae_game_platform.h"
#include "ae_game_frame_host.h"

/* Native systems the game logic talks to. Every callback may be NULL (then it does nothing / returns 0). */
typedef struct AeGameHost {
    void *user;
    AeGamePlatform platform;
    AeGameRenderer renderer;
    AeGameArchive archive;
    AeGameFrameHost frame;
    /* Public MCX APIs from member 186; native backend owns card protocol/I/O. */
    int32_t (*card_call)(void *user, AeRam *ram, const char *name, uint32_t address,
                         uint32_t count, const uint32_t *words);
    /* Synchronous renderer request; the draw pointer is valid for this call. */
    void (*hud_draw)(void *user, AeRam *ram, const AeHudDraw *draw);
    /* Member bytes remain owned by the host; copied before this callback's next use. */
    int (*area_member)(void *user, int32_t area, const uint8_t **bytes, size_t *size);
    /* Return 1 for an overlay, 0 for none, or -1 if unavailable. The member
     * image contains data used by native ports; its instructions are never run. */
    int (*area_overlay)(void *user, int32_t area, uint32_t *member, uint32_t *address,
                        const uint8_t **bytes, size_t *size);
    /* Called before each main mode entry; zero ends a bounded native run. */
    int (*mode_continue)(void *user, AeRam *ram);
    void (*transition_present)(void *user, AeRam *ram);
    /* An original game-logic function that is not ported yet was called (name, address); return value goes back to
       the caller. Default (NULL): print each name once to stderr and return 0. */
    int32_t (*missing)(void *user, AeRam *r, const char *name, uint32_t address,
                       uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
    /* Extended callback words, including original stack arguments. */
    int32_t (*missing_words)(void *user, AeRam *r, const char *name, uint32_t address,
                             uint32_t count, const uint32_t *words);
    /* Rendering hooks of the game logic (model transform / draw, shadows, alert icons, GTE matrix loads, colour
       fades, projected positions): the native renderer draws from RAM state instead, so these are notifications. */
    int32_t (*render)(void *user, AeRam *r, const char *name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
    int32_t (*render_words)(void *user, AeRam *r, const char *name, uint32_t address,
                            uint32_t count, const uint32_t *args);
    /* Debug text / printf / debug-menu registration (format strings live in game RAM). */
    void (*debug)(void *user, AeRam *r, const char *name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
    /* VSync(-1): the vertical-blank counter (the game seeds its monkey RNG from it). */
    int32_t (*vblank_count)(void *user);
    /* PlayerUpdate exposes original unwritten locals and the unused caller
     * argument of the lethal-ground voice call as explicit host services. */
    uint32_t (*body_direction_padding)(void *user, AeRam *ram);
    uint32_t (*player_update_locals)(void *user, AeRam *ram);
    uint32_t (*player_voice_fallback)(void *user, AeRam *ram);
    uint32_t (*player_actor_camera_table)(void *user, AeRam *ram);
    /* Explicit caller residue used by PauseMenuRun's mode-3 exit. Native
     * sessions choose zero; differential hosts can supply their caller input. */
    uint32_t pause_menu_caller_s5;
    /* XaPlay copies untouched high bytes of its byte locals into CD queue
     * words: selector 0 supplies filter bits 16..31, 1 mode bits 8..31. */
    uint32_t (*xa_caller_padding)(void *user, AeRam *ram, uint32_t local);
    /* Sound chip boundary (src/sound AeSoundHw contract, notes/port/sound.md). NULL members are no-ops. */
    AeSoundHw sound_hw;
} AeGameHost;

typedef struct AeGame {
    AeRam *ram;
    AeGameHost host;
    AeMonkeyHooks monkey;
    AeBodyHooks body;
    AeWorldHooks world;
    AeSolidHooks solid;
    AeGimmickHooks gimmick;
    AeSpawnHooks spawn;
    AeAnimEventHooks anim;
    AE_GAME_MODULE_FIELDS
    uint32_t call_stack_pointer; /* Original o32 entry SP for extra arguments. */
    uint32_t loaded_overlay;
    struct {
        void *user;
        void (*select_gadget)(void *, AeRam *, int32_t, int32_t);
        void (*hud_find_icon_pos)(void *, AeRam *);
    } legacy_gadget;
    uint32_t missing_addresses[1024];
    const char *missing_names[1024]; /* Callers supply static function names. */
    size_t missing_address_count;
    uint32_t missing_seen[4];   /* which missing functions were already reported (bit per entry, see ae_game.c) */
} AeGame;

/* Wire all hook tables of `g` for RAM image `ram` (not owned). `host` may be NULL. */
void ae_game_init(AeGame *g, AeRam *ram, const AeGameHost *host);
/* Historical test fixtures isolate the original seven-table integration. */
void ae_game_init_legacy(AeGame *g, AeRam *ram, const AeGameHost *host);

/* One Mode_Stage iteration in original order, with native host boundaries.
   Returns 0 on a stage fade exit or when the RAM image raised its error flag. */
int ae_game_frame(AeGame *g);
/* Run original main with native archive/display services. ArcLoadMember copies
 * its member to the requested RAM destination and uses ae_ram_fail on failure. */
int32_t ae_game_run(AeGame *g);
int32_t ae_game_load_area(AeGame *g, int32_t area, int32_t entry);
/* Named mode subsystem adapter; used by the generated mode hook table. */
int32_t ae_game_mode_call(void *user, AeRam *r, const char *name, uint32_t address,
                          uint32_t count, const uint32_t *args);
/* Legacy integration slice retained for the original checker fixtures. */
int ae_game_logic_frame(AeGame *g);

#endif
