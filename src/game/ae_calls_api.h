#ifndef AE_CALLS_API_H
#define AE_CALLS_API_H
#include "../core/ae_ram.h"
typedef struct AeGame AeGame;

typedef int32_t (*AeGameCallFn)(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
typedef struct AeGameCall {
    uint32_t address;
    const char *name;
    AeGameCallFn invoke;
    uint32_t overlay_member; /* 0 for EXE, UINT32_MAX when identity is not in the ledger. */
} AeGameCall;

size_t ae_game_call_count(void);
const AeGameCall *ae_game_call_at(size_t index);
const AeGameCall *ae_game_find_call(uint32_t address);
/* Extra original o32 arguments are read at call_stack_pointer + 16, +20, ... .
 * Returns zero for C void ports: their original leftover v0 is unspecified. */
int32_t ae_game_call(AeGame *, uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
int32_t ae_game_call5(AeGame *, uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
int32_t ae_game_call6(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t ae_game_call7(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t ae_game_collision_ray(AeGame *, uint32_t mode, uint32_t params, uint32_t out);
int32_t ae_game_missing_call(AeGame *, const char *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t ae_game_missing_words(AeGame *, const char *, uint32_t, uint32_t, const uint32_t *);
#endif
