#include "../../src/core/ae_ram.h"
#include "../../src/bodies/ae_bodymove.h"
#include "../../src/bodies/ae_probe.h"

struct AeSoundHw;
struct AeGimmickAHooks;
typedef struct AeSoundHw AeSoundHw;
typedef struct AeGimmickAHooks AeGimmickAHooks;
typedef struct AePlayer2Hooks AePlayer2Hooks;

/* The focused checker links ae_hud.c to use its already-ported RadarUsable
 * leaf. These unrelated HUD draw paths are discarded at runtime. */
int32_t ae_AreaFeature30GetState(AeRam *r)
{
    (void)r;
    return 0;
}

int32_t ae_MessageBoxBusy(AeRam *r)
{
    return ae_rd8(r, 0x800F5A00u);
}

/* StageRequest is exercised here only with an active-transition guard, before
 * any area-load collision update can run. These test-only symbols satisfy the
 * unrelated CollisionUpdate sections pulled in by ae_bodies.c. */
int32_t ae_PlayerMoveRumbleSoundUpdate(AeRam *r, const AeBodyHooks *h, uint32_t body)
{ (void)r; (void)h; (void)body; return 0; }
int32_t ae_PlayerVehicleSoundUpdate(AeRam *r, const AeBodyHooks *h, uint32_t body)
{ (void)r; (void)h; (void)body; return 0; }
int32_t ae_BodyProbeGround(AeRam *r, const AeWorldHooks *h, uint32_t body)
{ (void)r; (void)h; (void)body; return 0; }
int32_t ae_CollisionStub_ReturnZero(AeRam *r, uint32_t body)
{ (void)r; (void)body; return 0; }
int32_t ae_CollisionBuildProbePoints(AeRam *r, int32_t type, uint32_t body)
{ (void)r; (void)type; (void)body; return 0; }
int32_t ae_CollisionResolveProbes(AeRam *r, const AeBodyMoveHooks *h,
                                  int32_t type, uint32_t body)
{ (void)r; (void)h; (void)type; (void)body; return 0; }
int32_t ae_BodyLandOnGround(AeRam *r, const AeBodyMoveHooks *h,
                            uint32_t body, int32_t type, int32_t ceiling)
{ (void)r; (void)h; (void)body; (void)type; (void)ceiling; return 0; }
void ae_PlayerTryLedgeGrab(AeRam *r, const AeBodyMoveHooks *h)
{ (void)r; (void)h; }
int32_t ae_BodyRecordColliderContact(AeRam *r, const AeBodyMoveHooks *h,
                                     uint32_t body, int32_t type)
{ (void)r; (void)h; (void)body; (void)type; return 0; }

/* Only DrawTextSprites and SetDAT are retained from these large modules in
 * this focused checker. These unrelated progress/menu branches are inert. */
int32_t ae_SoundLoopStop(AeRam *r, const AeSoundHw *sound_hw,
                         int32_t id, int32_t voice)
{ (void)r; (void)sound_hw; (void)id; (void)voice; return 0; }
int32_t ae_BgmSeqSetVolume(AeRam *r, const AeSoundHw *sound_hw,
                           int32_t track, int32_t volume)
{ (void)r; (void)sound_hw; (void)track; (void)volume; return 0; }
int32_t ae_GimmickSaveStates(AeRam *r) { (void)r; return 0; }
int32_t ae_GimmickSaveApplyProgress(AeRam *r, const AeGimmickAHooks *h)
{ (void)r; (void)h; return 0; }
int32_t ae_PlayerSelectGadget(AeRam *r, const AePlayer2Hooks *hooks,
                              int32_t gadget, int32_t previous_gadget)
{ (void)r; (void)hooks; (void)gadget; (void)previous_gadget; return 0; }

/* The tested spiral fade path does not invoke panel transitions. */
int32_t ae_HudPanelB_Start(AeRam *r, int32_t mode, int32_t argument)
{ (void)r; (void)mode; (void)argument; return 0; }
