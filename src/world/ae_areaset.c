#include "ae_areaset.h"

#include "../bodies/ae_bodies.h"
#include "../player/ae_gadget.h"

/* Area metadata and persistent state. */
#define AE_AREASET_G_AREA_METADATA          0x800A81B0u
#define AE_AREASET_AREA_METADATA_STRIDE      8u
#define AE_AREASET_AREA_LEVEL_FIELD          4u
#define AE_AREASET_AREA_KIND_FIELD           6u
#define AE_AREASET_G_AREA_STATE              0x800F4350u
#define AE_AREASET_STATE_LEVEL               0x124u
#define AE_AREASET_STATE_CURRENT_AREA        0x126u
#define AE_AREASET_STATE_MONKEY_LOD_MODEL    0x1A4u
#define AE_AREASET_G_AREA_SPECIAL_FLAGS      0x800F5120u
#define AE_AREASET_G_FEATURE_30              0x800F5AA0u
#define AE_AREASET_G_FEATURE_31              0x800F5AACu
#define AE_AREASET_G_FEATURE_32              0x800F5AB0u
#define AE_AREASET_FEATURE_30_POINTER        0x08u
#define AE_AREASET_FEATURE_30_RESET_A        0x0Cu
#define AE_AREASET_FEATURE_30_RESET_B        0x10u
#define AE_AREASET_G_MESSAGE_BOX             0x800F5A00u

/* Model record layouts and shared model instances. */
#define AE_AREASET_MODEL_HEADER_BYTES        0x0Cu
#define AE_AREASET_MODEL_BONE_COUNT          0x04u
#define AE_AREASET_MODEL_VERTEX_COUNT        0x06u
#define AE_AREASET_MODEL_COLOR_COUNT         0x08u
#define AE_AREASET_MODEL_INSTANCE_MODEL      0x00u
#define AE_AREASET_MODEL_INSTANCE_COLORS     0x04u
#define AE_AREASET_MODEL_INSTANCE_PRIMS      0x08u
#define AE_AREASET_MODEL_INSTANCE_BONES      0x0Cu
#define AE_AREASET_MODEL_INSTANCE_VERTICES   0x10u
#define AE_AREASET_MODEL_INSTANCE_MATRICES   0x14u
#define AE_AREASET_MODEL_INSTANCE_ANIM       0x18u
#define AE_AREASET_MODEL_MATRIX_BANK         0x800E9C60u
#define AE_AREASET_INDICATOR_MODEL_A         0x800BBC54u
#define AE_AREASET_INDICATOR_MODEL_B         0x800BBCC4u
#define AE_AREASET_INDICATOR_COUNT           0x800BBD99u
#define AE_AREASET_INDICATOR_ACTIVE          0x800BBC50u
#define AE_AREASET_FEATURE_MODEL_OFFSET      0x04u
#define AE_AREASET_FEATURE_ANIM_OFFSET       0x08u
#define AE_AREASET_FEATURE_ANIM_DATA_BASE    0x04u
#define AE_AREASET_STATIC_FEATURE_MODEL      0x0Cu

/* Area feature state. */
#define AE_AREASET_G_ZONE_BOXES              0x800B4D30u
#define AE_AREASET_ZONE_BOX_COUNT            0x00u
#define AE_AREASET_ZONE_BOX_DATA             0x04u
#define AE_AREASET_SPECIAL_ACTOR             0x800B4DC4u
#define AE_AREASET_SPECIAL_ACTOR_TICK        0x800B4DC0u
#define AE_AREASET_SPECIAL_ACTOR_DRAW        0x800B4DC8u
#define AE_AREASET_SPECIAL_ACTOR_UPDATE      0x800B4DCCu
#define AE_AREASET_SPECIAL_ACTOR_INIT        0x800B4DD0u
#define AE_AREASET_SPECIAL_ACTOR_BYTES       0xACu
#define AE_AREASET_SPECIAL_ACTOR_STATIC      0x90u
#define AE_AREASET_SPECIAL_ACTOR_STATIC_KIND 0x8Eu
#define AE_AREASET_SPECIAL_ACTOR_SCALE_X     0x60u
#define AE_AREASET_SPECIAL_ACTOR_SCALE_Y     0x62u
#define AE_AREASET_SPECIAL_ACTOR_SCALE_Z     0x64u
#define AE_AREASET_SPECIAL_ACTOR_ANGLE_X     0x68u
#define AE_AREASET_SPECIAL_ACTOR_ANGLE_Y     0x6Au
#define AE_AREASET_SPECIAL_ACTOR_ANGLE_Z     0x6Cu
#define AE_AREASET_SPECIAL_ACTOR_ACTIVE      0x8Fu
#define AE_AREASET_SPECIAL_ACTOR_UNIT_SCALE  0x1000u
#define AE_AREASET_SPECIAL_ACTOR_TABLE       0x800A6840u
#define AE_AREASET_SPECIAL_ACTOR_LEVEL_FIRST 4u
#define AE_AREASET_SPECIAL_ACTOR_LEVEL_COUNT 30u
#define AE_AREASET_ACTOR_CASE_LEVEL4         0x8009B4F8u
#define AE_AREASET_ACTOR_CASE_LEVEL12        0x8009B518u
#define AE_AREASET_ACTOR_CASE_LEVEL21        0x8009B494u
#define AE_AREASET_ACTOR_CASE_LEVEL22        0x8009B4B8u
#define AE_AREASET_ACTOR_DRAW_LEVEL21        0x8014085Cu
#define AE_AREASET_ACTOR_INIT_LEVEL21        0x80141128u
#define AE_AREASET_ACTOR_INIT_LEVEL22        0x801431B0u
#define AE_AREASET_ACTOR_DRAW_LEVEL22        0x801428E4u
#define AE_AREASET_ACTOR_UPDATE_LEVEL22      0x80143F38u
#define AE_AREASET_ACTOR_TICK_LEVEL22        0x801433F0u
#define AE_AREASET_ACTOR_UPDATE_LEVEL4       0x801423ACu
#define AE_AREASET_ACTOR_TICK_LEVEL4         0x8014186Cu
#define AE_AREASET_ACTOR_UPDATE_LEVEL12      0x8014163Cu
#define AE_AREASET_ACTOR_TICK_LEVEL12        0x80140AFCu

/* Boss selection is stored as an area byte tag and dispatches through the EXE table. */
#define AE_AREASET_G_BOSS_INIT_FLAG          0x800AF840u
#define AE_AREASET_G_BOSS_DISPATCH           0x800A5360u
#define AE_AREASET_BOSS_CASE_D               0x8005ABD0u
#define AE_AREASET_BOSS_CASE_S               0x8005ABE0u
#define AE_AREASET_BOSS_CASE_V               0x8005ABF0u
#define AE_AREASET_BOSS_CASE_F               0x8005AC00u
#define AE_AREASET_BOSS_CASE_W               0x8005AC10u
#define AE_AREASET_AREA_TAG_D                0x44u
#define AE_AREASET_AREA_TAG_S                0x53u
#define AE_AREASET_AREA_TAG_V                0x56u
#define AE_AREASET_AREA_TAG_F                0x46u
#define AE_AREASET_AREA_TAG_W                0x57u
#define AE_AREASET_AREA_COLLISION_TABLE      0x244u
#define AE_AREASET_BOSS_RECORDS_OFFSET       0x08u
#define AE_AREASET_BOSS_RECORD_STRIDE        0x20u
#define AE_AREASET_BOSS_RECORD_X             0x02u
#define AE_AREASET_BOSS_RECORD_Y             0x04u
#define AE_AREASET_BOSS_RECORD_Z             0x06u
#define AE_AREASET_BOSS_RECORD_KIND          0x00u
#define AE_AREASET_BOSS_BODY                 0x24u
#define AE_AREASET_BOSS_BODY_FIELDS          0x46u
#define AE_AREASET_BOSS_BODY_TYPE            5
#define AE_AREASET_BOSS_MODEL_WORD           0x800E69BEu
#define AE_AREASET_BOSS_OBJECT_POSITION_X    0x24u
#define AE_AREASET_BOSS_OBJECT_POSITION_Y    0x28u
#define AE_AREASET_BOSS_OBJECT_POSITION_Z    0x2Cu
#define AE_AREASET_BOSS_BODY_ROTATION_X      0x1Au
#define AE_AREASET_BOSS_BODY_ROTATION_Y      0x1Cu
#define AE_AREASET_BOSS_BODY_ROTATION_Z      0x1Eu
#define AE_AREASET_BOSS_BODY_COLLISION_X     0x02u
#define AE_AREASET_BOSS_BODY_COLLISION_Y     0x04u
#define AE_AREASET_BOSS44_BODY_STATUS         0x69u
#define AE_AREASET_BOSS44_BODY_TIMER          0x32u
#define AE_AREASET_BOSS44_BODY_FLAGS          0x5Au
#define AE_AREASET_BOSS44_BODY_MODE           0x5Eu
#define AE_AREASET_BOSS44_BODY_COOLDOWN       0x6Cu
#define AE_AREASET_BOSS44_BODY_TARGET         0x76u
#define AE_AREASET_BOSS44_BODY_ACTIVE         0x35u
#define AE_AREASET_BOSS44_BODY_STATE          0x74u
#define AE_AREASET_BOSS44_BODY_ANIM           0x2Bu
#define AE_AREASET_BOSS44_BODY_REGISTERED_A   0x67u
#define AE_AREASET_BOSS44_BODY_REGISTERED_B   0x68u
#define AE_AREASET_BOSS44_BODY_COLLISION_X    0x02u
#define AE_AREASET_BOSS44_BODY_COLLISION_Y    0x04u
#define AE_AREASET_BOSS_BODY_COLLISION_PREV   0x02u
#define AE_AREASET_BOSS_BODY_COLLISION_BASE   0x00u
#define AE_AREASET_BOSS46_BODY_KIND_A         0x41u
#define AE_AREASET_BOSS46_BODY_KIND_B         0x40u
#define AE_AREASET_BOSS46_BODY_ENABLED        0x3Eu
#define AE_AREASET_BOSS46_BODY_REGISTERED_A   0x36u
#define AE_AREASET_BOSS46_BODY_REGISTERED_B   0x4Au
#define AE_AREASET_BOSS46_BODY_REGISTERED_C   0x4Du
#define AE_AREASET_BOSS46_BODY_REGISTERED_D   0x4Cu
#define AE_AREASET_BOSS46_SECONDARY_FLAGS     0x10u
#define AE_AREASET_BOSS46_SECONDARY_ACTIVE    0x1Cu
#define AE_AREASET_BOSS46_SECONDARY_MODE      0x1Du
#define AE_AREASET_BOSS46_SECONDARY_ENABLED   0x1Fu
#define AE_AREASET_BOSS46_SECONDARY_FIELD_A   0x4Au
#define AE_AREASET_BOSS46_SECONDARY_FIELD_B   0x44u
#define AE_AREASET_BOSS46_SECONDARY_FIELD_C   0x48u
#define AE_AREASET_BOSS46_SECONDARY_FIELD_D   0x46u
#define AE_AREASET_BOSS53_BODY_REGISTERED     0x2Eu
#define AE_AREASET_BOSS56_BODY_ACTIVE         0x46u
#define AE_AREASET_BOSS56_BODY_STATUS_A       0x49u
#define AE_AREASET_BOSS56_BODY_STATUS_B       0x48u
#define AE_AREASET_BOSS56_BODY_STATUS_C       0x47u
#define AE_AREASET_BOSS56_BODY_FLAGS          0x42u
#define AE_AREASET_BOSS56_SECONDARY_POSITION  0x74u
#define AE_AREASET_BOSS56_SECONDARY_FIELD_A   0x70u
#define AE_AREASET_BOSS56_SECONDARY_FIELD_B   0x4Au
#define AE_AREASET_BOSS56_SECONDARY_FIELD_C   0x48u
#define AE_AREASET_BOSS56_SECONDARY_FIELD_D   0x44u
#define AE_AREASET_BOSS56_SECONDARY_FIELD_E   0x46u
#define AE_AREASET_BOSS56_SECONDARY_FIELD_F   0x72u
#define AE_AREASET_BOSS57_BODY_KIND           0x43u
#define AE_AREASET_BOSS57_BODY_SIZE           0x44u
#define AE_AREASET_BOSS57_BODY_ENABLED        0x42u
#define AE_AREASET_BOSS57_BODY_KIND_B         0x38u
#define AE_AREASET_BOSS57_BODY_ENABLED_B      0x36u
#define AE_AREASET_BOSS57_BODY_FLAGS          0x40u
#define AE_AREASET_BOSS57_BODY_ANIM            0x42u
#define AE_AREASET_BOSS57_BODY_COLLISION_X    0x04u
#define AE_AREASET_BOSS57_BODY_COLLISION_Y    0xFEu
#define AE_AREASET_BOSS57_BODY_COLLISION_Z    0x02u
#define AE_AREASET_BOSS57_STATE               0x00u
#define AE_AREASET_BOSS57_STATE_CLEAR_BYTE    0x08u
#define AE_AREASET_BOSS57_STATE_ACTIVE        0x03u
#define AE_AREASET_BOSS57_STATE_TIMER         0x0Eu
#define AE_AREASET_BOSS44_EFFECT_ENABLED      0x0Cu
#define AE_AREASET_BOSS_MODEL_DATA            0x70u
#define AE_AREASET_BOSS_MODEL_RENDER_DATA     0x74u
#define AE_AREASET_BOSS_MODEL_CAMERA_DATA     0x84u
#define AE_AREASET_BOSS_SECONDARY_FLAG_NEXT   0x01u
#define AE_AREASET_BOSS53_STATE_READY         0x00u
#define AE_AREASET_BOSS53_STATE_TARGET        0x04u
#define AE_AREASET_BOSS53_STATE_ACTIVE        0x08u
#define AE_AREASET_BOSS53_STATE_COUNTER       0x0Cu
#define AE_AREASET_BOSS53_STATE_MODE          0x10u
#define AE_AREASET_BOSS53_STATE_TIMER_A       0x18u
#define AE_AREASET_BOSS53_STATE_TIMER_B       0x1Au
#define AE_AREASET_BOSS53_STATE_FLAGS         0x3Au
#define AE_AREASET_BOSS53_STATE_KIND          0x3Bu
#define AE_AREASET_BOSS53_STATE_COMPLETE      0x3Cu
#define AE_AREASET_BOSS46_STATE_READY         0x00u
#define AE_AREASET_BOSS46_STATE_FLAGS         0x10u
#define AE_AREASET_BOSS46_STATE_ACTIVE        0x1Cu
#define AE_AREASET_BOSS46_STATE_MODE          0x1Du
#define AE_AREASET_BOSS46_STATE_ENABLED       0x1Fu
#define AE_AREASET_BOSS56_TEMPLATE_POINTER    0x801444E4u
#define AE_AREASET_BOSS56_STATE_POINTER       0x08u
#define AE_AREASET_BOSS56_STATE_TIMER         0x10u
#define AE_AREASET_BOSS56_STATE_MODE          0x1Eu
#define AE_AREASET_BOSS56_STATE_ENABLED       0x14u
#define AE_AREASET_BOSS56_STATE_ACTIVE        0x0Eu
#define AE_AREASET_BOSS56_STATE_FLAG_A        0x0Du
#define AE_AREASET_BOSS56_STATE_FLAG_B        0x0Cu
#define AE_AREASET_BOSS56_STATE_FLAG_C        0x1Cu
#define AE_AREASET_BOSS56_STATE_FLAG_D        0x0Fu
#define AE_AREASET_BOSS56_STATE_FLAG_E        0x1Fu
#define AE_AREASET_BOSS56_STATE_COUNTER       0x24u
#define AE_AREASET_BOSS57_POSITION_X          0x08u
#define AE_AREASET_BOSS57_POSITION_Y          0x0Au
#define AE_AREASET_BOSS57_POSITION_Z          0x0Cu

/* Local overlay routines called by the five boss-init roots remain named hooks. */
#define AE_BOSS44_OBJECT_SETUP                0x8013D1F0u
#define AE_BOSS44_OBJECT_SET_ANIM             0x80140948u
#define AE_BOSS44_INIT_EFFECTS                0x8013F68Cu
#define AE_BOSS44_INIT_PROJECTILES            0x8013F130u
#define AE_BOSS44_INIT_SEGMENTS               0x8013ECF4u
#define AE_BOSS44_INIT_SPAWNS                 0x8013D52Cu
#define AE_BOSS44_INIT_BOSS                   0x8013DB98u
#define AE_BOSS44_INIT_ARENA                  0x8013DC00u
#define AE_BOSS46_OBJECT_SETUP                0x8013D440u
#define AE_BOSS46_OBJECT_SET_ANIM             0x8013EF0Cu
#define AE_BOSS46_CLEAR_FLAG                  0x8013EF00u
#define AE_BOSS46_INIT_EFFECTS                0x8013F544u
#define AE_BOSS46_INIT_PROJECTILES            0x8013E14Cu
#define AE_BOSS46_INIT_ARENA                  0x8013EC5Cu
#define AE_BOSS46_INIT_BOSS                   0x8013D8BCu
#define AE_BOSS46_INIT_ARENA_OBJECTS          0x8013DF28u
#define AE_BOSS53_OBJECT_SETUP                0x8013D10Cu
#define AE_BOSS53_INIT_EFFECTS                0x8013F4F0u
#define AE_BOSS53_INIT_PROJECTILES            0x8013E8C0u
#define AE_BOSS53_INIT_SPAWNS                 0x8013DE14u
#define AE_BOSS53_INIT_ARENA                  0x8013E4D8u
#define AE_BOSS53_INIT_BOSS                   0x8013D804u
#define AE_BOSS56_OBJECT_SETUP                0x8013D000u
#define AE_BOSS56_SET_CAMERA_ANIM             0x801406F8u
#define AE_BOSS56_INIT_EFFECTS                0x8013F86Cu
#define AE_BOSS56_INIT_PROJECTILES            0x8013E4A8u
#define AE_BOSS56_INIT_ARENA                  0x8013D47Cu
#define AE_BOSS56_INIT_BOSS                   0x8013E070u
#define AE_BOSS56_INIT_ROCKS                  0x8013E7F8u
#define AE_BOSS56_INIT_ARENA_OBJECTS          0x8013F080u
#define AE_BOSS57_OBJECT_SETUP                0x8013D000u
#define AE_BOSS57_OBJECT_SET_ANIM             0x8013F3A4u
#define AE_BOSS57_CLEAR_FLAG                  0x8013F398u
#define AE_BOSS57_INIT_EFFECTS                0x8013E020u
#define AE_BOSS57_INIT_PROJECTILES            0x8013D33Cu
#define AE_BOSS57_INIT_ARENA                  0x801407BCu
#define AE_BOSS57_INIT_BOSS                   0x8013D9A8u
#define AE_BOSS57_INIT_ARENA_OBJECTS          0x8013EE9Cu
#define AE_AREASET_BOSS_OBJECTS_44           0x80143D70u
#define AE_AREASET_BOSS_OBJECT_STRIDE_44     0xC4u
#define AE_AREASET_BOSS_EFFECT_44            0x80143EF8u
#define AE_AREASET_BOSS_OBJECTS_46           0x80142260u
#define AE_AREASET_BOSS_SECONDARY_46         0x801422F8u
#define AE_AREASET_BOSS_STATE_46             0x80141880u
#define AE_AREASET_BOSS_OBJECTS_53           0x80140470u
#define AE_AREASET_BOSS_OBJECT_STRIDE_53     0x80u
#define AE_AREASET_BOSS_STATE_53             0x801408C0u
#define AE_AREASET_BOSS_OBJECTS_56           0x80144460u
#define AE_AREASET_BOSS_OBJECT_STRIDE_56     0x98u
#define AE_AREASET_BOSS_SECONDARY_56         0x80144888u
#define AE_AREASET_BOSS_SECONDARY_STRIDE_56  0x78u
#define AE_AREASET_BOSS_SECONDARY_FLAGS_56   0x801446B6u
#define AE_AREASET_BOSS_STATE_56             0x80144BE8u
#define AE_AREASET_BOSS_OBJECT_57            0x80142338u
#define AE_AREASET_BOSS57_POSITION_TABLE     0x80141CC0u
#define AE_AREASET_BOSS_STATE_57             0x80142320u

/* Fixed-size area runtime pools. */
#define AE_AREASET_PROJECTILES               0x800B8D30u
#define AE_AREASET_PROJECTILE_COUNT          16u
#define AE_AREASET_PROJECTILE_STRIDE         0x30u
#define AE_AREASET_PROJECTILE_ACTIVE         0x0Cu
#define AE_AREASET_PARTICLES                 0x800B9900u
#define AE_AREASET_PARTICLE_COUNT            100u
#define AE_AREASET_PARTICLE_STRIDE           0x34u
#define AE_AREASET_PARTICLE_ACTIVE           0x00u
#define AE_AREASET_EFFECT_COUNT              0x800BAD50u
#define AE_AREASET_BLINK_FLAGS               0x800BAD78u
#define AE_AREASET_BLINK_FLAG_A              0x00u
#define AE_AREASET_BLINK_FLAG_B              0x01u
#define AE_AREASET_BLINK_FLAG_C              0x03u
#define AE_AREASET_BLINK_TIMER               0x06u
#define AE_AREASET_BLINK_WORD                0x800BAD70u
#define AE_AREASET_SPARKS                    0x800BAEA0u
#define AE_AREASET_SPARK_COUNT               8u
#define AE_AREASET_SPARK_STRIDE              0x0Cu
#define AE_AREASET_SPARK_ACTIVE              0x06u
#define AE_AREASET_TRAILS                    0x800F5010u
#define AE_AREASET_TRAIL_COUNT               8u
#define AE_AREASET_TRAIL_STRIDE              0x10u
#define AE_AREASET_TRAIL_ACTIVE              0x0Au
#define AE_AREASET_TRAIL_GLOBAL              0x800B1B74u
#define AE_AREASET_EFFECT_OBJECTS            0x800BAF00u
#define AE_AREASET_EFFECT_OBJECT_COUNT       16u
#define AE_AREASET_EFFECT_OBJECT_STRIDE      0x54u
#define AE_AREASET_EFFECT_OBJECT_ACTIVE      0x4Au
#define AE_AREASET_RANDOM_PARTICLES          0x800BB840u
#define AE_AREASET_RANDOM_PARTICLE_COUNT     8u
#define AE_AREASET_RANDOM_PARTICLE_STRIDE    0x1Cu
#define AE_AREASET_RANDOM_PARTICLE_ACTIVE    0x18u
#define AE_AREASET_RANDOM_PARTICLE_GLOBAL    0x800B205Cu
#define AE_AREASET_FX_TABLE_12               0x800BB920u
#define AE_AREASET_FX_TABLE_12_COUNT         12u
#define AE_AREASET_FX_TABLE_12_STRIDE        0x44u
#define AE_AREASET_FX_TABLE_12_ACTIVE        0x3Cu
#define AE_AREASET_FX_SLOTS                  0x800F5380u
#define AE_AREASET_FX_SLOT_COUNT             32u
#define AE_AREASET_FX_SLOT_STRIDE            0x34u
#define AE_AREASET_FX_SLOT_ACTIVE            0x00u
#define AE_AREASET_PROJECTILE_GROUPS         0x800BF300u
#define AE_AREASET_PROJECTILE_GROUP_COUNT    4u
#define AE_AREASET_PROJECTILE_GROUP_STRIDE   0x1Cu
#define AE_AREASET_PROJECTILE_GROUP_ACTIVE   0x12u
#define AE_AREASET_TARGET_LIST               0x800F51F0u
#define AE_AREASET_TARGET_COUNT              32u
#define AE_AREASET_TARGET_STRIDE             4u
#define AE_AREASET_RENDER_TABLE              0x800BEF60u
#define AE_AREASET_RENDER_TABLE_FLAG_A       0x00u
#define AE_AREASET_RENDER_TABLE_FLAG_B       0x02u
#define AE_AREASET_RENDER_TABLE_MODE         0x04u

#define AE_AREASET_MONKEY_LOD_INSTANCE       0x800F50B0u
#define AE_AREASET_MONKEY_LOD_ANIM_POINTER   0x800F50C8u
#define AE_AREASET_MONKEY_LOD_TABLE          0x80103000u
#define AE_AREASET_MONKEY_LOD_ANIM_INDEX     0x801030B8u
#define AE_AREASET_MONKEY_LOD_MODEL_INDEX    0x801030B4u
#define AE_AREASET_G_MONKEY_MODEL_POINTER    0x800B4DA0u

/* Heap and bump allocator. */
#define AE_AREASET_HEAP_BLOCKS               0x800BF690u
#define AE_AREASET_HEAP_BLOCK_COUNT          64u
#define AE_AREASET_HEAP_COUNT                0x800BF790u
#define AE_AREASET_HEAP_ALLOC_BASE           0x800BF798u
#define AE_AREASET_HEAP_CURSOR                0x800DF798u
#define AE_AREASET_HEAP_BLOCK_BYTES          0x800u
#define AE_AREASET_HEAP_LIMIT                0x80102800u
#define AE_AREASET_HEAP_LIMIT_LAST           0x801027FFu
#define AE_AREASET_HEAP_ERROR_TITLE          0x800A6340u
#define AE_AREASET_HEAP_ERROR_DETAIL         0x800A6350u
#define AE_AREASET_HEAP_ERROR_EXIT_STATUS    1u

/* RC-car registration field; CollisionBodyUnregister is already ported. */
#define AE_AREASET_RC_CAR                    0x800F5A20u
#define AE_AREASET_RC_CAR_BODY               0x24u
#define AE_AREASET_RC_CAR_ACTIVE             0x70u

#define AREA_HOOK(field, a0, a1, a2, a3) \
    areaset_call(r, h, h ? h->field : NULL, #field, (a0), (a1), (a2), (a3))

static int32_t areaset_call(AeRam *r, const AeAreaSetHooks *h, AeAreaSetHookFn fn,
                            const char *name, uint32_t a0, uint32_t a1,
                            uint32_t a2, uint32_t a3) {
    if (!fn) {
        ae_ram_fail(r, "missing area-setup hook: %s", name);
        return 0;
    }
    return fn(h->user, r, a0, a1, a2, a3);
}

static uint32_t areaset_sra32(uint32_t value, uint32_t amount) {
    uint32_t shifted = value >> amount;
    if ((value & 0x80000000u) != 0) shifted |= 0xFFFFFFFFu << (32u - amount);
    return shifted;
}

static uint32_t areaset_align_word(uint32_t byte_count) {
    uint32_t rounded = byte_count + 3u;
    return areaset_sra32(rounded, 2u) << 2;
}

static void areaset_clear_byte_records(AeRam *r, uint32_t base, uint32_t count,
                                       uint32_t stride, uint32_t active_field) {
    for (uint32_t index = 0; index < count; ++index) {
        ae_wr8(r, base + index * stride + active_field, 0u);
    }
}

int32_t ae_AreaGetKind(AeRam *r, const AeAreaSetHooks *h, int32_t area_index) {
    (void)h;
    uint32_t record = AE_AREASET_G_AREA_METADATA + (uint32_t)area_index *
                      AE_AREASET_AREA_METADATA_STRIDE;
    return ae_rds16(r, record + AE_AREASET_AREA_KIND_FIELD);
}

int32_t ae_AreaGetLevel(AeRam *r, const AeAreaSetHooks *h, int32_t area_index) {
    (void)h;
    uint32_t record = AE_AREASET_G_AREA_METADATA + (uint32_t)area_index *
                      AE_AREASET_AREA_METADATA_STRIDE;
    return ae_rds16(r, record + AE_AREASET_AREA_LEVEL_FIELD);
}

void ae_ModelInit(AeRam *r, const AeAreaSetHooks *h, uint32_t instance,
                  uint32_t model_header) {
    (void)h;
    uint32_t color_count = ae_rd16(r, model_header + AE_AREASET_MODEL_COLOR_COUNT);
    uint32_t bone_count = ae_rd16(r, model_header + AE_AREASET_MODEL_BONE_COUNT);
    uint32_t vertex_count = ae_rd16(r, model_header + AE_AREASET_MODEL_VERTEX_COUNT);
    uint32_t colors = model_header + AE_AREASET_MODEL_HEADER_BYTES;
    uint32_t bones = colors + color_count * 4u;
    uint32_t vertices = bones + bone_count * 12u;
    uint32_t primitives = vertices + vertex_count * 8u;

    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_MODEL, model_header);
    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_COLORS, colors);
    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_PRIMS, primitives);
    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_BONES, bones);
    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_VERTICES, vertices);
    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_MATRICES,
            AE_AREASET_MODEL_MATRIX_BANK);
}

void ae_StaticModelInit(AeRam *r, const AeAreaSetHooks *h, uint32_t instance,
                        uint32_t model_header) {
    (void)h;
    uint32_t vertex_count = ae_rd16(r, model_header + AE_AREASET_MODEL_VERTEX_COUNT);
    uint32_t color_count = ae_rd16(r, model_header + AE_AREASET_MODEL_COLOR_COUNT);
    uint32_t data = model_header + AE_AREASET_MODEL_HEADER_BYTES;
    uint32_t colors = data;
    uint32_t vertices = data + color_count * 4u;
    uint32_t primitives = data + vertex_count * 8u + color_count * 4u;

    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_MODEL, model_header);
    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_COLORS, colors);
    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_PRIMS, primitives);
    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_BONES, vertices);
}

void ae_AreaSetZoneBoxes(AeRam *r, const AeAreaSetHooks *h, uint32_t feature) {
    (void)h;
    if (feature == 0) {
        ae_wr32(r, AE_AREASET_G_ZONE_BOXES + AE_AREASET_ZONE_BOX_COUNT, 0u);
        return;
    }
    ae_wr32(r, AE_AREASET_G_ZONE_BOXES + AE_AREASET_ZONE_BOX_COUNT,
            ae_rd32(r, feature));
    ae_wr32(r, AE_AREASET_G_ZONE_BOXES + AE_AREASET_ZONE_BOX_DATA,
            feature + AE_AREASET_ZONE_BOX_DATA);
}

void ae_ProjectilesReset(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    areaset_clear_byte_records(r, AE_AREASET_PROJECTILES, AE_AREASET_PROJECTILE_COUNT,
                               AE_AREASET_PROJECTILE_STRIDE,
                               AE_AREASET_PROJECTILE_ACTIVE);
}

int32_t ae_BossKindForArea(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    uint32_t area_tag = ae_rd16(r, AE_AREASET_G_AREA_STATE +
                                AE_AREASET_STATE_CURRENT_AREA);
    switch (area_tag) {
    case AE_AREASET_AREA_TAG_D: return 1;
    case AE_AREASET_AREA_TAG_S: return 2;
    case AE_AREASET_AREA_TAG_V: return 3;
    case AE_AREASET_AREA_TAG_F: return 4;
    case AE_AREASET_AREA_TAG_W: return 5;
    default: return 0;
    }
}

void ae_BossInit(AeRam *r, const AeAreaSetHooks *h) {
    ae_wr8(r, AE_AREASET_G_BOSS_INIT_FLAG, 0u);
    int32_t kind = ae_BossKindForArea(r, h);
    if (kind < 1 || kind > 5) return;

    uint32_t dispatch = ae_rd32(r, AE_AREASET_G_BOSS_DISPATCH +
                                (uint32_t)(kind - 1) * 4u);
    switch (dispatch) {
    case AE_AREASET_BOSS_CASE_D:
        ae_FUN_8013dc68(r, h);
        break;
    case AE_AREASET_BOSS_CASE_S:
        ae_FUN_8013d1ac(r, h);
        break;
    case AE_AREASET_BOSS_CASE_V:
        ae_FUN_8013dae8(r, h);
        break;
    case AE_AREASET_BOSS_CASE_F:
        ae_FUN_8013df90(r, h);
        break;
    case AE_AREASET_BOSS_CASE_W:
        ae_FUN_8013da10(r, h);
        break;
    default:
        ae_ram_fail(r, "unexpected BossInit dispatch target: %08X", dispatch);
        break;
    }
}

static uint32_t areaset_boss_records(AeRam *r) {
    uint32_t collision_table = ae_rd32(
        r, AE_AREASET_G_AREA_STATE + AE_AREASET_AREA_COLLISION_TABLE);
    return collision_table + ae_rd32(r, collision_table + AE_AREASET_BOSS_RECORDS_OFFSET);
}

static void areaset_boss_load_position(AeRam *r, uint32_t object, uint32_t record) {
    ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_X,
            (uint32_t)(int32_t)ae_rds16(r, record + AE_AREASET_BOSS_RECORD_X));
    ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Y,
            (uint32_t)(int32_t)ae_rds16(r, record + AE_AREASET_BOSS_RECORD_Y));
    ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Z,
            (uint32_t)(int32_t)ae_rds16(r, record + AE_AREASET_BOSS_RECORD_Z));
}

static int32_t areaset_boss_progress(AeRam *r, const AeAreaSetHooks *h,
                                     uint32_t progress_id) {
    return AREA_HOOK(ProgressGetByte, progress_id, 0u, 0u, 0u);
}

static int32_t areaset_overlay_call(AeRam *r, const AeAreaSetHooks *h,
                                    uint32_t target, uint32_t a0, uint32_t a1,
                                    uint32_t a2, uint32_t a3) {
    switch (target) {
    case AE_BOSS44_OBJECT_SETUP: return AREA_HOOK(FUN_8013d1f0, a0, a1, a2, a3);
    case AE_BOSS44_OBJECT_SET_ANIM: return AREA_HOOK(Area44_Boss_SetAnim, a0, a1, a2, a3);
    case AE_BOSS44_INIT_EFFECTS: return AREA_HOOK(FUN_8013f68c, a0, a1, a2, a3);
    case AE_BOSS44_INIT_PROJECTILES: return AREA_HOOK(FUN_8013f130, a0, a1, a2, a3);
    case AE_BOSS44_INIT_SEGMENTS: return AREA_HOOK(FUN_8013ecf4, a0, a1, a2, a3);
    case AE_BOSS44_INIT_SPAWNS: return AREA_HOOK(FUN_8013d52c, a0, a1, a2, a3);
    case AE_BOSS44_INIT_BOSS: return AREA_HOOK(FUN_8013db98, a0, a1, a2, a3);
    case AE_BOSS44_INIT_ARENA: return AREA_HOOK(FUN_8013dc00, a0, a1, a2, a3);
    case AE_BOSS46_OBJECT_SETUP: return AREA_HOOK(FUN_8013d440, a0, a1, a2, a3);
    case AE_BOSS46_OBJECT_SET_ANIM: return AREA_HOOK(Area46_Boss_SetAnim, a0, a1, a2, a3);
    case AE_BOSS46_CLEAR_FLAG: return AREA_HOOK(Area46_Boss_ClearFlag, a0, a1, a2, a3);
    case AE_BOSS46_INIT_EFFECTS: return AREA_HOOK(FUN_8013f544, a0, a1, a2, a3);
    case AE_BOSS46_INIT_PROJECTILES: return AREA_HOOK(FUN_8013e14c, a0, a1, a2, a3);
    case AE_BOSS46_INIT_ARENA: return AREA_HOOK(FUN_8013ec5c, a0, a1, a2, a3);
    case AE_BOSS46_INIT_BOSS: return AREA_HOOK(FUN_8013d8bc, a0, a1, a2, a3);
    case AE_BOSS46_INIT_ARENA_OBJECTS: return AREA_HOOK(FUN_8013df28, a0, a1, a2, a3);
    case AE_BOSS53_OBJECT_SETUP: return AREA_HOOK(FUN_8013d10c, a0, a1, a2, a3);
    case AE_BOSS53_INIT_EFFECTS: return AREA_HOOK(FUN_8013f4f0, a0, a1, a2, a3);
    case AE_BOSS53_INIT_PROJECTILES: return AREA_HOOK(FUN_8013e8c0, a0, a1, a2, a3);
    case AE_BOSS53_INIT_SPAWNS: return AREA_HOOK(FUN_8013de14, a0, a1, a2, a3);
    case AE_BOSS53_INIT_ARENA: return AREA_HOOK(FUN_8013e4d8, a0, a1, a2, a3);
    case AE_BOSS53_INIT_BOSS: return AREA_HOOK(FUN_8013d804, a0, a1, a2, a3);
    case AE_BOSS56_OBJECT_SETUP: return AREA_HOOK(FUN_8013d000, a0, a1, a2, a3);
    case AE_BOSS56_SET_CAMERA_ANIM: return AREA_HOOK(Area56_Boss_SetCameraObjAnim, a0, a1, a2, a3);
    case AE_BOSS56_INIT_EFFECTS: return AREA_HOOK(FUN_8013f86c, a0, a1, a2, a3);
    case AE_BOSS56_INIT_PROJECTILES: return AREA_HOOK(FUN_8013e4a8, a0, a1, a2, a3);
    case AE_BOSS56_INIT_ARENA: return AREA_HOOK(FUN_8013d47c, a0, a1, a2, a3);
    case AE_BOSS56_INIT_BOSS: return AREA_HOOK(FUN_8013e070, a0, a1, a2, a3);
    case AE_BOSS56_INIT_ROCKS: return AREA_HOOK(FUN_8013e7f8, a0, a1, a2, a3);
    case AE_BOSS56_INIT_ARENA_OBJECTS: return AREA_HOOK(FUN_8013f080, a0, a1, a2, a3);
    case AE_BOSS57_OBJECT_SET_ANIM: return AREA_HOOK(Area57_Boss_SetAnim, a0, a1, a2, a3);
    case AE_BOSS57_CLEAR_FLAG: return AREA_HOOK(Area57_Boss_ClearFlag, a0, a1, a2, a3);
    case AE_BOSS57_INIT_EFFECTS: return AREA_HOOK(FUN_8013e020, a0, a1, a2, a3);
    case AE_BOSS57_INIT_PROJECTILES: return AREA_HOOK(FUN_8013d33c, a0, a1, a2, a3);
    case AE_BOSS57_INIT_ARENA: return AREA_HOOK(FUN_801407bc, a0, a1, a2, a3);
    case AE_BOSS57_INIT_BOSS: return AREA_HOOK(FUN_8013d9a8, a0, a1, a2, a3);
    case AE_BOSS57_INIT_ARENA_OBJECTS: return AREA_HOOK(FUN_8013ee9c, a0, a1, a2, a3);
    default:
        ae_ram_fail(r, "unmapped area overlay helper: %08X", target);
        return 0;
    }
}

static void areaset_boss_register_body(AeRam *r, const AeAreaSetHooks *h,
                                       uint32_t object) {
    (void)ae_CollisionBodyRegister(r, h ? h->body_hooks : NULL,
                                   AE_AREASET_BOSS_BODY_TYPE,
                                   object + AE_AREASET_BOSS_BODY, object);
}

void ae_FUN_8013dc68(AeRam *r, const AeAreaSetHooks *h) {
    ae_wr8(r, AE_AREASET_BOSS_EFFECT_44 + AE_AREASET_BOSS44_EFFECT_ENABLED, 1u);
    if (areaset_boss_progress(r, h, 0u) == 5) {
        ae_wr8(r, AE_AREASET_G_BOSS_INIT_FLAG, 1u);
        return;
    }

    ae_wr16(r, AE_AREASET_BOSS_MODEL_WORD, 0u);
    uint32_t record = areaset_boss_records(r);
    for (uint32_t index = 0; index < 2u; ++index) {
        uint32_t object = AE_AREASET_BOSS_OBJECTS_44 +
                          index * AE_AREASET_BOSS_OBJECT_STRIDE_44;
        uint32_t body_fields = object + AE_AREASET_BOSS_BODY_FIELDS;
        areaset_overlay_call(r, h, AE_BOSS44_OBJECT_SETUP, index, object,
                             object + AE_AREASET_BOSS_MODEL_RENDER_DATA, 0u);
        areaset_boss_load_position(r, object, record);

        ae_wr8(r, body_fields + AE_AREASET_BOSS44_BODY_STATUS, 6u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS44_BODY_TIMER, 0u);
        ae_wr32(r, body_fields + AE_AREASET_BOSS44_BODY_FLAGS, 0u);
        ae_wr32(r, body_fields + AE_AREASET_BOSS44_BODY_MODE, 0u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS44_BODY_COOLDOWN, 60u);
        ae_wr32(r, body_fields + AE_AREASET_BOSS44_BODY_TARGET, 0u);
        ae_wr8(r, body_fields + AE_AREASET_BOSS44_BODY_ACTIVE, 0u);
        ae_wr8(r, body_fields + AE_AREASET_BOSS44_BODY_STATE, 0u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_X, 0x1000u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_Y, 0u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_Z, 0u);
        ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Z,
                (uint32_t)(int32_t)ae_rds16(r, record + AE_AREASET_BOSS_RECORD_Z));

        if (ae_rd16(r, record + AE_AREASET_BOSS_RECORD_KIND) == 200u) {
            ae_wr8(r, body_fields + AE_AREASET_BOSS44_BODY_ANIM, 0u);
            areaset_overlay_call(r, h, AE_BOSS44_OBJECT_SET_ANIM,
                                 object, 2u, 0u, 0u);
            ae_wr8(r, body_fields + AE_AREASET_BOSS44_BODY_REGISTERED_A, 0u);
            ae_wr8(r, body_fields + AE_AREASET_BOSS44_BODY_REGISTERED_B, 0u);
            areaset_boss_register_body(r, h, object);
            ae_wr16(r, body_fields + AE_AREASET_BOSS44_BODY_COLLISION_X, 1u);
            ae_wr16(r, body_fields + AE_AREASET_BOSS44_BODY_COLLISION_Y, 0x8102u);
            ae_wr16(r, body_fields - AE_AREASET_BOSS_BODY_COLLISION_PREV, 1250u);
            ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_BASE, 768u);
        }
        record += AE_AREASET_BOSS_RECORD_STRIDE;
    }

    areaset_overlay_call(r, h, AE_BOSS44_INIT_EFFECTS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS44_INIT_PROJECTILES, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS44_INIT_SEGMENTS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS44_INIT_SPAWNS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS44_INIT_BOSS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS44_INIT_ARENA, 0u, 0u, 0u, 0u);
}

void ae_FUN_8013df90(AeRam *r, const AeAreaSetHooks *h) {
    if (areaset_boss_progress(r, h, 8u) == 5) {
        ae_wr8(r, AE_AREASET_G_BOSS_INIT_FLAG, 1u);
        return;
    }

    ae_wr16(r, AE_AREASET_BOSS_MODEL_WORD, 0u);
    uint32_t object = AE_AREASET_BOSS_OBJECTS_46;
    uint32_t body_fields = object + AE_AREASET_BOSS_BODY_FIELDS;
    areaset_overlay_call(r, h, AE_BOSS46_OBJECT_SETUP, 0u, object,
                         object + AE_AREASET_BOSS_MODEL_RENDER_DATA, 0u);
    ae_wr8(r, body_fields + AE_AREASET_BOSS46_BODY_KIND_A, 5u);
    ae_wr8(r, body_fields + AE_AREASET_BOSS46_BODY_KIND_B, 5u);
    ae_wr8(r, body_fields + AE_AREASET_BOSS46_BODY_ENABLED, 1u);
    ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_X, 0x600u);
    ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Y, 0xFFFFF600u);
    ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Z, 0xFFFFE4C0u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_X, 0u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_Y, 0u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_Z, 0x1000u);
    areaset_overlay_call(r, h, AE_BOSS46_OBJECT_SET_ANIM, object, 0u, 0u, 0u);

    ae_wr8(r, body_fields + AE_AREASET_BOSS46_BODY_REGISTERED_A, 0u);
    ae_wr8(r, body_fields + AE_AREASET_BOSS46_BODY_REGISTERED_B, 0u);
    ae_wr8(r, body_fields + AE_AREASET_BOSS46_BODY_REGISTERED_C, 0u);
    ae_wr8(r, body_fields + AE_AREASET_BOSS46_BODY_REGISTERED_D, 0u);
    areaset_boss_register_body(r, h, object);
    ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_Y, 0xC200u);
    ae_wr16(r, body_fields - AE_AREASET_BOSS_BODY_COLLISION_PREV, 450u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_X, 1u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_BASE, 360u);
    areaset_overlay_call(r, h, AE_BOSS46_CLEAR_FLAG, 0u, 0u, 0u, 0u);

    uint32_t state = AE_AREASET_BOSS_STATE_46;
    uint32_t secondary = AE_AREASET_BOSS_SECONDARY_46;
    ae_wr8(r, state, 1u);
    ae_wr32(r, state + AE_AREASET_BOSS46_SECONDARY_FLAGS, 0u);
    ae_wr8(r, state + AE_AREASET_BOSS46_SECONDARY_ACTIVE, 0u);
    ae_wr8(r, state + AE_AREASET_BOSS46_SECONDARY_MODE, 0u);
    ae_wr8(r, state + AE_AREASET_BOSS46_SECONDARY_ENABLED, 0u);
    areaset_boss_register_body(r, h, secondary);
    ae_wr16(r, secondary + AE_AREASET_BOSS46_SECONDARY_FIELD_A, 0x4000u);
    ae_wr16(r, secondary + AE_AREASET_BOSS46_SECONDARY_FIELD_B, 80u);
    ae_wr16(r, secondary + AE_AREASET_BOSS46_SECONDARY_FIELD_C, 0u);
    ae_wr16(r, secondary + AE_AREASET_BOSS46_SECONDARY_FIELD_D, 128u);

    areaset_overlay_call(r, h, AE_BOSS46_INIT_EFFECTS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS46_INIT_PROJECTILES, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS46_INIT_ARENA, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS46_INIT_BOSS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS46_INIT_ARENA_OBJECTS, 0u, 0u, 0u, 0u);
}

void ae_FUN_8013d1ac(AeRam *r, const AeAreaSetHooks *h) {
    ae_wr16(r, AE_AREASET_BOSS_MODEL_WORD, 0u);
    uint32_t record = areaset_boss_records(r);
    for (uint32_t index = 0; index < 8u; ++index) {
        uint32_t object = AE_AREASET_BOSS_OBJECTS_53 +
                          index * AE_AREASET_BOSS_OBJECT_STRIDE_53;
        uint32_t body_fields = object + AE_AREASET_BOSS_BODY_FIELDS;
        areaset_overlay_call(r, h, AE_BOSS53_OBJECT_SETUP, index, object,
                             object + AE_AREASET_BOSS_MODEL_DATA, 0u);
        ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Y, 0xFFFFCA00u);
        ae_wr32(r, body_fields + AE_AREASET_BOSS53_BODY_REGISTERED, 0u);
        ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_X, 0u);
        ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Z, 4792u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_X, 0u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_Y, 0u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_Z, 0xF000u);

        if (ae_rd16(r, record + AE_AREASET_BOSS_RECORD_KIND) == 0xD4u) {
            areaset_boss_register_body(r, h, object);
            ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_X, 0u);
            ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_Y, 0xC000u);
            ae_wr16(r, body_fields - AE_AREASET_BOSS_BODY_COLLISION_PREV, 560u);
            ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_BASE, 900u);
        }
        record += AE_AREASET_BOSS_RECORD_STRIDE;
    }

    uint32_t state = AE_AREASET_BOSS_STATE_53;
    ae_wr8(r, state + AE_AREASET_BOSS53_STATE_ACTIVE, 1u);
    ae_wr32(r, state + AE_AREASET_BOSS53_STATE_READY, 0u);
    ae_wr32(r, state + AE_AREASET_BOSS53_STATE_TARGET, 0u);
    ae_wr32(r, state + AE_AREASET_BOSS53_STATE_COUNTER, 0u);
    ae_wr32(r, state + AE_AREASET_BOSS53_STATE_MODE, 0u);
    ae_wr16(r, state + AE_AREASET_BOSS53_STATE_TIMER_A, 0u);
    ae_wr16(r, state + AE_AREASET_BOSS53_STATE_TIMER_B, 0u);
    ae_wr8(r, state + AE_AREASET_BOSS53_STATE_FLAGS, 1u);
    ae_wr8(r, state + AE_AREASET_BOSS53_STATE_KIND, 6u);
    ae_wr8(r, state + AE_AREASET_BOSS53_STATE_COMPLETE, 0u);
    areaset_overlay_call(r, h, AE_BOSS53_INIT_EFFECTS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS53_INIT_PROJECTILES, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS53_INIT_SPAWNS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS53_INIT_ARENA, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS53_INIT_BOSS, 0u, 0u, 0u, 0u);
}

void ae_FUN_8013dae8(AeRam *r, const AeAreaSetHooks *h) {
    ae_wr16(r, AE_AREASET_BOSS_MODEL_WORD, 0u);
    if (areaset_boss_progress(r, h, 3u) == 5) {
        ae_wr8(r, AE_AREASET_G_BOSS_INIT_FLAG, 1u);
        return;
    }

    uint32_t record = areaset_boss_records(r);
    for (uint32_t index = 0; index < 7u; ++index) {
        uint32_t object = AE_AREASET_BOSS_OBJECTS_56 +
                          index * AE_AREASET_BOSS_OBJECT_STRIDE_56;
        uint32_t body_fields = object + AE_AREASET_BOSS_BODY_FIELDS;
        areaset_overlay_call(r, h, AE_BOSS56_OBJECT_SETUP, index, object,
                             object + AE_AREASET_BOSS_MODEL_CAMERA_DATA, 0u);
        ae_wr8(r, body_fields + AE_AREASET_BOSS56_BODY_ACTIVE, 1u);
        ae_wr8(r, body_fields + AE_AREASET_BOSS56_BODY_STATUS_A, 6u);
        ae_wr8(r, body_fields + AE_AREASET_BOSS56_BODY_STATUS_B, 6u);
        ae_wr8(r, body_fields + AE_AREASET_BOSS56_BODY_STATUS_C, 0u);
        ae_wr32(r, body_fields + AE_AREASET_BOSS56_BODY_FLAGS, 0u);
        ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_X, 0u);
        ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Y, 0xFFFFD000u);
        ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Z, 0x10B8u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_X, 0u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_Y, 0u);
        ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_Z, 0xF000u);

        if (ae_rd16(r, record + AE_AREASET_BOSS_RECORD_KIND) == 0xD4u) {
            areaset_boss_register_body(r, h, object);
            ae_wr16(r, body_fields - AE_AREASET_BOSS_BODY_COLLISION_PREV, 600u);
            ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_X, 0u);
            ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_Y, 0xC000u);
            ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_BASE, 900u);
        }
        record += AE_AREASET_BOSS_RECORD_STRIDE;
    }

    uint32_t secondary = AE_AREASET_BOSS_SECONDARY_56;
    uint32_t secondary_flags = AE_AREASET_BOSS_SECONDARY_FLAGS_56;
    for (uint32_t index = 0; index < 2u; ++index) {
        areaset_boss_register_body(r, h, secondary);
        ae_wr16(r, secondary + AE_AREASET_BOSS56_SECONDARY_FIELD_A, 0x0180u - index * 0x0300u);
        ae_wr16(r, secondary + AE_AREASET_BOSS56_SECONDARY_FIELD_B, 0xC000u);
        ae_wr16(r, secondary + AE_AREASET_BOSS56_SECONDARY_FIELD_C, 0u);
        ae_wr16(r, secondary + AE_AREASET_BOSS56_SECONDARY_FIELD_D, 98u);
        ae_wr16(r, secondary + AE_AREASET_BOSS56_SECONDARY_FIELD_E, 160u);
        ae_wr16(r, secondary + AE_AREASET_BOSS56_SECONDARY_FIELD_F, 0xCCCCu);
        ae_wr16(r, secondary + AE_AREASET_BOSS56_SECONDARY_POSITION, 4280u);
        ae_wr8(r, secondary_flags, 2u);
        ae_wr8(r, secondary_flags + AE_AREASET_BOSS_SECONDARY_FLAG_NEXT, 2u);
        secondary += AE_AREASET_BOSS_SECONDARY_STRIDE_56;
        secondary_flags += AE_AREASET_BOSS_OBJECT_STRIDE_56;
    }

    uint32_t state = AE_AREASET_BOSS_STATE_56;
    ae_wr32(r, state + AE_AREASET_BOSS56_STATE_POINTER,
            ae_rd32(r, AE_AREASET_BOSS56_TEMPLATE_POINTER));
    areaset_overlay_call(r, h, AE_BOSS56_SET_CAMERA_ANIM, 0u, 0u, 0u, 0u);
    ae_wr16(r, state + AE_AREASET_BOSS56_STATE_TIMER, 90u);
    ae_wr8(r, state + AE_AREASET_BOSS56_STATE_MODE, 3u);
    ae_wr8(r, state + AE_AREASET_BOSS56_STATE_ENABLED, 0u);
    ae_wr8(r, state + AE_AREASET_BOSS56_STATE_FLAG_A, 0u);
    ae_wr8(r, state + AE_AREASET_BOSS56_STATE_FLAG_B, 0u);
    ae_wr8(r, state + AE_AREASET_BOSS56_STATE_FLAG_C, 0u);
    ae_wr8(r, state + AE_AREASET_BOSS56_STATE_ACTIVE, 1u);
    ae_wr8(r, state + AE_AREASET_BOSS56_STATE_FLAG_D, 0u);
    ae_wr8(r, state + AE_AREASET_BOSS56_STATE_FLAG_E, 0u);
    ae_wr16(r, state + AE_AREASET_BOSS56_STATE_COUNTER, 0u);
    areaset_overlay_call(r, h, AE_BOSS56_INIT_EFFECTS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS56_INIT_PROJECTILES, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS56_INIT_ARENA, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS56_INIT_BOSS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS56_INIT_ROCKS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS56_INIT_ARENA_OBJECTS, 0u, 0u, 0u, 0u);
}

void ae_FUN_8013da10(AeRam *r, const AeAreaSetHooks *h) {
    ae_wr16(r, AE_AREASET_BOSS_MODEL_WORD, 0u);
    if (areaset_boss_progress(r, h, 12u) == 5) {
        ae_wr8(r, AE_AREASET_G_BOSS_INIT_FLAG, 1u);
        return;
    }

    uint32_t object = AE_AREASET_BOSS_OBJECT_57;
    uint32_t body_fields = object + AE_AREASET_BOSS_BODY_FIELDS;
    areaset_overlay_call(r, h, AE_BOSS57_OBJECT_SETUP, 0u, object,
                         object + AE_AREASET_BOSS_MODEL_RENDER_DATA, 0u);
    ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_X,
            (uint32_t)(int32_t)ae_rds16(r, AE_AREASET_BOSS57_POSITION_TABLE +
                                       AE_AREASET_BOSS57_POSITION_X));
    ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Y,
            (uint32_t)(int32_t)ae_rds16(r, AE_AREASET_BOSS57_POSITION_TABLE +
                                       AE_AREASET_BOSS57_POSITION_Y));
    ae_wr32(r, object + AE_AREASET_BOSS_OBJECT_POSITION_Z,
            (uint32_t)(int32_t)ae_rds16(r, AE_AREASET_BOSS57_POSITION_TABLE +
                                       AE_AREASET_BOSS57_POSITION_Z));
    ae_wr8(r, body_fields + AE_AREASET_BOSS57_BODY_KIND, 2u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS57_BODY_SIZE, 90u);
    ae_wr8(r, body_fields + AE_AREASET_BOSS57_BODY_ENABLED, 1u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS57_BODY_FLAGS, 0u);
    ae_wr8(r, body_fields + AE_AREASET_BOSS57_BODY_KIND_B, 4u);
    ae_wr8(r, body_fields + AE_AREASET_BOSS57_BODY_ENABLED_B, 1u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_X, 0u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_Y, 0u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_ROTATION_Z, 0x1000u);
    areaset_overlay_call(r, h, AE_BOSS57_OBJECT_SET_ANIM, object, 0u, 0u, 0u);
    areaset_boss_register_body(r, h, object);
    ae_wr16(r, body_fields + AE_AREASET_BOSS57_BODY_COLLISION_X, 0xC200u);
    ae_wr16(r, body_fields - AE_AREASET_BOSS_BODY_COLLISION_PREV, 180u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS57_BODY_COLLISION_Z, 1u);
    ae_wr16(r, body_fields + AE_AREASET_BOSS_BODY_COLLISION_BASE, 800u);
    areaset_overlay_call(r, h, AE_BOSS57_CLEAR_FLAG, 1u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS57_INIT_EFFECTS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS57_INIT_PROJECTILES, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS57_INIT_ARENA, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS57_INIT_BOSS, 0u, 0u, 0u, 0u);
    areaset_overlay_call(r, h, AE_BOSS57_INIT_ARENA_OBJECTS, 0u, 0u, 0u, 0u);

    ae_wr8(r, AE_AREASET_BOSS_STATE_57 + AE_AREASET_BOSS57_STATE, 1u);
    ae_wr8(r, AE_AREASET_BOSS_STATE_57 + AE_AREASET_BOSS57_STATE_CLEAR_BYTE, 0u);
    ae_wr8(r, AE_AREASET_BOSS_STATE_57 + AE_AREASET_BOSS57_STATE_ACTIVE, 1u);
    ae_wr16(r, AE_AREASET_BOSS_STATE_57 + AE_AREASET_BOSS57_STATE_TIMER, 100u);
}

void ae_EffectsReset(AeRam *r, const AeAreaSetHooks *h) {
    areaset_clear_byte_records(r, AE_AREASET_PARTICLES, AE_AREASET_PARTICLE_COUNT,
                               AE_AREASET_PARTICLE_STRIDE,
                               AE_AREASET_PARTICLE_ACTIVE);
    ae_wr8(r, AE_AREASET_EFFECT_COUNT, 0u);
    ae_EffectsResetSubsystems(r, h);
    ae_PlayerFxBlinkReset(r, h);
}

void ae_EffectsResetSubsystems(AeRam *r, const AeAreaSetHooks *h) {
    ae_FxTableClear12(r, h);
    ae_EffectObjsReset(r, h);
    ae_RandomParticlesReset(r, h);
    ae_Gadget4TrailReset(r);
    ae_TrailsReset(r, h);
    ae_SparkTableClear(r, h);
}

void ae_PlayerFxBlinkReset(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    ae_wr8(r, AE_AREASET_BLINK_FLAGS + AE_AREASET_BLINK_FLAG_A, 0u);
    ae_wr8(r, AE_AREASET_BLINK_FLAGS + AE_AREASET_BLINK_FLAG_B, 0u);
    ae_wr8(r, AE_AREASET_BLINK_FLAGS + AE_AREASET_BLINK_FLAG_C, 0u);
    ae_wr16(r, AE_AREASET_BLINK_FLAGS + AE_AREASET_BLINK_TIMER, 0u);
    ae_wr32(r, AE_AREASET_BLINK_WORD, 0u);
}

void ae_SparkTableClear(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    areaset_clear_byte_records(r, AE_AREASET_SPARKS, AE_AREASET_SPARK_COUNT,
                               AE_AREASET_SPARK_STRIDE, AE_AREASET_SPARK_ACTIVE);
}

void ae_TrailsReset(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    areaset_clear_byte_records(r, AE_AREASET_TRAILS, AE_AREASET_TRAIL_COUNT,
                               AE_AREASET_TRAIL_STRIDE, AE_AREASET_TRAIL_ACTIVE);
    ae_wr8(r, AE_AREASET_TRAIL_GLOBAL, 0u);
}

void ae_EffectObjsReset(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    areaset_clear_byte_records(r, AE_AREASET_EFFECT_OBJECTS,
                               AE_AREASET_EFFECT_OBJECT_COUNT,
                               AE_AREASET_EFFECT_OBJECT_STRIDE,
                               AE_AREASET_EFFECT_OBJECT_ACTIVE);
}

void ae_RandomParticlesReset(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    areaset_clear_byte_records(r, AE_AREASET_RANDOM_PARTICLES,
                               AE_AREASET_RANDOM_PARTICLE_COUNT,
                               AE_AREASET_RANDOM_PARTICLE_STRIDE,
                               AE_AREASET_RANDOM_PARTICLE_ACTIVE);
    ae_wr8(r, AE_AREASET_RANDOM_PARTICLE_GLOBAL, 0u);
}

void ae_FxTableClear12(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    areaset_clear_byte_records(r, AE_AREASET_FX_TABLE_12,
                               AE_AREASET_FX_TABLE_12_COUNT,
                               AE_AREASET_FX_TABLE_12_STRIDE,
                               AE_AREASET_FX_TABLE_12_ACTIVE);
}

void ae_IndicatorsReset(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    ae_wr32(r, AE_AREASET_INDICATOR_ACTIVE, 0u);
    ae_wr8(r, AE_AREASET_INDICATOR_COUNT, 0u);
}

static void areaset_indicator_model(AeRam *r, uint32_t instance, uint32_t feature) {
    uint32_t model_header = feature +
        (uint32_t)ae_rd16(r, feature + AE_AREASET_FEATURE_MODEL_OFFSET) * 4u;
    ae_ModelInit(r, NULL, instance, model_header);
    uint32_t animation = feature +
        (uint32_t)ae_rd16(r, feature + AE_AREASET_FEATURE_ANIM_OFFSET) * 4u;
    ae_wr32(r, instance + AE_AREASET_MODEL_INSTANCE_ANIM, animation);
}

void ae_AreaSetIndicatorModelA(AeRam *r, const AeAreaSetHooks *h,
                               uint32_t feature) {
    (void)h;
    areaset_indicator_model(r, AE_AREASET_INDICATOR_MODEL_A, feature);
}

void ae_AreaSetIndicatorModelB(AeRam *r, const AeAreaSetHooks *h,
                               uint32_t feature) {
    (void)h;
    areaset_indicator_model(r, AE_AREASET_INDICATOR_MODEL_B, feature);
}

void ae_AreaSpecialFlagsClear(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    ae_wr32(r, AE_AREASET_G_AREA_SPECIAL_FLAGS, 0u);
}

void ae_MonkeyLodModelsInit(AeRam *r, const AeAreaSetHooks *h) {
    if (ae_rd16(r, AE_AREASET_G_AREA_STATE + AE_AREASET_STATE_LEVEL) != 0x20u) {
        uint32_t model_header = ae_rd32(r, AE_AREASET_G_AREA_STATE +
                                        AE_AREASET_STATE_MONKEY_LOD_MODEL);
        ae_ModelInit(r, h, AE_AREASET_MONKEY_LOD_INSTANCE, model_header);
        uint32_t anim_index = ae_rd16(r, AE_AREASET_MONKEY_LOD_ANIM_INDEX);
        ae_wr32(r, AE_AREASET_MONKEY_LOD_ANIM_POINTER,
                AE_AREASET_MONKEY_LOD_TABLE + anim_index * 4u);
        uint32_t lod_index = ae_rd16(r, AE_AREASET_MONKEY_LOD_MODEL_INDEX);
        ae_wr32(r, AE_AREASET_G_MONKEY_MODEL_POINTER,
                AE_AREASET_MONKEY_LOD_TABLE + lod_index * 4u);
    }
}

void ae_TargetListClear(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    for (uint32_t index = 0; index < AE_AREASET_TARGET_COUNT; ++index) {
        ae_wr32(r, AE_AREASET_TARGET_LIST + index * AE_AREASET_TARGET_STRIDE, 0u);
    }
}

void ae_TableClearDAT_800BEF60(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    ae_wr16(r, AE_AREASET_RENDER_TABLE + AE_AREASET_RENDER_TABLE_FLAG_A, 0u);
    ae_wr16(r, AE_AREASET_RENDER_TABLE + AE_AREASET_RENDER_TABLE_FLAG_B, 0u);
    ae_wr16(r, AE_AREASET_RENDER_TABLE + AE_AREASET_RENDER_TABLE_MODE, 1u);
}

void ae_FxSlotsReset(AeRam *r, const AeAreaSetHooks *h) {
    areaset_clear_byte_records(r, AE_AREASET_FX_SLOTS, AE_AREASET_FX_SLOT_COUNT,
                               AE_AREASET_FX_SLOT_STRIDE,
                               AE_AREASET_FX_SLOT_ACTIVE);
    ae_ProjectileGroupsClear(r, h);
}

void ae_ProjectileGroupsClear(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    areaset_clear_byte_records(r, AE_AREASET_PROJECTILE_GROUPS,
                               AE_AREASET_PROJECTILE_GROUP_COUNT,
                               AE_AREASET_PROJECTILE_GROUP_STRIDE,
                               AE_AREASET_PROJECTILE_GROUP_ACTIVE);
}

void ae_HeapInit(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    for (uint32_t index = 0; index < AE_AREASET_HEAP_BLOCK_COUNT; ++index) {
        ae_wr32(r, AE_AREASET_HEAP_BLOCKS + index * 4u, 0u);
    }
    ae_wr32(r, AE_AREASET_HEAP_COUNT, 0u);
    ae_wr32(r, AE_AREASET_HEAP_CURSOR, AE_AREASET_HEAP_LIMIT);
}

void ae_AreaBumpAllocSet(AeRam *r, const AeAreaSetHooks *h, uint32_t cursor) {
    (void)h;
    ae_wr32(r, AE_AREASET_HEAP_CURSOR, cursor);
}

int32_t ae_exit(AeRam *r, const AeAreaSetHooks *h, uint32_t status) {
    return AREA_HOOK(Exit, status, 0, 0, 0);
}

int32_t ae_printf(AeRam *r, const AeAreaSetHooks *h, uint32_t format,
                  uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return AREA_HOOK(Printf, format, arg1, arg2, arg3);
}

int32_t ae_SmallocError(AeRam *r, const AeAreaSetHooks *h, uint32_t byte_count) {
    (void)ae_printf(r, h, AE_AREASET_HEAP_ERROR_TITLE, 0u, 0u, 0u);
    (void)ae_printf(r, h, AE_AREASET_HEAP_ERROR_DETAIL, byte_count,
                    ae_rd32(r, AE_AREASET_HEAP_COUNT), 0u);
    return ae_exit(r, h, AE_AREASET_HEAP_ERROR_EXIT_STATUS);
}

uint32_t ae_HeapAlloc(AeRam *r, const AeAreaSetHooks *h, uint32_t byte_count) {
    uint32_t biased = byte_count + 0x7FFu;
    if ((int32_t)biased < 0) biased = byte_count + 0xFFEu;
    int32_t required_blocks = (int32_t)areaset_sra32(biased, 11u);
    if (required_blocks == 0) return 0u;

    uint32_t block_count = ae_rd32(r, AE_AREASET_HEAP_COUNT);
    uint32_t available_blocks = AE_AREASET_HEAP_BLOCK_COUNT - block_count;
    if ((int32_t)available_blocks < required_blocks) {
        return (uint32_t)ae_SmallocError(r, h, byte_count);
    }

    block_count = ae_rd32(r, AE_AREASET_HEAP_COUNT);
    if ((int32_t)block_count < (int32_t)AE_AREASET_HEAP_BLOCK_COUNT) {
        int32_t table_index = (int32_t)block_count;
        int32_t empty_run = 0;
        while (table_index < (int32_t)AE_AREASET_HEAP_BLOCK_COUNT) {
            uint32_t slot = AE_AREASET_HEAP_BLOCKS + (uint32_t)table_index * 4u;
            if (ae_rd32(r, slot) != 0u) {
                empty_run = 0;
            } else {
                ++empty_run;
                if (empty_run == required_blocks) {
                    int32_t first_block = table_index - empty_run + 1;
                    uint32_t allocation = AE_AREASET_HEAP_ALLOC_BASE +
                        (uint32_t)first_block * AE_AREASET_HEAP_BLOCK_BYTES;
                    for (int32_t index = first_block; index <= table_index; ++index) {
                        uint32_t entry = AE_AREASET_HEAP_BLOCKS + (uint32_t)index * 4u;
                        ae_wr32(r, entry, allocation);
                    }
                    ae_wr32(r, AE_AREASET_HEAP_COUNT, 0u);
                    return allocation;
                }
            }
            ++table_index;
        }
    }
    return (uint32_t)ae_SmallocError(r, h, byte_count);
}

uint32_t ae_AreaBumpAlloc(AeRam *r, const AeAreaSetHooks *h,
                          uint32_t byte_count) {
    uint32_t cursor = ae_rd32(r, AE_AREASET_HEAP_CURSOR);
    uint32_t requested_end = cursor + byte_count;
    if (AE_AREASET_HEAP_LIMIT_LAST < requested_end) {
        return ae_HeapAlloc(r, h, byte_count);
    }

    uint32_t next_cursor = cursor + areaset_align_word(byte_count);
    ae_wr32(r, AE_AREASET_HEAP_CURSOR, next_cursor);
    return cursor;
}

void ae_MessageBoxClose(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    ae_wr8(r, AE_AREASET_G_MESSAGE_BOX, 0u);
}

void ae_RcCarDespawn(AeRam *r, const AeAreaSetHooks *h) {
    uint32_t active = ae_rd32(r, AE_AREASET_RC_CAR + AE_AREASET_RC_CAR_ACTIVE);
    if (active != 0u) {
        ae_wr32(r, AE_AREASET_RC_CAR + AE_AREASET_RC_CAR_ACTIVE, 0u);
        (void)ae_CollisionBodyUnregister(r, h ? h->body_hooks : NULL,
            AE_AREASET_RC_CAR + AE_AREASET_RC_CAR_BODY);
    }
}

static void areaset_select_actor_callbacks(AeRam *r, uint32_t level) {
    uint32_t level_offset = level - AE_AREASET_SPECIAL_ACTOR_LEVEL_FIRST;
    if (level_offset >= AE_AREASET_SPECIAL_ACTOR_LEVEL_COUNT) return;

    uint32_t dispatch = ae_rd32(r, AE_AREASET_SPECIAL_ACTOR_TABLE + level_offset * 4u);
    switch (dispatch) {
    case AE_AREASET_ACTOR_CASE_LEVEL21:
        ae_wr32(r, AE_AREASET_SPECIAL_ACTOR_DRAW, AE_AREASET_ACTOR_DRAW_LEVEL21);
        ae_wr32(r, AE_AREASET_SPECIAL_ACTOR_INIT, AE_AREASET_ACTOR_INIT_LEVEL21);
        break;
    case AE_AREASET_ACTOR_CASE_LEVEL22:
        ae_wr32(r, AE_AREASET_SPECIAL_ACTOR_INIT, AE_AREASET_ACTOR_INIT_LEVEL22);
        ae_wr32(r, AE_AREASET_SPECIAL_ACTOR_DRAW, AE_AREASET_ACTOR_DRAW_LEVEL22);
        ae_wr32(r, AE_AREASET_SPECIAL_ACTOR_UPDATE,
                AE_AREASET_ACTOR_UPDATE_LEVEL22);
        ae_wr32(r, AE_AREASET_SPECIAL_ACTOR_TICK, AE_AREASET_ACTOR_TICK_LEVEL22);
        break;
    case AE_AREASET_ACTOR_CASE_LEVEL4:
        ae_wr32(r, AE_AREASET_SPECIAL_ACTOR_UPDATE, AE_AREASET_ACTOR_UPDATE_LEVEL4);
        ae_wr32(r, AE_AREASET_SPECIAL_ACTOR_TICK, AE_AREASET_ACTOR_TICK_LEVEL4);
        break;
    case AE_AREASET_ACTOR_CASE_LEVEL12:
        ae_wr32(r, AE_AREASET_SPECIAL_ACTOR_UPDATE,
                AE_AREASET_ACTOR_UPDATE_LEVEL12);
        ae_wr32(r, AE_AREASET_SPECIAL_ACTOR_TICK, AE_AREASET_ACTOR_TICK_LEVEL12);
        break;
    default:
        break;
    }
}

void ae_AreaSetSpecialActor(AeRam *r, const AeAreaSetHooks *h,
                            uint32_t feature, uint32_t feature_kind) {
    uint32_t actor = ae_AreaBumpAlloc(r, h, AE_AREASET_SPECIAL_ACTOR_BYTES);
    ae_wr32(r, AE_AREASET_SPECIAL_ACTOR, actor);

    uint32_t model_header = feature +
        (uint32_t)ae_rd16(r, feature + AE_AREASET_FEATURE_MODEL_OFFSET) * 4u;
    ae_ModelInit(r, h, actor, model_header);

    ae_wr16(r, actor + AE_AREASET_SPECIAL_ACTOR_SCALE_X, 0u);
    ae_wr16(r, actor + AE_AREASET_SPECIAL_ACTOR_SCALE_Y, 0u);
    ae_wr16(r, actor + AE_AREASET_SPECIAL_ACTOR_SCALE_Z,
            AE_AREASET_SPECIAL_ACTOR_UNIT_SCALE);
    ae_wr16(r, actor + AE_AREASET_SPECIAL_ACTOR_ANGLE_X, 0u);
    ae_wr16(r, actor + AE_AREASET_SPECIAL_ACTOR_ANGLE_Y,
            AE_AREASET_SPECIAL_ACTOR_UNIT_SCALE);
    ae_wr16(r, actor + AE_AREASET_SPECIAL_ACTOR_ANGLE_Z, 0u);
    ae_wr8(r, actor + AE_AREASET_SPECIAL_ACTOR_ACTIVE, 0u);

    uint32_t animation = feature + AE_AREASET_FEATURE_ANIM_DATA_BASE +
        (uint32_t)ae_rd16(r, feature + AE_AREASET_FEATURE_ANIM_OFFSET) * 4u;
    ae_wr32(r, actor + AE_AREASET_MODEL_INSTANCE_ANIM, animation);
    areaset_select_actor_callbacks(r,
        ae_rd16(r, AE_AREASET_G_AREA_STATE + AE_AREASET_STATE_LEVEL));

    if (feature_kind == 1u) {
        uint32_t static_header = feature +
            (uint32_t)ae_rd16(r, feature + AE_AREASET_STATIC_FEATURE_MODEL) * 4u;
        ae_StaticModelInit(r, h, actor + AE_AREASET_SPECIAL_ACTOR_STATIC,
                           static_header);
        ae_wr8(r, actor + AE_AREASET_SPECIAL_ACTOR_STATIC_KIND, 6u);
    }
}

void ae_AreaSetFeature30(AeRam *r, const AeAreaSetHooks *h, uint32_t feature) {
    (void)h;
    ae_wr32(r, AE_AREASET_G_FEATURE_30, 0u);
    ae_wr32(r, AE_AREASET_G_FEATURE_30 + AE_AREASET_FEATURE_30_POINTER, feature);
    ae_wr32(r, AE_AREASET_G_FEATURE_30 + AE_AREASET_FEATURE_30_RESET_A, 0u);
    ae_wr32(r, AE_AREASET_G_FEATURE_30 + AE_AREASET_FEATURE_30_RESET_B, 0u);
}

void ae_AreaSetFeature31(AeRam *r, const AeAreaSetHooks *h, uint32_t feature) {
    (void)h;
    ae_wr32(r, AE_AREASET_G_FEATURE_31, feature);
}

void ae_AreaSetFeature32(AeRam *r, const AeAreaSetHooks *h, uint32_t feature) {
    (void)h;
    ae_wr32(r, AE_AREASET_G_FEATURE_32, feature);
}

