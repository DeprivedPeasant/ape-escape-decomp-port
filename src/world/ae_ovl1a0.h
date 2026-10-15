#ifndef AE_OVL1A0_H
#define AE_OVL1A0_H

#include "../bodies/ae_bodies.h"
#include "../sound/ae_bgm.h"
#include "../camera/ae_camera.h"
#include "../camera/ae_camupd.h"
#include "../core/ae_ram.h"
#include "../effects/ae_spawn.h"
#include "../world/ae_stagefade.h"
#include "ae_zako2.h"

/* Area 46's code overlay is loaded at 0x8013D000. */
#define AE_OVL1A0_BASE                         0x8013D000u
#define AE_OVL1A0_ACTOR_RECORDS                0x80141D20u
#define AE_OVL1A0_ACTOR_RECORD_COUNT           8u
#define AE_OVL1A0_ACTOR_RECORD_SIZE            0x94u
#define AE_OVL1A0_BACKDROP                     0x801421C0u
#define AE_OVL1A0_MODEL_RECORDS                0x80142260u
#define AE_OVL1A0_MODEL_RECORD_COUNT           5u
#define AE_OVL1A0_MODEL_RECORD_SIZE            0x94u
#define AE_OVL1A0_SHOCKWAVE_RECORDS            0x801418A0u
#define AE_OVL1A0_SHOCKWAVE_COUNT              8u
#define AE_OVL1A0_SHOCKWAVE_SIZE               0x90u
#define AE_OVL1A0_DEBRIS_RECORDS               0x80142368u
#define AE_OVL1A0_DEBRIS_COUNT                 80u
#define AE_OVL1A0_DEBRIS_SIZE                  0x0Cu
#define AE_OVL1A0_BOSS_STATE                   0x80141880u
#define AE_OVL1A0_BOSS_LOOP_HANDLE             0x80142728u
#define AE_OVL1A0_PARTICLE_SOURCE              0x80142640u
#define AE_OVL1A0_PARTICLE_SOURCE_ALT          0x80142260u
#define AE_OVL1A0_ACTOR_ARRAY                  0x800EC350u
#define AE_OVL1A0_ACTOR_TABLE_POINTER          0x800F455Cu
#define AE_OVL1A0_ACTOR_ARRAY_POINTERS         0x800F4350u
#define AE_OVL1A0_ACTOR_ARRAY_DATA             0x800F4350u
#define AE_OVL1A0_ACTOR_ARRAY_POINTER_OFFSET   0x20Cu
#define AE_OVL1A0_AREA_MODEL_DATA_POINTER      0x800F4594u
#define AE_OVL1A0_AREA_MODEL_DATA              0x80143000u
#define AE_OVL1A0_MODEL_DATA_LAYOUT_OFFSET     0x08u
#define AE_OVL1A0_MODEL_LAYOUT_STRIDE          0x20u
#define AE_OVL1A0_MODEL_LAYOUT_INSTANCE        0x18u
#define AE_OVL1A0_MODEL_LAYOUT_HEADER          0x1Cu
#define AE_OVL1A0_MODEL_HEADER_COUNT           0x00u
#define AE_OVL1A0_MODEL_HEADER_VERTEX_COUNT    0x08u
#define AE_OVL1A0_MODEL_HEADER_VERTEX_DATA     0x0Cu
#define AE_OVL1A0_MODEL_INSTANCE_PTRS          0x80142240u
#define AE_OVL1A0_MODEL_HEADERS                0x80142250u
#define AE_OVL1A0_MODEL_POINTER_SLOT_SIZE      4u
#define AE_OVL1A0_INIT_GTE_TRANSLATION         0x1F800000u
#define AE_OVL1A0_DEBRIS_GTE_VERTEX_BASE        0x48u
#define AE_OVL1A0_DEBRIS_GTE_VERTEX_COUNT       9u
#define AE_OVL1A0_DEBRIS_GTE_QUAD_0             0x48u
#define AE_OVL1A0_DEBRIS_GTE_QUAD_1             0x68u
#define AE_OVL1A0_DEBRIS_GTE_QUAD_2             0x88u
#define AE_OVL1A0_DEBRIS_GTE_QUAD_3             0xA8u
#define AE_OVL1A0_DEBRIS_GTE_SCREEN_X           0xC8u
#define AE_OVL1A0_DEBRIS_GTE_SCREEN_Y           0xCCu
#define AE_OVL1A0_DEBRIS_GTE_SCREEN_Z           0xD0u
#define AE_OVL1A0_DEBRIS_GTE_DEPTH              0xD4u
#define AE_OVL1A0_INIT_GTE_COLOR_OUTPUT        0x1F8003C0u
#define AE_OVL1A0_INIT_GTE_COLOR_X             0x3C0u
#define AE_OVL1A0_INIT_GTE_COLOR_Y             0x3C2u
#define AE_OVL1A0_INIT_GTE_FLAG                0x3C4u
#define AE_OVL1A0_INIT_GTE_MODE                0x3C6u
#define AE_OVL1A0_INIT_GTE_SCRATCH              0x3F4u
#define AE_OVL1A0_GTE_LIGHT_GLOBALS            0x800FBFA0u
#define AE_OVL1A0_GTE_LIGHT_X                  0x24u
#define AE_OVL1A0_GTE_LIGHT_Y                  0x28u
#define AE_OVL1A0_INIT_LIGHT_HOOK              0x8013D514u
#define AE_OVL1A0_INIT_TRANSFORM_HOOK          0x8013D59Cu
#define AE_OVL1A0_MODEL_INSTANCE_POINTERS      0x80142240u
#define AE_OVL1A0_MODEL_HEADER_POINTERS        0x80142250u
#define AE_OVL1A0_RAND_SEED                    0x800AE8F0u
#define AE_OVL1A0_AREA_STATE                   0x800EC2D2u
#define AE_OVL1A0_PLAYER_STATE                 0x800EC204u
#define AE_OVL1A0_PLAYER_OBJECT                0x800EC1E0u
#define AE_OVL1A0_PLAYER_OBJECT_FLAGS          0x800E6916u
#define AE_OVL1A0_OT_CURSOR                    0x800EC354u
#define AE_OVL1A0_OT_TABLE                     0x800EC350u
#define AE_OVL1A0_OT_SELECTOR                  0x800EC35Cu
#define AE_OVL1A0_OT_DISPLAY_RECORDS            0x800EC3D0u
#define AE_OVL1A0_HOP_RENDER_PHASE             0x80141870u
#define AE_OVL1A0_SCREEN_FLASH_RENDER_HOOK     0x8013F954u
#define AE_OVL1A0_MODEL_LIGHTING_RENDER_HOOK   0x8013D4E0u
#define AE_OVL1A0_DEBRIS_RENDER_HOOK           0x8013DCA0u
#define AE_OVL1A0_STACK_VECTOR                 0x801FF180u
#define AE_OVL1A0_STACK_MATRIX                 0x801FF190u
#define AE_OVL1A0_STACK_PROJECTED_SCREEN       0x801FF1B0u
#define AE_OVL1A0_ENTRY_RENDER_SCALE           0x801FFF90u
#define AE_OVL1A0_ENTRY_VECTOR                 0x801FFFA0u
#define AE_OVL1A0_ENTRY_DIRECTION_VECTOR       0x801FFFA8u
#define AE_OVL1A0_STACK_SCREEN_XY               0x00u
#define AE_OVL1A0_STACK_DEPTH                  0x04u
#define AE_OVL1A0_STACK_FLAGS                  0x08u
#define AE_OVL1A0_ACTOR_TABLE_STRIDE           56u
#define AE_OVL1A0_ACTOR_SOURCE_ID              0x08u
#define AE_OVL1A0_ACTOR_CLEAR_FIELD            0x06u
#define AE_OVL1A0_ACTOR_COPY_FIELD             0x02u
#define AE_OVL1A0_ACTOR_ENCOUNTER_STATE        0x02u
#define AE_OVL1A0_ACTOR_INDEX                  0x04u
#define AE_OVL1A0_ACTOR_TYPE                   0x0Cu
#define AE_OVL1A0_ACTOR_TABLE_FLAGS            0x18u
#define AE_OVL1A0_ACTOR_TABLE_ACTIVE           0x16u
#define AE_OVL1A0_ACTOR_RECORD_INDEX           0x82u
#define AE_OVL1A0_ACTOR_RECORD_TYPE            0x83u
#define AE_OVL1A0_ACTOR_RECORD_STATE            0x84u
#define AE_OVL1A0_ACTOR_RECORD_FLAGS           0x85u
#define AE_OVL1A0_ACTOR_RECORD_PHASE           0x86u
#define AE_OVL1A0_ACTOR_RECORD_BODY_POINTER     0x70u
#define AE_OVL1A0_ACTOR_RECORD_MODEL_HEADER     0x7Cu
#define AE_OVL1A0_ACTOR_RECORD_BODY_STATE       0x5Cu
#define AE_OVL1A0_ACTOR_RECORD_INIT_A           0x7Eu
#define AE_OVL1A0_ACTOR_RECORD_INIT_B           0x7Fu
#define AE_OVL1A0_ACTOR_RECORD_RANDOM_SEED      0x8Fu
#define AE_OVL1A0_ACTOR_RECORD_VALUE_A          0x88u
#define AE_OVL1A0_ACTOR_RECORD_VALUE_B          0x8Au
#define AE_OVL1A0_ACTOR_RECORD_VALUE_C          0x8Cu
#define AE_OVL1A0_ACTOR_RECORD_KIND             0x90u
#define AE_OVL1A0_ACTOR_RECORD_KNOWN_TYPE       0x83u
#define AE_OVL1A0_BOSS_STATUS                  0x00u
#define AE_OVL1A0_BOSS_EVENT_FLAG              0x01u
#define AE_OVL1A0_BOSS_FRAME                   0x10u
#define AE_OVL1A0_BOSS_TIMER                   0x14u
#define AE_OVL1A0_BOSS_LOOP_STATE              0x18u
#define AE_OVL1A0_BOSS_PICKUP_GUARD            0x1Cu
#define AE_OVL1A0_BOSS_TRANSITION              0x1Fu
#define AE_OVL1A0_BOSS_INIT_PROGRESS_FLAG      0x800AF840u
#define AE_OVL1A0_BOSS_PHASE_METRIC            0x0Cu
#define AE_OVL1A0_BOSS_CLEAR_PROGRESS_HALFWORD 0x800E69BEu
#define AE_OVL1A0_BACKDROP_BODY_POINTER         0x70u
#define AE_OVL1A0_BACKDROP_BODY_ACTIVE         0x78u
#define AE_OVL1A0_BACKDROP_BODY_POSITION_X     0x24u
#define AE_OVL1A0_BACKDROP_BODY_POSITION_Y     0x28u
#define AE_OVL1A0_BACKDROP_BODY_POSITION_Z     0x2Cu
#define AE_OVL1A0_BACKDROP_BODY_ANGLE           0x7Au
#define AE_OVL1A0_MODEL_FIELD_BODY_REGISTER     0x24u
#define AE_OVL1A0_MODEL_INIT_STATE             0x84u
#define AE_OVL1A0_MODEL_INIT_FLAGS             0x86u
#define AE_OVL1A0_MODEL_INIT_TYPE              0x87u
#define AE_OVL1A0_MODEL_BODY_RECORD_SIZE       0x94u
#define AE_OVL1A0_ACTOR_RECORD_EXTRA_BYTE_1    0x7Eu
#define AE_OVL1A0_ACTOR_RECORD_EXTRA_BYTE_2    0x7Fu
#define AE_OVL1A0_ACTOR_RECORD_EXTRA_HALF_1    0x88u
#define AE_OVL1A0_ACTOR_RECORD_EXTRA_HALF_2    0x8Au
#define AE_OVL1A0_ACTOR_RECORD_EXTRA_HALF_3    0x8Cu
#define AE_OVL1A0_ACTOR_RECORD_EXTRA_BYTE_3    0x8Eu
#define AE_OVL1A0_ACTOR_RECORD_EXTRA_BYTE_4    0x8Fu
#define AE_OVL1A0_ACTOR_RECORD_ACTIVE_COUNT    0x84u
#define AE_OVL1A0_ACTOR_RECORD_ANIM            0x78u
#define AE_OVL1A0_ACTOR_RECORD_ANIM_ACTIVE     0x7Au
#define AE_OVL1A0_ACTOR_RECORD_ANIM_LOOP       0x7Bu
#define AE_OVL1A0_MODEL_MATRIX_POINTER         0x14u
#define AE_OVL1A0_MODEL_NEXT_HEADER            0x18u
#define AE_OVL1A0_MODEL_LIGHT_DIRECTION        0x60u
#define AE_OVL1A0_MODEL_INSTANCE_STRIDE        0x4078u
#define AE_OVL1A0_EFFECT_BODY_POINTER          0x00u
#define AE_OVL1A0_EFFECT_POS_X                 0x04u
#define AE_OVL1A0_EFFECT_POS_Y                 0x06u
#define AE_OVL1A0_EFFECT_POS_Z                 0x08u
#define AE_OVL1A0_EFFECT_BODY_PARAM            0x20u
#define AE_OVL1A0_EFFECT_BODY_FLAGS            0x24u
#define AE_OVL1A0_EFFECT_BODY_ANIM             0x26u
#define AE_OVL1A0_BODY_POSITION_X              0x00u
#define AE_OVL1A0_BODY_POSITION_Y              0x04u
#define AE_OVL1A0_BODY_POSITION_Z              0x08u
#define AE_OVL1A0_BODY_PARAM                   0x20u
#define AE_OVL1A0_BODY_FLAGS                   0x24u
#define AE_OVL1A0_BODY_ANIM                    0x26u
#define AE_OVL1A0_PLAYER_X                     0x24u
#define AE_OVL1A0_PLAYER_Z                     0x2Cu
#define AE_OVL1A0_PICKUP_DIFFICULTY            0x800EC2C8u
#define AE_OVL1A0_AREA_ACTOR_MATCH_RECORD      0x800F4350u
#define AE_OVL1A0_ACTOR_MATCH_FIELD            0x154u
#define AE_OVL1A0_ACTOR_FLAG_TABLE_POINTER     0x800B4D64u
#define AE_OVL1A0_ACTOR_FLAG_ARRAY             AE_OVL1A0_ACTOR_FLAG_TABLE_POINTER
#define AE_OVL1A0_ACTOR_FLAG_FIELD             0x16u
#define AE_OVL1A0_DROP_MATCH_VALUE             0x800F51C1u
#define AE_OVL1A0_DROP_ACTOR_MATCH_OFFSET      340u
#define AE_OVL1A0_DEBRIS_VELOCITY_X            0x08u
#define AE_OVL1A0_DEBRIS_VELOCITY_Y            0x06u
#define AE_OVL1A0_DEBRIS_VELOCITY_Z            0x09u
#define AE_OVL1A0_DEBRIS_LIFETIME              0x0Au

/* Actor entry layout, relative to each 0x94-byte record. */
#define AE_OVL1A0_ACTOR_POS_X                  0x24u
#define AE_OVL1A0_ACTOR_POS_Y                  0x28u
#define AE_OVL1A0_ACTOR_POS_Z                  0x2Cu
#define AE_OVL1A0_ACTOR_ENTRY_PTR              0x1Cu
#define AE_OVL1A0_ACTOR_FRAME                  0x5Cu
#define AE_OVL1A0_ACTOR_STATE                  AE_OVL1A0_ACTOR_RECORD_STATE
#define AE_OVL1A0_ACTOR_TIMER                  AE_OVL1A0_ACTOR_RECORD_INDEX
#define AE_OVL1A0_ACTOR_FLAGS                  0x85u
#define AE_OVL1A0_ACTOR_PHASE                  0x86u
#define AE_OVL1A0_ACTOR_ANIM                   AE_OVL1A0_ACTOR_RECORD_ANIM
#define AE_OVL1A0_ACTOR_ANIM_ACTIVE            AE_OVL1A0_ACTOR_RECORD_ANIM_ACTIVE
#define AE_OVL1A0_ACTOR_ANIM_LOOP              AE_OVL1A0_ACTOR_RECORD_ANIM_LOOP
#define AE_OVL1A0_ACTOR_RENDER_PARAMS          0x74u
#define AE_OVL1A0_ACTOR_RENDER_X               0x68u
#define AE_OVL1A0_ACTOR_RENDER_Y               0x6Au
#define AE_OVL1A0_ACTOR_RENDER_Z               0x6Cu
#define AE_OVL1A0_ACTOR_COOLDOWN               0x6Au
#define AE_OVL1A0_ACTOR_HOP_TIMER               0x90u
#define AE_OVL1A0_ACTOR_HOP_PHASE               0x91u
#define AE_OVL1A0_ACTOR_ENTRY_ACTIVE            0x84u
#define AE_OVL1A0_ACTOR_ENTRY_BODY_OBJECT       0x70u
#define AE_OVL1A0_ACTOR_ENTRY_COUNTDOWN          0x82u
#define AE_OVL1A0_ACTOR_ENTRY_STATE              0x83u
#define AE_OVL1A0_ACTOR_ENTRY_TICK               0x85u
#define AE_OVL1A0_ACTOR_ENTRY_TURN               0x86u
#define AE_OVL1A0_ACTOR_ENTRY_SUBSTATE           0x87u
#define AE_OVL1A0_ACTOR_ENTRY_ANGLE              0x88u
#define AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X         0x88u
#define AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Y         0x8Au
#define AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Z         0x8Cu
#define AE_OVL1A0_ACTOR_ENTRY_COOLDOWN           0x90u
#define AE_OVL1A0_ACTOR_ENTRY_TIMER              0x9Au
#define AE_OVL1A0_ACTOR_ENTRY_KIND               0xA3u
#define AE_OVL1A0_ACTOR_ENTRY_EFFECT_TIMER       0x8Eu
#define AE_OVL1A0_ACTOR_ENTRY_EFFECT_VARIANT     0x8Du
#define AE_OVL1A0_ACTOR_ENTRY_SPIN_TIMER         0x7Au
#define AE_OVL1A0_ACTOR_ENTRY_EFFECT_DELAY       0x8Fu
#define AE_OVL1A0_ACTOR_ENTRY_STATE_TICK         0x7Bu
#define AE_OVL1A0_ACTOR_ENTRY_TARGET_X           0x14u
#define AE_OVL1A0_ACTOR_ENTRY_TARGET_Y           0x16u
#define AE_OVL1A0_ACTOR_ENTRY_TARGET_Z           0x18u
#define AE_OVL1A0_ACTOR_ENTRY_RENDER_ANGLE        0x76u
#define AE_OVL1A0_ACTOR_ENTRY_RENDER_FRAME        0x7Cu
#define AE_OVL1A0_ACTOR_ENTRY_RENDER_FLAGS        0x80u
#define AE_OVL1A0_ACTOR_ENTRY_RENDERED_X          0x74u
#define AE_OVL1A0_ACTOR_ENTRY_RENDERED_Y          0x76u
#define AE_OVL1A0_ACTOR_ENTRY_RENDERED_Z          0x78u
#define AE_OVL1A0_ACTOR_ENTRY_CONTACT             0x50u
#define AE_OVL1A0_ACTOR_ENTRY_COLLISION_RADIUS    0x4Au
#define AE_OVL1A0_ACTOR_ENTRY_COLLISION_WIDTH     0x44u
#define AE_OVL1A0_ACTOR_ENTRY_COLLISION_ACTIVE    0x48u
#define AE_OVL1A0_ACTOR_ENTRY_COLLISION_HEIGHT    0x46u
#define AE_OVL1A0_ACTOR_ENTRY_SPIN_RENDER_MODE    0x00141E80u
#define AE_OVL1A0_ACTOR_ENTRY_ACTIVE_MOVING       2u
#define AE_OVL1A0_ACTOR_ENTRY_STATIONARY          0u
#define AE_OVL1A0_ACTOR_ENTRY_APPROACH             1u
#define AE_OVL1A0_ACTOR_ENTRY_JUMP                 2u
#define AE_OVL1A0_ACTOR_ENTRY_RETREAT              3u
#define AE_OVL1A0_ACTOR_ENTRY_SPIN                 4u

/* Backdrop state fields. */
#define AE_OVL1A0_BACKDROP_ACTIVE              0x78u
#define AE_OVL1A0_BACKDROP_FRAME               0x5Cu
#define AE_OVL1A0_BACKDROP_ANIM_TABLE          0x70u
#define AE_OVL1A0_BACKDROP_ROTATION            0x60u
#define AE_OVL1A0_BACKDROP_POSITION_X          0x24u
#define AE_OVL1A0_BACKDROP_POSITION_Y          0x28u
#define AE_OVL1A0_BACKDROP_POSITION_Z          0x2Cu
#define AE_OVL1A0_BACKDROP_FRAME_STEP           0x7Au
#define AE_OVL1A0_BACKDROP_FADE                 0x7Cu
#define AE_OVL1A0_BACKDROP_LIGHT_ROTATION        0x60u
#define AE_OVL1A0_BACKDROP_ANIM_TIMER            0x5Cu
#define AE_OVL1A0_BACKDROP_ANIM_DESCRIPTOR       0x70u
#define AE_OVL1A0_BACKDROP_MODEL_MATRIX           0x14u
#define AE_OVL1A0_BACKDROP_ANGLE_STEP             0x7Au
#define AE_OVL1A0_BACKDROP_ALPHA                  0x28u
#define AE_OVL1A0_BACKDROP_EVENT_TIMER            0x78u

/* Shockwave fields. */
#define AE_OVL1A0_SHOCK_ACTIVE                 0x84u
#define AE_OVL1A0_SHOCK_POS_X                  0x00u
#define AE_OVL1A0_SHOCK_POS_Y                  0x02u
#define AE_OVL1A0_SHOCK_POS_Z                  0x04u
#define AE_OVL1A0_SHOCK_VEL_X                  0x08u
#define AE_OVL1A0_SHOCK_VEL_Y                  0x0Au
#define AE_OVL1A0_SHOCK_VEL_Z                  0x0Cu
#define AE_OVL1A0_SHOCK_RADIUS                 0x2Eu
#define AE_OVL1A0_SHOCK_HEIGHT                 0x30u
#define AE_OVL1A0_SHOCK_TIMER                  0x88u
#define AE_OVL1A0_SHOCK_VELOCITY_X             0x88u
#define AE_OVL1A0_SHOCK_VELOCITY_Y             0x8Au
#define AE_OVL1A0_SHOCK_VELOCITY_Z             0x8Cu
#define AE_OVL1A0_SHOCK_STATE                  0x88u
#define AE_OVL1A0_SHOCK_DELAY                  0x90u
#define AE_OVL1A0_SHOCK_RADIUS_STEP             0x92u
#define AE_OVL1A0_SHOCK_SPREAD                  0x94u
#define AE_OVL1A0_SHOCK_AGE                    0x96u
#define AE_OVL1A0_SHOCK_TARGET_X                0x18u
#define AE_OVL1A0_SHOCK_TARGET_Y                0x1Au
#define AE_OVL1A0_SHOCK_TARGET_Z                0x1Cu
#define AE_OVL1A0_SHOCK_ROTATION                0x60u

/* Debris fields. */
#define AE_OVL1A0_DEBRIS_POS_X                 0x00u
#define AE_OVL1A0_DEBRIS_POS_Y                 0x02u
#define AE_OVL1A0_DEBRIS_POS_Z                 0x04u
#define AE_OVL1A0_DEBRIS_VEL_X                 0x08u
#define AE_OVL1A0_DEBRIS_VEL_Y                 0x06u
#define AE_OVL1A0_DEBRIS_VEL_Z                 0x09u

#define AE_OVL1A0_CAMERA_MATRIX                0x800E78C0u
#define AE_OVL1A0_PLAYER_VELOCITY_X             0x30u
#define AE_OVL1A0_PLAYER_VELOCITY_Z             0x38u
#define AE_OVL1A0_BOSS_ENTRY_TABLE             0x80141370u
#define AE_OVL1A0_BACKDROP_MATRIX_TEMPLATE    0x80141384u
#define AE_OVL1A0_SHOCKWAVE_MATRIX_TEMPLATE   0x80141394u
#define AE_OVL1A0_GTE_SCRATCH_BASE              0x1F800000u
#define AE_OVL1A0_MEMSET_HOOK                  0x800A3740u
#define AE_OVL1A0_MEMSET_NAME                  "memset"
#define AE_OVL1A0_EXE_BGM_SET_VOLUME_MODE      0x800863B0u
#define AE_OVL1A0_EXE_AREA_FEATURE_30_REQUEST  0x8009B7B0u
#define AE_OVL1A0_EXE_MESH_INIT_FROM_AREA      0x80015CC8u
#define AE_OVL1A0_EXE_GTE_SET_ROT_MATRIX       0x8002FB00u
#define AE_OVL1A0_EXE_GTE_SET_TRANS_MATRIX     0x8002FB30u
#define AE_OVL1A0_EXE_ROT_TRANS_PERS           0x80031330u
#define AE_OVL1A0_EXE_MODEL_TRANSFORM_SKINNED  0x80016A60u
#define AE_OVL1A0_MODEL_LIGHTING_DATA          0x800E7D00u
#define AE_OVL1A0_EFFECT_GROUP_ASSET           0x80141640u
#define AE_OVL1A0_BOSS_MODEL_HEADER             0x74u
#define AE_OVL1A0_BACKDROP_ROT_COS             0x60u
#define AE_OVL1A0_BACKDROP_ROT_SIN             0x64u
#define AE_OVL1A0_BACKDROP_ROT_ZERO             0x62u

/* Common component offsets for the overlay's packed vectors and matrices. */
#define AE_OVL1A0_VECTOR16_X                   0x00u
#define AE_OVL1A0_VECTOR16_Y                   0x02u
#define AE_OVL1A0_VECTOR16_Z                   0x04u
#define AE_OVL1A0_VECTOR32_X                   0x00u
#define AE_OVL1A0_VECTOR32_Y                   0x04u
#define AE_OVL1A0_VECTOR32_Z                   0x08u
#define AE_OVL1A0_MATRIX_TRANSLATION_X         0x14u
#define AE_OVL1A0_MATRIX_TRANSLATION_Y         0x18u
#define AE_OVL1A0_MATRIX_TRANSLATION_Z         0x1Cu
#define AE_OVL1A0_MODEL_HEADER_FIRST_POINTER   0x04u
#define AE_OVL1A0_ANIMATION_CLIP_END            0x02u

/* Actor model and body fields used by boss initialization. */
#define AE_OVL1A0_ACTOR_MESH_ENTER              0x0Eu
#define AE_OVL1A0_ACTOR_MESH_EXIT               0x10u
#define AE_OVL1A0_MODEL_BODY_OUTPUT_POINTER     0x74u
#define AE_OVL1A0_MODEL_ROTATION_X              0x60u
#define AE_OVL1A0_MODEL_ROTATION_Y              0x62u
#define AE_OVL1A0_MODEL_ROTATION_Z              0x64u
#define AE_OVL1A0_MODEL_ANIMATION_POINTER       0x7Cu
#define AE_OVL1A0_MODEL_BODY_STATE_A            0x90u
#define AE_OVL1A0_MODEL_BODY_STATE_B            0x92u
#define AE_OVL1A0_MODEL_BODY_STATE_C            0x93u
#define AE_OVL1A0_MODEL_COLLISION_RADIUS        0x4Au
#define AE_OVL1A0_MODEL_COLLISION_WIDTH         0x44u
#define AE_OVL1A0_MODEL_COLLISION_HEIGHT        0x46u
#define AE_OVL1A0_MODEL_COLLISION_ACTIVE        0x48u
#define AE_OVL1A0_REGISTERED_BODY_OFFSET        0xBCu
#define AE_OVL1A0_REGISTERED_OWNER_BACK_OFFSET  36u
#define AE_OVL1A0_BOSS_PICKUP_GUARD_EXTRA       0x1Du

/* Stack vectors, projected screen result and boss camera scratch fields. */
#define AE_OVL1A0_STACK_PARTICLE_POSITION       (AE_OVL1A0_STACK_VECTOR + 0x20u)
#define AE_OVL1A0_STACK_EFFECT_POINT            (AE_OVL1A0_STACK_VECTOR + 0x28u)
#define AE_OVL1A0_GTE_CAMERA_COPY               (AE_OVL1A0_GTE_SCRATCH_BASE + 0x20u)
#define AE_OVL1A0_GTE_DEBRIS_ANGLES             (AE_OVL1A0_GTE_SCRATCH_BASE + 0x40u)
#define AE_OVL1A0_SCREEN_RESULT_Y               0x02u
#define AE_OVL1A0_GTE_CAMERA_CENTER_X           0x00u
#define AE_OVL1A0_GTE_CAMERA_TARGET_Y           0x02u
#define AE_OVL1A0_GTE_CAMERA_TARGET_Z           0x04u
#define AE_OVL1A0_GTE_CAMERA_HALF_X             0x08u
#define AE_OVL1A0_GTE_CAMERA_EYE_Y              0x0Au
#define AE_OVL1A0_GTE_CAMERA_FAR_Z              0x0Cu
#define AE_OVL1A0_GTE_CAMERA_PLAYER_X           0x10u
#define AE_OVL1A0_GTE_CAMERA_PLAYER_Y           0x12u
#define AE_OVL1A0_GTE_CAMERA_PLAYER_Z           0x14u

/* Shockwave world, render, velocity and model fields. */
#define AE_OVL1A0_SHOCK_BODY_POS_X              0x24u
#define AE_OVL1A0_SHOCK_BODY_POS_Y              0x28u
#define AE_OVL1A0_SHOCK_BODY_POS_Z              0x2Cu
#define AE_OVL1A0_SHOCK_RENDERED_X              0x74u
#define AE_OVL1A0_SHOCK_RENDERED_Y              0x76u
#define AE_OVL1A0_SHOCK_RENDERED_Z              0x78u
#define AE_OVL1A0_SHOCK_BODY_OBJECT             0x70u
#define AE_OVL1A0_SHOCK_DIRECTION               0x60u
#define AE_OVL1A0_BOSS_STATE_PHASE               0x01u
#define AE_OVL1A0_ACTOR_ENTRY_NORMAL_VECTOR     0x60u
#define AE_OVL1A0_PLAYER_KNOCKBACK_TICKS        0x02u
#define AE_OVL1A0_PLAYER_KNOCKBACK_VELOCITY_X   0x04u
#define AE_OVL1A0_PLAYER_KNOCKBACK_VELOCITY_Z   0x08u
#define AE_OVL1A0_HOP_RENDER_MATRIX             (AE_OVL1A0_BOSS_STATE - 0x20u)
#define AE_OVL1A0_KNOCKBACK_EFFECT_POSITION     0x800B4D88u
#define AE_OVL1A0_SHOCK_RENDER_MODEL            0x70u
#define AE_OVL1A0_SHOCK_DIRECTION_OUTPUT        0x88u
#define AE_OVL1A0_SHOCK_ANIMATION_STATE         0x5Cu

typedef int32_t (*AeOvl1A0CallFn)(void *user, AeRam *r, const char *name,
                                  uint32_t address, uint32_t count,
                                  const uint32_t *args);
typedef int32_t (*AeOvl1A0RenderFn)(void *user, AeRam *r, const char *name,
                                    uint32_t address, uint32_t count,
                                    const uint32_t *args);

typedef struct AeOvl1A0Hooks {
    void *user;
    const AeBodyHooks *body_hooks;
    const AeSpawnHooks *spawn_hooks;
    const AeCameraHooks *camera_hooks;
    const AeCamupdHooks *camupd_hooks;
    const AeZako2Hooks *zako2_hooks;
    const AeSoundHw *sound_hw;
    const AeStageFadeHooks *stagefade_hooks;
    AeOvl1A0CallFn original_call;
    AeOvl1A0RenderFn renderer;
} AeOvl1A0Hooks;

/* Overlay functions. Functions backed entirely by native rendering are exposed
 * as named hooks and intentionally have no verified.csv row. */
int32_t ae_Area46_Boss_RegisterEntry(AeRam *r, uint32_t actor);          /* 8013D000 */
void ae_Area46_Boss_UpdateEncounterActor(AeRam *r,
                                          const AeOvl1A0Hooks *h,
                                          uint32_t actor);                /* 8013D06C */
int32_t ae_Area46_Boss_UpdateEntryProximity(AeRam *r,
                                             const AeOvl1A0Hooks *h,
                                             uint32_t actor);             /* 8013D2A0 */
void ae_Area46_Boss_InitializeModelEntry(AeRam *r, const AeOvl1A0Hooks *h,
                                          uint32_t index, uint32_t model,
                                          uint32_t output, uint32_t mode); /* 8013D440 */
int32_t ae_Area46_Boss_ProjectAndEmit(AeRam *r, const AeOvl1A0Hooks *h,
                                      uint32_t model, uint32_t matrix,
                                      uint32_t mode);                     /* 8013D63C */
void ae_Area46_Boss_SpawnEffectNear(AeRam *r, const AeOvl1A0Hooks *h,
                                    uint32_t effect, uint32_t owner,
                                    int32_t body_param, int32_t body_flags,
                                    int32_t body_anim);                    /* 8013D77C */
void ae_Area46_Boss_SpawnEffectNearWrap(AeRam *r, const AeOvl1A0Hooks *h,
                                        uint32_t effect, uint32_t owner,
                                        int32_t body_param,
                                        int32_t body_anim);                /* 8013D890 */
void ae_Area46_Boss_ClearDebris(AeRam *r);                               /* 8013D8BC */
void ae_Area46_Boss_SpawnDebris(AeRam *r, uint32_t source);             /* 8013D8E4 */
void ae_Area46_Boss_UpdateDebris(AeRam *r, const AeOvl1A0Hooks *h);     /* 8013DA4C */
void ae_Area46_Boss_RumbleLong(AeRam *r, const AeOvl1A0Hooks *h);       /* 8013DE1C */
void ae_Area46_Boss_RumbleShort(AeRam *r, const AeOvl1A0Hooks *h);      /* 8013DE44 */
void ae_Area46_Boss_RumbleMedium(AeRam *r, const AeOvl1A0Hooks *h);     /* 8013DE6C */
void ae_Area46_Boss_SpawnDropPickups(AeRam *r, const AeOvl1A0Hooks *h,
                                     uint32_t position);                 /* 8013DE94 */
void ae_Area46_Boss_ResetLoopHandle(AeRam *r);                          /* 8013DF28 */
int32_t ae_Area46_Boss_StartLoop(AeRam *r, const AeOvl1A0Hooks *h,
                                 int32_t sound_id);                       /* 8013DF38 */
void ae_Area46_Boss_LoopStop(AeRam *r, const AeOvl1A0Hooks *h);            /* 8013DF60 */
void ae_Area46_BossInit(AeRam *r, const AeOvl1A0Hooks *h);                /* 8013DF90 */
void ae_Area46_Boss_InitializeActorRecords(AeRam *r,
                                            const AeOvl1A0Hooks *h,
                                            uint32_t incoming_model_mode); /* 8013E14C */
void ae_Area46_Boss_ClearEntries(AeRam *r);                             /* 8013E1AC */
void ae_Area46_Boss_AddEntry(AeRam *r, uint32_t position,
                             uint32_t type);                            /* 8013E1D0 */
void ae_Area46_Boss_UpdateEntries(AeRam *r, const AeOvl1A0Hooks *h);   /* 8013E2BC */
void ae_Area46_Boss_InitializeBackdrop(AeRam *r,
                                       const AeOvl1A0Hooks *h,
                                       uint32_t incoming_model_mode);    /* 8013EC5C */
void ae_Area46_Boss_SetBackdrop(AeRam *r, int32_t x, int32_t y,
                                int32_t z);                              /* 8013EC94 */
void ae_Area46_Boss_UpdateBackdrop(AeRam *r, const AeOvl1A0Hooks *h);  /* 8013ECD4 */
void ae_Area46_Boss_ResetBoss(AeRam *r);                               /* 8013EE74 */
void ae_Area46_Boss_OnBossStart(AeRam *r, const AeOvl1A0Hooks *h);        /* 8013EEB8 */
void ae_Area46_Boss_ClearFlag(AeRam *r, uint32_t value);               /* 8013EF00 */
void ae_Area46_Boss_SetAnim(AeRam *r, uint32_t model, uint32_t animation); /* 8013EF0C */
void ae_Area46_Boss_StepAnim(AeRam *r, uint32_t model);                /* 8013EF44 */
void ae_Area46_Boss_UpdateHop(AeRam *r, const AeOvl1A0Hooks *h,
                              uint32_t actor);                           /* 8013EFCC */
void ae_Area46_Boss_UpdatePlayerKnockback(AeRam *r,
                                          const AeOvl1A0Hooks *h);       /* 8013F0E0 */
int32_t ae_Area46_BossUpdate(AeRam *r, const AeOvl1A0Hooks *h,
                             uint32_t subject);                          /* 8013F1AC */
void ae_Area46_Boss_ClearShockwaves(AeRam *r,
                                    const AeOvl1A0Hooks *h,
                                    uint32_t incoming_model_mode);        /* 8013F544 */
void ae_Area46_Boss_AddShockwave(AeRam *r, const AeOvl1A0Hooks *h,
                                 uint32_t position);                     /* 8013F5A8 */
void ae_Area46_Boss_UpdateShockwaves(AeRam *r,
                                     const AeOvl1A0Hooks *h);             /* 8013F6F8 */

#endif
