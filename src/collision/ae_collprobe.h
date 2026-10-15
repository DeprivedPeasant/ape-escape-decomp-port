#ifndef AE_COLLPROBE_H
#define AE_COLLPROBE_H

#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hook signature for original functions outside this batch. The four arguments are the original a0..a3 values. */
typedef int32_t (*AeCollProbeHookFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t);

/* Complete o32 climb contract, including the two stack words. */
typedef int32_t (*AeCollProbeClimbFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

typedef struct AeCollProbeHooks {
    void *user;
    int32_t stale_wall_axis; /* caller $s1 for unsupported orientation words */
    int16_t stale_edge_x, stale_edge_z; /* caller $s2/$s3 on unsupported edge orientations */
    AeCollProbeHookFn PointInBoxXZ; /* 80073B64: inclusive XZ bounds test */
    AeCollProbeHookFn printf; /* 800A3720: external debug output used by the no-contact handler */
    AeCollProbeHookFn HalfPipeRangeTest; /* 80050490: vertical overlap gate */
    AeCollProbeHookFn ColliderSlopeContactDir; /* 8004F884: slope/ramp contact top and height */
    AeCollProbeHookFn DirIndexFromFlags; /* 8007774C: direction index selected by orientation flags */
    AeCollProbeClimbFn PlayerSetClimbTarget; /* 80066E28: attach the player to a climbable collider */
    AeCollProbeHookFn PlayerSetRope; /* 80066E8C: bind the player to a rope segment */
} AeCollProbeHooks;

/* EXE string passed to printf by CollProbe_NoContact. */
#define AE_COLLPROBE_NO_CONTACT_FORMAT 0x800A5E80u
#define AE_COLLPROBE_CURRENT_POSITION 0x800B20B4u /* pointer to the probe point's signed x/y/z words */
#define AE_COLLPROBE_PREVIOUS_POSITION 0x800B20B8u /* pointer to the preceding frame's probe point */

/* Probe descriptor passed by CollisionResolveProbes / CollisionRayTest. */
#define AE_COLLPROBE_INPUT_RADIUS 0x00u
#define AE_COLLPROBE_INPUT_TYPE 0x04u
#define AE_COLLPROBE_INPUT_HEIGHT 0x08u
#define AE_COLLPROBE_POSITION_X 0x00u
#define AE_COLLPROBE_POSITION_Y 0x04u
#define AE_COLLPROBE_POSITION_Z 0x08u

/* Push vector and optional vertical/contact response written by the probes. */
#define AE_COLLPROBE_OUTPUT_X 0x00u
#define AE_COLLPROBE_OUTPUT_Y 0x0Cu
#define AE_COLLPROBE_OUTPUT_Z 0x08u
#define AE_COLLPROBE_OUTPUT_SLOPE_X 0x10u
#define AE_COLLPROBE_OUTPUT_SLOPE_Z 0x14u
#define AE_COLLPROBE_DIAGONAL_PROJECTION 0x1F800124u
#define AE_COLLPROBE_SLOPE_CONTACT 0x801FFED8u
#define AE_COLLPROBE_SLOPE_GRADIENT 0x801FFEE0u
#define AE_COLLPROBE_RAMP_POINT 0x1F800134u
#define AE_COLLPROBE_RAMP_HEIGHT 0x1F800124u
#define AE_COLLPROBE_TRENCH_POINT 0x1F800124u
#define AE_COLLPROBE_TRENCH_TOP_HEIGHT 0x1F800130u
#define AE_COLLPROBE_TRENCH_FOOT_HEIGHT 0x1F800138u
#define AE_COLLPROBE_TRENCH_HALFPIPE_SHAPE 0x801FFEC0u
#define AE_COLLPROBE_TRIANGLE_DIRECTION_TABLE 0x800A5D84u
#define AE_COLLPROBE_TRIANGLE_DIRECTION_COPY 0x801FFEC0u
#define AE_COLLPROBE_BODY_CUR_TYPE 0x800B20CCu
#define AE_COLLPROBE_PLAYER_ACTION_A 0x800B20C6u
#define AE_COLLPROBE_PLAYER_ACTION_B 0x800B20CAu
#define AE_COLLPROBE_ROPE_ACTION 0x800B20C8u
#define AE_COLLPROBE_ATTACH_KIND 0x800F44A0u
#define AE_COLLPROBE_ROPE_INDEX 0x800EC238u
#define AE_COLLPROBE_ROPE_END_A_X 0x00u
#define AE_COLLPROBE_ROPE_END_A_Y 0x02u
#define AE_COLLPROBE_ROPE_END_A_Z 0x04u
#define AE_COLLPROBE_ROPE_END_B_X 0x24u
#define AE_COLLPROBE_ROPE_END_B_Y 0x26u
#define AE_COLLPROBE_ROPE_END_B_Z 0x28u
#define AE_COLLPROBE_ROPE_PROJECTED_X 0x1F800124u
#define AE_COLLPROBE_ROPE_PROJECTED_Z 0x1F800128u
#define AE_COLLPROBE_ROPE_DISTANCE_FROM_PLAYER 0x1F80012Cu
#define AE_COLLPROBE_ROPE_DISTANCE_FROM_MIDPOINT 0x1F800130u

#define AE_COLLPROBE_STRIP_Z_A 0x4Au
#define AE_COLLPROBE_STRIP_X_A 0x4Bu
#define AE_COLLPROBE_STRIP_Z_B 0x4Cu
#define AE_COLLPROBE_STRIP_X_B 0x4Du

int32_t ae_CollProbe_NoContact(AeRam *r, const AeCollProbeHooks *h,
                               uint32_t collider, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_Strip(AeRam *r, const AeCollProbeHooks *h,
                           uint32_t collider, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_ThinWall(AeRam *r, const AeCollProbeHooks *h,
                              uint32_t shape, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_EdgeWall(AeRam *r, const AeCollProbeHooks *h,
                              uint32_t shape, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_Box(AeRam *r, const AeCollProbeHooks *h,
                         uint32_t shape, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_DiagonalWall(AeRam *r, const AeCollProbeHooks *h,
                                 uint32_t shape, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_Ramp(AeRam *r, const AeCollProbeHooks *h,
                          uint32_t shape, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_Trench(AeRam *r, const AeCollProbeHooks *h,
                            uint32_t shape, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_Triangle(AeRam *r, const AeCollProbeHooks *h,
                              uint32_t shape, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_Radial(AeRam *r, const AeCollProbeHooks *h,
                            uint32_t shape, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_Rope(AeRam *r, const AeCollProbeHooks *h,
                          uint32_t shape, uint32_t probe, uint32_t out);
int32_t ae_CollProbe_HalfPipe(AeRam *r, const AeCollProbeHooks *h,
                              uint32_t shape, uint32_t probe, uint32_t out);

#ifdef __cplusplus
}
#endif
#endif

