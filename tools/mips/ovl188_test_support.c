/* Link-only fail-closed stubs for unrelated functions from the large port units.
 * The overlay checker links those units with section GC so the functions used by
 * member 188 remain the production implementations.  If a case reaches one of
 * these unrelated entry points, the RAM image is marked failed. */
#include "../../src/core/ae_ram.h"

#define O188_UNEXPECTED(name) \
    int32_t name(AeRam *ram, ...) { \
        ae_ram_fail(ram, "unexpected test-link stub: " #name); \
        return 0; \
    }

O188_UNEXPECTED(ae_Gadget4TrailReset)
O188_UNEXPECTED(ae_CameraNudge)
O188_UNEXPECTED(ae_PlayerSetMode)
O188_UNEXPECTED(ae_GimmickCarryBodiesByDelta)
O188_UNEXPECTED(ae_GimmickCarryBodiesSwing)
O188_UNEXPECTED(ae_ColliderComputeQuadrantMask)
O188_UNEXPECTED(ae_GimmickSwingAdvance)
O188_UNEXPECTED(ae_GimmickSwingFirstStep)
O188_UNEXPECTED(ae_GetGroundHeight)
O188_UNEXPECTED(ae_player_vec_length3)
O188_UNEXPECTED(ae_PlayerMoveRumbleSoundUpdate)
O188_UNEXPECTED(ae_BodyProbeGround)
O188_UNEXPECTED(ae_CollisionStub_ReturnZero)
O188_UNEXPECTED(ae_CollisionBuildProbePoints)
O188_UNEXPECTED(ae_CollisionResolveProbes)
O188_UNEXPECTED(ae_BodyLandOnGround)
O188_UNEXPECTED(ae_PlayerTryLedgeGrab)
O188_UNEXPECTED(ae_BodyRecordColliderContact)
O188_UNEXPECTED(ae_PlayerVehicleSoundUpdate)
O188_UNEXPECTED(ae_CameraSlotCreate)
O188_UNEXPECTED(ae_AreaLoadQueueRead)
O188_UNEXPECTED(ae_StageRequest)
O188_UNEXPECTED(ae_HudPanelB_Start)
O188_UNEXPECTED(ae_player_is_swimming)
O188_UNEXPECTED(ae_player_can_switch_gadget)
O188_UNEXPECTED(ae_PlayerCanExitStage)
O188_UNEXPECTED(ae_AreaFeature30GetState)
O188_UNEXPECTED(ae_IsTimeAttack)
