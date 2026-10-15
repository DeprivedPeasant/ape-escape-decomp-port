/* Per-frame dispatch and player-effects orchestration. */
#include "ae_effects.h"

static void effects_dispatch_call(AeRam *r, const AeEffectsHooks *hooks,
                                  AeEffectsCallFn callback, const char *name) {
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.%s not provided", name);
        return;
    }
    (void)callback(hooks->user, r, 0, 0, 0, 0);
}

/* PlayerEffectsUpdate @8006B800: update the player's short trail first, then
 * gate water, tint, and trail-point effects with the stage-exit predicate. */
void ae_PlayerEffectsUpdate(AeRam *r, const AeSpawnHooks *spawn,
                            const AeEffectsHooks *hooks) {
    ae_PlayerFxTrailUpdate(r, spawn);
    if (ae_ram_failed(r)) return;

    if (!ae_PlayerCanExitStage(r)) return;

    ae_PlayerWaterEffectsUpdate(r, spawn, hooks);
    if (ae_ram_failed(r)) return;

    ae_PlayerTintEffectUpdate(r, spawn, hooks);
    if (ae_ram_failed(r)) return;

    ae_TrailPointsDraw(r, hooks);
}

/* EffectsUpdateAll @8006A9F8: preserve the game's once-per-frame subsystem
 * order while calling completed ports directly and leaving unfinished
 * subsystems at their named boundaries. */
void ae_EffectsUpdateAll(AeRam *r, const AeSpawnHooks *spawn,
                         const AeEffectsHooks *hooks) {
    ae_FlyingMeshesUpdateDraw(r, spawn, hooks);
    if (ae_ram_failed(r)) return;

    ae_EffectObjsUpdate(r, hooks);
    if (ae_ram_failed(r)) return;
    ae_RandomParticlesUpdate(r, hooks);
    if (ae_ram_failed(r)) return;
    effects_dispatch_call(r, hooks, hooks == NULL ? NULL : hooks->Gadget4TrailUpdate,
                          "Gadget4TrailUpdate");
    if (ae_ram_failed(r)) return;

    ae_TrailsUpdate(r, hooks);
    if (ae_ram_failed(r)) return;
    ae_FxPlayerState6Update(r, hooks);
    if (ae_ram_failed(r)) return;
    ae_GroundSparksUpdateDraw(r, spawn, hooks);
    if (ae_ram_failed(r)) return;
    effects_dispatch_call(r, hooks, hooks == NULL ? NULL : hooks->GadgetFlashUpdate,
                          "GadgetFlashUpdate");
    if (ae_ram_failed(r)) return;
    ae_SpriteEffectsDraw(r, hooks);
    if (ae_ram_failed(r)) return;

    ae_PlayerEffectsUpdate(r, spawn, hooks);
}
