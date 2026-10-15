#include "ae_ovl18a.h"

#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../core/ae_pose.h"
#include "../player/ae_animdraw.h"
#include "../player/ae_player2.h"
#include "../math/native_sdk_apply.h"
#include "../player/ae_player_spawn.h"
#include "../sound/ae_bgm.h"
#include "../world/ae_misc_batch.h"
#include "../world/ae_progress.h"

#include <limits.h>
#include <string.h>

#define OVL18A_API_INIT_DISPLAY 0x8005CFECu
#define OVL18A_API_RESET_GRAPH 0x80028350u
#define OVL18A_API_CLEAR_IMAGE 0x80028850u
#define OVL18A_API_DRAW_SYNC 0x800286CCu
#define OVL18A_API_BEGIN_FRAME 0x8005DF48u
#define OVL18A_API_VSYNC 0x8002E520u
#define OVL18A_API_PUT_DISPLAY_ENV 0x80028EFCu
#define OVL18A_API_PUT_DRAW_ENV 0x80028D30u
#define OVL18A_API_DRAW_OTAG 0x80028CC0u
#define OVL18A_API_SET_GEOM_SCREEN 0x8002FAF0u
#define OVL18A_API_ARC_LOAD_ENTRY 0x80011194u
#define OVL18A_API_LOAD_LIST_STEP 0x800116C0u
#define OVL18A_API_CD_UPDATE 0x800125B8u
#define OVL18A_API_CD_STOP_STREAM 0x8001256Cu
#define OVL18A_API_SOUND_STOP_RESTORE 0x80086170u
#define OVL18A_API_MESH_INIT 0x80015C34u
#define OVL18A_API_SCALE_MATRIX_L 0x800305B0u
#define OVL18A_API_MODEL_TRANSFORM_SKINNED 0x80016A60u
#define OVL18A_API_MODEL_DRAW 0x80017CD4u
#define OVL18A_API_MODEL_TRANSFORM 0x800160ECu
#define OVL18A_API_MODEL_DRAW_BACKGROUND 0x80017EFCu

#define OVL18A_ENTRY_FRAME_BYTES 48u
#define OVL18A_LOOKAT_FRAME_BYTES 120u
#define OVL18A_SPIN_FRAME_BYTES 104u
#define OVL18A_ADVANCE_FRAME_BYTES 40u
#define OVL18A_PLAY_XA_FRAME_BYTES 24u
#define OVL18A_XA_PLAY_FRAME_BYTES 64u
#define OVL18A_LOOKAT_VECTOR_OFFSET 16u
#define OVL18A_LOOKAT_NORMAL_OFFSET 32u
#define OVL18A_LOOKAT_SCALE_MATRIX_OFFSET 48u
#define OVL18A_LOOKAT_SCALE_VECTOR_OFFSET 80u
#define OVL18A_SPIN_LOCAL_MATRIX_OFFSET 16u
#define OVL18A_SPIN_ANGLE_VECTOR_OFFSET 48u
#define OVL18A_SPIN_TRANSLATION_OFFSET 36u
#define OVL18A_SPIN_SCALE_VECTOR_OFFSET 56u
#define OVL18A_ADVANCE_DRAW_CONTEXT_OFFSET 8u
#define OVL18A_ENTRY_CLEAR_RECT 0x80136DF0u
#define OVL18A_ENTRY_CAMERA_START_X 0
#define OVL18A_ENTRY_CAMERA_START_Y (-256)
#define OVL18A_ENTRY_CAMERA_START_Z (-512)
#define OVL18A_ENTRY_CAMERA_TARGET_X 0
#define OVL18A_ENTRY_CAMERA_TARGET_Y (-128)
#define OVL18A_ENTRY_CAMERA_TARGET_Z 0
#define OVL18A_ENTRY_CAMERA_FRAMES 60
#define OVL18A_ENTRY_OT_SIZE 0x1000u
#define OVL18A_BUTTON_START 0x0800u
#define OVL18A_ACTOR_STRIDE 0x88u
#define OVL18A_SCENE_RECORD_STRIDE 0x0Cu
#define OVL18A_FRAME_CURSOR_OFFSET 0x04u
#define OVL18A_FRAME_CLEAR_COLOR_OFFSET 0x08u
#define OVL18A_G_ANIMATED_PANEL_PARAMETER 0x801370A0u

enum AeOvl18aMatrixField {
    OVL18A_MATRIX_R00 = 0x00,
    OVL18A_MATRIX_R01 = 0x02,
    OVL18A_MATRIX_R02 = 0x04,
    OVL18A_MATRIX_R10 = 0x06,
    OVL18A_MATRIX_R11 = 0x08,
    OVL18A_MATRIX_R12 = 0x0A,
    OVL18A_MATRIX_R20 = 0x0C,
    OVL18A_MATRIX_R21 = 0x0E,
    OVL18A_MATRIX_R22 = 0x10,
    OVL18A_MATRIX_TRANSLATION_X = 0x14,
    OVL18A_MATRIX_TRANSLATION_Y = 0x18,
    OVL18A_MATRIX_TRANSLATION_Z = 0x1C
};

enum AeOvl18aVectorField {
    OVL18A_VECTOR_X = 0x00,
    OVL18A_VECTOR_Y = 0x04,
    OVL18A_VECTOR_Z = 0x08
};

enum AeOvl18aPointField {
    OVL18A_POINT_X = 0x00,
    OVL18A_POINT_Y = 0x02,
    OVL18A_POINT_Z = 0x04
};

enum AeOvl18aActorField {
    OVL18A_ACTOR_MODEL = AE_OVL18A_ACTOR_MODEL,
    OVL18A_ACTOR_ANIMATION_DATA = AE_OVL18A_ACTOR_ANIMATION_DATA,
    OVL18A_ACTOR_TRANSFORM = 0x14,
    OVL18A_ACTOR_ANIM_FRAME = 0x5C,
    OVL18A_ACTOR_ANIM_INDEX = 0x5E,
    OVL18A_ACTOR_POS_X = 0x60,
    OVL18A_ACTOR_POS_Y = 0x62,
    OVL18A_ACTOR_POS_Z = 0x64,
    OVL18A_ACTOR_SPEED = 0x24,
    OVL18A_ACTOR_HEIGHT = 0x28,
    OVL18A_ACTOR_DEPTH = 0x2C,
    OVL18A_ACTOR_TARGET = 0x70,
    OVL18A_ACTOR_ANIM_TABLE = 0x74
};

enum AeOvl18aSceneField {
    OVL18A_SCENE_CAMERA_CURRENT = 0x10,
    OVL18A_SCENE_CAMERA_TARGET = 0x18,
    OVL18A_SCENE_CAMERA_DESTINATION_X = 0x34,
    OVL18A_SCENE_CAMERA_DESTINATION_Y = 0x36,
    OVL18A_SCENE_CAMERA_DESTINATION_Z = 0x38,
    OVL18A_SCENE_CAMERA_FRAMES_LEFT = 0x3A,
    OVL18A_SCENE_ANIM_CURRENT = 0x54,
    OVL18A_SCENE_ANIM_COUNTER = 0x5C,
    OVL18A_SCENE_MODEL_B = 0x4C,
    OVL18A_SCENE_BACKGROUND_Y = 0x50,
    OVL18A_SCENE_SPIN_Z = 0x54
};

enum AeOvl18aAreaField {
    OVL18A_AREA_SCENE_MODEL_A = 0x04,
    OVL18A_AREA_SCENE_ANIM_A = 0x0C,
    OVL18A_AREA_MESH_MODEL_A = 0x14,
    OVL18A_AREA_MESH_MODEL_B = 0x18
};

enum AeOvl18aStackField {
    OVL18A_STACK_SCALE_VECTOR_X = 0x38,
    OVL18A_STACK_SCALE_VECTOR_Y = 0x3C,
    OVL18A_STACK_SCALE_VECTOR_Z = 0x40,
    OVL18A_STACK_TRANSLATION_X = 0x24,
    OVL18A_STACK_TRANSLATION_Y = 0x28,
    OVL18A_STACK_TRANSLATION_Z = 0x2C
};

enum AeOvl18aAdvanceStackField {
    OVL18A_ADVANCE_DIRECTION = AE_OVL18A_ADVANCE_DIRECTION_OFFSET
};

static uint32_t ovl18a_stack_base(const AeOvl18aHooks *h)
{
    return h != NULL && h->stack_pointer != 0u ? h->stack_pointer : 0x801FFF00u;
}

static uint32_t ovl18a_frame_offset(uint32_t index)
{
    return index * AE_OVL18A_FRAME_STRIDE;
}

static int32_t ovl18a_bits_s32(uint32_t bits)
{
    int32_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int32_t ovl18a_asr(int32_t value, uint32_t shift)
{
    uint32_t bits = (uint32_t)value;
    if (shift == 0u) return value;
    bits >>= shift;
    if (value < 0) bits |= 0xFFFFFFFFu << (32u - shift);
    return ovl18a_bits_s32(bits);
}

static int32_t ovl18a_call(AeRam *r, const AeOvl18aHooks *h,
                           AeOvl18aNamedHook callback, const char *name,
                           uint32_t address, uint32_t argc,
                           const uint32_t *argv)
{
    if (callback == NULL) {
        ae_ram_fail(r, "AeOvl18A %s hook not provided", name);
        return 0;
    }
    return callback(h != NULL ? h->user : NULL, r, name, address, argc, argv);
}

static int32_t ovl18a_native(AeRam *r, const AeOvl18aHooks *h,
                             const char *name, uint32_t address,
                             uint32_t argc, const uint32_t *argv)
{
    return ovl18a_call(r, h, h != NULL ? h->native_call : NULL,
                       name, address, argc, argv);
}

static int32_t ovl18a_render(AeRam *r, const AeOvl18aHooks *h,
                             const char *name, uint32_t address,
                             uint32_t argc, const uint32_t *argv)
{
    return ovl18a_call(r, h, h != NULL ? h->renderer : NULL,
                       name, address, argc, argv);
}

#define OVL18A_NATIVE0(r,h,n,a) ovl18a_native((r),(h),(n),(a),0u,NULL)
#define OVL18A_NATIVE1(r,h,n,a,x) \
    ovl18a_native((r),(h),(n),(a),1u,(uint32_t[]){(uint32_t)(x)})
#define OVL18A_NATIVE2(r,h,n,a,x,y) \
    ovl18a_native((r),(h),(n),(a),2u,(uint32_t[]){(uint32_t)(x),(uint32_t)(y)})
#define OVL18A_NATIVE3(r,h,n,a,x,y,z) \
    ovl18a_native((r),(h),(n),(a),3u,(uint32_t[]){(uint32_t)(x),(uint32_t)(y),(uint32_t)(z)})
#define OVL18A_NATIVE4(r,h,n,a,x,y,z,w) \
    ovl18a_native((r),(h),(n),(a),4u,(uint32_t[]){(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),(uint32_t)(w)})

static int32_t ovl18a_signed_divide(int32_t dividend, int32_t divisor)
{
    if (divisor == 0) return 0;
    if (dividend == INT32_MIN && divisor == -1) return INT32_MIN;
    return dividend / divisor;
}

/* Return a pointer to the indexed NUL-terminated entry in a packed string table. */
uint32_t ae_Ovl18A_StringTableIndex(AeRam *r, uint32_t strings,
                                   int32_t index)
{
    uint32_t entries = 0u;
    if (index <= 0) return strings;
    while (entries < (uint32_t)index) {
        if (ae_rd8(r, strings) == 0u) entries++;
        strings++;
    }
    return strings;
}

/* Full camera look-at matrix @80136000. */
void ae_StageClearUi_LookAt_18A(AeRam *r, const AeOvl18aHooks *h,
                                uint32_t eye, uint32_t target)
{
    uint32_t frame = ovl18a_stack_base(h) - OVL18A_LOOKAT_FRAME_BYTES;
    uint32_t vector = frame + OVL18A_LOOKAT_VECTOR_OFFSET;
    uint32_t normalized = frame + OVL18A_LOOKAT_NORMAL_OFFSET;
    uint32_t scaled = frame + OVL18A_LOOKAT_SCALE_MATRIX_OFFSET;
    uint32_t scale = frame + OVL18A_LOOKAT_SCALE_VECTOR_OFFSET;
    uint32_t view = AE_OVL18A_G_RENDER_MATRIX;
    int32_t dx = (int32_t)ae_rds16(r, target + OVL18A_POINT_X) -
                 (int32_t)ae_rds16(r, eye + OVL18A_POINT_X);
    int32_t dy = (int32_t)ae_rds16(r, target + OVL18A_POINT_Y) -
                 (int32_t)ae_rds16(r, eye + OVL18A_POINT_Y);
    int32_t dz = (int32_t)ae_rds16(r, target + OVL18A_POINT_Z) -
                 (int32_t)ae_rds16(r, eye + OVL18A_POINT_Z);
    int32_t horizontal_x;
    int32_t horizontal_z;
    int32_t direction_y;
    int32_t horizontal_length;
    uint32_t product;

    ae_wr32(r, view + OVL18A_MATRIX_TRANSLATION_X,
            0u - (uint32_t)ae_rds16(r, eye + OVL18A_POINT_X));
    ae_wr32(r, view + OVL18A_MATRIX_TRANSLATION_Y,
            0u - (uint32_t)ae_rds16(r, eye + OVL18A_POINT_Y));
    ae_wr32(r, view + OVL18A_MATRIX_TRANSLATION_Z,
            0u - (uint32_t)ae_rds16(r, eye + OVL18A_POINT_Z));
    if (dx == 0 && dz == 0) dz = 4096;

    ae_wr32(r, vector + OVL18A_VECTOR_X, (uint32_t)dx);
    ae_wr32(r, vector + OVL18A_VECTOR_Y, 0u);
    ae_wr32(r, vector + OVL18A_VECTOR_Z, (uint32_t)dz);
    (void)ae_sdk_VectorNormal(r, vector, normalized);
    if (ae_ram_failed(r)) return;
    horizontal_x = ae_rds32(r, normalized + OVL18A_VECTOR_X);
    horizontal_z = ae_rds32(r, normalized + OVL18A_VECTOR_Z);

    ae_wr32(r, vector + OVL18A_VECTOR_Y, (uint32_t)dy);
    (void)ae_sdk_VectorNormal(r, vector, normalized);
    if (ae_ram_failed(r)) return;
    direction_y = ae_rds32(r, normalized + OVL18A_VECTOR_Y);
    product = (uint32_t)ae_rds32(r, normalized + OVL18A_VECTOR_X) *
              (uint32_t)ae_rds32(r, normalized + OVL18A_VECTOR_X) +
              (uint32_t)ae_rds32(r, normalized + OVL18A_VECTOR_Z) *
              (uint32_t)ae_rds32(r, normalized + OVL18A_VECTOR_Z);
    horizontal_length = ae_sdk_SquareRoot0(r, ovl18a_bits_s32(product));
    if (ae_ram_failed(r)) return;

    ae_wr16(r, view + OVL18A_MATRIX_R00, (uint32_t)horizontal_z);
    ae_wr16(r, view + OVL18A_MATRIX_R01, 0u);
    ae_wr16(r, view + OVL18A_MATRIX_R02, 0u - (uint32_t)horizontal_x);
    product = (0u - (uint32_t)horizontal_x) * (uint32_t)direction_y;
    ae_wr16(r, view + OVL18A_MATRIX_R10,
            (uint32_t)ovl18a_asr(ovl18a_bits_s32(product), 12u));
    ae_wr16(r, view + OVL18A_MATRIX_R11, (uint32_t)horizontal_length);
    product = (uint32_t)direction_y * (uint32_t)horizontal_z;
    ae_wr16(r, view + OVL18A_MATRIX_R12,
            (uint32_t)ovl18a_asr(ovl18a_bits_s32(0u - product), 12u));
    product = (uint32_t)horizontal_length * (uint32_t)horizontal_x;
    ae_wr16(r, view + OVL18A_MATRIX_R20,
            (uint32_t)ovl18a_asr(ovl18a_bits_s32(product), 12u));
    ae_wr16(r, view + OVL18A_MATRIX_R21, (uint32_t)direction_y);
    product = (uint32_t)horizontal_length * (uint32_t)horizontal_z;
    ae_wr16(r, view + OVL18A_MATRIX_R22,
            (uint32_t)ovl18a_asr(ovl18a_bits_s32(product), 12u));
    if (ae_ram_failed(r)) return;

    (void)ae_sdk_apply_matrix_lv(ae_ptr(r, view, 32u),
                                 ae_ptr(r, view + OVL18A_MATRIX_TRANSLATION_X, 12u),
                                 ae_ptr(r, view + OVL18A_MATRIX_TRANSLATION_X, 12u));
    (void)ae_sdk_CompMatrix(r, AE_OVL18A_G_BASE_MATRIX, view, view);
    (void)ae_sdk_MatrixIdentity(r, scaled);
    ae_wr32(r, scale + OVL18A_VECTOR_X, 0x2000u);
    ae_wr32(r, scale + OVL18A_VECTOR_Y, 0x2000u);
    ae_wr32(r, scale + OVL18A_VECTOR_Z, 0x2000u);
    (void)OVL18A_NATIVE2(r, h, "ScaleMatrixL", OVL18A_API_SCALE_MATRIX_L,
                         scaled, scale);
    (void)ae_sdk_CompMatrix(r, scaled, view, view);
}

/* Wait until Start is pressed or the 601-frame timeout expires @80136724. */
int32_t ae_Ovl18A_WaitInputTimeout(AeRam *r)
{
    uint32_t buttons = ae_rd32(r, AE_OVL18A_PAD_STATE + AE_OVL18A_PAD_HELD) &
                       ae_rd32(r, AE_OVL18A_PAD_STATE + AE_OVL18A_PAD_CHANGED);
    uint32_t timeout = ae_rd16(r, AE_OVL18A_G_TIMEOUT);
    if ((buttons & OVL18A_BUTTON_START) != 0u) return -1;
    timeout = (timeout + 1u) & 0xFFFFu;
    ae_wr16(r, AE_OVL18A_G_TIMEOUT, timeout);
    int32_t signed_timeout = timeout < 0x8000u ? (int32_t)timeout :
                            (int32_t)timeout - 0x10000;
    if (signed_timeout < 601) return 0;
    return -1;
}

/* Build the loaded area's two mesh buffers and actor records @80136780. */
int32_t ae_Ovl18A_SceneInit(AeRam *r, const AeOvl18aHooks *h)
{
    uint32_t area = AE_OVL18A_G_AREA_BASE;
    uint32_t mesh_count = 2u;
    uint32_t mesh_buffer;
    uint32_t actor = AE_OVL18A_G_ACTOR;

    ae_wr32(r, AE_OVL18A_G_MODEL_BUFFER_COUNT, mesh_count);
    mesh_buffer = ae_HeapAlloc(r, h != NULL ? h->area_set : NULL, 64u);
    ae_wr32(r, AE_OVL18A_G_MODEL_BUFFER, mesh_buffer);
    if (ae_ram_failed(r)) return 0;

    for (uint32_t mesh_index = 0; mesh_index < mesh_count; ++mesh_index) {
        uint32_t model_id = ae_rd16(r, area + OVL18A_AREA_MESH_MODEL_A +
                                    mesh_index * 4u);
        uint32_t instance = mesh_buffer + mesh_index * 32u;
        uint32_t model_header = area + model_id * 4u;
        (void)OVL18A_NATIVE2(r, h, "MeshInit", OVL18A_API_MESH_INIT,
                             instance, model_header);
        if (ae_ram_failed(r)) return 0;
    }

    for (uint32_t actor_index = 0; actor_index < 2u; ++actor_index) {
        uint32_t model_id = ae_rd16(r, area + OVL18A_AREA_SCENE_MODEL_A +
                                    actor_index * 4u);
        uint32_t animation_id = ae_rd16(r, area + OVL18A_AREA_SCENE_ANIM_A +
                                        actor_index * 4u);
        uint32_t animation_table = area + animation_id * 4u;
        uint32_t animation_count = ae_rd32(r, animation_table);
        uint32_t model_header = area + model_id * 4u;
        uint32_t animation_data;

        animation_data = animation_table + 4u + animation_count * 4u;
        ae_ModelInit(r, h != NULL ? h->area_set : NULL, actor, model_header);
        ae_wr32(r, actor + OVL18A_ACTOR_ANIMATION_DATA, animation_data);
        ae_wr16(r, actor + OVL18A_ACTOR_ANIM_FRAME, 0u);
        ae_wr16(r, actor + OVL18A_ACTOR_ANIM_INDEX, 0u);
        ae_wr16(r, actor + OVL18A_ACTOR_TARGET, 0u);
        ae_wr16(r, actor + OVL18A_ACTOR_TARGET + 2u, 0u);
        ae_wr16(r, actor + OVL18A_ACTOR_POS_X, 0u);
        ae_wr16(r, actor + OVL18A_ACTOR_POS_Y, 0u);
        ae_wr16(r, actor + OVL18A_ACTOR_POS_Z, 0xF000u);
        ae_wr32(r, actor + OVL18A_ACTOR_ANIM_TABLE, animation_table + 4u);
        if (ae_ram_failed(r)) return 0;
        actor += OVL18A_ACTOR_STRIDE;
    }
    return 0;
}

/* Advance and draw the foreground model, then emit the background model @801368CC. */
void ae_Ovl18A_UpdateSpin(AeRam *r, const AeOvl18aHooks *h)
{
    uint32_t actor = AE_OVL18A_G_ACTOR;
    uint32_t frame = ovl18a_stack_base(h) - OVL18A_SPIN_FRAME_BYTES;
    uint32_t angle_vector = frame + OVL18A_SPIN_ANGLE_VECTOR_OFFSET;
    uint32_t local_matrix = frame + OVL18A_SPIN_LOCAL_MATRIX_OFFSET;
    uint32_t scale_vector = frame + OVL18A_STACK_SCALE_VECTOR_X;
    uint32_t translated_vector = frame + OVL18A_STACK_TRANSLATION_X;
    uint32_t transform = ae_rd32(r, actor + OVL18A_ACTOR_TRANSFORM);
    uint32_t model_buffer = ae_rd32(r, AE_OVL18A_G_MODEL_BUFFER);
    uint32_t frame_index = ae_rd32(r, AE_OVL18A_G_FRAME_COUNTER);
    uint32_t animation_table = ae_rd32(r, actor + OVL18A_ACTOR_ANIM_TABLE);
    uint32_t animation_frame;
    uint32_t animation_index;
    int32_t animation_end;

    ae_wr16(r, angle_vector, 0u);
    ae_wr16(r, angle_vector + OVL18A_POINT_Y, frame_index * 12u);
    ae_wr16(r, angle_vector + OVL18A_POINT_Z, 0u);
    (void)ae_sdk_RotMatrixZYX(r, angle_vector, transform);

    ae_wr32(r, actor + OVL18A_ACTOR_SPEED, 0u);
    ae_wr32(r, actor + OVL18A_ACTOR_HEIGHT, 0u);
    ae_wr32(r, actor + OVL18A_ACTOR_DEPTH, 0u);
    animation_frame = (ae_rd16(r, actor + OVL18A_ACTOR_ANIM_FRAME) + 1u) &
                      0xFFFFu;
    ae_wr16(r, actor + OVL18A_ACTOR_ANIM_FRAME, animation_frame);
    animation_index = ae_rd16(r, actor + OVL18A_ACTOR_ANIM_INDEX);
    animation_end = ae_rds16(r, animation_table + animation_index * 4u + 2u);
    if (animation_end < (int32_t)animation_frame) {
        int32_t target = ae_rds16(r, actor + OVL18A_ACTOR_TARGET);
        animation_index = (uint32_t)target;
        animation_frame = ae_rd16(r, animation_table + (uint32_t)target * 4u);
        ae_wr16(r, actor + OVL18A_ACTOR_ANIM_INDEX, animation_index);
        ae_wr16(r, actor + OVL18A_ACTOR_ANIM_FRAME, animation_frame);
    }
    if (ae_ram_failed(r)) return;

    ae_ModelPoseBones(r, actor, 1u);
    if (ae_ram_failed(r)) return;
    (void)OVL18A_NATIVE1(r, h, "ModelTransformSkinned",
                         OVL18A_API_MODEL_TRANSFORM_SKINNED, actor);
    (void)OVL18A_NATIVE3(r, h, "ModelDraw", OVL18A_API_MODEL_DRAW,
                         ae_rd32(r, AE_OVL18A_G_FRAME + 8u), 0x800E7D00u,
                         actor);
    (void)OVL18A_NATIVE3(r, h, "MeshTransform", OVL18A_API_MODEL_TRANSFORM,
                         model_buffer, transform, 0x800E7D00u);
    (void)OVL18A_NATIVE4(r, h, "MeshDrawAnimColors",
                         OVL18A_API_MODEL_DRAW_BACKGROUND,
                         ae_rd32(r, AE_OVL18A_G_FRAME + 8u), 0x800E7D00u,
                         model_buffer, 0u);

    ae_wr16(r, angle_vector, 0u);
    ae_wr16(r, angle_vector + OVL18A_POINT_Y,
            0u - (uint32_t)(frame_index << 6));
    ae_wr16(r, angle_vector + OVL18A_POINT_Z, 0u);
    (void)ae_sdk_RotMatrixZYX(r, angle_vector, local_matrix);
    {
        int32_t sine = ae_sdk_rsin(r, (int32_t)(frame_index << 7));
        int32_t scale = ovl18a_asr(sine, 3u);
        ae_wr32(r, scale_vector + OVL18A_VECTOR_X,
                4096u - (uint32_t)scale);
        ae_wr32(r, scale_vector + OVL18A_VECTOR_Y,
                4096u + (uint32_t)scale);
        ae_wr32(r, scale_vector + OVL18A_VECTOR_Z,
                4096u - (uint32_t)scale);
        (void)ae_sdk_ScaleMatrix(r, local_matrix, scale_vector);
    }
    ae_wr32(r, translated_vector + OVL18A_VECTOR_X, 0u);
    ae_wr32(r, translated_vector + OVL18A_VECTOR_Y, (uint32_t)-128);
    ae_wr32(r, translated_vector + OVL18A_VECTOR_Z, 96u);
    (void)ae_sdk_CompMatrix(r, transform, local_matrix, local_matrix);
    (void)OVL18A_NATIVE3(r, h, "MeshTransform", OVL18A_API_MODEL_TRANSFORM,
                         model_buffer + AE_OVL18A_MESH_INSTANCE_STRIDE,
                         local_matrix, 0x800E7D00u);
    (void)OVL18A_NATIVE4(r, h, "MeshDrawAnimColors",
                         OVL18A_API_MODEL_DRAW_BACKGROUND,
                         ae_rd32(r, AE_OVL18A_G_FRAME + 8u),
                         0x800E7D00u,
                         model_buffer + AE_OVL18A_MESH_INSTANCE_STRIDE, 0u);
    (void)translated_vector;
}

/* Move the background animation and smoothly change its pose @80136AA0. */
void ae_Ovl18A_AdvanceAnim(AeRam *r, const AeOvl18aHooks *h)
{
    uint32_t config = AE_OVL18A_G_SCENE_CONFIG;
    uint32_t actor = AE_OVL18A_G_ACTOR_SECOND;
    uint32_t frame = ovl18a_stack_base(h) - OVL18A_ADVANCE_FRAME_BYTES;
    uint32_t direction = frame + OVL18A_ADVANCE_DIRECTION;
    uint32_t animation_table = ae_rd32(r, actor + OVL18A_ACTOR_ANIM_TABLE);
    uint32_t active = ae_rd32(r, config + OVL18A_SCENE_ANIM_COUNTER);
    uint32_t target = ae_rd32(r, config + OVL18A_SCENE_ANIM_CURRENT);
    uint32_t done = 0u;

    if (active < target) return;

    uint32_t animation_frame = (ae_rd16(r, actor + OVL18A_ACTOR_ANIM_FRAME) +
                                1u) & 0xFFFFu;
    ae_wr16(r, actor + OVL18A_ACTOR_ANIM_FRAME, animation_frame);
    uint32_t animation_index = ae_rd16(r, actor + OVL18A_ACTOR_ANIM_INDEX);
    int32_t animation_end = ae_rds16(r, animation_table +
                                     animation_index * 4u + 2u);
    if (animation_end < (int32_t)animation_frame) {
        int32_t next_clip = ae_rds16(r, actor + OVL18A_ACTOR_TARGET);
        if (next_clip < 0) {
            animation_frame = (animation_frame - 1u) & 0xFFFFu;
            ae_wr16(r, actor + OVL18A_ACTOR_ANIM_FRAME, animation_frame);
            done = 1u;
        } else {
            uint32_t next_index = ae_rd16(r, actor + OVL18A_ACTOR_TARGET);
            animation_frame = ae_rd16(r, animation_table + next_index * 4u);
            ae_wr16(r, actor + OVL18A_ACTOR_ANIM_INDEX, next_index);
            ae_wr16(r, actor + OVL18A_ACTOR_ANIM_FRAME, animation_frame);
        }
    }

    (void)ae_sdk_VectorNormalSS(r, actor + OVL18A_ACTOR_POS_X, direction);
    (void)ae_sdk_MatrixFromDirXZ(r, direction,
                                 ae_rd32(r, actor + OVL18A_ACTOR_TRANSFORM));
    if (done != 0u) {
        int32_t speed = ae_StepToward(r,
                                      ae_rds32(r, actor + OVL18A_ACTOR_SPEED),
                                      316, 16);
        ae_wr32(r, actor + OVL18A_ACTOR_SPEED, (uint32_t)speed);
        ae_wr32(r, actor + OVL18A_ACTOR_DEPTH, (uint32_t)-300);
    } else {
        int32_t y = ae_rds32(r, actor + OVL18A_ACTOR_HEIGHT);
        int32_t base_y = ae_rds32(r, config + OVL18A_SCENE_BACKGROUND_Y);
        ae_wr32(r, actor + OVL18A_ACTOR_SPEED, 64u);
        ae_wr32(r, actor + OVL18A_ACTOR_DEPTH, (uint32_t)-300);
        ae_wr32(r, actor + OVL18A_ACTOR_HEIGHT,
                (uint32_t)ovl18a_asr(ovl18a_bits_s32((uint32_t)y +
                                                    (uint32_t)base_y + 48u),
                                     1u));
    }
    ae_ModelPoseBones(r, actor, 1u);
    if (ae_ram_failed(r)) return;
    (void)OVL18A_NATIVE1(r, h, "ModelTransformSkinned",
                         OVL18A_API_MODEL_TRANSFORM_SKINNED, actor);
    (void)OVL18A_NATIVE3(r, h, "ModelDraw", OVL18A_API_MODEL_DRAW,
                         AE_OVL18A_G_ORDER_TABLES +
                             ovl18a_frame_offset(ae_rd32(r, AE_OVL18A_G_OT_INDEX)),
                         0x800E7D00u, actor);
}

/* Select the scene row's clips and reset its camera animation @80136C04. */
void ae_Ovl18A_StartClip(AeRam *r)
{
    uint32_t selection = ae_rd32(r, AE_OVL18A_G_SCENE_INDEX);
    uint32_t row = AE_OVL18A_G_CLIP_TABLE + selection * 12u;
    uint32_t actor = AE_OVL18A_G_ACTOR_SECOND;
    uint32_t clip = ae_rd8(r, row + 1u);
    uint32_t animation_table = ae_rd32(r, actor + OVL18A_ACTOR_ANIM_TABLE);
    uint32_t start_frame = ae_rd16(r, animation_table + clip * 4u);

    ae_wr16(r, actor + OVL18A_ACTOR_POS_X, (uint32_t)-1024);
    ae_wr16(r, actor + OVL18A_ACTOR_POS_Z, (uint32_t)-4096);
    ae_wr16(r, actor + OVL18A_ACTOR_TARGET, 0xFFFFu);
    ae_wr32(r, actor + OVL18A_ACTOR_HEIGHT, 512u);
    ae_wr16(r, actor + OVL18A_ACTOR_ANIM_INDEX, clip);
    ae_wr16(r, actor + OVL18A_ACTOR_ANIM_FRAME, start_frame);
}

typedef struct Ovl18aXaContext {
    const AePlayer1Hooks *source;
    uint32_t output_address;
} Ovl18aXaContext;

static uint32_t ovl18a_xa_forward(void *user, AeRam *r, AePlayer1HookFn hook,
                                  uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    Ovl18aXaContext *context = (Ovl18aXaContext *)user;
    return hook(context->source->user, r, a0, a1, a2, a3);
}

static uint32_t ovl18a_xa_cd_int_to_pos(void *user, AeRam *r,
                                       uint32_t sector, uint32_t output,
                                       uint32_t a2, uint32_t a3)
{
    Ovl18aXaContext *context = (Ovl18aXaContext *)user;
    uint32_t result = ovl18a_xa_forward(user, r, context->source->CdIntToPos,
                                        sector, context->output_address, a2, a3);
    ae_wr32(r, output, ae_rd32(r, context->output_address));
    return result;
}

static uint32_t ovl18a_xa_cd_pos_to_int(void *user, AeRam *r,
                                        uint32_t a0, uint32_t a1,
                                        uint32_t a2, uint32_t a3)
{
    Ovl18aXaContext *context = (Ovl18aXaContext *)user;
    return ovl18a_xa_forward(user, r, context->source->CdPosToInt,
                             a0, a1, a2, a3);
}

static uint32_t ovl18a_xa_cd_control(void *user, AeRam *r,
                                     uint32_t a0, uint32_t a1,
                                     uint32_t a2, uint32_t a3)
{
    Ovl18aXaContext *context = (Ovl18aXaContext *)user;
    return ovl18a_xa_forward(user, r, context->source->CdControl,
                             a0, a1, a2, a3);
}

static uint32_t ovl18a_xa_cd_ready_callback(void *user, AeRam *r,
                                            uint32_t a0, uint32_t a1,
                                            uint32_t a2, uint32_t a3)
{
    Ovl18aXaContext *context = (Ovl18aXaContext *)user;
    return ovl18a_xa_forward(user, r, context->source->CdReadyCallback,
                             a0, a1, a2, a3);
}

static uint32_t ovl18a_xa_vsync(void *user, AeRam *r,
                                uint32_t a0, uint32_t a1,
                                uint32_t a2, uint32_t a3)
{
    Ovl18aXaContext *context = (Ovl18aXaContext *)user;
    return ovl18a_xa_forward(user, r, context->source->VSync,
                             a0, a1, a2, a3);
}

static uint32_t ovl18a_xa_printf(void *user, AeRam *r,
                                 uint32_t a0, uint32_t a1,
                                 uint32_t a2, uint32_t a3)
{
    Ovl18aXaContext *context = (Ovl18aXaContext *)user;
    return ovl18a_xa_forward(user, r, context->source->printf,
                             a0, a1, a2, a3);
}

static uint32_t ovl18a_xa_sound_volume_mode(void *user, AeRam *r,
                                            uint32_t a0, uint32_t a1,
                                            uint32_t a2, uint32_t a3)
{
    (void)user;
    (void)a1;
    (void)a2;
    (void)a3;
    return (uint32_t)ae_SoundSetVolumeMode(r, (int32_t)a0);
}

/* Start the selected XA track through the verified player sound port @80136C6C. */
int32_t ae_Ovl18A_PlayXa(AeRam *r, const AeOvl18aHooks *h)
{
    if (h == NULL || h->player1 == NULL) {
        ae_ram_fail(r, "AeOvl18aHooks.player1 not provided");
        return 0;
    }

    uint32_t call_stack = ovl18a_stack_base(h);
    uint32_t xa_frame = call_stack - OVL18A_PLAY_XA_FRAME_BYTES -
                        OVL18A_XA_PLAY_FRAME_BYTES;
    Ovl18aXaContext context = {
        h->player1,
        xa_frame + (AE_PLAYER1_XA_STACK_LOCATION - AE_PLAYER1_XA_STACK_FRAME)
    };
    AePlayer1Hooks player_hooks = *h->player1;

    /* The original queues four-byte words from two byte-sized stack locals. */
    for (uint32_t byte = 2u; byte < 4u; ++byte) {
        ae_wr8(r, AE_PLAYER1_XA_STACK_PARAMS + byte,
              ae_rd8(r, xa_frame + (AE_PLAYER1_XA_STACK_PARAMS -
                                    AE_PLAYER1_XA_STACK_FRAME) + byte));
    }
    for (uint32_t byte = 1u; byte < 4u; ++byte) {
        ae_wr8(r, AE_PLAYER1_XA_STACK_MODE + byte,
              ae_rd8(r, xa_frame + (AE_PLAYER1_XA_STACK_MODE -
                                    AE_PLAYER1_XA_STACK_FRAME) + byte));
    }

    player_hooks.user = &context;
    player_hooks.CdIntToPos = h->player1->CdIntToPos != NULL ?
                             ovl18a_xa_cd_int_to_pos : NULL;
    player_hooks.CdPosToInt = h->player1->CdPosToInt != NULL ?
                              ovl18a_xa_cd_pos_to_int : NULL;
    player_hooks.CdControl = h->player1->CdControl != NULL ?
                             ovl18a_xa_cd_control : NULL;
    player_hooks.CdReadyCallback = h->player1->CdReadyCallback != NULL ?
                                  ovl18a_xa_cd_ready_callback : NULL;
    player_hooks.VSync = h->player1->VSync != NULL ? ovl18a_xa_vsync : NULL;
    player_hooks.printf = h->player1->printf != NULL ? ovl18a_xa_printf : NULL;
    player_hooks.SoundSetVolumeMode = ovl18a_xa_sound_volume_mode;
    return ae_XaPlay(r, &player_hooks,
                     (int32_t)ae_rd32(r, AE_OVL18A_G_XA_INDEX));
}

/* Resolve the next of fourteen demo scenes using saved progress @80136C94. */
void ae_Ovl18A_PickScene(AeRam *r)
{
    uint32_t frame = ae_rd32(r, AE_OVL18A_G_FRAME_COUNTER);
    uint32_t scene = frame % 14u;

    switch (scene) {
    case 0u:
    case 1u:
    case 2u:
        if (ae_LevelProgressGet(r, 17u) > 0 &&
            ae_ProgressGetByte(r, 4u) != 5u)
            scene += 6u;
        break;
    case 3u:
    case 4u:
    case 5u:
        if (ae_LevelProgressGet(r, 17u) > 0 &&
            ae_ProgressGetByte(r, 5u) != 5u)
            scene += 3u;
        break;
    case 9u:
    case 10u:
        if (ae_ProgressGetByte(r, 8u) == 5u) scene -= 2u;
        break;
    case 11u:
    case 12u:
    case 13u:
        if (ae_ProgressGetByte(r, 12u) == 5u) scene -= 5u;
        break;
    default:
        break;
    }

    uint32_t row = AE_OVL18A_G_CLIP_TABLE + scene * 12u;
    uint32_t config = AE_OVL18A_G_SCENE_CONFIG;
    ae_wr32(r, AE_OVL18A_G_SCENE_INDEX, scene);
    ae_wr32(r, config + OVL18A_SCENE_MODEL_B, ae_rd16(r, row + 2u));
    ae_wr32(r, config + OVL18A_SCENE_BACKGROUND_Y,
            (uint32_t)ae_rds16(r, row + 6u));
    ae_wr32(r, config + OVL18A_SCENE_ANIM_CURRENT,
            (uint32_t)ae_rds16(r, row + 4u));
    ae_wr16(r, AE_OVL18A_G_XA_OFFSET, ae_rd16(r, row + 8u));
}

/* Scene spin and XA viewer mode @80136208. */
int32_t ae_ovl18A_entry_80136208(AeRam *r, const AeOvl18aHooks *h)
{
    uint32_t stack_pointer = ovl18a_stack_base(h) - OVL18A_ENTRY_FRAME_BYTES;
    uint32_t rectangle = stack_pointer + 0x10u;
    uint32_t frame = AE_OVL18A_G_FRAME;
    uint32_t scene = AE_OVL18A_G_SCENE_CONFIG;
    int32_t load_result;
    AeOvl18aHooks nested_hooks = h != NULL ? *h : (AeOvl18aHooks){0};

    nested_hooks.stack_pointer = stack_pointer;

    ae_wr32(r, rectangle, ae_rd32(r, OVL18A_ENTRY_CLEAR_RECT));
    ae_wr32(r, rectangle + 4u, ae_rd32(r, OVL18A_ENTRY_CLEAR_RECT + 4u));
    ae_HeapInit(r, h != NULL ? h->area_set : NULL);
    (void)OVL18A_NATIVE0(r, h, "InitDisplay", OVL18A_API_INIT_DISPLAY);
    (void)OVL18A_NATIVE4(r, h, "ClearImage", OVL18A_API_CLEAR_IMAGE,
                         rectangle, 0u, 0u, 0u);
    (void)OVL18A_NATIVE1(r, h, "DrawSync", OVL18A_API_DRAW_SYNC, 0u);
    (void)OVL18A_NATIVE1(r, h, "PutDispEnv", OVL18A_API_PUT_DISPLAY_ENV,
                         AE_OVL18A_G_DISPLAY_ENV);
    ae_wr32(r, AE_OVL18A_G_OT_INDEX, 0u);
    (void)OVL18A_NATIVE1(r, h, "PutDrawEnv", OVL18A_API_PUT_DRAW_ENV,
                         AE_OVL18A_G_DRAW_ENV);
    (void)OVL18A_NATIVE1(r, h, "VSync", OVL18A_API_VSYNC, 0u);
    ae_wr16(r, AE_OVL18A_G_TIMEOUT, 0u);
    ae_Ovl18A_PickScene(r);
    (void)OVL18A_NATIVE1(r, h, "ArcLoadEntry", OVL18A_API_ARC_LOAD_ENTRY,
                         AE_OVL18A_G_XA_FILE);
    do {
        load_result = OVL18A_NATIVE0(r, h, "LoadListStep",
                                     OVL18A_API_LOAD_LIST_STEP);
        if (load_result >= 0) {
            (void)OVL18A_NATIVE1(r, h, "VSync", OVL18A_API_VSYNC, 2u);
            (void)OVL18A_NATIVE0(r, h, "CdMgrUpdate", OVL18A_API_CD_UPDATE);
        }
    } while (load_result >= 0 && !ae_ram_failed(r));
    if (ae_ram_failed(r)) return 0;
    (void)OVL18A_NATIVE1(r, h, "CdMgrStopStream",
                         OVL18A_API_CD_STOP_STREAM, 0u);

    (void)ae_Ovl18A_SceneInit(r, h);
    (void)OVL18A_NATIVE0(r, h, "SoundStopAndRestoreVolume",
                         OVL18A_API_SOUND_STOP_RESTORE);
    (void)OVL18A_NATIVE1(r, h, "SetGeomScreen", OVL18A_API_SET_GEOM_SCREEN,
                         328u);
    ae_SetColorMatrixMode(r, h != NULL ? h->items : NULL,
                          0u, 0u, 0u, 0u);

    ae_wr32(r, AE_OVL18A_G_LEVEL_GRID + AE_OVL18A_LEVEL_GRID_FOG_PARAMETER, 0u);
    ae_wr32(r, AE_OVL18A_G_LEVEL_GRID + AE_OVL18A_LEVEL_GRID_FOG_START,
            OVL18A_ENTRY_OT_SIZE);
    ae_wr32(r, AE_OVL18A_G_LEVEL_GRID + AE_OVL18A_LEVEL_GRID_FOG_END,
            OVL18A_ENTRY_OT_SIZE);

    ae_wr16(r, scene + OVL18A_SCENE_CAMERA_CURRENT, 0u);
    ae_wr16(r, scene + OVL18A_SCENE_CAMERA_CURRENT + 2u,
            (uint32_t)OVL18A_ENTRY_CAMERA_START_Y);
    ae_wr16(r, scene + OVL18A_SCENE_CAMERA_CURRENT + 4u,
            (uint32_t)OVL18A_ENTRY_CAMERA_START_Z);
    ae_wr16(r, scene + OVL18A_SCENE_CAMERA_TARGET, 0u);
    ae_wr16(r, scene + OVL18A_SCENE_CAMERA_TARGET + 2u,
            (uint32_t)OVL18A_ENTRY_CAMERA_TARGET_Y);
    ae_wr16(r, scene + OVL18A_SCENE_CAMERA_TARGET + 4u, 0u);
    ae_wr16(r, scene + OVL18A_SCENE_CAMERA_FRAMES_LEFT,
            OVL18A_ENTRY_CAMERA_FRAMES);
    ae_wr16(r, scene + OVL18A_SCENE_CAMERA_DESTINATION_X,
            (uint32_t)OVL18A_ENTRY_CAMERA_START_X);
    ae_wr16(r, scene + OVL18A_SCENE_CAMERA_DESTINATION_Y,
            (uint32_t)OVL18A_ENTRY_CAMERA_START_Y);
    ae_wr16(r, scene + OVL18A_SCENE_CAMERA_DESTINATION_Z,
            (uint32_t)OVL18A_ENTRY_CAMERA_START_Z);
    ae_wr32(r, scene + OVL18A_SCENE_ANIM_COUNTER, 0u);
    ae_StageClearUi_LookAt_18A(r, &nested_hooks,
                              scene + OVL18A_SCENE_CAMERA_CURRENT,
                              scene + OVL18A_SCENE_CAMERA_TARGET);
    ae_Ovl18A_StartClip(r);
    (void)ae_Ovl18A_PlayXa(r, &nested_hooks);
    if (ae_ram_failed(r)) return 0;

    ae_pad_PadUpdate(r, h != NULL ? h->pad : NULL, 0u);
    for (;;) {
        int32_t wait_result;
        uint32_t cursor;
        uint32_t ot_index;
        uint32_t args[4];

        (void)OVL18A_NATIVE0(r, h, "BeginFrame", OVL18A_API_BEGIN_FRAME);
        ae_ColorFadeReset(r);
        cursor = ae_rd32(r, AE_OVL18A_G_FRAME_CURSOR);
        ot_index = ae_rd32(r, AE_OVL18A_G_OT_INDEX);
        args[0] = cursor;
        args[1] = ot_index;
        args[2] = ae_rd32(r, AE_OVL18A_G_SCENE_INDEX);
        args[3] = ae_rd32(r, AE_OVL18A_G_FRAME + 8u);
        (void)ovl18a_render(r, h, "Ovl18A_DrawClearPanel",
                            AE_OVL18A_BASE + 0x03B4u, 4u, args);
        ae_Ovl18A_UpdateSpin(r, &nested_hooks);
        ae_wr32(r, AE_OVL18A_G_FRAME_CURSOR, cursor + 24u);
        ae_Ovl18A_AdvanceAnim(r, &nested_hooks);

        cursor = ae_rd32(r, AE_OVL18A_G_FRAME_CURSOR);
        args[0] = cursor;
        args[1] = ot_index;
        args[2] = ae_rd32(r, AE_OVL18A_G_SCENE_INDEX);
        args[3] = ae_rd32(r, OVL18A_G_ANIMATED_PANEL_PARAMETER);
        (void)ovl18a_render(r, h, "Ovl18A_DrawAnimatedPanel",
                            AE_OVL18A_BASE + 0x042Cu, 4u, args);
        ae_wr32(r, AE_OVL18A_G_FRAME_CURSOR, cursor + 16u);
        wait_result = ae_Ovl18A_WaitInputTimeout(r);
        if (ae_ram_failed(r)) return 0;

        if (wait_result < 0 && ae_rd32(r, AE_OVL18A_G_SYSTEM_STATE) == 0u) break;
        (void)OVL18A_NATIVE1(r, h, "DrawSync", OVL18A_API_DRAW_SYNC, 0u);
        (void)OVL18A_NATIVE1(r, h, "VSync", OVL18A_API_VSYNC, 0u);
        ae_wr32(r, frame, ae_rd32(r, frame) + 1u);
        {
            uint32_t frame_offset = ovl18a_frame_offset(ae_rd32(r, AE_OVL18A_G_OT_INDEX));
            (void)OVL18A_NATIVE1(r, h, "PutDispEnv",
                                 OVL18A_API_PUT_DISPLAY_ENV,
                                 AE_OVL18A_G_DISPLAY_ENV + frame_offset);
            (void)OVL18A_NATIVE1(r, h, "PutDrawEnv",
                                 OVL18A_API_PUT_DRAW_ENV,
                                 AE_OVL18A_G_DRAW_ENV + frame_offset);
            (void)OVL18A_NATIVE1(r, h, "DrawOTag", OVL18A_API_DRAW_OTAG,
                                 AE_OVL18A_G_ORDER_TABLES + frame_offset +
                                     AE_OVL18A_OT_END);
        }
        (void)OVL18A_NATIVE0(r, h, "CdMgrUpdate", OVL18A_API_CD_UPDATE);

        int32_t frames_left = ae_rds16(r, scene + OVL18A_SCENE_CAMERA_FRAMES_LEFT);
        if (frames_left > 0) {
            for (uint32_t axis = 0; axis < 3u; ++axis) {
                uint32_t current_address = scene + OVL18A_SCENE_CAMERA_CURRENT + axis * 2u;
                uint32_t destination_offset = OVL18A_SCENE_CAMERA_DESTINATION_X + axis * 2u;
                int32_t difference = (int32_t)ae_rds16(r, scene + destination_offset) -
                                     (int32_t)ae_rds16(r, current_address);
                int32_t step = ovl18a_signed_divide(difference, frames_left);
                ae_wr16(r, current_address,
                        ae_rd16(r, current_address) + (uint32_t)step);
            }
            ae_wr16(r, scene + OVL18A_SCENE_CAMERA_FRAMES_LEFT,
                    (uint32_t)(frames_left - 1));
        }
        ae_StageClearUi_LookAt_18A(r,
                                  &nested_hooks,
                                  scene + OVL18A_SCENE_CAMERA_CURRENT,
                                  scene + OVL18A_SCENE_CAMERA_TARGET);
        ae_wr32(r, AE_OVL18A_G_SCENE_CONFIG + OVL18A_SCENE_ANIM_COUNTER,
                ae_rd32(r, AE_OVL18A_G_SCENE_CONFIG + OVL18A_SCENE_ANIM_COUNTER) + 1u);
        ae_pad_PadUpdate(r, h != NULL ? h->pad : NULL, 0u);
    }

    ae_wr16(r, AE_OVL18A_G_STAGE_STATE + AE_OVL18A_STAGE_MODE, 13u);
    (void)OVL18A_NATIVE4(r, h, "ClearImage", OVL18A_API_CLEAR_IMAGE,
                         rectangle, 0u, 0u, 0u);
    (void)OVL18A_NATIVE1(r, h, "DrawSync", OVL18A_API_DRAW_SYNC, 0u);
    (void)OVL18A_NATIVE1(r, h, "VSync", OVL18A_API_VSYNC, 0u);
    (void)OVL18A_NATIVE4(r, h, "ClearImage", OVL18A_API_CLEAR_IMAGE,
                         rectangle, 0u, 0u, 0u);
    (void)OVL18A_NATIVE1(r, h, "DrawSync", OVL18A_API_DRAW_SYNC, 0u);
    (void)OVL18A_NATIVE0(r, h, "InitDisplay", OVL18A_API_INIT_DISPLAY);
    {
        uint32_t frame_offset = ovl18a_frame_offset(ae_rd32(r, AE_OVL18A_G_OT_INDEX));
        (void)OVL18A_NATIVE1(r, h, "PutDispEnv",
                             OVL18A_API_PUT_DISPLAY_ENV,
                             AE_OVL18A_G_DISPLAY_ENV + frame_offset);
        (void)OVL18A_NATIVE1(r, h, "PutDrawEnv",
                             OVL18A_API_PUT_DRAW_ENV,
                             AE_OVL18A_G_DRAW_ENV + frame_offset);
    }
    (void)OVL18A_NATIVE1(r, h, "VSync", OVL18A_API_VSYNC, 0u);
    return 0;
}
