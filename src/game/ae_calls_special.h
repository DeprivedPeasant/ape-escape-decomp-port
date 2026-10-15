#ifndef AE_CALLS_SPECIAL_H
#define AE_CALLS_SPECIAL_H
#include "ae_game.h"
int32_t special_ae_GadgetSlotsInitForExit(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t special_ae_PlayerAct_ScaleMoveVel(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t special_ae_PlayerAct_TurnAroundAndJump(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t special_ae_pad_CameraRotMatrix(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t special_ae_player_can_switch_gadget(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t special_ae_player_gadget_select_request(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t special_ae_player_is_swimming(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t special_ae_player_normalize(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t special_ae_sdk_normalize_core(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t special_ae_sdk_square0_lane(AeGame *, uint32_t, uint32_t, uint32_t, uint32_t);
void ae_GadgetSlotsSave(AeRam *);
void ae_GadgetSlotsRestore(AeRam *);
int32_t ae_GadgetSlotsInitForArea(AeRam *, int32_t);
#endif
