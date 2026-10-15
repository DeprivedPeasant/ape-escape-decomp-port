#ifndef AE_OVL180_H
#define AE_OVL180_H

#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native platform and renderer boundaries used by title / sequence player member 180. */
typedef int32_t (*AeOvl180CallFn)(void *user, AeRam *ram, const char *name,
                                  uint32_t address, uint32_t argc,
                                  const uint32_t *args);

typedef struct AeOvl180Hooks {
    void *user;
    AeOvl180CallFn native_call;
    AeOvl180CallFn renderer;
    uint32_t ram_stack_pointer;
} AeOvl180Hooks;

/* Overlay globals. The member is loaded at 0x80136000; these addresses are not unique
 * across overlays, so callers and checkers must pair them with member 180.bin. */
#define AE_OVL180_G_STREAM_RING          0x8013C1A8u
#define AE_OVL180_G_COMMAND_TABLE        0x8013C1ACu
#define AE_OVL180_G_SEQUENCE             0x8013C480u
#define AE_OVL180_G_SEQUENCE_FILE        0x8013C484u
#define AE_OVL180_G_OBJECTS               0x8013C564u
#define AE_OVL180_G_OBJECT_COUNT          0x8013C568u
#define AE_OVL180_G_HOOK_VALUE_A           0x8013C56Cu
#define AE_OVL180_G_HOOK_VALUE_B           0x8013C570u
#define AE_OVL180_G_FILE_COUNT             0x8013C544u
#define AE_OVL180_G_FILE_DATA              0x8013C548u
#define AE_OVL180_G_OBJECT_FLAGS           0x8013C574u
#define AE_OVL180_G_RANGE_COUNT            0x8013C714u
#define AE_OVL180_G_RANGES                 0x8013C758u
#define AE_OVL180_G_FILE_INDEX_TABLE       0x8013C090u
#define AE_OVL180_G_DISPLAY_STATE          0x8013C488u
#define AE_OVL180_G_DISPLAY_BUFFER         0x8013C48Cu
#define AE_OVL180_G_LAST_VSYNC             0x8013C490u
#define AE_OVL180_G_FRAME_COUNTER          0x8013C494u
#define AE_OVL180_G_FRAME_FLAGS            0x800EC350u
#define AE_OVL180_G_MODE_STATE             0x800F4470u
#define AE_OVL180_G_MODE_SUBSTATE          0x800F4476u
#define AE_OVL180_G_TITLE_SEQUENCE_INDEX   0x800F44AAu

/* Sequence header and command-record fields. */
#define AE_OVL180_SEQ_HEADER_BYTES         0x18u
#define AE_OVL180_SEQ_HEADER_FLAGS         0x10u
#define AE_OVL180_SEQ_HEADER_FLAG_LETTERBOX 0x08u
#define AE_OVL180_SEQ_OBJECT_STRIDE        0x04u
#define AE_OVL180_OBJECT_FLAG_BYTES        0x00u
#define AE_OVL180_RANGE_STRIDE             0x08u
#define AE_OVL180_RANGE_LOW                0x00u
#define AE_OVL180_RANGE_HIGH               0x02u

/* Original member-180 function addresses used as named hooks by the native host. */
#define AE_OVL180_FN_STREAM_RING_INIT      0x80136000u
#define AE_OVL180_FN_CD_READ_SEEK          0x80136454u
#define AE_OVL180_FN_CD_PLAY_XA_RANGE      0x801364B0u
#define AE_OVL180_FN_COPY_SCREEN_TO_BACK   0x801367DCu
#define AE_OVL180_FN_SETUP_DISPLAY         0x80136BD4u
#define AE_OVL180_FN_STREAM_WAIT           0x80136E00u
#define AE_OVL180_FN_CD_SET_FILTER         0x80136E68u
#define AE_OVL180_FN_PRESENT               0x80136FF0u
#define AE_OVL180_FN_DRAW_SPRITE           0x80138354u
#define AE_OVL180_FN_DRAW_TEXT             0x8013AD40u
#define AE_OVL180_FN_CD_READ_ASYNC         0x8013ADA0u
#define AE_OVL180_FN_CD_READY_CALLBACK     0x8013AE90u
#define AE_OVL180_FN_LOAD_SAVED_IMAGE      0x8013AA70u
#define AE_OVL180_FN_DRAW_LIT_MESH         0x8013B9B0u
#define AE_OVL180_FN_SAVE_RESTORE_VRAM     0x8013BA80u
#define AE_OVL180_FN_PARSE_OBJECTS         0x8013990Cu
#define AE_OVL180_FN_GET_BUSY              0x8013AE80u
#define AE_OVL180_FN_FADE_IN               0x80136EA8u

/* Original functions whose game-state code is ported. */
void ae_SeqPlayer_StreamRingInit(AeRam *r, const AeOvl180Hooks *h,
                                 uint32_t unused, uint32_t sectors);
uint32_t ae_SeqPlayer_DisplayStateInit(AeRam *r, const AeOvl180Hooks *h);
int32_t ae_SeqPlayer_Init(AeRam *r, const AeOvl180Hooks *h,
                          uint32_t display_state, uint32_t sequence_index);
void ae_SeqPlayer_FreeObjects(AeRam *r);
void ae_SeqPlayer_Stop(AeRam *r, const AeOvl180Hooks *h, uint32_t restore);
int32_t ae_Mode_Title(AeRam *r, const AeOvl180Hooks *h);
void ae_SeqPlayer_FadeIn(AeRam *r, const AeOvl180Hooks *h);
void ae_SeqPlayer_ParseObjects(AeRam *r, const AeOvl180Hooks *h, uint32_t data);
int32_t ae_SeqPlayer_Run(AeRam *r, const AeOvl180Hooks *h,
                         uint32_t sequence_index);
void ae_SeqPlayer_InitHook(AeRam *r, const AeOvl180Hooks *h,
                           uint32_t out_a, uint32_t out_b);
void ae_SeqPlayer_Hook2(AeRam *r, uint32_t out_a, uint32_t out_b);
uint32_t ae_SeqPlayer_GetObject(AeRam *r, uint32_t index);
uint32_t ae_SeqPlayer_GetObjectFlag(AeRam *r, uint32_t index);
void ae_SeqPlayer_ClearObjectFlags(AeRam *r);
void ae_SeqPlayer_ClearObjectFlagsWrap(AeRam *r, const AeOvl180Hooks *h,
                                       uint32_t byte_count);
void ae_SeqPlayer_MeshInit(AeRam *r, const AeOvl180Hooks *h,
                           uint32_t object, uint32_t mesh);
void ae_SeqPlayer_ModelInit(AeRam *r, const AeOvl180Hooks *h,
                            uint32_t object, uint32_t model);
uint32_t ae_SeqPlayer_SumRecordSizes(AeRam *r, uint32_t records, int32_t count);
uint32_t ae_SeqPlayer_FindRecordByTime(AeRam *r, uint32_t records, uint32_t time);
int32_t ae_SeqPlayer_FindRangeIndex(AeRam *r, uint32_t value);
void ae_SeqPlayer_ApplyFlagOps(AeRam *r, uint32_t records, int32_t count,
                               uint32_t operation);
uint32_t ae_SeqPlayer_SetTransitionSweep(AeRam *r, uint32_t command,
                                         uint32_t arguments);
void ae_SeqPlayer_AdvanceTransitionSweep(AeRam *r,
                                         const AeOvl180Hooks *h,
                                         uint32_t command,
                                         uint32_t arguments);
void ae_SeqPlayer_CommandWriteObjectFlags(AeRam *r, uint32_t command,
                                          uint32_t arguments);

/* Adapters that let this overlay call independently verified game ports. */
void ovl180_RunEffectScript(AeRam *r, const AeOvl180Hooks *h);
void ovl180_RunEffects(AeRam *r, const AeOvl180Hooks *h);
void ovl180_RunPadUpdate(AeRam *r, const AeOvl180Hooks *h, uint32_t flags);

#ifdef __cplusplus
}
#endif
#endif
