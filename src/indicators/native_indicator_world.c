/* Native world indicator presentation (see native_indicator_world.md). */
#include "native_indicator_world.h"
#include "../render/native_actor.h"
#include "../camera/native_camera_math.h"
#include "native_indicators.h"
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_BYTES 16u
#define RECORD_BYTES 84u
#define RUNTIME_RECORD_BYTES 164u /* 84 presentation + 24 trigger + 56 collider */
#define TRIGGER_OFFSET 84u
#define COLLIDER_OFFSET 108u
#define MAX_PACK_BYTES (HEADER_BYTES + RUNTIME_RECORD_BYTES * AE_INDICATOR_MAX)
#define NAME_BYTES 64u
#define PATH_LIMIT 512u

typedef struct {
    const char *loop_name, *activate_name;
    uint32_t loop_frames, activate_frames;
} KindClips;

/* Idle frames 0..loop_frames-1, then activation frames continue from loop_frames:
 * kinds 0/1 wrap at 28 / 88 and kind 2 at 57 / 87 in ae_indicator_step. */
static const KindClips kind_clips[2] = {
    {"indicator10_loop", "indicator10_activate", 29, 60},
    {"indicator11_loop", "indicator11_activate", 58, 30},
};

typedef struct {
    AeActor *actor;
    uint32_t clip[2]; /* loop, activate actor clip indices */
    uint32_t source_object, hint_sector;
    uint16_t model_a, model_b; /* step's model frame outputs; the slot frame is drawn */
    uint8_t trigger[24], collider[56]; /* AEIND2 only; the slot itself lives in world->slots */
} Entry;

struct AeIndicatorWorld {
    uint32_t area, count, ticks;
    int runtime;
    AeNativeIndicator slots[AE_INDICATOR_MAX];
    Entry entries[AE_INDICATOR_MAX];
};

static char world_error[256];
static int bad(const char *message) { snprintf(world_error, sizeof(world_error), "%s", message); return 0; }
const char *ae_indicator_world_error(void) { return world_error; }

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int16_t rd16s(const uint8_t *p) {
    uint32_t value = (uint32_t)p[0] | (uint32_t)p[1] << 8;
    return value & 0x8000u ? (int16_t)((int32_t)value - 0x10000) : (int16_t)value;
}
static uint16_t rd16u(const uint8_t *p) { return (uint16_t)((uint32_t)p[0] | (uint32_t)p[1] << 8); }

void ae_indicator_world_free(AeIndicatorWorld *world) {
    if (!world) return;
    for (uint32_t i = 0; i < AE_INDICATOR_MAX; ++i) ae_actor_free(world->entries[i].actor);
    free(world);
}

static int safe_name(const uint8_t *name) {
    size_t length = 0;
    while (length < NAME_BYTES && name[length]) ++length;
    if (!length || length >= NAME_BYTES) return 0; /* empty or not null terminated */
    for (size_t i = 0; i < length; ++i) {
        uint8_t c = name[i];
        if (c <= 0x20 || c >= 0x7F || c == '/' || c == '\\' || c == ':') return 0;
        if (c == '.' && (i + 1 == length ? 0 : name[i + 1] == '.')) return 0;
    }
    return !(length == 1 && name[0] == '.');
}

static int join_path(char *out, const char *pack_path, const uint8_t *name) {
    size_t dir = 0;
    for (size_t i = 0; pack_path[i]; ++i) if (pack_path[i] == '/' || pack_path[i] == '\\') dir = i + 1;
    if (dir + strlen((const char *)name) + 1 > PATH_LIMIT) return bad("indicator actor path exceeds 512 bytes");
    memcpy(out, pack_path, dir);
    strcpy(out + dir, (const char *)name);
    return 1;
}

static int find_clip(const AeActor *actor, const char *name, uint32_t *index) {
    uint32_t info[4], found = 0;
    ae_actor_info(actor, info);
    for (uint32_t i = 0; i < info[3]; ++i)
        if (!strcmp(ae_actor_clip_name(actor, i), name)) { if (found++) return 0; *index = i; }
    return found == 1;
}

static int load_entry(Entry *entry, const char *pack_path, const uint8_t *record, int kind) {
    char path[PATH_LIMIT];
    const KindClips *clips = &kind_clips[kind == 2];
    const char *names[2] = {clips->loop_name, clips->activate_name};
    uint32_t frames[2] = {clips->loop_frames, clips->activate_frames};
    if (!safe_name(record + 20)) return bad("indicator pack actor name invalid");
    if (!join_path(path, pack_path, record + 20)) return 0;
    entry->actor = ae_actor_load(path);
    if (!entry->actor) return bad(ae_actor_error());
    for (int c = 0; c < 2; ++c) {
        if (!find_clip(entry->actor, names[c], &entry->clip[c])) return bad("indicator actor lacks a required clip (or has it twice)");
        if (ae_actor_frames(entry->actor, entry->clip[c]) != frames[c]) return bad("indicator actor clip frame count mismatch");
    }
    entry->source_object = rd32(record + 12);
    entry->hint_sector = rd32(record + 16);
    return 1;
}

/* AEIND2: the raw Gimmick instance (trigger) and referenced collider follow the
 * presentation record. They must agree with the prepared fields before the
 * original initializer composition registers the slot in the shared bank. */
static int prepare_runtime(AeIndicatorWorld *world, uint32_t i, const uint8_t *record,
                           const AeCameraMathTables *math) {
    Entry *entry = &world->entries[i];
    memcpy(entry->trigger, record + TRIGGER_OFFSET, sizeof(entry->trigger));
    memcpy(entry->collider, record + COLLIDER_OFFSET, sizeof(entry->collider));
    if (rd16s(entry->trigger + 0x0A) != rd16s(record)) return bad("indicator runtime trigger kind does not match the prepared record");
    if (rd16s(entry->trigger + 0x0C) != rd16s(record + 2)) return bad("indicator runtime trigger angle does not match the prepared record");
    if (rd32(record + 16) != (uint32_t)(int32_t)rd16s(entry->trigger + 0x12)) return bad("indicator runtime trigger hint sector does not match the prepared record");
    if (memcmp(entry->collider, record + 4, 6)) return bad("indicator runtime collider position does not match the prepared record");
    if (!ae_indicator_runtime_init((AeIndicatorTrigger *)entry->trigger, entry->collider,
                                   world->slots, &world->count, world->area, 0, math))
        return bad("indicator runtime initialization failed");
    if (world->count != i + 1 || rd16u(entry->trigger + 0x14) != i) return bad("indicator runtime registration order mismatch");
    return 1;
}

int ae_indicator_world_load(AeIndicatorWorld **out, const char *pack_path,
                            const uint8_t *exe, size_t exe_bytes, uint32_t area) {
    static const uint8_t magic1[8] = {'A', 'E', 'I', 'N', 'D', '1', 0, 0};
    static const uint8_t magic2[8] = {'A', 'E', 'I', 'N', 'D', '2', 0, 0};
    uint8_t bytes[MAX_PACK_BYTES];
    AeCameraMathTables math;
    AeIndicatorWorld *world;
    FILE *file;
    long length;
    uint32_t count, stride;
    int runtime;
    if (!out || !pack_path || !exe) return bad("indicator world: null argument");
    if (*out) return bad("indicator world: output pointer must be NULL");
    if (!ae_camera_math_load(&math, exe, exe_bytes)) return bad("indicator world: original math tables unavailable");
    file = fopen(pack_path, "rb");
    if (!file) return bad("cannot open indicator pack");
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < (long)HEADER_BYTES || length > (long)sizeof(bytes)) {
        fclose(file); return bad("indicator pack size invalid (16 to 672 bytes)");
    }
    rewind(file);
    if (fread(bytes, 1, (size_t)length, file) != (size_t)length) { fclose(file); return bad("indicator pack read failed"); }
    fclose(file);
    if (!memcmp(bytes, magic1, 8)) runtime = 0;
    else if (!memcmp(bytes, magic2, 8)) runtime = 1;
    else return bad("indicator pack signature invalid");
    stride = runtime ? RUNTIME_RECORD_BYTES : RECORD_BYTES;
    count = rd32(bytes + 12);
    if (rd32(bytes + 8) != area) return bad("indicator pack area does not match the scene");
    if (count > AE_INDICATOR_MAX) return bad("indicator pack count exceeds 4");
    if ((size_t)length != HEADER_BYTES + (size_t)stride * count) return bad("indicator pack size does not match its count");
    world = (AeIndicatorWorld *)calloc(1, sizeof(*world));
    if (!world) return bad("indicator world allocation failed");
    world->area = area;
    world->runtime = runtime;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *record = bytes + HEADER_BYTES + (size_t)stride * i;
        int16_t kind = rd16s(record);
        uint8_t position8[8] = {0};
        int index;
        if (kind < 0 || kind > 2) { ae_indicator_world_free(world); return bad("indicator kind outside 0..2"); }
        if (!load_entry(&world->entries[i], pack_path, record, kind)) { ae_indicator_world_free(world); return 0; }
        if (runtime) {
            if (!prepare_runtime(world, i, record, &math)) { ae_indicator_world_free(world); return 0; }
            continue;
        }
        memcpy(position8, record + 4, 6); /* padding halfword stays zero */
        index = ae_indicator_register(world->slots, &world->count, kind, position8, rd16s(record + 2), &math);
        if (index != (int)i) { ae_indicator_world_free(world); return bad("indicator registration failed"); }
    }
    *out = world;
    return 1;
}

int ae_indicator_world_upload(AeIndicatorWorld *world) {
    if (!world) return bad("indicator world: null world");
    for (uint32_t i = 0; i < world->count; ++i)
        if (!ae_actor_upload(world->entries[i].actor)) return bad(ae_actor_error());
    return 1;
}

void ae_indicator_world_tick(AeIndicatorWorld *world) {
    if (!world) return;
    for (uint32_t i = 0; i < world->count; ++i)
        ae_indicator_step(&world->slots[i], &world->entries[i].model_a, &world->entries[i].model_b);
    ++world->ticks;
}

uint32_t ae_indicator_world_count(const AeIndicatorWorld *world) { return world ? world->count : 0; }
uint32_t ae_indicator_world_ticks(const AeIndicatorWorld *world) { return world ? world->ticks : 0; }
uint32_t ae_indicator_world_frame(const AeIndicatorWorld *world, uint32_t index) {
    return world && index < world->count ? rd16u(world->slots[index].raw + 0x14) : 0;
}
uint32_t ae_indicator_world_state(const AeIndicatorWorld *world, uint32_t index) {
    return world && index < world->count ? rd16u(world->slots[index].raw + 0x16) : 0;
}
int ae_indicator_world_hint_sector(const AeIndicatorWorld *world, uint32_t index) {
    if (!world || index >= world->count || world->entries[index].hint_sector > 0x7FFFFFFFu) return -1;
    return (int)world->entries[index].hint_sector;
}
int ae_indicator_world_activate(AeIndicatorWorld *world, uint32_t index,
                                int message_busy, AeIndicatorActivation *events) {
    if (!world || index >= world->count || !events) return 0;
    return ae_indicator_activate(&world->slots[index],
        (int32_t)world->entries[index].hint_sector, message_busy, events);
}
int ae_indicator_world_kind(const AeIndicatorWorld *world, uint32_t index) {
    return world && index < world->count ? rd16s(world->slots[index].raw) : -1;
}

int ae_indicator_world_clip_frame(const AeIndicatorWorld *world, uint32_t index, uint32_t *clip, uint32_t *frame) {
    int kind = ae_indicator_world_kind(world, index);
    const KindClips *clips;
    uint32_t raw;
    if (kind < 0 || kind > 2 || !clip || !frame) return 0;
    clips = &kind_clips[kind == 2];
    raw = ae_indicator_world_frame(world, index);
    if (raw < clips->loop_frames) { *clip = 0; *frame = raw; return 1; }
    raw -= clips->loop_frames;
    if (raw >= clips->activate_frames) return 0;
    *clip = 1; *frame = raw;
    return 1;
}

int ae_indicator_world_placement(const AeIndicatorWorld *world, uint32_t index, float position[3], float *yaw) {
    const uint8_t *raw;
    if (!world || index >= world->count || !position || !yaw) return 0;
    raw = world->slots[index].raw;
    position[0] = rd16s(raw + 4) / 256.f;
    position[1] = -rd16s(raw + 6) / 256.f;
    position[2] = rd16s(raw + 8) / 256.f;
    /* MatrixFromDirXZ(sin, cos) = [-cos 0 -sin; 0 4096 0; sin 0 -cos], i.e. Ry(angle + 180). */
    *yaw = 180.f + atan2f((float)rd16s(raw + 0x0C), (float)rd16s(raw + 0x10)) * 180.f / 3.14159265358979323846f;
    return 1;
}

int ae_indicator_world_has_runtime(const AeIndicatorWorld *world) { return world ? world->runtime : 0; }

int ae_indicator_world_runtime_record(const AeIndicatorWorld *world, uint32_t index,
                                      AeIndicatorTrigger *trigger, uint8_t collider[56],
                                      AeNativeIndicator *slot) {
    if (!world || !world->runtime || index >= world->count || !trigger || !collider || !slot) return 0;
    memcpy(trigger->raw, world->entries[index].trigger, sizeof(trigger->raw));
    memcpy(collider, world->entries[index].collider, 56);
    memcpy(slot, &world->slots[index], sizeof(*slot));
    return 1;
}

int ae_indicator_world_proximity_step(AeIndicatorWorld *world, const int32_t player[3],
                                      uint32_t exit_code, int message_busy,
                                      const int16_t touched_by[4], uint32_t touch_known_mask,
                                      AeIndicatorRuntimeEvents events[4], uint32_t *missing_touch_mask) {
    AeIndicatorTrigger triggers[AE_INDICATOR_MAX];
    uint8_t colliders[AE_INDICATOR_MAX][56];
    AeNativeIndicator slots[AE_INDICATOR_MAX];
    AeIndicatorRuntimeEvents ev[AE_INDICATOR_MAX];
    uint32_t code = exit_code & 255u, missing = 0;
    if (!world || !world->runtime || !events || !missing_touch_mask) return bad("indicator proximity: invalid world or output");
    if (code == 0x17u || code == 7u || code == 0x15u) {
        memset(events, 0, sizeof(ev));
        *missing_touch_mask = 0;
        return 1;
    }
    if (!player) return bad("indicator proximity: player position required");
    if (touch_known_mask & ~((1u << world->count) - 1u)) return bad("indicator proximity: known-touch mask names a missing indicator");
    if (touch_known_mask && !touched_by) return bad("indicator proximity: touched-by values required for the known mask");
    memset(ev, 0, sizeof(ev));
    memcpy(slots, world->slots, sizeof(slots));
    for (uint32_t i = 0; i < world->count; ++i) {
        const Entry *entry = &world->entries[i];
        int16_t center[3];
        int32_t distance, state, radius;
        uint16_t touch;
        memcpy(triggers[i].raw, entry->trigger, sizeof(triggers[i].raw));
        memcpy(colliders[i], entry->collider, 56);
        /* The prepared disk touched field is never live contact: unknown means -1. */
        touch = (touch_known_mask >> i & 1u) ? (uint16_t)touched_by[i] : 0xFFFFu;
        colliders[i][30] = (uint8_t)touch;
        colliders[i][31] = (uint8_t)(touch >> 8);
        if (!(touch_known_mask >> i & 1u)) {
            for (int k = 0; k < 3; ++k) center[k] = rd16s(colliders[i] + 2 * k);
            distance = ae_indicator_trigger_distance(player, center);
            state = rd16s(triggers[i].raw + 2);
            radius = rd16s(triggers[i].raw + 14);
            /* Only the touch-reading branches of Gimmick_ProxTrigger_Update; state 0
             * activates without touch and state 3 beyond the radius resets without it. */
            if ((state == 1 && distance < radius) || (state == 3 && distance <= radius)) missing |= 1u << i;
        }
    }
    for (uint32_t i = 0; i < world->count; ++i)
        if (!ae_indicator_runtime_step(&triggers[i], colliders[i], slots, world->count, player,
                                       world->area, exit_code, message_busy, &ev[i]))
            return bad("indicator proximity: runtime step failed");
    for (uint32_t i = 0; i < world->count; ++i) {
        memcpy(world->entries[i].trigger, triggers[i].raw, sizeof(triggers[i].raw));
        memcpy(world->entries[i].collider, colliders[i], 56);
    }
    memcpy(world->slots, slots, sizeof(slots));
    memcpy(events, ev, sizeof(ev));
    *missing_touch_mask = missing;
    return 1;
}

uint32_t ae_indicator_world_model_frame(const AeIndicatorWorld *world, uint32_t index) {
    if (!world || index >= world->count) return 0;
    return ae_indicator_world_kind(world, index) == 2 ? world->entries[index].model_b : world->entries[index].model_a;
}

static int draw_world(AeIndicatorWorld *world, int textured, int stepped) {
    if (!world) return bad("indicator world: null world");
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    for (uint32_t i = 0; i < world->count; ++i) {
        const Entry *entry = &world->entries[i];
        uint32_t which, frame;
        float position[3], yaw;
        if (stepped) {
            const KindClips *clips = &kind_clips[ae_indicator_world_kind(world, i) == 2];
            frame = ae_indicator_world_model_frame(world, i);
            which = frame >= clips->loop_frames;
            if (which) frame -= clips->loop_frames;
            if (frame >= (which ? clips->activate_frames : clips->loop_frames))
                return bad("indicator cached model frame outside clip bounds");
        } else if (!ae_indicator_world_clip_frame(world, i, &which, &frame)) return bad("indicator raw frame outside clip bounds");
        ae_indicator_world_placement(world, i, position, &yaw);
        if (!ae_actor_pose(entry->actor, entry->clip[which], frame)) return bad(ae_actor_error());
        /* Preserve the original quantized MatrixFromDirXZ entries, including
         * their slight deviation from a unit-length floating rotation. */
        const uint8_t *raw=world->slots[i].raw;
        float sn=rd16s(raw+0x0C)/4096.f, cs=rd16s(raw+0x10)/4096.f;
        float matrix[16]={-cs,0,sn,0, 0,1,0,0, -sn,0,-cs,0,
                          position[0],position[1],position[2],1};
        const float origin[3]={0,0,0};
        /* Kind 1's original drawflag 0x40 is not modelled; no shadow, culling or sorting. */
        glPushMatrix(); glMultMatrixf(matrix);
        ae_actor_draw(entry->actor, textured, origin, 0);
        glPopMatrix();
    }
    return 1;
}

int ae_indicator_world_draw(AeIndicatorWorld *world, int textured) {
    return draw_world(world, textured, 0);
}

int ae_indicator_world_draw_stepped(AeIndicatorWorld *world, int textured) {
    return draw_world(world, textured, 1);
}
