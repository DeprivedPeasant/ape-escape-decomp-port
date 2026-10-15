/* Original special sky-prop (mesh type 4 / 5) vertex transform, integer only
 * (see native_sky.md). Pure helper: no game data, no projection, no OT. C11,
 * no dependencies beyond the standard headers. */
#ifndef AE_NATIVE_SKY_H
#define AE_NATIVE_SKY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AE_SKY_TYPE_FOLLOW_PLAYER_Y 4 /* result Y = wrap16(y - wrap16(playerY)) */
#define AE_SKY_TYPE_FIXED_Y 5         /* result Y = y */

/* Draw policy of the original special path for a supported type. All fields
 * are fixed by the original DrawAreaProps; none depends on the placement. */
typedef struct AeSkyPolicy {
    int uses_placement;        /* 0: placement x/y/z are ignored */
    int zero_camera_translation; /* 1: SetTransMatrix with all-zero translation, view rotation kept */
    int fog;                   /* 0: fogParam is 0 during the group */
    int background_bucket;     /* 1: OT bucket pinned to the background */
} AeSkyPolicy;

/* Fills *policy and returns 1 for type 4 or 5. Returns 0 with *policy unchanged
 * for any other type or a NULL policy. */
int ae_sky_policy(int type, AeSkyPolicy *policy);

/* Original quarter turn of an instance rotation: (rotation >> 10) & 3. Low bits
 * are rounded down, not interpolated. */
int ae_sky_quarter_turn(uint16_t rotation);

/* MeshTransformRot90 for one vertex. in/out are raw signed 16-bit mesh
 * coordinates (x, y, z); quarter q gives q0 (x,y,z), q1 (z,y,-x),
 * q2 (-x,y,-z), q3 (-z,y,x). Every result is stored as 16 bits with wrap (no
 * clamp). Type 4 then Y = wrap16(y - wrap16(playerY)); type 5 leaves Y = y;
 * playerY is ignored for type 5. in and out may alias. Returns 1 on success.
 * Returns 0 with out unchanged if type is not 4/5 or in/out is NULL. */
int ae_sky_vertex(int type, uint16_t rotation, int16_t player_y, const int16_t in[3], int16_t out[3]);

#ifdef __cplusplus
}
#endif

#endif
