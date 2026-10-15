#ifndef AE_MONKEY_H
#define AE_MONKEY_H
/*
 * Monkey code of Ape Escape (PS1, range 0x80032E30..0x80043300), ported 1:1 from the original instructions and
 * run on a byte-exact image of PSX main RAM (AeRam, see src/core/ae_ram.h and notes/port/bodies.md for the pattern).
 *
 * Conventions (shared by every batch of the monkey port; see notes/port/monkeys-port.md):
 *  - A function is `ae_<OriginalName>` taking `AeRam *r` (+ `const AeMonkeyHooks *h` when it calls anything through a
 *    hook or a hooked function of a later batch) followed by the original arguments as 32-bit values (`int32_t` for
 *    values the original sign-extends, `uint32_t` for addresses / zero-extended values).  The original address is in a
 *    comment at each function.  Implicit `this` is the global g_CurMonkey (0x800B4D08), exactly as in the original.
 *  - Void functions (the original leaves an unrelated $v0) return void; the check scripts do not compare $v0 for them.
 *  - Globals are used at their original addresses through the AE_G_* constants; struct fields through AE_MK_* offsets.
 *  - Calls outside the port go through AeMonkeyHooks.  Every hook has the original function's name and address and one
 *    uniform signature mirroring the MIPS calling convention: (user, ram, a0, a1, a2, a3) -> $v0.  Arguments the
 *    original passes by stack pointer are RAM addresses inside the per-function scratch area AE_STK(n) (the stack
 *    window, which the checks do not compare).  A NULL hook sets the AeRam error flag and returns 0 (fail closed).
 *  - When a later batch ports a hooked monkey-range function, it replaces the hook by the direct call and removes the
 *    field from AeMonkeyHooks (the check script derives its hook table from this header, so keep the one-line
 *    form `AeMonkeyHookFn Name;` followed by a comment that starts with the 8 digit address).
 *  - SDK maths comes from src/core/ae_sdk.h (validated), collision bodies from src/bodies/ae_bodies.h.
 */
#include "../core/ae_ram.h"
#include "../core/ae_sdk.h"
#include "../bodies/ae_bodies.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- global addresses ------------------------------------------------------------------------ */
#define AE_G_MONKEY_INFO_TABLE 0x800B4CF0u   /* MonkeyInfo* (0x88 bytes each) */
#define AE_G_MONKEY_FLAG_4CF4 0x800B4CF4u    /* u8, DAT_800B4CF4: non-zero keeps all saved activities at 0 */
#define AE_G_MONKEY_TARGET 0x800B4CF8u       /* Obj* the monkey reacts to (g_Player / g_RcCar) */
#define AE_G_NAV_NODES 0x800B4D00u           /* NavNode* (0x18 bytes each) */
#define AE_G_CUR_MONKEY 0x800B4D08u          /* Monkey* being processed (implicit this) */
#define AE_G_MONKEY_RAND_SEED 0x800B4D10u    /* u32 (16 bit LCG state) */
#define AE_G_NAV_NODE_COUNT 0x800B4D18u      /* s16 */
#define AE_G_MENU_VALUE_4D44 0x800B4D44u     /* u32 read by MonkeyDebugMenuInit2 */
#define AE_G_MONKEY_COUNT 0x800AEB68u        /* s16 */
#define AE_G_ACTIVE_MONKEY_MAX 0x800AEB66u   /* s16 */
#define AE_G_CAUGHT_MONKEY 0x800AEB5Au       /* s16: monkey info index (MonkeyCaughtGroupValue) */
#define AE_G_LINK_CODE_TABLE 0x800AEB6Cu     /* u16[]: indexed by the hundreds of a nav node field (NavNodeGetLinkIdsEx) */
#define AE_G_PROJECTILE_COUNTER 0x800AEBF4u  /* u8: round-robin counter of MonkeySpawnProjectile type 6 */
#define AE_G_MONKEY_STATE_TABLE 0x800AE940u  /* {u32 animation, u32 flags}[0x43] */
#define AE_G_MONKEYS 0x800E5500u             /* Monkey[8] */
#define AE_G_VOICE_QUEUE 0x800E6780u         /* 16 x 20 bytes: s32 sfx, s16 timer, pad, s32 pos[3] */
#define AE_G_OBJS 0x800E6960u                /* Obj[16], 0xE4 bytes each */
#define AE_G_STATE_BASE 0x800F4350u          /* the game-state block (+0x122 area kind, +0x126 area, +0x16C activities) */
#define AE_G_MODEL_DRAW_BASE 0x800EC350u     /* StaticModelDraw: draw contexts (0x4078 bytes each) from +0x80, index at +0x0C */
#define AE_G_AREA_KIND 0x800F4472u           /* u16: AreaGetKind of the current area (3 / 6 / 8 bosses) */
#define AE_G_CURRENT_AREA 0x800F4476u        /* u16 */
#define AE_G_MONKEY_ACTIVITY 0x800F44BCu     /* u8[8]: saved per-monkey activity */
#define AE_G_BONE_MATRICES 0x800F4750u       /* MATRIX[] (0x20 each), indexed by the second argument of ObjGetBonePos */
/* batch 2 */
#define AE_G_FRAME_COUNTER 0x800AEB60u       /* u32 frame counter (also used shifted as the angle of the wobble functions) */
#define AE_G_DEBUG_PRINT 0x800AEB59u         /* u8: non-zero makes MonkeyPickGoalNode printf */
#define AE_G_NAV_AREA_FLAG 0x800B4D04u       /* u8 (MonkeyLineOfSight) */
#define AE_G_ROUTE_TABLE 0x800B4D0Cu         /* u8*: per monkey id (0x80 bytes) x route (0x10 bytes) goal node u16s from +0x88 */
#define AE_G_NAV_EXTRA_LINKS 0x800B4D1Cu     /* 12 byte entries {s16 lo, s16 hi, u32 flags, u32 extra} sorted by (lo, hi) */
#define AE_G_NAV_EXTRA_COUNT 0x800B4D20u     /* s16 */
#define AE_G_NAV_GROUND_REF 0x800F455Cu      /* pointer used by NavNodeGetPos for nodes with bit 0 (56 byte records) */
#define AE_G_CAMERA_SVEC 0x800F45A8u         /* SVECTOR: camera / view position (MonkeyProjectToScreen, MonkeySelectLod) */
#define AE_G_VIEW_MATRIX 0x800E78C0u         /* MATRIX loaded into the GTE by SetRotMatrix / SetTransMatrix */
#define AE_G_LOD_SWITCH 0x800E6908u          /* u8: 1 forces the highest LOD (MonkeySelectLod) */
#define AE_G_LOD_MODELS 0x800F44F4u          /* u32[3]: model headers passed to ModelInit by MonkeySelectLod */

/* batch 3 */
#define AE_G_ANIM_SET 0x800F4504u            /* u32: animation set pointer written to monkey +0x18 (normal states) */
#define AE_G_ANIM_SET2 0x800F4508u           /* u32: animation set of states 0x32..0x34 */
#define AE_G_AREA_DATA 0x800F4590u           /* u32: base added to the header offsets of MonkeySetState */
#define AE_G_AREA_HEADER 0x800B4D14u         /* u32*: area header (+0x18 / +0x1C animation table offsets) */

/* batch 4 */
#define AE_G_MONKEY_PTRS 0x800E54F0u         /* Monkey*[4]: the monkeys MonkeyUpdateVisuals walks */
#define AE_G_DEBUG_FLAG_EB58 0x800AEB58u     /* u8 (MonkeyUpdateVisuals: == 2 prints the monkey id) */

#define AE_MONKEY_COUNT 8
#define AE_MONKEY_SIZE 0x250u
#define AE_MONKEY_INFO_SIZE 0x88u
#define AE_NAV_NODE_SIZE 0x18u
#define AE_OBJ_SIZE 0xE4u
#define AE_VOICE_QUEUE_COUNT 16
#define AE_VOICE_QUEUE_SIZE 20u

/* ---- Monkey field offsets (tools/structs.txt, struct Monkey 0x250) ------------------------------ */
#define AE_MK_POS 0x24u                /* s32[3] */
#define AE_MK_ANIM_STATE_FRAME 0x242u  /* s16, returned by MonkeyGetAnimIndex for states 0x32..0x34 */
#define AE_MK_MOVE_DIR 0x60u           /* SVECTOR */
#define AE_MK_WISH_DIR 0x68u           /* SVECTOR */
#define AE_MK_AI_STATE 0x78u           /* u8 */
#define AE_MK_ACTIVITY 0x7Au           /* u8 */
#define AE_MK_SLOT 0x7Bu               /* u8 */
#define AE_MK_ID 0x7Cu                 /* u8: index into the MonkeyInfo table */
#define AE_MK_GOAL_POS 0xBCu           /* s32[3] */
#define AE_MK_LOD 0xD4u                /* s8 (read signed by MonkeyAttackChance) */
#define AE_MK_GOAL_NODE 0xE6u          /* s16 */
#define AE_MK_GOAL_AGE 0xEAu           /* u16 */
#define AE_MK_NAV_NODE0 0xF2u          /* u16 nav node history: current, previous ... */
#define AE_MK_NAV_NODE1 0xF4u
#define AE_MK_NAV_NODE2 0xF6u
#define AE_MK_NAV_PREV0 0xF8u
#define AE_MK_NAV_PREV1 0xFAu
#define AE_MK_NAV_PREV2 0xFCu
#define AE_MK_LINK_FROM 0x100u         /* s32[3] */
#define AE_MK_LINK_TO 0x110u           /* s32[3] */
#define AE_MK_LINK_FRAME 0x120u        /* s16 */
#define AE_MK_LINK_FRAMES 0x122u       /* s16 */
#define AE_MK_DIST_TO_TARGET 0x20Cu    /* s32 */
#define AE_MK_DIST_TO_PLAYER 0x210u    /* s32: MonkeySelectActive ranks the monkeys by it */
#define AE_MK_REACH_RADIUS 0x220u      /* s16 (MonkeyReachedNode scales it) */
#define AE_MK_FLAGS2 0x228u            /* u32 */
#define AE_MK_FLAGS 0x248u             /* u32 */
#define AE_MK_FLAGS3 0x24Cu            /* u32: bit 1 = collision body registered */
#define AE_MK_FLAGS3_BODY_REGISTERED 0x2u
#define AE_MK_ANIM_FRAME 0x5Cu         /* u16 (batch 2) */
#define AE_MK_COUNTER96 0x96u          /* s16 (shot counter / animation step counter) */
#define AE_MK_ANIM_TABLE 0x16Cu        /* {u16 first frame, u16 last frame, ...}[8 bytes each] */
/* more fields (tools/structs.txt) */
#define AE_MK_ANIM_INDEX 0x5Eu         /* u16: index into the animation table */
#define AE_MK_VEL 0x70u                /* s16[3]: added to pos every frame in the move / jump states */
#define AE_MK_ALERTED 0x79u            /* u8: 1 = has noticed the target */
#define AE_MK_GROUP 0xD8u              /* u8: current patrol group */
#define AE_MK_NAV_MARK_COUNT 0xEEu     /* s16 */
#define AE_MK_ANIM_LENGTH 0x128u       /* u16: frames of the current animation (also the jump arc duration) */
#define AE_MK_JUMP_START 0x12Cu        /* s32[3]: jump arc start position */
#define AE_MK_JUMP_VEL 0x13Cu          /* s32[3]: jump arc velocity (4.12; y gets the gravity term) */
#define AE_MK_JUMP_LAND_FRAMES 0x170u  /* u16: landing phase length of a jump animation */
#define AE_MK_JUMP_RISE_FRAMES 0x172u  /* u16: frames advanced during the rise of a jump */
#define AE_MK_PATROL_COUNTER 0x222u    /* s8: counted down by MonkeyAi_Patrol */
#define AE_NAV_NODE_VISIT_MARKS 0x14u  /* NavNode: one visit byte per monkey slot */
#define AE_MK_IDLE_TIMER 0x9Au         /* s16: counted down in state 1, picks an idle activity at <= 0 */
#define AE_MK_HOP_COUNTER 0xE3u        /* u8: frame counter of the state 0x2A hop */
#define AE_MK_HOP_START_Y 0xE4u        /* s16: start height of the state 0x2A hop */
#define AE_MK_LINK_ANGLE 0x21Cu        /* u32: link angle (0..0x1000) MonkeyAi_Patrol picks the idle activity by */
#define AE_MK_NODE_WAIT 0x22Cu         /* u16: wait frames at the reached node */
#define AE_MK_LOST_SIGHT_COUNT 0x249u  /* u8: failed sight checks while alerted at a bone node (MonkeyAi_Patrol) */
/* fields of MonkeyAiStateUpdate / MonkeySpawnAll (tools/structs.txt; tentative names) */
#define AE_MK_VEL32 0x30u              /* s32[3]: velocity of the thrown monkey (state 5 / 0x41), x at +0x30, z at +0x38 */
#define AE_MK_PREV_DIR 0x3Cu           /* copy of the move direction (+0x60 / +0x64) at the end of the frame */
#define AE_MK_BODY_HEIGHT 0x46u        /* s16: collision body height (0x100, or -0x100 in the LOD scale states) */
#define AE_MK_ROT 0x84u                /* SVECTOR: rotation angles of the move direction matrix */
#define AE_MK_HEALTH 0x8Eu             /* s16 */
#define AE_MK_TIMER_90 0x90u           /* s16: counted down while activity == 3; the monkey is removed at 0 */
#define AE_MK_THROW_TIMER 0x92u        /* s16: timer / counter of the states 5, 0x41 and 0x25 */
#define AE_MK_ACTION_COOLDOWN 0x94u    /* s16 */
#define AE_MK_ROUTE_COUNT 0xD6u        /* u8: spawn candidates of the monkey in the route table */
#define AE_MK_GROUP_COUNT 0xD7u        /* u8: patrol groups */
#define AE_MK_GROUP_SIZES 0xDAu        /* u8[8]: nodes in each patrol group */
#define AE_MK_STUN_TIMER 0xE2u         /* u8 */
#define AE_MK_NOTICE_COUNT 0xF0u       /* s16 */
#define AE_MK_TIMER_124 0x124u         /* s16 */
#define AE_MK_KIND 0x126u              /* s16: 8 / 11 select special contact handling (MonkeyAiStateUpdate) */
#define AE_MK_RESET_154 0x154u         /* s32: set to -1 when flags bit 21 is cleared */
#define AE_MK_TIMER_158 0x158u         /* s16: ticked down every frame by MonkeyUpdateAll */
#define AE_MK_THROW_DAMP 0x166u        /* s16: factor (1/32768) applied to the velocity of the thrown monkey */
#define AE_MK_TIMER_174 0x174u         /* u16 */
#define AE_MK_STATE_TIMER 0x230u       /* s32: frames in state 0x1E (limit 0xF0) */
#define AE_MK_FLAGS_HI 0x24Au          /* u16: upper half of the flags */
#define AE_MK_PREV_NODE 0xECu          /* s16: previous nav node (-1 = none) */
#define AE_MK_DIST_XZ_TO_GOAL 0x214u   /* s32: horizontal distance to the goal (MonkeyAi_State1E_20) */
#define AE_MK_CIRCLE_CENTER 0x234u     /* s32 x at +0x234, z at +0x23C: centre of the area the flying monkey circles in */
#define AE_MK_CIRCLE_RADIUS 0x240u     /* s16 */
#define AE_G_PLAYER 0x800EC1E0u        /* the player object (+0x24 position, +0xF2 s8: area state byte, 7 = RC car mode) */
#define AE_G_RC_CAR 0x800F5A20u        /* the RC car object (+0x24 position) */
#define AE_G_MONKEY_COUNT_F44D4 0x800F44D4u  /* s16: the monkey count of the area (0 switches the monkey system off) */
#define AE_G_ACTIVE_MAX_COPY 0x800AEB64u     /* s16: copy of the active maximum made by MonkeySpawnAll */
#define AE_G_SPAWN_SWITCH 0x800A8A58u        /* u32: non-zero starts the random seed at 0 */
#define AE_G_GAME_FLAGS_F4450 0x800F4450u    /* u32: bit 1 stops MonkeyThinkActive */
#define AE_G_LAST_MONKEY_COUNT 0x800AEC10u   /* s16: monkey count of the last MonkeyUpdateAll */
#define AE_G_LAST_ACTIVE_MAX 0x800AEC12u     /* s16: active maximum of the last MonkeyUpdateAll */
/* NavNode flags (+0x00; named after their use in MonkeyAi_Patrol, tentative) */
#define AE_NAV_FLAG_WAIT 0x4u          /* the monkey waits at the node (MK_NODE_WAIT) */
#define AE_NAV_FLAG_NEAR_TARGET 0x8u   /* decide when the target is nearer than the s16 at +0x10 */
#define AE_NAV_FLAG_REF_POS 0x40u      /* the position comes from the reference table (AE_G_NAV_GROUND_REF), record s16 at +0x10 */
#define AE_NAV_FLAG_BONE 0x400u        /* the node follows an object bone (+0x10 = object * 100 + bone) */
#define AE_NAV_FLAG_FAR_TARGET 0x8000u /* decide when the target is farther than 1500 */
#define AE_NAV_FLAG_OBJECT 0x10000u    /* the node belongs to an object slot (s16 at +0x10) */

/* Readability names: layouts from tools/structs.txt and the verified field accesses. */
#define AE_MK_ANIM_SET 0x18u /* anim set */
#define AE_MK_POS_SHORT 0x1Cu /* pos short */
#define AE_MK_BODY_FLAGS 0x4Au /* u16: collision body flags */
#define AE_MK_CONTACT 0x50u /* u32: collision contact word */
#define AE_MK_AIM_FRAME 0x7Eu /* u16: selected aiming animation frame */
#define AE_MK_SCREEN_X 0x80u /* screen x */
#define AE_MK_SCREEN_Y 0x82u /* screen y */
#define AE_MK_ROT_MATRIX 0x9Cu /* rot matrix */
#define AE_MK_GOAL_POS_PAD 0xC8u /* u32: fourth word copied along with goalPos, including stale scratch data */
#define AE_MK_MODEL_LOD 0xD5u /* u8: model detail level; distinct from the hesitation byte at +0xD4 */
#define AE_MK_GROUP_VISIT_MASK 0xE8u /* group visit mask */
#define AE_MK_SHOT_COOLDOWN 0x150u /* s16: set to 10 after an aimed shot */
#define AE_MK_FLIGHT_SFX_HANDLE 0x15Cu /* s32: flight sound loop handle */
#define AE_MK_SFX_HANDLE 0x160u /* s32: sound loop handle */
#define AE_MK_SHADOW_SIZE 0x168u /* shadow size */
#define AE_MK_LINK_SCORES 0x176u /* s16[]: candidate scores from MonkeyScoreLinksByTarget */
#define AE_MK_LINK_BLOCKED 0x1DAu /* u8[]: per-link blocked markers */
#define AE_MK_DIST_TO_GOAL 0x218u /* s32: full distance to the goal */
#define AE_MK_EXTRA_LINK_FROM 0x224u /* s16: cached extra-link start node */
#define AE_MK_EXTRA_LINK_TO 0x226u /* s16: cached extra-link end node */
#define AE_MK_LINK_SLOPE 0x22Eu /* u8: 0 none, 1 rising, 2 falling */
#define AE_MK_ALT_ANIM_CODE 0x244u /* u16: alternate animation code from the node */
#define AE_MK_ALT_ANIM_FRAME 0x246u /* u16: alternate animation frame from the node */
#define AE_NAV_LINK_COUNT 0xAu /* u8: total links, including inactive links */
#define AE_NAV_ACTIVE_LINK_COUNT 0xBu /* u8: active link count */
#define AE_NAV_LINKS 0xCu /* u16*: sorted link entries */
#define AE_NAV_LINK_CODE 0x10u /* s16: packed object/link code */
#define AE_NAV_LINK_EXTRA_CODE 0x12u /* s16: second packed link code */
#define AE_MK_INFO_WALK_SPEED 0x2u /* s16: unalerted movement speed */
#define AE_MK_INFO_ALERT_SPEED 0x4u /* s16: alerted speed; also gates sight and dust */
#define AE_MK_INFO_ATTACK_CHANCES 0x20u /* u16[]: attack chances at an eight-byte stride */
#define AE_MK_INFO_LOD_ATTACK_CHANCE 0x58u /* u16: attack chance when model detail is reduced */
#define AE_MK_INFO_ATTACK_COUNT_7 0x56u /* u16: shot count for attack kind 7 */
#define AE_MK_OBJ_FLAGS 0xE0u /* u32: object flags */
#define AE_MK_OBJ_MOVE_DIR 0x60u /* SVECTOR: object facing */
#define AE_MK_OBJ_BONE_MATRICES 0x14u /* MATRIX*: object bone transforms */
#define AE_MK_OBJ_VERTS 0x10u /* SVECTOR*: object vertices */
#define AE_MK_MATRIX_TRANS_X 0x14u /* s32: matrix translation x */
#define AE_MK_MATRIX_TRANS_Y 0x18u /* s32: matrix translation y */
#define AE_MK_MATRIX_TRANS_Z 0x1Cu /* s32: matrix translation z */
#define AE_G_MODEL_VERTICES 0x800E7D00u /* model vertex buffer */
#define AE_G_MODEL_BONE_BUFFER 0x800E9C60u /* model bone matrix buffer */
#define AE_G_VOICE_QUEUE_FULL_TEXT 0x800A4900u /* voice queue full diagnostic format */
#define AE_G_MONKEY_DEBUG_MENU 0x800B000Cu /* debug menu descriptor */
#define AE_G_MONKEY_COUNT_ITEM 0x800AEC20u /* monkey count menu item */
#define AE_G_MONKEY_INFO_ITEMS 0x800AEC8Cu /* monkey info menu items */
#define AE_G_MONKEY_MENU_CALLBACK 0x80043358u /* debug menu change callback */

#define AE_MK_INFO_IDLE_CHANCE 0x8u /* s16: idle decision probability */
#define AE_MK_INFO_SIGHT_RANGES 0xCu /* s16[]: sight ranges by target movement kind */
#define AE_MK_INFO_ALERT_DISTANCE 0xEu /* s16: distance threshold for alert transition */
#define AE_MK_INFO_IDLE_DISTANCE 0x12u /* s16: idle distance threshold */
#define AE_MK_INFO_SIGHT_RANGE 0x1Au /* s16: fallback sight range */
#define AE_MK_INFO_ATTACK_COUNT 0x1Eu /* u16: count for one attack kind */
#define AE_MK_INFO_ATTACK_MIN_RANGE 0x22u /* s16: minimum attack distance */
#define AE_MK_INFO_ATTACK_MAX_RANGE 0x24u /* s16: maximum attack distance */

#define AE_MK_BONE_MATRICES 0x14u /* MATRIX*: actor bone transforms */
#define AE_MK_IDLE_ACTIVITY_TIMER 0x98u /* u16: timer of the looping idle activity */
#define AE_MK_ALERT_ICON_KIND 0x14Cu /* u32: previous alert icon kind */
#define AE_MK_CONTACT_OWNER_KIND 0x24u /* u16: contact owner kind flags; distinct from actor position */
#define AE_MK_AREA_ANIM_TABLE 0x18u /* area header: animation table offset */
#define AE_MK_AREA_ALT_ANIM_TABLE 0x1Cu /* area header: alternate animation table offset */
#define AE_MK_ROUTE_NODES 0x88u /* route table: goal node indices */
#define AE_MK_DRAW_INDEX 0xCu /* draw-context base: buffer index */
#define AE_MK_DRAW_CONTEXTS 0x80u /* draw-context base: rigid model contexts */
#define AE_MK_DRAW_CONTEXTS_WITH_OT 0x90u /* draw-context base: model contexts with ordering table */
#define AE_MK_VOICE_TIMER 0x4u /* voice queue entry: signed timer */
#define AE_MK_VOICE_POS 0x8u /* voice queue entry: s32[3] position */
#define AE_MK_MENU_COUNT_LINK 0x2Cu /* menu descriptor: monkey count item */
#define AE_MK_MENU_INFO_LINK 0x50u /* menu descriptor: monkey info items */
#define AE_MK_INFO_RANGES 0xCu /* MonkeyInfo: debug range fields */
#define AE_MK_ANIM_FIRST_FRAME 0x0u /* animation record: first frame */
#define AE_MK_ANIM_LAST_FRAME 0x2u /* animation record: last frame */
#define AE_NAV_EXTRA_HIGH_NODE 0x2u /* extra link entry: high node index */
#define AE_NAV_EXTRA_FLAGS 0x4u /* extra link entry: flags word */
#define AE_NAV_EXTRA_VALUE 0x8u /* extra link entry: extra value */
#define AE_G_MONKEY_ATTACK_TABLE 0x800AEBB4u /* attack descriptors: multiplier, state, model, flags */
#define AE_G_MONKEY_ATTACK_FLAGS 0x800AEBB8u /* flags in attack descriptors */
#define AE_G_MONKEY_HELD_MODELS 0x800F4630u /* held-model instances, 16 bytes each */
#define AE_G_MONKEY_BOSS_MODEL 0x800F46B0u /* area 0x46 extra model */
#define AE_G_MONKEY_EXTRA_MODEL_A 0x800EC180u /* area 0x24 flying model */
#define AE_G_MONKEY_EXTRA_MODEL_B 0x800EC0C0u /* area 0x24 ground model */
#define AE_G_MONKEY_GROUP_MODELS 0x800AEBF8u /* model index by caught-group value */
#define AE_G_MONKEY_GOAL_TEXT 0x800A4964u /* goal selection diagnostic format */
#define AE_G_MONKEY_ROUTE_TEXT 0x800A4988u /* route selection diagnostic format */
#define AE_G_MONKEY_SELECT_TEXT 0x800A499Cu /* node selection diagnostic format */
#define AE_G_MONKEY_ID_TEXT 0x800A4BBCu /* monkey ID diagnostic format */
#define AE_G_MONKEY_ANIM_ANGLES 0x800F4B30u /* three SVECTORs: animation rotation angles */
#define AE_G_SHADOW_CONTEXT 0x1F8003FCu /* scratchpad: shadow renderer context */
#define AE_G_MONKEY_BOSS_MODE 0x800F447Cu /* u8: boss decision mode */
#define AE_G_MONKEY_TURN_TABLE 0x800A49B8u /* turn-step dispatch table */
#define AE_G_MONKEY_SIGHT_TABLE 0x800A4A98u /* sight-pattern dispatch table */
#define AE_G_MONKEY_ICON_MATRIX 0x800F47F0u /* MATRIX: alert icon placement */
#define AE_MK_ANIM_ANGLE_SECOND 0x8u /* animation rotations: second SVECTOR */
#define AE_MK_ANIM_ANGLE_THIRD 0x10u /* animation rotations: third SVECTOR */

#define AE_MK_MODEL_BONE_COUNT 4u /* ModelHeader: u16 bone count */
#define AE_NAV_POS 4u /* NavNode: s16[3] position */
#define AE_NAV_REF_Y_OFFSET 8u /* ground-reference record: s16 vertical offset subtracted from y */
#define AE_MK_TARGET_STATE_FLAGS 0x70u /* u32: target actor state flags (Player.stateFlags); low nibble selects sight range */
#define AE_MK_INFO_IDLE_DURATION 0xAu /* s16: maximum idle-wait duration */
#define AE_MK_MENU_ITEM_SIZE 36u /* bytes per debug menu item */

/* ---- scratch "stack" areas for arguments the original passes by stack pointer ------------------- */
/* Inside the top-of-RAM stack window (0x801FF000..), which the check scripts exclude from comparison; one block
   per function so nothing nests.  Add new blocks at the end. */
#define AE_STK(n) (0x801FF000u + (uint32_t)(n) * 0x40u)
enum {
    AE_STK_GIMMICK_PASSAGE = 0,   /* GimmickAllowsMonkeyPassage: GimmickGetInfo out (s16 type, s16 value) */
    AE_STK_DELAYED_SFX,           /* DelayedSfxUpdate: three s16 position */
    AE_STK_SPAWN_PROJECTILE,      /* MonkeySpawnProjectile: s16[3] position @+0, s16[3] offset @+8 */
    AE_STK_PUFF14,                /* MonkeyPuff14: three s16 position */
    AE_STK_PUFF5,                 /* MonkeyPuff5: three s16 position */
    /* batch 2 */
    AE_STK_AIM_TARGET,            /* MonkeyAimAtTarget: s16[3] position @+16, s16[3] direction @+24 */
    AE_STK_FIRE_AIMED,            /* MonkeyFireProjectileAimed: @+16 position, @+24 offset */
    AE_STK_FIRE_8,                /* MonkeyFireProjectile8 */
    AE_STK_ANGLE_TO_TARGET,       /* MonkeyAngleToTarget: s32[3] @+16 and @+32 */
    AE_STK_NAV_NODE_POS,          /* NavNodeGetPos: s16[3] @+16, three u16 link ids @+24 */
    AE_STK_FOOT_DUST,
    AE_STK_DEBRIS6,
    AE_STK_DEBRIS1,
    AE_STK_ALERT_PUFF,
    AE_STK_PROJECT,               /* MonkeyProjectToScreen */
    AE_STK_MOVE_STEP,             /* MonkeyMoveStep: s16 vectors @+16, +24, +32 */
    AE_STK_TURN,                  /* MonkeyTurnTowardTarget: s16[3] @+16, MATRIX @+24 (+0x14 bytes) */
    AE_STK_LINE_OF_SIGHT,         /* MonkeyLineOfSight: s16[3] @+16, s32 @+24 / +28 / +36 / +40, bytes @+32 / +33 */
    /* batch 3 */
    AE_STK_ENTER_32,              /* MonkeyEnterState_32: s16 link code @+24 */
    AE_STK_LINK_SLOPE,            /* MonkeyUpdateLinkSlope: positions @+16 / +32 */
    AE_STK_GOAL_POS,              /* (unused: MonkeyUpdateGoalPos shares AE_STK_LINK_SLOPE, see there) */
    AE_STK_JUMP_ARC,              /* MonkeyPrepareJumpArc: node pos @+16 */
    AE_STK_NAV_LINK_ALLOWED,      /* MonkeyNavLinkAllowed: node pos @+16, own pos @+32, extra value @+48 */
    AE_STK_ALERT_ICON,            /* MonkeyDrawAlertIcon: s16[3] @+16 */
    AE_STK_SCORE_LINKS,           /* MonkeyScoreLinksByTarget: pos @+16, dir to target @+32, to node @+40 */
    AE_STK_NODE_COSINES,          /* MonkeyNodeTargetCosines: @+16, @+24, @+32 */
    AE_STK_WAYPOINT,              /* MonkeyGetWaypointPos: @+16, @+32 */
    AE_STK_UPDATE_FACING,         /* MonkeyUpdateFacing: s16 @+16 / +24, positions @+32 / +48 */
    AE_STK_ENTER_1E,              /* MonkeyEnterState_1E: node pos @+16, extra value @+32 */
    AE_STK_KNOCKDOWN,             /* MonkeyEnterKnockdown: s16[3] @+16 / +24 */
    AE_STK_ENTER_2F,              /* MonkeyEnterState2F: s16[3] @+16 */
    AE_STK_ENTER_30,              /* MonkeyEnterState_30: s16[3] @+16 */
    AE_STK_ENTER_20,              /* MonkeyEnterState_20: s16[3] @+16 */
    AE_STK_ENTER_09,              /* MonkeyEnterState_09: extra value @+16 */
    AE_STK_DRAW_MODEL,            /* MonkeyDrawModel: VECTOR (3 x s32) @+16 */
    /* batch 4 */
    AE_STK_ENTER_1D,              /* MonkeyEnterState_1D: NavLinkLookupExtra out word @+16 */
    AE_STK_ARRIVE,                /* MonkeyArriveAtNode: three s16 link ids @+16 */
    AE_STK_PICK_LINK,             /* MonkeyPickBestLink: s16 candidate list @+16 (3 blocks: the frame is 168 bytes) */
    AE_STK_PICK_LINK_1,
    AE_STK_PICK_LINK_2,
    AE_STK_TURN_BACK,             /* MonkeyConsiderTurnBack: s16[3] @+16, s16[3] @+24, node position (s32[3]) @+32 */
    AE_STK_TURN_BACK_RANDOM,      /* MonkeyConsiderTurnBackRandom: two s16 cosines @+16 / +18 */
    AE_STK_SELECT_NEXT,           /* MonkeySelectNextNode: saved node history u16[6] @+16, candidate list s32[10] @+32 (2 blocks) */
    AE_STK_SELECT_NEXT_1,
    AE_STK_GOTO_FLAGGED,          /* MonkeyAi_GoToFlaggedNode: node position s32[3] @+16 */
    /* batch 5 */
    AE_STK_MOVE_UNALERTED,        /* MonkeyAi_MoveUnalerted: two node positions (s32[3]) @+16 / +32 */
    AE_STK_MONKEY_INIT,           /* MonkeyInit: node position @+16 (four words are copied: sp + 28 is never written) */
    AE_STK_JUMP_ARC_AI,           /* MonkeyAi_JumpArc / JumpArcPhased: node position @+16 */
    AE_STK_PATROL,                /* MonkeyAi_Patrol: node positions @+16 / +32 */
    AE_STK_SELECT_ACTIVE,         /* MonkeySelectActive: saved ids u16[4] @+16 */
    AE_STK_AI_STATE_UPDATE,       /* MonkeyAiStateUpdate: node position @+16, MATRIX @+32, particle position @+64 / +72 (2 blocks) */
    AE_STK_AI_STATE_UPDATE_1
};

/* ---- hook table ------------------------------------------------------------------------------ */
typedef int32_t (*AeMonkeyHookFn)(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

typedef struct AeMonkeyHooks {
    void *user;
    /* collision-body hooks for the ae_CollisionBody* ports that are called directly (may be NULL: those hooks are
       only reached on error paths / body type 2) */
    const AeBodyHooks *body;
    /* ---- non-monkey code (stays a hook: owned by other modules) ---- */
    AeMonkeyHookFn Printf;                /* 800A3720 (a0 = format address) */
    AeMonkeyHookFn GimmickGetInfo;        /* 8005B7BC (a0 = gimmick, a1 = out: s16 type, s16 value); -1 = none */
    AeMonkeyHookFn PlaySfxAt;             /* 80087A20 (a0 = sfx, a1 = s16[3] position) */
    AeMonkeyHookFn ProjectileSpawn;       /* 80059304 (a0 = s16[3] pos, a1 = monkey, a2 = type, a3 = s16[3] / counter) */
    AeMonkeyHookFn SpawnParticle;         /* 800698B8 (a0 = type, a1 = s16[3] position) */
    AeMonkeyHookFn TargetListAdd;         /* 8007CD88 (a0 = pointer) */
    AeMonkeyHookFn TargetListRemove;      /* 8007CDC4 (a0 = pointer) */
    AeMonkeyHookFn MatrixFromDirXZ;       /* 80014804 (a0 = MATRIX*, a1 = dir) */
    AeMonkeyHookFn ModelPoseBones;        /* 80015780 (a0 = model, a1 = flags) */
    AeMonkeyHookFn ModelTransformRigid;   /* 800161C0 (a0 = obj, a1 = bone data, a2 = vertex buffer) */
    AeMonkeyHookFn ModelDraw;             /* 80017CD4 (a0 = draw context, a1 = vertex buffer, a2 = obj) */
    AeMonkeyHookFn MenuItemInitBound;     /* 8007D1E0 (a0 = item, a1 = bound variable, a2 = signed flag) */
    AeMonkeyHookFn MenuItemSetRange;      /* 8007D290 (a0 = item, a1 = min, a2 = max) */
    AeMonkeyHookFn MenuItemSetCallback;   /* 8007D29C (a0 = item, a1 = callback) */
    AeMonkeyHookFn MenuItemSetStep;       /* 8007D2A4 (a0 = item, a1 = step) */
    AeMonkeyHookFn EffectBigBurst;        /* 8006B3F0 (a0 = x, a1 = y, a2 = z, sign extended s16) */
    AeMonkeyHookFn SparkAddRandom;        /* 8006E0DC (a0 = s16[3] position, a1 = s16[3] velocity, a2 = mode) */
    AeMonkeyHookFn ModelInit;             /* 80015AC0 (a0 = monkey, a1 = model header) */
    AeMonkeyHookFn SetRotMatrix;          /* 8002FB00 (a0 = MATRIX*): loads GTE rotation registers */
    AeMonkeyHookFn SetTransMatrix;        /* 8002FB30 (a0 = MATRIX*): loads GTE translation registers */
    AeMonkeyHookFn RotTransPers;          /* 80031330 (a0 = SVECTOR*, a1 = s16[2] screen xy out, a2 = u32 out, a3 = u32 flag out); returns OTZ */
    AeMonkeyHookFn GetGroundHeight;       /* 80043400 (a0 = x, a1 = y, a2 = z): floor height below the point */
    AeMonkeyHookFn ObjSlotGetAnimIndex;   /* 80052C4C (a0 = object slot, sign extended s16) */
    AeMonkeyHookFn AlertIconDraw;         /* 8006ABF0 (a0 = s16[3] position, a1 = icon kind 0..2, a2 = flag) */
    AeMonkeyHookFn ObjSlotSetField_CF;    /* 80052CA0 (a0 = object slot, sign extended s16, a1 = 4) */
    AeMonkeyHookFn ObjSlotGetTarget;      /* 80052D04 (a0 = object slot, a1 = position out s32[3]) */
    AeMonkeyHookFn SoundLoopStartEx;      /* 800882C4 (a0 = 1, a1 = sound, a2 = monkey id); returns the loop handle */
    AeMonkeyHookFn SoundLoopStopAndFree;  /* 800880D0 (a0 = sound) */
    AeMonkeyHookFn SpawnPuff;             /* 8006B3CC (a0 = s16[3] position) */
    AeMonkeyHookFn MatrixLookDir;         /* 8001484C (a0 = direction s16[3], a1 = MATRIX*) */
    AeMonkeyHookFn ScaleMatrix;           /* 80030E60 (a0 = MATRIX*, a1 = VECTOR scale) */
    AeMonkeyHookFn ModelBuildBoneMatrices;/* 80015504 (a0 = monkey object, a1 = 3) */
    AeMonkeyHookFn ModelTransformSkinned; /* 80016A60 (a0 = monkey object) */
    AeMonkeyHookFn ModelDrawWithOt;       /* 80017D94 (a0 = draw context, a1 = vertex buffer, a2 = monkey, a3 = icon / ot offset) */
    /* batch 4 */
    AeMonkeyHookFn GimmickEvent;          /* 8005B880 (a0 = event 0 / 1 / 2, a1 = gimmick id (sign extended s16), a2 = 0) */
    AeMonkeyHookFn ActorAnimDecode;       /* 80014E34 (a0 = actor) */
    AeMonkeyHookFn DrawShadow;            /* 8001F604 (a0 = position s32[3], a1 = ground y, a2 = size, a3 = scratchpad word 0x1F8003FC) */
    AeMonkeyHookFn ColorFadeSet;          /* 8008C3C0 (a0 = s16, a1 = s16) */
    AeMonkeyHookFn DebugTextPrintfWhite;  /* 8008CBFC (a0 = format address, a1 = monkey id) */
    /* batch 5 */
    AeMonkeyHookFn ClearWords;            /* 80032708 (a0 = address, a1 = number of 32 bit words to zero) */
    AeMonkeyHookFn VSync;                 /* 8002E520 (a0 = mode, -1 = the vertical blank counter) */
    AeMonkeyHookFn StageExitIsActive;     /* 8005DCE4 (no arguments) */
    /* ---- monkey-range functions of later batches (replace by the direct call when ported) ---- */
} AeMonkeyHooks;

/* Calls hook NAME with up to four arguments (unused ones 0).  NULL hook: error flag + 0. */
int32_t ae_monkey_hook_missing(AeRam *r, const char *name);
#define AE_MK_HOOK(r, h, NAME, a0, a1, a2, a3) \
    (((h) && (h)->NAME) ? (h)->NAME((h)->user, (r), (uint32_t)(a0), (uint32_t)(a1), (uint32_t)(a2), (uint32_t)(a3)) \
                        : ae_monkey_hook_missing((r), #NAME))

/* ====================================================================================================
 * Batch 1 (call-graph level 0): ae_monkey_base.c
 * ================================================================================================== */
/* ---- random / small queries ---- */
uint32_t ae_MonkeyRand(AeRam *r);                                    /* 80032E30: (seed * 5 + 1) & 0xFFFF, returns seed >> 8 */
uint32_t ae_MonkeyCaughtGroupValue(AeRam *r);                        /* 80032E58 */
int32_t ae_MonkeyGetAnimIndex(AeRam *r);                             /* 80032E84 */
int32_t ae_MonkeyNop(AeRam *r);                                      /* 80033000: returns 2 */
int32_t ae_MonkeyBodyRegister(AeRam *r, const AeMonkeyHooks *h);     /* 80033008 */
int32_t ae_MonkeyBodyUnregister(AeRam *r, const AeMonkeyHooks *h);   /* 80033068 */
void ae_MonkeyActivityReset(AeRam *r);                               /* 8003317C */
void ae_MonkeyStoreActivities(AeRam *r);                             /* 800334C8 */
int32_t ae_GimmickAllowsMonkeyPassage(AeRam *r, const AeMonkeyHooks *h, uint32_t gimmick); /* 800335D0 */
void ae_MonkeyVoiceQueueReset(AeRam *r);                             /* 80033708 */
void ae_MonkeyQueueVoice(AeRam *r, const AeMonkeyHooks *h, int32_t sfx, int32_t timer);    /* 80033744 */
void ae_DelayedSfxUpdate(AeRam *r, const AeMonkeyHooks *h);          /* 800337C4 */
int32_t ae_NavNodeGetLink(AeRam *r, int32_t node, int32_t index);    /* 8003386C */
int32_t ae_AreaIsSpecialMonkeyArea(AeRam *r);                        /* 8003398C */
int32_t ae_MonkeyIsState1E_to_20(AeRam *r);                          /* 800339CC */
int32_t ae_AreaIsBossArena(AeRam *r);                                /* 800339EC */
int32_t ae_MonkeyIsIdleActivityState(AeRam *r);                      /* 80033B10 */
int32_t ae_MonkeyIsIdleEndState(AeRam *r);                           /* 80033B9C */
int32_t ae_MonkeyIsAltAnimState(AeRam *r);                           /* 80033BE0 */
int32_t ae_MonkeyIsState0C_or_24(AeRam *r);                          /* 80033C00 */
int32_t ae_MonkeyIsJumpArcState(AeRam *r);                           /* 80033C34 */
int32_t ae_MonkeyIsAttackState(AeRam *r);                            /* 80033C80 */
int32_t ae_MonkeySpawnProjectile(AeRam *r, const AeMonkeyHooks *h, int32_t type);          /* 80033CCC */
/* ---- vector maths ---- */
int32_t ae_AngleBetweenXZ(AeRam *r, uint32_t a, uint32_t b);         /* 800344C8: s32[3] vectors */
int32_t ae_DistanceIntToShort3D(AeRam *r, uint32_t a, uint32_t b);   /* 800345D4: (s32[3], s16[3]); XZ distance only (Y is never squared) */
int32_t ae_Distance3D(AeRam *r, uint32_t a, uint32_t b);             /* 80034684 */
int32_t ae_DistanceXZ(AeRam *r, uint32_t a, uint32_t b);             /* 8003473C */
void ae_MonkeyCalcMoveDir(AeRam *r);                                 /* 800347D8 */
int32_t ae_DotXZ_SV(AeRam *r, uint32_t a, uint32_t b);               /* 800348CC: s16[3] vectors */
int32_t ae_DotXZ_V(AeRam *r, uint32_t a, uint32_t b);                /* 80034940: s32[3] vectors */
int32_t ae_Atan2Angle4096(AeRam *r, int32_t y, int32_t x);           /* 800349B4 */
/* ---- navigation nodes ---- */
int32_t ae_NavNodeIsSpecial(AeRam *r, int32_t node);                 /* 800349F4 */
void ae_NavNodeGetLinkIds(AeRam *r, int32_t node, uint32_t o1, uint32_t o2, uint32_t o3);  /* 80034B74 */
void ae_NavNodeGetLinkIdsEx(AeRam *r, int32_t node, uint32_t o1, uint32_t o2, uint32_t o3, uint32_t o4); /* 80034C08 (o4 = 5th argument, on the stack) */
int32_t ae_ObjGetBonePos(AeRam *r, const AeMonkeyHooks *h, int32_t obj, uint32_t matrix_index, uint32_t bone, uint32_t out); /* 80034D30 */
int32_t ae_MonkeyReachedNode(AeRam *r);                              /* 8003516C */
int32_t ae_MonkeyNavBacktracked(AeRam *r);                           /* 80035604 */
int32_t ae_MonkeyGetCallVoice(AeRam *r);                             /* 800356C0 */
int32_t ae_MonkeyGetCallVoice2(AeRam *r);                            /* 80035750 */
void ae_MonkeyFacingFromLinkCode(AeRam *r, int32_t code);            /* 80036BD0 */
int32_t ae_MonkeyAttackChance(AeRam *r, int32_t index);              /* 80037068 */
int32_t ae_MonkeyApproachAngleNear(AeRam *r);                        /* 80037D6C */
int32_t ae_MonkeyApproachAngleFar(AeRam *r);                         /* 80037DEC */
int32_t ae_NavNodeLinkFlag(AeRam *r, int32_t node, int32_t link);    /* 80038608 */
int32_t ae_MonkeyPuff14(AeRam *r, const AeMonkeyHooks *h);           /* 80039200 */
int32_t ae_MonkeyPuff5(AeRam *r, const AeMonkeyHooks *h);            /* 8003929C */
int32_t ae_StaticModelDraw(AeRam *r, const AeMonkeyHooks *h, uint32_t obj);                /* 80039E50 */
int32_t ae_MonkeyStub_ReturnZero(AeRam *r);                          /* 8003A334 */
void ae_MonkeyInterpolateLinkVector(AeRam *r);                       /* 8003C568 */
void ae_MonkeyResetNavVisits(AeRam *r);                              /* 800407F0 */
int32_t ae_NavGroundHeightBetween(AeRam *r, uint32_t a, uint32_t b); /* 80040BDC */
int32_t ae_ActorContactOwnerType(AeRam *r);                          /* 80041280 */
void ae_MonkeyDebugMenuInit(AeRam *r, const AeMonkeyHooks *h);       /* 80043060 */
void ae_MonkeyDebugMenuInit2(AeRam *r, const AeMonkeyHooks *h, int32_t monkey);            /* 800430F0 */
/* BodyContactType @800392E4 is ae_BodyContactType in src/bodies (already ported and diffed). */

/* ====================================================================================================
 * Batch 2 (call-graph level 1: callees are batch 1 only): ae_monkey_level1.c
 * ================================================================================================== */
/* ---- animation ---- */
int32_t ae_MonkeyRandomHesitate(AeRam *r);                           /* 80032F24 */
void ae_MonkeyAdvanceAnimFrame(AeRam *r);                            /* 800330C8 */
void ae_MonkeyPickAnim(AeRam *r);                                    /* 800331B0 */
int32_t ae_MonkeyAnimFinished(AeRam *r);                             /* 80033A3C */
int32_t ae_MonkeyAnimAtStart(AeRam *r);                              /* 80033ACC */
/* ---- projectiles / aiming ---- */
void ae_MonkeyFireProjectile6(AeRam *r, const AeMonkeyHooks *h);    /* 80033DF0 */
void ae_MonkeyAimAtTarget(AeRam *r, const AeMonkeyHooks *h, int32_t arg);   /* 80033E4C */
void ae_MonkeyFireProjectileAimed(AeRam *r, const AeMonkeyHooks *h, int32_t arg);   /* 8003429C */
void ae_MonkeyFireProjectile7(AeRam *r, const AeMonkeyHooks *h);    /* 800343B0 */
void ae_MonkeyFireProjectile8(AeRam *r, const AeMonkeyHooks *h);    /* 8003440C */
int32_t ae_MonkeyAngleToTarget(AeRam *r);                            /* 8003455C */
void ae_MonkeyApplyMoveDir(AeRam *r);                                /* 80034834 */
void ae_NavNodeGetPos(AeRam *r, const AeMonkeyHooks *h, int32_t node, uint32_t out);   /* 80034EF8 */
int32_t ae_MonkeyTargetInSightRange(AeRam *r);                       /* 800361B4 */
void ae_MonkeyBodyOff(AeRam *r, const AeMonkeyHooks *h);             /* 80036B24 */
void ae_MonkeyBodyOn(AeRam *r, const AeMonkeyHooks *h);              /* 80036B78 */
void ae_MonkeyDefeatBurst(AeRam *r, const AeMonkeyHooks *h);         /* 80036C84 */
/* ---- particles / contact / screen ---- */
void ae_MonkeyFootDust(AeRam *r, const AeMonkeyHooks *h);            /* 80038F00 */
void ae_MonkeyDebrisBurst6(AeRam *r, const AeMonkeyHooks *h);        /* 80038FD8 */
void ae_MonkeyDebrisBurst1(AeRam *r, const AeMonkeyHooks *h);        /* 800390F8 */
void ae_MonkeyAlertPuff(AeRam *r, const AeMonkeyHooks *h);           /* 80039248 */
int32_t ae_MonkeyUpdateContact(AeRam *r);                            /* 800392F0 */
int32_t ae_MonkeyProjectToScreen(AeRam *r, const AeMonkeyHooks *h);  /* 800393E4: 1 = off screen */
int32_t ae_MonkeyAlertIconKind(AeRam *r);                            /* 80039B90 */
void ae_MonkeySelectLod(AeRam *r, const AeMonkeyHooks *h);           /* 8003A33C */
/* ---- navigation / movement ---- */
void ae_MonkeyPickGoalNode(AeRam *r, const AeMonkeyHooks *h);        /* 8003AD9C */
void ae_MonkeyLoadLinkExtra(AeRam *r, int32_t from, int32_t to);     /* 8003B674 */
int32_t ae_NavLinkLookupExtra(AeRam *r, int32_t from, int32_t to, uint32_t out);   /* 8003B880 */
void ae_MonkeyMoveStep(AeRam *r, const AeMonkeyHooks *h);            /* 8003B9C0 */
void ae_MonkeyTurnTowardTarget(AeRam *r, int32_t rate);              /* 8003CC3C */
int32_t ae_MonkeyLineOfSight(AeRam *r, const AeMonkeyHooks *h, int32_t mode);   /* 800401F4 */

/* ====================================================================================================
 * Batch 3a (call-graph level 2: callees are batch 1 / 2 functions): ae_monkey_level2.c
 * ================================================================================================== */
void ae_MonkeySetState(AeRam *r, uint32_t state);                                         /* 80033220 */
void ae_MonkeyUpdateLinkSlope(AeRam *r, const AeMonkeyHooks *h);                          /* 80035370 */
void ae_MonkeyUpdateGoalPos(AeRam *r, const AeMonkeyHooks *h);                            /* 80035474 */
void ae_MonkeyEnterState_32(AeRam *r, uint32_t node);                                     /* 80035918 */
void ae_MonkeyEnterState_3D(AeRam *r, const AeMonkeyHooks *h);                            /* 800360D8 */
void ae_MonkeyEnterState_3E(AeRam *r);                                                    /* 8003612C */
void ae_MonkeyEnterState_3F(AeRam *r);                                                    /* 80036174 */
void ae_MonkeyEnterState_17(AeRam *r, const AeMonkeyHooks *h);                            /* 8003641C */
void ae_MonkeyEnterState_2A(AeRam *r, const AeMonkeyHooks *h);                            /* 80036460 */
void ae_MonkeyPrepareJumpArc(AeRam *r, const AeMonkeyHooks *h);                           /* 800364BC */
int32_t ae_MonkeyTryAttackKind(AeRam *r, const AeMonkeyHooks *h, int32_t kind);           /* 8003710C */
int32_t ae_MonkeyNavLinkAllowed(AeRam *r, const AeMonkeyHooks *h, int32_t from, int32_t to); /* 80037E8C */
void ae_MonkeyDrawAlertIcon(AeRam *r, const AeMonkeyHooks *h);                            /* 80039C38 */
void ae_MonkeyScoreLinksByTarget(AeRam *r, const AeMonkeyHooks *h, int32_t node);         /* 8003AC54 */
void ae_MonkeyNodeTargetCosines(AeRam *r, const AeMonkeyHooks *h, uint32_t out0, uint32_t out1); /* 8003B534 */
void ae_MonkeyUpdateFacing(AeRam *r, const AeMonkeyHooks *h);                             /* 8003C204 */
void ae_MonkeyFireProjectileByKind(AeRam *r, const AeMonkeyHooks *h, int32_t kind);       /* 8003CD24 */
int32_t ae_MonkeyUpdateSight(AeRam *r, const AeMonkeyHooks *h, uint32_t pattern);         /* 800405C4 */
int32_t ae_MonkeyGetWaypointPos(AeRam *r, const AeMonkeyHooks *h, int32_t which);         /* 80040CFC */

/* ====================================================================================================
 * Batch 3b (call-graph level 3: callees are batch 1 / 2 / 3a functions): ae_monkey_level3.c
 * ================================================================================================== */
void ae_MonkeyRestoreActivities(AeRam *r, const AeMonkeyHooks *h);                        /* 800332F8 */
void ae_MonkeyUpdateGoal(AeRam *r, const AeMonkeyHooks *h);                               /* 800355DC */
void ae_MonkeyStartNotice(AeRam *r, const AeMonkeyHooks *h);                              /* 800357E0 */
int32_t ae_MonkeyEnterState_1E(AeRam *r, const AeMonkeyHooks *h);                         /* 8003581C: returns 1 */
void ae_MonkeyEnterState_34(AeRam *r);                                                    /* 800359B4 */
void ae_MonkeyEnterKnockdown(AeRam *r, const AeMonkeyHooks *h, int32_t state);            /* 80035BD0 */
void ae_MonkeyEnterState2F(AeRam *r, const AeMonkeyHooks *h);                             /* 80035D18 */
void ae_MonkeyEnterState_30(AeRam *r, const AeMonkeyHooks *h);                            /* 80035DD8 */
void ae_MonkeyEnterState_36(AeRam *r, const AeMonkeyHooks *h);                            /* 80035E6C */
void ae_MonkeyEnterState_20(AeRam *r, const AeMonkeyHooks *h);                            /* 80035EA4 */
void ae_MonkeyEnterState_24(AeRam *r, const AeMonkeyHooks *h);                            /* 80035F1C */
void ae_MonkeyEnterState_0C(AeRam *r, const AeMonkeyHooks *h);                            /* 80035F54 */
void ae_MonkeyEnterState_35(AeRam *r);                                                    /* 80035F8C */
void ae_MonkeyEnterState_1A(AeRam *r);                                                    /* 80035FBC */
void ae_MonkeyEnterState_09(AeRam *r);                                                    /* 8003601C */
void ae_MonkeyEnterIdleWait(AeRam *r);                                                    /* 80036300 */
void ae_MonkeyResumeIdle(AeRam *r);                                                       /* 800363A4 */
void ae_MonkeyEnterRandomLook(AeRam *r);                                                  /* 800363E4 */
int32_t ae_MonkeyPickAttackA(AeRam *r, const AeMonkeyHooks *h);                           /* 8003738C: attack kind or 100 */
int32_t ae_MonkeyPickAttackB(AeRam *r, const AeMonkeyHooks *h);                           /* 80037420 */
int32_t ae_MonkeyPickAttackC(AeRam *r, const AeMonkeyHooks *h);                           /* 800374B8 */
void ae_MonkeyMarkBlockedLinks(AeRam *r, const AeMonkeyHooks *h, int32_t node);           /* 80038054 */
void ae_MonkeyUpdateTargetDistance(AeRam *r, const AeMonkeyHooks *h);                     /* 80039B30 */
void ae_MonkeyDrawModel(AeRam *r, const AeMonkeyHooks *h);                                /* 80039ED4 */
void ae_MonkeyAi_State3E(AeRam *r);                                                       /* 8003FC08 */
void ae_MonkeyNavVisitUpdate(AeRam *r, const AeMonkeyHooks *h);                           /* 8004086C */

/* ====================================================================================================
 * Batch 4a (call-graph level 4): ae_monkey_level4.c
 * ================================================================================================== */
void ae_MonkeySetNavNode(AeRam *r, const AeMonkeyHooks *h, int32_t node);                 /* 80035080 */
int32_t ae_MonkeyEnterState_1D(AeRam *r, const AeMonkeyHooks *h);                         /* 800359F0: returns 1 */
int32_t ae_MonkeyEnterState_04(AeRam *r, const AeMonkeyHooks *h);                         /* 80035A7C: returns 1 */
int32_t ae_MonkeyEnterState_2C(AeRam *r, const AeMonkeyHooks *h);                         /* 80036964: returns 1 */
int32_t ae_MonkeyPickBestLink(AeRam *r, const AeMonkeyHooks *h, int32_t limit);           /* 80038120 */
int32_t ae_MonkeySwapNavNodes(AeRam *r, const AeMonkeyHooks *h);                         /* 80038694: returns the $v0 of UpdateGoalPos (Distance3D) */
void ae_MonkeyAi_Fall(AeRam *r, const AeMonkeyHooks *h);                                  /* 8003FCD8 */
int32_t ae_MonkeyArriveAtNode(AeRam *r, const AeMonkeyHooks *h, int32_t node);            /* 80040D90 */
void ae_MonkeyUpdateVisuals(AeRam *r, const AeMonkeyHooks *h);                            /* 80042B68 */

/* ====================================================================================================
 * Batch 4b (call-graph level 5): ae_monkey_level5.c
 * ================================================================================================== */
int32_t ae_MonkeyNodeActionDispatch(AeRam *r, const AeMonkeyHooks *h);                    /* 80036CD0: 1 = a state was entered */
int32_t ae_MonkeyConsiderTurnBack(AeRam *r, const AeMonkeyHooks *h);                      /* 800386C8 */
void ae_MonkeyConsiderTurnBackRandom(AeRam *r, const AeMonkeyHooks *h);                   /* 80038914 */

/* ====================================================================================================
 * Batch 4c (call-graph level 6): ae_monkey_level6.c
 * ================================================================================================== */
void ae_MonkeyAlertedDecide(AeRam *r, const AeMonkeyHooks *h);                            /* 80037550 */
int32_t ae_MonkeyChooseNextNodeA(AeRam *r, const AeMonkeyHooks *h, int32_t p1, int32_t p2); /* 80038A1C */
int32_t ae_MonkeyChooseNextNodeB(AeRam *r, const AeMonkeyHooks *h, int32_t p1, int32_t p2); /* 80038A9C */

/* ====================================================================================================
 * Batch 4d (call-graph level 7): ae_monkey_level7.c
 * ================================================================================================== */
void ae_MonkeyPatrolDecide(AeRam *r, const AeMonkeyHooks *h);                             /* 80037908 */
int32_t ae_MonkeySelectNextNode(AeRam *r, const AeMonkeyHooks *h, int32_t p1, int32_t p2); /* 8003B074: 1 = a node was selected */
void ae_MonkeyAi_IdleActivityStart(AeRam *r, const AeMonkeyHooks *h);                     /* 8003F18C */
void ae_MonkeyAi_IdleActivityLoop(AeRam *r, const AeMonkeyHooks *h);                      /* 8003F308 */
void ae_MonkeyAi_GoToFlaggedNode(AeRam *r, const AeMonkeyHooks *h, uint32_t fallback_node);       /* 8004002C: caller-supplied fallback node; see the notes */

/* ====================================================================================================
 * Batch 5a (call-graph level 8): ae_monkey_level8.c
 * ================================================================================================== */
void ae_MonkeyDecideAction(AeRam *r, const AeMonkeyHooks *h);                             /* 800378C0 */
void ae_MonkeyInit(AeRam *r, const AeMonkeyHooks *h, uint32_t id, uint32_t activity, uint32_t slot); /* 8003A414 */
void ae_MonkeyAi_MoveAlerted(AeRam *r, const AeMonkeyHooks *h);                           /* 8003C6D4 */
void ae_MonkeyAi_State1E_20(AeRam *r, const AeMonkeyHooks *h);                            /* 8003CD84 */
void ae_MonkeyAi_MoveUnalerted(AeRam *r, const AeMonkeyHooks *h);                         /* 8003D9D4 */
void ae_MonkeyAi_IdleActivityEnd(AeRam *r, const AeMonkeyHooks *h);                       /* 8003F4B4 */
void ae_MonkeyAi_State07(AeRam *r, const AeMonkeyHooks *h);                               /* 80040170 */

/* ====================================================================================================
 * Batch 5b (call-graph level 9): ae_monkey_level9.c, ae_monkey_level9_patrol.c
 * ================================================================================================== */
void ae_MonkeyAi_State3D(AeRam *r, const AeMonkeyHooks *h);                               /* 8003FB44 */
void ae_MonkeyAi_JumpArc(AeRam *r, const AeMonkeyHooks *h);                               /* 8003F530 */
void ae_MonkeyAi_JumpArcPhased(AeRam *r, const AeMonkeyHooks *h);                         /* 8003F86C */
void ae_MonkeyAi_JumpArcB(AeRam *r, const AeMonkeyHooks *h);                              /* 8003FD7C */
void ae_MonkeyAi_Patrol(AeRam *r, const AeMonkeyHooks *h);                                /* 8003DC7C */
void ae_MonkeySelectActive(AeRam *r, const AeMonkeyHooks *h);                             /* 800426C4 */

/* ====================================================================================================
 * Batch 5c (call-graph levels 10 - 12): ae_monkey_level10.c
 * ================================================================================================== */
void ae_MonkeyAiStateUpdate(AeRam *r, const AeMonkeyHooks *h, uint32_t s2_in);            /* 800412B0 (s2_in = the caller's $s2, see the notes) */
void ae_MonkeySpawnAll(AeRam *r, const AeMonkeyHooks *h);                                 /* 8003958C */
void ae_MonkeyThinkActive(AeRam *r, const AeMonkeyHooks *h);                              /* 800424D4 */
void ae_MonkeyUpdateAll(AeRam *r, const AeMonkeyHooks *h);                                /* 80042E18 */

#ifdef __cplusplus
}
#endif
#endif
