#ifndef AE_PLAYERUPDATE_H
#define AE_PLAYERUPDATE_H

#include "../core/ae_ram.h"

typedef struct AePlayerUpdateHooks {
    void *user;
    /* PsyQ SDK boundary, angle and MATRIX address. */
    int32_t (*RotMatrixY)(void *user, AeRam *ram, uint32_t angle, uint32_t matrix); /* 80030890 */
} AePlayerUpdateHooks;

/* 80066C90: target and direction are eight-byte SVECTORs. */
void ae_TurnTowardsY(AeRam *r, const AePlayerUpdateHooks *hooks,
                     uint32_t target, uint32_t direction, int32_t max_angle);

#endif
