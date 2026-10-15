/* Generated typed adapters; edit tools/generate_game_calls.py to regenerate. */
#include "ae_calls.h"
#include "ae_calls_special.h"
int32_t ae_CollProbe_Slope(AeRam *, const AeCollProbeHooks *, uint32_t, uint32_t, uint32_t);

/* ---- src/bodies/ae_bodies.c ---- */
static int32_t call_ae_BodyCheckPickupContact(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyCheckPickupContact(g->ram, &g->body, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_BodyCollideBodiesDetailed(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyCollideBodiesDetailed(g->ram, &g->body, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_BodyCollideWithBodies(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyCollideWithBodies(g->ram, &g->body, (uint32_t)a0);
}

static int32_t call_ae_BodyContactSfx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyContactSfx(g->ram, &g->body, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_BodyContactType(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyContactType(g->ram, (uint32_t)a0);
}

static int32_t call_ae_BodyFindOverlapType1(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyFindOverlapType1(g->ram, &g->body, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_BodyFindOverlapType3to5(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyFindOverlapType3to5(g->ram, &g->body, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_BodyTypeOfActor(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyTypeOfActor(g->ram, (uint32_t)a0);
}

static int32_t call_ae_CollisionBodiesReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionBodiesReset(g->ram);
}

static int32_t call_ae_CollisionBodyAlloc(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionBodyAlloc(g->ram);
}

static int32_t call_ae_CollisionBodyAllocSlot(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionBodyAllocSlot(g->ram);
}

static int32_t call_ae_CollisionBodyClear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionBodyClear(g->ram, (uint32_t)a0);
}

static int32_t call_ae_CollisionBodyFree(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionBodyFree(g->ram, (uint32_t)a0);
}

static int32_t call_ae_CollisionBodyGetCenter(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionBodyGetCenter(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollisionBodyIndexOf(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionBodyIndexOf(g->ram, (int32_t)a0);
}

static int32_t call_ae_CollisionBodyRegister(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionBodyRegister(g->ram, &g->body, (int32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollisionBodyUnregister(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionBodyUnregister(g->ram, &g->body, (uint32_t)a0);
}

static int32_t call_ae_CollisionUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionUpdate(g->ram, &g->body);
}

static int32_t call_ae_GetObjectByIndex(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GetObjectByIndex(g->ram, (uint32_t)a0);
}

static int32_t call_ae_LoopSlotAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_LoopSlotAdd(g->ram, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_LoopSlotClearAll(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_LoopSlotClearAll(g->ram);
}

static int32_t call_ae_LoopSlotRef(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_LoopSlotRef(g->ram, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_LoopSlotSweep(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_LoopSlotSweep(g->ram);
}

static int32_t call_ae_VectorNormalS(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_VectorNormalS(g->ram, (uint32_t)a0, (uint32_t)a1);
}

/* ---- src/bodies/ae_bodymove.c ---- */
static int32_t call_ae_BodyLandOnGround(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyLandOnGround(g->ram, &g->body_move, (uint32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_BodyRecordColliderContact(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyRecordColliderContact(g->ram, &g->body_move, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ColliderSlopeContactDir(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderSlopeContactDir(g->ram, &g->body_move, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollisionResolveProbes(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionResolveProbes(g->ram, &g->body_move, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_CollisionStub_ReturnZero(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionStub_ReturnZero(g->ram, (uint32_t)a0);
}

static int32_t call_ae_PlayerMoveRumbleSoundUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerMoveRumbleSoundUpdate(g->ram, &g->body, (uint32_t)a0);
}

static int32_t call_ae_PlayerTryLedgeGrab(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerTryLedgeGrab(g->ram, &g->body_move);
    return 0;
}

static int32_t call_ae_PlayerVehicleSoundUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerVehicleSoundUpdate(g->ram, &g->body, (uint32_t)a0);
}

static int32_t call_ae_SignedRangeOverlap(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SignedRangeOverlap((int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_SoundEmitterPan(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundEmitterPan(g->ram, &g->body_move, (int32_t)a0, (int32_t)a1, (int32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_SoundEmitterPitch(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundEmitterPitch(g->ram, &g->body_move, (int32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_SoundLoopStop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundLoopStop(g->ram, &g->host.sound_hw, (int32_t)a0, (int32_t)a1);
}

/* ---- src/bodies/ae_probe.c ---- */
static int32_t call_ae_BodyProbeGround(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BodyProbeGround(g->ram, &g->world, (uint32_t)a0);
}

static int32_t call_ae_CollisionBuildProbePoints(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionBuildProbePoints(g->ram, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_CollisionPointInGrid(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionPointInGrid(g->ram, (uint32_t)a0);
}

static int32_t call_ae_CollisionShiftProbePoints(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollisionShiftProbePoints(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_PointInBoxXYZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PointInBoxXYZ(g->ram, (uint32_t)a0, (uint32_t)a1);
}

/* ---- src/camera/ae_camera.c ---- */
static int32_t call_ae_CameraFollowAimedVariant(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraFollowAimedVariant(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraFollowDispatch(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraFollowDispatch(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraFollowGadgetVariant(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraFollowGadgetVariant(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraFollowUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraFollowUpdate(g->ram, &g->camera, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_CameraKind3FollowUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraKind3FollowUpdate(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraKindAimUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraKindAimUpdate(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraKindAim_SnapToFocus(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraKindAim_SnapToFocus(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraKindFixedUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraKindFixedUpdate(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraKindFixed_Nop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraKindFixed_Nop(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraSlotAimAtFocus(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotAimAtFocus(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraSlotClearScript(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotClearScript(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraSlotCreate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CameraSlotCreate(g->ram, &g->camera, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_CameraSlotFlagBit81(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotFlagBit81(g->ram, &g->camera);
    return 0;
}

static int32_t call_ae_CameraSlotGetFocusPoint(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotGetFocusPoint(g->ram, &g->camera, (uint32_t)a0, (int32_t)a1, (uint32_t)a2);
    return 0;
}

static int32_t call_ae_CameraSlotGroundClamp(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotGroundClamp(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraSlotGroundClampNear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotGroundClampNear(g->ram, &g->camera, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_CameraSlotInitFollow(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotInitFollow(g->ram, &g->camera, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_CameraSlotPlaceBehindPlayer(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotPlaceBehindPlayer(g->ram, &g->camera, (uint32_t)a0, (uint32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_CameraSlotRemove(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotRemove(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraSlotResetFollow(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotResetFollow(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraSlotSmoothEye(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotSmoothEye(g->ram, &g->camera, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_CameraSlotSmoothTarget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotSmoothTarget(g->ram, &g->camera, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_CameraSlotSnapshotEye(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotSnapshotEye(g->ram, &g->camera);
    return 0;
}

static int32_t call_ae_CameraSlotStickOrbit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotStickOrbit(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraSlotUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotUpdate(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraSlotUpdateYaw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSlotUpdateYaw(g->ram, &g->camera, (uint32_t)a0);
    return 0;
}

/* ---- src/camera/ae_camupd.c ---- */
static int32_t call_ae_Atan2Safe(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Atan2Safe(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_CameraApplySlot(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraApplySlot(g->ram);
    return 0;
}

static int32_t call_ae_CameraGetEye(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraGetEye(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraGetPlayerDir(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraGetPlayerDir(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
    return 0;
}

static int32_t call_ae_CameraRegionTest(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CameraRegionTest(g->ram, (uint32_t)a0);
}

static int32_t call_ae_CameraRegionUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraRegionUpdate(g->ram, &g->camupd);
    return 0;
}

static int32_t call_ae_CameraScriptRun(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraScriptRun(g->ram, &g->camupd, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraSelectMode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSelectMode(g->ram, &g->camupd);
    return 0;
}

static int32_t call_ae_CameraShakeApply(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraShakeApply(g->ram);
    return 0;
}

static int32_t call_ae_CameraSnapshotPlayer(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraSnapshotPlayer(g->ram);
    return 0;
}

static int32_t call_ae_CameraUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraUpdate(g->ram, &g->camupd);
    return 0;
}

static int32_t call_ae_CopySvecAt1C(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CopySvecAt1C(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_CutsceneModeOff(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CutsceneModeOff(g->ram);
    return 0;
}

static int32_t call_ae_CutsceneModeOn(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CutsceneModeOn(g->ram);
    return 0;
}

static int32_t call_ae_EventCamCreate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_EventCamCreate(g->ram, &g->camupd, (int32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_EventCamLoadRecord(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EventCamLoadRecord(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_EventCamStart(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EventCamStart(g->ram, &g->camupd, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_EventCamTrigger(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EventCamTrigger(g->ram, &g->camupd, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_EventCamUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EventCamUpdate(g->ram, &g->camupd);
    return 0;
}

static int32_t call_ae_IsFlightArea(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_IsFlightArea(g->ram);
}

static int32_t call_ae_PadRumble(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PadRumble(g->ram, &g->camupd, (int32_t)a0, (uint32_t)a1, (uint32_t)a2);
    return 0;
}

static int32_t call_ae_ScreenFadeUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ScreenFadeUpdate(g->ram, &g->camupd);
    return 0;
}

static int32_t call_ae_StageExitRequest(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageExitRequest(g->ram, &g->camupd);
    return 0;
}

static int32_t call_ae_StageExitSequenceUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageExitSequenceUpdate(g->ram, &g->camupd);
    return 0;
}

/* ---- src/collision/ae_collprobe.c ---- */
static int32_t call_ae_CollProbe_Box(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_Box(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_DiagonalWall(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_DiagonalWall(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_EdgeWall(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_EdgeWall(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_HalfPipe(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_HalfPipe(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_NoContact(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_NoContact(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_Radial(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_Radial(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_Ramp(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_Ramp(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_Rope(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_Rope(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_Slope(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_Slope(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_Strip(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_Strip(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_ThinWall(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_ThinWall(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_Trench(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_Trench(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CollProbe_Triangle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbe_Triangle(g->ram, &g->coll_probe, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

/* ---- src/core/ae_pad.c ---- */
static int32_t call_ae_pad_ApplyMatrixSV(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_pad_ApplyMatrixSV(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_pad_CameraRotMatrix(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return special_ae_pad_CameraRotMatrix(g, a0, a1, a2, a3);
}

static int32_t call_ae_pad_PadNormalizeSticks(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_pad_PadNormalizeSticks(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_pad_PadSampleStickHistory(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_pad_PadSampleStickHistory(g->ram);
    return 0;
}

static int32_t call_ae_pad_PadUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_pad_PadUpdate(g->ram, &g->pad, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_pad_QuadrantFromSigns(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_pad_QuadrantFromSigns((int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_pad_VectorNormal(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_pad_VectorNormal(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_pad_VectorNormalSS(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_pad_VectorNormalSS(g->ram, (uint32_t)a0, (uint32_t)a1);
}

/* ---- src/core/ae_pose.c ---- */
static int32_t call_ae_ActorAnimDecode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ActorAnimDecode(g->ram, (uint32_t)a0);
}

static int32_t call_ae_AnimDecodeLerp(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AnimDecodeLerp(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_ModelBuildBoneMatrices(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ModelBuildBoneMatrices(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ModelPoseBones(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ModelPoseBones(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

/* ---- src/core/ae_sdk.c ---- */
static int32_t call_ae_sdk_ApplyMatrixSV(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_ApplyMatrixSV(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_sdk_RotMatrix(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_RotMatrix(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_RotMatrixYXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_RotMatrixYXZ(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_SquareRoot0(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_SquareRoot0(g->ram, (int32_t)a0);
}

static int32_t call_ae_sdk_VectorNormal(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_VectorNormal(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_VectorNormalS(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_VectorNormalS(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_VectorNormalSS(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_VectorNormalSS(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_normalize_core(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return special_ae_sdk_normalize_core(g, a0, a1, a2, a3);
}

static int32_t call_ae_sdk_ratan2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_ratan2(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_sdk_rcos(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_rcos(g->ram, (int32_t)a0);
}

static int32_t call_ae_sdk_rsin(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_rsin(g->ram, (int32_t)a0);
}

static int32_t call_ae_sdk_square0_lane(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return special_ae_sdk_square0_lane(g, a0, a1, a2, a3);
}

/* ---- src/core/ae_sdk_matrix.c ---- */
static int32_t call_ae_sdk_CompMatrix(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_CompMatrix(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_sdk_MatrixFromDirXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_MatrixFromDirXZ(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_MatrixIdentity(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_MatrixIdentity(g->ram, (uint32_t)a0);
}

static int32_t call_ae_sdk_MatrixLookDir(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_MatrixLookDir(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_RotMatrixX(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_RotMatrixX(g->ram, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_RotMatrixY(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_RotMatrixY(g->ram, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_RotMatrixZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_RotMatrixZ(g->ram, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_RotMatrixZYX(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_RotMatrixZYX(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_sdk_ScaleMatrix(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_sdk_ScaleMatrix(g->ram, (uint32_t)a0, (uint32_t)a1);
}

/* ---- src/effects/ae_effects_core.c ---- */
static int32_t call_ae_PlayerCanExitStage(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCanExitStage(g->ram);
}

static int32_t call_ae_SpriteEffectQueue(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpriteEffectQueue(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_SpriteEffectQueue2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpriteEffectQueue2(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_TrailPointAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TrailPointAdd(g->ram, (uint32_t)a0);
    return 0;
}

/* ---- src/effects/ae_effects_dispatch.c ---- */
static int32_t call_ae_EffectsUpdateAll(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectsUpdateAll(g->ram, &g->spawn, &g->effects);
    return 0;
}

static int32_t call_ae_PlayerEffectsUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerEffectsUpdate(g->ram, &g->spawn, &g->effects);
    return 0;
}

/* ---- src/effects/ae_effects_flying.c ---- */
static int32_t call_ae_FlyingMeshesUpdateDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FlyingMeshesUpdateDraw(g->ram, &g->spawn, &g->effects);
    return 0;
}

/* ---- src/effects/ae_effects_ground_sparks.c ---- */
static int32_t call_ae_GroundSparksUpdateDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GroundSparksUpdateDraw(g->ram, &g->spawn, &g->effects);
    return 0;
}

/* ---- src/effects/ae_effects_objects.c ---- */
static int32_t call_ae_EffectObjDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectObjDraw(g->ram, &g->effects, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_EffectObjsUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectObjsUpdate(g->ram, &g->effects);
    return 0;
}

/* ---- src/effects/ae_effects_player.c ---- */
static int32_t call_ae_PlayerFxTrailUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerFxTrailUpdate(g->ram, &g->spawn);
    return 0;
}

static int32_t call_ae_PlayerSplash(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSplash(g->ram, &g->spawn, &g->host.sound_hw);
    return 0;
}

static int32_t call_ae_PlayerTintEffectUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerTintEffectUpdate(g->ram, &g->spawn, &g->effects);
    return 0;
}

/* ---- src/effects/ae_effects_player_state.c ---- */
static int32_t call_ae_FxPlayerState6Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FxPlayerState6Update(g->ram, &g->effects);
    return 0;
}

/* ---- src/effects/ae_effects_random.c ---- */
static int32_t call_ae_RandomParticlesUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_RandomParticlesUpdate(g->ram, &g->effects);
    return 0;
}

/* ---- src/effects/ae_effects_sprite_draw.c ---- */
static int32_t call_ae_SpriteEffectsDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpriteEffectsDraw(g->ram, &g->effects);
    return 0;
}

/* ---- src/effects/ae_effects_trail_draw.c ---- */
static int32_t call_ae_TrailPointsDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TrailPointsDraw(g->ram, &g->effects);
    return 0;
}

/* ---- src/effects/ae_effects_trails.c ---- */
static int32_t call_ae_TrailsUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TrailsUpdate(g->ram, &g->effects);
    return 0;
}

/* ---- src/effects/ae_effects_water.c ---- */
static int32_t call_ae_PlayerWaterEffectsUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerWaterEffectsUpdate(g->ram, &g->spawn, &g->effects);
    return 0;
}

/* ---- src/effects/ae_fxscript.c ---- */
static int32_t call_ae_EffectScriptUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectScriptUpdate(g->ram, &g->fxscript);
    return 0;
}

static int32_t call_ae_ParticlesDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ParticlesDraw(g->ram, &g->fxscript);
    return 0;
}

static int32_t call_ae_ProjectileExpire(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileExpire(g->ram, &g->fxscript, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ProjectileGroupSpawn(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ProjectileGroupSpawn(g->ram, &g->fxscript, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_ProjectileGroupSpawnSpread(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileGroupSpawnSpread(g->ram, &g->fxscript, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
    return 0;
}

static int32_t call_ae_ProjectileGroupsUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileGroupsUpdate(g->ram, &g->fxscript);
    return 0;
}

static int32_t call_ae_ProjectileRecordsUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileRecordsUpdate(g->ram, &g->fxscript);
    return 0;
}

static int32_t call_ae_ProjectileSystemsUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileSystemsUpdate(g->ram, &g->fxscript);
    return 0;
}

static int32_t call_ae_RcCarUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_RcCarUpdate(g->ram, &g->fxscript);
    return 0;
}

static int32_t call_ae_RibbonDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_RibbonDraw(g->ram, &g->fxscript, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

/* ---- src/effects/ae_spawn.c ---- */
static int32_t call_ae_CameraShake(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraShake(g->ram, (int32_t)a0);
    return 0;
}

static int32_t call_ae_EffectBigBurst(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectBigBurst(g->ram, &g->spawn, (int32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_EffectObjAlloc(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_EffectObjAlloc(g->ram);
}

static int32_t call_ae_EffectObjSpawn(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectObjSpawn(g->ram, &g->spawn, (uint32_t)a0, (uint32_t)a1, (int32_t)a2, (int32_t)a3, (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u), (uint32_t)ae_rd32(g->ram, g->call_stack_pointer + 20u));
    return 0;
}

static int32_t call_ae_EffectSpawnFromDef(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectSpawnFromDef(g->ram, &g->spawn, (uint32_t)a0, (uint32_t)a1, (int32_t)a2, (int32_t)a3, (uint32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u));
    return 0;
}

static int32_t call_ae_ParticleAlloc(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ParticleAlloc(g->ram);
}

static int32_t call_ae_ProjectileAllocSlot(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ProjectileAllocSlot(g->ram);
}

static int32_t call_ae_ProjectileInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileInit(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_ProjectileSetByte2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileSetByte2(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ProjectileSetColor(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileSetColor(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ProjectileSetUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileSetUpdate(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_ProjectileSpawn(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ProjectileSpawn(g->ram, &g->spawn, (uint32_t)a0, (uint32_t)a1, (int32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_SparkAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SparkAdd(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_SparkAddRandom(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SparkAddRandom(g->ram, (uint32_t)a0, (uint32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_SpawnEffectGroup(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpawnEffectGroup(g->ram, &g->spawn, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_SpawnOneParticle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpawnOneParticle(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2);
    return 0;
}

static int32_t call_ae_SpawnParticle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpawnParticle(g->ram, &g->spawn, (int32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_SpawnPuff(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpawnPuff(g->ram, &g->spawn, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_TrailSpawn(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TrailSpawn(g->ram, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

/* ---- src/game/ae_modes.c ---- */
static int32_t call_ae_BumpAllocPeek(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BumpAllocPeek(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Mode_04(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Mode_04(g->ram);
}

static int32_t call_ae_Mode_06(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Mode_06(g->ram, &g->mode);
}

static int32_t call_ae_Mode_Stage(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Mode_Stage(g->ram, &g->mode);
}

static int32_t call_ae_NullInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_NullInit(g->ram);
    return 0;
}

static int32_t call_ae_ResetFrameFlip(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ResetFrameFlip(g->ram);
    return 0;
}

static int32_t call_ae_StageInitMemory(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageInitMemory(g->ram, &g->mode, (uint32_t)a0);
}

static int32_t call_ae_StageRenderInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageRenderInit(g->ram, &g->mode);
}

static int32_t call_ae_main(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_main(g->ram, &g->mode);
}

/* ---- src/modes/ae_ovl18b.c ---- */
static int32_t call_ae_Mode_SpecialAreaEnter(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Mode_SpecialAreaEnter(g->ram, &g->ovl18b);
}

static int32_t call_ae_SaveUi_StringTableIndex_18B(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SaveUi_StringTableIndex_18B(g->ram, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_SpecialEnterUi_DrawBlink(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpecialEnterUi_DrawBlink(g->ram, &g->ovl18b);
    return 0;
}

static int32_t call_ae_SpecialEnterUi_DrawMessageShadow(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpecialEnterUi_DrawMessageShadow(g->ram, &g->ovl18b, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_SpecialEnterUi_DrawScene(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpecialEnterUi_DrawScene(g->ram, &g->ovl18b);
    return 0;
}

static int32_t call_ae_SpecialEnterUi_DrawText(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpecialEnterUi_DrawText(g->ram, &g->ovl18b, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_SpecialEnterUi_DrawWipe(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SpecialEnterUi_DrawWipe(g->ram, &g->ovl18b, (uint32_t)a0);
}

static int32_t call_ae_SpecialEnterUi_PlayXa(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpecialEnterUi_PlayXa(g->ram, &g->ovl18b);
    return 0;
}

static int32_t call_ae_SpecialEnterUi_RunScript(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpecialEnterUi_RunScript(g->ram, &g->ovl18b);
    return 0;
}

static int32_t call_ae_SpecialEnterUi_SetAreaTablePointer(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpecialEnterUi_SetAreaTablePointer(g->ram);
    return 0;
}

static int32_t call_ae_SpecialEnterUi_TextWidth(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SpecialEnterUi_TextWidth(g->ram, &g->ovl18b, (uint32_t)a0);
}

static int32_t call_ae_SpecialEnterUi_UpdateObjects(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpecialEnterUi_UpdateObjects(g->ram, &g->ovl18b, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_StageClearUi_CopyStrip_18B(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageClearUi_CopyStrip_18B(g->ram, &g->ovl18b, (int32_t)a0);
    return 0;
}

static int32_t call_ae_StageClearUi_DrawScanlines_18B(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageClearUi_DrawScanlines_18B(g->ram, &g->ovl18b);
    return 0;
}

static int32_t call_ae_StageClearUi_LookAt_18B(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageClearUi_LookAt_18B(g->ram, &g->ovl18b, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_StageClearUi_SetDrawStp_18B(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageClearUi_SetDrawStp_18B(g->ram, &g->ovl18b);
    return 0;
}

/* ---- src/monkeys/ae_monkey_base.c ---- */
static int32_t call_ae_ActorContactOwnerType(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ActorContactOwnerType(g->ram);
}

static int32_t call_ae_AngleBetweenXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AngleBetweenXZ(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_AreaIsBossArena(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaIsBossArena(g->ram);
}

static int32_t call_ae_AreaIsSpecialMonkeyArea(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaIsSpecialMonkeyArea(g->ram);
}

static int32_t call_ae_Atan2Angle4096(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Atan2Angle4096(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_DelayedSfxUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_DelayedSfxUpdate(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_Distance3D(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Distance3D(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_DistanceIntToShort3D(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_DistanceIntToShort3D(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_DistanceXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_DistanceXZ(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_DotXZ_SV(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_DotXZ_SV(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_DotXZ_V(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_DotXZ_V(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_GimmickAllowsMonkeyPassage(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickAllowsMonkeyPassage(g->ram, &g->monkey, (uint32_t)a0);
}

static int32_t call_ae_MonkeyActivityReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyActivityReset(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyApproachAngleFar(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyApproachAngleFar(g->ram);
}

static int32_t call_ae_MonkeyApproachAngleNear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyApproachAngleNear(g->ram);
}

static int32_t call_ae_MonkeyAttackChance(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyAttackChance(g->ram, (int32_t)a0);
}

static int32_t call_ae_MonkeyBodyRegister(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyBodyRegister(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyBodyUnregister(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyBodyUnregister(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyCalcMoveDir(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyCalcMoveDir(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyCaughtGroupValue(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyCaughtGroupValue(g->ram);
}

static int32_t call_ae_MonkeyDebugMenuInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyDebugMenuInit(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyDebugMenuInit2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyDebugMenuInit2(g->ram, &g->monkey, (int32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyFacingFromLinkCode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyFacingFromLinkCode(g->ram, (int32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyGetAnimIndex(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyGetAnimIndex(g->ram);
}

static int32_t call_ae_MonkeyGetCallVoice(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyGetCallVoice(g->ram);
}

static int32_t call_ae_MonkeyGetCallVoice2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyGetCallVoice2(g->ram);
}

static int32_t call_ae_MonkeyInterpolateLinkVector(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyInterpolateLinkVector(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyIsAltAnimState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyIsAltAnimState(g->ram);
}

static int32_t call_ae_MonkeyIsAttackState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyIsAttackState(g->ram);
}

static int32_t call_ae_MonkeyIsIdleActivityState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyIsIdleActivityState(g->ram);
}

static int32_t call_ae_MonkeyIsIdleEndState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyIsIdleEndState(g->ram);
}

static int32_t call_ae_MonkeyIsJumpArcState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyIsJumpArcState(g->ram);
}

static int32_t call_ae_MonkeyIsState0C_or_24(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyIsState0C_or_24(g->ram);
}

static int32_t call_ae_MonkeyIsState1E_to_20(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyIsState1E_to_20(g->ram);
}

static int32_t call_ae_MonkeyNavBacktracked(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyNavBacktracked(g->ram);
}

static int32_t call_ae_MonkeyNop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyNop(g->ram);
}

static int32_t call_ae_MonkeyPuff14(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyPuff14(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyPuff5(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyPuff5(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyQueueVoice(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyQueueVoice(g->ram, &g->monkey, (int32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_MonkeyRand(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyRand(g->ram);
}

static int32_t call_ae_MonkeyReachedNode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyReachedNode(g->ram);
}

static int32_t call_ae_MonkeyResetNavVisits(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyResetNavVisits(g->ram);
    return 0;
}

static int32_t call_ae_MonkeySpawnProjectile(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeySpawnProjectile(g->ram, &g->monkey, (int32_t)a0);
}

static int32_t call_ae_MonkeyStoreActivities(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyStoreActivities(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyStub_ReturnZero(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyStub_ReturnZero(g->ram);
}

static int32_t call_ae_MonkeyVoiceQueueReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyVoiceQueueReset(g->ram);
    return 0;
}

static int32_t call_ae_NavGroundHeightBetween(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_NavGroundHeightBetween(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_NavNodeGetLink(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_NavNodeGetLink(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_NavNodeGetLinkIds(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_NavNodeGetLinkIds(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_NavNodeGetLinkIdsEx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_NavNodeGetLinkIdsEx(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3, (uint32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u));
    return 0;
}

static int32_t call_ae_NavNodeIsSpecial(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_NavNodeIsSpecial(g->ram, (int32_t)a0);
}

static int32_t call_ae_NavNodeLinkFlag(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_NavNodeLinkFlag(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ObjGetBonePos(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjGetBonePos(g->ram, &g->monkey, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_StaticModelDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StaticModelDraw(g->ram, &g->monkey, (uint32_t)a0);
}

/* ---- src/monkeys/ae_monkey_level1.c ---- */
static int32_t call_ae_MonkeyAdvanceAnimFrame(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAdvanceAnimFrame(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyAimAtTarget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAimAtTarget(g->ram, &g->monkey, (int32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyAlertIconKind(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyAlertIconKind(g->ram);
}

static int32_t call_ae_MonkeyAlertPuff(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAlertPuff(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyAngleToTarget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyAngleToTarget(g->ram);
}

static int32_t call_ae_MonkeyAnimAtStart(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyAnimAtStart(g->ram);
}

static int32_t call_ae_MonkeyAnimFinished(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyAnimFinished(g->ram);
}

static int32_t call_ae_MonkeyApplyMoveDir(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyApplyMoveDir(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyBodyOff(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyBodyOff(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyBodyOn(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyBodyOn(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyDebrisBurst1(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyDebrisBurst1(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyDebrisBurst6(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyDebrisBurst6(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyDefeatBurst(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyDefeatBurst(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyFireProjectile6(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyFireProjectile6(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyFireProjectile7(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyFireProjectile7(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyFireProjectile8(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyFireProjectile8(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyFireProjectileAimed(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyFireProjectileAimed(g->ram, &g->monkey, (int32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyFootDust(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyFootDust(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyLineOfSight(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyLineOfSight(g->ram, &g->monkey, (int32_t)a0);
}

static int32_t call_ae_MonkeyLoadLinkExtra(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyLoadLinkExtra(g->ram, (int32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_MonkeyMoveStep(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyMoveStep(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyPickAnim(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyPickAnim(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyPickGoalNode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyPickGoalNode(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyProjectToScreen(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyProjectToScreen(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyRandomHesitate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyRandomHesitate(g->ram);
}

static int32_t call_ae_MonkeySelectLod(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeySelectLod(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyTargetInSightRange(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyTargetInSightRange(g->ram);
}

static int32_t call_ae_MonkeyTurnTowardTarget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyTurnTowardTarget(g->ram, (int32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyUpdateContact(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyUpdateContact(g->ram);
}

static int32_t call_ae_NavLinkLookupExtra(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_NavLinkLookupExtra(g->ram, (int32_t)a0, (int32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_NavNodeGetPos(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_NavNodeGetPos(g->ram, &g->monkey, (int32_t)a0, (uint32_t)a1);
    return 0;
}

/* ---- src/monkeys/ae_monkey_level10.c ---- */
static int32_t call_ae_MonkeyAiStateUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAiStateUpdate(g->ram, &g->monkey, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeySpawnAll(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeySpawnAll(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyThinkActive(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyThinkActive(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyUpdateAll(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyUpdateAll(g->ram, &g->monkey);
    return 0;
}

/* ---- src/monkeys/ae_monkey_level2.c ---- */
static int32_t call_ae_MonkeyDrawAlertIcon(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyDrawAlertIcon(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_17(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_17(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_2A(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_2A(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_32(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_32(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_3D(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_3D(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_3E(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_3E(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_3F(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_3F(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyFireProjectileByKind(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyFireProjectileByKind(g->ram, &g->monkey, (int32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyGetWaypointPos(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyGetWaypointPos(g->ram, &g->monkey, (int32_t)a0);
}

static int32_t call_ae_MonkeyNavLinkAllowed(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyNavLinkAllowed(g->ram, &g->monkey, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_MonkeyNodeTargetCosines(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyNodeTargetCosines(g->ram, &g->monkey, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_MonkeyPrepareJumpArc(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyPrepareJumpArc(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyScoreLinksByTarget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyScoreLinksByTarget(g->ram, &g->monkey, (int32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeySetState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeySetState(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyTryAttackKind(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyTryAttackKind(g->ram, &g->monkey, (int32_t)a0);
}

static int32_t call_ae_MonkeyUpdateFacing(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyUpdateFacing(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyUpdateGoalPos(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyUpdateGoalPos(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyUpdateLinkSlope(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyUpdateLinkSlope(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyUpdateSight(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyUpdateSight(g->ram, &g->monkey, (uint32_t)a0);
}

/* ---- src/monkeys/ae_monkey_level3.c ---- */
static int32_t call_ae_MonkeyAi_State3E(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_State3E(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyDrawModel(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyDrawModel(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyEnterIdleWait(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterIdleWait(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyEnterKnockdown(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterKnockdown(g->ram, &g->monkey, (int32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyEnterRandomLook(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterRandomLook(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyEnterState2F(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState2F(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_09(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_09(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_0C(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_0C(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_1A(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_1A(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_1E(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyEnterState_1E(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyEnterState_20(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_20(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_24(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_24(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_30(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_30(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_34(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_34(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_35(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_35(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyEnterState_36(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyEnterState_36(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyMarkBlockedLinks(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyMarkBlockedLinks(g->ram, &g->monkey, (int32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyNavVisitUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyNavVisitUpdate(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyPickAttackA(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyPickAttackA(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyPickAttackB(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyPickAttackB(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyPickAttackC(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyPickAttackC(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyRestoreActivities(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyRestoreActivities(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyResumeIdle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyResumeIdle(g->ram);
    return 0;
}

static int32_t call_ae_MonkeyStartNotice(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyStartNotice(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyUpdateGoal(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyUpdateGoal(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyUpdateTargetDistance(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyUpdateTargetDistance(g->ram, &g->monkey);
    return 0;
}

/* ---- src/monkeys/ae_monkey_level4.c ---- */
static int32_t call_ae_MonkeyAi_Fall(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_Fall(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyArriveAtNode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyArriveAtNode(g->ram, &g->monkey, (int32_t)a0);
}

static int32_t call_ae_MonkeyEnterState_04(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyEnterState_04(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyEnterState_1D(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyEnterState_1D(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyEnterState_2C(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyEnterState_2C(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyPickBestLink(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyPickBestLink(g->ram, &g->monkey, (int32_t)a0);
}

static int32_t call_ae_MonkeySetNavNode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeySetNavNode(g->ram, &g->monkey, (int32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeySwapNavNodes(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeySwapNavNodes(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyUpdateVisuals(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyUpdateVisuals(g->ram, &g->monkey);
    return 0;
}

/* ---- src/monkeys/ae_monkey_level5.c ---- */
static int32_t call_ae_MonkeyConsiderTurnBack(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyConsiderTurnBack(g->ram, &g->monkey);
}

static int32_t call_ae_MonkeyConsiderTurnBackRandom(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyConsiderTurnBackRandom(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyNodeActionDispatch(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyNodeActionDispatch(g->ram, &g->monkey);
}

/* ---- src/monkeys/ae_monkey_level6.c ---- */
static int32_t call_ae_MonkeyAlertedDecide(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAlertedDecide(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyChooseNextNodeA(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyChooseNextNodeA(g->ram, &g->monkey, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_MonkeyChooseNextNodeB(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyChooseNextNodeB(g->ram, &g->monkey, (int32_t)a0, (int32_t)a1);
}

/* ---- src/monkeys/ae_monkey_level7.c ---- */
static int32_t call_ae_MonkeyAi_GoToFlaggedNode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_GoToFlaggedNode(g->ram, &g->monkey, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_MonkeyAi_IdleActivityLoop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_IdleActivityLoop(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyAi_IdleActivityStart(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_IdleActivityStart(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyPatrolDecide(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyPatrolDecide(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeySelectNextNode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeySelectNextNode(g->ram, &g->monkey, (int32_t)a0, (int32_t)a1);
}

/* ---- src/monkeys/ae_monkey_level8.c ---- */
static int32_t call_ae_MonkeyAi_IdleActivityEnd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_IdleActivityEnd(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyAi_MoveAlerted(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_MoveAlerted(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyAi_MoveUnalerted(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_MoveUnalerted(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyAi_State07(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_State07(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyAi_State1E_20(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_State1E_20(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyDecideAction(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyDecideAction(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyInit(g->ram, &g->monkey, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
    return 0;
}

/* ---- src/monkeys/ae_monkey_level9.c ---- */
static int32_t call_ae_MonkeyAi_JumpArc(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_JumpArc(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyAi_JumpArcB(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_JumpArcB(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyAi_JumpArcPhased(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_JumpArcPhased(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeyAi_State3D(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_State3D(g->ram, &g->monkey);
    return 0;
}

static int32_t call_ae_MonkeySelectActive(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeySelectActive(g->ram, &g->monkey);
    return 0;
}

/* ---- src/monkeys/ae_monkey_level9_patrol.c ---- */
static int32_t call_ae_MonkeyAi_Patrol(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyAi_Patrol(g->ram, &g->monkey);
    return 0;
}

/* ---- src/player/ae_anim_events.c ---- */
static int32_t call_ae_PlayerAnimEventAddHitBodies(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAnimEventAddHitBodies(g->ram, &g->body, (uint32_t)a0, (int32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_PlayerRunAnimEvents(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerRunAnimEvents(g->ram, &g->body, &g->anim, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

/* ---- src/player/ae_animdraw.c ---- */
static int32_t call_ae_AnimDecodeFrame(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AnimDecodeFrame(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_AnimDecodeLerpPtr(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AnimDecodeLerpPtr(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_AnimFramePtr(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AnimFramePtr(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_AreaGetMonkeyRecord(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaGetMonkeyRecord(g->ram, &g->area_set, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_IntSqrt(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_IntSqrt(g->ram, (int32_t)a0);
}

static int32_t call_ae_MatrixFromFrontUp(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MatrixFromFrontUp(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_MonkeyFlag248Set(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyFlag248Set(g->ram, (uint32_t)a0);
}

static int32_t call_ae_MonkeyRadarSignal(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MonkeyRadarSignal(g->ram, &g->anim_draw);
}

static int32_t call_ae_PlayerAnimateAndDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAnimateAndDraw(g->ram, &g->anim_draw);
    return 0;
}

static int32_t call_ae_PlayerCanActivateGadget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCanActivateGadget(g->ram, (uint32_t)a0);
}

static int32_t call_ae_PlayerHeadLookUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerHeadLookUpdate(g->ram, &g->anim_draw);
    return 0;
}

static int32_t call_ae_PlayerRumbleSlam(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerRumbleSlam(g->ram, &g->anim_draw);
    return 0;
}

static int32_t call_ae_PlayerSwimSfxUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSwimSfxUpdate(g->ram, &g->anim_draw);
    return 0;
}

static int32_t call_ae_RadarUpdateDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_RadarUpdateDraw(g->ram, &g->anim_draw);
    return 0;
}

static int32_t call_ae_RcCarDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_RcCarDraw(g->ram, &g->anim_draw);
    return 0;
}

static int32_t call_ae_StepToward(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StepToward(g->ram, (int32_t)a0, (int32_t)a1, (int32_t)a2);
}

/* ---- src/player/ae_gadget.c ---- */
static int32_t call_ae_Gadget3AimUpdateAmmo(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget3AimUpdateAmmo(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_Gadget3FindAimTarget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget3FindAimTarget(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_Gadget3LockOnAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget3LockOnAdd(g->ram, &g->gadget, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Gadget3UpdateDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget3UpdateDraw(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_Gadget4BuildMatrix(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget4BuildMatrix(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_Gadget4PlayerUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget4PlayerUpdate(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_Gadget4TrailReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget4TrailReset(g->ram);
    return 0;
}

static int32_t call_ae_Gadget4TrailUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget4TrailUpdate(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_Gadget4UpdateDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget4UpdateDraw(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_Gadget6SparkleFx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget6SparkleFx(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_Gadget6UpdateSound(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget6UpdateSound(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_Gadget7TrailUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Gadget7TrailUpdate(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_GadgetDrawSlot0Attachment(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetDrawSlot0Attachment(g->ram, &g->gadget, (uint32_t)a0, (uint32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_GadgetDrawStaticModel(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetDrawStaticModel(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_GadgetDrawWithStateCheck(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetDrawWithStateCheck(g->ram, &g->gadget, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_GadgetFlashSet(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetFlashSet(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_GadgetFlashUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetFlashUpdate(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_GadgetHasAltStance(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GadgetHasAltStance(g->ram);
}

static int32_t call_ae_GadgetSelectFx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetSelectFx(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_GadgetSelectFxDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetSelectFxDraw(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_GadgetSetSelected(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetSetSelected(g->ram, &g->gadget, (int32_t)a0);
    return 0;
}

static int32_t call_ae_GadgetSlotRemap(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetSlotRemap(g->ram, (int32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_GadgetSwingParamsInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetSwingParamsInit(g->ram);
    return 0;
}

static int32_t call_ae_GadgetSwingUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetSwingUpdate(g->ram, &g->gadget);
    return 0;
}

static int32_t call_ae_GadgetUnlockMask(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetUnlockMask(g->ram, (uint32_t)a0);
    return 0;
}

/* ---- src/player/ae_player1.c ---- */
static int32_t call_ae_ActorAnimDecodeEntry(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ActorAnimDecodeEntry(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_AimModeEnter(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AimModeEnter(g->ram, &g->player1);
    return 0;
}

static int32_t call_ae_AimRequestClear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AimRequestClear(g->ram);
    return 0;
}

static int32_t call_ae_ArcGetMemberPos(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ArcGetMemberPos(g->ram, &g->player1, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_CameraFixedModeOff(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraFixedModeOff(g->ram);
    return 0;
}

static int32_t call_ae_CdMgrIssueCommand(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CdMgrIssueCommand(g->ram, &g->player1, (uint32_t)a0);
}

static int32_t call_ae_CdMgrQueueCommand(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CdMgrQueueCommand(g->ram, &g->player1, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_EventCamStartRandom(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EventCamStartRandom(g->ram, &g->player1);
    return 0;
}

static int32_t call_ae_HudShowHealth(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudShowHealth(g->ram, (int32_t)a0);
    return 0;
}

static int32_t call_ae_IsArea53(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_IsArea53(g->ram);
}

static int32_t call_ae_ModelDrawWithOt(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ModelDrawWithOt(g->ram, &g->player1, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_ModelPrepCycleColors(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ModelPrepCycleColors(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_PlayerDamage(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerDamage(g->ram, (int32_t)a0);
    return 0;
}

static int32_t call_ae_PlayerIdleModelSetup(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerIdleModelSetup(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_TrailRingReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TrailRingReset(g->ram);
    return 0;
}

static int32_t call_ae_XaPlay(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_XaPlay(g->ram, &g->player1, (int32_t)a0);
}

/* ---- src/player/ae_player2_leaf.c ---- */
static int32_t call_ae_AimTableClear3(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AimTableClear3(g->ram);
    return 0;
}

static int32_t call_ae_EffectGroundImpact(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectGroundImpact(g->ram, &g->world, &g->spawn, &g->player2, (int32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_EffectGroundImpactAtPlayer(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectGroundImpactAtPlayer(g->ram, &g->world, &g->spawn, &g->player2);
    return 0;
}

static int32_t call_ae_FUN_8013f0c0(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FUN_8013f0c0(g->ram, &g->player2);
    return 0;
}

static int32_t call_ae_IsPauseMenuOpen(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_IsPauseMenuOpen(g->ram);
}

static int32_t call_ae_Max16(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Max16(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_MonkeyNetCatchDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyNetCatchDraw(g->ram, &g->player2);
    return 0;
}

static int32_t call_ae_MonkeyNetCatchUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyNetCatchUpdate(g->ram, &g->player2, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayPlayerVoice(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayPlayerVoice(g->ram, &g->player2, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_PlayerAccelXYZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerAccelXYZ(g->ram, (int32_t)a0);
}

static int32_t call_ae_PlayerAccelXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerAccelXZ(g->ram, (int32_t)a0);
}

static int32_t call_ae_PlayerApplyGadgetStance(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerApplyGadgetStance(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerCheckWaterEntry(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCheckWaterEntry(g->ram, &g->player2, &g->spawn);
}

static int32_t call_ae_PlayerDrawSquashed(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerDrawSquashed(g->ram, &g->player2);
    return 0;
}

static int32_t call_ae_PlayerDrawSquashedFlattened(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerDrawSquashedFlattened(g->ram, &g->player2, &g->spawn);
    return 0;
}

static int32_t call_ae_PlayerDrawSquashedWithFx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerDrawSquashedWithFx(g->ram, &g->player2, &g->spawn);
    return 0;
}

static int32_t call_ae_PlayerEnterSwimIfInWater(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerEnterSwimIfInWater(g->ram);
    return 0;
}

static int32_t call_ae_PlayerFxClearDAT_800F5144(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerFxClearDAT_800F5144(g->ram);
    return 0;
}

static int32_t call_ae_PlayerFxFlattenedParticles(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerFxFlattenedParticles(g->ram, &g->spawn, &g->player2);
    return 0;
}

static int32_t call_ae_PlayerFxResetBlink(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerFxResetBlink(g->ram);
    return 0;
}

static int32_t call_ae_PlayerFxSpawn10C(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerFxSpawn10C(g->ram, &g->spawn);
    return 0;
}

static int32_t call_ae_PlayerFxSquashParticles(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerFxSquashParticles(g->ram, &g->spawn, &g->player2);
    return 0;
}

static int32_t call_ae_PlayerIsOnGround(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerIsOnGround(g->ram);
}

static int32_t call_ae_PlayerOnContact(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerOnContact(g->ram, &g->player2, &g->player_act, &g->world, &g->spawn, &g->host.sound_hw);
}

static int32_t call_ae_PlayerOnWallBump(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerOnWallBump(g->ram, &g->player2, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_PlayerResetAir(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerResetAir(g->ram);
    return 0;
}

static int32_t call_ae_PlayerRunTransitions(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerRunTransitions(g->ram, &g->player2, &g->player_act, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_PlayerSelectGadget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerSelectGadget(g->ram, &g->player2, &g->player_act, &g->spawn, &g->body, &g->host.sound_hw, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_PlayerSparkleBurst(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSparkleBurst(g->ram, &g->player2);
    return 0;
}

static int32_t call_ae_PlayerSpawnDust(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSpawnDust(g->ram, &g->player2);
    return 0;
}

static int32_t call_ae_PlayerSpawnDustBehind(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSpawnDustBehind(g->ram, &g->player2);
    return 0;
}

static int32_t call_ae_PlayerSplashStart(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSplashStart(g->ram, &g->spawn, &g->player2);
    return 0;
}

static int32_t call_ae_PlayerStartHurtBlink(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerStartHurtBlink(g->ram);
    return 0;
}

static int32_t call_ae_PlayerUpdateClimb(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerUpdateClimb(g->ram, &g->player2, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerUpdateHeldByObject(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerUpdateHeldByObject(g->ram, &g->player2, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerUpdateIdleAnim(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerUpdateIdleAnim(g->ram, &g->player2, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerUpdateRope(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerUpdateRope(g->ram, &g->player2, &g->player_act);
    return 0;
}

static int32_t call_ae_RadarResetState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_RadarResetState(g->ram);
    return 0;
}

static int32_t call_ae_RcCarSpawn(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_RcCarSpawn(g->ram, &g->player2, &g->body, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
    return 0;
}

static int32_t call_ae_SoundSetVolumeMode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundSetVolumeMode(g->ram, (int32_t)a0);
}

static int32_t call_ae_SpawnRandomParticle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpawnRandomParticle(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_SpuSetMainVolume(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpuSetMainVolume(g->ram, (int32_t)a0, (int32_t)a1);
    return 0;
}

/* ---- src/player/ae_player_act.c ---- */
static int32_t call_ae_PlayerAct_AimModeEnter(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_AimModeEnter(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_AimModeLeave(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_AimModeLeave(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_AnimSpeedHalf(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_AnimSpeedHalf(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_AnimSpeedNormal(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_AnimSpeedNormal(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_BounceBack(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_BounceBack(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_DriftForward(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_DriftForward(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_ExitToMode0Forward(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_ExitToMode0Forward(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_FaceLeftPerp(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_FaceLeftPerp(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_FaceRightPerp(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_FaceRightPerp(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_FaceStick(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_FaceStick(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_FaceStickMaybeHalfSpeed(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_FaceStickMaybeHalfSpeed(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_FaceStickOnRotation(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_FaceStickOnRotation(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_FaceStickRotateA(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_FaceStickRotateA(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_FaceStickRotateB(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_FaceStickRotateB(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_FlyerLaunchA(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_FlyerLaunchA(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_FlyerLaunchB(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_FlyerLaunchB(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_FlyerThrust(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_FlyerThrust(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_Gadget3Aim(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_Gadget3Aim(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_GadgetSpinFromStick(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_GadgetSpinFromStick(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_HurtVoiceFlag(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_HurtVoiceFlag(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_JumpFromLStick(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_JumpFromLStick(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_JumpHoldBoost(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_JumpHoldBoost(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_JumpPct2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_JumpPct2(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_JumpPct3(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_JumpPct3(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_NegateInputXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_NegateInputXZ(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_Nop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_Nop(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_Rebound(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_Rebound(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_ScaleMoveVel(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return special_ae_PlayerAct_ScaleMoveVel(g, a0, a1, a2, a3);
}

static int32_t call_ae_PlayerAct_SquashReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_SquashReset(g->ram, &g->player_act);
    return 0;
}

static int32_t call_ae_PlayerAct_StepBackDrop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_StepBackDrop(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_TurnAround(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_TurnAround(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_TurnAroundAndJump(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return special_ae_PlayerAct_TurnAroundAndJump(g, a0, a1, a2, a3);
}

static int32_t call_ae_PlayerAct_TurnFacingLeft90(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_TurnFacingLeft90(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_TurnFacingRight90(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_TurnFacingRight90(g->ram);
    return 0;
}

static int32_t call_ae_PlayerAct_VerticalOnly(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_VerticalOnly(g->ram);
    return 0;
}

/* ---- src/player/ae_player_act_land.c ---- */
static int32_t call_ae_PlayerAct_Land(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerAct_Land(g->ram, &g->player_act);
    return 0;
}

/* ---- src/player/ae_player_act_mode.c ---- */
static int32_t call_ae_PlayerSetMode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSetMode(g->ram, &g->player_act, (int32_t)a0);
    return 0;
}

/* ---- src/player/ae_player_act_state.c ---- */
static int32_t call_ae_PlayerSetClimbTarget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSetClimbTarget(g->ram, (int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3, (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u), (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 20u));
    return 0;
}

static int32_t call_ae_PlayerSetEyeFrame(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSetEyeFrame(g->ram, &g->player_act, (int32_t)a0);
    return 0;
}

static int32_t call_ae_PlayerSetRope(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSetRope(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_PlayerSetState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSetState(g->ram, &g->player_act, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_PlayerSetTintEffect(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerSetTintEffect(g->ram, (int32_t)a0);
    return 0;
}

/* ---- src/player/ae_player_cond.c ---- */
static int32_t call_ae_PlayerCond_AnimDone(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_AnimDone(g->ram);
}

static int32_t call_ae_PlayerCond_AnimDoneRandom(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_AnimDoneRandom(g->ram);
}

static int32_t call_ae_PlayerCond_AttachTimerDone(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_AttachTimerDone(g->ram);
}

static int32_t call_ae_PlayerCond_CanJump(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_CanJump(g->ram);
}

static int32_t call_ae_PlayerCond_ContactKind8(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_ContactKind8(g->ram);
}

static int32_t call_ae_PlayerCond_ContactWall3B(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_ContactWall3B(g->ram);
}

static int32_t call_ae_PlayerCond_Dead(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_Dead(g->ram);
}

static int32_t call_ae_PlayerCond_FacingContactWall(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_FacingContactWall(g->ram);
}

static int32_t call_ae_PlayerCond_FacingDesiredDir(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_FacingDesiredDir(g->ram);
}

static int32_t call_ae_PlayerCond_FallingHigh(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_FallingHigh(g->ram);
}

static int32_t call_ae_PlayerCond_False(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_False(g->ram);
}

static int32_t call_ae_PlayerCond_False2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_False2(g->ram);
}

static int32_t call_ae_PlayerCond_FirstFrameRStickEvent10(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_FirstFrameRStickEvent10(g->ram);
}

static int32_t call_ae_PlayerCond_FirstFrameRStickEvent20(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_FirstFrameRStickEvent20(g->ram);
}

static int32_t call_ae_PlayerCond_Gadget1RStickFwdFlick(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_Gadget1RStickFwdFlick(g->ram);
}

static int32_t call_ae_PlayerCond_Gadget3RStickBack(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_Gadget3RStickBack(g->ram);
}

static int32_t call_ae_PlayerCond_Gadget4Mode1(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_Gadget4Mode1(g->ram);
}

static int32_t call_ae_PlayerCond_GadgetBusyOrGesture(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_GadgetBusyOrGesture(g->ram);
}

static int32_t call_ae_PlayerCond_GestureEvent04(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_GestureEvent04(g->ram);
}

static int32_t call_ae_PlayerCond_GestureEvent08(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_GestureEvent08(g->ram);
}

static int32_t call_ae_PlayerCond_GestureEvent0C(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_GestureEvent0C(g->ram);
}

static int32_t call_ae_PlayerCond_GroundBehindLow(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_GroundBehindLow(g->ram, &g->player_cond);
}

static int32_t call_ae_PlayerCond_InWater(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_InWater(g->ram);
}

static int32_t call_ae_PlayerCond_JumpHeld(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_JumpHeld(g->ram);
}

static int32_t call_ae_PlayerCond_JumpOutOfWater(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_JumpOutOfWater(g->ram, &g->player_cond);
}

static int32_t call_ae_PlayerCond_JumpPressed(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_JumpPressed(g->ram);
}

static int32_t call_ae_PlayerCond_JumpPressedAtApex(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_JumpPressedAtApex(g->ram, &g->player_cond);
}

static int32_t call_ae_PlayerCond_LStickBack(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickBack(g->ram);
}

static int32_t call_ae_PlayerCond_LStickClass2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickClass2(g->ram);
}

static int32_t call_ae_PlayerCond_LStickClass4(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickClass4(g->ram);
}

static int32_t call_ae_PlayerCond_LStickClass8(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickClass8(g->ram);
}

static int32_t call_ae_PlayerCond_LStickFull(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickFull(g->ram);
}

static int32_t call_ae_PlayerCond_LStickFwd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickFwd(g->ram);
}

static int32_t call_ae_PlayerCond_LStickIdle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickIdle(g->ram);
}

static int32_t call_ae_PlayerCond_LStickIdleL3Held(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickIdleL3Held(g->ram);
}

static int32_t call_ae_PlayerCond_LStickIdleL3Pressed(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickIdleL3Pressed(g->ram);
}

static int32_t call_ae_PlayerCond_LStickIdleStillMoving(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickIdleStillMoving(g->ram);
}

static int32_t call_ae_PlayerCond_LStickPartial(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickPartial(g->ram);
}

static int32_t call_ae_PlayerCond_LStickSideAny(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickSideAny(g->ram);
}

static int32_t call_ae_PlayerCond_LStickSideNeg(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickSideNeg(g->ram);
}

static int32_t call_ae_PlayerCond_LStickSidePos(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LStickSidePos(g->ram);
}

static int32_t call_ae_PlayerCond_LandingSoon(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_LandingSoon(g->ram, &g->player_cond);
}

static int32_t call_ae_PlayerCond_NoGestureR3Held(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_NoGestureR3Held(g->ram);
}

static int32_t call_ae_PlayerCond_NoMoveInput(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_NoMoveInput(g->ram);
}

static int32_t call_ae_PlayerCond_NotContactKind8(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_NotContactKind8(g->ram);
}

static int32_t call_ae_PlayerCond_RStickBack(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickBack(g->ram);
}

static int32_t call_ae_PlayerCond_RStickBackHard(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickBackHard(g->ram);
}

static int32_t call_ae_PlayerCond_RStickEvent02(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickEvent02(g->ram);
}

static int32_t call_ae_PlayerCond_RStickEvent04(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickEvent04(g->ram);
}

static int32_t call_ae_PlayerCond_RStickEvent08(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickEvent08(g->ram);
}

static int32_t call_ae_PlayerCond_RStickEvent10(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickEvent10(g->ram);
}

static int32_t call_ae_PlayerCond_RStickEvent20(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickEvent20(g->ram);
}

static int32_t call_ae_PlayerCond_RStickEvent40(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickEvent40(g->ram);
}

static int32_t call_ae_PlayerCond_RStickFull(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickFull(g->ram);
}

static int32_t call_ae_PlayerCond_RStickFwd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickFwd(g->ram);
}

static int32_t call_ae_PlayerCond_RStickGesture10(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickGesture10(g->ram);
}

static int32_t call_ae_PlayerCond_RStickGesture20(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickGesture20(g->ram);
}

static int32_t call_ae_PlayerCond_RStickGestureOrTravel(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickGestureOrTravel(g->ram);
}

static int32_t call_ae_PlayerCond_RStickNoGesture(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickNoGesture(g->ram);
}

static int32_t call_ae_PlayerCond_RStickPartial(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickPartial(g->ram);
}

static int32_t call_ae_PlayerCond_RStickRotating(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickRotating(g->ram);
}

static int32_t call_ae_PlayerCond_RStickSideNeg(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickSideNeg(g->ram);
}

static int32_t call_ae_PlayerCond_RStickSidePos(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_RStickSidePos(g->ram);
}

static int32_t call_ae_PlayerCond_Still(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_Still(g->ram);
}

static int32_t call_ae_PlayerCond_WadingOnGround(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerCond_WadingOnGround(g->ram, &g->player_cond);
}

/* ---- src/player/ae_player_spawn.c ---- */
static int32_t call_ae_AimModeLeave(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AimModeLeave(g->ram, &g->player_spawn);
    return 0;
}

static int32_t call_ae_AreaGetEntryPoint(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaGetEntryPoint(g->ram, (int32_t)a0, (int32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_CameraActivateSlot(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraActivateSlot(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_CameraResetAll(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraResetAll(g->ram, &g->player_spawn);
    return 0;
}

static int32_t call_ae_CameraShakeReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraShakeReset(g->ram);
    return 0;
}

static int32_t call_ae_EventCamSetScript(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EventCamSetScript(g->ram, (uint32_t)a0, (int32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_PlaySfx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlaySfx(g->ram, &g->host.sound_hw, (int32_t)a0);
}

static int32_t call_ae_PlayerSpawnAtEntry(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerSpawnAtEntry(g->ram, &g->player_spawn);
}

static int32_t call_ae_SetProjection(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SetProjection(g->ram, &g->player_spawn, (int32_t)a0);
    return 0;
}

static int32_t call_ae_StageExitFlagsClear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageExitFlagsClear(g->ram);
    return 0;
}

/* ---- src/player/ae_playerupdate.c ---- */
static int32_t call_ae_TurnTowardsY(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TurnTowardsY(g->ram, &g->player_update, (uint32_t)a0, (uint32_t)a1, (int32_t)a2);
    return 0;
}

/* ---- src/player/ae_playerupdate_frame.c ---- */
static int32_t call_ae_PlayerUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerUpdate(g->ram, &g->player_update_frame);
    return 0;
}

/* ---- src/player/native_gadget_slots.c ---- */
static int32_t call_ae_GadgetSlotsInitForArea(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GadgetSlotsInitForArea(g->ram, (int32_t)a0);
}

static int32_t call_ae_GadgetSlotsInitForExit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return special_ae_GadgetSlotsInitForExit(g, a0, a1, a2, a3);
}

static int32_t call_ae_GadgetSlotsRestore(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetSlotsRestore(g->ram);
    return 0;
}

static int32_t call_ae_GadgetSlotsSave(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_GadgetSlotsSave(g->ram);
    return 0;
}

/* ---- src/player/native_player.c ---- */
static int32_t call_ae_player_can_switch_gadget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return special_ae_player_can_switch_gadget(g, a0, a1, a2, a3);
}

static int32_t call_ae_player_gadget_select_request(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return special_ae_player_gadget_select_request(g, a0, a1, a2, a3);
}

static int32_t call_ae_player_is_swimming(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return special_ae_player_is_swimming(g, a0, a1, a2, a3);
}

static int32_t call_ae_player_normalize(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return special_ae_player_normalize(g, a0, a1, a2, a3);
}

static int32_t call_ae_player_vec_length3(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_player_vec_length3((int32_t)a0, (int32_t)a1, (int32_t)a2);
}

/* ---- src/render/ae_tex_copy.c ---- */
static int32_t call_ae_AreaTexCopyInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaTexCopyInit(g->ram, &g->tex_copy, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_HeapFree(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HeapFree(g->ram, (uint32_t)a0);
}

/* ---- src/render/ae_vram_anim.c ---- */
static int32_t call_ae_AreaSetVramAnimList(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaSetVramAnimList(g->ram, &g->vram_anim, (uint32_t)a0);
}

/* ---- src/sound/ae_bgm.c ---- */
static int32_t call_ae_BgmModeWantsMusic(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BgmModeWantsMusic(g->ram);
}

static int32_t call_ae_BgmSeqOpen(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BgmSeqOpen(g->ram, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_BgmSeqSetVolume(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BgmSeqSetVolume(g->ram, &g->host.sound_hw, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_BgmStart(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BgmStart(g->ram, &g->host.sound_hw);
}

static int32_t call_ae_BgmUpdateVolume(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BgmUpdateVolume(g->ram, &g->host.sound_hw);
}

static int32_t call_ae_ProgressGetByte(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ProgressGetByte(g->ram, (uint32_t)a0);
}

static int32_t call_ae_SeqFadeInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SeqFadeInit(g->ram, (int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_SeqFadeTrackIn(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SeqFadeTrackIn(g->ram, &g->host.sound_hw, (int32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_SeqTrackRearm(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SeqTrackRearm(g->ram, (int32_t)a0);
    return 0;
}

static int32_t call_ae_SeqUpdateVoiceVolume(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SeqUpdateVoiceVolume(g->ram, &g->host.sound_hw, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_SeqVoiceVolumeSide(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SeqVoiceVolumeSide(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_SoundAreaKeepsMusic(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundAreaKeepsMusic(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_SoundDelayedQueueReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundDelayedQueueReset(g->ram);
}

static int32_t call_ae_SoundLoopStart(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundLoopStart(g->ram, &g->host.sound_hw, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_SoundLoopsStopAll(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundLoopsStopAll(g->ram, &g->host.sound_hw);
}

/* ---- src/sound/ae_sound.c ---- */
static int32_t call_ae_PlaySfxAt(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlaySfxAt(g->ram, &g->host.sound_hw, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_SeqSetTickFlag(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SeqSetTickFlag(g->ram, (int32_t)a0);
}

static int32_t call_ae_SfxIsQueued(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SfxIsQueued(g->ram, (int32_t)a0);
}

static int32_t call_ae_SfxLookupTone(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SfxLookupTone(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_SfxVoiceAlloc(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SfxVoiceAlloc(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_SfxVoiceStart(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SfxVoiceStart(g->ram, &g->host.sound_hw, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_SfxVoiceStartEx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SfxVoiceStartEx(g->ram, &g->host.sound_hw, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (int32_t)a3, (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u), (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 20u));
}

static int32_t call_ae_SfxVoiceStop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SfxVoiceStop(g->ram, &g->host.sound_hw, (int32_t)a0);
    return 0;
}

static int32_t call_ae_SoundDelayedQueueAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundDelayedQueueAdd(g->ram, (int32_t)a0, (int32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_SoundEmitterCalcPan(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundEmitterCalcPan(g->ram, (uint32_t)a0, (int32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_SoundLoopAllocSlot(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundLoopAllocSlot(g->ram);
}

static int32_t call_ae_SoundLoopFree(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundLoopFree(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_SoundLoopStartEx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundLoopStartEx(g->ram, &g->host.sound_hw, (int32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_SoundLoopStopAndFree(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundLoopStopAndFree(g->ram, &g->host.sound_hw, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_SpuVolumeClamp(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SpuVolumeClamp(g->ram, (int32_t)a0, (int32_t)a1);
}

/* ---- src/ui/ae_hud.c ---- */
static int32_t call_ae_AreaTexCopyDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaTexCopyDraw(g->ram, &g->hud, (int32_t)a0);
    return 0;
}

static int32_t call_ae_FontDrawGlyph(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FontDrawGlyph(g->ram, &g->hud, (int32_t)a0, (int32_t)a1, (uint32_t)a2);
    return 0;
}

static int32_t call_ae_GetDAT_800B01C2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GetDAT_800B01C2(g->ram);
}

static int32_t call_ae_HudCounterC_Draw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HudCounterC_Draw(g->ram, &g->hud);
}

static int32_t call_ae_HudCounterC_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HudCounterC_Update(g->ram, &g->hud);
}

static int32_t call_ae_HudDrawDigit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawDigit(g->ram, &g->hud, (int32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_HudDrawGadgetIcon(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HudDrawGadgetIcon(g->ram, &g->hud, (int32_t)a0, (int32_t)a1, (int32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_HudDrawGadgetIcons(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawGadgetIcons(g->ram, &g->hud);
    return 0;
}

static int32_t call_ae_HudDrawGadgetSlotIcons(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawGadgetSlotIcons(g->ram, &g->hud);
    return 0;
}

static int32_t call_ae_HudDrawGadgetSlots(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawGadgetSlots(g->ram, &g->hud);
    return 0;
}

static int32_t call_ae_HudDrawGadgetSlotsFrame(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawGadgetSlotsFrame(g->ram, &g->hud);
    return 0;
}

static int32_t call_ae_HudDrawHealth(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawHealth(g->ram, &g->hud);
    return 0;
}

static int32_t call_ae_HudDrawIcon16(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawIcon16(g->ram, &g->hud, (int32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_HudDrawIconSprite(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawIconSprite(g->ram, &g->hud, (int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_HudDrawNumber(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawNumber(g->ram, &g->hud, (int32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_HudDrawTime(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawTime(g->ram, &g->hud, (int32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_HudDrawTimerGauge(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawTimerGauge(g->ram, &g->hud, (int32_t)a0);
    return 0;
}

static int32_t call_ae_HudPanelA_DrawIcon(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudPanelA_DrawIcon(g->ram, &g->hud, (int32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_HudPanelA_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HudPanelA_Update(g->ram, &g->hud);
}

static int32_t call_ae_HudPanelB_DrawSprite(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudPanelB_DrawSprite(g->ram, &g->hud, (int32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_HudPanelB_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HudPanelB_Update(g->ram, &g->hud);
}

static int32_t call_ae_MessageBoxDrawPrompt(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MessageBoxDrawPrompt(g->ram, &g->hud);
    return 0;
}

static int32_t call_ae_MessageBoxDrawText(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MessageBoxDrawText(g->ram, &g->hud);
    return 0;
}

static int32_t call_ae_MessageBoxDrawWindow(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MessageBoxDrawWindow(g->ram, &g->hud);
    return 0;
}

static int32_t call_ae_MessageBoxUpdateDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MessageBoxUpdateDraw(g->ram, &g->hud);
    return 0;
}

static int32_t call_ae_RadarUsable(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_RadarUsable(g->ram);
}

static int32_t call_ae_StageDrawScreenOverlay(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageDrawScreenOverlay(g->ram, &g->hud, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_TextNextGlyph(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_TextNextGlyph(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_TextSetCursor(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TextSetCursor(g->ram, (int32_t)a0, (int32_t)a1);
    return 0;
}

/* ---- src/ui/ae_menuwin.c ---- */
static int32_t call_ae_MenuStrLen(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MenuStrLen(g->ram, (uint32_t)a0);
}

static int32_t call_ae_MenuWindowAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MenuWindowAdd(g->ram, (uint32_t)a0);
}

static int32_t call_ae_MenuWindowsRun(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MenuWindowsRun(g->ram, &g->menu_win, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_WriteSizedValue(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_WriteSizedValue(g->ram, (uint32_t)a0, (uint32_t)a1, (int32_t)a2);
}

/* ---- src/ui/ae_pausemenu.c ---- */
static int32_t call_ae_DrawScaledModelAt(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_DrawScaledModelAt(g->ram, &g->pause_menu, (uint32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_HudDrawWindowFrame(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudDrawWindowFrame(g->ram, &g->pause_menu, (int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_MessageBoxDrawString(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MessageBoxDrawString(g->ram, &g->pause_menu, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_PauseItemEnabled(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PauseItemEnabled(g->ram, (int32_t)a0);
}

static int32_t call_ae_PauseMenuDrawButton(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PauseMenuDrawButton(g->ram, &g->pause_menu, (int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3, (uint32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u));
}

static int32_t call_ae_PauseMenuDrawButtons(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PauseMenuDrawButtons(g->ram, &g->pause_menu);
    return 0;
}

static int32_t call_ae_PauseMenuRun(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PauseMenuRun(g->ram, &g->pause_menu);
}

static int32_t call_ae_PauseMenuSetLayout(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PauseMenuSetLayout(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_PauseMenuUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PauseMenuUpdate(g->ram, &g->pause_menu);
}

static int32_t call_ae_PlayerStopGadgetSounds(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerStopGadgetSounds(g->ram, &g->pause_menu, (int32_t)a0);
    return 0;
}

static int32_t call_ae_RadarDrawModel(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_RadarDrawModel(g->ram, &g->pause_menu, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_ScreenSnapshotRestore(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ScreenSnapshotRestore(g->ram, &g->pause_menu);
    return 0;
}

static int32_t call_ae_ScreenWipeBlurBlock(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ScreenWipeBlurBlock(g->ram, &g->pause_menu, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ScreenWipeReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ScreenWipeReset(g->ram);
    return 0;
}

static int32_t call_ae_ScreenWipeStep(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ScreenWipeStep(g->ram, &g->pause_menu);
    return 0;
}

static int32_t call_ae_SetDAT_800BEF80(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SetDAT_800BEF80(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_SoundLoopsResume(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SoundLoopsResume(g->ram, &g->pause_menu);
    return 0;
}

static int32_t call_ae_SoundLoopsSetPan(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundLoopsSetPan(g->ram, &g->pause_menu, (int32_t)a0);
}

static int32_t call_ae_TextWidthUntilGlyph(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_TextWidthUntilGlyph(g->ram, &g->pause_menu, (uint32_t)a0, (uint32_t)a1);
}

/* ---- src/world/ae_area02.c ---- */
static int32_t call_ae_HalfPipeRangeTest(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HalfPipeRangeTest(g->ram, (int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

/* ---- src/world/ae_area02_paths.c ---- */
static int32_t call_ae_ObjAdvancePath(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjAdvancePath(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjFacePathPoint(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjFacePathPoint(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjFollowPath(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjFollowPath(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_ObjMove(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjMove(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjSeekPathPoint(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjSeekPathPoint(g->ram, &g->zako1, (uint32_t)a0, (uint32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_ObjSeekPoint(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjSeekPoint(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3, (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u));
}

static int32_t call_ae_ObjSetPosToPathPoint(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSetPosToPathPoint(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ZakoHooksAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ZakoHooksAdd(g->ram, (uint32_t)a0);
}

/* ---- src/world/ae_areaset.c ---- */
static int32_t call_ae_AreaBumpAlloc(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaBumpAlloc(g->ram, &g->area_set, (uint32_t)a0);
}

static int32_t call_ae_AreaBumpAllocSet(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaBumpAllocSet(g->ram, &g->area_set, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_AreaGetKind(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaGetKind(g->ram, &g->area_set, (int32_t)a0);
}

static int32_t call_ae_AreaGetLevel(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaGetLevel(g->ram, &g->area_set, (int32_t)a0);
}

static int32_t call_ae_AreaSetFeature30(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaSetFeature30(g->ram, &g->area_set, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_AreaSetFeature31(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaSetFeature31(g->ram, &g->area_set, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_AreaSetFeature32(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaSetFeature32(g->ram, &g->area_set, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_AreaSetIndicatorModelA(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaSetIndicatorModelA(g->ram, &g->area_set, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_AreaSetIndicatorModelB(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaSetIndicatorModelB(g->ram, &g->area_set, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_AreaSetSpecialActor(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaSetSpecialActor(g->ram, &g->area_set, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_AreaSetZoneBoxes(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaSetZoneBoxes(g->ram, &g->area_set, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_AreaSpecialFlagsClear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaSpecialFlagsClear(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_BossInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_BossInit(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_BossKindForArea(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BossKindForArea(g->ram, &g->area_set);
}

static int32_t call_ae_EffectObjsReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectObjsReset(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_EffectsReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectsReset(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_EffectsResetSubsystems(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_EffectsResetSubsystems(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_FUN_8013d1ac(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FUN_8013d1ac(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_FUN_8013da10(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FUN_8013da10(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_FUN_8013dae8(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FUN_8013dae8(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_FUN_8013dc68(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FUN_8013dc68(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_FUN_8013df90(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FUN_8013df90(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_FxSlotsReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FxSlotsReset(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_FxTableClear12(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FxTableClear12(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_HeapAlloc(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HeapAlloc(g->ram, &g->area_set, (uint32_t)a0);
}

static int32_t call_ae_HeapInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HeapInit(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_IndicatorsReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_IndicatorsReset(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_MessageBoxClose(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MessageBoxClose(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_ModelInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ModelInit(g->ram, &g->area_set, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_MonkeyLodModelsInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MonkeyLodModelsInit(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_PlayerFxBlinkReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerFxBlinkReset(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_ProjectileGroupsClear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileGroupsClear(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_ProjectilesReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectilesReset(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_RandomParticlesReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_RandomParticlesReset(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_RcCarDespawn(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_RcCarDespawn(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_SmallocError(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SmallocError(g->ram, &g->area_set, (uint32_t)a0);
}

static int32_t call_ae_SparkTableClear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SparkTableClear(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_StaticModelInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StaticModelInit(g->ram, &g->area_set, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_TableClearDAT_800BEF60(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TableClearDAT_800BEF60(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_TargetListClear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TargetListClear(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_TrailsReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TrailsReset(g->ram, &g->area_set);
    return 0;
}

static int32_t call_ae_exit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_exit(g->ram, &g->area_set, (uint32_t)a0);
}

static int32_t call_ae_printf(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_printf(g->ram, &g->area_set, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

/* ---- src/world/ae_gimmick.c ---- */
static int32_t call_ae_GimmickActivate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickActivate(g->ram, &g->gimmick, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_GimmickDeactivateChain(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickDeactivateChain(g->ram, (uint32_t)a0);
}

static int32_t call_ae_SwingDoorSetPivot(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SwingDoorSetPivot(g->ram, (uint32_t)a0, (int32_t)a1, (int32_t)a2);
}

/* ---- src/world/ae_gimmick_a.c ---- */
static int32_t call_ae_GimmickAnyBodyInside(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickAnyBodyInside(g->ram, &g->gimmick_a, (uint32_t)a0, (int32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_GimmickCarryBodiesByDelta(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickCarryBodiesByDelta(g->ram, &g->gimmick_a, (uint32_t)a0, (uint32_t)a1, (int32_t)a2);
}

static int32_t call_ae_GimmickCarryBodiesSwing(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickCarryBodiesSwing(g->ram, &g->gimmick_a, (uint32_t)a0, (uint32_t)a1, (int32_t)a2);
}

static int32_t call_ae_GimmickGetState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickGetState(g->ram, (int32_t)a0);
}

static int32_t call_ae_GimmickInitFromProgress(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickInitFromProgress(g->ram, &g->gimmick_a);
}

static int32_t call_ae_GimmickPlayTypeSfx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickPlayTypeSfx(g->ram, &g->gimmick_a, (uint32_t)a0);
}

static int32_t call_ae_GimmickRestoreStates(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickRestoreStates(g->ram);
}

static int32_t call_ae_GimmickSaveApplyProgress(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickSaveApplyProgress(g->ram, &g->gimmick_a);
}

static int32_t call_ae_GimmickSaveEntryAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickSaveEntryAdd(g->ram, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_GimmickSaveStates(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickSaveStates(g->ram);
}

static int32_t call_ae_GimmickSaveTableReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickSaveTableReset(g->ram);
}

static int32_t call_ae_GimmickSpawnAll(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickSpawnAll(g->ram, &g->gimmick_a);
}

static int32_t call_ae_GimmickSwingAdvance(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickSwingAdvance(g->ram, (uint32_t)a0);
}

static int32_t call_ae_GimmickSwingAngle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickSwingAngle(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_GimmickSwingCarryPoint(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickSwingCarryPoint(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_GimmickSwingFirstStep(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickSwingFirstStep(g->ram, (uint32_t)a0);
}

static int32_t call_ae_GimmickSwingProbeWall(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickSwingProbeWall(g->ram, &g->gimmick_a, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_GimmickTypeToSfx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickTypeToSfx(g->ram, (int32_t)a0);
}

static int32_t call_ae_GimmickUpdateAll(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickUpdateAll(g->ram, &g->gimmick_a);
}

static int32_t call_ae_Gimmick_Hit_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Hit_Update(g->ram, &g->gimmick_a, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_Counter(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_Counter(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_PairToggle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_PairToggle(g->ram, &g->gimmick_a, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_ThresholdDoor(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_ThresholdDoor(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_LiftDoor_Init(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_LiftDoor_Init(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_LiftDoor_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_LiftDoor_Update(g->ram, &g->gimmick_a, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_SlideDoor_Init(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_SlideDoor_Init(g->ram, &g->gimmick_a, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_SlideDoor_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_SlideDoor_Update(g->ram, &g->gimmick_a, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_SpinRoller_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_SpinRoller_Update(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_SwingDoor_Init(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_SwingDoor_Init(g->ram, &g->gimmick_a, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_SwingDoor_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_SwingDoor_Update(g->ram, &g->gimmick_a, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Switch_Init(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Switch_Init(g->ram, &g->gimmick_a, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Switch_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Switch_Update(g->ram, &g->gimmick_a, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_ThresholdDoor_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_ThresholdDoor_Update(g->ram, &g->gimmick_a, (uint32_t)a0);
}

/* ---- src/world/ae_gimmick_b.c ---- */
static int32_t call_ae_GimmickMoveCollider(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickMoveCollider(g->ram, &g->gimmick_b, (int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_GimmickMoveColliderTo(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickMoveColliderTo(g->ram, &g->gimmick_b, (int32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_GimmickPlayerInBox(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickPlayerInBox(g->ram, (int32_t)a0, (uint32_t)a1, (int32_t)a2);
}

static int32_t call_ae_GimmickStateTablePtr(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickStateTablePtr(g->ram);
}

static int32_t call_ae_Gimmick_Bounce_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Bounce_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Breakable_Init(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Breakable_Init(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Breakable_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Breakable_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Counter_EventCam_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Counter_EventCam_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Falling_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Falling_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Grab_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Grab_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_Closed(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_Closed(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_Closed2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_Closed2(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_Closed3(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_Closed3(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_CounterGate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_CounterGate(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_Disabled(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_Disabled(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_Hidden(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_Hidden(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_MonkeyQuota(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_MonkeyQuota(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_PropFlag2000(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_PropFlag2000(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_PropHidden(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_PropHidden(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_PropMeshSet(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_PropMeshSet(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_PropTilt(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_PropTilt(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_RecordOrigin(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_RecordOrigin(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Init_ZoneMask(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Init_ZoneMask(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_LinkedLift_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_LinkedLift_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_MonkeyQuota_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_MonkeyQuota_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_NopInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_NopInit(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_NopUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_NopUpdate(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_NopUpdate2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_NopUpdate2(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_NopUpdate3(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_NopUpdate3(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_NopUpdate4(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_NopUpdate4(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Oscillate_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Oscillate_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_PlayerBlast_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_PlayerBlast_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_PlayerBox_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_PlayerBox_Update(g->ram, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_PlayerInCylinder_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_PlayerInCylinder_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_ProxTrigger_Init(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_ProxTrigger_Init(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_ProxTrigger_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_ProxTrigger_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Resize_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Resize_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Rotate_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Rotate_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_SliderQuiet_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_SliderQuiet_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

static int32_t call_ae_Gimmick_Slider_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Gimmick_Slider_Update(g->ram, &g->gimmick_b, (uint32_t)a0);
}

/* ---- src/world/ae_items_init.c ---- */
static int32_t call_ae_ApplyLevelFog(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ApplyLevelFog(g->ram, &g->items_init);
    return 0;
}

static int32_t call_ae_CollectedItemsHas(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollectedItemsHas(g->ram, (uint32_t)a0);
}

static int32_t call_ae_GetCollectedItemsTable(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GetCollectedItemsTable(g->ram);
}

static int32_t call_ae_ItemsLoadForArea(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ItemsLoadForArea(g->ram, &g->items_init);
    return 0;
}

static int32_t call_ae_MatrixCopy8(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MatrixCopy8(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_PickupSpawnSlot(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PickupSpawnSlot(g->ram, &g->items_init, (uint32_t)a0, (uint32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_SetColorMatrixMode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SetColorMatrixMode(g->ram, &g->items_init, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
    return 0;
}

/* ---- src/world/ae_misc.c ---- */
static int32_t call_ae_AreaFeature30GetState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaFeature30GetState(g->ram);
}

static int32_t call_ae_AreaOverlayUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaOverlayUpdate(g->ram, &g->misc);
}

static int32_t call_ae_BgmApplyPlayerEffect(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BgmApplyPlayerEffect(g->ram);
}

static int32_t call_ae_CameraNudge(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CameraNudge(g->ram, (int32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_CollectedItemsAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_CollectedItemsAdd(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ColliderScriptRun(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderScriptRun(g->ram, &g->misc, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ColliderScriptsUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderScriptsUpdate(g->ram, &g->misc);
}

static int32_t call_ae_CollisionRayTest(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return ae_game_collision_ray(g, a0, a1, a2);
}

static int32_t call_ae_HudCounterC_Start(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HudCounterC_Start(g->ram, (int32_t)a0);
}

static int32_t call_ae_HudPanelB_Start(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HudPanelB_Start(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_HudPopupStartPickup(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudPopupStartPickup(g->ram);
    return 0;
}

static int32_t call_ae_HudPopupUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudPopupUpdate(g->ram, &g->misc);
    return 0;
}

static int32_t call_ae_IndicatorHintFree(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_IndicatorHintFree(g->ram, &g->misc);
    return 0;
}

static int32_t call_ae_IndicatorHintReady(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_IndicatorHintReady(g->ram, &g->misc);
}

static int32_t call_ae_IndicatorsUpdateDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_IndicatorsUpdateDraw(g->ram, &g->misc);
    return 0;
}

static int32_t call_ae_ItemDrawModelEnvMap(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ItemDrawModelEnvMap(g->ram, &g->misc, (int32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ItemsDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ItemsDraw(g->ram, &g->misc);
    return 0;
}

static int32_t call_ae_ItemsUpdateDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ItemsUpdateDraw(g->ram, &g->misc);
    return 0;
}

static int32_t call_ae_MatrixRotYMul(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MatrixRotYMul(g->ram, &g->misc, (int32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_MeshCalcEnvUVs(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MeshCalcEnvUVs(g->ram, &g->misc, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_MessageBoxBusy(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MessageBoxBusy(g->ram);
}

static int32_t call_ae_MessageBoxSetText(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MessageBoxSetText(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_PickupCollect(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PickupCollect(g->ram, &g->misc, (int32_t)a0);
    return 0;
}

static int32_t call_ae_PickupSetPosition(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PickupSetPosition(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_PickupsUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PickupsUpdate(g->ram, &g->misc);
    return 0;
}

static int32_t call_ae_PlayerClearDAT44A2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerClearDAT44A2(g->ram);
    return 0;
}

static int32_t call_ae_PlayerEyeBlinkUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerEyeBlinkUpdate(g->ram, &g->misc);
    return 0;
}

static int32_t call_ae_PlayerHeal(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PlayerHeal(g->ram, &g->misc, (int32_t)a0);
    return 0;
}

static int32_t call_ae_SeqFadeTrackProgram(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SeqFadeTrackProgram(g->ram, (int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_SeqGetChannelProgram(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SeqGetChannelProgram(g->ram, (int32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_SfxVoiceSetPanVol(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SfxVoiceSetPanVol(g->ram, &g->misc, (int32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_SoundEmitterAttenuation(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundEmitterAttenuation(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_SoundEmitterUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundEmitterUpdate(g->ram, &g->misc);
}

static int32_t call_ae_SpecialActorPlayerNear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpecialActorPlayerNear(g->ram);
    return 0;
}

static int32_t call_ae_StageTimerAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageTimerAdd(g->ram, (int32_t)a0);
    return 0;
}

/* ---- src/world/ae_misc_batch.c ---- */
static int32_t call_ae_AreaFeature30Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaFeature30Update(g->ram, &g->misc);
    return 0;
}

static int32_t call_ae_BossUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BossUpdate(g->ram, &g->misc, (uint32_t)a0);
}

static int32_t call_ae_CameraRcCarActive(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CameraRcCarActive(g->ram);
}

static int32_t call_ae_ColliderResetToStart(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ColliderResetToStart(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ColliderScriptCarry(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderScriptCarry(g->ram, &g->misc, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ColliderScriptCommand(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderScriptCommand(g->ram, &g->misc, (uint32_t)a0, (uint32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_ColliderScriptFollowPath(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderScriptFollowPath(g->ram, &g->misc, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ColliderScriptPlayerMove(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderScriptPlayerMove(g->ram, &g->misc, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ColorFadeReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ColorFadeReset(g->ram);
    return 0;
}

static int32_t call_ae_GetZoneMask(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GetZoneMask(g->ram, (int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_HudFindGadgetIconPos(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_HudFindGadgetIconPos(g->ram);
    return 0;
}

static int32_t call_ae_Indicator_Register(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Indicator_Register(g->ram, &g->misc, (int32_t)a0, (uint32_t)a1, (int32_t)a2);
}

static int32_t call_ae_MenuItemSetRange(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MenuItemSetRange(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
    return 0;
}

static int32_t call_ae_MenuItemSetStep(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MenuItemSetStep(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_PadRumbleReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_PadRumbleReset(g->ram, (int32_t)a0);
    return 0;
}

static int32_t call_ae_PlayerIsSubmerged(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerIsSubmerged(g->ram);
}

static int32_t call_ae_ProgressFlagGet(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ProgressFlagGet(g->ram, (uint32_t)a0);
}

static int32_t call_ae_StageExitPending(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageExitPending(g->ram);
}

static int32_t call_ae_VramScrollUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_VramScrollUpdate(g->ram, &g->misc);
    return 0;
}

/* ---- src/world/ae_ovl18c.c ---- */
static int32_t call_ae_Area02_CopyEnemyModel(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_CopyEnemyModel(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
    return 0;
}

static int32_t call_ae_Area02_EffectBelowGround(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Area02_EffectBelowGround(g->ram, &g->ovl18c, (uint32_t)a0);
}

static int32_t call_ae_Area02_EffectDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Area02_EffectDraw(g->ram, &g->ovl18c);
}

static int32_t call_ae_Area02_EnemyAnimationTriggerUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_EnemyAnimationTriggerUpdate(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_EnemyEnterDefeat(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_EnemyEnterDefeat(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_EnemyEnterHitReaction(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_EnemyEnterHitReaction(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_EnemyHitUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_EnemyHitUpdate(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_EnemyPhase6Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Area02_EnemyPhase6Update(g->ram, &g->ovl18c, (uint32_t)a0);
}

static int32_t call_ae_Area02_EnemySplashUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_EnemySplashUpdate(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_EnemyType0B_Update(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_EnemyType0B_Update(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_FindFreeEffectSlot(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Area02_FindFreeEffectSlot(g->ram);
}

static int32_t call_ae_Area02_InitializeEffects(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_InitializeEffects(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_InterpolateAnimValue(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Area02_InterpolateAnimValue(g->ram, (uint32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_Area02_PlayerOutsideHeightAndRadius(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Area02_PlayerOutsideHeightAndRadius(g->ram, &g->ovl18c);
}

static int32_t call_ae_Area02_PlayerWithinHeightAndRadius(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Area02_PlayerWithinHeightAndRadius(g->ram, &g->ovl18c);
}

static int32_t call_ae_Area02_SpawnAmbientParticle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_SpawnAmbientParticle(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_SpawnEffectBurst(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_SpawnEffectBurst(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_SpawnEffectParticle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_SpawnEffectParticle(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_SpawnEnemyHitParticle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_SpawnEnemyHitParticle(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_SpawnRadialEffect(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_SpawnRadialEffect(g->ram, &g->ovl18c, (uint32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_Area02_StageSpecificEnemyUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_StageSpecificEnemyUpdate(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_SurfaceDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Area02_SurfaceDraw(g->ram, &g->ovl18c);
}

static int32_t call_ae_Area02_SurfaceEffectsInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_Area02_SurfaceEffectsInit(g->ram, &g->ovl18c);
}

static int32_t call_ae_Area02_UpdateEffects(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_UpdateEffects(g->ram, &g->ovl18c);
    return 0;
}

static int32_t call_ae_Area02_UpdateEnemyWake(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_UpdateEnemyWake(g->ram, &g->ovl18c, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Area02_UpdateEnemyWakeOrigin(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Area02_UpdateEnemyWakeOrigin(g->ram, (uint32_t)a0);
    return 0;
}

/* ---- src/world/ae_progress.c ---- */
static int32_t call_ae_AreaKindVisitedGet(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaKindVisitedGet(g->ram, &g->area_set, (int32_t)a0);
}

static int32_t call_ae_ClearDAT_800BEF70(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ClearDAT_800BEF70(g->ram);
    return 0;
}

static int32_t call_ae_DrawTextSprites(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_DrawTextSprites(g->ram, &g->progress, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_ExitSpecialArea(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ExitSpecialArea(g->ram, &g->progress);
}

static int32_t call_ae_HubDestinationState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HubDestinationState(g->ram, &g->progress, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_HubGateBitTest(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HubGateBitTest(g->ram, (uint32_t)a0);
}

static int32_t call_ae_HubGateLocked(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HubGateLocked(g->ram, (int32_t)a0);
}

static int32_t call_ae_LevelProgressGet(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_LevelProgressGet(g->ram, (uint32_t)a0);
}

static int32_t call_ae_MeshAnimAdvance(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MeshAnimAdvance(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_OpenPauseMenu(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_OpenPauseMenu(g->ram, (int32_t)a0);
    return 0;
}

static int32_t call_ae_ProgressMarkStageResult(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ProgressMarkStageResult(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_RecordBestTime(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_RecordBestTime(g->ram);
}

static int32_t call_ae_SaveBlockPtr(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SaveBlockPtr(g->ram);
}

static int32_t call_ae_SaveBufferFill(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SaveBufferFill(g->ram, &g->progress);
}

static int32_t call_ae_SaveCaptureState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SaveCaptureState(g->ram, &g->progress);
}

static int32_t call_ae_SaveHeaderFill(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SaveHeaderFill(g->ram);
}

static int32_t call_ae_SaveSlotInfoPtr(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SaveSlotInfoPtr(g->ram, (uint32_t)a0);
}

static int32_t call_ae_SoundGetVolumes(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundGetVolumes(g->ram, (uint32_t)a0);
}

static int32_t call_ae_SoundLoopsPause(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundLoopsPause(g->ram, &g->progress);
}

static int32_t call_ae_SoundRestoreVolumes(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundRestoreVolumes(g->ram, &g->progress);
}

static int32_t call_ae_SoundSaveVolumes(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundSaveVolumes(g->ram);
}

static int32_t call_ae_SoundSetVolumes(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundSetVolumes(g->ram, &g->progress, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_SoundVolumesDefault(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundVolumesDefault(g->ram, &g->progress, (int32_t)a0);
}

static int32_t call_ae_SpriteEffectQueue0(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpriteEffectQueue0(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_UnlockSpecialArea(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_UnlockSpecialArea(g->ram);
}

/* ---- src/world/ae_solid.c ---- */
static int32_t call_ae_CollHalfPipeHeight(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollHalfPipeHeight(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollProbeCounterInc(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollProbeCounterInc(g->ram);
}

static int32_t call_ae_CollShape_Box(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollShape_Box(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollShape_Cylinder(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollShape_Cylinder(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollShape_DiagonalBox(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollShape_DiagonalBox(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollShape_Ramp2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollShape_Ramp2(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollShape_Ramp4(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollShape_Ramp4(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollShape_Ramp45(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollShape_Ramp45(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollShape_WallDiagonal(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollShape_WallDiagonal(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollShape_WallEdge(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollShape_WallEdge(g->ram, &g->solid, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollShape_WallThin(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollShape_WallThin(g->ram, &g->solid, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_CollShape_Wedge(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_CollShape_Wedge(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_ColliderHalfSpaceInside(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderHalfSpaceInside(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_ColliderHeightAt(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderHeightAt(g->ram, &g->solid, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_ColliderRingHeight(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderRingHeight(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_ColliderSlopeCorner(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderSlopeCorner(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_ColliderSlopeHeight(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderSlopeHeight(g->ram, &g->solid, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_ColliderSlopeSide(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderSlopeSide(g->ram, (uint32_t)a0, (uint32_t)a1, (int32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_ColliderStripHeight(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderStripHeight(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_ColliderStripHeightOut(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderStripHeightOut(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_ColliderTrenchHeight(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderTrenchHeight(g->ram, (int32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_ColliderTypeActive(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderTypeActive(g->ram, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_DiagonalSignedDistance(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_DiagonalSignedDistance(g->ram, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_GetColliderQuadrantMaskXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GetColliderQuadrantMaskXZ(g->ram, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_GetSolidHeight(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GetSolidHeight(g->ram, &g->solid, (int32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_GetSurfaceType(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GetSurfaceType(g->ram, (int32_t)a0, (int32_t)a1, (int32_t)a2);
}

/* ---- src/world/ae_stage.c ---- */
static int32_t call_ae_AreaLoad(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaLoad(g->ram, &g->stage, (int32_t)a0, (int32_t)a1);
}

static int32_t call_ae_AreaLoadGridAndProps(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_AreaLoadGridAndProps(g->ram, &g->stage);
    return 0;
}

static int32_t call_ae_AreaLoadQueueRead(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaLoadQueueRead(g->ram, &g->stage);
}

static int32_t call_ae_AreaLoadStart(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaLoadStart(g->ram, &g->stage, (int32_t)a0);
}

static int32_t call_ae_AreaLoadStartSeek(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_AreaLoadStartSeek(g->ram, &g->stage, (int32_t)a0);
}

static int32_t call_ae_StageProgressOnEnter(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageProgressOnEnter(g->ram, &g->stage, (int32_t)a0);
}

static int32_t call_ae_StageProgressOnExit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageProgressOnExit(g->ram, &g->stage, (int32_t)a0);
}

static int32_t call_ae_StageProgressRestore(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageProgressRestore(g->ram, &g->stage, (int32_t)a0);
}

static int32_t call_ae_StageProgressRestore_Nop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageProgressRestore_Nop(g->ram);
    return 0;
}

static int32_t call_ae_StageProgressRestore_Nop2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_StageProgressRestore_Nop2(g->ram);
    return 0;
}

static int32_t call_ae_StageProgressStoreArea(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageProgressStoreArea(g->ram, &g->stage, (int32_t)a0);
}

static int32_t call_ae_StageRequest(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageRequest(g->ram, &g->stage, (int32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_StageRequestExit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageRequestExit(g->ram, &g->stage);
}

static int32_t call_ae_StageRequestRetry(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageRequestRetry(g->ram, &g->stage);
}

static int32_t call_ae_StageRequestUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageRequestUpdate(g->ram, &g->stage);
}

/* ---- src/world/ae_stagefade.c ---- */
static int32_t call_ae_FadeSpiralDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_FadeSpiralDraw(g->ram, &g->stage_fade);
    return 0;
}

static int32_t call_ae_HudPanelA_Start(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_HudPanelA_Start(g->ram, (int32_t)a0);
}

static int32_t call_ae_ScreenFlashFadeDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ScreenFlashFadeDraw(g->ram, &g->stage_fade);
    return 0;
}

static int32_t call_ae_SoundLoopsReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundLoopsReset(g->ram);
}

static int32_t call_ae_SoundStop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_SoundStop(g->ram, &g->stage_fade, (int32_t)a0);
}

static int32_t call_ae_StageFadeUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageFadeUpdate(g->ram, &g->stage_fade);
}

static int32_t call_ae_TimListResidentUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_TimListResidentUpdate(g->ram, &g->stage_fade, (int32_t)a0, (int32_t)a1);
}

/* ---- src/world/ae_world.c ---- */
static int32_t call_ae_ClearWords(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ClearWords(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_GetGroundHeight(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GetGroundHeight(g->ram, &g->world, (int32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_GetGroundHeightEx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GetGroundHeightEx(g->ram, &g->world, (int32_t)a0, (int32_t)a1, (int32_t)a2, (uint32_t)a3);
}

static int32_t call_ae_GimmickEvent(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickEvent(g->ram, &g->world, (uint32_t)a0, (int32_t)a1, (uint32_t)a2);
}

static int32_t call_ae_GimmickGetInfo(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GimmickGetInfo(g->ram, (int32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_ObjSlotGetAnimIndex(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjSlotGetAnimIndex(g->ram, (int32_t)a0);
}

static int32_t call_ae_ObjSlotGetTarget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjSlotGetTarget(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_ObjSlotSetField_CF(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSlotSetField_CF(g->ram, (int32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_SetDAT_800E77E2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SetDAT_800E77E2(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_StageExitIsActive(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_StageExitIsActive(g->ram);
}

static int32_t call_ae_TargetListAdd(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_TargetListAdd(g->ram, (uint32_t)a0);
}

static int32_t call_ae_TargetListRemove(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_TargetListRemove(g->ram, (uint32_t)a0);
    return 0;
}

/* ---- src/world/ae_zako1.c ---- */
static int32_t call_ae_ModelLightColorsToScratch(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ModelLightColorsToScratch(g->ram, &g->zako1, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_ObjAnimClampLastFrame(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjAnimClampLastFrame(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjApplyDamage(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjApplyDamage(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_ObjApplyVelocity(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjApplyVelocity(g->ram, &g->zako1, (uint32_t)a0);
}

static int32_t call_ae_ObjBounceOnGround(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjBounceOnGround(g->ram, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ObjDeathSfx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjDeathSfx(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjGroundHeightCached(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjGroundHeightCached(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_ObjIsOwnerType0B(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjIsOwnerType0B(g->ram, (uint32_t)a0);
}

static int32_t call_ae_ObjKnockbackDecay(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjKnockbackDecay(g->ram, (uint32_t)a0);
}

static int32_t call_ae_ObjPlaySfx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjPlaySfx(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjPuffAt(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjPuffAt(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_ObjReenableBody(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjReenableBody(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjResetMotion(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjResetMotion(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjSwapToReactAnim(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSwapToReactAnim(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjSyncBodyHitFlags(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSyncBodyHitFlags(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjTurnTowardsXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjTurnTowardsXZ(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3, (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u));
    return 0;
}

static int32_t call_ae_ObjUpdateMotionState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjUpdateMotionState(g->ram, &g->zako1, (uint32_t)a0);
}

static int32_t call_ae_PointOffscreen(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PointOffscreen(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_ZakoAllocSlot(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ZakoAllocSlot(g->ram);
}

static int32_t call_ae_ZakoApplyHit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoApplyHit(g->ram, &g->zako1, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ZakoBuildMatrix(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoBuildMatrix(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoCullAndDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoCullAndDraw(g->ram, &g->zako1);
    return 0;
}

static int32_t call_ae_ZakoDebugMenuDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoDebugMenuDraw(g->ram, &g->zako1);
    return 0;
}

static int32_t call_ae_ZakoDefeat(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoDefeat(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoDraw(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoDropPickups(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoDropPickups(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoGetBonePoint(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoGetBonePoint(g->ram, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (uint32_t)a3);
    return 0;
}

static int32_t call_ae_ZakoHooksRun(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoHooksRun(g->ram, &g->zako1);
    return 0;
}

static int32_t call_ae_ZakoOnHitByGadget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoOnHitByGadget(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoPoseHidden(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoPoseHidden(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoSetKnockback(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoSetKnockback(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_ZakoSpawnNearby(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoSpawnNearby(g->ram, &g->zako1);
    return 0;
}

static int32_t call_ae_ZakoUpdateAll(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoUpdateAll(g->ram, &g->zako1);
    return 0;
}

static int32_t call_ae_ZakoUpdateColliderPos(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoUpdateColliderPos(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoUpdateLinkPoints(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoUpdateLinkPoints(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoUpdateLoop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoUpdateLoop(g->ram, &g->zako1);
    return 0;
}

/* ---- src/world/ae_zako2.c ---- */
static int32_t call_ae_ColorFadeSet(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ColorFadeSet(g->ram, (int32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_DebugTextPrintf(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_DebugTextPrintf(g->ram, &g->zako2, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_DebugTextPrintfWhite(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_DebugTextPrintfWhite(g->ram, &g->zako2, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_IsTimeAttack(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_IsTimeAttack(g->ram);
}

static int32_t call_ae_PickupPickKindA(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PickupPickKindA(g->ram);
}

static int32_t call_ae_PickupPickKindB(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PickupPickKindB(g->ram);
}

static int32_t call_ae_ProjectileAddHitBody(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectileAddHitBody(g->ram, &g->zako2, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_ProjectilesDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectilesDraw(g->ram, &g->zako2);
    return 0;
}

static int32_t call_ae_ProjectilesUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectilesUpdate(g->ram, &g->zako2);
    return 0;
}

static int32_t call_ae_ProjectilesUpdateDraw(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ProjectilesUpdateDraw(g->ram, &g->zako2);
    return 0;
}

static int32_t call_ae_SpawnPickups(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SpawnPickups(g->ram, &g->zako2, (uint32_t)a0, (uint32_t)a1, (int32_t)a2, (uint32_t)a3);
    return 0;
}

/* ---- src/world/ae_zako_init.c ---- */
static int32_t call_ae_GetLevelFirstArea(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_GetLevelFirstArea(g->ram, (int32_t)a0);
}

static int32_t call_ae_MenuItemInitToggle(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MenuItemInitToggle(g->ram, &g->zako, (uint32_t)a0, (uint32_t)a1, (uint32_t)a2);
    return 0;
}

static int32_t call_ae_ModelInitAuto(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ModelInitAuto(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ObjInitModel(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjInitModel(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ObjKill(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjKill(g->ram, &g->zako, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjSetPathFromTable(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSetPathFromTable(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjUnregisterBody(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjUnregisterBody(g->ram, &g->zako, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoClearSlotTable(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoClearSlotTable(g->ram);
    return 0;
}

static int32_t call_ae_ZakoDebugMenuInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoDebugMenuInit(g->ram, &g->zako);
    return 0;
}

static int32_t call_ae_ZakoFindEntryByType(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ZakoFindEntryByType(g->ram, (uint32_t)a0);
}

static int32_t call_ae_ZakoGetAnimTable(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ZakoGetAnimTable(g->ram, (uint32_t)a0, (uint32_t)a1);
}

static int32_t call_ae_ZakoHooksReset(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoHooksReset(g->ram);
    return 0;
}

static int32_t call_ae_ZakoInit(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoInit(g->ram, &g->zako);
    return 0;
}

static int32_t call_ae_ZakoSetAnimFromEntry(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoSetAnimFromEntry(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ZakoSetAreaMode(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoSetAreaMode(g->ram, &g->zako, (uint32_t)a0, (uint32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_ZakoSetupType(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoSetupType(g->ram, &g->zako, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ZakoSpawnFromEntry(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoSpawnFromEntry(g->ram, &g->zako, (uint32_t)a0, (uint32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_ZakoTypeIsIgnored(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ZakoTypeIsIgnored(g->ram, (int32_t)a0);
}

/* ---- src/world/ae_zakoai.c ---- */
static int32_t call_ae_ZakoAreaModeNoop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoAreaModeNoop(g->ram);
    return 0;
}

static int32_t call_ae_ZakoUpdateGenericEnemy(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoUpdateGenericEnemy(g->ram, &g->zako_ai, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoUpdateStageSpecificEnemy(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoUpdateStageSpecificEnemy(g->ram, &g->zako_ai, (uint32_t)a0);
    return 0;
}

/* ---- src/world/ae_zakohelpers.c ---- */
static int32_t call_ae_ObjAccelerate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjAccelerate(g->ram, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_ObjAddHitBody(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjAddHitBody(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3, (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u), (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 20u), (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 24u));
}

static int32_t call_ae_ObjAddHitBodyIfPlayerNear(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjAddHitBodyIfPlayerNear(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3, (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u), (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 20u), (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 24u));
}

static int32_t call_ae_ObjAimAtPlayer(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjAimAtPlayer(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjChasePlayer(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjChasePlayer(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjClampFrame(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjClampFrame(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjCooldown(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjCooldown(g->ram, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ObjFallAndBounce(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjFallAndBounce(g->ram, &g->zako1, (uint32_t)a0);
}

static int32_t call_ae_ObjFallToGround(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjFallToGround(g->ram, &g->zako1, (uint32_t)a0);
}

static int32_t call_ae_ObjFleeFromPlayerInArea(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjFleeFromPlayerInArea(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjFootstepFx(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjFootstepFx(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjGetLinkedObjState(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjGetLinkedObjState(g->ram, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ObjPlayerInCone(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjPlayerInCone(g->ram, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3, (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u));
}

static int32_t call_ae_ObjPlayerInConeFromPos2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjPlayerInConeFromPos2(g->ram, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_ObjPlayerNearPointXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjPlayerNearPointXZ(g->ram, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3, (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u));
}

static int32_t call_ae_ObjPlayerOutOfYRange(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjPlayerOutOfYRange(g->ram, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ObjSetSpeed96(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSetSpeed96(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjSetVelAlongDir(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSetVelAlongDir(g->ram, &g->zako1, (uint32_t)a0, (uint32_t)a1, (int32_t)a2, (int32_t)a3);
    return 0;
}

static int32_t call_ae_ObjSetVelFromFacing(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSetVelFromFacing(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjSetVelTowardPlayer(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSetVelTowardPlayer(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_ObjSetVelTowardTarget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSetVelTowardTarget(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_ObjSpawnParticleAt(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjSpawnParticleAt(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3, (int32_t)ae_rd32(g->ram, g->call_stack_pointer + 16u));
    return 0;
}

static int32_t call_ae_ObjTargetBeyondRange(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjTargetBeyondRange(g->ram, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ObjTryStepOnGround(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjTryStepOnGround(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_ObjTurnToPlayer(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjTurnToPlayer(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjTurnTowardsPlayerXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjTurnTowardsPlayerXZ(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjWalkOnGround(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjWalkOnGround(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ObjWalkToPoint(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjWalkToPoint(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
}

static int32_t call_ae_ObjWanderOrReturn(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ObjWanderOrReturn(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_ObjWanderStep(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ObjWanderStep(g->ram, &g->zako1, (uint32_t)a0, (int32_t)a1, (int32_t)a2);
    return 0;
}

static int32_t call_ae_PlayerWithinRadiusXZ(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_PlayerWithinRadiusXZ(g->ram, (int32_t)a0, (int32_t)a1, (int32_t)a2);
}

static int32_t call_ae_ZakoChasePlayer(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoChasePlayer(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoEnterDeath(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoEnterDeath(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoEnterIdleWait(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoEnterIdleWait(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoReturnIfFar(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoReturnIfFar(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoStateHurt(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoStateHurt(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoStateWatch(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoStateWatch(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_ZakoWalkToTarget(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_ZakoWalkToTarget(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Zako_InitDefaultStats(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Zako_InitDefaultStats(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Zako_Nop(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Zako_Nop(g->ram, (uint32_t)a0, (int32_t)a1);
    return 0;
}

static int32_t call_ae_Zako_RecoverFromStun(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Zako_RecoverFromStun(g->ram, (uint32_t)a0);
    return 0;
}

static int32_t call_ae_Zako_WanderBounce(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_Zako_WanderBounce(g->ram, &g->zako1, (uint32_t)a0);
    return 0;
}

/* ---- src/world/ae_zones.c ---- */
static int32_t call_ae_BuildTerrainBlockMask(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_BuildTerrainBlockMask(g->ram, &g->zones);
}

static int32_t call_ae_ColliderComputeQuadrantMask(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderComputeQuadrantMask(g->ram, &g->zones, (uint32_t)a0);
}

static int32_t call_ae_ColliderZoneMasksUpdate(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ColliderZoneMasksUpdate(g->ram, &g->zones);
}

static int32_t call_ae_DebugMenuInitCounters(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_DebugMenuInitCounters(g->ram);
    return 0;
}

static int32_t call_ae_DebugMenuInitCounters2(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_DebugMenuInitCounters2(g->ram);
    return 0;
}

static int32_t call_ae_DebugMenuInitFlags(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_DebugMenuInitFlags(g->ram);
    return 0;
}

static int32_t call_ae_DebugMenuInitPlayerParams(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_DebugMenuInitPlayerParams(g->ram);
    return 0;
}

static int32_t call_ae_MenuItemInitBound(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_MenuItemInitBound(g->ram, (uint32_t)a0, (uint32_t)a1, (int32_t)a2);
}

static int32_t call_ae_MenuItemSetCallback(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_MenuItemSetCallback(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

static int32_t call_ae_ReadSizedValue(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    return (int32_t)ae_ReadSizedValue(g->ram, (uint32_t)a0, (int32_t)a1);
}

static int32_t call_ae_SvecToDiagMatrix(AeGame *g, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)g; (void)a0; (void)a1; (void)a2; (void)a3;
    ae_SvecToDiagMatrix(g->ram, (uint32_t)a0, (uint32_t)a1);
    return 0;
}

/* Duplicate ledger ports share an address; RAM-image ports are preferred. */
static const AeGameCall CALLS[] = {
    {0x80010B50u, "ae_StageInitMemory", call_ae_StageInitMemory, 0x00000000u},
    {0x80010C3Cu, "ae_AreaGetEntryPoint", call_ae_AreaGetEntryPoint, 0x00000000u},
    {0x80010CA4u, "ae_AreaGetKind", call_ae_AreaGetKind, 0x00000000u},
    {0x80010CC0u, "ae_AreaGetLevel", call_ae_AreaGetLevel, 0x00000000u},
    {0x80010D04u, "ae_AreaLoadStartSeek", call_ae_AreaLoadStartSeek, 0x00000000u},
    {0x80011208u, "ae_AreaLoadQueueRead", call_ae_AreaLoadQueueRead, 0x00000000u},
    {0x800114D8u, "ae_AreaLoadStart", call_ae_AreaLoadStart, 0x00000000u},
    {0x800119F4u, "ae_SaveSlotInfoPtr", call_ae_SaveSlotInfoPtr, 0x00000000u},
    {0x80011F6Cu, "ae_XaPlay", call_ae_XaPlay, 0x00000000u},
    {0x80012360u, "ae_CdMgrQueueCommand", call_ae_CdMgrQueueCommand, 0x00000000u},
    {0x80012A0Cu, "ae_CdMgrIssueCommand", call_ae_CdMgrIssueCommand, 0x00000000u},
    {0x80012C0Cu, "ae_ArcGetMemberPos", call_ae_ArcGetMemberPos, 0x00000000u},
    {0x8001300Cu, "ae_pad_PadSampleStickHistory", call_ae_pad_PadSampleStickHistory, 0x00000000u},
    {0x800130E0u, "ae_pad_PadNormalizeSticks", call_ae_pad_PadNormalizeSticks, 0x00000000u},
    {0x8001325Cu, "ae_pad_PadUpdate", call_ae_pad_PadUpdate, 0x00000000u},
    {0x8001413Cu, "ae_pad_QuadrantFromSigns", call_ae_pad_QuadrantFromSigns, 0x00000000u},
    {0x80014168u, "ae_PadRumble", call_ae_PadRumble, 0x00000000u},
    {0x80014208u, "ae_PadRumbleReset", call_ae_PadRumbleReset, 0x00000000u},
    {0x800143B0u, "ae_NullInit", call_ae_NullInit, 0x00000000u},
    {0x80014498u, "ae_sdk_square0_lane", call_ae_sdk_square0_lane, 0x00000000u},
    {0x800145D0u, "ae_sdk_ratan2", call_ae_sdk_ratan2, 0x00000000u},
    {0x80014750u, "ae_MatrixRotYMul", call_ae_MatrixRotYMul, 0x00000000u},
    {0x800147D0u, "ae_SvecToDiagMatrix", call_ae_SvecToDiagMatrix, 0x00000000u},
    {0x80014804u, "ae_sdk_MatrixFromDirXZ", call_ae_sdk_MatrixFromDirXZ, 0x00000000u},
    {0x8001484Cu, "ae_sdk_MatrixLookDir", call_ae_sdk_MatrixLookDir, 0x00000000u},
    {0x80014A84u, "ae_MatrixFromFrontUp", call_ae_MatrixFromFrontUp, 0x00000000u},
    {0x80014D70u, "ae_AnimDecodeFrame", call_ae_AnimDecodeFrame, 0x00000000u},
    {0x80014E34u, "ae_ActorAnimDecode", call_ae_ActorAnimDecode, 0x00000000u},
    {0x80014EF0u, "ae_ActorAnimDecodeEntry", call_ae_ActorAnimDecodeEntry, 0x00000000u},
    {0x80014FA0u, "ae_AnimDecodeLerp", call_ae_AnimDecodeLerp, 0x00000000u},
    {0x80015240u, "ae_AnimFramePtr", call_ae_AnimFramePtr, 0x00000000u},
    {0x8001526Cu, "ae_AnimDecodeLerpPtr", call_ae_AnimDecodeLerpPtr, 0x00000000u},
    {0x800154DCu, "ae_CopySvecAt1C", call_ae_CopySvecAt1C, 0x00000000u},
    {0x80015504u, "ae_ModelBuildBoneMatrices", call_ae_ModelBuildBoneMatrices, 0x00000000u},
    {0x80015780u, "ae_ModelPoseBones", call_ae_ModelPoseBones, 0x00000000u},
    {0x80015AC0u, "ae_ModelInit", call_ae_ModelInit, 0x00000000u},
    {0x80015BA0u, "ae_ModelInitAuto", call_ae_ModelInitAuto, 0x00000000u},
    {0x80015D10u, "ae_StaticModelInit", call_ae_StaticModelInit, 0x00000000u},
    {0x80015D58u, "ae_SetColorMatrixMode", call_ae_SetColorMatrixMode, 0x00000000u},
    {0x80015F24u, "ae_MatrixCopy8", call_ae_MatrixCopy8, 0x00000000u},
    {0x80016D60u, "ae_MeshCalcEnvUVs", call_ae_MeshCalcEnvUVs, 0x00000000u},
    {0x80016E68u, "ae_MeshAnimAdvance", call_ae_MeshAnimAdvance, 0x00000000u},
    {0x80017D94u, "ae_ModelDrawWithOt", call_ae_ModelDrawWithOt, 0x00000000u},
    {0x8001D4A8u, "ae_ModelPrepCycleColors", call_ae_ModelPrepCycleColors, 0x00000000u},
    {0x8001F884u, "ae_TrailRingReset", call_ae_TrailRingReset, 0x00000000u},
    {0x8002FB50u, "ae_sdk_rsin", call_ae_sdk_rsin, 0x00000000u},
    {0x8002FC20u, "ae_sdk_rcos", call_ae_sdk_rcos, 0x00000000u},
    {0x8002FCC0u, "ae_sdk_RotMatrixZYX", call_ae_sdk_RotMatrixZYX, 0x00000000u},
    {0x8002FF50u, "ae_sdk_CompMatrix", call_ae_sdk_CompMatrix, 0x00000000u},
    {0x800300B0u, "ae_pad_ApplyMatrixSV", call_ae_pad_ApplyMatrixSV, 0x00000000u},
    {0x800300B0u, "ae_sdk_ApplyMatrixSV", call_ae_sdk_ApplyMatrixSV, 0x00000000u},
    {0x8003019Cu, "ae_VectorNormalS", call_ae_VectorNormalS, 0x00000000u},
    {0x8003019Cu, "ae_sdk_VectorNormalS", call_ae_sdk_VectorNormalS, 0x00000000u},
    {0x800301B0u, "ae_pad_VectorNormal", call_ae_pad_VectorNormal, 0x00000000u},
    {0x800301B0u, "ae_sdk_VectorNormal", call_ae_sdk_VectorNormal, 0x00000000u},
    {0x800301B0u, "ae_player_normalize", call_ae_player_normalize, 0x00000000u},
    {0x800301E0u, "ae_pad_VectorNormalSS", call_ae_pad_VectorNormalSS, 0x00000000u},
    {0x800301E0u, "ae_sdk_VectorNormalSS", call_ae_sdk_VectorNormalSS, 0x00000000u},
    {0x80030210u, "ae_sdk_normalize_core", call_ae_sdk_normalize_core, 0x00000000u},
    {0x800303C0u, "ae_sdk_SquareRoot0", call_ae_sdk_SquareRoot0, 0x00000000u},
    {0x800306F0u, "ae_sdk_RotMatrixZ", call_ae_sdk_RotMatrixZ, 0x00000000u},
    {0x80030890u, "ae_sdk_RotMatrixY", call_ae_sdk_RotMatrixY, 0x00000000u},
    {0x80030A30u, "ae_sdk_RotMatrixX", call_ae_sdk_RotMatrixX, 0x00000000u},
    {0x80030BD0u, "ae_sdk_RotMatrix", call_ae_sdk_RotMatrix, 0x00000000u},
    {0x80030E60u, "ae_sdk_ScaleMatrix", call_ae_sdk_ScaleMatrix, 0x00000000u},
    {0x80030FA0u, "ae_sdk_RotMatrixYXZ", call_ae_sdk_RotMatrixYXZ, 0x00000000u},
    {0x80031B60u, "ae_AreaLoad", call_ae_AreaLoad, 0x00000000u},
    {0x800323F8u, "ae_ApplyLevelFog", call_ae_ApplyLevelFog, 0x00000000u},
    {0x8003244Cu, "ae_AreaLoadGridAndProps", call_ae_AreaLoadGridAndProps, 0x00000000u},
    {0x80032708u, "ae_ClearWords", call_ae_ClearWords, 0x00000000u},
    {0x80032ADCu, "ae_PlayerIdleModelSetup", call_ae_PlayerIdleModelSetup, 0x00000000u},
    {0x80032BBCu, "ae_PlayerSpawnAtEntry", call_ae_PlayerSpawnAtEntry, 0x00000000u},
    {0x80032E30u, "ae_MonkeyRand", call_ae_MonkeyRand, 0x00000000u},
    {0x80032E58u, "ae_MonkeyCaughtGroupValue", call_ae_MonkeyCaughtGroupValue, 0x00000000u},
    {0x80032E84u, "ae_MonkeyGetAnimIndex", call_ae_MonkeyGetAnimIndex, 0x00000000u},
    {0x80032F24u, "ae_MonkeyRandomHesitate", call_ae_MonkeyRandomHesitate, 0x00000000u},
    {0x80033000u, "ae_MonkeyNop", call_ae_MonkeyNop, 0x00000000u},
    {0x80033008u, "ae_MonkeyBodyRegister", call_ae_MonkeyBodyRegister, 0x00000000u},
    {0x80033068u, "ae_MonkeyBodyUnregister", call_ae_MonkeyBodyUnregister, 0x00000000u},
    {0x800330C8u, "ae_MonkeyAdvanceAnimFrame", call_ae_MonkeyAdvanceAnimFrame, 0x00000000u},
    {0x8003317Cu, "ae_MonkeyActivityReset", call_ae_MonkeyActivityReset, 0x00000000u},
    {0x800331B0u, "ae_MonkeyPickAnim", call_ae_MonkeyPickAnim, 0x00000000u},
    {0x80033220u, "ae_MonkeySetState", call_ae_MonkeySetState, 0x00000000u},
    {0x800332F8u, "ae_MonkeyRestoreActivities", call_ae_MonkeyRestoreActivities, 0x00000000u},
    {0x800334C8u, "ae_MonkeyStoreActivities", call_ae_MonkeyStoreActivities, 0x00000000u},
    {0x800335D0u, "ae_GimmickAllowsMonkeyPassage", call_ae_GimmickAllowsMonkeyPassage, 0x00000000u},
    {0x80033708u, "ae_MonkeyVoiceQueueReset", call_ae_MonkeyVoiceQueueReset, 0x00000000u},
    {0x80033744u, "ae_MonkeyQueueVoice", call_ae_MonkeyQueueVoice, 0x00000000u},
    {0x800337C4u, "ae_DelayedSfxUpdate", call_ae_DelayedSfxUpdate, 0x00000000u},
    {0x8003386Cu, "ae_NavNodeGetLink", call_ae_NavNodeGetLink, 0x00000000u},
    {0x8003398Cu, "ae_AreaIsSpecialMonkeyArea", call_ae_AreaIsSpecialMonkeyArea, 0x00000000u},
    {0x800339CCu, "ae_MonkeyIsState1E_to_20", call_ae_MonkeyIsState1E_to_20, 0x00000000u},
    {0x800339ECu, "ae_AreaIsBossArena", call_ae_AreaIsBossArena, 0x00000000u},
    {0x80033A3Cu, "ae_MonkeyAnimFinished", call_ae_MonkeyAnimFinished, 0x00000000u},
    {0x80033ACCu, "ae_MonkeyAnimAtStart", call_ae_MonkeyAnimAtStart, 0x00000000u},
    {0x80033B10u, "ae_MonkeyIsIdleActivityState", call_ae_MonkeyIsIdleActivityState, 0x00000000u},
    {0x80033B9Cu, "ae_MonkeyIsIdleEndState", call_ae_MonkeyIsIdleEndState, 0x00000000u},
    {0x80033BE0u, "ae_MonkeyIsAltAnimState", call_ae_MonkeyIsAltAnimState, 0x00000000u},
    {0x80033C00u, "ae_MonkeyIsState0C_or_24", call_ae_MonkeyIsState0C_or_24, 0x00000000u},
    {0x80033C34u, "ae_MonkeyIsJumpArcState", call_ae_MonkeyIsJumpArcState, 0x00000000u},
    {0x80033C80u, "ae_MonkeyIsAttackState", call_ae_MonkeyIsAttackState, 0x00000000u},
    {0x80033CCCu, "ae_MonkeySpawnProjectile", call_ae_MonkeySpawnProjectile, 0x00000000u},
    {0x80033DF0u, "ae_MonkeyFireProjectile6", call_ae_MonkeyFireProjectile6, 0x00000000u},
    {0x80033E4Cu, "ae_MonkeyAimAtTarget", call_ae_MonkeyAimAtTarget, 0x00000000u},
    {0x8003429Cu, "ae_MonkeyFireProjectileAimed", call_ae_MonkeyFireProjectileAimed, 0x00000000u},
    {0x800343B0u, "ae_MonkeyFireProjectile7", call_ae_MonkeyFireProjectile7, 0x00000000u},
    {0x8003440Cu, "ae_MonkeyFireProjectile8", call_ae_MonkeyFireProjectile8, 0x00000000u},
    {0x800344C8u, "ae_AngleBetweenXZ", call_ae_AngleBetweenXZ, 0x00000000u},
    {0x8003455Cu, "ae_MonkeyAngleToTarget", call_ae_MonkeyAngleToTarget, 0x00000000u},
    {0x800345D4u, "ae_DistanceIntToShort3D", call_ae_DistanceIntToShort3D, 0x00000000u},
    {0x80034684u, "ae_Distance3D", call_ae_Distance3D, 0x00000000u},
    {0x8003473Cu, "ae_DistanceXZ", call_ae_DistanceXZ, 0x00000000u},
    {0x800347D8u, "ae_MonkeyCalcMoveDir", call_ae_MonkeyCalcMoveDir, 0x00000000u},
    {0x80034834u, "ae_MonkeyApplyMoveDir", call_ae_MonkeyApplyMoveDir, 0x00000000u},
    {0x800348CCu, "ae_DotXZ_SV", call_ae_DotXZ_SV, 0x00000000u},
    {0x80034940u, "ae_DotXZ_V", call_ae_DotXZ_V, 0x00000000u},
    {0x800349B4u, "ae_Atan2Angle4096", call_ae_Atan2Angle4096, 0x00000000u},
    {0x800349F4u, "ae_NavNodeIsSpecial", call_ae_NavNodeIsSpecial, 0x00000000u},
    {0x80034B74u, "ae_NavNodeGetLinkIds", call_ae_NavNodeGetLinkIds, 0x00000000u},
    {0x80034C08u, "ae_NavNodeGetLinkIdsEx", call_ae_NavNodeGetLinkIdsEx, 0x00000000u},
    {0x80034D30u, "ae_ObjGetBonePos", call_ae_ObjGetBonePos, 0x00000000u},
    {0x80034EF8u, "ae_NavNodeGetPos", call_ae_NavNodeGetPos, 0x00000000u},
    {0x80035080u, "ae_MonkeySetNavNode", call_ae_MonkeySetNavNode, 0x00000000u},
    {0x8003516Cu, "ae_MonkeyReachedNode", call_ae_MonkeyReachedNode, 0x00000000u},
    {0x80035370u, "ae_MonkeyUpdateLinkSlope", call_ae_MonkeyUpdateLinkSlope, 0x00000000u},
    {0x80035474u, "ae_MonkeyUpdateGoalPos", call_ae_MonkeyUpdateGoalPos, 0x00000000u},
    {0x800355DCu, "ae_MonkeyUpdateGoal", call_ae_MonkeyUpdateGoal, 0x00000000u},
    {0x80035604u, "ae_MonkeyNavBacktracked", call_ae_MonkeyNavBacktracked, 0x00000000u},
    {0x800356C0u, "ae_MonkeyGetCallVoice", call_ae_MonkeyGetCallVoice, 0x00000000u},
    {0x80035750u, "ae_MonkeyGetCallVoice2", call_ae_MonkeyGetCallVoice2, 0x00000000u},
    {0x800357E0u, "ae_MonkeyStartNotice", call_ae_MonkeyStartNotice, 0x00000000u},
    {0x8003581Cu, "ae_MonkeyEnterState_1E", call_ae_MonkeyEnterState_1E, 0x00000000u},
    {0x80035918u, "ae_MonkeyEnterState_32", call_ae_MonkeyEnterState_32, 0x00000000u},
    {0x800359B4u, "ae_MonkeyEnterState_34", call_ae_MonkeyEnterState_34, 0x00000000u},
    {0x800359F0u, "ae_MonkeyEnterState_1D", call_ae_MonkeyEnterState_1D, 0x00000000u},
    {0x80035A7Cu, "ae_MonkeyEnterState_04", call_ae_MonkeyEnterState_04, 0x00000000u},
    {0x80035BD0u, "ae_MonkeyEnterKnockdown", call_ae_MonkeyEnterKnockdown, 0x00000000u},
    {0x80035D18u, "ae_MonkeyEnterState2F", call_ae_MonkeyEnterState2F, 0x00000000u},
    {0x80035DD8u, "ae_MonkeyEnterState_30", call_ae_MonkeyEnterState_30, 0x00000000u},
    {0x80035E6Cu, "ae_MonkeyEnterState_36", call_ae_MonkeyEnterState_36, 0x00000000u},
    {0x80035EA4u, "ae_MonkeyEnterState_20", call_ae_MonkeyEnterState_20, 0x00000000u},
    {0x80035F1Cu, "ae_MonkeyEnterState_24", call_ae_MonkeyEnterState_24, 0x00000000u},
    {0x80035F54u, "ae_MonkeyEnterState_0C", call_ae_MonkeyEnterState_0C, 0x00000000u},
    {0x80035F8Cu, "ae_MonkeyEnterState_35", call_ae_MonkeyEnterState_35, 0x00000000u},
    {0x80035FBCu, "ae_MonkeyEnterState_1A", call_ae_MonkeyEnterState_1A, 0x00000000u},
    {0x8003601Cu, "ae_MonkeyEnterState_09", call_ae_MonkeyEnterState_09, 0x00000000u},
    {0x800360D8u, "ae_MonkeyEnterState_3D", call_ae_MonkeyEnterState_3D, 0x00000000u},
    {0x8003612Cu, "ae_MonkeyEnterState_3E", call_ae_MonkeyEnterState_3E, 0x00000000u},
    {0x80036174u, "ae_MonkeyEnterState_3F", call_ae_MonkeyEnterState_3F, 0x00000000u},
    {0x800361B4u, "ae_MonkeyTargetInSightRange", call_ae_MonkeyTargetInSightRange, 0x00000000u},
    {0x80036300u, "ae_MonkeyEnterIdleWait", call_ae_MonkeyEnterIdleWait, 0x00000000u},
    {0x800363A4u, "ae_MonkeyResumeIdle", call_ae_MonkeyResumeIdle, 0x00000000u},
    {0x800363E4u, "ae_MonkeyEnterRandomLook", call_ae_MonkeyEnterRandomLook, 0x00000000u},
    {0x8003641Cu, "ae_MonkeyEnterState_17", call_ae_MonkeyEnterState_17, 0x00000000u},
    {0x80036460u, "ae_MonkeyEnterState_2A", call_ae_MonkeyEnterState_2A, 0x00000000u},
    {0x800364BCu, "ae_MonkeyPrepareJumpArc", call_ae_MonkeyPrepareJumpArc, 0x00000000u},
    {0x80036964u, "ae_MonkeyEnterState_2C", call_ae_MonkeyEnterState_2C, 0x00000000u},
    {0x80036B24u, "ae_MonkeyBodyOff", call_ae_MonkeyBodyOff, 0x00000000u},
    {0x80036B78u, "ae_MonkeyBodyOn", call_ae_MonkeyBodyOn, 0x00000000u},
    {0x80036BD0u, "ae_MonkeyFacingFromLinkCode", call_ae_MonkeyFacingFromLinkCode, 0x00000000u},
    {0x80036C84u, "ae_MonkeyDefeatBurst", call_ae_MonkeyDefeatBurst, 0x00000000u},
    {0x80036CD0u, "ae_MonkeyNodeActionDispatch", call_ae_MonkeyNodeActionDispatch, 0x00000000u},
    {0x80037068u, "ae_MonkeyAttackChance", call_ae_MonkeyAttackChance, 0x00000000u},
    {0x8003710Cu, "ae_MonkeyTryAttackKind", call_ae_MonkeyTryAttackKind, 0x00000000u},
    {0x8003738Cu, "ae_MonkeyPickAttackA", call_ae_MonkeyPickAttackA, 0x00000000u},
    {0x80037420u, "ae_MonkeyPickAttackB", call_ae_MonkeyPickAttackB, 0x00000000u},
    {0x800374B8u, "ae_MonkeyPickAttackC", call_ae_MonkeyPickAttackC, 0x00000000u},
    {0x80037550u, "ae_MonkeyAlertedDecide", call_ae_MonkeyAlertedDecide, 0x00000000u},
    {0x800378C0u, "ae_MonkeyDecideAction", call_ae_MonkeyDecideAction, 0x00000000u},
    {0x80037908u, "ae_MonkeyPatrolDecide", call_ae_MonkeyPatrolDecide, 0x00000000u},
    {0x80037D6Cu, "ae_MonkeyApproachAngleNear", call_ae_MonkeyApproachAngleNear, 0x00000000u},
    {0x80037DECu, "ae_MonkeyApproachAngleFar", call_ae_MonkeyApproachAngleFar, 0x00000000u},
    {0x80037E8Cu, "ae_MonkeyNavLinkAllowed", call_ae_MonkeyNavLinkAllowed, 0x00000000u},
    {0x80038054u, "ae_MonkeyMarkBlockedLinks", call_ae_MonkeyMarkBlockedLinks, 0x00000000u},
    {0x80038120u, "ae_MonkeyPickBestLink", call_ae_MonkeyPickBestLink, 0x00000000u},
    {0x80038608u, "ae_NavNodeLinkFlag", call_ae_NavNodeLinkFlag, 0x00000000u},
    {0x80038694u, "ae_MonkeySwapNavNodes", call_ae_MonkeySwapNavNodes, 0x00000000u},
    {0x800386C8u, "ae_MonkeyConsiderTurnBack", call_ae_MonkeyConsiderTurnBack, 0x00000000u},
    {0x80038914u, "ae_MonkeyConsiderTurnBackRandom", call_ae_MonkeyConsiderTurnBackRandom, 0x00000000u},
    {0x80038A1Cu, "ae_MonkeyChooseNextNodeA", call_ae_MonkeyChooseNextNodeA, 0x00000000u},
    {0x80038A9Cu, "ae_MonkeyChooseNextNodeB", call_ae_MonkeyChooseNextNodeB, 0x00000000u},
    {0x80038F00u, "ae_MonkeyFootDust", call_ae_MonkeyFootDust, 0x00000000u},
    {0x80038FD8u, "ae_MonkeyDebrisBurst6", call_ae_MonkeyDebrisBurst6, 0x00000000u},
    {0x800390F8u, "ae_MonkeyDebrisBurst1", call_ae_MonkeyDebrisBurst1, 0x00000000u},
    {0x80039200u, "ae_MonkeyPuff14", call_ae_MonkeyPuff14, 0x00000000u},
    {0x80039248u, "ae_MonkeyAlertPuff", call_ae_MonkeyAlertPuff, 0x00000000u},
    {0x8003929Cu, "ae_MonkeyPuff5", call_ae_MonkeyPuff5, 0x00000000u},
    {0x800392E4u, "ae_BodyContactType", call_ae_BodyContactType, 0x00000000u},
    {0x800392F0u, "ae_MonkeyUpdateContact", call_ae_MonkeyUpdateContact, 0x00000000u},
    {0x800393E4u, "ae_MonkeyProjectToScreen", call_ae_MonkeyProjectToScreen, 0x00000000u},
    {0x8003958Cu, "ae_MonkeySpawnAll", call_ae_MonkeySpawnAll, 0x00000000u},
    {0x80039B30u, "ae_MonkeyUpdateTargetDistance", call_ae_MonkeyUpdateTargetDistance, 0x00000000u},
    {0x80039B90u, "ae_MonkeyAlertIconKind", call_ae_MonkeyAlertIconKind, 0x00000000u},
    {0x80039C38u, "ae_MonkeyDrawAlertIcon", call_ae_MonkeyDrawAlertIcon, 0x00000000u},
    {0x80039E50u, "ae_StaticModelDraw", call_ae_StaticModelDraw, 0x00000000u},
    {0x80039ED4u, "ae_MonkeyDrawModel", call_ae_MonkeyDrawModel, 0x00000000u},
    {0x8003A334u, "ae_MonkeyStub_ReturnZero", call_ae_MonkeyStub_ReturnZero, 0x00000000u},
    {0x8003A33Cu, "ae_MonkeySelectLod", call_ae_MonkeySelectLod, 0x00000000u},
    {0x8003A414u, "ae_MonkeyInit", call_ae_MonkeyInit, 0x00000000u},
    {0x8003AC54u, "ae_MonkeyScoreLinksByTarget", call_ae_MonkeyScoreLinksByTarget, 0x00000000u},
    {0x8003AD9Cu, "ae_MonkeyPickGoalNode", call_ae_MonkeyPickGoalNode, 0x00000000u},
    {0x8003B074u, "ae_MonkeySelectNextNode", call_ae_MonkeySelectNextNode, 0x00000000u},
    {0x8003B534u, "ae_MonkeyNodeTargetCosines", call_ae_MonkeyNodeTargetCosines, 0x00000000u},
    {0x8003B674u, "ae_MonkeyLoadLinkExtra", call_ae_MonkeyLoadLinkExtra, 0x00000000u},
    {0x8003B880u, "ae_NavLinkLookupExtra", call_ae_NavLinkLookupExtra, 0x00000000u},
    {0x8003B9C0u, "ae_MonkeyMoveStep", call_ae_MonkeyMoveStep, 0x00000000u},
    {0x8003C204u, "ae_MonkeyUpdateFacing", call_ae_MonkeyUpdateFacing, 0x00000000u},
    {0x8003C568u, "ae_MonkeyInterpolateLinkVector", call_ae_MonkeyInterpolateLinkVector, 0x00000000u},
    {0x8003C6D4u, "ae_MonkeyAi_MoveAlerted", call_ae_MonkeyAi_MoveAlerted, 0x00000000u},
    {0x8003CC3Cu, "ae_MonkeyTurnTowardTarget", call_ae_MonkeyTurnTowardTarget, 0x00000000u},
    {0x8003CD24u, "ae_MonkeyFireProjectileByKind", call_ae_MonkeyFireProjectileByKind, 0x00000000u},
    {0x8003CD84u, "ae_MonkeyAi_State1E_20", call_ae_MonkeyAi_State1E_20, 0x00000000u},
    {0x8003D9D4u, "ae_MonkeyAi_MoveUnalerted", call_ae_MonkeyAi_MoveUnalerted, 0x00000000u},
    {0x8003DC7Cu, "ae_MonkeyAi_Patrol", call_ae_MonkeyAi_Patrol, 0x00000000u},
    {0x8003F18Cu, "ae_MonkeyAi_IdleActivityStart", call_ae_MonkeyAi_IdleActivityStart, 0x00000000u},
    {0x8003F308u, "ae_MonkeyAi_IdleActivityLoop", call_ae_MonkeyAi_IdleActivityLoop, 0x00000000u},
    {0x8003F4B4u, "ae_MonkeyAi_IdleActivityEnd", call_ae_MonkeyAi_IdleActivityEnd, 0x00000000u},
    {0x8003F530u, "ae_MonkeyAi_JumpArc", call_ae_MonkeyAi_JumpArc, 0x00000000u},
    {0x8003F86Cu, "ae_MonkeyAi_JumpArcPhased", call_ae_MonkeyAi_JumpArcPhased, 0x00000000u},
    {0x8003FB44u, "ae_MonkeyAi_State3D", call_ae_MonkeyAi_State3D, 0x00000000u},
    {0x8003FC08u, "ae_MonkeyAi_State3E", call_ae_MonkeyAi_State3E, 0x00000000u},
    {0x8003FCD8u, "ae_MonkeyAi_Fall", call_ae_MonkeyAi_Fall, 0x00000000u},
    {0x8003FD7Cu, "ae_MonkeyAi_JumpArcB", call_ae_MonkeyAi_JumpArcB, 0x00000000u},
    {0x8004002Cu, "ae_MonkeyAi_GoToFlaggedNode", call_ae_MonkeyAi_GoToFlaggedNode, 0x00000000u},
    {0x80040170u, "ae_MonkeyAi_State07", call_ae_MonkeyAi_State07, 0x00000000u},
    {0x800401F4u, "ae_MonkeyLineOfSight", call_ae_MonkeyLineOfSight, 0x00000000u},
    {0x800405C4u, "ae_MonkeyUpdateSight", call_ae_MonkeyUpdateSight, 0x00000000u},
    {0x800407F0u, "ae_MonkeyResetNavVisits", call_ae_MonkeyResetNavVisits, 0x00000000u},
    {0x8004086Cu, "ae_MonkeyNavVisitUpdate", call_ae_MonkeyNavVisitUpdate, 0x00000000u},
    {0x80040BDCu, "ae_NavGroundHeightBetween", call_ae_NavGroundHeightBetween, 0x00000000u},
    {0x80040CFCu, "ae_MonkeyGetWaypointPos", call_ae_MonkeyGetWaypointPos, 0x00000000u},
    {0x80040D90u, "ae_MonkeyArriveAtNode", call_ae_MonkeyArriveAtNode, 0x00000000u},
    {0x80041280u, "ae_ActorContactOwnerType", call_ae_ActorContactOwnerType, 0x00000000u},
    {0x800412B0u, "ae_MonkeyAiStateUpdate", call_ae_MonkeyAiStateUpdate, 0x00000000u},
    {0x800424D4u, "ae_MonkeyThinkActive", call_ae_MonkeyThinkActive, 0x00000000u},
    {0x800426C4u, "ae_MonkeySelectActive", call_ae_MonkeySelectActive, 0x00000000u},
    {0x80042B68u, "ae_MonkeyUpdateVisuals", call_ae_MonkeyUpdateVisuals, 0x00000000u},
    {0x80042E18u, "ae_MonkeyUpdateAll", call_ae_MonkeyUpdateAll, 0x00000000u},
    {0x80043060u, "ae_MonkeyDebugMenuInit", call_ae_MonkeyDebugMenuInit, 0x00000000u},
    {0x800430F0u, "ae_MonkeyDebugMenuInit2", call_ae_MonkeyDebugMenuInit2, 0x00000000u},
    {0x80043380u, "ae_AreaSetZoneBoxes", call_ae_AreaSetZoneBoxes, 0x00000000u},
    {0x800433B0u, "ae_GetGroundHeightEx", call_ae_GetGroundHeightEx, 0x00000000u},
    {0x80043400u, "ae_GetGroundHeight", call_ae_GetGroundHeight, 0x00000000u},
    {0x800438F8u, "ae_BuildTerrainBlockMask", call_ae_BuildTerrainBlockMask, 0x00000000u},
    {0x8004626Cu, "ae_GetZoneMask", call_ae_GetZoneMask, 0x00000000u},
    {0x80046340u, "ae_Atan2Safe", call_ae_Atan2Safe, 0x00000000u},
    {0x8004637Cu, "ae_sdk_MatrixIdentity", call_ae_sdk_MatrixIdentity, 0x00000000u},
    {0x800463A4u, "ae_StepToward", call_ae_StepToward, 0x00000000u},
    {0x80046584u, "ae_IntSqrt", call_ae_IntSqrt, 0x00000000u},
    {0x80046618u, "ae_player_vec_length3", call_ae_player_vec_length3, 0x00000000u},
    {0x80046670u, "ae_pad_CameraRotMatrix", call_ae_pad_CameraRotMatrix, 0x00000000u},
    {0x800468ECu, "ae_RadarUsable", call_ae_RadarUsable, 0x00000000u},
    {0x80046970u, "ae_CameraKindFixed_Nop", call_ae_CameraKindFixed_Nop, 0x00000000u},
    {0x80046A78u, "ae_ScreenFadeUpdate", call_ae_ScreenFadeUpdate, 0x00000000u},
    {0x80046ACCu, "ae_HudDrawIconSprite", call_ae_HudDrawIconSprite, 0x00000000u},
    {0x80046BC4u, "ae_HudDrawGadgetIcons", call_ae_HudDrawGadgetIcons, 0x00000000u},
    {0x80046D80u, "ae_SetProjection", call_ae_SetProjection, 0x00000000u},
    {0x80046DACu, "ae_CameraActivateSlot", call_ae_CameraActivateSlot, 0x00000000u},
    {0x80046DE8u, "ae_CameraSelectMode", call_ae_CameraSelectMode, 0x00000000u},
    {0x800470CCu, "ae_CameraGetEye", call_ae_CameraGetEye, 0x00000000u},
    {0x80047104u, "ae_CameraSlotInitFollow", call_ae_CameraSlotInitFollow, 0x00000000u},
    {0x800471E4u, "ae_CameraSlotCreate", call_ae_CameraSlotCreate, 0x00000000u},
    {0x80047290u, "ae_CameraSlotRemove", call_ae_CameraSlotRemove, 0x00000000u},
    {0x80047360u, "ae_CameraSlotAimAtFocus", call_ae_CameraSlotAimAtFocus, 0x00000000u},
    {0x80047464u, "ae_CameraSlotSmoothEye", call_ae_CameraSlotSmoothEye, 0x00000000u},
    {0x80047558u, "ae_CameraSlotSmoothTarget", call_ae_CameraSlotSmoothTarget, 0x00000000u},
    {0x80047654u, "ae_CameraKindAim_SnapToFocus", call_ae_CameraKindAim_SnapToFocus, 0x00000000u},
    {0x80047690u, "ae_CameraNudge", call_ae_CameraNudge, 0x00000000u},
    {0x80047754u, "ae_CameraResetAll", call_ae_CameraResetAll, 0x00000000u},
    {0x80047BD0u, "ae_CameraUpdate", call_ae_CameraUpdate, 0x00000000u},
    {0x80047DA4u, "ae_CameraSnapshotPlayer", call_ae_CameraSnapshotPlayer, 0x00000000u},
    {0x80047FF0u, "ae_CameraApplySlot", call_ae_CameraApplySlot, 0x00000000u},
    {0x800482C0u, "ae_CameraFollowDispatch", call_ae_CameraFollowDispatch, 0x00000000u},
    {0x80048430u, "ae_AimRequestClear", call_ae_AimRequestClear, 0x00000000u},
    {0x80048454u, "ae_CameraKindAimUpdate", call_ae_CameraKindAimUpdate, 0x00000000u},
    {0x80048748u, "ae_CameraFixedModeOff", call_ae_CameraFixedModeOff, 0x00000000u},
    {0x8004875Cu, "ae_CameraKindFixedUpdate", call_ae_CameraKindFixedUpdate, 0x00000000u},
    {0x800489ECu, "ae_CameraShakeReset", call_ae_CameraShakeReset, 0x00000000u},
    {0x80048A00u, "ae_CameraShake", call_ae_CameraShake, 0x00000000u},
    {0x80048BBCu, "ae_CameraShakeApply", call_ae_CameraShakeApply, 0x00000000u},
    {0x80048D10u, "ae_CameraSlotPlaceBehindPlayer", call_ae_CameraSlotPlaceBehindPlayer, 0x00000000u},
    {0x80048DD0u, "ae_CameraSlotUpdateYaw", call_ae_CameraSlotUpdateYaw, 0x00000000u},
    {0x80048F6Cu, "ae_CameraSlotGroundClamp", call_ae_CameraSlotGroundClamp, 0x00000000u},
    {0x8004906Cu, "ae_CameraSlotGroundClampNear", call_ae_CameraSlotGroundClampNear, 0x00000000u},
    {0x8004923Cu, "ae_CameraSlotStickOrbit", call_ae_CameraSlotStickOrbit, 0x00000000u},
    {0x800493D4u, "ae_CameraSlotResetFollow", call_ae_CameraSlotResetFollow, 0x00000000u},
    {0x80049514u, "ae_CameraSlotSnapshotEye", call_ae_CameraSlotSnapshotEye, 0x00000000u},
    {0x800495F4u, "ae_CameraFollowUpdate", call_ae_CameraFollowUpdate, 0x00000000u},
    {0x8004A940u, "ae_IsFlightArea", call_ae_IsFlightArea, 0x00000000u},
    {0x8004A978u, "ae_CameraRcCarActive", call_ae_CameraRcCarActive, 0x00000000u},
    {0x8004AA24u, "ae_CameraSlotFlagBit81", call_ae_CameraSlotFlagBit81, 0x00000000u},
    {0x8004AC84u, "ae_CameraFollowAimedVariant", call_ae_CameraFollowAimedVariant, 0x00000000u},
    {0x8004AF78u, "ae_CameraFollowGadgetVariant", call_ae_CameraFollowGadgetVariant, 0x00000000u},
    {0x8004B4B8u, "ae_AimModeEnter", call_ae_AimModeEnter, 0x00000000u},
    {0x8004B510u, "ae_AimModeLeave", call_ae_AimModeLeave, 0x00000000u},
    {0x8004B5FCu, "ae_CameraKind3FollowUpdate", call_ae_CameraKind3FollowUpdate, 0x00000000u},
    {0x8004B880u, "ae_CameraGetPlayerDir", call_ae_CameraGetPlayerDir, 0x00000000u},
    {0x8004B8F4u, "ae_CameraSlotGetFocusPoint", call_ae_CameraSlotGetFocusPoint, 0x00000000u},
    {0x8004BA0Cu, "ae_EventCamTrigger", call_ae_EventCamTrigger, 0x00000000u},
    {0x8004BAD8u, "ae_EventCamSetScript", call_ae_EventCamSetScript, 0x00000000u},
    {0x8004BB68u, "ae_EventCamCreate", call_ae_EventCamCreate, 0x00000000u},
    {0x8004BC9Cu, "ae_EventCamStartRandom", call_ae_EventCamStartRandom, 0x00000000u},
    {0x8004BD98u, "ae_CameraRegionTest", call_ae_CameraRegionTest, 0x00000000u},
    {0x8004BF20u, "ae_CameraRegionUpdate", call_ae_CameraRegionUpdate, 0x00000000u},
    {0x8004C2E4u, "ae_CameraSlotClearScript", call_ae_CameraSlotClearScript, 0x00000000u},
    {0x8004C318u, "ae_CameraScriptRun", call_ae_CameraScriptRun, 0x00000000u},
    {0x8004CA70u, "ae_CameraSlotUpdate", call_ae_CameraSlotUpdate, 0x00000000u},
    {0x8004D0F0u, "ae_StageExitFlagsClear", call_ae_StageExitFlagsClear, 0x00000000u},
    {0x8004D104u, "ae_StageExitPending", call_ae_StageExitPending, 0x00000000u},
    {0x8004D134u, "ae_EventCamLoadRecord", call_ae_EventCamLoadRecord, 0x00000000u},
    {0x8004D39Cu, "ae_EventCamStart", call_ae_EventCamStart, 0x00000000u},
    {0x8004D40Cu, "ae_StageExitSequenceUpdate", call_ae_StageExitSequenceUpdate, 0x00000000u},
    {0x8004D6F4u, "ae_StageExitRequest", call_ae_StageExitRequest, 0x00000000u},
    {0x8004D7A0u, "ae_EventCamUpdate", call_ae_EventCamUpdate, 0x00000000u},
    {0x8004D8C0u, "ae_GetSolidHeight", call_ae_GetSolidHeight, 0x00000000u},
    {0x8004DF38u, "ae_ColliderHeightAt", call_ae_ColliderHeightAt, 0x00000000u},
    {0x8004E33Cu, "ae_CollShape_Box", call_ae_CollShape_Box, 0x00000000u},
    {0x8004E444u, "ae_CollShape_WallThin", call_ae_CollShape_WallThin, 0x00000000u},
    {0x8004E63Cu, "ae_CollShape_WallEdge", call_ae_CollShape_WallEdge, 0x00000000u},
    {0x8004E8F8u, "ae_CollShape_WallDiagonal", call_ae_CollShape_WallDiagonal, 0x00000000u},
    {0x8004EA1Cu, "ae_CollShape_Ramp45", call_ae_CollShape_Ramp45, 0x00000000u},
    {0x8004EC3Cu, "ae_CollShape_Cylinder", call_ae_CollShape_Cylinder, 0x00000000u},
    {0x8004ED30u, "ae_CollShape_Ramp2", call_ae_CollShape_Ramp2, 0x00000000u},
    {0x8004EE94u, "ae_CollShape_Ramp4", call_ae_CollShape_Ramp4, 0x00000000u},
    {0x8004F000u, "ae_CollShape_Wedge", call_ae_CollShape_Wedge, 0x00000000u},
    {0x8004F330u, "ae_CollShape_DiagonalBox", call_ae_CollShape_DiagonalBox, 0x00000000u},
    {0x8004F438u, "ae_ColliderRingHeight", call_ae_ColliderRingHeight, 0x00000000u},
    {0x8004F514u, "ae_ColliderStripHeightOut", call_ae_ColliderStripHeightOut, 0x00000000u},
    {0x8004F548u, "ae_ColliderStripHeight", call_ae_ColliderStripHeight, 0x00000000u},
    {0x8004F6E8u, "ae_CollisionPointInGrid", call_ae_CollisionPointInGrid, 0x00000000u},
    {0x8004F884u, "ae_ColliderSlopeContactDir", call_ae_ColliderSlopeContactDir, 0x00000000u},
    {0x8004F984u, "ae_ColliderSlopeHeight", call_ae_ColliderSlopeHeight, 0x00000000u},
    {0x8004FC8Cu, "ae_ColliderSlopeSide", call_ae_ColliderSlopeSide, 0x00000000u},
    {0x8004FE48u, "ae_ColliderSlopeCorner", call_ae_ColliderSlopeCorner, 0x00000000u},
    {0x8004FF20u, "ae_ColliderHalfSpaceInside", call_ae_ColliderHalfSpaceInside, 0x00000000u},
    {0x8004FFC8u, "ae_CollHalfPipeHeight", call_ae_CollHalfPipeHeight, 0x00000000u},
    {0x800500D8u, "ae_ColliderTrenchHeight", call_ae_ColliderTrenchHeight, 0x00000000u},
    {0x80050490u, "ae_HalfPipeRangeTest", call_ae_HalfPipeRangeTest, 0x00000000u},
    {0x80050580u, "ae_ZakoTypeIsIgnored", call_ae_ZakoTypeIsIgnored, 0x00000000u},
    {0x800505C8u, "ae_ZakoInit", call_ae_ZakoInit, 0x00000000u},
    {0x800508B8u, "ae_ZakoSpawnFromEntry", call_ae_ZakoSpawnFromEntry, 0x00000000u},
    {0x800509A4u, "ae_ZakoClearSlotTable", call_ae_ZakoClearSlotTable, 0x00000000u},
    {0x80050AFCu, "ae_StageProgressRestore_Nop", call_ae_StageProgressRestore_Nop, 0x00000000u},
    {0x80050B04u, "ae_ZakoSpawnNearby", call_ae_ZakoSpawnNearby, 0x00000000u},
    {0x80050C34u, "ae_ZakoUpdateAll", call_ae_ZakoUpdateAll, 0x00000000u},
    {0x80050CB0u, "ae_ZakoAllocSlot", call_ae_ZakoAllocSlot, 0x00000000u},
    {0x80050D04u, "ae_ZakoSetupType", call_ae_ZakoSetupType, 0x00000000u},
    {0x800511A4u, "ae_ZakoUpdateLoop", call_ae_ZakoUpdateLoop, 0x00000000u},
    {0x80051410u, "ae_PointOffscreen", call_ae_PointOffscreen, 0x00000000u},
    {0x80051534u, "ae_ZakoCullAndDraw", call_ae_ZakoCullAndDraw, 0x00000000u},
    {0x80051620u, "ae_ZakoGetBonePoint", call_ae_ZakoGetBonePoint, 0x00000000u},
    {0x800516D8u, "ae_ZakoUpdateLinkPoints", call_ae_ZakoUpdateLinkPoints, 0x00000000u},
    {0x80051740u, "ae_ZakoUpdateColliderPos", call_ae_ZakoUpdateColliderPos, 0x00000000u},
    {0x800518ACu, "ae_ZakoBuildMatrix", call_ae_ZakoBuildMatrix, 0x00000000u},
    {0x80051A28u, "ae_ZakoPoseHidden", call_ae_ZakoPoseHidden, 0x00000000u},
    {0x80051AA4u, "ae_ModelLightColorsToScratch", call_ae_ModelLightColorsToScratch, 0x00000000u},
    {0x80051C00u, "ae_ZakoDraw", call_ae_ZakoDraw, 0x00000000u},
    {0x80051DCCu, "ae_ObjReenableBody", call_ae_ObjReenableBody, 0x00000000u},
    {0x80051E34u, "ae_ObjUnregisterBody", call_ae_ObjUnregisterBody, 0x00000000u},
    {0x80051E98u, "ae_ObjKill", call_ae_ObjKill, 0x00000000u},
    {0x80051F1Cu, "ae_ZakoDefeat", call_ae_ZakoDefeat, 0x00000000u},
    {0x80051FE8u, "ae_ZakoFindEntryByType", call_ae_ZakoFindEntryByType, 0x00000000u},
    {0x80052038u, "ae_ObjInitModel", call_ae_ObjInitModel, 0x00000000u},
    {0x800520A8u, "ae_ZakoGetAnimTable", call_ae_ZakoGetAnimTable, 0x00000000u},
    {0x80052134u, "ae_ZakoSetAnimFromEntry", call_ae_ZakoSetAnimFromEntry, 0x00000000u},
    {0x8005216Cu, "ae_ZakoDropPickups", call_ae_ZakoDropPickups, 0x00000000u},
    {0x800522A8u, "ae_ZakoSetKnockback", call_ae_ZakoSetKnockback, 0x00000000u},
    {0x8005240Cu, "ae_ZakoApplyHit", call_ae_ZakoApplyHit, 0x00000000u},
    {0x800525A0u, "ae_ObjApplyDamage", call_ae_ObjApplyDamage, 0x00000000u},
    {0x800525E0u, "ae_ZakoOnHitByGadget", call_ae_ZakoOnHitByGadget, 0x00000000u},
    {0x80052640u, "ae_ObjIsOwnerType0B", call_ae_ObjIsOwnerType0B, 0x00000000u},
    {0x80052658u, "ae_ObjAnimClampLastFrame", call_ae_ObjAnimClampLastFrame, 0x00000000u},
    {0x80052688u, "ae_ObjBounceOnGround", call_ae_ObjBounceOnGround, 0x00000000u},
    {0x8005278Cu, "ae_ObjApplyVelocity", call_ae_ObjApplyVelocity, 0x00000000u},
    {0x80052800u, "ae_ObjKnockbackDecay", call_ae_ObjKnockbackDecay, 0x00000000u},
    {0x80052864u, "ae_ObjDeathSfx", call_ae_ObjDeathSfx, 0x00000000u},
    {0x800528E0u, "ae_ObjResetMotion", call_ae_ObjResetMotion, 0x00000000u},
    {0x800529A0u, "ae_ObjSwapToReactAnim", call_ae_ObjSwapToReactAnim, 0x00000000u},
    {0x80052A24u, "ae_ObjUpdateMotionState", call_ae_ObjUpdateMotionState, 0x00000000u},
    {0x80052BC8u, "ae_ObjSyncBodyHitFlags", call_ae_ObjSyncBodyHitFlags, 0x00000000u},
    {0x80052C4Cu, "ae_ObjSlotGetAnimIndex", call_ae_ObjSlotGetAnimIndex, 0x00000000u},
    {0x80052CA0u, "ae_ObjSlotSetField_CF", call_ae_ObjSlotSetField_CF, 0x00000000u},
    {0x80052CECu, "ae_SetDAT_800E77E2", call_ae_SetDAT_800E77E2, 0x00000000u},
    {0x80052D04u, "ae_ObjSlotGetTarget", call_ae_ObjSlotGetTarget, 0x00000000u},
    {0x80052D70u, "ae_ZakoWalkToTarget", call_ae_ZakoWalkToTarget, 0x00000000u},
    {0x80052DFCu, "ae_ZakoStateHurt", call_ae_ZakoStateHurt, 0x00000000u},
    {0x80052ED4u, "ae_ZakoEnterIdleWait", call_ae_ZakoEnterIdleWait, 0x00000000u},
    {0x80052F28u, "ae_ObjFootstepFx", call_ae_ObjFootstepFx, 0x00000000u},
    {0x80052FE8u, "ae_ZakoChasePlayer", call_ae_ZakoChasePlayer, 0x00000000u},
    {0x800530D0u, "ae_ZakoEnterDeath", call_ae_ZakoEnterDeath, 0x00000000u},
    {0x80053154u, "ae_ObjTurnTowardsPlayerXZ", call_ae_ObjTurnTowardsPlayerXZ, 0x00000000u},
    {0x8005325Cu, "ae_ObjSetSpeed96", call_ae_ObjSetSpeed96, 0x00000000u},
    {0x80053264u, "ae_ZakoStateWatch", call_ae_ZakoStateWatch, 0x00000000u},
    {0x80053304u, "ae_Zako_WanderBounce", call_ae_Zako_WanderBounce, 0x00000000u},
    {0x8005337Cu, "ae_Zako_InitDefaultStats", call_ae_Zako_InitDefaultStats, 0x00000000u},
    {0x80053440u, "ae_Zako_RecoverFromStun", call_ae_Zako_RecoverFromStun, 0x00000000u},
    {0x800534E8u, "ae_ZakoReturnIfFar", call_ae_ZakoReturnIfFar, 0x00000000u},
    {0x80053550u, "ae_ZakoUpdateGenericEnemy", call_ae_ZakoUpdateGenericEnemy, 0x00000000u},
    {0x800546DCu, "ae_ZakoUpdateStageSpecificEnemy", call_ae_ZakoUpdateStageSpecificEnemy, 0x00000000u},
    {0x80055370u, "ae_ObjTargetBeyondRange", call_ae_ObjTargetBeyondRange, 0x00000000u},
    {0x80055404u, "ae_ObjPlaySfx", call_ae_ObjPlaySfx, 0x00000000u},
    {0x80055460u, "ae_ObjWalkOnGround", call_ae_ObjWalkOnGround, 0x00000000u},
    {0x800554F0u, "ae_ObjMove", call_ae_ObjMove, 0x00000000u},
    {0x80055524u, "ae_ObjSetVelAlongDir", call_ae_ObjSetVelAlongDir, 0x00000000u},
    {0x800555D8u, "ae_ObjSetVelTowardPlayer", call_ae_ObjSetVelTowardPlayer, 0x00000000u},
    {0x80055674u, "ae_ObjSetVelTowardTarget", call_ae_ObjSetVelTowardTarget, 0x00000000u},
    {0x80055718u, "ae_ObjSeekPoint", call_ae_ObjSeekPoint, 0x00000000u},
    {0x8005588Cu, "ae_ObjCooldown", call_ae_ObjCooldown, 0x00000000u},
    {0x80055AA0u, "ae_ObjWalkToPoint", call_ae_ObjWalkToPoint, 0x00000000u},
    {0x80055C00u, "ae_ObjChasePlayer", call_ae_ObjChasePlayer, 0x00000000u},
    {0x80055D28u, "ae_ObjWanderStep", call_ae_ObjWanderStep, 0x00000000u},
    {0x80055DC0u, "ae_ObjWanderOrReturn", call_ae_ObjWanderOrReturn, 0x00000000u},
    {0x80056128u, "ae_ObjFleeFromPlayerInArea", call_ae_ObjFleeFromPlayerInArea, 0x00000000u},
    {0x80056200u, "ae_ObjSetVelFromFacing", call_ae_ObjSetVelFromFacing, 0x00000000u},
    {0x80056344u, "ae_ObjTryStepOnGround", call_ae_ObjTryStepOnGround, 0x00000000u},
    {0x8005648Cu, "ae_ObjFallToGround", call_ae_ObjFallToGround, 0x00000000u},
    {0x800564ECu, "ae_ObjPlayerOutOfYRange", call_ae_ObjPlayerOutOfYRange, 0x00000000u},
    {0x8005654Cu, "ae_ObjPlayerNearPointXZ", call_ae_ObjPlayerNearPointXZ, 0x00000000u},
    {0x800565C4u, "ae_ObjPlayerInCone", call_ae_ObjPlayerInCone, 0x00000000u},
    {0x800567B8u, "ae_ObjPlayerInConeFromPos2", call_ae_ObjPlayerInConeFromPos2, 0x00000000u},
    {0x8005685Cu, "ae_ObjTurnTowardsXZ", call_ae_ObjTurnTowardsXZ, 0x00000000u},
    {0x8005696Cu, "ae_ObjTurnToPlayer", call_ae_ObjTurnToPlayer, 0x00000000u},
    {0x80056C58u, "ae_ObjGroundHeightCached", call_ae_ObjGroundHeightCached, 0x00000000u},
    {0x80056DC4u, "ae_ObjAccelerate", call_ae_ObjAccelerate, 0x00000000u},
    {0x800570D4u, "ae_ObjClampFrame", call_ae_ObjClampFrame, 0x00000000u},
    {0x800570F4u, "ae_PlayerWithinRadiusXZ", call_ae_PlayerWithinRadiusXZ, 0x00000000u},
    {0x800571C4u, "ae_ObjAimAtPlayer", call_ae_ObjAimAtPlayer, 0x00000000u},
    {0x800572CCu, "ae_ObjFallAndBounce", call_ae_ObjFallAndBounce, 0x00000000u},
    {0x800573A0u, "ae_ObjPuffAt", call_ae_ObjPuffAt, 0x00000000u},
    {0x80057504u, "ae_ObjSpawnParticleAt", call_ae_ObjSpawnParticleAt, 0x00000000u},
    {0x80057558u, "ae_ZakoHooksReset", call_ae_ZakoHooksReset, 0x00000000u},
    {0x80057588u, "ae_ZakoHooksAdd", call_ae_ZakoHooksAdd, 0x00000000u},
    {0x8005760Cu, "ae_ZakoHooksRun", call_ae_ZakoHooksRun, 0x00000000u},
    {0x80057680u, "ae_ZakoAreaModeNoop", call_ae_ZakoAreaModeNoop, 0x00000000u},
    {0x80057FD0u, "ae_ZakoSetAreaMode", call_ae_ZakoSetAreaMode, 0x00000000u},
    {0x80058134u, "ae_Zako_Nop", call_ae_Zako_Nop, 0x00000000u},
    {0x8005813Cu, "ae_ObjAddHitBody", call_ae_ObjAddHitBody, 0x00000000u},
    {0x80058220u, "ae_ObjAddHitBodyIfPlayerNear", call_ae_ObjAddHitBodyIfPlayerNear, 0x00000000u},
    {0x800582ECu, "ae_ObjGetLinkedObjState", call_ae_ObjGetLinkedObjState, 0x00000000u},
    {0x800583BCu, "ae_ObjSetPathFromTable", call_ae_ObjSetPathFromTable, 0x00000000u},
    {0x80058400u, "ae_ObjSeekPathPoint", call_ae_ObjSeekPathPoint, 0x00000000u},
    {0x800584C8u, "ae_ObjAdvancePath", call_ae_ObjAdvancePath, 0x00000000u},
    {0x800585D0u, "ae_ObjFollowPath", call_ae_ObjFollowPath, 0x00000000u},
    {0x80058638u, "ae_ObjSetPosToPathPoint", call_ae_ObjSetPosToPathPoint, 0x00000000u},
    {0x8005866Cu, "ae_ObjFacePathPoint", call_ae_ObjFacePathPoint, 0x00000000u},
    {0x800587ECu, "ae_ZakoDebugMenuInit", call_ae_ZakoDebugMenuInit, 0x00000000u},
    {0x80058954u, "ae_ZakoDebugMenuDraw", call_ae_ZakoDebugMenuDraw, 0x00000000u},
    {0x800592E0u, "ae_ProjectilesReset", call_ae_ProjectilesReset, 0x00000000u},
    {0x80059304u, "ae_ProjectileSpawn", call_ae_ProjectileSpawn, 0x00000000u},
    {0x80059A54u, "ae_ProjectileAddHitBody", call_ae_ProjectileAddHitBody, 0x00000000u},
    {0x80059B08u, "ae_ProjectilesUpdateDraw", call_ae_ProjectilesUpdateDraw, 0x00000000u},
    {0x80059B54u, "ae_ProjectilesDraw", call_ae_ProjectilesDraw, 0x00000000u},
    {0x80059FA4u, "ae_ProjectilesUpdate", call_ae_ProjectilesUpdate, 0x00000000u},
    {0x8005AB00u, "ae_BossKindForArea", call_ae_BossKindForArea, 0x00000000u},
    {0x8005AB6Cu, "ae_IsArea53", call_ae_IsArea53, 0x00000000u},
    {0x8005AB90u, "ae_BossInit", call_ae_BossInit, 0x00000000u},
    {0x8005AC28u, "ae_BossUpdate", call_ae_BossUpdate, 0x00000000u},
    {0x8005ACE8u, "ae_AreaOverlayUpdate", call_ae_AreaOverlayUpdate, 0x00000000u},
    {0x8005AD90u, "ae_GimmickSaveTableReset", call_ae_GimmickSaveTableReset, 0x00000000u},
    {0x8005ADF8u, "ae_GimmickSaveStates", call_ae_GimmickSaveStates, 0x00000000u},
    {0x8005AF4Cu, "ae_GimmickRestoreStates", call_ae_GimmickRestoreStates, 0x00000000u},
    {0x8005B130u, "ae_GimmickSaveEntryAdd", call_ae_GimmickSaveEntryAdd, 0x00000000u},
    {0x8005B318u, "ae_GimmickInitFromProgress", call_ae_GimmickInitFromProgress, 0x00000000u},
    {0x8005B480u, "ae_GimmickSaveApplyProgress", call_ae_GimmickSaveApplyProgress, 0x00000000u},
    {0x8005B538u, "ae_GimmickSpawnAll", call_ae_GimmickSpawnAll, 0x00000000u},
    {0x8005B68Cu, "ae_GimmickUpdateAll", call_ae_GimmickUpdateAll, 0x00000000u},
    {0x8005B7BCu, "ae_GimmickGetInfo", call_ae_GimmickGetInfo, 0x00000000u},
    {0x8005B880u, "ae_GimmickEvent", call_ae_GimmickEvent, 0x00000000u},
    {0x8005BB5Cu, "ae_GimmickGetState", call_ae_GimmickGetState, 0x00000000u},
    {0x8005BB98u, "ae_GimmickTypeToSfx", call_ae_GimmickTypeToSfx, 0x00000000u},
    {0x8005BDC0u, "ae_main", call_ae_main, 0x00000000u},
    {0x8005BEE4u, "ae_Mode_04", call_ae_Mode_04, 0x00000000u},
    {0x8005BEF0u, "ae_StageRenderInit", call_ae_StageRenderInit, 0x00000000u},
    {0x8005BF70u, "ae_Mode_Stage", call_ae_Mode_Stage, 0x00000000u},
    {0x8005CA34u, "ae_DebugMenuInitFlags", call_ae_DebugMenuInitFlags, 0x00000000u},
    {0x8005CB1Cu, "ae_DebugMenuInitPlayerParams", call_ae_DebugMenuInitPlayerParams, 0x00000000u},
    {0x8005CC58u, "ae_DebugMenuInitCounters", call_ae_DebugMenuInitCounters, 0x00000000u},
    {0x8005CCDCu, "ae_DebugMenuInitCounters2", call_ae_DebugMenuInitCounters2, 0x00000000u},
    {0x8005D100u, "ae_StageRequest", call_ae_StageRequest, 0x00000000u},
    {0x8005D890u, "ae_StageRequestUpdate", call_ae_StageRequestUpdate, 0x00000000u},
    {0x8005DC78u, "ae_PlayerCanExitStage", call_ae_PlayerCanExitStage, 0x00000000u},
    {0x8005DCE4u, "ae_StageExitIsActive", call_ae_StageExitIsActive, 0x00000000u},
    {0x8005DD18u, "ae_CutsceneModeOn", call_ae_CutsceneModeOn, 0x00000000u},
    {0x8005DD28u, "ae_CutsceneModeOff", call_ae_CutsceneModeOff, 0x00000000u},
    {0x8005DD34u, "ae_GetDAT_800B01C2", call_ae_GetDAT_800B01C2, 0x00000000u},
    {0x8005DD78u, "ae_StageTimerAdd", call_ae_StageTimerAdd, 0x00000000u},
    {0x8005DDE0u, "ae_ResetFrameFlip", call_ae_ResetFrameFlip, 0x00000000u},
    {0x8005E0F0u, "ae_StageDrawScreenOverlay", call_ae_StageDrawScreenOverlay, 0x00000000u},
    {0x8005E42Cu, "ae_HudShowHealth", call_ae_HudShowHealth, 0x00000000u},
    {0x8005E4A8u, "ae_HudDrawHealth", call_ae_HudDrawHealth, 0x00000000u},
    {0x8005E6F0u, "ae_PlayerDamage", call_ae_PlayerDamage, 0x00000000u},
    {0x8005E794u, "ae_PlayerHeal", call_ae_PlayerHeal, 0x00000000u},
    {0x8005E7ECu, "ae_AreaTexCopyInit", call_ae_AreaTexCopyInit, 0x00000000u},
    {0x8005EAB0u, "ae_AreaTexCopyDraw", call_ae_AreaTexCopyDraw, 0x00000000u},
    {0x8005F2E0u, "ae_HudPanelA_DrawIcon", call_ae_HudPanelA_DrawIcon, 0x00000000u},
    {0x8005F5FCu, "ae_HudPanelB_DrawSprite", call_ae_HudPanelB_DrawSprite, 0x00000000u},
    {0x8005F770u, "ae_HudDrawDigit", call_ae_HudDrawDigit, 0x00000000u},
    {0x8005F87Cu, "ae_HudDrawNumber", call_ae_HudDrawNumber, 0x00000000u},
    {0x8005F978u, "ae_HudDrawTime", call_ae_HudDrawTime, 0x00000000u},
    {0x8005FCA4u, "ae_HudDrawIcon16", call_ae_HudDrawIcon16, 0x00000000u},
    {0x8005FD80u, "ae_HudCounterC_Draw", call_ae_HudCounterC_Draw, 0x00000000u},
    {0x8005FFD4u, "ae_HudDrawTimerGauge", call_ae_HudDrawTimerGauge, 0x00000000u},
    {0x80060780u, "ae_PlayerUpdate", call_ae_PlayerUpdate, 0x00000000u},
    {0x80063628u, "ae_PlayerAnimateAndDraw", call_ae_PlayerAnimateAndDraw, 0x00000000u},
    {0x80063EC4u, "ae_PlayerTryLedgeGrab", call_ae_PlayerTryLedgeGrab, 0x00000000u},
    {0x80064208u, "ae_PlayerSetState", call_ae_PlayerSetState, 0x00000000u},
    {0x80064454u, "ae_PlayerRunAnimEvents", call_ae_PlayerRunAnimEvents, 0x00000000u},
    {0x800649D8u, "ae_PlayerRunTransitions", call_ae_PlayerRunTransitions, 0x00000000u},
    {0x80064AF0u, "ae_PlayerCond_False", call_ae_PlayerCond_False, 0x00000000u},
    {0x80064AF8u, "ae_PlayerCond_JumpHeld", call_ae_PlayerCond_JumpHeld, 0x00000000u},
    {0x80064B08u, "ae_PlayerCond_JumpPressed", call_ae_PlayerCond_JumpPressed, 0x00000000u},
    {0x80064B24u, "ae_PlayerCond_RStickEvent02", call_ae_PlayerCond_RStickEvent02, 0x00000000u},
    {0x80064BBCu, "ae_PlayerCond_RStickEvent40", call_ae_PlayerCond_RStickEvent40, 0x00000000u},
    {0x80064C3Cu, "ae_PlayerCond_RStickBackHard", call_ae_PlayerCond_RStickBackHard, 0x00000000u},
    {0x80064C74u, "ae_PlayerCond_GadgetBusyOrGesture", call_ae_PlayerCond_GadgetBusyOrGesture, 0x00000000u},
    {0x80064CC8u, "ae_PlayerCond_FacingDesiredDir", call_ae_PlayerCond_FacingDesiredDir, 0x00000000u},
    {0x80064D18u, "ae_PlayerCond_RStickSideNeg", call_ae_PlayerCond_RStickSideNeg, 0x00000000u},
    {0x80064D4Cu, "ae_PlayerCond_RStickSidePos", call_ae_PlayerCond_RStickSidePos, 0x00000000u},
    {0x80064D7Cu, "ae_PlayerCond_RStickFwd", call_ae_PlayerCond_RStickFwd, 0x00000000u},
    {0x80064DACu, "ae_PlayerCond_RStickBack", call_ae_PlayerCond_RStickBack, 0x00000000u},
    {0x80064DE0u, "ae_PlayerCond_AttachTimerDone", call_ae_PlayerCond_AttachTimerDone, 0x00000000u},
    {0x80064E04u, "ae_PlayerCond_GroundBehindLow", call_ae_PlayerCond_GroundBehindLow, 0x00000000u},
    {0x80064E6Cu, "ae_PlayerCond_RStickGestureOrTravel", call_ae_PlayerCond_RStickGestureOrTravel, 0x00000000u},
    {0x80064EACu, "ae_PlayerCond_AnimDoneRandom", call_ae_PlayerCond_AnimDoneRandom, 0x00000000u},
    {0x80064EF8u, "ae_PlayerCond_NoMoveInput", call_ae_PlayerCond_NoMoveInput, 0x00000000u},
    {0x80064F28u, "ae_PlayerCond_LStickFwd", call_ae_PlayerCond_LStickFwd, 0x00000000u},
    {0x80064F5Cu, "ae_PlayerCond_LStickBack", call_ae_PlayerCond_LStickBack, 0x00000000u},
    {0x80064F90u, "ae_PlayerCond_RStickEvent04", call_ae_PlayerCond_RStickEvent04, 0x00000000u},
    {0x8006501Cu, "ae_PlayerCond_RStickEvent08", call_ae_PlayerCond_RStickEvent08, 0x00000000u},
    {0x800650A8u, "ae_PlayerCond_NoGestureR3Held", call_ae_PlayerCond_NoGestureR3Held, 0x00000000u},
    {0x800650E8u, "ae_PlayerCond_RStickRotating", call_ae_PlayerCond_RStickRotating, 0x00000000u},
    {0x8006512Cu, "ae_PlayerCond_Gadget1RStickFwdFlick", call_ae_PlayerCond_Gadget1RStickFwdFlick, 0x00000000u},
    {0x80065178u, "ae_PlayerCond_Gadget3RStickBack", call_ae_PlayerCond_Gadget3RStickBack, 0x00000000u},
    {0x800651C4u, "ae_PlayerCond_RStickGesture10", call_ae_PlayerCond_RStickGesture10, 0x00000000u},
    {0x800651D4u, "ae_PlayerCond_RStickGesture20", call_ae_PlayerCond_RStickGesture20, 0x00000000u},
    {0x800651E4u, "ae_PlayerCond_RStickNoGesture", call_ae_PlayerCond_RStickNoGesture, 0x00000000u},
    {0x800651F4u, "ae_PlayerCond_GestureEvent04", call_ae_PlayerCond_GestureEvent04, 0x00000000u},
    {0x80065204u, "ae_PlayerCond_GestureEvent08", call_ae_PlayerCond_GestureEvent08, 0x00000000u},
    {0x80065214u, "ae_PlayerCond_LStickIdleL3Held", call_ae_PlayerCond_LStickIdleL3Held, 0x00000000u},
    {0x80065244u, "ae_PlayerCond_LStickIdle", call_ae_PlayerCond_LStickIdle, 0x00000000u},
    {0x80065254u, "ae_PlayerCond_LStickClass2", call_ae_PlayerCond_LStickClass2, 0x00000000u},
    {0x80065264u, "ae_PlayerCond_LStickClass4", call_ae_PlayerCond_LStickClass4, 0x00000000u},
    {0x80065274u, "ae_PlayerCond_LStickClass8", call_ae_PlayerCond_LStickClass8, 0x00000000u},
    {0x80065284u, "ae_PlayerCond_LStickFull", call_ae_PlayerCond_LStickFull, 0x00000000u},
    {0x8006529Cu, "ae_PlayerCond_RStickFull", call_ae_PlayerCond_RStickFull, 0x00000000u},
    {0x800652B4u, "ae_PlayerCond_LStickPartial", call_ae_PlayerCond_LStickPartial, 0x00000000u},
    {0x800652CCu, "ae_PlayerCond_RStickPartial", call_ae_PlayerCond_RStickPartial, 0x00000000u},
    {0x800652E4u, "ae_PlayerIsOnGround", call_ae_PlayerIsOnGround, 0x00000000u},
    {0x80065318u, "ae_PlayerCond_LandingSoon", call_ae_PlayerCond_LandingSoon, 0x00000000u},
    {0x800653C0u, "ae_PlayerCond_FacingContactWall", call_ae_PlayerCond_FacingContactWall, 0x00000000u},
    {0x8006543Cu, "ae_PlayerCond_ContactKind8", call_ae_PlayerCond_ContactKind8, 0x00000000u},
    {0x80065458u, "ae_PlayerCond_NotContactKind8", call_ae_PlayerCond_NotContactKind8, 0x00000000u},
    {0x80065474u, "ae_PlayerCond_FallingHigh", call_ae_PlayerCond_FallingHigh, 0x00000000u},
    {0x800654ACu, "ae_PlayerCond_RStickEvent10", call_ae_PlayerCond_RStickEvent10, 0x00000000u},
    {0x80065538u, "ae_PlayerCond_RStickEvent20", call_ae_PlayerCond_RStickEvent20, 0x00000000u},
    {0x800655C4u, "ae_PlayerCond_FirstFrameRStickEvent10", call_ae_PlayerCond_FirstFrameRStickEvent10, 0x00000000u},
    {0x8006562Cu, "ae_PlayerCond_FirstFrameRStickEvent20", call_ae_PlayerCond_FirstFrameRStickEvent20, 0x00000000u},
    {0x80065694u, "ae_GadgetHasAltStance", call_ae_GadgetHasAltStance, 0x00000000u},
    {0x800656DCu, "ae_PlayerCond_JumpPressedAtApex", call_ae_PlayerCond_JumpPressedAtApex, 0x00000000u},
    {0x80065748u, "ae_PlayerCond_LStickSidePos", call_ae_PlayerCond_LStickSidePos, 0x00000000u},
    {0x80065760u, "ae_PlayerCond_LStickSideNeg", call_ae_PlayerCond_LStickSideNeg, 0x00000000u},
    {0x80065770u, "ae_PlayerCond_LStickSideAny", call_ae_PlayerCond_LStickSideAny, 0x00000000u},
    {0x80065790u, "ae_PlayerCond_LStickIdleStillMoving", call_ae_PlayerCond_LStickIdleStillMoving, 0x00000000u},
    {0x800657E0u, "ae_PlayerCond_GestureEvent0C", call_ae_PlayerCond_GestureEvent0C, 0x00000000u},
    {0x800657F0u, "ae_PlayerCheckWaterEntry", call_ae_PlayerCheckWaterEntry, 0x00000000u},
    {0x800658E8u, "ae_PlayerCond_InWater", call_ae_PlayerCond_InWater, 0x00000000u},
    {0x80065904u, "ae_PlayerCond_WadingOnGround", call_ae_PlayerCond_WadingOnGround, 0x00000000u},
    {0x80065980u, "ae_PlayerCond_ContactWall3B", call_ae_PlayerCond_ContactWall3B, 0x00000000u},
    {0x800659ACu, "ae_PlayerCond_JumpOutOfWater", call_ae_PlayerCond_JumpOutOfWater, 0x00000000u},
    {0x80065A24u, "ae_PlayerCond_CanJump", call_ae_PlayerCond_CanJump, 0x00000000u},
    {0x80065AE8u, "ae_PlayerCond_LStickIdleL3Pressed", call_ae_PlayerCond_LStickIdleL3Pressed, 0x00000000u},
    {0x80065B20u, "ae_PlayerCond_Dead", call_ae_PlayerCond_Dead, 0x00000000u},
    {0x80065B54u, "ae_PlayerCond_Gadget4Mode1", call_ae_PlayerCond_Gadget4Mode1, 0x00000000u},
    {0x80065B6Cu, "ae_PlayerCond_AnimDone", call_ae_PlayerCond_AnimDone, 0x00000000u},
    {0x80065BA8u, "ae_PlayerCond_False2", call_ae_PlayerCond_False2, 0x00000000u},
    {0x80065BB0u, "ae_PlayerCond_Still", call_ae_PlayerCond_Still, 0x00000000u},
    {0x80065C20u, "ae_PlayerAct_ScaleMoveVel", call_ae_PlayerAct_ScaleMoveVel, 0x00000000u},
    {0x80065D1Cu, "ae_PlayerAct_JumpHoldBoost", call_ae_PlayerAct_JumpHoldBoost, 0x00000000u},
    {0x80065D68u, "ae_PlayerAct_JumpPct3", call_ae_PlayerAct_JumpPct3, 0x00000000u},
    {0x80065DECu, "ae_PlayerAct_VerticalOnly", call_ae_PlayerAct_VerticalOnly, 0x00000000u},
    {0x80065E18u, "ae_PlayerAct_JumpPct2", call_ae_PlayerAct_JumpPct2, 0x00000000u},
    {0x80065E9Cu, "ae_PlayerAct_SquashReset", call_ae_PlayerAct_SquashReset, 0x00000000u},
    {0x80065ED4u, "ae_PlayerAct_FlyerThrust", call_ae_PlayerAct_FlyerThrust, 0x00000000u},
    {0x80065FC8u, "ae_PlayerAct_FlyerLaunchA", call_ae_PlayerAct_FlyerLaunchA, 0x00000000u},
    {0x80065FF0u, "ae_PlayerAct_FlyerLaunchB", call_ae_PlayerAct_FlyerLaunchB, 0x00000000u},
    {0x80066018u, "ae_PlayerStartHurtBlink", call_ae_PlayerStartHurtBlink, 0x00000000u},
    {0x80066028u, "ae_PlayerAct_GadgetSpinFromStick", call_ae_PlayerAct_GadgetSpinFromStick, 0x00000000u},
    {0x80066094u, "ae_PlayerEnterSwimIfInWater", call_ae_PlayerEnterSwimIfInWater, 0x00000000u},
    {0x800660C8u, "ae_PlayerAct_Gadget3Aim", call_ae_PlayerAct_Gadget3Aim, 0x00000000u},
    {0x800660E8u, "ae_PlayerAct_DriftForward", call_ae_PlayerAct_DriftForward, 0x00000000u},
    {0x8006611Cu, "ae_PlayerAct_NegateInputXZ", call_ae_PlayerAct_NegateInputXZ, 0x00000000u},
    {0x80066140u, "ae_PlayerAct_TurnAround", call_ae_PlayerAct_TurnAround, 0x00000000u},
    {0x80066168u, "ae_PlayerAct_TurnAroundAndJump", call_ae_PlayerAct_TurnAroundAndJump, 0x00000000u},
    {0x80066190u, "ae_PlayerAct_Nop", call_ae_PlayerAct_Nop, 0x00000000u},
    {0x80066198u, "ae_PlayerAct_FaceStick", call_ae_PlayerAct_FaceStick, 0x00000000u},
    {0x800661D4u, "ae_PlayerAct_JumpFromLStick", call_ae_PlayerAct_JumpFromLStick, 0x00000000u},
    {0x800662DCu, "ae_PlayerAct_StepBackDrop", call_ae_PlayerAct_StepBackDrop, 0x00000000u},
    {0x80066358u, "ae_PlayerAct_BounceBack", call_ae_PlayerAct_BounceBack, 0x00000000u},
    {0x80066434u, "ae_PlayerAct_ExitToMode0Forward", call_ae_PlayerAct_ExitToMode0Forward, 0x00000000u},
    {0x800664D0u, "ae_PlayerAct_AnimSpeedNormal", call_ae_PlayerAct_AnimSpeedNormal, 0x00000000u},
    {0x800664E0u, "ae_PlayerAct_AnimSpeedHalf", call_ae_PlayerAct_AnimSpeedHalf, 0x00000000u},
    {0x800664F0u, "ae_PlayerAct_FaceStickMaybeHalfSpeed", call_ae_PlayerAct_FaceStickMaybeHalfSpeed, 0x00000000u},
    {0x80066530u, "ae_PlayerAct_FaceStickRotateA", call_ae_PlayerAct_FaceStickRotateA, 0x00000000u},
    {0x80066568u, "ae_PlayerAct_FaceStickRotateB", call_ae_PlayerAct_FaceStickRotateB, 0x00000000u},
    {0x800665A0u, "ae_PlayerAct_AimModeEnter", call_ae_PlayerAct_AimModeEnter, 0x00000000u},
    {0x800665C0u, "ae_PlayerAct_AimModeLeave", call_ae_PlayerAct_AimModeLeave, 0x00000000u},
    {0x800665E0u, "ae_PlayerResetAir", call_ae_PlayerResetAir, 0x00000000u},
    {0x800665F8u, "ae_PlayerAct_FaceRightPerp", call_ae_PlayerAct_FaceRightPerp, 0x00000000u},
    {0x8006663Cu, "ae_PlayerAct_FaceLeftPerp", call_ae_PlayerAct_FaceLeftPerp, 0x00000000u},
    {0x80066680u, "ae_PlayerAct_TurnFacingLeft90", call_ae_PlayerAct_TurnFacingLeft90, 0x00000000u},
    {0x800666A4u, "ae_PlayerAct_TurnFacingRight90", call_ae_PlayerAct_TurnFacingRight90, 0x00000000u},
    {0x800666C8u, "ae_PlayerAct_FaceStickOnRotation", call_ae_PlayerAct_FaceStickOnRotation, 0x00000000u},
    {0x8006671Cu, "ae_PlayerAct_Rebound", call_ae_PlayerAct_Rebound, 0x00000000u},
    {0x80066744u, "ae_PlayerAct_HurtVoiceFlag", call_ae_PlayerAct_HurtVoiceFlag, 0x00000000u},
    {0x80066770u, "ae_PlayerAct_Land", call_ae_PlayerAct_Land, 0x00000000u},
    {0x800668A0u, "ae_PlayerOnWallBump", call_ae_PlayerOnWallBump, 0x00000000u},
    {0x8006699Cu, "ae_PlayerAccelXZ", call_ae_PlayerAccelXZ, 0x00000000u},
    {0x80066ADCu, "ae_PlayerAccelXYZ", call_ae_PlayerAccelXYZ, 0x00000000u},
    {0x80066C90u, "ae_TurnTowardsY", call_ae_TurnTowardsY, 0x00000000u},
    {0x80066E28u, "ae_PlayerSetClimbTarget", call_ae_PlayerSetClimbTarget, 0x00000000u},
    {0x80066E8Cu, "ae_PlayerSetRope", call_ae_PlayerSetRope, 0x00000000u},
    {0x80066F50u, "ae_player_can_switch_gadget", call_ae_player_can_switch_gadget, 0x00000000u},
    {0x80066FC8u, "ae_PlayerOnContact", call_ae_PlayerOnContact, 0x00000000u},
    {0x80067974u, "ae_player_gadget_select_request", call_ae_player_gadget_select_request, 0x00000000u},
    {0x80067A1Cu, "ae_PlayerApplyGadgetStance", call_ae_PlayerApplyGadgetStance, 0x00000000u},
    {0x80067AF0u, "ae_PlayerEyeBlinkUpdate", call_ae_PlayerEyeBlinkUpdate, 0x00000000u},
    {0x80067C08u, "ae_PlayerSetEyeFrame", call_ae_PlayerSetEyeFrame, 0x00000000u},
    {0x80067CACu, "ae_PlayerUpdateClimb", call_ae_PlayerUpdateClimb, 0x00000000u},
    {0x800681E4u, "ae_PlayerUpdateRope", call_ae_PlayerUpdateRope, 0x00000000u},
    {0x80068634u, "ae_PlayerUpdateHeldByObject", call_ae_PlayerUpdateHeldByObject, 0x00000000u},
    {0x800687D8u, "ae_PlayerUpdateIdleAnim", call_ae_PlayerUpdateIdleAnim, 0x00000000u},
    {0x8006896Cu, "ae_PlayPlayerVoice", call_ae_PlayPlayerVoice, 0x00000000u},
    {0x800689F0u, "ae_PlayerDrawSquashed", call_ae_PlayerDrawSquashed, 0x00000000u},
    {0x80068ADCu, "ae_PlayerDrawSquashedWithFx", call_ae_PlayerDrawSquashedWithFx, 0x00000000u},
    {0x80068B1Cu, "ae_PlayerDrawSquashedFlattened", call_ae_PlayerDrawSquashedFlattened, 0x00000000u},
    {0x80068B7Cu, "ae_player_is_swimming", call_ae_player_is_swimming, 0x00000000u},
    {0x80068BC0u, "ae_PlayerSwimSfxUpdate", call_ae_PlayerSwimSfxUpdate, 0x00000000u},
    {0x800690E8u, "ae_AreaSetVramAnimList", call_ae_AreaSetVramAnimList, 0x00000000u},
    {0x800691F8u, "ae_VramScrollUpdate", call_ae_VramScrollUpdate, 0x00000000u},
    {0x80069500u, "ae_EffectsReset", call_ae_EffectsReset, 0x00000000u},
    {0x80069548u, "ae_ParticleAlloc", call_ae_ParticleAlloc, 0x00000000u},
    {0x8006962Cu, "ae_SpawnOneParticle", call_ae_SpawnOneParticle, 0x00000000u},
    {0x800696ECu, "ae_SpawnEffectGroup", call_ae_SpawnEffectGroup, 0x00000000u},
    {0x800698B8u, "ae_SpawnParticle", call_ae_SpawnParticle, 0x00000000u},
    {0x80069920u, "ae_ParticlesDraw", call_ae_ParticlesDraw, 0x00000000u},
    {0x80069BECu, "ae_EffectScriptUpdate", call_ae_EffectScriptUpdate, 0x00000000u},
    {0x8006A9B0u, "ae_EffectsResetSubsystems", call_ae_EffectsResetSubsystems, 0x00000000u},
    {0x8006A9F8u, "ae_EffectsUpdateAll", call_ae_EffectsUpdateAll, 0x00000000u},
    {0x8006B168u, "ae_PlayerFxTrailUpdate", call_ae_PlayerFxTrailUpdate, 0x00000000u},
    {0x8006B3CCu, "ae_SpawnPuff", call_ae_SpawnPuff, 0x00000000u},
    {0x8006B3F0u, "ae_EffectBigBurst", call_ae_EffectBigBurst, 0x00000000u},
    {0x8006B570u, "ae_EffectGroundImpact", call_ae_EffectGroundImpact, 0x00000000u},
    {0x8006B70Cu, "ae_PlayerSpawnDust", call_ae_PlayerSpawnDust, 0x00000000u},
    {0x8006B7DCu, "ae_PlayerFxBlinkReset", call_ae_PlayerFxBlinkReset, 0x00000000u},
    {0x8006B800u, "ae_PlayerEffectsUpdate", call_ae_PlayerEffectsUpdate, 0x00000000u},
    {0x8006B848u, "ae_PlayerSplashStart", call_ae_PlayerSplashStart, 0x00000000u},
    {0x8006B950u, "ae_PlayerSplash", call_ae_PlayerSplash, 0x00000000u},
    {0x8006B9F8u, "ae_PlayerIsSubmerged", call_ae_PlayerIsSubmerged, 0x00000000u},
    {0x8006BA2Cu, "ae_PlayerWaterEffectsUpdate", call_ae_PlayerWaterEffectsUpdate, 0x00000000u},
    {0x8006BF10u, "ae_PlayerSpawnDustBehind", call_ae_PlayerSpawnDustBehind, 0x00000000u},
    {0x8006BF84u, "ae_EffectGroundImpactAtPlayer", call_ae_EffectGroundImpactAtPlayer, 0x00000000u},
    {0x8006C204u, "ae_PlayerSparkleBurst", call_ae_PlayerSparkleBurst, 0x00000000u},
    {0x8006C2C0u, "ae_PlayerFxSpawn10C", call_ae_PlayerFxSpawn10C, 0x00000000u},
    {0x8006C2E8u, "ae_PlayerSetTintEffect", call_ae_PlayerSetTintEffect, 0x00000000u},
    {0x8006C334u, "ae_PlayerTintEffectUpdate", call_ae_PlayerTintEffectUpdate, 0x00000000u},
    {0x8006C6A4u, "ae_GadgetFlashSet", call_ae_GadgetFlashSet, 0x00000000u},
    {0x8006C6D4u, "ae_GadgetFlashUpdate", call_ae_GadgetFlashUpdate, 0x00000000u},
    {0x8006C92Cu, "ae_PlayerFxSquashParticles", call_ae_PlayerFxSquashParticles, 0x00000000u},
    {0x8006CC28u, "ae_PlayerFxFlattenedParticles", call_ae_PlayerFxFlattenedParticles, 0x00000000u},
    {0x8006CDE0u, "ae_GadgetSelectFx", call_ae_GadgetSelectFx, 0x00000000u},
    {0x8006CEF0u, "ae_Gadget6SparkleFx", call_ae_Gadget6SparkleFx, 0x00000000u},
    {0x8006CFA4u, "ae_PlayerFxResetBlink", call_ae_PlayerFxResetBlink, 0x00000000u},
    {0x8006D044u, "ae_Gadget7TrailUpdate", call_ae_Gadget7TrailUpdate, 0x00000000u},
    {0x8006D4C0u, "ae_SpriteEffectQueue", call_ae_SpriteEffectQueue, 0x00000000u},
    {0x8006D548u, "ae_SpriteEffectQueue0", call_ae_SpriteEffectQueue0, 0x00000000u},
    {0x8006D588u, "ae_SpriteEffectQueue2", call_ae_SpriteEffectQueue2, 0x00000000u},
    {0x8006D5A8u, "ae_SpriteEffectsDraw", call_ae_SpriteEffectsDraw, 0x00000000u},
    {0x8006DAC8u, "ae_Gadget4TrailReset", call_ae_Gadget4TrailReset, 0x00000000u},
    {0x8006DAF8u, "ae_Gadget4TrailUpdate", call_ae_Gadget4TrailUpdate, 0x00000000u},
    {0x8006E02Cu, "ae_SparkTableClear", call_ae_SparkTableClear, 0x00000000u},
    {0x8006E054u, "ae_SparkAdd", call_ae_SparkAdd, 0x00000000u},
    {0x8006E0DCu, "ae_SparkAddRandom", call_ae_SparkAddRandom, 0x00000000u},
    {0x8006E1C0u, "ae_FxPlayerState6Update", call_ae_FxPlayerState6Update, 0x00000000u},
    {0x8006ED4Cu, "ae_TrailsReset", call_ae_TrailsReset, 0x00000000u},
    {0x8006ED78u, "ae_TrailSpawn", call_ae_TrailSpawn, 0x00000000u},
    {0x8006EE3Cu, "ae_TrailsUpdate", call_ae_TrailsUpdate, 0x00000000u},
    {0x8006EF08u, "ae_TrailPointAdd", call_ae_TrailPointAdd, 0x00000000u},
    {0x8006EF88u, "ae_TrailPointsDraw", call_ae_TrailPointsDraw, 0x00000000u},
    {0x8006F3D0u, "ae_RibbonDraw", call_ae_RibbonDraw, 0x00000000u},
    {0x8006FA10u, "ae_EffectObjDraw", call_ae_EffectObjDraw, 0x00000000u},
    {0x8006FE54u, "ae_EffectObjsReset", call_ae_EffectObjsReset, 0x00000000u},
    {0x8006FE7Cu, "ae_EffectObjAlloc", call_ae_EffectObjAlloc, 0x00000000u},
    {0x8006FEE8u, "ae_EffectObjsUpdate", call_ae_EffectObjsUpdate, 0x00000000u},
    {0x80070130u, "ae_EffectObjSpawn", call_ae_EffectObjSpawn, 0x00000000u},
    {0x80070284u, "ae_EffectSpawnFromDef", call_ae_EffectSpawnFromDef, 0x00000000u},
    {0x80070560u, "ae_GroundSparksUpdateDraw", call_ae_GroundSparksUpdateDraw, 0x00000000u},
    {0x80070B48u, "ae_RandomParticlesReset", call_ae_RandomParticlesReset, 0x00000000u},
    {0x80070B70u, "ae_SpawnRandomParticle", call_ae_SpawnRandomParticle, 0x00000000u},
    {0x80070D94u, "ae_RandomParticlesUpdate", call_ae_RandomParticlesUpdate, 0x00000000u},
    {0x800710F8u, "ae_FxTableClear12", call_ae_FxTableClear12, 0x00000000u},
    {0x80071370u, "ae_FlyingMeshesUpdateDraw", call_ae_FlyingMeshesUpdateDraw, 0x00000000u},
    {0x800715A0u, "ae_IndicatorsReset", call_ae_IndicatorsReset, 0x00000000u},
    {0x800715B4u, "ae_IndicatorHintFree", call_ae_IndicatorHintFree, 0x00000000u},
    {0x800715FCu, "ae_IndicatorHintReady", call_ae_IndicatorHintReady, 0x00000000u},
    {0x8007163Cu, "ae_AreaSetIndicatorModelA", call_ae_AreaSetIndicatorModelA, 0x00000000u},
    {0x80071694u, "ae_AreaSetIndicatorModelB", call_ae_AreaSetIndicatorModelB, 0x00000000u},
    {0x800716ECu, "ae_Indicator_Register", call_ae_Indicator_Register, 0x00000000u},
    {0x800719F8u, "ae_IndicatorsUpdateDraw", call_ae_IndicatorsUpdateDraw, 0x00000000u},
    {0x80071D20u, "ae_CollProbeCounterInc", call_ae_CollProbeCounterInc, 0x00000000u},
    {0x80071D3Cu, "ae_CollisionBodiesReset", call_ae_CollisionBodiesReset, 0x00000000u},
    {0x80071E44u, "ae_CollisionBodyRegister", call_ae_CollisionBodyRegister, 0x00000000u},
    {0x80071FDCu, "ae_CollisionBodyAllocSlot", call_ae_CollisionBodyAllocSlot, 0x00000000u},
    {0x80072018u, "ae_CollisionBodyUnregister", call_ae_CollisionBodyUnregister, 0x00000000u},
    {0x80072170u, "ae_GetObjectByIndex", call_ae_GetObjectByIndex, 0x00000000u},
    {0x800721B4u, "ae_CollisionBodyIndexOf", call_ae_CollisionBodyIndexOf, 0x00000000u},
    {0x80072220u, "ae_CollisionBodyAlloc", call_ae_CollisionBodyAlloc, 0x00000000u},
    {0x8007228Cu, "ae_CollisionBodyClear", call_ae_CollisionBodyClear, 0x00000000u},
    {0x800722D4u, "ae_CollisionBodyFree", call_ae_CollisionBodyFree, 0x00000000u},
    {0x80072334u, "ae_LoopSlotAdd", call_ae_LoopSlotAdd, 0x00000000u},
    {0x80072380u, "ae_LoopSlotRef", call_ae_LoopSlotRef, 0x00000000u},
    {0x800723E8u, "ae_LoopSlotSweep", call_ae_LoopSlotSweep, 0x00000000u},
    {0x80072454u, "ae_LoopSlotClearAll", call_ae_LoopSlotClearAll, 0x00000000u},
    {0x8007247Cu, "ae_CollisionUpdate", call_ae_CollisionUpdate, 0x00000000u},
    {0x8007292Cu, "ae_SoundLoopsPause", call_ae_SoundLoopsPause, 0x00000000u},
    {0x800729CCu, "ae_SoundLoopsResume", call_ae_SoundLoopsResume, 0x00000000u},
    {0x80072A14u, "ae_PlayerVehicleSoundUpdate", call_ae_PlayerVehicleSoundUpdate, 0x00000000u},
    {0x80072BE8u, "ae_PlayerMoveRumbleSoundUpdate", call_ae_PlayerMoveRumbleSoundUpdate, 0x00000000u},
    {0x80072D28u, "ae_BodyProbeGround", call_ae_BodyProbeGround, 0x00000000u},
    {0x80072DC4u, "ae_CollisionStub_ReturnZero", call_ae_CollisionStub_ReturnZero, 0x00000000u},
    {0x80072DCCu, "ae_BodyLandOnGround", call_ae_BodyLandOnGround, 0x00000000u},
    {0x8007304Cu, "ae_CollisionBuildProbePoints", call_ae_CollisionBuildProbePoints, 0x00000000u},
    {0x800733DCu, "ae_CollisionShiftProbePoints", call_ae_CollisionShiftProbePoints, 0x00000000u},
    {0x80073450u, "ae_CollisionResolveProbes", call_ae_CollisionResolveProbes, 0x00000000u},
    {0x800739FCu, "ae_SignedRangeOverlap", call_ae_SignedRangeOverlap, 0x00000000u},
    {0x80073AA0u, "ae_PointInBoxXYZ", call_ae_PointInBoxXYZ, 0x00000000u},
    {0x80073BC8u, "ae_CollProbe_Box", call_ae_CollProbe_Box, 0x00000000u},
    {0x80073FD4u, "ae_CollProbe_Ramp", call_ae_CollProbe_Ramp, 0x00000000u},
    {0x8007449Cu, "ae_CollProbe_Triangle", call_ae_CollProbe_Triangle, 0x00000000u},
    {0x80074BBCu, "ae_CollProbe_Radial", call_ae_CollProbe_Radial, 0x00000000u},
    {0x80075630u, "ae_CollProbe_HalfPipe", call_ae_CollProbe_HalfPipe, 0x00000000u},
    {0x80075A40u, "ae_CollProbe_Trench", call_ae_CollProbe_Trench, 0x00000000u},
    {0x80075F48u, "ae_CollProbe_ThinWall", call_ae_CollProbe_ThinWall, 0x00000000u},
    {0x800761ACu, "ae_CollProbe_EdgeWall", call_ae_CollProbe_EdgeWall, 0x00000000u},
    {0x80076488u, "ae_CollProbe_DiagonalWall", call_ae_CollProbe_DiagonalWall, 0x00000000u},
    {0x80076AC8u, "ae_CollProbe_Slope", call_ae_CollProbe_Slope, 0x00000000u},
    {0x80076E80u, "ae_CollProbe_Rope", call_ae_CollProbe_Rope, 0x00000000u},
    {0x8007734Cu, "ae_CollProbe_Strip", call_ae_CollProbe_Strip, 0x00000000u},
    {0x80077490u, "ae_CollProbe_NoContact", call_ae_CollProbe_NoContact, 0x00000000u},
    {0x800774B8u, "ae_DiagonalSignedDistance", call_ae_DiagonalSignedDistance, 0x00000000u},
    {0x800777ACu, "ae_BodyCollideWithBodies", call_ae_BodyCollideWithBodies, 0x00000000u},
    {0x80077C28u, "ae_CollisionBodyGetCenter", call_ae_CollisionBodyGetCenter, 0x00000000u},
    {0x80077D18u, "ae_BodyFindOverlapType1", call_ae_BodyFindOverlapType1, 0x00000000u},
    {0x80077EE8u, "ae_BodyFindOverlapType3to5", call_ae_BodyFindOverlapType3to5, 0x00000000u},
    {0x80078088u, "ae_BodyCollideBodiesDetailed", call_ae_BodyCollideBodiesDetailed, 0x00000000u},
    {0x80078808u, "ae_BodyContactSfx", call_ae_BodyContactSfx, 0x00000000u},
    {0x80078864u, "ae_BodyTypeOfActor", call_ae_BodyTypeOfActor, 0x00000000u},
    {0x800788B8u, "ae_BodyCheckPickupContact", call_ae_BodyCheckPickupContact, 0x00000000u},
    {0x80078A18u, "ae_PickupCollect", call_ae_PickupCollect, 0x00000000u},
    {0x80078C9Cu, "ae_CollisionRayTest", call_ae_CollisionRayTest, 0x00000000u},
    {0x800792C8u, "ae_BodyRecordColliderContact", call_ae_BodyRecordColliderContact, 0x00000000u},
    {0x800797F8u, "ae_GetSurfaceType", call_ae_GetSurfaceType, 0x00000000u},
    {0x80079A28u, "ae_ColliderTypeActive", call_ae_ColliderTypeActive, 0x00000000u},
    {0x80079B10u, "ae_StageFadeUpdate", call_ae_StageFadeUpdate, 0x00000000u},
    {0x80079E78u, "ae_ScreenFlashFadeDraw", call_ae_ScreenFlashFadeDraw, 0x00000000u},
    {0x80079ECCu, "ae_FadeSpiralDraw", call_ae_FadeSpiralDraw, 0x00000000u},
    {0x8007A180u, "ae_PlayerStopGadgetSounds", call_ae_PlayerStopGadgetSounds, 0x00000000u},
    {0x8007A304u, "ae_AimTableClear3", call_ae_AimTableClear3, 0x00000000u},
    {0x8007A32Cu, "ae_Gadget3FindAimTarget", call_ae_Gadget3FindAimTarget, 0x00000000u},
    {0x8007A48Cu, "ae_Gadget3LockOnAdd", call_ae_Gadget3LockOnAdd, 0x00000000u},
    {0x8007A514u, "ae_Gadget4PlayerUpdate", call_ae_Gadget4PlayerUpdate, 0x00000000u},
    {0x8007AA1Cu, "ae_GadgetSwingParamsInit", call_ae_GadgetSwingParamsInit, 0x00000000u},
    {0x8007AA64u, "ae_PlayerFxClearDAT_800F5144", call_ae_PlayerFxClearDAT_800F5144, 0x00000000u},
    {0x8007AA98u, "ae_Gadget3AimUpdateAmmo", call_ae_Gadget3AimUpdateAmmo, 0x00000000u},
    {0x8007AC64u, "ae_PlayerSelectGadget", call_ae_PlayerSelectGadget, 0x00000000u},
    {0x8007AFB8u, "ae_AreaSpecialFlagsClear", call_ae_AreaSpecialFlagsClear, 0x00000000u},
    {0x8007AFC4u, "ae_GadgetSlotsInitForArea", call_ae_GadgetSlotsInitForArea, 0x00000000u},
    {0x8007B16Cu, "ae_GadgetSlotsInitForExit", call_ae_GadgetSlotsInitForExit, 0x00000000u},
    {0x8007B358u, "ae_GadgetUnlockMask", call_ae_GadgetUnlockMask, 0x00000000u},
    {0x8007B374u, "ae_PlayerSetMode", call_ae_PlayerSetMode, 0x00000000u},
    {0x8007BC9Cu, "ae_PlayerClearDAT44A2", call_ae_PlayerClearDAT44A2, 0x00000000u},
    {0x8007BCB4u, "ae_MonkeyLodModelsInit", call_ae_MonkeyLodModelsInit, 0x00000000u},
    {0x8007BD38u, "ae_MonkeyNetCatchDraw", call_ae_MonkeyNetCatchDraw, 0x00000000u},
    {0x8007C2A0u, "ae_MonkeyNetCatchUpdate", call_ae_MonkeyNetCatchUpdate, 0x00000000u},
    {0x8007C468u, "ae_PlayerAnimEventAddHitBodies", call_ae_PlayerAnimEventAddHitBodies, 0x00000000u},
    {0x8007C618u, "ae_PlayerRumbleSlam", call_ae_PlayerRumbleSlam, 0x00000000u},
    {0x8007C6CCu, "ae_GadgetSelectFxDraw", call_ae_GadgetSelectFxDraw, 0x00000000u},
    {0x8007C9D8u, "ae_GadgetSlotsSave", call_ae_GadgetSlotsSave, 0x00000000u},
    {0x8007CA10u, "ae_GadgetSlotsRestore", call_ae_GadgetSlotsRestore, 0x00000000u},
    {0x8007CA48u, "ae_PlayerCanActivateGadget", call_ae_PlayerCanActivateGadget, 0x00000000u},
    {0x8007CAE0u, "ae_DrawScaledModelAt", call_ae_DrawScaledModelAt, 0x00000000u},
    {0x8007CD0Cu, "ae_GadgetSetSelected", call_ae_GadgetSetSelected, 0x00000000u},
    {0x8007CD60u, "ae_TargetListClear", call_ae_TargetListClear, 0x00000000u},
    {0x8007CD88u, "ae_TargetListAdd", call_ae_TargetListAdd, 0x00000000u},
    {0x8007CDC4u, "ae_TargetListRemove", call_ae_TargetListRemove, 0x00000000u},
    {0x8007D0F0u, "ae_TableClearDAT_800BEF60", call_ae_TableClearDAT_800BEF60, 0x00000000u},
    {0x8007D118u, "ae_ReadSizedValue", call_ae_ReadSizedValue, 0x00000000u},
    {0x8007D188u, "ae_WriteSizedValue", call_ae_WriteSizedValue, 0x00000000u},
    {0x8007D1E0u, "ae_MenuItemInitBound", call_ae_MenuItemInitBound, 0x00000000u},
    {0x8007D23Cu, "ae_MenuItemInitToggle", call_ae_MenuItemInitToggle, 0x00000000u},
    {0x8007D290u, "ae_MenuItemSetRange", call_ae_MenuItemSetRange, 0x00000000u},
    {0x8007D29Cu, "ae_MenuItemSetCallback", call_ae_MenuItemSetCallback, 0x00000000u},
    {0x8007D2A4u, "ae_MenuItemSetStep", call_ae_MenuItemSetStep, 0x00000000u},
    {0x8007D2ACu, "ae_MenuStrLen", call_ae_MenuStrLen, 0x00000000u},
    {0x8007D2D8u, "ae_MenuWindowAdd", call_ae_MenuWindowAdd, 0x00000000u},
    {0x8007D530u, "ae_MenuWindowsRun", call_ae_MenuWindowsRun, 0x00000000u},
    {0x8007F400u, "ae_ClearDAT_800BEF70", call_ae_ClearDAT_800BEF70, 0x00000000u},
    {0x8007F56Cu, "ae_OpenPauseMenu", call_ae_OpenPauseMenu, 0x00000000u},
    {0x8007F5D4u, "ae_IsPauseMenuOpen", call_ae_IsPauseMenuOpen, 0x00000000u},
    {0x8007F5E4u, "ae_PauseMenuDrawButton", call_ae_PauseMenuDrawButton, 0x00000000u},
    {0x8007F830u, "ae_ScreenWipeReset", call_ae_ScreenWipeReset, 0x00000000u},
    {0x8007F848u, "ae_ScreenSnapshotRestore", call_ae_ScreenSnapshotRestore, 0x00000000u},
    {0x8007F8F4u, "ae_ScreenWipeBlurBlock", call_ae_ScreenWipeBlurBlock, 0x00000000u},
    {0x8007F9D0u, "ae_ScreenWipeStep", call_ae_ScreenWipeStep, 0x00000000u},
    {0x8007FA4Cu, "ae_PauseMenuRun", call_ae_PauseMenuRun, 0x00000000u},
    {0x80081144u, "ae_GadgetSlotRemap", call_ae_GadgetSlotRemap, 0x00000000u},
    {0x80081180u, "ae_HudDrawGadgetSlotsFrame", call_ae_HudDrawGadgetSlotsFrame, 0x00000000u},
    {0x800813D8u, "ae_HudDrawGadgetSlotIcons", call_ae_HudDrawGadgetSlotIcons, 0x00000000u},
    {0x80081664u, "ae_HudDrawGadgetSlots", call_ae_HudDrawGadgetSlots, 0x00000000u},
    {0x8008197Cu, "ae_HudFindGadgetIconPos", call_ae_HudFindGadgetIconPos, 0x00000000u},
    {0x800819E8u, "ae_HudDrawGadgetIcon", call_ae_HudDrawGadgetIcon, 0x00000000u},
    {0x80081B9Cu, "ae_PauseMenuSetLayout", call_ae_PauseMenuSetLayout, 0x00000000u},
    {0x80081D54u, "ae_PauseMenuDrawButtons", call_ae_PauseMenuDrawButtons, 0x00000000u},
    {0x80081F18u, "ae_HudDrawWindowFrame", call_ae_HudDrawWindowFrame, 0x00000000u},
    {0x80082030u, "ae_PauseItemEnabled", call_ae_PauseItemEnabled, 0x00000000u},
    {0x800821D4u, "ae_PauseMenuUpdate", call_ae_PauseMenuUpdate, 0x00000000u},
    {0x8008227Cu, "ae_SetDAT_800BEF80", call_ae_SetDAT_800BEF80, 0x00000000u},
    {0x80082288u, "ae_DrawTextSprites", call_ae_DrawTextSprites, 0x00000000u},
    {0x80083470u, "ae_ItemsLoadForArea", call_ae_ItemsLoadForArea, 0x00000000u},
    {0x800836E0u, "ae_PickupSetPosition", call_ae_PickupSetPosition, 0x00000000u},
    {0x8008373Cu, "ae_PickupPickKindA", call_ae_PickupPickKindA, 0x00000000u},
    {0x80083790u, "ae_PickupPickKindB", call_ae_PickupPickKindB, 0x00000000u},
    {0x80083844u, "ae_PickupSpawnSlot", call_ae_PickupSpawnSlot, 0x00000000u},
    {0x80083AA8u, "ae_SpawnPickups", call_ae_SpawnPickups, 0x00000000u},
    {0x80083CB8u, "ae_PickupsUpdate", call_ae_PickupsUpdate, 0x00000000u},
    {0x8008435Cu, "ae_ItemsDraw", call_ae_ItemsDraw, 0x00000000u},
    {0x80084598u, "ae_ItemsUpdateDraw", call_ae_ItemsUpdateDraw, 0x00000000u},
    {0x800846D8u, "ae_ItemDrawModelEnvMap", call_ae_ItemDrawModelEnvMap, 0x00000000u},
    {0x800848B0u, "ae_StageProgressRestore_Nop2", call_ae_StageProgressRestore_Nop2, 0x00000000u},
    {0x80084904u, "ae_CollectedItemsHas", call_ae_CollectedItemsHas, 0x00000000u},
    {0x80084964u, "ae_CollectedItemsAdd", call_ae_CollectedItemsAdd, 0x00000000u},
    {0x80084A60u, "ae_FxSlotsReset", call_ae_FxSlotsReset, 0x00000000u},
    {0x80084AA0u, "ae_ProjectileSystemsUpdate", call_ae_ProjectileSystemsUpdate, 0x00000000u},
    {0x80084AC8u, "ae_ProjectileAllocSlot", call_ae_ProjectileAllocSlot, 0x00000000u},
    {0x80084B48u, "ae_ProjectileInit", call_ae_ProjectileInit, 0x00000000u},
    {0x80084B74u, "ae_ProjectileSetColor", call_ae_ProjectileSetColor, 0x00000000u},
    {0x80084BB8u, "ae_ProjectileSetByte2", call_ae_ProjectileSetByte2, 0x00000000u},
    {0x80084BC0u, "ae_ProjectileExpire", call_ae_ProjectileExpire, 0x00000000u},
    {0x80084C78u, "ae_ProjectileSetUpdate", call_ae_ProjectileSetUpdate, 0x00000000u},
    {0x80084C94u, "ae_ProjectileRecordsUpdate", call_ae_ProjectileRecordsUpdate, 0x00000000u},
    {0x80085230u, "ae_ProjectileGroupsClear", call_ae_ProjectileGroupsClear, 0x00000000u},
    {0x80085254u, "ae_ProjectileGroupSpawn", call_ae_ProjectileGroupSpawn, 0x00000000u},
    {0x80085630u, "ae_ProjectileGroupSpawnSpread", call_ae_ProjectileGroupSpawnSpread, 0x00000000u},
    {0x800856FCu, "ae_ProjectileGroupsUpdate", call_ae_ProjectileGroupsUpdate, 0x00000000u},
    {0x80086248u, "ae_SoundSetVolumeMode", call_ae_SoundSetVolumeMode, 0x00000000u},
    {0x800862E4u, "ae_SoundSetVolumes", call_ae_SoundSetVolumes, 0x00000000u},
    {0x80086428u, "ae_SoundVolumesDefault", call_ae_SoundVolumesDefault, 0x00000000u},
    {0x800864B4u, "ae_SoundGetVolumes", call_ae_SoundGetVolumes, 0x00000000u},
    {0x800864DCu, "ae_SoundSaveVolumes", call_ae_SoundSaveVolumes, 0x00000000u},
    {0x800864FCu, "ae_SoundRestoreVolumes", call_ae_SoundRestoreVolumes, 0x00000000u},
    {0x80086540u, "ae_SoundLoopsStopAll", call_ae_SoundLoopsStopAll, 0x00000000u},
    {0x80086788u, "ae_SoundLoopsSetPan", call_ae_SoundLoopsSetPan, 0x00000000u},
    {0x8008686Cu, "ae_SoundEmitterUpdate", call_ae_SoundEmitterUpdate, 0x00000000u},
    {0x80086F00u, "ae_SoundAreaKeepsMusic", call_ae_SoundAreaKeepsMusic, 0x00000000u},
    {0x80086FD0u, "ae_SoundStop", call_ae_SoundStop, 0x00000000u},
    {0x80087388u, "ae_BgmStart", call_ae_BgmStart, 0x00000000u},
    {0x8008740Cu, "ae_BgmModeWantsMusic", call_ae_BgmModeWantsMusic, 0x00000000u},
    {0x8008745Cu, "ae_BgmUpdateVolume", call_ae_BgmUpdateVolume, 0x00000000u},
    {0x80087654u, "ae_BgmApplyPlayerEffect", call_ae_BgmApplyPlayerEffect, 0x00000000u},
    {0x800877F8u, "ae_PlaySfx", call_ae_PlaySfx, 0x00000000u},
    {0x800878E0u, "ae_SoundLoopStart", call_ae_SoundLoopStart, 0x00000000u},
    {0x80087A20u, "ae_PlaySfxAt", call_ae_PlaySfxAt, 0x00000000u},
    {0x80087B34u, "ae_SoundEmitterPan", call_ae_SoundEmitterPan, 0x00000000u},
    {0x80087C28u, "ae_SoundEmitterPitch", call_ae_SoundEmitterPitch, 0x00000000u},
    {0x80087C8Cu, "ae_SoundEmitterCalcPan", call_ae_SoundEmitterCalcPan, 0x00000000u},
    {0x80087EE0u, "ae_SoundEmitterAttenuation", call_ae_SoundEmitterAttenuation, 0x00000000u},
    {0x8008806Cu, "ae_SoundLoopStop", call_ae_SoundLoopStop, 0x00000000u},
    {0x800880D0u, "ae_SoundLoopStopAndFree", call_ae_SoundLoopStopAndFree, 0x00000000u},
    {0x80088194u, "ae_SoundLoopsReset", call_ae_SoundLoopsReset, 0x00000000u},
    {0x800881D0u, "ae_SoundLoopAllocSlot", call_ae_SoundLoopAllocSlot, 0x00000000u},
    {0x8008820Cu, "ae_SoundLoopFree", call_ae_SoundLoopFree, 0x00000000u},
    {0x800882C4u, "ae_SoundLoopStartEx", call_ae_SoundLoopStartEx, 0x00000000u},
    {0x80088578u, "ae_SoundDelayedQueueReset", call_ae_SoundDelayedQueueReset, 0x00000000u},
    {0x800885A8u, "ae_SfxIsQueued", call_ae_SfxIsQueued, 0x00000000u},
    {0x800885FCu, "ae_SoundDelayedQueueAdd", call_ae_SoundDelayedQueueAdd, 0x00000000u},
    {0x80088670u, "ae_ColliderZoneMasksUpdate", call_ae_ColliderZoneMasksUpdate, 0x00000000u},
    {0x800889B4u, "ae_GetColliderQuadrantMaskXZ", call_ae_GetColliderQuadrantMaskXZ, 0x00000000u},
    {0x800889ECu, "ae_ColliderComputeQuadrantMask", call_ae_ColliderComputeQuadrantMask, 0x00000000u},
    {0x80088AB8u, "ae_ColliderScriptsUpdate", call_ae_ColliderScriptsUpdate, 0x00000000u},
    {0x800892C4u, "ae_ColliderScriptFollowPath", call_ae_ColliderScriptFollowPath, 0x00000000u},
    {0x8008A4B0u, "ae_ColliderScriptPlayerMove", call_ae_ColliderScriptPlayerMove, 0x00000000u},
    {0x8008A91Cu, "ae_ColliderScriptCarry", call_ae_ColliderScriptCarry, 0x00000000u},
    {0x8008AA6Cu, "ae_GimmickCarryBodiesByDelta", call_ae_GimmickCarryBodiesByDelta, 0x00000000u},
    {0x8008ABB0u, "ae_GimmickCarryBodiesSwing", call_ae_GimmickCarryBodiesSwing, 0x00000000u},
    {0x8008AD14u, "ae_GimmickSwingCarryPoint", call_ae_GimmickSwingCarryPoint, 0x00000000u},
    {0x8008AF98u, "ae_GimmickSwingAngle", call_ae_GimmickSwingAngle, 0x00000000u},
    {0x8008B060u, "ae_GimmickSwingFirstStep", call_ae_GimmickSwingFirstStep, 0x00000000u},
    {0x8008B228u, "ae_GimmickSwingAdvance", call_ae_GimmickSwingAdvance, 0x00000000u},
    {0x8008B3A4u, "ae_GimmickSwingProbeWall", call_ae_GimmickSwingProbeWall, 0x00000000u},
    {0x8008B7F8u, "ae_ColliderScriptRun", call_ae_ColliderScriptRun, 0x00000000u},
    {0x8008B980u, "ae_ColliderScriptCommand", call_ae_ColliderScriptCommand, 0x00000000u},
    {0x8008BDB4u, "ae_ColliderResetToStart", call_ae_ColliderResetToStart, 0x00000000u},
    {0x8008C000u, "ae_BumpAllocPeek", call_ae_BumpAllocPeek, 0x00000000u},
    {0x8008C01Cu, "ae_HeapInit", call_ae_HeapInit, 0x00000000u},
    {0x8008C058u, "ae_AreaBumpAllocSet", call_ae_AreaBumpAllocSet, 0x00000000u},
    {0x8008C064u, "ae_HeapFree", call_ae_HeapFree, 0x00000000u},
    {0x8008C09Cu, "ae_HeapAlloc", call_ae_HeapAlloc, 0x00000000u},
    {0x8008C1B8u, "ae_AreaBumpAlloc", call_ae_AreaBumpAlloc, 0x00000000u},
    {0x8008C348u, "ae_SmallocError", call_ae_SmallocError, 0x00000000u},
    {0x8008C390u, "ae_ColorFadeReset", call_ae_ColorFadeReset, 0x00000000u},
    {0x8008C3C0u, "ae_ColorFadeSet", call_ae_ColorFadeSet, 0x00000000u},
    {0x8008C6F0u, "ae_DebugTextPrintf", call_ae_DebugTextPrintf, 0x00000000u},
    {0x8008CBFCu, "ae_DebugTextPrintfWhite", call_ae_DebugTextPrintfWhite, 0x00000000u},
    {0x8008D064u, "ae_TimListResidentUpdate", call_ae_TimListResidentUpdate, 0x00000000u},
    {0x8008D1F0u, "ae_Mode_06", call_ae_Mode_06, 0x00000000u},
    {0x8008D9D0u, "ae_FontDrawGlyph", call_ae_FontDrawGlyph, 0x00000000u},
    {0x8008DD68u, "ae_TextNextGlyph", call_ae_TextNextGlyph, 0x00000000u},
    {0x8008DDB0u, "ae_TextSetCursor", call_ae_TextSetCursor, 0x00000000u},
    {0x8008DDC8u, "ae_MessageBoxDrawString", call_ae_MessageBoxDrawString, 0x00000000u},
    {0x8008DE68u, "ae_TextWidthUntilGlyph", call_ae_TextWidthUntilGlyph, 0x00000000u},
    {0x8008DEE0u, "ae_MessageBoxClose", call_ae_MessageBoxClose, 0x00000000u},
    {0x8008DF20u, "ae_MessageBoxSetText", call_ae_MessageBoxSetText, 0x00000000u},
    {0x8008DF3Cu, "ae_MessageBoxBusy", call_ae_MessageBoxBusy, 0x00000000u},
    {0x8008DF4Cu, "ae_MessageBoxUpdateDraw", call_ae_MessageBoxUpdateDraw, 0x00000000u},
    {0x8008DF80u, "ae_MessageBoxDrawText", call_ae_MessageBoxDrawText, 0x00000000u},
    {0x8008DFFCu, "ae_MessageBoxDrawPrompt", call_ae_MessageBoxDrawPrompt, 0x00000000u},
    {0x8008E23Cu, "ae_MessageBoxDrawWindow", call_ae_MessageBoxDrawWindow, 0x00000000u},
    {0x8008E5C0u, "ae_Gimmick_SwingDoor_Init", call_ae_Gimmick_SwingDoor_Init, 0x00000000u},
    {0x8008E90Cu, "ae_Gimmick_SwingDoor_Update", call_ae_Gimmick_SwingDoor_Update, 0x00000000u},
    {0x8008EAE4u, "ae_GimmickPlayTypeSfx", call_ae_GimmickPlayTypeSfx, 0x00000000u},
    {0x8008EB58u, "ae_GimmickAnyBodyInside", call_ae_GimmickAnyBodyInside, 0x00000000u},
    {0x8008ECFCu, "ae_SwingDoorSetPivot", call_ae_SwingDoorSetPivot, 0x00000000u},
    {0x8008EF60u, "ae_Gimmick_LiftDoor_Init", call_ae_Gimmick_LiftDoor_Init, 0x00000000u},
    {0x8008F0BCu, "ae_Gimmick_LiftDoor_Update", call_ae_Gimmick_LiftDoor_Update, 0x00000000u},
    {0x8008F468u, "ae_Gimmick_Init_ThresholdDoor", call_ae_Gimmick_Init_ThresholdDoor, 0x00000000u},
    {0x8008F53Cu, "ae_Gimmick_ThresholdDoor_Update", call_ae_Gimmick_ThresholdDoor_Update, 0x00000000u},
    {0x8008F720u, "ae_Gimmick_SlideDoor_Init", call_ae_Gimmick_SlideDoor_Init, 0x00000000u},
    {0x8008F8D4u, "ae_Gimmick_SlideDoor_Update", call_ae_Gimmick_SlideDoor_Update, 0x00000000u},
    {0x8008FC30u, "ae_GimmickActivate", call_ae_GimmickActivate, 0x00000000u},
    {0x80090230u, "ae_GimmickDeactivateChain", call_ae_GimmickDeactivateChain, 0x00000000u},
    {0x80090370u, "ae_Gimmick_Switch_Init", call_ae_Gimmick_Switch_Init, 0x00000000u},
    {0x80090500u, "ae_Gimmick_Switch_Update", call_ae_Gimmick_Switch_Update, 0x00000000u},
    {0x800905A4u, "ae_Gimmick_Init_PairToggle", call_ae_Gimmick_Init_PairToggle, 0x00000000u},
    {0x80090660u, "ae_Gimmick_Hit_Update", call_ae_Gimmick_Hit_Update, 0x00000000u},
    {0x800909A8u, "ae_Gimmick_Init_Counter", call_ae_Gimmick_Init_Counter, 0x00000000u},
    {0x800909FCu, "ae_Gimmick_SpinRoller_Update", call_ae_Gimmick_SpinRoller_Update, 0x00000000u},
    {0x80090D6Cu, "ae_Gimmick_Init_MonkeyQuota", call_ae_Gimmick_Init_MonkeyQuota, 0x00000000u},
    {0x80090D94u, "ae_Gimmick_MonkeyQuota_Update", call_ae_Gimmick_MonkeyQuota_Update, 0x00000000u},
    {0x80090EB0u, "ae_Gimmick_NopInit", call_ae_Gimmick_NopInit, 0x00000000u},
    {0x80090EB8u, "ae_Gimmick_NopUpdate", call_ae_Gimmick_NopUpdate, 0x00000000u},
    {0x80090EC0u, "ae_Gimmick_Init_Closed", call_ae_Gimmick_Init_Closed, 0x00000000u},
    {0x80090F0Cu, "ae_Gimmick_Init_ZoneMask", call_ae_Gimmick_Init_ZoneMask, 0x00000000u},
    {0x80090F80u, "ae_Gimmick_PlayerInCylinder_Update", call_ae_Gimmick_PlayerInCylinder_Update, 0x00000000u},
    {0x80091060u, "ae_Gimmick_ProxTrigger_Init", call_ae_Gimmick_ProxTrigger_Init, 0x00000000u},
    {0x80091178u, "ae_Gimmick_ProxTrigger_Update", call_ae_Gimmick_ProxTrigger_Update, 0x00000000u},
    {0x80091358u, "ae_Gimmick_Breakable_Init", call_ae_Gimmick_Breakable_Init, 0x00000000u},
    {0x800913ECu, "ae_Gimmick_Breakable_Update", call_ae_Gimmick_Breakable_Update, 0x00000000u},
    {0x80091688u, "ae_Gimmick_Init_Closed2", call_ae_Gimmick_Init_Closed2, 0x00000000u},
    {0x800916FCu, "ae_Gimmick_NopUpdate2", call_ae_Gimmick_NopUpdate2, 0x00000000u},
    {0x80091704u, "ae_Gimmick_Init_PropHidden", call_ae_Gimmick_Init_PropHidden, 0x00000000u},
    {0x8009176Cu, "ae_Gimmick_Resize_Update", call_ae_Gimmick_Resize_Update, 0x00000000u},
    {0x80091870u, "ae_Gimmick_Init_Closed3", call_ae_Gimmick_Init_Closed3, 0x00000000u},
    {0x800918FCu, "ae_Gimmick_Slider_Update", call_ae_Gimmick_Slider_Update, 0x00000000u},
    {0x80091A18u, "ae_Gimmick_SliderQuiet_Update", call_ae_Gimmick_SliderQuiet_Update, 0x00000000u},
    {0x80091CF8u, "ae_Gimmick_PlayerBox_Update", call_ae_Gimmick_PlayerBox_Update, 0x00000000u},
    {0x80091D24u, "ae_GimmickPlayerInBox", call_ae_GimmickPlayerInBox, 0x00000000u},
    {0x80091FB0u, "ae_Gimmick_Init_RecordOrigin", call_ae_Gimmick_Init_RecordOrigin, 0x00000000u},
    {0x800920ACu, "ae_Gimmick_LinkedLift_Update", call_ae_Gimmick_LinkedLift_Update, 0x00000000u},
    {0x80092A68u, "ae_Gimmick_Init_PropTilt", call_ae_Gimmick_Init_PropTilt, 0x00000000u},
    {0x80092B0Cu, "ae_Gimmick_Oscillate_Update", call_ae_Gimmick_Oscillate_Update, 0x00000000u},
    {0x80092E74u, "ae_Gimmick_Init_PropMeshSet", call_ae_Gimmick_Init_PropMeshSet, 0x00000000u},
    {0x80092F50u, "ae_Gimmick_Init_CounterGate", call_ae_Gimmick_Init_CounterGate, 0x00000000u},
    {0x80092FBCu, "ae_Gimmick_Counter_EventCam_Update", call_ae_Gimmick_Counter_EventCam_Update, 0x00000000u},
    {0x800935F0u, "ae_Gimmick_PlayerBlast_Update", call_ae_Gimmick_PlayerBlast_Update, 0x00000000u},
    {0x80093740u, "ae_Gimmick_Init_Disabled", call_ae_Gimmick_Init_Disabled, 0x00000000u},
    {0x80093788u, "ae_Gimmick_Grab_Update", call_ae_Gimmick_Grab_Update, 0x00000000u},
    {0x8009391Cu, "ae_Gimmick_Init_PropFlag2000", call_ae_Gimmick_Init_PropFlag2000, 0x00000000u},
    {0x8009398Cu, "ae_Gimmick_Bounce_Update", call_ae_Gimmick_Bounce_Update, 0x00000000u},
    {0x80093E74u, "ae_Gimmick_Init_Hidden", call_ae_Gimmick_Init_Hidden, 0x00000000u},
    {0x80093F04u, "ae_Gimmick_Rotate_Update", call_ae_Gimmick_Rotate_Update, 0x00000000u},
    {0x8009468Cu, "ae_Gimmick_Falling_Update", call_ae_Gimmick_Falling_Update, 0x00000000u},
    {0x80094B6Cu, "ae_GimmickMoveColliderTo", call_ae_GimmickMoveColliderTo, 0x00000000u},
    {0x80094C10u, "ae_GimmickMoveCollider", call_ae_GimmickMoveCollider, 0x00000000u},
    {0x80094F20u, "ae_StageRequestExit", call_ae_StageRequestExit, 0x00000000u},
    {0x80094F40u, "ae_Gimmick_NopUpdate3", call_ae_Gimmick_NopUpdate3, 0x00000000u},
    {0x80095244u, "ae_HudPanelA_Start", call_ae_HudPanelA_Start, 0x00000000u},
    {0x8009529Cu, "ae_HudPanelA_Update", call_ae_HudPanelA_Update, 0x00000000u},
    {0x80095464u, "ae_Gimmick_NopUpdate4", call_ae_Gimmick_NopUpdate4, 0x00000000u},
    {0x800955C8u, "ae_StageRequestRetry", call_ae_StageRequestRetry, 0x00000000u},
    {0x80095600u, "ae_HudPanelB_Start", call_ae_HudPanelB_Start, 0x00000000u},
    {0x80095690u, "ae_HudPanelB_Update", call_ae_HudPanelB_Update, 0x00000000u},
    {0x800958C0u, "ae_HudCounterC_Start", call_ae_HudCounterC_Start, 0x00000000u},
    {0x80095924u, "ae_HudCounterC_Update", call_ae_HudCounterC_Update, 0x00000000u},
    {0x80095B3Cu, "ae_SaveBufferFill", call_ae_SaveBufferFill, 0x00000000u},
    {0x80095CECu, "ae_SaveCaptureState", call_ae_SaveCaptureState, 0x00000000u},
    {0x80095D44u, "ae_SaveHeaderFill", call_ae_SaveHeaderFill, 0x00000000u},
    {0x800961D0u, "ae_RecordBestTime", call_ae_RecordBestTime, 0x00000000u},
    {0x8009628Cu, "ae_SaveBlockPtr", call_ae_SaveBlockPtr, 0x00000000u},
    {0x800962A4u, "ae_GimmickStateTablePtr", call_ae_GimmickStateTablePtr, 0x00000000u},
    {0x800962E8u, "ae_GetCollectedItemsTable", call_ae_GetCollectedItemsTable, 0x00000000u},
    {0x8009632Cu, "ae_ProgressFlagGet", call_ae_ProgressFlagGet, 0x00000000u},
    {0x800963ACu, "ae_ProgressGetByte", call_ae_ProgressGetByte, 0x00000000u},
    {0x800963DCu, "ae_ProgressMarkStageResult", call_ae_ProgressMarkStageResult, 0x00000000u},
    {0x80096514u, "ae_AreaKindVisitedGet", call_ae_AreaKindVisitedGet, 0x00000000u},
    {0x80096580u, "ae_LevelProgressGet", call_ae_LevelProgressGet, 0x00000000u},
    {0x800966B4u, "ae_StageProgressOnEnter", call_ae_StageProgressOnEnter, 0x00000000u},
    {0x80096E0Cu, "ae_StageProgressStoreArea", call_ae_StageProgressStoreArea, 0x00000000u},
    {0x80096FACu, "ae_StageProgressOnExit", call_ae_StageProgressOnExit, 0x00000000u},
    {0x800972E0u, "ae_StageProgressRestore", call_ae_StageProgressRestore, 0x00000000u},
    {0x8009764Cu, "ae_GetLevelFirstArea", call_ae_GetLevelFirstArea, 0x00000000u},
    {0x80097678u, "ae_IsTimeAttack", call_ae_IsTimeAttack, 0x00000000u},
    {0x800976B8u, "ae_HubDestinationState", call_ae_HubDestinationState, 0x00000000u},
    {0x800977BCu, "ae_HubGateLocked", call_ae_HubGateLocked, 0x00000000u},
    {0x80097820u, "ae_UnlockSpecialArea", call_ae_UnlockSpecialArea, 0x00000000u},
    {0x800978E0u, "ae_HubGateBitTest", call_ae_HubGateBitTest, 0x00000000u},
    {0x800978FCu, "ae_ExitSpecialArea", call_ae_ExitSpecialArea, 0x00000000u},
    {0x80097E88u, "ae_AreaGetMonkeyRecord", call_ae_AreaGetMonkeyRecord, 0x00000000u},
    {0x80098398u, "ae_HudPopupStartPickup", call_ae_HudPopupStartPickup, 0x00000000u},
    {0x800983E8u, "ae_HudPopupUpdate", call_ae_HudPopupUpdate, 0x00000000u},
    {0x800986E0u, "ae_Gadget4UpdateDraw", call_ae_Gadget4UpdateDraw, 0x00000000u},
    {0x80098DD0u, "ae_Gadget4BuildMatrix", call_ae_Gadget4BuildMatrix, 0x00000000u},
    {0x80098FECu, "ae_PlayerHeadLookUpdate", call_ae_PlayerHeadLookUpdate, 0x00000000u},
    {0x80099360u, "ae_RcCarSpawn", call_ae_RcCarSpawn, 0x00000000u},
    {0x8009946Cu, "ae_RcCarDespawn", call_ae_RcCarDespawn, 0x00000000u},
    {0x800994A8u, "ae_RcCarUpdate", call_ae_RcCarUpdate, 0x00000000u},
    {0x80099AE0u, "ae_RcCarDraw", call_ae_RcCarDraw, 0x00000000u},
    {0x80099C44u, "ae_GadgetDrawStaticModel", call_ae_GadgetDrawStaticModel, 0x00000000u},
    {0x80099D20u, "ae_RadarResetState", call_ae_RadarResetState, 0x00000000u},
    {0x80099D70u, "ae_RadarUpdateDraw", call_ae_RadarUpdateDraw, 0x00000000u},
    {0x8009A17Cu, "ae_MonkeyFlag248Set", call_ae_MonkeyFlag248Set, 0x00000000u},
    {0x8009A1A4u, "ae_MonkeyRadarSignal", call_ae_MonkeyRadarSignal, 0x00000000u},
    {0x8009A4E8u, "ae_GadgetDrawWithStateCheck", call_ae_GadgetDrawWithStateCheck, 0x00000000u},
    {0x8009A574u, "ae_RadarDrawModel", call_ae_RadarDrawModel, 0x00000000u},
    {0x8009A800u, "ae_GadgetSwingUpdate", call_ae_GadgetSwingUpdate, 0x00000000u},
    {0x8009ABF4u, "ae_GadgetDrawSlot0Attachment", call_ae_GadgetDrawSlot0Attachment, 0x00000000u},
    {0x8009AD60u, "ae_Gadget3UpdateDraw", call_ae_Gadget3UpdateDraw, 0x00000000u},
    {0x8009B1C0u, "ae_Gadget6UpdateSound", call_ae_Gadget6UpdateSound, 0x00000000u},
    {0x8009B3CCu, "ae_AreaSetSpecialActor", call_ae_AreaSetSpecialActor, 0x00000000u},
    {0x8009B6C8u, "ae_SpecialActorPlayerNear", call_ae_SpecialActorPlayerNear, 0x00000000u},
    {0x8009B7A0u, "ae_AreaFeature30GetState", call_ae_AreaFeature30GetState, 0x00000000u},
    {0x8009B7C8u, "ae_AreaSetFeature30", call_ae_AreaSetFeature30, 0x00000000u},
    {0x8009B7E4u, "ae_AreaSetFeature31", call_ae_AreaSetFeature31, 0x00000000u},
    {0x8009B7F0u, "ae_AreaSetFeature32", call_ae_AreaSetFeature32, 0x00000000u},
    {0x8009B7FCu, "ae_AreaFeature30Update", call_ae_AreaFeature30Update, 0x00000000u},
    {0x8009E82Cu, "ae_SfxLookupTone", call_ae_SfxLookupTone, 0x00000000u},
    {0x8009E940u, "ae_SfxVoiceAlloc", call_ae_SfxVoiceAlloc, 0x00000000u},
    {0x800A0900u, "ae_SeqUpdateVoiceVolume", call_ae_SeqUpdateVoiceVolume, 0x00000000u},
    {0x800A0A64u, "ae_SeqVoiceVolumeSide", call_ae_SeqVoiceVolumeSide, 0x00000000u},
    {0x800A0DB0u, "ae_SeqFadeInit", call_ae_SeqFadeInit, 0x00000000u},
    {0x800A1174u, "ae_Max16", call_ae_Max16, 0x00000000u},
    {0x800A119Cu, "ae_SpuVolumeClamp", call_ae_SpuVolumeClamp, 0x00000000u},
    {0x800A1378u, "ae_SeqSetTickFlag", call_ae_SeqSetTickFlag, 0x00000000u},
    {0x800A1DB0u, "ae_BgmSeqOpen", call_ae_BgmSeqOpen, 0x00000000u},
    {0x800A2428u, "ae_BgmSeqSetVolume", call_ae_BgmSeqSetVolume, 0x00000000u},
    {0x800A25CCu, "ae_SeqFadeTrackProgram", call_ae_SeqFadeTrackProgram, 0x00000000u},
    {0x800A2A14u, "ae_SpuSetMainVolume", call_ae_SpuSetMainVolume, 0x00000000u},
    {0x800A2ACCu, "ae_SeqTrackRearm", call_ae_SeqTrackRearm, 0x00000000u},
    {0x800A2E3Cu, "ae_SeqFadeTrackIn", call_ae_SeqFadeTrackIn, 0x00000000u},
    {0x800A30D8u, "ae_SeqGetChannelProgram", call_ae_SeqGetChannelProgram, 0x00000000u},
    {0x800A3158u, "ae_SfxVoiceStart", call_ae_SfxVoiceStart, 0x00000000u},
    {0x800A31B8u, "ae_SfxVoiceStartEx", call_ae_SfxVoiceStartEx, 0x00000000u},
    {0x800A329Cu, "ae_SfxVoiceStop", call_ae_SfxVoiceStop, 0x00000000u},
    {0x800A3570u, "ae_SfxVoiceSetPanVol", call_ae_SfxVoiceSetPanVol, 0x00000000u},
    {0x800A3700u, "ae_exit", call_ae_exit, 0x00000000u},
    {0x800A3720u, "ae_printf", call_ae_printf, 0x00000000u},
    {0x80136000u, "ae_StageClearUi_LookAt_18B", call_ae_StageClearUi_LookAt_18B, 0x0000018Bu},
    {0x801361D8u, "ae_SaveUi_StringTableIndex_18B", call_ae_SaveUi_StringTableIndex_18B, 0x0000018Bu},
    {0x80136208u, "ae_Mode_SpecialAreaEnter", call_ae_Mode_SpecialAreaEnter, 0x0000018Bu},
    {0x80136954u, "ae_SpecialEnterUi_DrawWipe", call_ae_SpecialEnterUi_DrawWipe, 0x0000018Bu},
    {0x80137214u, "ae_SpecialEnterUi_DrawText", call_ae_SpecialEnterUi_DrawText, 0x0000018Bu},
    {0x80137334u, "ae_SpecialEnterUi_DrawMessageShadow", call_ae_SpecialEnterUi_DrawMessageShadow, 0x0000018Bu},
    {0x801373A4u, "ae_SpecialEnterUi_DrawBlink", call_ae_SpecialEnterUi_DrawBlink, 0x0000018Bu},
    {0x801374D0u, "ae_SpecialEnterUi_RunScript", call_ae_SpecialEnterUi_RunScript, 0x0000018Bu},
    {0x80137CD4u, "ae_StageClearUi_SetDrawStp_18B", call_ae_StageClearUi_SetDrawStp_18B, 0x0000018Bu},
    {0x80137D4Cu, "ae_StageClearUi_CopyStrip_18B", call_ae_StageClearUi_CopyStrip_18B, 0x0000018Bu},
    {0x80137DF0u, "ae_SpecialEnterUi_UpdateObjects", call_ae_SpecialEnterUi_UpdateObjects, 0x0000018Bu},
    {0x801380D8u, "ae_SpecialEnterUi_DrawScene", call_ae_SpecialEnterUi_DrawScene, 0x0000018Bu},
    {0x801383B8u, "ae_SpecialEnterUi_SetAreaTablePointer", call_ae_SpecialEnterUi_SetAreaTablePointer, 0x0000018Bu},
    {0x801383D8u, "ae_SpecialEnterUi_TextWidth", call_ae_SpecialEnterUi_TextWidth, 0x0000018Bu},
    {0x80138468u, "ae_SpecialEnterUi_PlayXa", call_ae_SpecialEnterUi_PlayXa, 0x0000018Bu},
    {0x80138490u, "ae_StageClearUi_DrawScanlines_18B", call_ae_StageClearUi_DrawScanlines_18B, 0x0000018Bu},
    {0x8013D000u, "ae_Area02_SpawnAmbientParticle", call_ae_Area02_SpawnAmbientParticle, 0x0000018Cu},
    {0x8013D0B0u, "ae_Area02_InterpolateAnimValue", call_ae_Area02_InterpolateAnimValue, 0x0000018Cu},
    {0x8013D1ACu, "ae_FUN_8013d1ac", call_ae_FUN_8013d1ac, 0xFFFFFFFFu},
    {0x8013D1C4u, "ae_Area02_PlayerWithinHeightAndRadius", call_ae_Area02_PlayerWithinHeightAndRadius, 0x0000018Cu},
    {0x8013D224u, "ae_Area02_PlayerOutsideHeightAndRadius", call_ae_Area02_PlayerOutsideHeightAndRadius, 0x0000018Cu},
    {0x8013D294u, "ae_Area02_EnemyAnimationTriggerUpdate", call_ae_Area02_EnemyAnimationTriggerUpdate, 0x0000018Cu},
    {0x8013D3B8u, "ae_Area02_UpdateEnemyWakeOrigin", call_ae_Area02_UpdateEnemyWakeOrigin, 0x0000018Cu},
    {0x8013D430u, "ae_Area02_UpdateEnemyWake", call_ae_Area02_UpdateEnemyWake, 0x0000018Cu},
    {0x8013D518u, "ae_Area02_EnemyType0B_Update", call_ae_Area02_EnemyType0B_Update, 0x0000018Cu},
    {0x8013D660u, "ae_Area02_CopyEnemyModel", call_ae_Area02_CopyEnemyModel, 0x0000018Cu},
    {0x8013D7C8u, "ae_Area02_InitializeEffects", call_ae_Area02_InitializeEffects, 0x0000018Cu},
    {0x8013D81Cu, "ae_Area02_FindFreeEffectSlot", call_ae_Area02_FindFreeEffectSlot, 0x0000018Cu},
    {0x8013D858u, "ae_Area02_SpawnRadialEffect", call_ae_Area02_SpawnRadialEffect, 0x0000018Cu},
    {0x8013D990u, "ae_Area02_EffectBelowGround", call_ae_Area02_EffectBelowGround, 0x0000018Cu},
    {0x8013D9CCu, "ae_Area02_SpawnEffectParticle", call_ae_Area02_SpawnEffectParticle, 0x0000018Cu},
    {0x8013DA0Cu, "ae_Area02_SpawnEffectBurst", call_ae_Area02_SpawnEffectBurst, 0x0000018Cu},
    {0x8013DA10u, "ae_FUN_8013da10", call_ae_FUN_8013da10, 0xFFFFFFFFu},
    {0x8013DA94u, "ae_Area02_UpdateEffects", call_ae_Area02_UpdateEffects, 0x0000018Cu},
    {0x8013DAE8u, "ae_FUN_8013dae8", call_ae_FUN_8013dae8, 0xFFFFFFFFu},
    {0x8013DC68u, "ae_FUN_8013dc68", call_ae_FUN_8013dc68, 0xFFFFFFFFu},
    {0x8013DC70u, "ae_Area02_SpawnEnemyHitParticle", call_ae_Area02_SpawnEnemyHitParticle, 0x0000018Cu},
    {0x8013DC9Cu, "ae_Area02_EnemyHitUpdate", call_ae_Area02_EnemyHitUpdate, 0x0000018Cu},
    {0x8013DD2Cu, "ae_Area02_EnemyEnterHitReaction", call_ae_Area02_EnemyEnterHitReaction, 0x0000018Cu},
    {0x8013DDBCu, "ae_Area02_EnemySplashUpdate", call_ae_Area02_EnemySplashUpdate, 0x0000018Cu},
    {0x8013DE20u, "ae_Area02_EnemyEnterDefeat", call_ae_Area02_EnemyEnterDefeat, 0x0000018Cu},
    {0x8013DEF8u, "ae_Area02_EnemyPhase6Update", call_ae_Area02_EnemyPhase6Update, 0x0000018Cu},
    {0x8013DF54u, "ae_Area02_StageSpecificEnemyUpdate", call_ae_Area02_StageSpecificEnemyUpdate, 0x0000018Cu},
    {0x8013DF90u, "ae_FUN_8013df90", call_ae_FUN_8013df90, 0xFFFFFFFFu},
    {0x8013E1E0u, "ae_Area02_SurfaceEffectsInit", call_ae_Area02_SurfaceEffectsInit, 0x0000018Cu},
    {0x8013E33Cu, "ae_Area02_EffectDraw", call_ae_Area02_EffectDraw, 0x0000018Cu},
    {0x8013E950u, "ae_Area02_SurfaceDraw", call_ae_Area02_SurfaceDraw, 0x0000018Cu},
    {0x8013F0C0u, "ae_FUN_8013f0c0", call_ae_FUN_8013f0c0, 0x0000018Cu},
};

size_t ae_game_call_count(void) { return sizeof(CALLS) / sizeof(CALLS[0]); }
const AeGameCall *ae_game_call_at(size_t index) {
    return index < ae_game_call_count() ? &CALLS[index] : NULL;
}
const AeGameCall *ae_game_find_call(uint32_t address) {
    size_t lo = 0, hi = ae_game_call_count();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (CALLS[mid].address < address) lo = mid + 1; else hi = mid;
    }
    return lo < ae_game_call_count() && CALLS[lo].address == address ? &CALLS[lo] : NULL;
}
int32_t ae_game_call(AeGame *g, uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeGameCall *call = ae_game_find_call(address);
    if (call && address >= 0x80100000u) {
        size_t index = (size_t)(call - CALLS);
        call = NULL;
        for (; index < ae_game_call_count() && CALLS[index].address == address; ++index)
            if (CALLS[index].overlay_member == g->loaded_overlay) { call = &CALLS[index]; break; }
    }
    /* Overlay address alone cannot identify the loaded code. */
    if (!call || (address >= 0x80100000u && (call->overlay_member == UINT32_MAX || call->overlay_member != g->loaded_overlay)))
        return ae_game_missing_call(g, call ? call->name : "UnknownFunction", address, a0, a1, a2, a3);
    return call->invoke(g, a0, a1, a2, a3);
}
