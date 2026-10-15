#include "ae_ovl18b.h"
#include "../core/ae_sdk.h"
#include "../player/ae_player_spawn.h"

#include <limits.h>
#include <string.h>

#define OVL18B_API_INIT_DISPLAY 0x8005CFECu
#define OVL18B_API_RESET_GRAPH 0x80028350u
#define OVL18B_API_CLEAR_IMAGE 0x80028850u
#define OVL18B_API_SET_GEOM_SCREEN 0x8002FAF0u
#define OVL18B_API_SET_COLOR_MATRIX_MODE 0x80015D58u
#define OVL18B_API_ARC_LOAD_ENTRY 0x80011194u
#define OVL18B_API_LOAD_LIST_START 0x80011104u
#define OVL18B_API_LOAD_LIST_STEP 0x800116C0u
#define OVL18B_API_CD_MANAGER_STOP_STREAM 0x8001231Cu
#define OVL18B_API_CD_MANAGER_UPDATE 0x800125B8u
#define OVL18B_API_BEGIN_FRAME 0x8005DF48u
#define OVL18B_API_DRAW_SYNC 0x800286CCu
#define OVL18B_API_DRAW_OTAG 0x80028CC0u
#define OVL18B_API_PUT_DRAW_ENV 0x80028D30u
#define OVL18B_API_PUT_DISPLAY_ENV 0x80028EFCu
#define OVL18B_API_TEXT_SET_CURSOR 0x8008DDB0u
#define OVL18B_API_TEXT_LINE_WIDTH 0x8008DE1Cu
#define OVL18B_API_MESSAGE_DRAW_STRING 0x8008DDC8u
#define OVL18B_API_PLAY_SFX 0x800877F8u
#define OVL18B_API_XA_PLAY 0x80011F6Cu
#define OVL18B_API_MESH_INIT 0x80015C34u
#define OVL18B_API_MODEL_TRANSFORM_SKINNED 0x80016A60u
#define OVL18B_API_MODEL_DRAW 0x80017CD4u
#define OVL18B_API_DRAW_MOVE 0x8002B380u
#define OVL18B_API_SET_DRAW_STP 0x8002B3E0u
#define OVL18B_API_ROT_MATRIX_Z 0x800306F0u
#define OVL18B_API_ROT_MATRIX_Y 0x80030890u
#define OVL18B_API_ROT_MATRIX_X 0x80030A30u
#define OVL18B_API_ROT_TRANS_SV 0x80031230u
#define OVL18B_API_SOUND_STOP_RESTORE_VOLUME 0x80086170u
#define OVL18B_API_LOAD_LIST_STOP 0x8002E520u
#define OVL18B_API_DRAW_SYNC_WAIT 0x800125B8u
#define OVL18B_API_VSYNC 0x8002E520u
#define OVL18B_API_FRAME_FLIP 0x800125B8u
#define OVL18B_API_SOUND_STOP_AND_RESTORE 0x80086170u
#define OVL18B_API_GLYPH_DRAW 0x8008D9D0u
#define OVL18B_API_RENDER_AREA_PROPS 0x80016F34u
#define OVL18B_API_RADAR_DRAW_MODEL 0x8009A574u

#define OVL18B_LOOKAT_FRAME_BYTES 120u
#define OVL18B_LOOKAT_VECTOR 16u
#define OVL18B_LOOKAT_NORMAL 32u
#define OVL18B_LOOKAT_SCALE_MATRIX 48u
#define OVL18B_LOOKAT_SCALE_VECTOR 80u
#define OVL18B_LOOKAT_RENDER_MATRIX 0x800E78C0u
#define OVL18B_LOOKAT_BASE_MATRIX 0x800AF0A0u
#define OVL18B_DRAW_SCENE_FRAME_BYTES 120u
#define OVL18B_DRAW_SCENE_SETTINGS_OFFSET 16u
#define OVL18B_DRAW_SCENE_SETTINGS_WORDS 8u
#define OVL18B_DRAW_SCENE_SETTINGS_DEPTH_OFFSET 20u
#define OVL18B_DRAW_SCENE_SETTINGS_WIDTH_OFFSET 24u
#define OVL18B_DRAW_SCENE_SETTINGS_HEIGHT_OFFSET 28u
#define OVL18B_DRAW_SCENE_SETTINGS_SOURCE 0x800AE840u
#define OVL18B_DRAW_SCENE_GEOMETRY_SCREEN 2248u
#define OVL18B_DRAWBLINK_FRAME_BYTES 40u
#define OVL18B_DRAWMSG_FRAME_BYTES 32u
#define OVL18B_COPYSTRIP_FRAME_BYTES 40u
#define OVL18B_SCANLINE_FRAME_BYTES 72u
#define OVL18B_SCRIPT_FRAME_BYTES 48u
#define OVL18B_SCRIPT_WORD_BYTES 2u
#define OVL18B_POINT_COMPONENT_STRIDE 2u
#define OVL18B_VECTOR_COMPONENT_STRIDE 4u
#define OVL18B_UPDATE_FRAME_BYTES 48u
#define OVL18B_DRAWSCENE_FRAME_BYTES 120u
#define OVL18B_MODE_FRAME_BYTES 64u
#define OVL18B_WIPE_FRAME_BYTES 56u

enum AeOvl18bMatrixField {
    OVL18B_MATRIX_R00 = 0x00,
    OVL18B_MATRIX_R01 = 0x02,
    OVL18B_MATRIX_R02 = 0x04,
    OVL18B_MATRIX_R10 = 0x06,
    OVL18B_MATRIX_R11 = 0x08,
    OVL18B_MATRIX_R12 = 0x0A,
    OVL18B_MATRIX_R20 = 0x0C,
    OVL18B_MATRIX_R21 = 0x0E,
    OVL18B_MATRIX_R22 = 0x10,
    OVL18B_MATRIX_TRANSLATION_X = 0x14,
    OVL18B_MATRIX_TRANSLATION_Y = 0x18,
    OVL18B_MATRIX_TRANSLATION_Z = 0x1C,
    OVL18B_G_VIEW_ROTATION = 0x800E78C0u
};

enum AeOvl18bPointField {
    OVL18B_POINT_X = 0x00,
    OVL18B_POINT_Y = 0x02,
    OVL18B_POINT_Z = 0x04
};

enum AeOvl18bVectorField {
    OVL18B_VECTOR_X = 0x00,
    OVL18B_VECTOR_Y = 0x04,
    OVL18B_VECTOR_Z = 0x08
};

enum AeOvl18bRectangleField {
    OVL18B_RECTANGLE_X = 0x00,
    OVL18B_RECTANGLE_Y = 0x02,
    OVL18B_RECTANGLE_WIDTH = 0x04,
    OVL18B_RECTANGLE_HEIGHT = 0x06
};

enum AeOvl18bScriptWaitEntryField {
    OVL18B_SCRIPT_ENTRY_TABLE_HEADER = 0x04,
    OVL18B_SCRIPT_ENTRY_STRIDE = 0x08,
    OVL18B_SCRIPT_ENTRY_WAIT = 0x02
};

enum AeOvl18bScanlineRecordField {
    OVL18B_SCANLINE_X = 0x00,
    OVL18B_SCANLINE_Y = 0x02,
    OVL18B_SCANLINE_TEXT_INDEX = 0x04,
    OVL18B_SCANLINE_TYPE = 0x06,
    OVL18B_SCANLINE_ENABLED = 0x07
};

enum AeOvl18bModeFrameField {
    OVL18B_MODE_FRAME_CLEAR_RECTANGLE = 0x10,
    OVL18B_SCANLINE_FRAME_RECTANGLE = 0x10
};

enum AeOvl18bClearOrderingTableField {
    OVL18B_CLEAR_OT_FIELD_24 = 0x24,
    OVL18B_CLEAR_OT_FIELD_28 = 0x28,
    OVL18B_CLEAR_OT_FIELD_2C = 0x2C
};

#define OVL18B_MODE_STAGE_ENTRY_SETUP_FIELD 0x08u


#define OVL18B_G_CLEAR_OT 0x800EBFA0u
#define OVL18B_MODE_STAGE_ENTRY 0x801389B4u
#define OVL18B_G_MODE_STAGE_TAG 0x80138880u
#define OVL18B_G_MODE_STAGE_AREA_ENTRY 0x4Cu
#define OVL18B_GLYPH_PACKET_BYTES 40u
#define OVL18B_DRAWBLINK_PACKET_BYTES 24u
#define OVL18B_DRAWSTP_PACKET_BYTES 12u
#define OVL18B_DRAW_MOVE_PACKET_BYTES 24u
#define OVL18B_WIPE_IN_PACKET_BYTES 80u
#define OVL18B_WIPE_OUT_PACKET_BYTES 240u
#define OVL18B_SCANLINE_BASE_PACKET_BYTES (120u * 168u)
#define OVL18B_DRAW_FRAME_RECTANGLE 0x10u
#define OVL18B_UPDATE_FRAME_NORMAL_VECTOR 0x10u

static uint32_t stack_base(const AeOvl18bHooks *h)
{
    return h != NULL && h->stack_pointer != 0u ? h->stack_pointer : 0x801FFF00u;
}

static uint32_t stack_local(const AeOvl18bHooks *h, uint32_t frame_bytes,
                            uint32_t local_offset)
{
    return stack_base(h) - frame_bytes + local_offset;
}

static int32_t bits_s32(uint32_t bits)
{
    int32_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int32_t sign16(uint32_t value)
{
    uint32_t low = value & 0xFFFFu;
    return (low & 0x8000u) != 0u ? (int32_t)(low | 0xFFFF0000u) : (int32_t)low;
}

static int32_t arithmetic_shift_right(int32_t value, uint32_t shift)
{
    uint32_t bits = (uint32_t)value;
    if (shift == 0u) return value;
    bits >>= shift;
    if (value < 0) bits |= 0xFFFFFFFFu << (32u - shift);
    return bits_s32(bits);
}

static int32_t named_call(AeRam *r, const AeOvl18bHooks *h,
                          AeOvl18bNamedHook callback, const char *name,
                          uint32_t address, uint32_t argc,
                          const uint32_t *argv)
{
    if (callback == NULL) {
        ae_ram_fail(r, "AeOvl18b %s hook not provided", name);
        return 0;
    }
    return callback(h != NULL ? h->user : NULL, r, name, address, argc, argv);
}

static int32_t api_call(AeRam *r, const AeOvl18bHooks *h,
                        const char *name, uint32_t address,
                        uint32_t argc, const uint32_t *argv)
{
    return named_call(r, h, h != NULL ? h->native_call : NULL,
                      name, address, argc, argv);
}

static int32_t render_call(AeRam *r, const AeOvl18bHooks *h,
                           const char *name, uint32_t address,
                           uint32_t argc, const uint32_t *argv)
{
    return named_call(r, h, h != NULL ? h->renderer : NULL,
                      name, address, argc, argv);
}

#define API0(r,h,n,a) api_call((r),(h),(n),(a),0u,NULL)
#define API1(r,h,n,a,x0) api_call((r),(h),(n),(a),1u,(uint32_t[]){(uint32_t)(x0)})
#define API2(r,h,n,a,x0,x1) api_call((r),(h),(n),(a),2u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1)})
#define API3(r,h,n,a,x0,x1,x2) api_call((r),(h),(n),(a),3u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2)})
#define API4(r,h,n,a,x0,x1,x2,x3) api_call((r),(h),(n),(a),4u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2),(uint32_t)(x3)})
#define RENDER0(r,h,n,a) render_call((r),(h),(n),(a),0u,NULL)
#define RENDER1(r,h,n,a,x0) render_call((r),(h),(n),(a),1u,(uint32_t[]){(uint32_t)(x0)})
#define RENDER2(r,h,n,a,x0,x1) render_call((r),(h),(n),(a),2u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1)})
#define RENDER3(r,h,n,a,x0,x1,x2) render_call((r),(h),(n),(a),3u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2)})
#define RENDER4(r,h,n,a,x0,x1,x2,x3) render_call((r),(h),(n),(a),4u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2),(uint32_t)(x3)})
#define RENDER6(r,h,n,a,x0,x1,x2,x3,x4,x5) render_call((r),(h),(n),(a),6u,(uint32_t[]){(uint32_t)(x0),(uint32_t)(x1),(uint32_t)(x2),(uint32_t)(x3),(uint32_t)(x4),(uint32_t)(x5)})

static AeOvl18bHooks nested_hooks(const AeOvl18bHooks *h,
                                 uint32_t caller_frame_bytes)
{
    AeOvl18bHooks out;
    if (h != NULL) {
        out = *h;
    } else {
        memset(&out, 0, sizeof(out));
    }
    out.stack_pointer = stack_base(h) - caller_frame_bytes;
    return out;
}

static int32_t maximum_i32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

/* MatrixLookAt-equivalent helper @80136000. Pure matrix arithmetic remains in the SDK ports. */
void ae_StageClearUi_LookAt_18B(AeRam *r, const AeOvl18bHooks *h,
                                uint32_t eye, uint32_t target)
{
    uint32_t frame = stack_local(h, OVL18B_LOOKAT_FRAME_BYTES, 0u);
    uint32_t vector = frame + OVL18B_LOOKAT_VECTOR;
    uint32_t normalized = frame + OVL18B_LOOKAT_NORMAL;
    uint32_t scaled = frame + OVL18B_LOOKAT_SCALE_MATRIX;
    uint32_t scale = frame + OVL18B_LOOKAT_SCALE_VECTOR;
    uint32_t view = AE_OVL18B_G_RENDER_MATRIX;
    int32_t dx = (int32_t)ae_rds16(r, target) - (int32_t)ae_rds16(r, eye);
    int32_t dy = (int32_t)ae_rds16(r, target + OVL18B_POINT_Y) - (int32_t)ae_rds16(r, eye + OVL18B_POINT_Y);
    int32_t dz = (int32_t)ae_rds16(r, target + OVL18B_POINT_Z) - (int32_t)ae_rds16(r, eye + OVL18B_POINT_Z);
    int32_t horizontal_x, horizontal_z, direction_y;
    int32_t horizontal_length;
    uint32_t product;

    ae_wr32(r, view + OVL18B_MATRIX_TRANSLATION_X, 0u - (uint32_t)ae_rds16(r, eye));
    ae_wr32(r, view + OVL18B_MATRIX_TRANSLATION_Y, 0u - (uint32_t)ae_rds16(r, eye + OVL18B_POINT_Y));
    ae_wr32(r, view + OVL18B_MATRIX_TRANSLATION_Z, 0u - (uint32_t)ae_rds16(r, eye + OVL18B_POINT_Z));
    if (dx == 0 && dz == 0) dz = 4096;

    ae_wr32(r, vector, (uint32_t)dx);
    ae_wr32(r, vector + OVL18B_VECTOR_Y, 0u);
    ae_wr32(r, vector + OVL18B_VECTOR_Z, (uint32_t)dz);
    (void)ae_sdk_VectorNormal(r, vector, normalized);
    if (ae_ram_failed(r)) return;
    horizontal_x = ae_rds32(r, normalized);
    horizontal_z = ae_rds32(r, normalized + OVL18B_VECTOR_Z);

    ae_wr32(r, vector + OVL18B_VECTOR_Y, (uint32_t)dy);
    (void)ae_sdk_VectorNormal(r, vector, normalized);
    if (ae_ram_failed(r)) return;
    direction_y = ae_rds32(r, normalized + OVL18B_VECTOR_Y);
    product = (uint32_t)ae_rds32(r, normalized) * (uint32_t)ae_rds32(r, normalized) +
              (uint32_t)ae_rds32(r, normalized + OVL18B_VECTOR_Z) * (uint32_t)ae_rds32(r, normalized + OVL18B_VECTOR_Z);
    horizontal_length = ae_sdk_SquareRoot0(r, bits_s32(product));
    if (ae_ram_failed(r)) return;

    ae_wr16(r, view + OVL18B_MATRIX_R00, (uint32_t)horizontal_z);
    ae_wr16(r, view + OVL18B_MATRIX_R01, 0u);
    ae_wr16(r, view + OVL18B_MATRIX_R02, 0u - (uint32_t)horizontal_x);
    product = (0u - (uint32_t)horizontal_x) * (uint32_t)direction_y;
    ae_wr16(r, view + OVL18B_MATRIX_R10,
            (uint32_t)arithmetic_shift_right(bits_s32(product), 12u));
    ae_wr16(r, view + OVL18B_MATRIX_R11, (uint32_t)horizontal_length);
    product = (uint32_t)direction_y * (uint32_t)horizontal_z;
    ae_wr16(r, view + OVL18B_MATRIX_R12,
            (uint32_t)arithmetic_shift_right(bits_s32(0u - product), 12u));
    product = (uint32_t)horizontal_length * (uint32_t)horizontal_x;
    ae_wr16(r, view + OVL18B_MATRIX_R20,
            (uint32_t)arithmetic_shift_right(bits_s32(product), 12u));
    ae_wr16(r, view + OVL18B_MATRIX_R21, (uint32_t)direction_y);
    product = (uint32_t)horizontal_length * (uint32_t)horizontal_z;
    ae_wr16(r, view + OVL18B_MATRIX_R22,
            (uint32_t)arithmetic_shift_right(bits_s32(product), 12u));
    if (ae_ram_failed(r)) return;

    (void)ae_sdk_apply_matrix_lv(ae_ptr(r, view, 32u),
                                 ae_ptr(r, view + OVL18B_MATRIX_TRANSLATION_X, 12u),
                                 ae_ptr(r, view + OVL18B_MATRIX_TRANSLATION_X, 12u));
    (void)ae_sdk_CompMatrix(r, OVL18B_LOOKAT_BASE_MATRIX, view, view);
    (void)ae_sdk_MatrixIdentity(r, scaled);
    ae_wr32(r, scale, 0x2000u);
    ae_wr32(r, scale + OVL18B_VECTOR_Y, 0x2000u);
    ae_wr32(r, scale + OVL18B_VECTOR_Z, 0x2000u);
    (void)API2(r, h, "ScaleMatrixL", 0x800305B0u, scaled, scale);
    (void)ae_sdk_CompMatrix(r, scaled, view, view);
}
/* The packed string-table walk counts NUL terminators and preserves non-positive indices. */
uint32_t ae_SaveUi_StringTableIndex_18B(AeRam *r, uint32_t strings,
                                        int32_t index)
{
    uint32_t passed = 0u;
    while (index > 0) {
        uint32_t byte = ae_rd8(r, strings);
        if (ae_ram_failed(r)) return strings;
        if (byte == 0u) {
            passed++;
            if ((int32_t)passed >= index) return strings + 1u;
        }
        strings++;
    }
    return strings;
}

/* The area member's object table is selected by its halfword table index at +0x1C. */
void ae_SpecialEnterUi_SetAreaTablePointer(AeRam *r)
{
    uint32_t area = AE_AREA_BASE;
    uint32_t index = ae_rd16(r, area + AE_OVL18B_AREA_TABLE_INDEX);
    ae_wr32(r, AE_OVL18B_G_AREA_TABLE, area + index * 4u);
}

/* The original calls TextLineWidth for the first line and the text after the last CR. */
int32_t ae_SpecialEnterUi_TextWidth(AeRam *r, const AeOvl18bHooks *h,
                                    uint32_t text)
{
    int32_t width = API1(r, h, "TextLineWidth", OVL18B_API_TEXT_LINE_WIDTH, text);
    uint32_t cursor = text;
    uint32_t after_last_cr = 0u;

    while (!ae_ram_failed(r)) {
        uint32_t ch = ae_rd8(r, cursor++);
        if (ch == 0u) break;
        if (ch == 13u) after_last_cr = cursor;
    }
    if (ae_ram_failed(r)) return 0;
    if (after_last_cr != 0u) {
        int32_t last_width = API1(r, h, "TextLineWidth",
                                  OVL18B_API_TEXT_LINE_WIDTH, after_last_cr);
        width = maximum_i32(width, last_width);
    }
    return arithmetic_shift_right(bits_s32(384u - (uint32_t)width), 1u);
}

/* XA streaming remains on the host audio boundary. */
void ae_SpecialEnterUi_PlayXa(AeRam *r, const AeOvl18bHooks *h)
{
    uint32_t xa_index = ae_rd32(r, AE_OVL18B_G_XA_INDEX);
    if (xa_index != 0u) {
        (void)API1(r, h, "XaPlay", OVL18B_API_XA_PLAY, xa_index);
    }
}

/* Ported MessageBoxDrawString is called directly; only TextSetCursor crosses the UI boundary. */
void ae_SpecialEnterUi_DrawMessageShadow(AeRam *r, const AeOvl18bHooks *h,
                                         uint32_t text)
{
    uint32_t ui = AE_OVL18B_G_UI_STATE;
    int32_t x = ae_rds16(r, ui + AE_OVL18B_UI_TEXT_X);
    int32_t y = ae_rds16(r, ui + AE_OVL18B_UI_TEXT_Y);
    if (h == NULL || h->pause_menu == NULL) {
        ae_ram_fail(r, "AeOvl18b pause-menu hooks not provided");
        return;
    }
    AePauseMenuHooks pause_hooks = *h->pause_menu;
    pause_hooks.stack_pointer = stack_base(h) - OVL18B_DRAWMSG_FRAME_BYTES;
    (void)API2(r, h, "TextSetCursor", OVL18B_API_TEXT_SET_CURSOR,
               (uint32_t)x, (uint32_t)y);
    ae_MessageBoxDrawString(r, &pause_hooks, text);
    if (ae_ram_failed(r)) return;
    (void)API2(r, h, "TextSetCursor", OVL18B_API_TEXT_SET_CURSOR,
               (uint32_t)sign16((uint32_t)x + 1u), (uint32_t)y);
    ae_MessageBoxDrawString(r, &pause_hooks, text);
}

/* Ordering-table insertion is performed by the host renderer. */
static void render_otag_link(AeRam *r, const AeOvl18bHooks *h,
                             uint32_t ordering_tag, uint32_t primitive)
{
    (void)RENDER2(r, h, "RendererLinkOtag", 0x80137D04u,
                  ordering_tag, primitive);
}

static uint32_t ot_packet_for_frame(AeRam *r)
{
    uint32_t index = ae_rd32(r, AE_OVL18B_G_FRAME_OT_INDEX);
    return AE_OVL18B_G_OT_PACKET_BASE + index * AE_OVL18B_OT_PACKET_STRIDE;
}

void ae_StageClearUi_SetDrawStp_18B(AeRam *r, const AeOvl18bHooks *h)
{
    uint32_t cursor = ae_rd32(r, AE_OVL18B_G_PRIMITIVE_CURSOR);
    (void)RENDER2(r, h, "SetDrawStp", OVL18B_API_SET_DRAW_STP, cursor, 1u);
    if (ae_ram_failed(r)) return;
    render_otag_link(r, h, ot_packet_for_frame(r), cursor);
    ae_wr32(r, AE_OVL18B_G_PRIMITIVE_CURSOR, cursor + OVL18B_DRAWSTP_PACKET_BYTES);
}

void ae_StageClearUi_CopyStrip_18B(AeRam *r, const AeOvl18bHooks *h,
                                   int32_t index)
{
    uint32_t cursor = ae_rd32(r, AE_OVL18B_G_PRIMITIVE_CURSOR);
    uint32_t rect = stack_local(h, OVL18B_COPYSTRIP_FRAME_BYTES, OVL18B_DRAW_FRAME_RECTANGLE);
    ae_wr16(r, rect, 16u);
    ae_wr16(r, rect + OVL18B_RECTANGLE_Y, (uint32_t)(index * 64 + 64));
    ae_wr16(r, rect + OVL18B_RECTANGLE_WIDTH, 16u);
    ae_wr16(r, rect + OVL18B_RECTANGLE_HEIGHT, 64u);
    (void)RENDER4(r, h, "DrawMove", OVL18B_API_DRAW_MOVE,
                  cursor, rect, 560u, 0u);
    if (ae_ram_failed(r)) return;
    render_otag_link(r, h, ot_packet_for_frame(r), cursor);
    ae_wr32(r, AE_OVL18B_G_PRIMITIVE_CURSOR, cursor + OVL18B_DRAW_MOVE_PACKET_BYTES);
}
/* Preserve the original view-matrix store; the remaining body is native rendering. */
void ae_SpecialEnterUi_DrawScene(AeRam *r, const AeOvl18bHooks *h)
{
    uint32_t settings = stack_local(h, OVL18B_DRAW_SCENE_FRAME_BYTES,
                                    OVL18B_DRAW_SCENE_SETTINGS_OFFSET);
    uint32_t frame_index = ae_rd32(r, AE_OVL18B_G_FRAME_OT_INDEX);
    uint32_t instances = AE_OVL18B_G_FRAME_MODEL_INSTANCES +
                         frame_index * AE_OVL18B_OT_PACKET_STRIDE;
    uint32_t otag = AE_OVL18B_G_OT_PACKET_BASE +
                    frame_index * AE_OVL18B_OT_PACKET_STRIDE;
    uint32_t arguments[9] = {
        OVL18B_DRAW_SCENE_GEOMETRY_SCREEN,
        AE_OVL18B_G_RENDER_MATRIX,
        settings,
        instances,
        AE_OVL18B_G_MODEL_DRAW_BASE,
        ae_rd32(r, AE_OVL18B_G_DISPLAY_MODE),
        AE_OVL18B_G_UI_OBJECTS,
        otag,
        ae_rd32(r, AE_OVL18B_G_PRIMITIVE_CURSOR)
    };

    for (uint32_t word = 0u; word < OVL18B_DRAW_SCENE_SETTINGS_WORDS; ++word) {
        uint32_t offset = word * 4u;
        ae_wr32(r, AE_OVL18B_G_RENDER_MATRIX + offset,
                ae_rd32(r, OVL18B_LOOKAT_BASE_MATRIX + offset));
    }
    for (uint32_t word = 0u; word < OVL18B_DRAW_SCENE_SETTINGS_WORDS; ++word) {
        uint32_t offset = word * 4u;
        ae_wr32(r, settings + offset,
                ae_rd32(r, OVL18B_DRAW_SCENE_SETTINGS_SOURCE + offset));
    }
    ae_wr32(r, settings + OVL18B_DRAW_SCENE_SETTINGS_DEPTH_OFFSET, 64u);
    ae_wr32(r, settings + OVL18B_DRAW_SCENE_SETTINGS_WIDTH_OFFSET, 16u);
    ae_wr32(r, settings + OVL18B_DRAW_SCENE_SETTINGS_HEIGHT_OFFSET, 328u);

    (void)render_call(r, h, "SpecialEnterUi_RenderScene", 0x801381D8u,
                      9u, arguments);
}




/* Text packets are built by the renderer; C retains string traversal and cursor arithmetic. */
void ae_SpecialEnterUi_DrawText(AeRam *r, const AeOvl18bHooks *h,
                                uint32_t text)
{
    uint32_t ui = AE_OVL18B_G_UI_STATE;
    uint32_t cursor = ae_rd32(r, AE_OVL18B_G_PRIMITIVE_CURSOR);
    uint32_t glyph_widths = AE_OVL18B_G_GLYPH_TABLE;
    uint32_t ordering_tag = ae_rd32(r, ui + AE_OVL18B_UI_RENDER_RESULT);
    int32_t x = ae_rds16(r, ui + AE_OVL18B_UI_TEXT_X);
    int32_t y = ae_rds16(r, ui + AE_OVL18B_UI_TEXT_Y);
    int32_t spacing = ae_rds32(r, ui + AE_OVL18B_UI_TEXT_SPACING);

    while (!ae_ram_failed(r)) {
        uint32_t glyph = ae_rd8(r, text);
        uint32_t arguments[9];
        int32_t advance;
        int32_t moved;
        if (glyph == 0u || ae_ram_failed(r)) break;
        arguments[0] = cursor;
        arguments[1] = glyph;
        arguments[2] = (uint32_t)x;
        arguments[3] = (uint32_t)y;
        arguments[4] = (uint32_t)spacing;
        arguments[5] = glyph_widths + glyph;
        arguments[6] = ae_rd16(r, ui + AE_OVL18B_UI_TEXTURE_PAGE);
        arguments[7] = ae_rd16(r, ui + AE_OVL18B_UI_FONT_ROW);
        arguments[8] = ordering_tag;
        (void)render_call(r, h, "SpecialEnterUi_DrawGlyph", 0x80137214u,
                          9u, arguments);
        if (ae_ram_failed(r)) return;
        advance = (int32_t)((uint32_t)ae_rd8(r, glyph_widths + glyph) *
                            (uint32_t)spacing);
        moved = advance < 0 ? bits_s32((uint32_t)advance + 15u) : advance;
        x = bits_s32((uint32_t)x + (uint32_t)arithmetic_shift_right(moved, 4u));
        cursor += OVL18B_GLYPH_PACKET_BYTES;
        text++;
    }
    if (ae_ram_failed(r)) return;
    ae_wr16(r, ui + AE_OVL18B_UI_TEXT_X, (uint32_t)x);
    ae_wr32(r, AE_OVL18B_G_PRIMITIVE_CURSOR, cursor);
}

void ae_SpecialEnterUi_DrawBlink(AeRam *r, const AeOvl18bHooks *h)
{
    int32_t frame = ae_rds32(r, AE_OVL18B_G_BLINK_FRAME);
    int32_t remainder;
    uint32_t type = 0u;
    uint32_t cursor;
    uint32_t rect;
    int32_t frame_index;

    if (frame < 6 || frame > 99) return;
    remainder = frame % 15;
    if (remainder == 0) type = 1u;
    else if (remainder == 7) type = 2u;
    if (type == 0u) return;

    cursor = ae_rd32(r, AE_OVL18B_G_PRIMITIVE_CURSOR);
    rect = stack_local(h, OVL18B_DRAWBLINK_FRAME_BYTES, OVL18B_DRAW_FRAME_RECTANGLE);
    ae_wr16(r, rect, 560u);
    ae_wr16(r, rect + OVL18B_RECTANGLE_Y, type == 1u ? 112u : 176u);
    ae_wr16(r, rect + OVL18B_RECTANGLE_WIDTH, 16u);
    ae_wr16(r, rect + OVL18B_RECTANGLE_HEIGHT, 16u);
    frame_index = ae_rds32(r, AE_OVL18B_G_FRAME_OT_INDEX);
    (void)RENDER4(r, h, "DrawMove", OVL18B_API_DRAW_MOVE,
                  cursor, rect, 560u, 48u);
    if (ae_ram_failed(r)) return;
    render_otag_link(r, h, AE_OVL18B_G_OT_PACKET_BASE +
                     (uint32_t)frame_index * AE_OVL18B_OT_PACKET_STRIDE, cursor);
    ae_wr32(r, AE_OVL18B_G_PRIMITIVE_CURSOR, cursor + OVL18B_DRAW_MOVE_PACKET_BYTES);
}

static uint32_t lcg_step(AeRam *r)
{
    uint32_t next = (ae_rd16(r, AE_OVL18B_G_RANDOM_SEED) * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_OVL18B_G_RANDOM_SEED, next);
    return next;
}

static uint32_t random_modulo(AeRam *r, uint32_t modulus)
{
    return ((lcg_step(r) >> 8) % modulus);
}

/* Packet layout is native; phase, RNG, strip choice, and cursor remain game state. */
void ae_StageClearUi_DrawScanlines_18B(AeRam *r,
                                       const AeOvl18bHooks *h)
{
    uint32_t cursor = ae_rd32(r, AE_OVL18B_G_PRIMITIVE_CURSOR);
    uint32_t frame_index = ae_rd32(r, AE_OVL18B_G_FRAME_OT_INDEX);
    uint32_t ordering_tag = ae_rd32(r, AE_OVL18B_G_FRAME_DRAW_CONTEXT);
    uint32_t phase = ae_rd32(r, AE_OVL18B_G_SCANLINE_PHASE);
    uint32_t threshold = 10u;
    uint32_t frame = stack_local(h, OVL18B_SCANLINE_FRAME_BYTES, 0u);
    uint32_t rect = frame + OVL18B_SCANLINE_FRAME_RECTANGLE;
    uint32_t screen_y = frame_index != 0u ? 256u : 0u;
    uint32_t base_args[4] = {
        cursor, ae_rd32(r, AE_OVL18B_G_FRAME_COUNTER), frame_index, ordering_tag
    };

    (void)render_call(r, h, "RendererBuildScanlineBase", 0x80138494u,
                      4u, base_args);
    if (ae_ram_failed(r)) return;
    cursor += OVL18B_SCANLINE_BASE_PACKET_BYTES;

    if (bits_s32(phase) >= 11) {
        threshold = 0u;
        ae_wr32(r, AE_OVL18B_G_SCANLINE_PHASE, phase - 1u);
    } else if (bits_s32(phase) > 0) {
        ae_wr32(r, AE_OVL18B_G_SCANLINE_PHASE, phase - 1u);
    } else {
        phase = random_modulo(r, 200u) + 20u;
        ae_wr32(r, AE_OVL18B_G_SCANLINE_PHASE, phase);
    }

    for (uint32_t row = 0u; row < 240u; row += 2u) {
        uint32_t skip_value = random_modulo(r, 100u);
        if (skip_value <= threshold) {
            uint32_t line_height = random_modulo(r, 4u) + 2u;
            ae_wr16(r, rect, 0u - line_height);
            ae_wr16(r, rect + OVL18B_RECTANGLE_Y, 254u);
            ae_wr16(r, rect + OVL18B_RECTANGLE_WIDTH, 384u + line_height);
            ae_wr16(r, rect + OVL18B_RECTANGLE_HEIGHT, 1u);
            (void)RENDER4(r, h, "DrawMove", OVL18B_API_DRAW_MOVE,
                          cursor, rect, line_height, screen_y);
            if (ae_ram_failed(r)) return;
            render_otag_link(r, h, ordering_tag, cursor);
            cursor += OVL18B_DRAW_MOVE_PACKET_BYTES;

            ae_wr16(r, rect, 0u);
            ae_wr16(r, rect + OVL18B_RECTANGLE_Y, screen_y);
            ae_wr16(r, rect + OVL18B_RECTANGLE_WIDTH, 384u);
            ae_wr16(r, rect + OVL18B_RECTANGLE_HEIGHT, 1u);
            (void)RENDER4(r, h, "DrawMove", OVL18B_API_DRAW_MOVE,
                          cursor, rect, 0u, 254u);
            if (ae_ram_failed(r)) return;
            render_otag_link(r, h, ordering_tag, cursor);
            cursor += OVL18B_DRAW_MOVE_PACKET_BYTES;
        }
        if (ae_ram_failed(r)) return;
        screen_y += 2u;
    }
    ae_wr32(r, AE_OVL18B_G_PRIMITIVE_CURSOR, cursor);
}

#define AE_OVL18B_OBJ_ANIMATION_TARGET 0x70u
#define OVL18B_OBJ_DIRECTION_MATRIX_PTR 0x14u
#define OVL18B_OBJ_ACTIVE_FLAGS 0x72u
#define AE_OVL18B_OBJ_SIZE 0x88u
#define AE_OVL18B_G_UI_OBJECTS 0x80138D00u
#define OVL18B_SCRIPT_END 0x3FC0u
#define OVL18B_SCRIPT_OP_MASK 0x3FC0u
#define OVL18B_SCRIPT_INDEX_MASK 0x003Fu
#define OVL18B_SCRIPT_SCANLINE_STRIDE 8u
#define OVL18B_SCRIPT_FRAME_BYTES 48u
#define OVL18B_SCRIPT_WORD_BYTES 2u
#define OVL18B_POINT_COMPONENT_STRIDE 2u
#define OVL18B_VECTOR_COMPONENT_STRIDE 4u
#define OVL18B_OBJECT_FRAME_BYTES 48u

static uint32_t script_object(uint32_t index)
{
    return AE_OVL18B_G_UI_OBJECTS + index * AE_OVL18B_OBJ_SIZE;
}

static int32_t divide_truncating(AeRam *r, int32_t numerator, int32_t denominator)
{
    if (denominator == 0) {
        ae_ram_fail(r, "AeOvl18b signed division by zero");
        return 0;
    }
    if (numerator == INT32_MIN && denominator == -1) {
        ae_ram_fail(r, "AeOvl18b signed division overflow");
        return 0;
    }
    return numerator / denominator;
}

/* Dispatch the readable script opcodes and apply their original field writes. */
void ae_SpecialEnterUi_RunScript(AeRam *r, const AeOvl18bHooks *h)
{
    uint32_t ui = AE_OVL18B_G_UI_STATE;
    uint32_t script = ae_rd32(r, ui + AE_OVL18B_UI_SCRIPT);
    uint32_t flags = ae_rd32(r, ui + AE_OVL18B_UI_SCRIPT_FLAGS);

    if ((flags & 1u) != 0u) {
        uint32_t entries = ae_rd32(r, ui + AE_OVL18B_UI_OBJECT_LIST);
        int32_t wait = ae_rds32(r, ui + AE_OVL18B_UI_SCRIPT_WAIT);
        int32_t next_wait = wait > 0xFFFE ? wait : wait + 1;
        int32_t count = ae_rds32(r, entries);
        uint32_t matched = 0u;
        for (int32_t i = 0; i < count; ++i) {
            if (ae_rds16(r, entries + OVL18B_SCRIPT_ENTRY_TABLE_HEADER + (uint32_t)i * OVL18B_SCRIPT_ENTRY_STRIDE + OVL18B_SCRIPT_ENTRY_WAIT) >= next_wait) {
                matched = 1u;
            }
        }
        ae_wr32(r, ui + AE_OVL18B_UI_SCRIPT_WAIT, (uint32_t)next_wait);
        if (matched == 0u) {
            flags = ae_rd32(r, ui + AE_OVL18B_UI_SCRIPT_FLAGS) | 2u;
            ae_wr32(r, ui + AE_OVL18B_UI_SCRIPT_FLAGS, flags);
        }
    }

    while (!ae_ram_failed(r)) {
        int32_t advance_to_next_frame = 0;
        uint32_t instruction = ae_rd16(r, script);
        uint32_t operation = instruction & OVL18B_SCRIPT_OP_MASK;
        uint32_t index = instruction & OVL18B_SCRIPT_INDEX_MASK;
        uint32_t object;
        uint32_t operand;
        int32_t signed_operand;

        if (instruction == OVL18B_SCRIPT_END) break;
        switch (operation) {
        case 0x0000u:
            if (index < 5u) {
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, script_object(index) + OVL18B_OBJ_ACTIVE_FLAGS, ae_rd16(r, script));
            }
            break;
        case 0x0040u:
            if (index < 5u) {
                object = script_object(index);
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, object + AE_OVL18B_OBJ_POSITION_TARGET_X, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, object + AE_OVL18B_OBJ_POSITION_TARGET_Y, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, object + AE_OVL18B_OBJ_POSITION_TARGET_Z, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, object + AE_OVL18B_OBJ_POSITION_REMAINING, ae_rd16(r, script));
            } else if (index == 16u) {
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_POSITION_X, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_POSITION_Y, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_POSITION_Z, ae_rd16(r, script));
            } else if (index == 17u) {
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_ROTATION_X, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_ROTATION_Y, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_ROTATION_Z, ae_rd16(r, script));
            } else if (index == 18u) {
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_POSITION_TARGET_X, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_POSITION_TARGET_Y, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_POSITION_TARGET_Z, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_POSITION_REMAINING, ae_rd16(r, script));
            } else if (index == 19u) {
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_ROTATION_TARGET_X, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_ROTATION_TARGET_Y, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_ROTATION_TARGET_Z, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, ui + AE_OVL18B_UI_ROTATION_REMAINING, ae_rd16(r, script));
            }
            break;
        case 0x0080u:
            if (index < 5u) {
                object = script_object(index);
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, object + AE_OVL18B_OBJ_ROTATION_TARGET_X, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, object + AE_OVL18B_OBJ_ROTATION_TARGET_Y, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, object + AE_OVL18B_OBJ_ROTATION_TARGET_Z, ae_rd16(r, script));
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, object + AE_OVL18B_OBJ_ROTATION_REMAINING, ae_rd16(r, script));
            }
            break;
        case 0x00C0u:
        case 0x0100u:
            script += OVL18B_SCRIPT_WORD_BYTES;
            signed_operand = ae_rds16(r, script);
            operand = AE_OVL18B_G_SCANLINE_AUX + (uint32_t)signed_operand * OVL18B_SCRIPT_SCANLINE_STRIDE + OVL18B_SCANLINE_ENABLED;
            ae_wr8(r, operand, operation == 0x00C0u ? 1u : 0u);
            break;
        case 0x0140u:
            if (index < 5u) {
                script += OVL18B_SCRIPT_WORD_BYTES;
                ae_wr16(r, script_object(index) + AE_OVL18B_OBJ_ANIMATION_TARGET,
                        ae_rd16(r, script));
            }
            break;
        case 0x0180u:
            if (index < 5u) {
                object = script_object(index);
                script += OVL18B_SCRIPT_WORD_BYTES;
                signed_operand = ae_rds16(r, script);
                operand = ae_rd32(r, object + AE_OVL18B_OBJ_ANIM_TABLE) +
                          (uint32_t)signed_operand * 4u;
                ae_wr16(r, object + AE_OVL18B_OBJ_ANIM_INDEX,
                        (uint32_t)signed_operand);
                ae_wr16(r, object + AE_OVL18B_OBJ_ANIMATION_TARGET,
                        (uint32_t)signed_operand);
                ae_wr16(r, object + AE_OVL18B_OBJ_ANIMATION_FRAME,
                        ae_rd16(r, operand));
            }
            break;
        case 0x01C0u:
            script += OVL18B_SCRIPT_WORD_BYTES;
            ae_wr32(r, ui + AE_OVL18B_UI_WAIT_TIMER, (uint32_t)ae_rds16(r, script));
            break;
        case 0x0200u:
            operand = ae_rd32(r, ui + AE_OVL18B_UI_WAIT_TIMER);
            if (operand != 0u) {
                ae_wr32(r, ui + AE_OVL18B_UI_WAIT_TIMER, operand - 1u);
                advance_to_next_frame = 1;
            }
            break;
        case 0x0240u:
            ae_wr32(r, ui + AE_OVL18B_UI_SCRIPT_AUX, script + OVL18B_SCRIPT_WORD_BYTES);
            break;
        case 0x0280u:
            script = ae_rd32(r, ui + AE_OVL18B_UI_SCRIPT_AUX);
            advance_to_next_frame = 1;
            break;
        case 0x02C0u: {
            script += OVL18B_SCRIPT_WORD_BYTES;
            signed_operand = ae_rds16(r, script);
            AeOvl18bHooks frame_hooks = nested_hooks(h, OVL18B_SCRIPT_FRAME_BYTES);
            ae_StageClearUi_CopyStrip_18B(r, &frame_hooks, signed_operand);
            break;
        }
        case 0x0300u:
            ae_wr32(r, ui + AE_OVL18B_UI_SCRIPT_FLAGS, 1u);
            break;
        case 0x0340u:
            if ((ae_rd32(r, ui + AE_OVL18B_UI_SCRIPT_FLAGS) & 2u) == 0u) {
                ae_wr32(r, ui + AE_OVL18B_UI_WAIT_TIMER, ae_rd32(r, ui + AE_OVL18B_UI_WAIT_TIMER) - 1u);
                advance_to_next_frame = 1;
            }
            break;
        case 0x03C0u:
            ae_SpecialEnterUi_PlayXa(r, h);
            break;
        default:
            break;
        }
        if (ae_ram_failed(r)) return;
        if (advance_to_next_frame != 0) break;
        script += OVL18B_SCRIPT_WORD_BYTES;
    }
    if (ae_ram_failed(r)) return;
    ae_wr32(r, ui + AE_OVL18B_UI_SCRIPT, script);
    if (ae_rd32(r, ui + AE_OVL18B_UI_OBJECT_COUNT) != 7u) {
        AeOvl18bHooks frame_hooks = nested_hooks(h, OVL18B_SCRIPT_FRAME_BYTES);
        ae_SpecialEnterUi_DrawBlink(r, &frame_hooks);
        if (ae_ram_failed(r)) return;
    }

    {
        int32_t left = ae_rds16(r, ui + AE_OVL18B_UI_ROTATION_REMAINING);
        if (left > 0) {
            for (uint32_t axis = 0u; axis < 3u; ++axis) {
                uint32_t current_field = ui + AE_OVL18B_UI_CAMERA_ROTATION_X + axis * OVL18B_POINT_COMPONENT_STRIDE;
                uint32_t target_field = ui + AE_OVL18B_UI_ROTATION_TARGET_X + axis * OVL18B_POINT_COMPONENT_STRIDE;
                int32_t difference = (int32_t)ae_rds16(r, target_field) -
                                     (int32_t)ae_rds16(r, current_field);
                int32_t step = divide_truncating(r, difference, left);
                ae_wr16(r, current_field, ae_rd16(r, current_field) + (uint32_t)step);
            }
            ae_wr16(r, ui + AE_OVL18B_UI_ROTATION_REMAINING, (uint32_t)(left - 1));
        }
        left = ae_rds16(r, ui + AE_OVL18B_UI_POSITION_REMAINING);
        if (left > 0) {
            for (uint32_t axis = 0u; axis < 3u; ++axis) {
                uint32_t current_field = ui + AE_OVL18B_UI_CAMERA_POSITION_X + axis * OVL18B_POINT_COMPONENT_STRIDE;
                uint32_t target_field = ui + AE_OVL18B_UI_POSITION_TARGET_X + axis * OVL18B_POINT_COMPONENT_STRIDE;
                int32_t difference = (int32_t)ae_rds16(r, target_field) -
                                     (int32_t)ae_rds16(r, current_field);
                int32_t step = divide_truncating(r, difference, left);
                ae_wr16(r, current_field, ae_rd16(r, current_field) + (uint32_t)step);
            }
            ae_wr16(r, ui + AE_OVL18B_UI_POSITION_REMAINING, (uint32_t)(left - 1));
        }
    }

    for (uint32_t record = AE_OVL18B_G_SCANLINE_AUX;
         !ae_ram_failed(r) && ae_rds8(r, record + OVL18B_SCANLINE_ENABLED) >= 0;
         record += OVL18B_SCRIPT_SCANLINE_STRIDE) {
        int32_t enabled = ae_rds8(r, record + OVL18B_SCANLINE_ENABLED);
        if (enabled == 0) continue;
        ae_wr16(r, ui + AE_OVL18B_UI_TEXT_X, ae_rd16(r, record));
        ae_wr16(r, ui + AE_OVL18B_UI_TEXT_Y, ae_rd16(r, record + OVL18B_SCANLINE_Y));
        switch (ae_rds8(r, record + OVL18B_SCANLINE_TYPE)) {
        case 0: {
            uint32_t message = ae_SaveUi_StringTableIndex_18B(
                r, AE_OVL18B_G_TEXT_TABLE, ae_rds16(r, record + OVL18B_SCANLINE_TEXT_INDEX));
            AeOvl18bHooks frame_hooks = nested_hooks(h, OVL18B_SCRIPT_FRAME_BYTES);
            ae_SpecialEnterUi_DrawText(r, &frame_hooks, message);
            break;
        }
        case 1: {
            uint32_t message = ae_SaveUi_StringTableIndex_18B(
                r, AE_OVL18B_G_TEXT_TABLE, ae_rds16(r, record + OVL18B_SCANLINE_TEXT_INDEX));
            AeOvl18bHooks frame_hooks = nested_hooks(h, OVL18B_SCRIPT_FRAME_BYTES);
            ae_SpecialEnterUi_DrawMessageShadow(r, &frame_hooks, message);
            break;
        }
        case 2: {
            AeOvl18bHooks frame_hooks = nested_hooks(h, OVL18B_SCRIPT_FRAME_BYTES);
            ae_SpecialEnterUi_DrawScene(r, &frame_hooks);
            break;
        }
        default:
            break;
        }
    }

}

void ae_SpecialEnterUi_UpdateObjects(AeRam *r, const AeOvl18bHooks *h,
                                     uint32_t objects)
{
    uint32_t frame = stack_local(h, OVL18B_OBJECT_FRAME_BYTES, 0u);
    uint32_t normal = frame + OVL18B_UPDATE_FRAME_NORMAL_VECTOR;

    for (uint32_t index = 0u; index < 5u; ++index) {
        uint32_t object = objects + index * AE_OVL18B_OBJ_SIZE;
        if ((ae_rd16(r, object + OVL18B_OBJ_ACTIVE_FLAGS) & 1u) == 0u) {
            continue;
        }

        int32_t left = ae_rds16(r, object + AE_OVL18B_OBJ_ROTATION_REMAINING);
        if (left > 0) {
            for (uint32_t axis = 0u; axis < 3u; ++axis) {
                uint32_t current = object + AE_OVL18B_OBJ_POSITION_CURRENT_X + axis * OVL18B_POINT_COMPONENT_STRIDE;
                uint32_t target = object + AE_OVL18B_OBJ_ROTATION_TARGET_X + axis * OVL18B_POINT_COMPONENT_STRIDE;
                int32_t step = divide_truncating(r,
                    (int32_t)ae_rds16(r, target) - (int32_t)ae_rds16(r, current), left);
                ae_wr16(r, current, ae_rd16(r, current) + (uint32_t)step);
            }
            ae_wr16(r, object + AE_OVL18B_OBJ_ROTATION_REMAINING, (uint32_t)(left - 1));
        }

        left = ae_rds16(r, object + AE_OVL18B_OBJ_POSITION_REMAINING);
        if (left > 0) {
            for (uint32_t axis = 0u; axis < 3u; ++axis) {
                uint32_t current = object + AE_OVL18B_OBJ_POSITION_SOURCE_X + axis * OVL18B_VECTOR_COMPONENT_STRIDE;
                uint32_t target = object + AE_OVL18B_OBJ_POSITION_TARGET_X + axis * OVL18B_POINT_COMPONENT_STRIDE;
                int32_t step = divide_truncating(r,
                    (int32_t)ae_rds16(r, target) - ae_rds32(r, current), left);
                ae_wr32(r, current, (uint32_t)ae_rds32(r, current) + (uint32_t)step);
            }
            ae_wr16(r, object + AE_OVL18B_OBJ_POSITION_REMAINING, (uint32_t)(left - 1));
        }
        if (ae_ram_failed(r)) return;

        (void)ae_sdk_VectorNormalSS(r,
            object + AE_OVL18B_OBJ_POSITION_CURRENT_X, normal);
        (void)ae_sdk_MatrixFromDirXZ(r, normal, ae_rd32(r, object + OVL18B_OBJ_DIRECTION_MATRIX_PTR));
        if (ae_ram_failed(r)) return;

        ae_wr16(r, object + AE_OVL18B_OBJ_ANIMATION_FRAME,
                ae_rd16(r, object + AE_OVL18B_OBJ_ANIMATION_FRAME) + 1u);
        {
            uint32_t animation_table = ae_rd32(r, object + AE_OVL18B_OBJ_ANIM_TABLE);
            uint32_t current_animation = ae_rd16(r, object + AE_OVL18B_OBJ_ANIM_INDEX);
            int32_t frame_limit = ae_rds16(r, animation_table + current_animation * AE_OVL18B_ANIMATION_ENTRY_STRIDE + AE_OVL18B_ANIMATION_ENTRY_FRAME_LIMIT);
            int32_t animation_frame =
                (int32_t)ae_rd16(r, object + AE_OVL18B_OBJ_ANIMATION_FRAME);
            if (frame_limit < animation_frame) {
                uint32_t next_animation =
                    (uint32_t)ae_rds16(r, object + AE_OVL18B_OBJ_ANIMATION_TARGET);
                ae_wr16(r, object + AE_OVL18B_OBJ_ANIM_INDEX, next_animation);
                ae_wr16(r, object + AE_OVL18B_OBJ_ANIMATION_FRAME,
                        ae_rd16(r, animation_table + next_animation * AE_OVL18B_ANIMATION_ENTRY_STRIDE));
            }
        }

        ae_ModelPoseBones(r, object, 1u);
        if (ae_ram_failed(r)) return;
        (void)RENDER1(r, h, "ModelTransformSkinned",
                      OVL18B_API_MODEL_TRANSFORM_SKINNED, object);
        if (ae_ram_failed(r)) return;
        {
            uint32_t model_instance = AE_OVL18B_G_FRAME_MODEL_INSTANCES +
                                      ae_rd32(r, AE_OVL18B_G_FRAME_OT_INDEX) * AE_OVL18B_OT_PACKET_STRIDE;
            (void)RENDER3(r, h, "ModelDraw", OVL18B_API_MODEL_DRAW,
                          model_instance, AE_OVL18B_G_MODEL_DRAW_BASE, object);
        }
        if (ae_ram_failed(r)) return;
    }
}






#define OVL18B_G_WIPE_STATE 0x801389B0u
#define OVL18B_G_WIPE_COUNT 0x80138B8Eu
#define OVL18B_G_MODE_STAGE_ENTRY 0x801389B4u
#define OVL18B_G_FRAME_ENV_BASE AE_OVL18B_G_FRAME_COUNTER
#define OVL18B_G_DRAW_PACKET_CONTEXT_BASE (AE_OVL18B_G_FRAME_COUNTER + 0x4080u)

static void render_wipe_phase(AeRam *r, const AeOvl18bHooks *h,
                              uint32_t state, uint32_t cursor,
                              uint32_t frame_index)
{
    uint32_t arguments[6] = {
        state, cursor, frame_index,
        ae_rd16(r, AE_OVL18B_G_UI_STATE + AE_OVL18B_UI_STATE),
        ae_rd16(r, AE_OVL18B_G_UI_STATE + AE_OVL18B_UI_STATE_TIMER),
        AE_OVL18B_G_UI_OBJECTS
    };
    (void)render_call(r, h, "SpecialEnterUi_DrawWipePackets", 0x80136954u,
                      6u, arguments);
}

static uint32_t frame_context(uint32_t index)
{
    return OVL18B_G_FRAME_ENV_BASE + index * AE_OVL18B_OT_PACKET_STRIDE;
}

static uint32_t draw_packet_context_for_frame(AeRam *r)
{
    uint32_t index = ae_rd32(r, AE_OVL18B_G_FRAME_OT_INDEX);
    return OVL18B_G_DRAW_PACKET_CONTEXT_BASE + index * AE_OVL18B_OT_PACKET_STRIDE;
}

static uint32_t otag_for_frame(AeRam *r)
{
    uint32_t index = ae_rd32(r, AE_OVL18B_G_FRAME_OT_INDEX);
    return AE_OVL18B_G_OT_PACKET_BASE + index * AE_OVL18B_OT_PACKET_STRIDE;
}

/* Area-resource and animation state remain in RAM; only wipe packet emission is native. */
int32_t ae_SpecialEnterUi_DrawWipe(AeRam *r, const AeOvl18bHooks *h,
                                  uint32_t source)
{
    uint32_t state = ae_rd16(r, OVL18B_G_WIPE_STATE);
    uint32_t ui = AE_OVL18B_G_UI_STATE;
    (void)source;

    if (state == 0u) {
        uint32_t xa_active = ae_rd16(r, AE_OVL18B_G_WIPE_FRAME);
        if (xa_active != 0u) {
            int32_t load_state = api_call(r, h, "LoadListStep",
                                          OVL18B_API_LOAD_LIST_STEP, 0u, NULL);
            if (load_state < 0) {
                (void)API0(r, h, "SoundStopAndRestoreVolume",
                           OVL18B_API_SOUND_STOP_RESTORE_VOLUME);
                ae_wr16(r, AE_OVL18B_G_WIPE_FRAME, 0u);
            }
        }

        uint32_t count = (ae_rd16(r, OVL18B_G_WIPE_COUNT) + 2u) & 0xFFFFu;
        ae_wr16(r, OVL18B_G_WIPE_COUNT, count);
        if (sign16(count) < 121 || ae_rds16(r, AE_OVL18B_G_WIPE_FRAME) != 0) return 0;

        if (h == NULL || h->area == NULL) {
            ae_ram_fail(r, "AeOvl18b area hooks not provided");
            return 0;
        }
        ae_AreaSetZoneBoxes(r, h->area, 0u);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, OVL18B_G_WIPE_COUNT, 0u);

        {
            uint32_t area = AE_AREA_BASE;
            uint32_t resource_table = area +
                ae_rd16(r, area + AE_OVL18B_AREA_RESOURCE_TABLE_INDEX) * 4u;
            uint32_t model_table = area +
                ae_rd16(r, area + AE_OVL18B_AREA_MODEL_TABLE_INDEX) * 4u;
            uint32_t model_records = model_table + AE_OVL18B_AREA_MODEL_TABLE_RECORDS;
            uint32_t model_count = ae_rd32(r, model_table);
            uint32_t props = ae_HeapAlloc(r, h->area,
                                         model_count * AE_OVL18B_AREA_MODEL_PROP_STRIDE);
            uint32_t render_context = AE_OVL18B_G_SCREEN_AREA_STATE;
            ae_wr32(r, render_context + AE_OVL18B_OT_CONTEXT_RESOURCE_TABLE,
                    resource_table);
            ae_wr32(r, render_context + AE_OVL18B_OT_CONTEXT_MODEL_TABLE,
                    model_records);
            ae_wr32(r, render_context + AE_OVL18B_OT_CONTEXT_MODEL_COUNT,
                    model_count);
            ae_wr32(r, AE_OVL18B_G_RENDER_PROP_TABLE, props);
            if (ae_ram_failed(r)) return 0;

            for (uint32_t index = 0u; index < model_count; ++index) {
                uint32_t source_record = model_records +
                    index * AE_OVL18B_AREA_MODEL_RECORD_STRIDE;
                uint32_t prop = props + index * AE_OVL18B_AREA_MODEL_PROP_STRIDE;
                uint32_t mesh_record = resource_table +
                    ae_rd16(r, source_record + AE_OVL18B_AREA_MODEL_RECORD_MESH_INDEX) * 4u;
                uint32_t mesh_header = resource_table +
                    ae_rd16(r, mesh_record + AE_OVL18B_RESOURCE_MESH_HEADER_INDEX) * 4u;

                ae_wr16(r, prop + AE_OVL18B_MODEL_PROP_POSITION_X, ae_rd16(r, source_record + AE_OVL18B_AREA_MODEL_RECORD_POSITION_X));
                ae_wr16(r, prop + AE_OVL18B_MODEL_PROP_POSITION_Y, ae_rd16(r, source_record + AE_OVL18B_AREA_MODEL_RECORD_POSITION_Y));
                ae_wr16(r, prop + AE_OVL18B_MODEL_PROP_POSITION_Z, ae_rd16(r, source_record + AE_OVL18B_AREA_MODEL_RECORD_POSITION_Z));
                ae_wr16(r, prop + AE_OVL18B_MODEL_PROP_STATE, 0u);
                (void)RENDER2(r, h, "MeshInit", OVL18B_API_MESH_INIT,
                              prop, mesh_header);
                {
                    uint32_t mesh_instance = ae_rd32(
                        r, prop + AE_OVL18B_MODEL_PROP_MESH_INSTANCE);
                    uint32_t mesh_flags = ae_rd32(r, mesh_instance);
                    ae_wr32(r, mesh_instance,
                            mesh_flags & AE_OVL18B_MODEL_PROP_FLAGS_MASK);
                    ae_wr32(r, prop + AE_OVL18B_MODEL_PROP_SOURCE_FIELD_2,
                            ae_rd16(r, source_record + AE_OVL18B_AREA_MODEL_RECORD_SOURCE_FIELD_2));
                    ae_wr16(r, prop + AE_OVL18B_MODEL_PROP_STATE, 0xFFFFu);
                }
                if (ae_ram_failed(r)) return 0;
            }

            for (uint32_t index = 0u; index < 2u; ++index) {
                uint32_t entry = area + AE_OVL18B_AREA_OBJECT_MODEL_IDS +
                                 index * AE_OVL18B_AREA_OBJECT_ID_STRIDE;
                uint32_t object = AE_OVL18B_G_UI_OBJECTS +
                                  index * AE_OVL18B_OBJ_SIZE;
                uint32_t model_header = area + ae_rd16(r, entry) * 4u;
                uint32_t animation_header = area +
                    ae_rd16(r, area + AE_OVL18B_AREA_OBJECT_ANIM_IDS +
                               index * AE_OVL18B_AREA_OBJECT_ID_STRIDE) * 4u;
                uint32_t animation_count = ae_rd32(r, animation_header);
                uint32_t animation = animation_header + AE_OVL18B_ANIMATION_HEADER_ENTRIES + animation_count * AE_OVL18B_ANIMATION_ENTRY_STRIDE;

                ae_ModelInit(r, h->area, object, model_header);
                ae_wr32(r, object + AE_OVL18B_INSTANCE_ANIMATION, animation);
                ae_wr16(r, object + AE_OVL18B_INSTANCE_ANIMATION_FRAME, 0u);
                ae_wr16(r, object + AE_OVL18B_INSTANCE_ANIMATION_INDEX, 0u);
                ae_wr16(r, object + AE_OVL18B_INSTANCE_ANIMATION_TARGET, 0u);
                ae_wr16(r, object + AE_OVL18B_INSTANCE_FLAGS, 0u);
                ae_wr32(r, object + AE_OVL18B_INSTANCE_ANIMATION_TABLE,
                        animation_header + AE_OVL18B_ANIMATION_HEADER_ENTRIES);
                if (ae_ram_failed(r)) return 0;
            }
        }
        ae_SpecialEnterUi_SetAreaTablePointer(r);
        if (ae_ram_failed(r)) return 0;

        if (ae_rd32(r, ui + AE_OVL18B_UI_OBJECT_STATE) == 7u) {
            ae_wr32(r, ui + AE_OVL18B_UI_SCRIPT, AE_OVL18B_G_SCRIPT_AREA_ENTRY);
        } else {
            ae_wr32(r, ui + AE_OVL18B_UI_SCRIPT, AE_OVL18B_G_SCRIPT_DEFAULT_ENTRY);
        }
        {
            AeOvl18bHooks script_hooks = nested_hooks(h, OVL18B_WIPE_FRAME_BYTES);
            ae_SpecialEnterUi_RunScript(r, &script_hooks);
        }
        if (ae_ram_failed(r)) return 0;

        {
            AeOvl18bHooks frame_hooks = nested_hooks(h, OVL18B_WIPE_FRAME_BYTES);
            ae_StageClearUi_LookAt_18B(r, &frame_hooks, AE_OVL18B_G_UI_CAMERA_ROTATION,
                                       AE_OVL18B_G_UI_CAMERA_POSITION);
            if (ae_ram_failed(r)) return 0;
            (void)render_call(r, h, "DrawAreaProps", OVL18B_API_RENDER_AREA_PROPS,
                              1u, (uint32_t[]){stack_local(&frame_hooks,
                                  OVL18B_LOOKAT_FRAME_BYTES,
                                  OVL18B_LOOKAT_SCALE_MATRIX)});
            if (ae_ram_failed(r)) return 0;
            ae_SpecialEnterUi_UpdateObjects(r, &frame_hooks,
                                            AE_OVL18B_G_UI_OBJECTS);
        }
        if (ae_ram_failed(r)) return 0;

        ae_wr16(r, OVL18B_G_WIPE_STATE, 1u);
        ae_wr16(r, ui + AE_OVL18B_UI_STATE, 0u);
        ae_wr16(r, ui + AE_OVL18B_UI_STATE_TIMER, 0u);
        if (h != NULL && h->stage_fade != NULL &&
            h->stage_fade->sound_hw != NULL) {
            (void)ae_PlaySfx(r, h->stage_fade->sound_hw, 172);
        } else {
            ae_ram_fail(r, "AeOvl18b sound hooks not provided");
        }
        return 0;
    }

    if (state == 1u) {
        uint32_t cursor = ae_rd32(r, AE_OVL18B_G_PRIMITIVE_CURSOR);
        render_wipe_phase(r, h, state, cursor, ae_rd32(r, AE_OVL18B_G_FRAME_OT_INDEX));
        ae_wr32(r, AE_OVL18B_G_PRIMITIVE_CURSOR, cursor + OVL18B_WIPE_IN_PACKET_BYTES);
        uint32_t timer = (ae_rd16(r, ui + AE_OVL18B_UI_STATE_TIMER) + 1u) & 0xFFFFu;
        ae_wr16(r, ui + AE_OVL18B_UI_STATE_TIMER, timer);
        if (timer >= 11u) {
            ae_wr16(r, ui + AE_OVL18B_UI_STATE, 2u);
            return 0;
        }
        return 0;
    }

    if (state == 2u) {
        AeOvl18bHooks frame_hooks = nested_hooks(h, OVL18B_WIPE_FRAME_BYTES);
        ae_SpecialEnterUi_RunScript(r, &frame_hooks);
        if (ae_ram_failed(r)) return 0;
        ae_StageClearUi_LookAt_18B(r, &frame_hooks, AE_OVL18B_G_UI_CAMERA_ROTATION, AE_OVL18B_G_UI_CAMERA_POSITION);
        if (ae_ram_failed(r)) return 0;
        (void)render_call(r, h, "DrawAreaProps", OVL18B_API_RENDER_AREA_PROPS,
                          1u, (uint32_t[]){stack_local(&frame_hooks,
                              OVL18B_LOOKAT_FRAME_BYTES,
                              OVL18B_LOOKAT_SCALE_MATRIX)});
        ae_SpecialEnterUi_UpdateObjects(r, &frame_hooks,
                                        AE_OVL18B_G_UI_OBJECTS);
        ae_StageClearUi_DrawScanlines_18B(r, &frame_hooks);
        if (ae_ram_failed(r)) return 0;

        {
            uint32_t pad = AE_OVL18B_G_PAD_STATE;
            uint32_t pressed = ae_rd32(r, pad + AE_OVL18B_PAD_BUTTONS_PRESSED) &
                               ae_rd32(r, pad + AE_OVL18B_PAD_BUTTONS_CURRENT) & 0x860u;
            uint32_t script = ae_rd32(r, ui + AE_OVL18B_UI_SCRIPT);
            if (pressed == 0u && ae_rds16(r, script) != (int32_t)OVL18B_SCRIPT_END) {
                return 0;
            }
        }
        if (h != NULL && h->stage_fade != NULL &&
            h->stage_fade->sound_hw != NULL) {
            (void)ae_PlaySfx(r, h->stage_fade->sound_hw, 173);
        } else {
            ae_ram_fail(r, "AeOvl18b sound hooks not provided");
            return 0;
        }
        ae_wr16(r, OVL18B_G_WIPE_STATE, 3u);
        ae_wr16(r, ui + AE_OVL18B_UI_STATE, 0u);
        ae_wr16(r, ui + AE_OVL18B_UI_STATE_TIMER, 10u);
        return 0;
    }

    if (state == 3u) {
        uint32_t cursor = ae_rd32(r, AE_OVL18B_G_PRIMITIVE_CURSOR);
        render_wipe_phase(r, h, state, cursor, ae_rd32(r, AE_OVL18B_G_FRAME_OT_INDEX));
        ae_wr32(r, AE_OVL18B_G_PRIMITIVE_CURSOR, cursor + OVL18B_WIPE_OUT_PACKET_BYTES);
        {
            int32_t timer = (int32_t)ae_rds16(r, ui + AE_OVL18B_UI_STATE_TIMER) - 1;
            ae_wr16(r, ui + AE_OVL18B_UI_STATE_TIMER, (uint32_t)timer);
            if (timer < 0) ae_wr16(r, ui + AE_OVL18B_UI_STATE, 2u);
        }
        return 0;
    }
    return 0;
}

/* Mode startup and its frame loop keep area selection, input, and CD state in RAM. */
int32_t ae_Mode_SpecialAreaEnter(AeRam *r, const AeOvl18bHooks *h)
{
    uint32_t mode_frame = stack_local(h, OVL18B_MODE_FRAME_BYTES, 0u);
    uint32_t env = mode_frame + OVL18B_MODE_FRAME_CLEAR_RECTANGLE;
    uint32_t area_code = ae_rd16(r, AE_OVL18B_G_CURRENT_AREA);
    uint32_t ui = AE_OVL18B_G_UI_STATE;

    for (uint32_t word = 0u; word < 2u; ++word) {
        ae_wr32(r, env + word * 4u,
                ae_rd32(r, OVL18B_G_MODE_STAGE_TAG + word * 4u));
    }
    (void)API0(r, h, "InitDisplay", OVL18B_API_INIT_DISPLAY);
    (void)API1(r, h, "ResetGraph", OVL18B_API_RESET_GRAPH, 1u);
    (void)API4(r, h, "ClearImage", OVL18B_API_CLEAR_IMAGE, env, 0u, 0u, 0u);
    (void)API1(r, h, "SetGeomScreen", OVL18B_API_SET_GEOM_SCREEN, 328u);
    if (h == NULL || h->items == NULL) {
        ae_ram_fail(r, "AeOvl18b item hooks not provided");
        return 0;
    }
    ae_SetColorMatrixMode(r, h->items, 0u, 0u, 0u, 0u);
    ae_wr32(r, OVL18B_G_CLEAR_OT + OVL18B_CLEAR_OT_FIELD_24, 0u);
    ae_wr32(r, OVL18B_G_CLEAR_OT + OVL18B_CLEAR_OT_FIELD_28, 0x1000u);
    ae_wr32(r, OVL18B_G_CLEAR_OT + OVL18B_CLEAR_OT_FIELD_2C, 0x1000u);
    (void)API1(r, h, "DrawSync", OVL18B_API_DRAW_SYNC, 0u);

    ae_wr32(r, AE_OVL18B_G_DISPLAY_MODE, 0u);
    switch (area_code) {
    case 2u:
        ae_wr16(r, AE_OVL18B_G_CURRENT_AREA, 0x62u);
        ae_wr32(r, ui + AE_OVL18B_UI_OBJECT_STATE, 8u);
        ae_wr32(r, ui + AE_OVL18B_UI_AREA_ENTRY, 24u);
        break;
    case 6u:
        ae_wr16(r, AE_OVL18B_G_CURRENT_AREA, 0x5Cu);
        ae_wr32(r, ui + AE_OVL18B_UI_OBJECT_STATE, 2u);
        ae_wr16(r, AE_OVL18B_G_MODE_AREA_TAG, 48u);
        ae_wr32(r, ui + AE_OVL18B_UI_AREA_ENTRY, 25u);
        break;
    case 15u:
        ae_wr16(r, AE_OVL18B_G_CURRENT_AREA, 0x5Du);
        ae_wr32(r, ui + AE_OVL18B_UI_OBJECT_STATE, 3u);
        ae_wr8(r, AE_OVL18B_G_MODE_SFX_AREA_FLAGS, 7u);
        ae_wr8(r, AE_OVL18B_G_MODE_SFX_AREA_FLAGS_1, 3u);
        ae_wr16(r, AE_OVL18B_G_MODE_AREA_TAG, 49u);
        ae_wr32(r, ui + AE_OVL18B_UI_AREA_ENTRY, 26u);
        break;
    case 20u:
        ae_wr16(r, AE_OVL18B_G_CURRENT_AREA, 0x5Eu);
        ae_wr32(r, ui + AE_OVL18B_UI_OBJECT_STATE, 4u);
        ae_wr16(r, AE_OVL18B_G_MODE_AREA_TAG, 50u);
        ae_wr32(r, ui + AE_OVL18B_UI_AREA_ENTRY, 27u);
        break;
    case 30u:
        ae_wr16(r, AE_OVL18B_G_CURRENT_AREA, 0x60u);
        ae_wr32(r, ui + AE_OVL18B_UI_OBJECT_STATE, 6u);
        ae_wr16(r, AE_OVL18B_G_MODE_AREA_TAG, 51u);
        ae_wr32(r, ui + AE_OVL18B_UI_AREA_ENTRY, 28u);
        break;
    case 53u:
        ae_wr16(r, AE_OVL18B_G_CURRENT_AREA, 0x61u);
        ae_wr32(r, ui + AE_OVL18B_UI_OBJECT_STATE, 7u);
        ae_wr16(r, OVL18B_G_MODE_STAGE_ENTRY + OVL18B_MODE_STAGE_ENTRY_SETUP_FIELD, 36u);
        ae_wr16(r, AE_OVL18B_G_MODE_AREA_TAG, 52u);
        ae_wr32(r, ui + AE_OVL18B_UI_AREA_ENTRY, 29u);
        break;
    default:
        ae_wr16(r, AE_OVL18B_G_CURRENT_AREA, 0x5Fu);
        ae_wr32(r, ui + AE_OVL18B_UI_OBJECT_STATE, 5u);
        ae_wr16(r, AE_OVL18B_G_MODE_AREA_TAG, 53u);
        ae_wr32(r, ui + AE_OVL18B_UI_AREA_ENTRY, 30u);
        break;
    }

    (void)API1(r, h, "ArcLoadEntry", OVL18B_API_ARC_LOAD_ENTRY,
               OVL18B_G_MODE_STAGE_ENTRY);
    ae_wr32(r, AE_OVL18B_G_FRAME_OT_INDEX, 0u);
    ae_wr16(r, ui + AE_OVL18B_UI_TEXTURE_PAGE, 0x3FDFu);
    ae_wr16(r, ui + AE_OVL18B_UI_FONT_ROW, 38u);
    ae_wr32(r, ui + AE_OVL18B_UI_TEXT_SPACING, 32u);
    ae_wr16(r, ui + AE_OVL18B_UI_TEXT_COLOR, 0xF000u);
    ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_ROTATION_X, 0u);
    ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_ROTATION_Y, 0u);
    ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_POSITION_X, 0u);
    ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_POSITION_Y, 0u);
    ae_wr16(r, ui + AE_OVL18B_UI_CAMERA_POSITION_Z, 0u);
    ae_wr8(r, AE_OVL18B_G_FRAME_PACKET_FLAGS, 1u);
    ae_wr8(r, AE_OVL18B_G_FRAME_PACKET_FLAGS_1, 0u);
    ae_wr8(r, AE_OVL18B_G_FRAME_PACKET_FLAGS_2, 0u);
    ae_wr8(r, AE_OVL18B_G_FRAME_PACKET_FLAGS_3, 8u);
    ae_wr8(r, AE_OVL18B_G_FRAME_DISPLAY_FLAGS, 1u);
    ae_wr8(r, AE_OVL18B_G_FRAME_DISPLAY_FLAGS_1, 0u);
    ae_wr8(r, AE_OVL18B_G_FRAME_DISPLAY_FLAGS_2, 0u);
    ae_wr8(r, AE_OVL18B_G_FRAME_DISPLAY_FLAGS_3, 8u);
    ae_wr16(r, AE_OVL18B_G_SCREEN_MODE, 17u);
    ae_wr16(r, OVL18B_G_WIPE_STATE, 0u);

    if (h == NULL || h->pad == NULL) {
        ae_ram_fail(r, "AeOvl18b pad hooks not provided");
        return 0;
    }
    for (;;) {
        AeOvl18bHooks frame_hooks = nested_hooks(h, OVL18B_MODE_FRAME_BYTES);
        if (h == NULL || h->pad == NULL) {
            ae_ram_fail(r, "AeOvl18b pad hooks not provided");
            return 0;
        }
        ae_pad_PadUpdate(r, h->pad, 0u);
        if (ae_ram_failed(r)) return 0;
        uint32_t frame_counter = ae_rd32(r, AE_OVL18B_G_FRAME_COUNTER) + 1u;
        (void)API0(r, h, "BeginFrame", OVL18B_API_BEGIN_FRAME);
        ae_wr32(r, AE_OVL18B_G_FRAME_COUNTER, frame_counter);
        uint32_t source = draw_packet_context_for_frame(r);
        ae_wr32(r, ui + AE_OVL18B_UI_RENDER_RESULT, source);
        int32_t wipe_result = ae_SpecialEnterUi_DrawWipe(r, &frame_hooks, source);
        if (ae_ram_failed(r)) return 0;
        if (wipe_result < 0) break;

        (void)API1(r, h, "DrawSync", OVL18B_API_DRAW_SYNC, 0u);
        (void)API1(r, h, "VSync", OVL18B_API_VSYNC, 2u);
        if (ae_ram_failed(r)) return 0;

        {
            uint32_t wipe_state = ae_rd16(r, OVL18B_G_WIPE_STATE);
            uint32_t ui_state = ae_rd16(r, ui + AE_OVL18B_UI_STATE);
            uint32_t frame_index = ae_rd16(r, AE_OVL18B_G_FRAME_OT_INDEX);
            uint32_t display_index = ae_rd16(r, ui + AE_OVL18B_UI_DISPLAY_INDEX);
            uint32_t display_context;
            uint32_t draw_context;

            if (wipe_state == 1u && ui_state == 0u) {
                AeOvl18bHooks wipe_hooks = nested_hooks(h, OVL18B_MODE_FRAME_BYTES);
                ae_StageClearUi_SetDrawStp_18B(r, &wipe_hooks);
                display_context = frame_context(frame_index);
                (void)API1(r, h, "PutDispEnv", OVL18B_API_PUT_DISPLAY_ENV,
                           display_context + AE_OVL18B_OT_DISPLAY_ENV_OFFSET);
                draw_context = frame_context(frame_index);
                (void)API1(r, h, "PutDrawEnv", OVL18B_API_PUT_DRAW_ENV,
                           draw_context + AE_OVL18B_OT_DRAW_ENV_OFFSET);
                ae_wr8(r, AE_OVL18B_G_FRAME_PACKET_FLAGS, 0u);
                ae_wr8(r, AE_OVL18B_G_FRAME_DISPLAY_FLAGS, 0u);
                ae_wr16(r, ui + AE_OVL18B_UI_STATE, 1u);
                ae_wr16(r, ui + AE_OVL18B_UI_DISPLAY_INDEX, frame_index);
            } else if (wipe_state == 1u &&
                       (ui_state == 1u || ui_state == 2u)) {
                if (ui_state == 1u) {
                    ae_wr8(r, AE_OVL18B_G_FRAME_PACKET_FLAGS, 0u);
                    ae_wr8(r, AE_OVL18B_G_FRAME_DISPLAY_FLAGS, 0u);
                }
                display_context = frame_context(display_index);
                (void)API1(r, h, "PutDispEnv", OVL18B_API_PUT_DISPLAY_ENV,
                           display_context + AE_OVL18B_OT_DISPLAY_ENV_OFFSET);
                display_index ^= 1u;
                draw_context = frame_context(display_index);
                (void)API1(r, h, "PutDrawEnv", OVL18B_API_PUT_DRAW_ENV,
                           draw_context + AE_OVL18B_OT_DRAW_ENV_OFFSET);
                if (ui_state == 2u) {
                    ae_wr8(r, AE_OVL18B_G_FRAME_PACKET_FLAGS, 1u);
                    ae_wr8(r, AE_OVL18B_G_FRAME_DISPLAY_FLAGS, 1u);
                    ae_wr16(r, OVL18B_G_WIPE_STATE, 2u);
                }
            } else if (wipe_state == 2u) {
                display_context = frame_context(frame_index);
                (void)API1(r, h, "PutDispEnv", OVL18B_API_PUT_DISPLAY_ENV,
                           display_context + AE_OVL18B_OT_DISPLAY_ENV_OFFSET);
                draw_context = frame_context(frame_index);
                (void)API1(r, h, "PutDrawEnv", OVL18B_API_PUT_DRAW_ENV,
                           draw_context + AE_OVL18B_OT_DRAW_ENV_OFFSET);
            } else if (wipe_state == 3u && ui_state == 0u) {
                display_context = frame_context(frame_index);
                (void)API1(r, h, "PutDispEnv", OVL18B_API_PUT_DISPLAY_ENV,
                           display_context + AE_OVL18B_OT_DISPLAY_ENV_OFFSET);
                (void)API1(r, h, "PutDrawEnv", OVL18B_API_PUT_DRAW_ENV,
                           display_context + AE_OVL18B_OT_DRAW_ENV_OFFSET);
                ae_wr8(r, AE_OVL18B_G_FRAME_PACKET_FLAGS, 0u);
                ae_wr8(r, AE_OVL18B_G_FRAME_DISPLAY_FLAGS, 0u);
                ae_wr16(r, ui + AE_OVL18B_UI_STATE, 1u);
                ae_wr16(r, ui + AE_OVL18B_UI_DISPLAY_INDEX, frame_index);
            } else if (wipe_state == 3u &&
                       (ui_state == 1u || ui_state == 2u)) {
                display_context = frame_context(display_index);
                (void)API1(r, h, "PutDispEnv", OVL18B_API_PUT_DISPLAY_ENV,
                           display_context + AE_OVL18B_OT_DISPLAY_ENV_OFFSET);
                display_index ^= 1u;
                draw_context = frame_context(display_index);
                (void)API1(r, h, "PutDrawEnv", OVL18B_API_PUT_DRAW_ENV,
                           draw_context + AE_OVL18B_OT_DRAW_ENV_OFFSET);
                if (ui_state == 2u) ae_wr16(r, OVL18B_G_WIPE_STATE, 4u);
            }
        }

        {
            uint32_t otag = otag_for_frame(r);
            (void)API1(r, h, "DrawOTag", OVL18B_API_DRAW_OTAG, otag);
            (void)API0(r, h, "CdMgrUpdate", OVL18B_API_CD_MANAGER_UPDATE);
        }
        if (ae_ram_failed(r)) return 0;
        if (ae_rd16(r, OVL18B_G_WIPE_STATE) == 4u) break;

    }

    (void)API1(r, h, "DrawSync", OVL18B_API_DRAW_SYNC, 0u);
    (void)API1(r, h, "VSync", OVL18B_API_VSYNC, 0u);
    (void)API0(r, h, "CdMgrStopStream", OVL18B_API_CD_MANAGER_STOP_STREAM);
    if (h != NULL && h->stage_fade != NULL) {
        (void)ae_SoundStop(r, h->stage_fade, 1);
    } else {
        ae_ram_fail(r, "AeOvl18b stage-fade hooks not provided");
    }
    (void)API2(r, h, "LoadListStart", OVL18B_API_LOAD_LIST_START,
               1u, ae_rd16(r, AE_OVL18B_G_CURRENT_AREA));
    while (!ae_ram_failed(r)) {
        int32_t load = API0(r, h, "LoadListStep", OVL18B_API_LOAD_LIST_STEP);
        if (load != 0) break;
        (void)API0(r, h, "CdMgrUpdate", OVL18B_API_CD_MANAGER_UPDATE);
    }
    return 0;
}



