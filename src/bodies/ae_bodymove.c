/*
 * Player / RC-car collision body movement and its original callees.
 *
 * These functions run on AeRam and keep the EXE's RAM and scratchpad layout. Verified game ports are called directly;
 * native-system operations and the named collision-probe/game callbacks remain hooks at their existing boundaries.
 */
#include "ae_bodymove.h"

#include "ae_probe.h"
#include "../collision/ae_collprobe.h"
#include "../core/ae_sdk.h"
#include "../effects/ae_spawn.h"
#include "../player/ae_player1.h"
#include "../player/ae_player_act.h"
#include "../player/ae_player_spawn.h"
#include "../sound/ae_bgm.h"
#include "../sound/ae_sound.h"
#include "../world/ae_areaset.h"
#include "../world/ae_misc.h"
#include "../world/ae_solid.h"
#include "../world/ae_world.h"

#include <limits.h>

void ae_PadRumble(AeRam *r, const AeCamupdHooks *h,
                  int32_t port, uint32_t small_motor, uint32_t large_motor);
int32_t ae_CollProbe_Slope(AeRam *r, const AeCollProbeHooks *h,
                           uint32_t shape, uint32_t probe, uint32_t out);

/* ---- original globals and record fields ------------------------------------------------------ */
#define AE_G_BODY_PROBE_GRID         AE_G_COLL_PROBE_POINTS
#define AE_G_BODY_LAND_FLAGS         AE_G_COLL_PROBE_E8
#define AE_G_PLAYER_AUDIO_FLAGS      0x800EC350u
#define AE_G_PLAYER_POS_X            0x800EC204u
#define AE_G_PLAYER_POS_Y            0x800EC208u
#define AE_G_PLAYER_POS_Z            0x800EC20Cu
#define AE_G_PLAYER_FACING           0x800EC240u
#define AE_G_PLAYER_SLOPE_X          0x800EC2E4u
#define AE_G_BODY_RUMBLE_STRENGTH    0x800B213Cu
#define AE_G_BODY_RUMBLE_TIMER       0x800B213Eu
#define AE_G_BODY_RUMBLE_GATE        0x800E53C0u
#define AE_G_RUMBLE_SOUND_SLOT       0x800B20D4u
#define AE_G_RUMBLE_SOUND_ACTIVE     0x800B20D6u
#define AE_G_BODY_VEHICLE_PITCH      0x800B2134u
#define AE_G_BODY_VEHICLE_SPEED2     0x800B2138u
#define AE_G_BODY_COLLISION_MARKER   0x800B20C6u
#define AE_G_BODY_RECORD_POINT_PTR   0x800B20B4u
#define AE_G_BODY_PUSH_OUT           0x1F800118u
#define AE_G_BODY_RESOLVE_OUTPUT     0x1F800100u
#define AE_G_BODY_RESOLVE_INPUT      0x1F8000F0u
#define AE_SCR_BODY_RECORD_POINT     0x1F8000FCu
#define AE_G_BODY_RECORD_OUT         0x1F800108u
#define AE_G_BODY_GROUND_Y           0x1F8000FCu
#define AE_SCR_GROUND_SOLID_KIND     0x1F8003F0u
#define AE_SCR_GROUND_SOLID_SHAPE    0x1F8003F4u
#define AE_SCR_GROUND_SOLID_TYPE     0x1F8003F6u
#define AE_SCR_GROUND_SURFACE_FLAGS  0x1F8003F8u
#define AE_PLAYER_STATE              0x5Eu
#define AE_PLAYER_CONTACT_KIND       0x50u
#define AE_PLAYER_STATE_FLAGS        0x70u
#define AE_PLAYER_HEALTH             AE_PLAYER1_PLAYER_HEALTH
#define AE_PLAYER_BODY_RADIUS        0x44u
#define AE_PLAYER_BODY_HEIGHT        0x46u
#define AE_PLAYER_POSITION_FLAGS     0x50u
#define AE_BODY_LINK_ID              0x28u
#define AE_PLAYER_FLAGS_MASK         0xF0u
#define AE_PLAYER_FLAGS_HIT_IMMUNE   0xE0u
#define AE_PLAYER_FLAGS_CLIMBING     0xC0u
#define AE_BODY_PROBE_RECORD_SIZE    0x10u
#define AE_BODY_PROBE_RECORD_SHAPE   0x0Cu
#define AE_BODY_PROBE_SHAPE_TYPE     0x12u
#define AE_BODY_RESOLVE_OUTPUT_SIZE  0x18u
#define AE_BODY_FRAME_SOUND_PAN_SIZE 0x20u
#define AE_BODY_FRAME_SOUND_PAN_OUT  0x10u
#define AE_BODY_SOUND_PAN_LEFT       0x00u
#define AE_BODY_SOUND_PAN_RIGHT      0x02u
#define AE_BODY_FRAME_SLOPE_SIZE     0x50u
#define AE_BODY_FRAME_SLOPE_SHAPE    0x16u
#define AE_BODY_FRAME_SLOPE_OUT      0x28u
#define AE_BODY_FRAME_SLOPE_POINT    0x30u
#define AE_BODY_SLOPE_SHAPE_HALF_Y   0x02u
#define AE_BODY_SLOPE_SHAPE_HALF_Z   0x04u
#define AE_BODY_FRAME_RECORD_SIZE    0x38u
#define AE_BODY_FRAME_VEHICLE_SIZE   0x30u
#define AE_BODY_FRAME_PLAYER_LAND    0x30u
#define AE_BODY_FRAME_RESOLVE_SIZE   0x50u
#define AE_BODY_FRAME_PARTICLE_POS   0x10u
#define AE_BODY_FRAME_LEDGE_SIZE     0x48u
#define AE_BODY_FRAME_LEDGE_DIRECTION 0x10u
#define AE_BODY_FRAME_LEDGE_RAY_PARAMS 0x18u
#define AE_BODY_FRAME_LEDGE_RAY_OUT  0x30u
#define AE_BODY_VECTOR_X             0x00u
#define AE_BODY_VECTOR_Y             0x04u
#define AE_BODY_VECTOR_Z             0x08u
#define AE_BODY_SHORT_VECTOR_Y       0x02u
#define AE_BODY_SHORT_VECTOR_Z       0x04u
#define AE_BODY_VECTOR_RADIUS        0x0Cu
#define AE_BODY_VECTOR_HEIGHT        0x10u
#define AE_PLAYER_LEDGE_GROUND_OFFSET 0x100u
#define AE_PLAYER_LEDGE_HEIGHT_BIAS  0x81u
#define AE_PLAYER_LEDGE_HEIGHT_LIMIT 0x7Fu
#define AE_DEFAULT_STACK_TOP         0x801FFF00u
#define AE_BODY_DESPAWN_PUSH_LIMIT   97
#define AE_RC_CAR_BODY_OFFSET        0x24u
#define AE_BODY_DESPAWN_PARTICLE_POS (AE_G_RC_CAR + AE_RC_CAR_BODY_OFFSET)
#define AE_BODY_LAND_PARTICLE_POS    (AE_G_RC_CAR + AE_RC_CAR_BODY_OFFSET)
#define AE_COLLIDER_SLOPE_OUT_HEIGHT 0x04u
#define AE_BODY_COLLIDER_CONTACT_INDEX 0x16u
#define AE_BODY_LAND_PARTICLE_OFFSET 0x10u
#define AE_BODY_LAND_FRAME_SIZE      AE_BODY_FRAME_PLAYER_LAND
#define AE_BODY_LAND_PARTICLE_ID     0x102
#define AE_BODY_LAND_SOUND_ID        0x79
#define AE_PLAYER_BODY_VEL_X         0x30u
#define AE_PLAYER_BODY_VEL_Y         0x34u
#define AE_PLAYER_BODY_VEL_Z         0x38u

#define AE_BODY_POS_X                0x00u
#define AE_BODY_POS_Y                0x04u
#define AE_BODY_POS_Z                0x08u
#define AE_BODY_VEL_X                0x0Cu
#define AE_BODY_VEL_Y                0x10u
#define AE_BODY_VEL_Z                0x14u
#define AE_BODY_RADIUS               0x20u
#define AE_BODY_HEIGHT               0x22u
#define AE_BODY_PARAM                0x24u
#define AE_BODY_FLAGS                0x26u
#define AE_BODY_CONTACT              0x2Cu
#define AE_BODY_COLLIDER_INDEX       0x34u
#define AE_BODY_LAND_SNAPSHOT_POS_Y  0x04u
#define AE_BODY_SNAPSHOT_SIZE        AE_BODY_SIZE

#define AE_BODY_COLLIDER_X           0x00u
#define AE_BODY_COLLIDER_Y           0x02u
#define AE_BODY_COLLIDER_Z           0x04u
#define AE_BODY_COLLIDER_HALF_X      0x06u
#define AE_BODY_COLLIDER_HALF_Y      0x08u
#define AE_BODY_COLLIDER_HALF_Z      0x0Au
#define AE_BODY_COLLIDER_TYPE        0x10u
#define AE_BODY_COLLIDER_SURFACE     0x12u
#define AE_BODY_COLLIDER_FLAGS       0x18u
#define AE_BODY_COLLIDER_BODY_MARKER 0x32u
#define AE_BODY_PROBE_CONTACT        0x0Eu
#define AE_BODY_COLLIDER_SIZE        0x38u

#define AE_ADDR_COLL_SHAPE_BOX          0x8004E33Cu
#define AE_ADDR_COLL_SHAPE_WALL_THIN    0x8004E444u
#define AE_ADDR_COLL_SHAPE_WALL_EDGE    0x8004E63Cu
#define AE_ADDR_COLL_SHAPE_WALL_DIAGONAL 0x8004E8F8u
#define AE_ADDR_COLL_SHAPE_RAMP45       0x8004EA1Cu
#define AE_ADDR_COLL_SHAPE_CYLINDER     0x8004EC3Cu
#define AE_ADDR_COLL_SHAPE_RAMP2        0x8004ED30u
#define AE_ADDR_COLL_SHAPE_RAMP4        0x8004EE94u
#define AE_ADDR_COLL_SHAPE_WEDGE        0x8004F000u
#define AE_ADDR_COLL_SHAPE_DIAGONAL_BOX 0x8004F330u
#define AE_ADDR_COLL_PROBE_BOX          0x80073BC8u
#define AE_ADDR_COLL_PROBE_THIN_WALL    0x80075F48u
#define AE_ADDR_COLL_PROBE_RAMP         0x80073FD4u
#define AE_ADDR_COLL_PROBE_RADIAL       0x80074BBCu
#define AE_ADDR_COLL_PROBE_TRIANGLE     0x8007449Cu
#define AE_ADDR_COLL_PROBE_EDGE_WALL    0x800761ACu
#define AE_ADDR_COLL_PROBE_DIAGONAL_WALL 0x80076488u
#define AE_ADDR_COLL_PROBE_STRIP        0x8007734Cu
#define AE_ADDR_COLL_PROBE_NO_CONTACT   0x80077490u
#define AE_ADDR_COLL_PROBE_HALF_PIPE    0x80075630u
#define AE_ADDR_COLL_PROBE_SLOPE        0x80076AC8u
#define AE_ADDR_COLL_PROBE_TRENCH       0x80075A40u
#define AE_ADDR_COLL_PROBE_ROPE         0x80076E80u

#define AE_BODY_SOUND_SLOT           0x800B20D0u
#define AE_BODY_SOUND_ACTIVE         0x800B20D2u
#define AE_BODY_SCRATCH_POSITION     0x10u
#define AE_BODY_SCRATCH_OUTPUT       0x10u

#define AE_BODY_FRAME_COLLISION_UPDATE 56u

static int32_t body_s16(uint32_t value) { return (int16_t)(uint16_t)value; }
static int32_t body_add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t body_sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int32_t body_mul32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static int32_t body_abs32(int32_t value) { return value < 0 ? body_sub32(0, value) : value; }
static int32_t body_half_toward_zero(int32_t value) {
    return (value + (value < 0 ? 1 : 0)) >> 1;
}

static void body_missing_call(AeRam *r, const char *name) {
    ae_ram_fail(r, "AeBodyMoveHooks.%s not provided", name);
}

static uint32_t body_call_frame(const AeBodyMoveHooks *h, uint32_t frame_size) {
    return (h ? h->stack_pointer : AE_DEFAULT_STACK_TOP) - frame_size;
}

static void body_write_position_s16(AeRam *r, uint32_t destination, uint32_t body);

/* ------------------------------------------------------------------------------------------------
 * SignedRangeOverlap @800739FC
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_SignedRangeOverlap(int32_t first, int32_t second,
                              int32_t upper, int32_t extent) {
    int32_t a = body_s16((uint32_t)first);
    int32_t b = body_s16((uint32_t)second);
    int32_t c = body_s16((uint32_t)upper);
    int32_t d = body_s16((uint32_t)extent);
    int32_t difference = a - b;
    if (difference < c) {
        int32_t lower = c - d;
        if (lower < difference) return lower < a;
    }
    return difference < c - d && c < a;
}

/* ------------------------------------------------------------------------------------------------
 * CollisionStub_ReturnZero @80072DC4
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_CollisionStub_ReturnZero(AeRam *r, uint32_t body) {
    (void)r;
    (void)body;
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * SoundLoopStop @8008806C
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_SoundLoopStop(AeRam *r, const AeSoundHw *sound_hw,
                         int32_t id, int32_t voice) {
    if (voice < 0) return 0;
    int32_t voice16 = body_s16((uint32_t)voice);
    ae_SfxVoiceStop(r, sound_hw, voice16);
    if (((uint32_t)id & 0xF000u) == 0u)
        (void)ae_SoundLoopFree(r, body_s16((uint32_t)id), voice16);
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * SoundEmitterPan @80087B34
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_SoundEmitterPan(AeRam *r, const AeBodyMoveHooks *h,
                           int32_t sound_id, int32_t voice,
                           int32_t range, uint32_t position) {
    (void)sound_id;
    int32_t voice16 = body_s16((uint32_t)voice);
    if (voice16 < 0) return -1;
    uint32_t frame = body_call_frame(h, AE_BODY_FRAME_SOUND_PAN_SIZE);
    uint32_t pan_out = frame + AE_BODY_FRAME_SOUND_PAN_OUT;
    int32_t has_pan = ae_SoundEmitterCalcPan(r, position,
                                             body_s16((uint32_t)range),
                                             pan_out);
    int32_t left = 0;
    int32_t right = 0;
    if (has_pan != 0) {
        left = ae_rds16(r, pan_out + AE_BODY_SOUND_PAN_LEFT);
        right = ae_rds16(r, pan_out + AE_BODY_SOUND_PAN_RIGHT);
    }
    ae_SfxVoiceSetPanVol(r, h ? h->misc : NULL, voice16, left, right);
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * SoundEmitterPitch @80087C28
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_SoundEmitterPitch(AeRam *r, const AeBodyMoveHooks *h,
                             int32_t sound_id, int32_t voice,
                             int32_t pitch) {
    (void)sound_id;
    int32_t voice16 = body_s16((uint32_t)voice);
    if (voice16 < 0) return -1;
    if (!h || !h->SfxVoiceSetPitch) body_missing_call(r, "SfxVoiceSetPitch");
    else h->SfxVoiceSetPitch(h->user, r, voice16, body_s16((uint32_t)pitch));
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * ColliderSlopeContactDir @8004F884
 * ---------------------------------------------------------------------------------------------- */
static int32_t body_dispatch_solid_shape(AeRam *r, const AeBodyMoveHooks *h,
                                         int32_t type, uint32_t shape,
                                         uint32_t point, uint32_t out) {
    uint32_t handler = ae_rd32(r, AE_COLL_SHAPE_HANDLERS + (uint32_t)type * 4u);
    switch (handler) {
    case AE_ADDR_COLL_SHAPE_BOX: return ae_CollShape_Box(r, type, shape, point, out);
    case AE_ADDR_COLL_SHAPE_WALL_THIN: return ae_CollShape_WallThin(r, h ? h->solid : NULL, type, shape, point, out);
    case AE_ADDR_COLL_SHAPE_WALL_EDGE: return ae_CollShape_WallEdge(r, h ? h->solid : NULL, type, shape, point, out);
    case AE_ADDR_COLL_SHAPE_WALL_DIAGONAL: return ae_CollShape_WallDiagonal(r, type, shape, point, out);
    case AE_ADDR_COLL_SHAPE_RAMP45: return ae_CollShape_Ramp45(r, type, shape, point, out);
    case AE_ADDR_COLL_SHAPE_CYLINDER: return ae_CollShape_Cylinder(r, type, shape, point, out);
    case AE_ADDR_COLL_SHAPE_RAMP2: return ae_CollShape_Ramp2(r, type, shape, point, out);
    case AE_ADDR_COLL_SHAPE_RAMP4: return ae_CollShape_Ramp4(r, type, shape, point, out);
    case AE_ADDR_COLL_SHAPE_WEDGE: return ae_CollShape_Wedge(r, type, shape, point, out);
    case AE_ADDR_COLL_SHAPE_DIAGONAL_BOX: return ae_CollShape_DiagonalBox(r, type, shape, point, out);
    default:
        ae_ram_fail(r, "ColliderSlopeContactDir: unknown shape handler %08X for type %d",
                    handler, type);
        return 0;
    }
}

int32_t ae_ColliderSlopeContactDir(AeRam *r, const AeBodyMoveHooks *h,
                                   int32_t type, uint32_t collider,
                                   uint32_t point, uint32_t out) {
    int32_t shape_type = body_s16((uint32_t)type);
    uint32_t frame = body_call_frame(h, AE_BODY_FRAME_SLOPE_SIZE);
    uint32_t shape = frame + AE_BODY_FRAME_SLOPE_SHAPE;
    uint32_t handler_out = frame + AE_BODY_FRAME_SLOPE_OUT;
    uint32_t local_point = frame + AE_BODY_FRAME_SLOPE_POINT;
    int32_t px = body_sub32(ae_rds32(r, point), ae_rds16(r, collider + AE_BODY_COLLIDER_X));
    int32_t py = ae_rds32(r, point + AE_BODY_VECTOR_Y);
    int32_t pz = body_sub32(ae_rds32(r, point + AE_BODY_VECTOR_Z),
                            ae_rds16(r, collider + AE_BODY_COLLIDER_Z));
    ae_wr32(r, local_point, (uint32_t)px);
    ae_wr32(r, local_point + AE_BODY_VECTOR_Y, (uint32_t)py);
    ae_wr32(r, local_point + AE_BODY_VECTOR_Z, (uint32_t)pz);
    ae_wr16(r, shape, ae_rd16(r, collider + AE_BODY_COLLIDER_HALF_X));
    ae_wr16(r, shape + AE_BODY_SLOPE_SHAPE_HALF_Y,
            ae_rd16(r, collider + AE_BODY_COLLIDER_HALF_Y));
    ae_wr16(r, shape + AE_BODY_SLOPE_SHAPE_HALF_Z,
            ae_rd16(r, collider + AE_BODY_COLLIDER_HALF_Z));
    ae_wr32(r, handler_out, (uint32_t)ae_rds16(r, collider + AE_BODY_COLLIDER_Y));

    int32_t result = body_dispatch_solid_shape(r, h, shape_type, shape, local_point, handler_out);
    int32_t shape_y = ae_rds16(r, collider + AE_BODY_COLLIDER_Y);
    if ((uint32_t)(shape_type - 0x40) < 4u) {
        int32_t low = ae_rds32(r, handler_out + AE_COLLIDER_SLOPE_OUT_HEIGHT);
        ae_wr32(r, out, (uint32_t)body_sub32(shape_y, low));
        ae_wr32(r, out + AE_BODY_VECTOR_Y,
                (uint32_t)body_sub32(ae_rds32(r, handler_out), low));
    } else {
        ae_wr32(r, out, (uint32_t)shape_y);
        ae_wr32(r, out + AE_BODY_VECTOR_Y, ae_rd32(r, handler_out));
    }
    return result;
}

/* ------------------------------------------------------------------------------------------------
 * BodyRecordColliderContact @800792C8
 * ---------------------------------------------------------------------------------------------- */
static int body_record_box_xz(AeRam *r, uint32_t collider, int32_t x, int32_t z) {
    int32_t cx = ae_rds16(r, collider + AE_BODY_COLLIDER_X);
    int32_t cz = ae_rds16(r, collider + AE_BODY_COLLIDER_Z);
    int32_t hx = ae_rds16(r, collider + AE_BODY_COLLIDER_HALF_X);
    int32_t hz = ae_rds16(r, collider + AE_BODY_COLLIDER_HALF_Z);
    return !(x < cx - hx) && !(cx + hx < x) &&
           !(z < cz - hz) && !(cz + hz < z);
}

static int body_record_box_y(AeRam *r, uint32_t collider, int32_t y) {
    int32_t cy = ae_rds16(r, collider + AE_BODY_COLLIDER_Y);
    int32_t hy = ae_rds16(r, collider + AE_BODY_COLLIDER_HALF_Y);
    return !(y < cy - hy) && !(cy < y);
}

int32_t ae_BodyRecordColliderContact(AeRam *r, const AeBodyMoveHooks *h,
                                     uint32_t body, int32_t type) {
    int32_t body_type = body_s16((uint32_t)type);
    if (body_type == 1) {
        uint32_t player = AE_G_PLAYER;
        uint32_t flags = ae_rd32(r, player + AE_PLAYER_STATE_FLAGS) & AE_PLAYER_FLAGS_MASK;
        int32_t state = (int16_t)ae_rd16(r, player + AE_PLAYER_STATE);
        if (flags == 0xE0u || state == 0x23 || state == 0x4A || state == 0x2D)
            return 0;
    }
    if (ae_rd16(r, AE_G_PLAYER_MODE) == 6u && body_type == 1) return 0;

    int32_t count = body_s16(ae_rd16(r, AE_G_COLLIDER_COUNT) - 1u);
    if (count < 0) return 0;
    uint32_t collider_base = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t frame = body_call_frame(h, AE_BODY_FRAME_RECORD_SIZE);
    uint32_t point = AE_SCR_BODY_RECORD_POINT;
    uint32_t query_point = ae_rd32(r, AE_G_BODY_RECORD_POINT_PTR);
    uint32_t out = AE_G_BODY_RECORD_OUT;
    for (int32_t index = count; index >= 0; --index) {
        uint32_t collider = collider_base + (uint32_t)index * AE_BODY_COLLIDER_SIZE;
        uint32_t flags = ae_rd16(r, collider + AE_BODY_COLLIDER_FLAGS);
        if ((flags & 0x000Fu) == 3u || (flags & 0x8000u) != 0u) continue;
        if (ae_rd8(r, collider + AE_BODY_COLLIDER_SURFACE) == 9u) continue;

        uint32_t geometry = ae_rd16(r, collider + AE_BODY_COLLIDER_TYPE);
        uint32_t family = geometry & 0xF000u;
        int32_t px = ae_rds32(r, body + AE_BODY_POS_X);
        int32_t py = ae_rds32(r, body + AE_BODY_POS_Y);
        int32_t pz = ae_rds32(r, body + AE_BODY_POS_Z);
        int32_t cx = ae_rds16(r, collider + AE_BODY_COLLIDER_X);
        int32_t cy = ae_rds16(r, collider + AE_BODY_COLLIDER_Y);
        int32_t cz = ae_rds16(r, collider + AE_BODY_COLLIDER_Z);
        int matched = 0;

        if (family == 0x3000u) {
            uint32_t shape_type = geometry & 0xFFu;
            if (shape_type - 0x46u < 4u) continue;
            int32_t dx = body_sub32(px, cx);
            int32_t dy = body_sub32(py, cy);
            int32_t dz = body_sub32(pz, cz);
            if (shape_type - 0x24u < 2u) {
                ae_wr32(r, point, (uint32_t)dx);
                ae_wr32(r, point + AE_BODY_VECTOR_Y, (uint32_t)dy);
                ae_wr32(r, point + AE_BODY_VECTOR_Z, (uint32_t)dz);
                int32_t ok = ae_CollHalfPipeHeight(r, (int32_t)shape_type,
                                                    collider + AE_BODY_COLLIDER_HALF_X,
                                                    point, out);
                if (ok != 0) {
                    int32_t height = ae_rds32(r, out);
                    matched = !(py < cy - height) && !(cy + height < py);
                }
            } else if (shape_type - 0x53u < 2u) {
                ae_wr32(r, point, (uint32_t)dx);
                ae_wr32(r, point + AE_BODY_VECTOR_Y, (uint32_t)dy);
                ae_wr32(r, point + AE_BODY_VECTOR_Z, (uint32_t)dz);
                int32_t ok = ae_ColliderTrenchHeight(r, (int32_t)shape_type,
                                                      collider + AE_BODY_COLLIDER_HALF_X,
                                                      point, out);
                if (ok != 0) {
                    int32_t base = body_sub32(cy, ae_rds32(r, out));
                    matched = !(py < base) && !(body_add32(base, 0x40) < py);
                }
            } else {
                int32_t hx = ae_rds16(r, collider + AE_BODY_COLLIDER_HALF_X);
                int32_t dx2 = body_mul32(body_sub32(cx, px), body_sub32(cx, px));
                int32_t dz2 = body_mul32(body_sub32(cz, pz), body_sub32(cz, pz));
                int32_t radius2 = body_mul32(hx, hx);
                matched = body_add32(dx2, dz2) < radius2 && body_record_box_y(r, collider, py);
            }
        } else if (family == 0x4000u) {
            if (body_record_box_xz(r, collider, px, pz) && body_record_box_y(r, collider, py)) {
                ae_wr32(r, query_point, (uint32_t)px);
                ae_wr32(r, query_point + AE_BODY_VECTOR_Y, (uint32_t)py);
                ae_wr32(r, query_point + AE_BODY_VECTOR_Z, (uint32_t)pz);
                int32_t distance = ae_DiagonalSignedDistance(r, geometry, query_point, collider, out);
                matched = distance <= 0;
            }
        } else if (family == 0x2000u) {
            if (body_record_box_xz(r, collider, px, pz) && body_record_box_y(r, collider, py)) {
                ae_wr32(r, query_point, (uint32_t)px);
                ae_wr32(r, query_point + AE_BODY_VECTOR_Y, (uint32_t)py);
                ae_wr32(r, query_point + AE_BODY_VECTOR_Z, (uint32_t)pz);
                AeBodyMoveHooks nested = h ? *h : (AeBodyMoveHooks){0};
                nested.stack_pointer = frame;
                int32_t contact = ae_ColliderSlopeContactDir(r, &nested,
                    (int32_t)ae_rd8(r, collider + AE_BODY_COLLIDER_TYPE),
                    collider, query_point, out);
                if (contact != 0) {
                    int32_t height = ae_rds32(r, out + AE_BODY_VECTOR_Y);
                    matched = !(py < cy - height) && !(cy < py);
                }
            }
        } else if (family == 0u) {
            matched = body_record_box_xz(r, collider, px, pz) &&
                      body_record_box_y(r, collider, py);
        }
        if (matched) ae_wr16(r, body + AE_BODY_COLLIDER_INDEX, (uint32_t)index);
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * PlayerVehicleSoundUpdate @80072A14
 * ---------------------------------------------------------------------------------------------- */
static void body_write_position_s16(AeRam *r, uint32_t destination, uint32_t body) {
    ae_wr16(r, destination, ae_rd16(r, body + AE_BODY_POS_X));
    ae_wr16(r, destination + AE_BODY_SHORT_VECTOR_Y, ae_rd16(r, body + AE_BODY_POS_Y));
    ae_wr16(r, destination + AE_BODY_SHORT_VECTOR_Z, ae_rd16(r, body + AE_BODY_POS_Z));
}

int32_t ae_PlayerVehicleSoundUpdate(AeRam *r, const AeBodyHooks *h,
                                    uint32_t body) {
    uint32_t caller_sp = h ? h->move.stack_pointer : 0x801FFF00u;
    uint32_t frame = caller_sp - AE_BODY_FRAME_VEHICLE_SIZE;
    int32_t velocity_x = ae_rds32(r, body + AE_BODY_VEL_X);
    int32_t velocity_z = ae_rds32(r, body + AE_BODY_VEL_Z);
    int32_t absolute_sum = body_s16((uint32_t)body_add32(body_abs32(velocity_x),
                                                        body_abs32(velocity_z)));
    int32_t active = ae_rds16(r, AE_BODY_SOUND_ACTIVE);
    if (absolute_sum < 3) {
        if (active != 0) {
            (void)ae_SoundLoopStop(r, h ? h->move.sound_hw : NULL, 0x16,
                                   ae_rds16(r, AE_BODY_SOUND_SLOT));
            body_write_position_s16(r, frame + AE_BODY_SCRATCH_POSITION, body);
            (void)ae_PlaySfxAt(r, h ? h->move.sound_hw : NULL, 0x17,
                               frame + AE_BODY_SCRATCH_POSITION);
            ae_wr32(r, AE_G_BODY_VEHICLE_PITCH, 0x0D00u);
            ae_wr16(r, AE_BODY_SOUND_ACTIVE, 0);
            ae_wr32(r, AE_G_BODY_VEHICLE_SPEED2, 0);
        }
        return 0;
    }

    if (active == 0) {
        int32_t handle = ae_SoundLoopStart(r, h ? h->move.sound_hw : NULL, 1, 0x16);
        ae_wr16(r, AE_BODY_SOUND_SLOT, (uint32_t)handle);
        ae_wr16(r, AE_BODY_SOUND_ACTIVE, 1);
    }

    int32_t handle = ae_rds16(r, AE_BODY_SOUND_SLOT);
    body_write_position_s16(r, frame + AE_BODY_SCRATCH_POSITION, body);
    AeBodyMoveHooks nested = h ? h->move : (AeBodyMoveHooks){0};
    nested.stack_pointer = frame;
    (void)ae_SoundEmitterPan(r, &nested, 0x16, handle, 0x1000,
                             frame + AE_BODY_SCRATCH_POSITION);

    int32_t speed_squared = body_add32(body_mul32(velocity_x, velocity_x),
                                       body_mul32(velocity_z, velocity_z));
    int32_t previous_speed = ae_rds32(r, AE_G_BODY_VEHICLE_SPEED2);
    int32_t requested_pitch = body_add32(ae_rds32(r, AE_G_BODY_VEHICLE_PITCH),
                                         body_sub32(speed_squared, previous_speed));
    requested_pitch = body_s16((uint32_t)requested_pitch);
    if (requested_pitch >= 0x2000) requested_pitch = 0x1FFF;
    if (requested_pitch < 0) requested_pitch = 0;

    if (ae_rds32(r, AE_G_BODY_VEHICLE_PITCH) != requested_pitch)
        (void)ae_SoundEmitterPitch(r, &nested, 0x16, handle, requested_pitch);
    ae_wr32(r, AE_G_BODY_VEHICLE_PITCH, (uint32_t)requested_pitch);
    ae_wr32(r, AE_G_BODY_VEHICLE_SPEED2, (uint32_t)body_s16((uint32_t)speed_squared));
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * PlayerMoveRumbleSoundUpdate @80072BE8
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_PlayerMoveRumbleSoundUpdate(AeRam *r, const AeBodyHooks *h,
                                       uint32_t body) {
    if (ae_rd16(r, AE_G_PLAYER_MODE) != 1u) {
        if (ae_rds16(r, AE_G_RUMBLE_SOUND_ACTIVE) != 0) {
            (void)ae_SoundLoopStop(r, h ? h->move.sound_hw : NULL, 0xAD,
                                   ae_rds16(r, AE_G_RUMBLE_SOUND_SLOT));
            ae_wr16(r, AE_G_RUMBLE_SOUND_ACTIVE, 0);
        }
        ae_wr16(r, AE_G_BODY_RUMBLE_TIMER, 0);
        ae_wr16(r, AE_G_BODY_RUMBLE_STRENGTH, 0);
        return 0;
    }

    uint32_t timer = ae_rd16(r, AE_G_BODY_RUMBLE_TIMER);
    if (ae_rds16(r, AE_G_BODY_RUMBLE_TIMER) == 0) {
        ae_wr16(r, AE_G_BODY_RUMBLE_STRENGTH, 0x6E);
        timer = 2;
    } else {
        ae_wr16(r, AE_G_BODY_RUMBLE_STRENGTH, 0);
        timer = (timer - 1u) & 0xFFFFu;
    }
    ae_wr16(r, AE_G_BODY_RUMBLE_TIMER, timer);

    int moving = ae_rds32(r, body + AE_BODY_VEL_X) != 0 ||
                 ae_rds32(r, body + AE_BODY_VEL_Z) != 0 ||
                 (ae_rd32(r, AE_G_BODY_RUMBLE_GATE) & 0xFF00FF00u) != 0;
    if (!moving) {
        if (ae_rds16(r, AE_G_RUMBLE_SOUND_ACTIVE) != 0) {
            (void)ae_SoundLoopStop(r, h ? h->move.sound_hw : NULL, 0xAD,
                                   ae_rds16(r, AE_G_RUMBLE_SOUND_SLOT));
            ae_wr16(r, AE_G_RUMBLE_SOUND_ACTIVE, 0);
        }
        ae_wr16(r, AE_G_BODY_RUMBLE_TIMER, 0);
        ae_wr16(r, AE_G_BODY_RUMBLE_STRENGTH, 0);
        return 0;
    }

    if (ae_rds16(r, AE_G_RUMBLE_SOUND_ACTIVE) == 0) {
        int32_t handle = ae_SoundLoopStart(r, h ? h->move.sound_hw : NULL, 0, 0xAD);
        ae_wr16(r, AE_G_RUMBLE_SOUND_SLOT, (uint32_t)handle);
        ae_wr16(r, AE_G_RUMBLE_SOUND_ACTIVE, 1);
    }

    int32_t strength = ae_rds16(r, AE_G_BODY_RUMBLE_STRENGTH);
    if (strength > 0)
        ae_PadRumble(r, h ? h->move.camupd : NULL, 1,
                     (uint32_t)strength, ae_rd16(r, AE_G_BODY_RUMBLE_TIMER));
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * BodyLandOnGround @80072DCC
 * ---------------------------------------------------------------------------------------------- */
int32_t ae_BodyLandOnGround(AeRam *r, const AeBodyMoveHooks *h,
                            uint32_t body, int32_t type, int32_t ceiling) {
    ae_wr16(r, AE_G_BODY_LAND_FLAGS, 1);

    int32_t old_x = ae_rds32(r, body + AE_BODY_POS_X);
    int32_t old_y = ae_rds32(r, body + AE_BODY_POS_Y);
    int32_t old_z = ae_rds32(r, body + AE_BODY_POS_Z);
    int32_t velocity_y = ae_rds32(r, body + AE_BODY_VEL_Y);
    int32_t half_height = body_half_toward_zero(ae_rds16(r, body + AE_BODY_HEIGHT));
    int32_t predicted_y = body_add32(old_y, velocity_y);
    int32_t ground = ae_GetGroundHeight(r, h ? h->world : NULL, old_x,
                                        body_sub32(predicted_y, half_height), old_z);
    ae_wr32(r, AE_G_BODY_GROUND_Y, (uint32_t)ground);
    if (ground < predicted_y) {
        ae_wr32(r, body + AE_BODY_VEL_Y, (uint32_t)body_sub32(ground, old_y));
        ae_wr32(r, body + AE_BODY_POS_Y, (uint32_t)ground);
    } else {
        ae_wr32(r, body + AE_BODY_POS_Y,
                (uint32_t)body_add32(predicted_y, ceiling));
    }

    uint32_t snapshot = ae_rd32(r, AE_G_BODY_SNAPSHOT_PTR);
    int32_t moved_y = body_sub32(ae_rds32(r, body + AE_BODY_POS_Y),
                                 ae_rds32(r, snapshot + AE_BODY_LAND_SNAPSHOT_POS_Y));
    if (moved_y < -255) {
        ae_wr32(r, body + AE_BODY_POS_X, ae_rd32(r, snapshot + AE_BODY_POS_X));
        ae_wr32(r, body + AE_BODY_POS_Y, ae_rd32(r, snapshot + AE_BODY_POS_Y));
        ae_wr32(r, body + AE_BODY_POS_Z, ae_rd32(r, snapshot + AE_BODY_POS_Z));
        ae_wr32(r, body + AE_BODY_VEL_X, 0);
        ae_wr32(r, body + AE_BODY_VEL_Z, 0);
        ae_wr32(r, body + AE_BODY_VEL_Y, ae_rd32(r, snapshot + AE_BODY_VEL_Y));

        if (body_s16((uint32_t)type) == 1) {
            int32_t shape = 0;
            if (ae_rds16(r, AE_SCR_GROUND_SOLID_KIND) == 1)
                shape = ae_rd16(r, AE_SCR_GROUND_SOLID_SHAPE) & 0xFFu;
            uint32_t player = AE_G_PLAYER;
            int32_t state = ae_rds16(r, player + AE_PLAYER_STATE);
            int32_t contact = ae_rds32(r, body + AE_BODY_CONTACT);
            uint32_t state_flags = ae_rd32(r, player + AE_PLAYER_STATE_FLAGS) & AE_PLAYER_FLAGS_MASK;
            if (contact != 0 && state_flags != 0xE0u && state != 0x23 && state != 0x4A &&
                state != 0x2D && shape != 0x24 && shape != 0x25 &&
                ae_rd16(r, AE_PLAYER1_AREA_ID) != 0x44u) {
                if (ae_rd16(r, AE_PLAYER1_CURRENT_LEVEL) == 0x20u) {
                    ae_PlayerSetState(r, h ? h->player_act : NULL, 0x22u);
                    if (h && h->XaPlay)
                        h->XaPlay(h->user, r,
                                  (ae_rd32(r, AE_G_PLAYER_AUDIO_FLAGS) & 1u) != 0u ? 0x0D : 0x0F);
                    else
                        body_missing_call(r, "XaPlay");
                } else {
                    ae_PlayerDamage(r, 5);
                }
            }
        } else if (body_s16((uint32_t)type) == 2) {
            if (h && h->area_set != NULL) ae_RcCarDespawn(r, h->area_set);
            else ae_ram_fail(r, "BodyLandOnGround: AeAreaSetHooks not provided");
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, AE_BODY_LAND_SOUND_ID);
            uint32_t particle_position = body_call_frame(h, AE_BODY_LAND_FRAME_SIZE) +
                                         AE_BODY_LAND_PARTICLE_OFFSET;
            body_write_position_s16(r, particle_position, AE_BODY_LAND_PARTICLE_POS);
            ae_SpawnParticle(r, h ? h->spawn : NULL,
                             AE_BODY_LAND_PARTICLE_ID, particle_position);
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------
 * CollisionResolveProbes @80073450
 * ---------------------------------------------------------------------------------------------- */
static int32_t body_dispatch_probe(AeRam *r, const AeCollProbeHooks *h,
                                  uint32_t handler, uint32_t shape,
                                  uint32_t probe, uint32_t out) {
    switch (handler) {
    case AE_ADDR_COLL_PROBE_BOX: return ae_CollProbe_Box(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_THIN_WALL: return ae_CollProbe_ThinWall(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_RAMP: return ae_CollProbe_Ramp(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_RADIAL: return ae_CollProbe_Radial(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_TRIANGLE: return ae_CollProbe_Triangle(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_EDGE_WALL: return ae_CollProbe_EdgeWall(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_DIAGONAL_WALL: return ae_CollProbe_DiagonalWall(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_STRIP: return ae_CollProbe_Strip(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_NO_CONTACT: return ae_CollProbe_NoContact(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_HALF_PIPE: return ae_CollProbe_HalfPipe(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_SLOPE: return ae_CollProbe_Slope(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_TRENCH: return ae_CollProbe_Trench(r, h, shape, probe, out);
    case AE_ADDR_COLL_PROBE_ROPE: return ae_CollProbe_Rope(r, h, shape, probe, out);
    default:
        ae_ram_fail(r, "CollisionResolveProbes: unknown probe handler %08X", handler);
        return 0;
    }
}

int32_t ae_CollisionResolveProbes(AeRam *r, const AeBodyMoveHooks *h,
                                  int32_t type, uint32_t body) {
    uint32_t body_type = (uint32_t)body_s16((uint32_t)type);
    uint32_t input = AE_G_BODY_RESOLVE_INPUT;
    uint32_t output = AE_G_BODY_RESOLVE_OUTPUT;
    uint32_t push = AE_G_BODY_PUSH_OUT;
    uint32_t probe_grid = ae_rd32(r, AE_G_BODY_PROBE_GRID);
    int32_t ceiling = 0;
    int32_t contact = 0;

    ae_wr32(r, push, 0);
    ae_wr32(r, push + AE_BODY_VECTOR_Y, 0);
    ae_wr32(r, push + AE_BODY_VECTOR_Z, 0);
    ae_wr16(r, AE_G_BODY_COLLISION_MARKER, (uint32_t)-999);

    int32_t x = ae_rds32(r, body + AE_BODY_POS_X);
    int32_t z = ae_rds32(r, body + AE_BODY_POS_Z);
    int32_t vx = ae_rds32(r, body + AE_BODY_VEL_X);
    int32_t vy = ae_rds32(r, body + AE_BODY_VEL_Y);
    int32_t vz = ae_rds32(r, body + AE_BODY_VEL_Z);
    ae_wr32(r, body + AE_BODY_POS_X, (uint32_t)body_add32(x, vx));
    ae_wr32(r, body + AE_BODY_POS_Z, (uint32_t)body_add32(z, vz));
    ae_wr32(r, input, (uint32_t)ae_rds16(r, body + AE_BODY_RADIUS));
    ae_wr32(r, input + AE_COLLPROBE_INPUT_HEIGHT,
            (uint32_t)ae_rds16(r, body + AE_BODY_HEIGHT));

    for (int32_t index = 0; index < 8; ++index) {
        uint32_t record = probe_grid + (uint32_t)index * AE_BODY_PROBE_RECORD_SIZE;
        uint32_t shape = ae_rd32(r, record + AE_BODY_PROBE_RECORD_SHAPE);
        if (shape == 0) continue;
        uint32_t shape_type = ae_rd16(r, shape + AE_BODY_PROBE_SHAPE_TYPE);
        uint32_t handler_offset = ((shape_type & 0xF000u) >> 10);
        uint32_t dispatch = ae_rd32(r, AE_G_COLL_PROBE_HANDLERS + handler_offset);
        int32_t probe_y = body_add32(body_add32(ae_rds32(r, body + AE_BODY_POS_Y), vy),
                                     ae_rds32(r, push + AE_BODY_VECTOR_Y));
        int32_t probe_x = body_add32(ae_rds32(r, body + AE_BODY_POS_X), ae_rds32(r, push));
        int32_t probe_z = body_add32(ae_rds32(r, body + AE_BODY_POS_Z),
                                    ae_rds32(r, push + AE_BODY_VECTOR_Z));
        uint32_t current = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
        ae_wr32(r, current + AE_COLLPROBE_POSITION_X, (uint32_t)probe_x);
        ae_wr32(r, current + AE_COLLPROBE_POSITION_Y, (uint32_t)probe_y);
        ae_wr32(r, current + AE_COLLPROBE_POSITION_Z, (uint32_t)probe_z);
        ae_wr32(r, input + AE_COLLPROBE_INPUT_TYPE,
                (uint32_t)ae_rds16(r, shape + AE_BODY_PROBE_SHAPE_TYPE));
        for (uint32_t offset = 0; offset < AE_BODY_RESOLVE_OUTPUT_SIZE; offset += sizeof(uint32_t))
            ae_wr32(r, output + offset, 0);
        if (body_dispatch_probe(r, h ? h->coll_probe : NULL, dispatch,
                                record, input, output) == 0)
            continue;

        int32_t out_y = ae_rds32(r, output + AE_COLLPROBE_OUTPUT_Y);
        int skip_horizontal_push = index == 7 && out_y != 0;
        int32_t ceiling_value;
        if (skip_horizontal_push) {
            ae_wr32(r, body + AE_BODY_VEL_Y,
                    (uint32_t)body_add32(ae_rds32(r, body + AE_BODY_VEL_Y), out_y));
            ceiling_value = out_y;
        }
        if (!skip_horizontal_push) {
            ae_wr32(r, push + AE_BODY_VECTOR_Z,
                    (uint32_t)body_add32(ae_rds32(r, push + AE_BODY_VECTOR_Z),
                                         ae_rds32(r, output + AE_COLLPROBE_OUTPUT_Z)));
            ae_wr32(r, push,
                    (uint32_t)body_add32(ae_rds32(r, push), ae_rds32(r, output)));
            contact = body_s16(ae_rd16(r, shape + AE_BODY_PROBE_CONTACT)) |
                      (int32_t)0xA0000000u;
            ceiling_value = contact;
        }
        if (ceiling < ceiling_value) ceiling = ceiling_value;
    }

    int32_t count = body_s16((uint32_t)ae_rd16(r, AE_G_COLLIDER_COUNT) - 1u);
    for (int32_t index = count; index >= 0; --index) {
        ae_wr16(r, AE_G_BODY_COLLISION_MARKER, (uint32_t)index);
        uint32_t skip = ae_rd32(r, AE_G_COLL_PROBE_FLAGS);
        if (ae_rd8(r, skip + (uint32_t)index) != 0u) continue;
        ae_wr16(r, AE_G_COLL_COUNTER,
                (uint32_t)ae_rd16(r, AE_G_COLL_COUNTER) + 1u);
        uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
        uint32_t collider = colliders + (uint32_t)index * AE_BODY_COLLIDER_SIZE;
        int32_t px = body_add32(ae_rds32(r, body + AE_BODY_POS_X),
                                ae_rds32(r, push + AE_COLLPROBE_OUTPUT_X));
        int32_t py = body_add32(body_add32(ae_rds32(r, body + AE_BODY_POS_Y),
                                           ae_rds32(r, body + AE_BODY_VEL_Y)),
                                ae_rds32(r, push + AE_BODY_VECTOR_Y));
        int32_t pz = body_add32(ae_rds32(r, body + AE_BODY_POS_Z),
                                ae_rds32(r, push + AE_COLLPROBE_OUTPUT_Z));
        uint32_t current = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
        ae_wr32(r, current + AE_COLLPROBE_POSITION_X, (uint32_t)px);
        ae_wr32(r, current + AE_COLLPROBE_POSITION_Y, (uint32_t)py);
        ae_wr32(r, current + AE_COLLPROBE_POSITION_Z, (uint32_t)pz);
        uint32_t geometry = ae_rd16(r, collider + AE_BODY_COLLIDER_TYPE);
        ae_wr32(r, input, (uint32_t)ae_rds16(r, body + AE_BODY_RADIUS));
        ae_wr32(r, input + AE_COLLPROBE_INPUT_TYPE, geometry);
        ae_wr32(r, input + AE_COLLPROBE_INPUT_HEIGHT,
                (uint32_t)ae_rds16(r, body + AE_BODY_HEIGHT));
        for (uint32_t offset = 0; offset < AE_BODY_RESOLVE_OUTPUT_SIZE; offset += sizeof(uint32_t))
            ae_wr32(r, output + offset, 0);
        uint32_t dispatch_offset = (geometry & 0xF000u) >> 10;
        uint32_t dispatch = ae_rd32(r, AE_G_COLL_PROBE_HANDLERS + dispatch_offset);
        if (body_dispatch_probe(r, h ? h->coll_probe : NULL, dispatch,
                                collider, input, output) == 0)
            continue;

        int32_t out_y = ae_rds32(r, output + AE_COLLPROBE_OUTPUT_Y);
        int landed_on_wall = 0;
        if (out_y > 0) {
            int32_t c = body_s16((uint32_t)(ae_rd16(r, collider + AE_BODY_COLLIDER_Y) +
                                                   ae_rd16(r, output + AE_COLLPROBE_OUTPUT_SLOPE_X)));
            int32_t d = body_s16((uint32_t)(ae_rd16(r, collider + AE_BODY_COLLIDER_HALF_Y) +
                                                   ae_rd16(r, output + AE_COLLPROBE_OUTPUT_SLOPE_Z)));
            int32_t current_y = body_s16(ae_rd16(r, current + AE_COLLPROBE_POSITION_Y));
            int32_t height = ae_rds16(r, body + AE_BODY_HEIGHT);
            if (ae_SignedRangeOverlap(current_y, height, c, d)) {
                ae_wr32(r, body + AE_BODY_VEL_Y,
                        (uint32_t)body_add32(ae_rds32(r, body + AE_BODY_VEL_Y), out_y));
                landed_on_wall = 1;
            }
        }
        if (!landed_on_wall) {
            ae_wr32(r, push, (uint32_t)body_add32(ae_rds32(r, push), ae_rds32(r, output)));
            ae_wr32(r, push + AE_BODY_VECTOR_Z,
                    (uint32_t)body_add32(ae_rds32(r, push + AE_BODY_VECTOR_Z),
                                         ae_rds32(r, output + AE_COLLPROBE_OUTPUT_Z)));
        }
        uint32_t marker = ae_rd16(r, collider + AE_BODY_COLLIDER_CONTACT_INDEX) | 0xA0000000u;
        uint16_t surface = ae_rd16(r, collider + AE_BODY_COLLIDER_SURFACE);
        if ((surface & 0xFFu) == 1u) marker |= 0x08000000u;
        contact = (int32_t)marker;
        if (surface == 1u)
            ae_wr16(r, collider + AE_BODY_COLLIDER_BODY_MARKER,
                    ae_rd16(r, body + AE_BODY_LINK_ID));
    }

    ae_wr32(r, body + AE_BODY_POS_X,
            (uint32_t)body_add32(ae_rds32(r, body + AE_BODY_POS_X), ae_rds32(r, push)));
    ae_wr32(r, body + AE_BODY_POS_Z,
            (uint32_t)body_add32(ae_rds32(r, body + AE_BODY_POS_Z),
                                 ae_rds32(r, push + AE_BODY_VECTOR_Z)));
    if (ae_rds32(r, push) == 0 && ae_rds32(r, push + AE_BODY_VECTOR_Z) == 0 && contact == 0)
        return ceiling > 0 ? ceiling : 0;

    int32_t abs_push_x = body_abs32(ae_rds32(r, push));
    int32_t abs_push_z = body_abs32(ae_rds32(r, push + AE_BODY_VECTOR_Z));
    if (body_type == 2u &&
        !(abs_push_x < AE_BODY_DESPAWN_PUSH_LIMIT &&
          abs_push_z < AE_BODY_DESPAWN_PUSH_LIMIT)) {
        ae_wr32(r, push, 0);
        ae_wr32(r, push + AE_BODY_VECTOR_Z, 0);
        ae_RcCarDespawn(r, h ? h->area_set : NULL);
        (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 0x79);
        uint32_t particle_pos = body_call_frame(h, AE_BODY_FRAME_RESOLVE_SIZE) +
                                AE_BODY_FRAME_PARTICLE_POS;
        body_write_position_s16(r, particle_pos, AE_BODY_DESPAWN_PARTICLE_POS);
        ae_SpawnParticle(r, h ? h->spawn : NULL, 0x102, particle_pos);
    }

    if (ae_rds32(r, push) != 0)
        ae_wr32(r, body + AE_BODY_VEL_X,
                (uint32_t)body_add32(ae_rds32(r, body + AE_BODY_VEL_X), ae_rds32(r, push)));
    if (ae_rds32(r, push + AE_BODY_VECTOR_Z) != 0)
        ae_wr32(r, body + AE_BODY_VEL_Z,
                (uint32_t)body_add32(ae_rds32(r, body + AE_BODY_VEL_Z),
                                     ae_rds32(r, push + AE_BODY_VECTOR_Z)));
    ae_wr32(r, body + AE_BODY_CONTACT, (uint32_t)contact);
    return ceiling > 0 ? ceiling : 0;
}

/* ------------------------------------------------------------------------------------------------
 * PlayerTryLedgeGrab @80063EC4
 * ---------------------------------------------------------------------------------------------- */
static int32_t body_divide_32_toward_zero(int32_t value) {
    return value / 32;
}

void ae_PlayerTryLedgeGrab(AeRam *r, const AeBodyMoveHooks *h) {
    uint32_t caller_sp = h ? h->stack_pointer : AE_DEFAULT_STACK_TOP;
    uint32_t frame = caller_sp - AE_BODY_FRAME_LEDGE_SIZE;
    uint32_t direction = frame + AE_BODY_FRAME_LEDGE_DIRECTION;
    uint32_t ray_params = frame + AE_BODY_FRAME_LEDGE_RAY_PARAMS;
    uint32_t ray_out = frame + AE_BODY_FRAME_LEDGE_RAY_OUT;
    uint32_t player = AE_G_PLAYER;

    if (ae_rds16(r, player + AE_PLAYER_HEALTH) <= 0) return;
    if ((ae_rd32(r, player + AE_PLAYER_CONTACT_KIND) >> 28) == 8u) return;
    uint32_t mode = ae_rd16(r, AE_G_PLAYER_MODE);
    if (mode == 3u || mode == 1u || mode == 5u || mode == 6u) return;

    int32_t facing_x = ae_rds16(r, AE_G_PLAYER_FACING);
    int32_t facing_z = ae_rds16(r, AE_G_PLAYER_FACING + AE_BODY_SHORT_VECTOR_Z);
    int32_t dir_x = facing_x > 0x800 ? 1 : facing_x < -0x800 ? -1 : 0;
    int32_t dir_z = facing_z > 0x800 ? 1 : facing_z < -0x800 ? -1 : 0;
    ae_wr16(r, direction, (uint32_t)dir_x);
    ae_wr16(r, direction + AE_BODY_SHORT_VECTOR_Y, 0);
    ae_wr16(r, direction + AE_BODY_SHORT_VECTOR_Z, (uint32_t)dir_z);
    (void)ae_sdk_VectorNormalSS(r, direction, direction);

    int32_t velocity_y = ae_rds32(r, player + AE_PLAYER_BODY_VEL_Y);
    if (velocity_y <= 0) return;
    int32_t position_x = ae_rds32(r, AE_G_PLAYER_POS_X);
    int32_t position_y = ae_rds32(r, AE_G_PLAYER_POS_Y);
    int32_t position_z = ae_rds32(r, AE_G_PLAYER_POS_Z);
    int32_t ground_here = body_s16((uint32_t)ae_GetGroundHeight(r, h ? h->world : NULL,
                                                                 position_x, position_y, position_z));
    if (!(position_y < ground_here)) return;
    if ((ae_rd32(r, player + AE_PLAYER_STATE_FLAGS) & AE_PLAYER_FLAGS_MASK) ==
        AE_PLAYER_FLAGS_CLIMBING) return;

    ae_wr16(r, AE_G_COLL_WALL_PROBE, 1);
    int32_t dx = body_divide_32_toward_zero(ae_rds16(r, direction));
    int32_t dz = body_divide_32_toward_zero(ae_rds16(r, direction + AE_BODY_SHORT_VECTOR_Z));
    int32_t probe_x = body_add32(position_x, dx);
    int32_t probe_y = body_sub32(position_y, AE_PLAYER_LEDGE_GROUND_OFFSET);
    int32_t probe_z = body_add32(position_z, dz);
    int32_t top = body_s16((uint32_t)ae_GetGroundHeightEx(r, h ? h->world : NULL,
                                                           probe_x, probe_y, probe_z,
                                                           AE_G_PLAYER_SLOPE_X));
    int32_t body_height = ae_rds16(r, player + AE_PLAYER_BODY_HEIGHT);
    int32_t body_radius = ae_rds16(r, player + AE_PLAYER_BODY_RADIUS);
    ae_wr32(r, ray_params + AE_BODY_VECTOR_X, (uint32_t)probe_x);
    ae_wr32(r, ray_params + AE_BODY_VECTOR_Y, (uint32_t)probe_y);
    ae_wr32(r, ray_params + AE_BODY_VECTOR_Z, (uint32_t)probe_z);
    ae_wr32(r, ray_params + AE_BODY_VECTOR_RADIUS, (uint32_t)body_radius);
    ae_wr32(r, ray_params + AE_BODY_VECTOR_HEIGHT, (uint32_t)body_height);

    uint32_t height_delta = (uint32_t)position_y - (uint32_t)top - AE_PLAYER_LEDGE_HEIGHT_BIAS;
    if (height_delta >= AE_PLAYER_LEDGE_HEIGHT_LIMIT) return;
    if (ae_rd32(r, AE_G_PLAYER_SLOPE_X) != 0u) return;
    if (ae_rds16(r, AE_SCR_GROUND_SOLID_KIND) < 0) return;
    if ((ae_rd16(r, AE_SCR_GROUND_SURFACE_FLAGS) & 0x4000u) != 0u) return;
    uint32_t shape = ae_rd16(r, AE_SCR_GROUND_SOLID_SHAPE) & 0xFFu;
    if (shape == 0x24u || shape == 0x25u || shape == 0x53u || shape == 0x54u) return;
    if ((ae_rd16(r, AE_SCR_GROUND_SOLID_TYPE) & 0xFFu) == 10u) return;

    int32_t ray = ae_CollisionRayTest(r, h ? h->misc : NULL, 0,
                                      ray_params, ray_out);
    if (ray >= 0) return;

    ae_wr32(r, AE_G_PLAYER_POS_X, (uint32_t)probe_x);
    ae_wr32(r, AE_G_PLAYER_POS_Y, (uint32_t)top);
    ae_wr32(r, AE_G_PLAYER_POS_Z, (uint32_t)probe_z);
    ae_wr32(r, player + AE_PLAYER_BODY_VEL_X, 0);
    ae_wr32(r, player + AE_PLAYER_BODY_VEL_Y, 0);
    ae_wr32(r, player + AE_PLAYER_BODY_VEL_Z, 0);
    ae_PlayerSetState(r, h ? h->player_act : NULL, 0x0Eu);
    if (h && h->LocalDirectionPadding)
        ae_wr16(r, direction + 6u, h->LocalDirectionPadding(h->user, r));
    for (uint32_t offset = 0; offset < 8; ++offset)
        ae_wr8(r, AE_G_PLAYER_FACING + offset, ae_rd8(r, direction + offset));
}


