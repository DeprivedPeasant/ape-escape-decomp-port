#ifndef AE_MENUWIN_H
#define AE_MENUWIN_H

#include "../core/ae_ram.h"

typedef struct AePadHooks AePadHooks;
typedef struct AeStageHooks AeStageHooks;
typedef struct AeSoundHw AeSoundHw;
typedef struct AeZako2Hooks AeZako2Hooks;

#ifdef __cplusplus
extern "C" {
#endif

/* Original globals and records used by the menu-window system. */
#define AE_G_MENU_WINDOW_COUNT 0x800BEF60u
#define AE_G_MENU_WINDOW_SELECTION 0x800BEF62u
#define AE_MENU_WINDOWS 0x800F5270u
#define AE_MENU_WINDOW_COUNT_MAX 6u
#define AE_MENU_WINDOW_SIZE 0x2Cu

#define AE_MENU_WINDOW_POSITION 0x00u
#define AE_MENU_WINDOW_ITEMS 0x04u
#define AE_MENU_WINDOW_WIDTH 0x00u
#define AE_MENU_WINDOW_HEIGHT 0x02u
#define AE_MENU_WINDOW_DRAW_STATE 0x0Cu
#define AE_MENU_WINDOW_ITEM_COUNT 0x0Eu
#define AE_MENU_WINDOW_SELECTED_ITEM 0x0Fu
#define AE_MENU_WINDOW_ITEM_WIDTH 0x0Au
#define AE_MENU_WINDOW_ROW_HEIGHT 0x16u
#define AE_MENU_WINDOW_FRAME_WIDTH 0x18u
#define AE_MENU_WINDOW_FRAME_HEIGHT 0x1Au
#define AE_MENU_WINDOW_STYLE 0x1Cu
#define AE_MENU_WINDOW_DRAW_COLOR_A 0x0Cu
#define AE_MENU_WINDOW_DRAW_COLOR_B 0x0Du
#define AE_MENU_WINDOW_DRAW_COLOR_C 0x10u
#define AE_MENU_WINDOW_DRAW_COLOR_D 0x12u
#define AE_MENU_WINDOW_STYLE_BYTE 0x1Eu
#define AE_MENU_WINDOW_STYLE_FLAG 0x1Fu
#define AE_MENU_WINDOW_TEXT_HEIGHT 0x14u
#define AE_MENU_WINDOW_CONTENT_WIDTH 0x08u
#define AE_MENU_WINDOW_RIGHT_EDGE 0x22u
#define AE_MENU_WINDOW_BOTTOM_EDGE 0x24u
#define AE_MENU_WINDOW_ANIMATION_X 0x10u
#define AE_MENU_WINDOW_ANIMATION_Y 0x12u
#define AE_MENU_WINDOW_MAX_TEXT_HEIGHT 0x0Au
#define AE_MENU_WINDOW_CONTENT_HEIGHT 0x14u
#define AE_MENU_WINDOW_ROW_STEP 0x16u
#define AE_MENU_WINDOW_RENDER_WIDTH 0x18u
#define AE_MENU_WINDOW_RENDER_HEIGHT 0x1Au
#define AE_MENU_WINDOW_ITEM_FLAGS 0x1Cu
#define AE_MENU_WINDOW_DISPLAY_FLAGS 0x1Eu
#define AE_MENU_WINDOW_OPEN 0x0Cu
#define AE_MENU_WINDOW_ANIMATION 0x0Du
#define AE_MENU_WINDOW_ITEM_TOTAL 0x0Eu
#define AE_MENU_WINDOW_ACTIVE_ITEM 0x0Fu
#define AE_MENU_WINDOW_SELECTED_VALUE 0x28u

#define AE_MENU_RENDER_CONTEXT 0x800EC350u
#define AE_MENU_RENDER_PRIMITIVE 0x04u
#define AE_MENU_RENDER_ORDERING_TABLE 0x08u
#define AE_MENU_RENDER_BUFFER_INDEX 0x0Cu
#define AE_MENU_RENDER_BUFFER_STRIDE 0x4078u
#define AE_MENU_RENDER_BUFFER_DRAW_MODE 0x6Eu
#define AE_MENU_RENDER_BUFFER_AREA_Y 0x12u
#define AE_MENU_RENDER_CURSOR_PACKET 0x403Cu
#define AE_MENU_RENDER_PRIMITIVE_ADVANCE 0x18u
#define AE_MENU_RENDER_TPAGE_PRIMITIVE_OFFSET 0x0Cu
#define AE_MENU_RENDER_BUFFER_DRAWENV_OFFSET 0x10u
#define AE_MENU_RENDER_BUFFER_DISPENV_OFFSET 0x6Cu
#define AE_MENU_RENDER_BUFFER_DATA_OFFSET 0x80u
#define AE_MENU_RENDER_OT_TERMINATOR_OFFSET 0x3FFCu
#define AE_MENU_SUBMENU_DEFINITION_OFFSET 0x40u
#define AE_MENU_STACK_ARGUMENT_AREA_OFFSET 0x10u
#define AE_MENU_STACK_CURSOR_MATRIX_OFFSET 0x18u
#define AE_MENU_STACK_CURSOR_TRANSLATE_X_OFFSET 0x2Cu
#define AE_MENU_STACK_CURSOR_TRANSLATE_Y_OFFSET 0x30u
#define AE_MENU_STACK_CURSOR_TRANSLATE_Z_OFFSET 0x34u
#define AE_MENU_STACK_CURSOR_ANGLES_OFFSET 0x38u
#define AE_MENU_STACK_CURSOR_SCALE_OFFSET 0x40u
#define AE_MENU_CURSOR_MODEL_TRANSFORM 0x800F4C50u
#define AE_MENU_CURSOR_MODEL 0x800E7D00u
#define AE_MENU_CHILD_EXIT_ITEMS 0x800B21D0u
#define AE_MENU_DEBUG_FORMAT_VALUE 0x800A60C0u
#define AE_MENU_DEBUG_FORMAT_LABEL_SELECTED 0x800A60B4u
#define AE_MENU_DEBUG_FORMAT_LABEL 0x800A60BCu
#define AE_MENU_ARGUMENT_FRAME 0x801FFE68u
#define AE_MENU_ARGUMENT_FRAME_WORDS 8u

#define AE_G_MENU_INPUT_REPEAT 0x800BEF64u
#define AE_G_MENU_ITEM_CHOICE 0x800BEF70u
#define AE_G_MENU_ITEM_REPEAT 0x800BEF74u
#define AE_G_MENU_ITEM_ACTIVE 0x800BEF78u
#define AE_G_MENU_STAGE_STATE 0x800F4350u
#define AE_G_MENU_STAGE_AREA 0x0126u
#define AE_G_MENU_STAGE_ENTRY 0x012Au
#define AE_G_MENU_STAGE_MODE 0x012Cu
#define AE_G_MENU_DEBUG_ENABLED 0x800F460Au
#define AE_G_MENU_INPUT_HELD 0x800E5478u
#define AE_G_MENU_INPUT_CHANGED 0x800E5480u
#define AE_G_MENU_INPUT_REPEAT_MASK 0x800E5488u
#define AE_G_MENU_PAD_BUFFER 0x800E52D0u
#define AE_G_MENU_RENDER_CONTEXT_END 0x800F03C8u

#define AE_MENU_ITEM_FLAGS 0x00u
#define AE_MENU_ITEM_LABEL 0x04u
#define AE_MENU_ITEM_VARIABLE 0x08u
#define AE_MENU_ITEM_CALLBACK 0x0Cu
#define AE_MENU_ITEM_MINIMUM 0x10u
#define AE_MENU_ITEM_MAXIMUM 0x14u
#define AE_MENU_ITEM_VALUE 0x18u
#define AE_MENU_ITEM_STEP 0x1Cu
#define AE_MENU_ITEM_VALUE_SIZE 0x20u

#define AE_MENU_DEF_POSITION 0x00u
#define AE_MENU_DEF_ITEMS 0x04u
#define AE_MENU_DEF_ITEM_SIZE 0x24u
#define AE_MENU_DEF_ITEM_FLAGS 0x00u
#define AE_MENU_DEF_ITEM_TEXT 0x04u
#define AE_MENU_DEF_ITEM_INDEX 0x0Eu
#define AE_MENU_DEF_ITEM_SELECTED 0x0Fu

/* Hook types match the callee's actual argument count. */
typedef uint32_t (*AeMenuWinHook0Fn)(void *user, AeRam *r);
typedef uint32_t (*AeMenuWinHook1Fn)(void *user, AeRam *r, uint32_t a0);
typedef uint32_t (*AeMenuWinHook2Fn)(void *user, AeRam *r,
                                     uint32_t a0, uint32_t a1);
typedef uint32_t (*AeMenuWinHook3Fn)(void *user, AeRam *r,
                                     uint32_t a0, uint32_t a1, uint32_t a2);
typedef uint32_t (*AeMenuWinHook4Fn)(void *user, AeRam *r,
                                     uint32_t a0, uint32_t a1,
                                     uint32_t a2, uint32_t a3);

/* A menu item stores its own callback address. The target is passed explicitly because the three
 * original call sites invoke the stored callback with different first arguments. */
typedef uint32_t (*AeMenuWinItemCallbackFn)(void *user, AeRam *r,
                                            uint32_t target, uint32_t a0);

typedef struct AeMenuWinHooks {
    void *user;
    const AePadHooks *pad;             /* direct port: PadUpdate @8001325C */
    const AeStageHooks *stage;         /* direct ports: StageRequest* @8005D100/8005D890 */
    const AeSoundHw *sound;            /* direct port: PlaySfx @800877F8 */
    const AeZako2Hooks *debug_text;    /* direct ports: ColorFadeSet/DebugTextPrintf */
    AeMenuWinHook4Fn SetDrawMove;       /* 8002B380: DR_MOVE, RECT, x, y */
    AeMenuWinHook1Fn DrawPrim;          /* 80028C64: primitive */
    AeMenuWinHook2Fn SetDrawArea;       /* 80029458: DR_AREA, RECT */
    AeMenuWinHook4Fn SetDrawTPage;      /* 8002B410: DR_TPAGE, dfe, dtd, tpage */
    AeMenuWinHook0Fn BeginFrame;        /* 8005DF48: native ordering-table frame boundary */
    AeMenuWinHook0Fn ColorFadeReset;    /* 8008C390: unported game helper */
    AeMenuWinHook1Fn OtLinkSkipEmpty;   /* 8001F3A0: ordering table */
    AeMenuWinHook1Fn DrawSync;          /* 800286CC: PsyQ GPU synchronization mode */
    AeMenuWinHook1Fn VSync;             /* 8002E520: BIOS vertical-sync mode */
    AeMenuWinHook1Fn PutDrawEnv;        /* 80028D30: PsyQ draw environment */
    AeMenuWinHook1Fn PutDispEnv;        /* 80028EFC: PsyQ display environment */
    AeMenuWinHook1Fn DrawOTag;          /* 80028CC0: PsyQ ordering-table entry */
    AeMenuWinHook3Fn ModelTransformRigid; /* 800161C0: transform, matrix, vertex buffer */
    AeMenuWinHook3Fn ModelDraw;         /* 80017CD4: draw context, vertex buffer, transform */
    AeMenuWinHook4Fn DrawMenuChrome;    /* inline GPU packets: 8007D71C..8007D9EF */
    AeMenuWinHook4Fn DrawMenuRows;      /* inline GPU packets: 8007D9F0..8007DF63 */
    AeMenuWinHook4Fn DrawMenuHighlight; /* inline GPU packets: 8007E030..8007E12F */
    AeMenuWinHook4Fn DrawMenuItem;      /* inline GPU packets: 8007E1B8..8007E283 */
    AeMenuWinHook4Fn DrawMenuSelection; /* inline GPU packets: 8007E3F8..8007E5D7 */
    AeMenuWinHook4Fn DrawMenuCursor;    /* inline GPU packets: 8007F148..8007F2DF */
    AeMenuWinItemCallbackFn item_callback;
} AeMenuWinHooks;

int32_t ae_WriteSizedValue(AeRam *r, uint32_t address, uint32_t value, int32_t size); /* 8007D188 */
uint32_t ae_MenuStrLen(AeRam *r, uint32_t string);                                    /* 8007D2AC */
int32_t ae_MenuWindowAdd(AeRam *r, uint32_t definition);                               /* 8007D2D8 */
int32_t ae_MenuWindowsRun(AeRam *r, const AeMenuWinHooks *hooks,
                          uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);         /* 8007D530 */

#ifdef __cplusplus
}
#endif
#endif
