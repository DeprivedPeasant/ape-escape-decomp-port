#ifndef AE_ZAKO_INIT_H
#define AE_ZAKO_INIT_H

#include "../core/ae_ram.h"

typedef struct AeBodyHooks AeBodyHooks;

typedef int32_t (*AeZakoHookFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t);

/* Unported functions called by this batch. Pointer arguments are PSX RAM addresses. */
typedef struct AeZakoHooks {
    void *user;
    AeZakoHookFn Printf;                 /* 800A3720 */
    AeZakoHookFn MenuItemInitBound;      /* 8007D1E0 */
    AeZakoHookFn MenuItemSetCallback;    /* 8007D29C */
    AeZakoHookFn ReadSizedValue;         /* 8007D118 */
    const AeBodyHooks *body_hooks;
} AeZakoHooks;

/* Global addresses used by the original enemy initialization code. */
#define AE_G_ZAKO_STATE             0x800E77D0u
#define AE_G_ZAKO_ENTRY_TABLE       0x800E77D4u
#define AE_G_ZAKO_PATH_TABLE        0x800E77D8u
#define AE_G_ZAKO_ENTRY_COUNT       0x800E77DCu
#define AE_G_ZAKO_PATH_COUNT        0x800E77DEu
#define AE_G_ZAKO_SPAWN_COUNT       0x800E77E0u
#define AE_G_ZAKO_MODE              0x800E77A0u
#define AE_G_ZAKO_SLOT_BY_ENTRY     0x800E77B0u
#define AE_G_ZAKO_OBJECTS           0x800E6960u
#define AE_G_ZAKO_PROGRESS          0x800B8D00u
#define AE_G_ZAKO_HOOKS             0x800B8D20u
#define AE_G_ZAKO_HOOK_COUNT        0x800E77A5u
#define AE_G_ZAKO_MODE_KIND         0x800E77A0u
#define AE_G_ZAKO_MODE_COUNTER      0x800E77A1u
#define AE_G_ZAKO_MODE_ENTRY_INDEX  0x800E77A2u
#define AE_ZAKO_MODE_RESET_BYTE_03  0x03u
#define AE_ZAKO_MODE_RESET_BYTE_04  0x04u
#define AE_ZAKO_MODE_RESET_HALF_06  0x06u
#define AE_ZAKO_PROGRESS_WORD_SIZE  0x04u
#define AE_ZAKO_HOOK_POINTER_SIZE   0x04u
#define AE_ZAKO_HOOK_ENTRY_COUNT    2u
#define AE_G_ZAKO_DEBUG_ITEMS       0x800AF590u
#define AE_G_DEBUG_MENU_ITEM_HEAD   0x800B00A4u
#define AE_G_ZAKO_DEBUG_VALUES      0x800E77E4u
#define AE_G_ZAKO_DEBUG_CALLBACK    0x8005879Cu
#define AE_G_LEVEL_FIRST_AREAS      0x800B3858u
#define AE_LEVEL_FIRST_AREA_ENTRY_SIZE 0x02u
#define AE_G_AREA_ZED               0x800F4594u
#define AE_G_STAGE_STATE            0x800F4350u
#define AE_G_OBJ_KILL_COUNT         0x800F44B0u
#define AE_G_MODEL_BONE_MATRICES    0x800E9C60u
#define AE_AREA_MEMBER_BASE         0x80154000u
#define AE_AREA_ZED_OFFSET_WORD     0x4E0u
#define AE_ZAKO_FMT_SIGHT_OVERFLOW  0x800A4F60u

/* Area stage-state fields. */
#define AE_ZAKO_STAGE_LEVEL         0x124u
#define AE_ZAKO_STAGE_AREA          0x126u
#define AE_ZAKO_STAGE_AREA_KIND     0x12Cu
#define AE_ZAKO_GRID_LIMIT_SOURCE   0x800EBFC8u
#define AE_ZAKO_GRID_LIMIT_X        0x800B4D50u
#define AE_ZAKO_GRID_LIMIT_Z        0x800B4D52u
#define AE_ZAKO_GRID_LIMIT_X_MAX    1365
#define AE_ZAKO_GRID_LIMIT_X_CLAMP  1366
#define AE_ZAKO_GRID_LIMIT_Z_OFFSET 375
#define AE_ZAKO_GRID_LIMIT_Z_CLAMP  2731
#define AE_ZAKO_GRID_LIMIT_Z_MAX    2730

/* Fields in the global enemy-system state block. */
#define AE_ZAKO_STATE_AREA_ZED      0x00u
#define AE_ZAKO_STATE_ENTRY_TABLE   0x04u
#define AE_ZAKO_STATE_PATH_TABLE    0x08u
#define AE_ZAKO_STATE_ENTRY_COUNT   0x0Cu
#define AE_ZAKO_STATE_PATH_COUNT    0x0Eu
#define AE_ZAKO_STATE_SPAWN_COUNT   0x10u
#define AE_ZAKO_STATE_RESET_BYTE_12 0x12u
#define AE_ZAKO_STATE_RESET_BYTE_13 0x13u
#define AE_ZAKO_STATE_RESET_BYTE_14 0x14u
#define AE_ZAKO_STATE_RESET_BYTE_18 0x18u
#define AE_ZAKO_STATE_RESET_BYTE_1F 0x1Fu
#define AE_ZAKO_STATE_RESET_BYTE_23 0x23u
#define AE_ZAKO_PROGRESS_WORD_COUNT 8u
#define AE_ZAKO_FIRST_SPAWN_TYPE    0xB7u
#define AE_ZAKO_AREA_MODE_CLEAR     13u
#define AE_ZAKO_AREA_INDEX_LIMIT    8
#define AE_ZAKO_TYPE_EXEMPT_KILL    0x4Cu

/* ZED header and spawn-entry fields. */
#define AE_ZED_MAGIC                0x44455A2Eu
#define AE_ZED_ENTRY_COUNT          0x04u
#define AE_ZED_PATH_COUNT           0x06u
#define AE_ZED_ENTRY_OFFSET         0x08u
#define AE_ZED_PATH_OFFSET          0x0Cu
#define AE_ZED_ENTRY_SIZE           0x20u
#define AE_ZED_ENTRY_TYPE           0x00u
#define AE_ZED_ENTRY_X              0x02u
#define AE_ZED_ENTRY_Y              0x04u
#define AE_ZED_ENTRY_Z              0x06u
#define AE_ZED_ENTRY_SENSE_RADIUS   0x08u
#define AE_ZED_ENTRY_RADIUS         0x0Au
#define AE_ZED_ENTRY_COLLIDER       0x0Cu
#define AE_ZED_ENTRY_PATH_INDEX     0x0Eu
#define AE_ZED_ENTRY_DEFEATED_BITS  0x10u
#define AE_ZED_ENTRY_ACTIVE         0x16u
#define AE_ZED_ENTRY_MODEL_OFFSET   0x18u
#define AE_ZED_ENTRY_ANIM_OFFSET    0x1Cu
#define AE_ZED_PATH_ENTRY_SIZE      0x08u
#define AE_ZED_PATH_POINT_COUNT     0x00u
#define AE_ZED_PATH_DATA_OFFSET     0x04u
#define AE_ANIM_HEADER_CLIP_TABLE   0x04u
#define AE_ANIM_CLIP_RECORD_SIZE    0x04u

/* Actor and object-table fields. */
#define AE_ZAKO_OBJECT_COUNT        16u
#define AE_ZAKO_OBJECT_SIZE         0xE4u
#define AE_OBJ_MODEL                0x000u
#define AE_OBJ_COLORS               0x004u
#define AE_OBJ_PRIMS                0x008u
#define AE_OBJ_BONES                0x00Cu
#define AE_OBJ_VERTS                0x010u
#define AE_OBJ_BONE_MATRICES        0x014u
#define AE_OBJ_ANIM_SET             0x018u
#define AE_OBJ_TARGET_NODE          0x01Cu
#define AE_OBJ_POS_X                0x024u
#define AE_OBJ_POS_Y                0x028u
#define AE_OBJ_POS_Z                0x02Cu
#define AE_OBJ_BODY_RADIUS          0x044u
#define AE_OBJ_BODY_HEIGHT          0x046u
#define AE_OBJ_BODY_PARAM           0x048u
#define AE_OBJ_BODY_FLAGS           0x04Au
#define AE_OBJ_UNK_66               0x066u
#define AE_OBJ_BODY_SLOT            0x04Cu
#define AE_OBJ_TARGET_BODY          0x050u
#define AE_OBJ_ANIM_FRAME           0x05Cu
#define AE_OBJ_ANIM_INDEX           0x05Eu
#define AE_OBJ_FACING_X             0x060u
#define AE_OBJ_FACING_Y             0x062u
#define AE_OBJ_FACING_Z             0x064u
#define AE_OBJ_OWN_COLLIDER         0x070u
#define AE_OBJ_LINK0                0x074u
#define AE_OBJ_LINK1                0x078u
#define AE_OBJ_LOOP_SLOT            0x07Cu
#define AE_OBJ_DEFENSE              0x07Du
#define AE_OBJ_TYPE                 0x07Eu
#define AE_OBJ_ANIM_SLOT            0x07Fu
#define AE_OBJ_AI_STATE             0x080u
#define AE_OBJ_HP                   0x081u
#define AE_OBJ_MOTION_STATE         0x082u
#define AE_OBJ_LAST_HIT_TYPE        0x083u
#define AE_OBJ_FLASH_TIMER          0x084u
#define AE_OBJ_MOTION_TIMER         0x085u
#define AE_OBJ_ANIM_TABLE           0x088u
#define AE_OBJ_PATH_POINT           0x08Cu
#define AE_OBJ_PATH_INDEX           0x08Eu
#define AE_OBJ_SUB_STATE            0x08Fu
#define AE_OBJ_PATH                 0x090u
#define AE_OBJ_VELOCITY_X           0x094u
#define AE_OBJ_VELOCITY_Y           0x096u
#define AE_OBJ_VELOCITY_Z           0x098u
#define AE_OBJ_TARGET_X             0x09Au
#define AE_OBJ_HOME_Y               0x09Cu
#define AE_OBJ_TARGET_Z             0x09Eu
#define AE_OBJ_SHADOW_SIZE          0x0A0u
#define AE_OBJ_SENSE_RADIUS         0x0A4u
#define AE_OBJ_RADIUS               0x0A6u
#define AE_OBJ_SPEED                0x0A8u
#define AE_OBJ_ANGLE                0x0AAu
#define AE_OBJ_HEADING              0x0ACu
#define AE_OBJ_TIMER                0x0AEu
#define AE_OBJ_TURN_SPEED           0x0B2u
#define AE_OBJ_UNK_B0               0x0B0u
#define AE_OBJ_HIT_TARGET            0x0B4u
#define AE_OBJ_HIT_TARGET_AUX        0x0B8u
#define AE_OBJ_UNK_BC                0x0BCu
#define AE_OBJ_UNK_C0                0x0C0u
#define AE_OBJ_UNK_C2                0x0C2u
#define AE_OBJ_UNK_C6                0x0C6u
#define AE_OBJ_SCALE                0x0C8u
#define AE_OBJ_COOLDOWN             0x0C9u
#define AE_OBJ_SPAWN_SLOT            0x0CAu
#define AE_OBJ_SLOT                  0x0CBu
#define AE_OBJ_SPAWN_INDEX           0x0CCu
#define AE_OBJ_REACT_ANIM            0x0CDu
#define AE_OBJ_PREV_ANIM_SLOT        0x0CEu
#define AE_OBJ_GROUND_HEIGHT         0x0D0u
#define AE_OBJ_UNK_D4               0x0D4u
#define AE_OBJ_UNK_D6               0x0D6u
#define AE_OBJ_KNOCKBACK             0x0D8u
#define AE_OBJ_KNOCK_DECAY           0x0DAu
#define AE_OBJ_HIT_COUNT             0x0DBu
#define AE_OBJ_DROP_TABLE            0x0DCu
#define AE_OBJ_IDLE_PHASE            0x0DFu
#define AE_OBJ_FLAGS                 0x0E0u

#define AE_OBJ_FLAG_SPAWNED          0x00000010u
#define AE_OBJ_FLAG_BODY_ACTIVE      0x00004000u
#define AE_OBJ_FLAG_TARGET_LISTED    0x08000000u
#define AE_OBJ_FLAG_AREA_MODE        0x00040000u
#define AE_OBJ_FLAG_BODY_UNREGISTERED 0x00000002u
#define AE_OBJ_FLAG_TARGET_TRACKED   0x08000000u
#define AE_OBJ_FLAG_UNK_19           0x00080000u
#define AE_OBJ_FLAG_UNK_20           0x00100000u
#define AE_OBJ_FLAG_UNK_21           0x00200000u
#define AE_OBJ_FLAG_UNK_17           0x00020000u
#define AE_OBJ_FLAG_UNK_22_23        0x00C00000u
#define AE_OBJ_FLAG_UNK_26           0x04000000u
#define AE_OBJ_FLAG_UNK_7            0x00000080u
#define AE_OBJ_FLAG_UNK_3            0x00000008u
#define AE_OBJ_FLAG_UNK_6            0x00000040u
#define AE_OBJ_FLAG_UNK_9            0x00000200u
#define AE_OBJ_FLAG_UNK_24_25        0x03000000u
#define AE_OBJ_FLAG_UNK_12           0x00001000u
#define AE_OBJ_FLAG_UNK_13           0x00002000u
#define AE_OBJ_FLAG_UNK_15           0x00008000u
#define AE_ZAKO_NO_SPAWN_INDEX       0xFFu

/* Model header and auto-derived instance pointers. */
#define AE_MODEL_HEADER_BONE_COUNT   0x04u
#define AE_MODEL_HEADER_VERTEX_COUNT 0x06u
#define AE_MODEL_HEADER_COLOR_COUNT  0x08u
#define AE_MODEL_HEADER_HAS_NORMALS  0x0Au
#define AE_MODEL_HEADER_DATA         0x0Cu
#define AE_MODEL_VERTEX_STRIDE       0x08u
#define AE_MODEL_NORMAL_VERTEX_STRIDE 0x10u

/* Debug menu item records. */
#define AE_MENU_ITEM_SIZE            0x24u
#define AE_MENU_ITEM_VARIABLE        0x08u
#define AE_MENU_ITEM_READ_CALLBACK   0x0Cu
#define AE_MENU_ITEM_VALUE           0x18u
#define AE_MENU_ITEM_RANGE           0x1Cu
#define AE_MENU_ITEM_SIZE_KIND       0x20u
#define AE_MENU_ITEM_FIRST_RANGE     0x10u
#define AE_MENU_ITEM_SECOND_RANGE    0x14u
#define AE_MENU_ITEM_TOGGLE_CALLBACK 0x8007D110u

void ae_ModelInitAuto(AeRam *r, uint32_t instance, uint32_t model_header); /* 80015BA0 */
int32_t ae_ZakoTypeIsIgnored(AeRam *r, int32_t type); /* 80050580 */
void ae_ZakoInit(AeRam *r, const AeZakoHooks *h); /* 800505C8 */
void ae_ZakoSpawnFromEntry(AeRam *r, const AeZakoHooks *h, uint32_t obj,
                           uint32_t entry, int32_t entry_index); /* 800508B8 */
void ae_ZakoClearSlotTable(AeRam *r); /* 800509A4 */
void ae_ZakoSetupType(AeRam *r, const AeZakoHooks *h, uint32_t obj, uint32_t type); /* 80050D04 */
void ae_ObjUnregisterBody(AeRam *r, const AeZakoHooks *h, uint32_t obj); /* 80051E34 */
void ae_ObjKill(AeRam *r, const AeZakoHooks *h, uint32_t obj, int32_t count_as_kill); /* 80051E98 */
int32_t ae_ZakoFindEntryByType(AeRam *r, uint32_t type); /* 80051FE8 */
void ae_ObjInitModel(AeRam *r, uint32_t obj, uint32_t type); /* 80052038 */
uint32_t ae_ZakoGetAnimTable(AeRam *r, uint32_t obj, uint32_t type); /* 800520A8 */
void ae_ZakoSetAnimFromEntry(AeRam *r, uint32_t obj, uint32_t type); /* 80052134 */
void ae_ZakoHooksReset(AeRam *r); /* 80057558 */
void ae_ZakoSetAreaMode(AeRam *r, const AeZakoHooks *h, uint32_t obj,
                        uint32_t type, int32_t entry_index); /* 80057FD0 */
void ae_ObjSetPathFromTable(AeRam *r, uint32_t obj, int32_t table_index); /* 800583BC */
void ae_ZakoDebugMenuInit(AeRam *r, const AeZakoHooks *h); /* 800587EC */
void ae_MenuItemInitToggle(AeRam *r, const AeZakoHooks *h, uint32_t item,
                           uint32_t variable, uint32_t size); /* 8007D23C */
int32_t ae_GetLevelFirstArea(AeRam *r, int32_t level); /* 8009764C */

#endif
