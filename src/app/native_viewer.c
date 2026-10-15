/* Native SDL3 rendering foundation. No emulator or embedded game bytes.
 * SDL3 owns the platform; the current renderer remains OpenGL. See build_native_viewer.py.
 * AEV1 is a local prepared-asset format, not an original PSX file format.
 */
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#include "../render/native_actor.h"
#include "../collision/native_ground.h"
#include "../collision/native_collision.h"
#include "native_audio_output.h"
#include "native_input.h"
#include "../ui/native_ui.h"
#include "../indicators/native_indicator_world.h"
#include "../render/native_sky.h"
static int hint_sector;
static double ui_accumulator;
/* Optional --indicators-pack world indicators; ticked by their own 30 Hz accumulator. */
static AeIndicatorWorld *indicator_world;
static double indicator_accumulator;
/* Optional --indicator-proximity: world tick + proximity step run from the 30 Hz player tick
 * (after PlayerUpdate, before body integration) instead of indicator_accumulator. */
static int indicator_proximity;
typedef struct {
    uint32_t ticks,missing_mask,request_mask,warned_mask;
    uint32_t activations,hints,pickups,effects;
    int32_t position[3];
    int failed;
} IndicatorProximity;
static IndicatorProximity prox;
#define PROX_REQ_HINT 1u
#define PROX_REQ_PICKUPS 2u
#define PROX_REQ_EFFECT 4u
static int game_mode;
#ifdef AE_NATIVE_PLAYER
#include "native_player_host.h"
#include "viewer_game.h"
static ViewerGame game_session;
static int game_free_camera;
static const char *game_state_path;
static AePlayerHost player_host;
static int native_player_mode;
#include "../camera/native_camera_host.h"
#include "../camera/native_camera_render.h"
#include "../camera/native_camera_bank.h"
#include "../camera/native_camera_stage.h"
/* Opt-in original follow camera prototype (--native-camera); see native_camera_render.md.
 * --stage-camera additionally composes native_camera_stage (see native_camera_stage_viewer.md). */
#define CAMERA_PROJECTION_FALLBACK 0x200u /* viewer-side status bit: approximate projection failed */
#define CAMERA_INITIAL_SXY (192u|(120u<<16))
static AeCameraHost follow_host;
static int native_camera_requested, native_camera_mode;
static int camera_input_x, camera_input_y, camera_mouse_x, camera_mouse_y;
static uint32_t camera_last_sxy=CAMERA_INITIAL_SXY, camera_status;
static int camera_fallback_warned;
static uint8_t camera_render_position[8]; /* Previous draw position, before its body integration. */
/* Stage camera: global so the host's queries.user (&follow_host) and the stage stay at stable addresses. */
#define STAGE_NONFATAL (AE_CAMERA_STAGE_UNSUPPORTED_HANDLER|AE_CAMERA_STAGE_UNSUPPORTED_MODE)
static AeCameraStage camera_stage;
static int stage_camera_requested;
static uint8_t *stage_blob; /* viewer-owned byte copy of the area member; the stage only borrows it */
static size_t stage_blob_size;
static uint32_t camera_stage_status, stage_warned_status; /* separate from the legacy camera_status */
#define camera_follow_on() (native_camera_mode)
#define camera_input_clear() (camera_input_x=camera_input_y=camera_mouse_x=camera_mouse_y=0)
#else
#define camera_follow_on() 0
#define camera_input_clear() ((void)0)
#endif

typedef struct { float x, y, z, u, v, r, g, b; } Vertex;
/* Pack v1 stores the first four fields; v2 adds the split semi layer and source flags (see native_prop_rendering.md);
 * v3 adds sky_type (0 normal, 4/5 original special sky prop; see native_sky_rendering.md). v1/v2 load with sky_type 0. */
typedef struct { uint32_t first, count, texture, kind, semi_texture, flags, sky_type; } Batch;
enum { BATCH_SEMI=1, BATCH_MODE_SHIFT=1, BATCH_MODE_MASK=6, BATCH_UNTEXTURED_SEMI=8, BATCH_RESOLVED_MODE=16, BATCH_FLAGS_ALL=31 };
typedef struct {
    unsigned char *bytes;
    size_t size;
    /* blend_modes: bit per PSX mode used by semi batches (textured split layers and resolved untextured);
     * untextured_semi: legacy bit3 batches with unknown mode; untextured_resolved: bit4 batches with a source mode */
    uint32_t version, blend_modes, untextured_semi, untextured_resolved;
    uint32_t area, member, nv, nb, nt, terrain, props;
    uint32_t sky_batches, sky_triangles, sky_max_vertices; /* v3 only: type 4/5 batches, triangles, largest batch */
    float lo[3], hi[3], focus_lo[3], focus_hi[3];
    Vertex *vertices;   /* immutable stored pack vertices (sky: raw source quarter-turned geometry) */
    Vertex *sky_scratch; /* owned: per-draw transformed copy of the largest sky batch only */
    Batch *batches;
    const unsigned char **pixels;
    uint32_t *width, *height;
    GLuint *textures;
} Scene;
typedef struct { float target[3], yaw, pitch, distance, base_distance; } Camera;
static Scene scene;
static Camera camera;
static int width = 960, height = 640, running = 1, colliders = 0, textured = 1;
static int dragging = 0;
static SDL_Window *window_handle;
static SDL_GLContext context;
static SDL_Gamepad *gamepad;
static int directinput=0; /* --directinput: allow SDL's DirectInput joystick backend (off by default, can stall startup) */
static AeInput input_state; /* merged keyboard + gamepad pad word and sticks; edges advance once per 30 Hz tick */
static double input_accumulator;
static int keyboard_only=0; /* explicit --keyboard-only: gamepad subsystem is never initialised or opened */
static char error_text[256];
static AeActor *actor;
static uint32_t actor_clip, actor_count;
static float actor_position[3]={0,0,0}, actor_yaw=180, actor_time=0;
static int actor_paused=0;
static AeGround *ground;
/* Optional --monkey-actor: catchable monkeys (APPROXIMATE behaviour on the original nav data, see notes/port/monkeys.md). */
static AeActor *monkey_actor;
static AeCollision *collision;
static int wall_supported;
static uint32_t movement_contacts;
static int player_demo=0;
static uint32_t movement_blocked=0, movement_accepted=0;
static float movement_accumulator=0;

_Static_assert(sizeof(Vertex) == 32, "vertex packing");
_Static_assert(sizeof(Batch) == 28, "batch packing");
_Static_assert(sizeof(float) == 4, "float packing");

static uint32_t u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static float f32(const unsigned char *p) {
    uint32_t word = u32(p); float value; memcpy(&value, &word, 4); return value;
}
static int fail(const char *message) {
    snprintf(error_text, sizeof(error_text), "%s", message); return 0;
}
static int span(size_t size, size_t offset, size_t count, size_t stride) {
    return offset <= size && (!count || stride <= (size-offset)/count);
}
/* Stored sky coordinate (x/256, -y/256, z/256 as float) -> raw signed 16-bit mesh coordinate. negate=1 for stored Y.
 * Range checked; returns 0 for NaN/inf/out of range, never an implementation-defined cast. */
static int sky_to_s16(float value, int negate, int16_t *out) {
    double raw = (double)value*256.0;
    double d = floor(raw+.5);
    if (d != raw) return 0; /* v3 stores exact integer source coordinates, not rounded geometry */
    if (negate) d = -d;
    if (!(d >= -32768.0 && d <= 32767.0)) return 0;
    *out = (int16_t)(int32_t)d;
    return 1;
}
/* Original 16-bit wrap of an arbitrary host integer without implementation-defined conversions. */
static int16_t wrap_s16(int32_t value) {
    uint32_t u = (uint32_t)value & 0xFFFFu;
    return (int16_t)(u >= 0x8000u ? (int32_t)u-0x10000 : (int32_t)u);
}
static void scene_free(Scene *s) {
    if (context && s->textures) glDeleteTextures((GLsizei)s->nt, s->textures);
    free(s->bytes); free(s->batches); free(s->sky_scratch); free(s->pixels); free(s->width);
    free(s->height); free(s->textures); memset(s, 0, sizeof(*s));
}
static int scene_load(Scene *s, const char *path) {
    FILE *file = fopen(path, "rb");
    long length;
    size_t offset, stride;
    uint32_t expected = 0, counts[3] = {0,0,0};
    if (!file) return fail("cannot open scene pack");
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 64 || length > 268435456) {
        fclose(file); return fail("scene file size invalid (64 B to 256 MiB)");
    }
    rewind(file);
    s->size = (size_t)length;
    s->bytes = (unsigned char *)malloc(s->size);
    if (!s->bytes) { fclose(file); return fail("scene allocation failed"); }
    if (fread(s->bytes, 1, s->size, file) != s->size) {
        fclose(file); return fail("scene read failed");
    }
    fclose(file);
    s->version = u32(s->bytes+8);
    if (!((!memcmp(s->bytes, "AEVIEW1\0", 8) && s->version == 1) ||
          (!memcmp(s->bytes, "AEVIEW2\0", 8) && s->version == 2) ||
          (!memcmp(s->bytes, "AEVIEW3\0", 8) && s->version == 3))) return fail("unsupported scene signature/version");
    stride = s->version == 3 ? 28 : s->version == 2 ? 24 : 16;
    s->area = u32(s->bytes+12); s->member = u32(s->bytes+16);
    s->nv = u32(s->bytes+20); s->nb = u32(s->bytes+24); s->nt = u32(s->bytes+28);
    s->terrain = u32(s->bytes+32); s->props = u32(s->bytes+36);
    if (!s->area || s->area > 255 || s->member >= 0x180 || !s->nv || s->nv%3 || s->nv > 3000000 ||
        !s->nb || s->nb > 100000 || s->nt > 4096) return fail("scene count/identity limits violated");
    for (int axis=0; axis<3; ++axis) {
        s->lo[axis] = f32(s->bytes+40+axis*4); s->hi[axis] = f32(s->bytes+52+axis*4);
        if (!isfinite(s->lo[axis]) || !isfinite(s->hi[axis]) || s->lo[axis] > s->hi[axis]) return fail("invalid scene bounds");
        s->focus_lo[axis] = 1e30f; s->focus_hi[axis] = -1e30f;
    }
    offset = 64;
    if (!span(s->size, offset, s->nb, stride)) return fail("batch table truncated");
    s->batches = (Batch *)calloc(s->nb, sizeof(Batch));
    if (!s->batches) return fail("batch allocation failed");
    for (uint32_t i=0; i<s->nb; ++i) {
        Batch *b = s->batches+i;
        const unsigned char *p = s->bytes+offset+i*stride;
        b->first=u32(p); b->count=u32(p+4); b->texture=u32(p+8); b->kind=u32(p+12);
        b->semi_texture=UINT32_MAX; b->flags=0;
        b->sky_type=0;
        if (s->version >= 2) { b->semi_texture=u32(p+16); b->flags=u32(p+20); }
        if (s->version == 3) b->sky_type=u32(p+24);
        if (b->first != expected || !b->count || b->count%3 || b->count > s->nv-expected || b->kind > 2 ||
            (b->texture != UINT32_MAX && b->texture >= s->nt)) return fail("invalid batch range/material/kind");
        if (b->sky_type != 0 && b->sky_type != AE_SKY_TYPE_FOLLOW_PLAYER_Y && b->sky_type != AE_SKY_TYPE_FIXED_Y)
            return fail("invalid batch sky type");
        if (b->sky_type && b->kind != 1) return fail("sky type on a non-prop batch");
        if ((b->flags & ~(uint32_t)BATCH_FLAGS_ALL) || (b->flags && b->kind != 1)) return fail("invalid batch flags");
        if (b->semi_texture != UINT32_MAX && (b->kind != 1 || !(b->flags & BATCH_SEMI) || b->texture == UINT32_MAX ||
            b->semi_texture >= s->nt || b->semi_texture == b->texture)) return fail("invalid batch semi layer");
        if (!(b->flags & BATCH_SEMI) && b->flags) return fail("batch blend/untextured flags without semi bit");
        if ((b->flags & BATCH_RESOLVED_MODE) && !(b->flags & BATCH_UNTEXTURED_SEMI))
            return fail("resolved draw-mode flag without untextured semi flag");
        if (b->flags & BATCH_UNTEXTURED_SEMI) {
            if (b->texture != UINT32_MAX || b->semi_texture != UINT32_MAX) return fail("invalid untextured semi batch");
            if (b->flags & BATCH_RESOLVED_MODE) {
                s->untextured_resolved += b->count/3;
                s->blend_modes |= 1u << ((b->flags & BATCH_MODE_MASK) >> BATCH_MODE_SHIFT);
            } else {
                if (b->flags & BATCH_MODE_MASK) return fail("invalid untextured semi batch");
                s->untextured_semi += b->count/3;
            }
        } else if ((b->flags & BATCH_SEMI) && (b->texture == UINT32_MAX || b->semi_texture == UINT32_MAX))
            return fail("textured semi batch lacks a valid split layer");
        if (b->semi_texture != UINT32_MAX) s->blend_modes |= 1u << ((b->flags & BATCH_MODE_MASK) >> BATCH_MODE_SHIFT);
        counts[b->kind] += b->count/3;
        if (b->sky_type) {
            ++s->sky_batches; s->sky_triangles += b->count/3;
            if (b->count > s->sky_max_vertices) s->sky_max_vertices = b->count;
        }
        expected += b->count;
    }
    if (expected != s->nv || counts[0] != s->terrain || counts[1] != s->props || (!counts[0] && !counts[1]))
        return fail("batch coverage/triangle totals invalid");
    offset += (size_t)s->nb*stride;
    if (!span(s->size, offset, s->nv, 32)) return fail("vertices truncated");
    s->vertices = (Vertex *)(void *)(s->bytes+offset);
    for (uint32_t i=0; i<s->nv; ++i) {
        const float *v = (const float *)(const void *)(s->vertices+i);
        for (int k=0; k<8; ++k) {
            if (!isfinite(v[k]) || fabsf(v[k]) > 1e6f || (k>=5 && (v[k]<0 || v[k]>1))) return fail("invalid vertex value");
        }
    }
    for (uint32_t i=0; i<s->nb; ++i) {
        Batch *b = s->batches+i;
        if (b->kind == 2) continue;
        for (uint32_t j=b->first; j<b->first+b->count; ++j) {
            const float *v = (const float *)(const void *)(s->vertices+j);
            for (int axis=0; axis<3; ++axis) {
                if (v[axis] < s->lo[axis]-.001f || v[axis] > s->hi[axis]+.001f) return fail("vertex outside declared bounds");
                if (b->kind==0 || !counts[0]) {
                    s->focus_lo[axis] = fminf(s->focus_lo[axis],v[axis]);
                    s->focus_hi[axis] = fmaxf(s->focus_hi[axis],v[axis]);
                }
            }
        }
    }
    for (uint32_t i=0; i<s->nb; ++i) { /* sky vertices must convert to signed 16 bits (x/256, -y/256, z/256 storage) */
        const Batch *b = s->batches+i;
        int16_t ignored;
        if (!b->sky_type) continue;
        for (uint32_t j=b->first; j<b->first+b->count; ++j) {
            const Vertex *v = s->vertices+j;
            if (!sky_to_s16(v->x,0,&ignored) || !sky_to_s16(v->y,1,&ignored) || !sky_to_s16(v->z,0,&ignored))
                return fail("sky vertex outside signed 16-bit range");
        }
    }
    if (s->sky_max_vertices) {
        if (!span(SIZE_MAX,0,s->sky_max_vertices,sizeof(Vertex))) return fail("sky scratch size overflow");
        s->sky_scratch = (Vertex *)calloc(s->sky_max_vertices, sizeof(Vertex));
        if (!s->sky_scratch) return fail("sky scratch allocation failed");
    }
    offset += (size_t)s->nv*32;
    s->pixels =(const unsigned char **)calloc(s->nt+1, sizeof(*s->pixels));
    s->width = (uint32_t *)calloc(s->nt+1, sizeof(uint32_t));
    s->height = (uint32_t *)calloc(s->nt+1, sizeof(uint32_t));
    s->textures = (GLuint *)calloc(s->nt+1, sizeof(GLuint));
    if (!s->pixels || !s->width || !s->height || !s->textures) return fail("texture metadata allocation failed");
    for (uint32_t i=0; i<s->nt; ++i) {
        uint32_t bytes;
        if (!span(s->size, offset, 1, 12)) return fail("texture header truncated");
        s->width[i]=u32(s->bytes+offset); s->height[i]=u32(s->bytes+offset+4); bytes=u32(s->bytes+offset+8);
        if (!s->width[i] || !s->height[i] || s->width[i]>4096 || s->height[i]>4096 ||
            bytes != s->width[i]*s->height[i]*4) return fail("invalid texture dimensions/byte length");
        offset += 12;
        if (!span(s->size, offset, 1, bytes)) return fail("texture pixels truncated");
        s->pixels[i]=s->bytes+offset; offset += bytes;
    }
    if (offset != s->size) return fail("trailing scene bytes");
    /* Split layers must be 256x256 with alpha 0/255, and no texel opaque in both (disjoint opaque/semi coverage). */
    for (uint32_t i=0; i<s->nb; ++i) {
        const Batch *b = s->batches+i;
        const unsigned char *o, *m;
        if (b->semi_texture == UINT32_MAX) continue;
        if (s->width[b->texture] != 256 || s->height[b->texture] != 256 ||
            s->width[b->semi_texture] != 256 || s->height[b->semi_texture] != 256) return fail("semi layer pair is not 256x256");
        o = s->pixels[b->texture]; m = s->pixels[b->semi_texture];
        for (uint32_t t=0; t<256*256; ++t) {
            unsigned char ao=o[t*4+3], am=m[t*4+3];
            if ((ao != 0 && ao != 255) || (am != 0 && am != 255) || (ao && am)) return fail("semi layer pair is not disjoint alpha 0/255");
        }
    }
    if (s->untextured_semi)
        fprintf(stderr, "WARNING: %u untextured semi-transparent prop triangles in a legacy pack have no resolved draw mode; "
                "drawn opaque as a host approximation (blend mode unknown)\n", s->untextured_semi);
    return 1;
}

static void reset_camera(void) {
    float extent = 1;
    for (int axis=0; axis<3; ++axis) {
        camera.target[axis] = (scene.focus_lo[axis]+scene.focus_hi[axis])*.5f;
        extent = fmaxf(extent, scene.focus_hi[axis]-scene.focus_lo[axis]);
    }
    camera.yaw = 35; camera.pitch = 35;
    camera.base_distance = extent*1.3f; camera.distance = camera.base_distance;
}
static void focus_actor(void) {
    for (int i=0; i<3; ++i) camera.target[i]=actor_position[i];
    camera.target[1]+=.6f; camera.distance=4; camera.base_distance=4;
    camera.pitch=10; camera.yaw=0;
}
#ifdef AE_NATIVE_PLAYER
static uint8_t *camera_read_file(const char *path,size_t limit,size_t *size) {
    FILE *file=fopen(path,"rb");
    long length;
    uint8_t *bytes;
    if(!file)return NULL;
    if(fseek(file,0,SEEK_END)||(length=ftell(file))<=0||(unsigned long)length>limit){fclose(file);return NULL;}
    rewind(file);
    bytes=(uint8_t *)malloc((size_t)length);
    if(!bytes){fclose(file);return NULL;}
    if(fread(bytes,1,(size_t)length,file)!=(size_t)length){free(bytes);fclose(file);return NULL;}
    fclose(file);
    *size=(size_t)length;
    return bytes;
}
static int clamp127(long value) {
    return value<-127?-127:value>127?127:(int)value;
}
static void put16le(uint8_t *out,uint32_t value) {
    out[0]=(uint8_t)(value&0xFF);out[1]=(uint8_t)((value>>8)&0xFF);
}
static int16_t sext16(uint32_t value) {
    value&=0xFFFF;
    return value&0x8000?(int16_t)((int32_t)value-0x10000):(int16_t)value;
}
/* Spike's first bone offset is zero in member 008 +BC. ModelBuildBoneMatrices
 * (flags 3, mode 0) stores that world bone's position at actor +1C before
 * CollisionUpdate. Keep the previous draw position separate from the body. */
static void camera_cache_render_position(void) {
    memset(camera_render_position,0,sizeof(camera_render_position));
    for(int i=0;i<3;++i)put16le(camera_render_position+i*2,(uint32_t)player_host.player.pos[i]);
}
static int16_t camera_read16(const uint8_t *in) {
    return sext16((uint32_t)in[0]|(uint32_t)in[1]<<8);
}
static void camera_snapshot(AeCameraPlayerSnapshot *s) {
    const AePlayer *p=&player_host.player;
    memset(s,0,sizeof(*s));
    for(int i=0;i<3;++i){
        s->pos[i]=p->pos[i];s->vel[i]=p->vel[i];
    }
    memcpy(s->render8,camera_render_position,8);
    for(int i=0;i<4;++i)put16le(s->facing8+i*2,(uint32_t)(uint16_t)p->facing[i]);
    s->ground_y=p->ground_y;
}
static int camera_apply_orbit(void) {
    if(!ae_camera_render_orbit(&follow_host.slot,camera.target,&camera.yaw,&camera.pitch,&camera.distance))
        return fail("native camera slot cannot be rendered (degenerate eye/target)");
    camera.base_distance=camera.distance;
    return 1;
}
/* Stage first, then the blob it borrows. */
static void stage_camera_free(void) {
    ae_camera_stage_free(&camera_stage);
    free(stage_blob);stage_blob=NULL;stage_blob_size=0;
}
/* Validated read bounds: slot index / kind / current region, -1 when unavailable. */
static void stage_camera_selection(int *slot,int *kind,int *region) {
    const uint8_t *c=camera_stage.frame.controls;
    uint32_t index=(uint32_t)c[AE_CAMCTL_CURRENT]|(uint32_t)c[AE_CAMCTL_CURRENT+1]<<8;
    *slot=*kind=*region=-1;
    if(!camera_stage.ready)return;
    if(index<AE_CAMERA_BANK_SLOTS){*slot=(int)index;*kind=camera_stage.frame.slots[index].raw[0];}
    if(c[AE_CAMCTL_CLEAR_D]<camera_stage.regions.count)*region=c[AE_CAMCTL_CLEAR_D]; /* 0xFF = no region */
}
/* Fields for the capture report and the walk trace; the stage fields only when the stage runs. */
static void stage_camera_json(FILE *out) {
    int on=stage_camera_requested&&camera_follow_on(),slot,kind,region;
    fprintf(out,",\"stage_camera\":%s",on?"true":"false");
    if(!on)return;
    stage_camera_selection(&slot,&kind,&region);
    fprintf(out,",\"stage_camera_status\":%u,\"stage_slot\":%d,\"stage_slot_kind\":%d,\"stage_region\":%d",camera_stage_status,slot,kind,region);
}
/* Original IsFlightArea at 0x8004A940 requires mode 2 AND area 0x48..0x4C.
 * This viewer stays in mode 0; excluding flight-designated areas is a conservative
 * prototype policy, not an exact port of that predicate. */
static int stage_camera_area_is_flight(uint32_t area) {
    return area>=0x48&&area<=0x4C;
}
static int native_camera_init(const char *exe_path,const char *ground_member) {
    AeCameraPlayerSnapshot snapshot;
    size_t size=0;
    uint8_t *exe;
    int ok;
    if(stage_camera_requested&&stage_camera_area_is_flight(scene.area))
        return fail("stage camera: flight areas (0x48-0x4C) are not supported");
    exe=camera_read_file(exe_path,16u<<20,&size);
    if(!exe)return fail("native camera: cannot read collision EXE (1 B to 16 MiB)");
    camera_cache_render_position();
    camera_snapshot(&snapshot);
    ok=ae_camera_host_init(&follow_host,exe,size,ground,collision,&snapshot);
    free(exe); /* the host copied its math tables */
    if(!ok)return fail("native camera host initialization failed");
    if(stage_camera_requested){
        /* The extracted area member, once; the stage borrows these bytes. */
        stage_blob=camera_read_file(ground_member,64u<<20,&stage_blob_size);
        if(!stage_blob)return fail("stage camera: cannot read area member (1 B to 64 MiB)");
        if(!ae_camera_stage_init(&camera_stage,&follow_host,stage_blob,stage_blob_size,&snapshot)){
            char message[96];
            snprintf(message,sizeof(message),"stage camera initialization failed (status 0x%X)",camera_stage.status);
            stage_camera_free();
            return fail(message);
        }
        camera_stage_status=stage_warned_status=0;
    }
    native_camera_mode=1;
    return camera_apply_orbit();
}
static int native_camera_reset(void) {
    AeCameraPlayerSnapshot snapshot;
    camera_snapshot(&snapshot);
    if(!ae_camera_host_reset(&follow_host,&snapshot))return fail("native camera reset failed");
    if(stage_camera_requested){
        /* Free and re-init on the same blob; init seeds from the freshly reset host slot. */
        ae_camera_stage_free(&camera_stage);
        if(!ae_camera_stage_init(&camera_stage,&follow_host,stage_blob,stage_blob_size,&snapshot)){
            char message[96];
            snprintf(message,sizeof(message),"stage camera reset failed (status 0x%X)",camera_stage.status);
            return fail(message);
        }
        camera_stage_status=stage_warned_status=0;
    }
    camera_last_sxy=CAMERA_INITIAL_SXY;camera_status=0;camera_input_clear();
    return camera_apply_orbit();
}
static void native_camera_toggle(void) {
    if(!native_camera_requested||!follow_host.ready||!player_demo)return;
    native_camera_mode=!native_camera_mode;
    camera_input_clear();
    /* Disabling leaves the last orbit state for inspection; enabling snaps back to the slot. */
    if(native_camera_mode&&!camera_apply_orbit())running=0;
}
/* Mode_Stage calls CameraUpdate before PlayerUpdate and CollisionUpdate.
 * Host proxies are documented in native_camera_render.md. */
static int native_camera_step(void) {
    const AePlayer *p=&player_host.player;
    AeCameraFollowFrame *frame=&follow_host.frame;
    AeCameraPlayerSnapshot snapshot;
    int16_t point[3],water_y=0;
    uint32_t sxy=0;
    int sx=clamp127((long)camera_input_x+camera_mouse_x),sy=clamp127((long)camera_input_y+camera_mouse_y),mag;
    camera_mouse_x=camera_mouse_y=0;
    if(p->current_gadget==7)return fail("native camera: gadget 7 (RC car) camera is not modelled");
    mag=(int)lroundf(sqrtf((float)(sx*sx+sy*sy)));
    if(mag>127)mag=127;
    camera_snapshot(&snapshot);
    frame->state=p->state;frame->state_flags=p->state_flags;
    frame->current_area=scene.area;frame->current_gadget=(uint8_t)p->current_gadget;
    frame->player_mode=0;frame->stage_entry_flags=0; /* mode 0 module; stage loading events are absent */
    /* g_StageExitCode is unmodelled: 3 is the viewer's explicit normal-stage policy. */
    frame->can_exit=ae_camera_can_exit_stage(3,p->state,p->state_flags);
    {
        uint16_t surface=ae_collision_surface_type(collision,p->pos,&water_y);
        /* Host proxy for the original water-active byte: surface kind 1 at the player. */
        frame->submerged=ae_camera_player_submerged((surface>>12)==1,water_y,p->pos[1]);
    }
    frame->rccar_active=0;
    frame->pad_held=0; /* no verified pad-bit mapping for the camera orbit; stick values carry the input */
    frame->right_stick_word=(uint32_t)mag;
    frame->stick_x=(int8_t)sx;frame->stick_y=(int8_t)sy;
    /* RotTransPers input: render x, body y - 0x80 (16 bit wrap), render z, with the PREVIOUS slot. */
    point[0]=camera_read16(snapshot.render8);
    point[1]=sext16((uint32_t)p->pos[1]-0x80u);
    point[2]=camera_read16(snapshot.render8+4);
    camera_status=0;
    if(ae_camera_project_approx(&follow_host.slot,point,328.f,192.f,120.f,&sxy))camera_last_sxy=sxy;
    else{
        camera_status|=CAMERA_PROJECTION_FALLBACK;
        if(!camera_fallback_warned){
            camera_fallback_warned=1;
            fprintf(stderr,"CAMERA approximate projection invalid; using previous screen XY (status bit 0x%X, warned once) frame=%u\n",
                    CAMERA_PROJECTION_FALLBACK,player_host.world.frame_counter);
        }
    }
    frame->projected_sxy=camera_last_sxy;
    if(stage_camera_requested){
        /* is_flight_area=0: only regular ground follow dispatch is supported (flight areas are rejected at init). */
        if(!ae_camera_stage_step(&camera_stage,&follow_host,&snapshot,0)){
            uint32_t bits=camera_stage.status;
            if(!bits||(bits&~(uint32_t)STAGE_NONFATAL)){
                char message[96];
                snprintf(message,sizeof(message),"stage camera update failed (status 0x%X)",bits);
                return fail(message);
            }
            /* Missing indexed handler / unsupported selected mode: the stage rolled back, so the
             * prior view stays. Record it and warn once per changed status; no fake fallback. */
            camera_stage_status=bits;
            if(bits!=stage_warned_status){
                stage_warned_status=bits;
                fprintf(stderr,"CAMERA stage update unsupported (stage status 0x%X); keeping previous view frame=%u\n",
                        bits,player_host.world.frame_counter);
            }
            return 1;
        }
        camera_stage_status=stage_warned_status=0;
    }else if(!ae_camera_host_step(&follow_host,&snapshot,0)){
        char message[96];
        snprintf(message,sizeof(message),"native camera follow update failed (status 0x%X)",follow_host.status);
        return fail(message);
    }
    camera_status|=follow_host.status;
    return camera_apply_orbit();
}
#endif
static const char *gamepad_type_name(SDL_GamepadType t) {
    switch(t){
    case SDL_GAMEPAD_TYPE_STANDARD:return "standard";
    case SDL_GAMEPAD_TYPE_XBOX360:return "Xbox 360";
    case SDL_GAMEPAD_TYPE_XBOXONE:return "Xbox One";
    case SDL_GAMEPAD_TYPE_PS3:return "PS3";
    case SDL_GAMEPAD_TYPE_PS4:return "PS4";
    case SDL_GAMEPAD_TYPE_PS5:return "PS5";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:return "Nintendo Switch Pro";
    default:return "other";
    }
}
static void gamepad_open(SDL_JoystickID id) {
    if (keyboard_only) return;
    if (!gamepad && SDL_IsGamepad(id)) {
        gamepad=SDL_OpenGamepad(id);
        if(gamepad){const char *name=SDL_GetGamepadName(gamepad);
            fprintf(stderr,"GAMEPAD opened: %s (type %s)\n",name?name:"unknown",gamepad_type_name(SDL_GetGamepadType(gamepad)));}
        else fprintf(stderr,"GAMEPAD open failed: %s\n",SDL_GetError());
    }
}
/* Fill the pure-C samples from SDL and build the merged pad state (no edges: ae_input_tick derives them). */
static void input_sample(AeInput *out,AeInput *pad_out) {
    const bool *keys=SDL_GetKeyboardState(NULL);
    AeKeyboardSample ks;AeInput kb,pad;
    memset(&ks,0,sizeof(ks));
    ks.w=keys[SDL_SCANCODE_W];ks.a=keys[SDL_SCANCODE_A];ks.s=keys[SDL_SCANCODE_S];ks.d=keys[SDL_SCANCODE_D];
    ks.shift=(SDL_GetModState()&SDL_KMOD_SHIFT)!=0;
    ks.up=keys[SDL_SCANCODE_UP];ks.down=keys[SDL_SCANCODE_DOWN];ks.left=keys[SDL_SCANCODE_LEFT];ks.right=keys[SDL_SCANCODE_RIGHT];
    ks.space=keys[SDL_SCANCODE_SPACE];ks.lctrl=keys[SDL_SCANCODE_LCTRL];
    ks.i=keys[SDL_SCANCODE_I];ks.j=keys[SDL_SCANCODE_J];ks.k=keys[SDL_SCANCODE_K];ks.l=keys[SDL_SCANCODE_L];
    ks.enter=keys[SDL_SCANCODE_RETURN];ks.tab=keys[SDL_SCANCODE_TAB];ks.n1=keys[SDL_SCANCODE_1];ks.n2=keys[SDL_SCANCODE_2];
    ae_input_from_keyboard(&kb,&ks);
    ae_input_centered(&pad);
    if(gamepad){
        AeGamepadSample gs;memset(&gs,0,sizeof(gs));
#define GB(b) (SDL_GetGamepadButton(gamepad,b)?1:0)
        gs.south=GB(SDL_GAMEPAD_BUTTON_SOUTH);gs.east=GB(SDL_GAMEPAD_BUTTON_EAST);
        gs.west=GB(SDL_GAMEPAD_BUTTON_WEST);gs.north=GB(SDL_GAMEPAD_BUTTON_NORTH);
        gs.left_shoulder=GB(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);gs.right_shoulder=GB(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        gs.back=GB(SDL_GAMEPAD_BUTTON_BACK);gs.touchpad=GB(SDL_GAMEPAD_BUTTON_TOUCHPAD);gs.start=GB(SDL_GAMEPAD_BUTTON_START);
        gs.left_stick=GB(SDL_GAMEPAD_BUTTON_LEFT_STICK);gs.right_stick=GB(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
        gs.dpad_up=GB(SDL_GAMEPAD_BUTTON_DPAD_UP);gs.dpad_down=GB(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        gs.dpad_left=GB(SDL_GAMEPAD_BUTTON_DPAD_LEFT);gs.dpad_right=GB(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
#undef GB
        gs.lx=SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_LEFTX)/32768.f;gs.ly=SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_LEFTY)/32768.f;
        gs.rx=SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_RIGHTX)/32768.f;gs.ry=SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_RIGHTY)/32768.f;
        gs.lt=SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)/32767.f;gs.rt=SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)/32767.f;
        ae_input_from_gamepad(&pad,&gs);
    }
    ae_input_merge(out,&kb,&pad);
    if(pad_out)*pad_out=pad;
}
/* Explicit manual preview. Nearest selection is viewer policy, not the original
 * collider/proximity/progress logic. Do not enter unsupported player mode 0xB. */
static int indicator_hint_preview(int index) {
    uint32_t count=ae_indicator_world_count(indicator_world);
    if(index<0){
        double best=HUGE_VAL;
        for(uint32_t i=0;i<count;++i){
            float pos[3],yaw;double distance=0;
            if(ae_indicator_world_kind(indicator_world,i)>1)continue;
            if(!ae_indicator_world_placement(indicator_world,i,pos,&yaw))continue;
            for(int j=0;j<3;++j){double d=(double)pos[j]-actor_position[j];distance+=d*d;}
            if(distance<best){best=distance;index=(int)i;}
        }
    }
    if(index<0||(uint32_t)index>=count||ae_indicator_world_kind(indicator_world,(uint32_t)index)>1)return 0;
    if(ae_ui_state()||ae_indicator_world_state(indicator_world,(uint32_t)index)==1)return 0;
    int sector=ae_indicator_world_hint_sector(indicator_world,(uint32_t)index);
    if(!ae_ui_hint(sector))return 0;
    AeIndicatorActivation activation;
    if(!ae_indicator_world_activate(indicator_world,(uint32_t)index,0,&activation)||!activation.hint)return 0;
    hint_sector=sector;
    if(activation.sound)ae_audio_play_effect((int)activation.sound);
    printf("INDICATOR manual hint preview index=%d sector=%d (player mode/stage transition not applied)\n",index,sector);
    return 1;
}
static void events_update(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT: case SDL_EVENT_WINDOW_CLOSE_REQUESTED: running=0;break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: width=e.window.data1;height=e.window.data2;break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: dragging=0;movement_accumulator=0;camera_input_clear();SDL_CaptureMouse(false);break;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.repeat)break;
            switch(e.key.scancode) {
            case SDL_SCANCODE_H:
                if(indicator_world)indicator_hint_preview(-1);else ae_ui_hint(hint_sector);
                break;
            case SDL_SCANCODE_RIGHTBRACKET:++hint_sector;ae_ui_hint(hint_sector);break;
            case SDL_SCANCODE_LEFTBRACKET:if(hint_sector>0)--hint_sector;ae_ui_hint(hint_sector);break;
            case SDL_SCANCODE_BACKSPACE:ae_ui_press(0x800);break;
            case SDL_SCANCODE_Y:ae_audio_play_effect(ae_audio_stage_effect());break;
            case SDL_SCANCODE_ESCAPE:running=0;break;
            case SDL_SCANCODE_C:colliders=!colliders;break;
            case SDL_SCANCODE_T:textured=!textured;break;
            case SDL_SCANCODE_HOME:
                reset_camera();
#ifdef AE_NATIVE_PLAYER
                if(camera_follow_on()&&!native_camera_reset())running=0;
#endif
                break;
            case SDL_SCANCODE_F:
                if(actor)focus_actor();
#ifdef AE_NATIVE_PLAYER
                if(actor&&camera_follow_on()&&!native_camera_reset())running=0;
#endif
                break;
#ifdef AE_NATIVE_PLAYER
            case SDL_SCANCODE_V:if(game_mode)game_free_camera=!game_free_camera;else native_camera_toggle();break;
#endif
            case SDL_SCANCODE_N:
#ifdef AE_NATIVE_PLAYER
                if(native_player_mode)break;
#endif
                if(actor){actor_clip=(actor_clip+1)%actor_count;actor_time=0;}break;
            case SDL_SCANCODE_SPACE:
#ifdef AE_NATIVE_PLAYER
                if(native_player_mode)break;
#endif
                if(actor)actor_paused=!actor_paused;
                break;
            case SDL_SCANCODE_P:
                if(game_mode)break;
                if(actor&&ground&&wall_supported){
                    player_demo=!player_demo;movement_accumulator=0;camera_input_clear();
#ifdef AE_NATIVE_PLAYER
                    if(!player_demo)native_camera_mode=0;
#endif
                    focus_actor();
                }
                break;
            default:break;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if(e.button.button==SDL_BUTTON_RIGHT){dragging=e.button.down;SDL_CaptureMouse(dragging!=0);}break;
        case SDL_EVENT_MOUSE_MOTION:
#ifdef AE_NATIVE_PLAYER
            if(dragging&&camera_follow_on()){camera_mouse_x=clamp127((long)camera_mouse_x+lroundf(e.motion.xrel));camera_mouse_y=clamp127((long)camera_mouse_y+lroundf(e.motion.yrel));break;}
#endif
            if(dragging){camera.yaw-=e.motion.xrel*.3f;camera.pitch=fmaxf(-85,fminf(85,camera.pitch+e.motion.yrel*.3f));}break;
        case SDL_EVENT_MOUSE_WHEEL:
            if(camera_follow_on())break; /* original follow distance is controlled by the update */
            camera.distance=fmaxf(.5f,fminf(10000,camera.distance*powf(.85f,e.wheel.y*(e.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1:1))));break;
        case SDL_EVENT_GAMEPAD_ADDED:gamepad_open(e.gdevice.which);break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if(gamepad&&SDL_GetGamepadID(gamepad)==e.gdevice.which){SDL_CloseGamepad(gamepad);gamepad=NULL;}break;
        default:break;
        }
    }
}
/* opengl32 exports only GL 1.1 on Windows: blend constant/equation entry points come from the context. */
#define AE_GL_CONSTANT_COLOR 0x8001
#define AE_GL_FUNC_ADD 0x8006
#define AE_GL_FUNC_REVERSE_SUBTRACT 0x800B
typedef void (APIENTRY *AeBlendColorFn)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY *AeBlendEquationFn)(GLenum);
static AeBlendColorFn blend_color_fn;
static AeBlendEquationFn blend_equation_fn;
static int blend_state_dirty;
static int blend_procs_load(uint32_t modes) {
    if (modes & 9u) { /* average and quarter-add use the blend constant */
        blend_color_fn = (AeBlendColorFn)SDL_GL_GetProcAddress("glBlendColor");
        if (!blend_color_fn) return fail("prop semi blending needs glBlendColor, unavailable in this OpenGL context");
    }
    if (modes & 4u) { /* subtract needs the reverse-subtract equation */
        blend_equation_fn = (AeBlendEquationFn)SDL_GL_GetProcAddress("glBlendEquation");
        if (!blend_equation_fn) return fail("prop semi blending needs glBlendEquation, unavailable in this OpenGL context");
    }
    return 1;
}
/* Source-semi texel blend on top of the opaque layer already in the framebuffer (host OpenGL 2.1 equivalents). */
static void blend_enter(uint32_t mode) {
    glEnable(GL_BLEND); blend_state_dirty=1;
    switch (mode) {
    case 0: blend_color_fn(.5f,.5f,.5f,.5f); glBlendFunc(AE_GL_CONSTANT_COLOR,AE_GL_CONSTANT_COLOR); break; /* B/2+F/2 */
    case 1: glBlendFunc(GL_ONE,GL_ONE); break; /* B+F */
    case 2: blend_equation_fn(AE_GL_FUNC_REVERSE_SUBTRACT); glBlendFunc(GL_ONE,GL_ONE); break; /* B-F */
    default: blend_color_fn(.25f,.25f,.25f,.25f); glBlendFunc(AE_GL_CONSTANT_COLOR,GL_ONE); break; /* B+F/4 */
    }
}
static void blend_leave(void) {
    glDisable(GL_BLEND);
    if (blend_equation_fn) blend_equation_fn(AE_GL_FUNC_ADD);
}
static void blend_restore(void) {
    if (!blend_state_dirty) return;
    blend_leave();
    if (blend_color_fn) blend_color_fn(0,0,0,0);
    glBlendFunc(GL_ONE,GL_ZERO);
    blend_state_dirty=0;
}
static int graphics_init(int hidden) {
    SDL_SetMainReady();
    if(!directinput)SDL_SetHint(SDL_HINT_JOYSTICK_DIRECTINPUT,"0");
    if(!SDL_Init(keyboard_only?SDL_INIT_VIDEO:(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD)))return fail(SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    window_handle=SDL_CreateWindow("Ape Escape native area viewer",width,height,
        SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|(hidden?SDL_WINDOW_HIDDEN:0));
    if(!window_handle)return fail(SDL_GetError());
    context=SDL_GL_CreateContext(window_handle);
    if(!context||!SDL_GL_MakeCurrent(window_handle,context))return fail(SDL_GetError());
    if(!SDL_GL_SetSwapInterval(hidden?0:1))SDL_ClearError();
    SDL_GetWindowSizeInPixels(window_handle,&width,&height);
    if(!keyboard_only){
        int count=0;SDL_JoystickID *ids=SDL_GetGamepads(&count);
        for(int i=0;i<count&&!gamepad;++i)gamepad_open(ids[i]);
        SDL_free(ids);
    }
    glGenTextures((GLsizei)scene.nt,scene.textures);
    for (uint32_t i=0; i<scene.nt; ++i) {
        glBindTexture(GL_TEXTURE_2D,scene.textures[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,(GLsizei)scene.width[i],(GLsizei)scene.height[i],0,GL_RGBA,GL_UNSIGNED_BYTE,scene.pixels[i]);
    }
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,.01f);
    glDisable(GL_CULL_FACE); /* Existing exporter winding includes both orientations. */
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    if (glGetError()!=GL_NO_ERROR) return fail("OpenGL texture/state initialization failed");
    if (scene.blend_modes && !blend_procs_load(scene.blend_modes)) return 0;
    if (actor && !ae_actor_upload(actor)) return fail(ae_actor_error());
    if (monkey_actor && !ae_actor_upload(monkey_actor)) return fail(ae_actor_error());
    if (indicator_world && !ae_indicator_world_upload(indicator_world)) return fail(ae_indicator_world_error());
    if (!ae_ui_upload()) return fail("UI texture upload failed");
    if (!hidden) SDL_ShowWindow(window_handle);
    return 1;
}
static void graphics_free(void) {
    ae_audio_stop();
    ae_ui_free();
#ifdef AE_NATIVE_PLAYER
    stage_camera_free(); /* before the ground / collision the host queries borrow */
    ae_player_host_free(&player_host);
#endif
    ae_indicator_world_free(indicator_world); indicator_world=NULL; /* before the GL context goes */
    ae_actor_free(actor); actor=NULL;
    ae_actor_free(monkey_actor); monkey_actor=NULL;
    ae_ground_free(ground); ground=NULL;
    ae_collision_free(collision); collision=NULL;
    scene_free(&scene);
    if(gamepad){SDL_CloseGamepad(gamepad);gamepad=NULL;}
    if(context){SDL_GL_DestroyContext(context);context=NULL;}
    if(window_handle){SDL_DestroyWindow(window_handle);window_handle=NULL;}
    SDL_Quit();
}
static void bind_vertex_pointers(const Vertex *vertices) {
    glVertexPointer(3,GL_FLOAT,sizeof(Vertex),&vertices[0].x);
    glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),&vertices[0].u);
    glColorPointer(3,GL_FLOAT,sizeof(Vertex),&vertices[0].r);
}
/* Draw one batch whose vertices start at index `first` of the currently bound arrays (state setup, opaque draw,
 * resolved untextured semi draw and split semi layer; unchanged from the former inline loop). */
static void draw_batch(const Batch *b, uint32_t first) {
    if (b->kind==2 && !colliders) return;
    if (b->kind==2) {
        glDisable(GL_TEXTURE_2D); glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
        glDisableClientState(GL_COLOR_ARRAY); glColor3f(1,.3f,.1f);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); glEnableClientState(GL_COLOR_ARRAY);
        if (textured && b->texture!=UINT32_MAX) {
            glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,scene.textures[b->texture]);
        } else glDisable(GL_TEXTURE_2D);
    }
    if (textured && b->kind==1 && (b->flags&BATCH_RESOLVED_MODE)) {
        /* Resolved untextured semi (source draw-mode byte): one blended draw, no texture; state restored below. */
        blend_enter((b->flags&BATCH_MODE_MASK)>>BATCH_MODE_SHIFT);
        glDrawArrays(GL_TRIANGLES,(GLint)first,(GLsizei)b->count);
        blend_leave();
        return;
    }
    glDrawArrays(GL_TRIANGLES,(GLint)first,(GLsizei)b->count);
    if (textured && b->kind!=2 && b->semi_texture!=UINT32_MAX) {
        /* Semi texels (source CLUT bit 15) go over the opaque layer drawn above; both layers have alpha 0/255. */
        glBindTexture(GL_TEXTURE_2D,scene.textures[b->semi_texture]);
        blend_enter((b->flags&BATCH_MODE_MASK)>>BATCH_MODE_SHIFT);
        glDrawArrays(GL_TRIANGLES,(GLint)first,(GLsizei)b->count);
        blend_leave();
    }
}
/* Opt-in (AEVIEW3) background pass for the original special sky props, type 4/5. Host policy: depth test off and
 * depth write off (the original pins the OT bucket far/background); the view ROTATION (same pitch/yaw as the world)
 * is kept but there is no distance or target translation, matching the zero view translation. Type 4 Y follows the
 * player: playerY = wrap16(round(-actor_position[1]*256)) (actor_position[1] is render-up = -PSX Y/256); without an
 * actor playerY is 0. camera.target is NOT used: it includes eye anchors and offsets. The scratch copy is rebuilt
 * from the immutable stored vertices every draw. Matrix, depth state and array pointers are restored. */
static int draw_sky(void) {
    int16_t player_y=0;
    int ok=1;
    if (!scene.sky_batches) return 1;
    if (actor) {
        double d=floor(-(double)actor_position[1]*256.0+.5);
        if (!(d>=-2147483648.0 && d<=2147483647.0)) return fail("sky player Y out of range");
        player_y=wrap_s16((int32_t)d);
    }
    glPushMatrix(); glLoadIdentity();
    glScalef(-1,1,1);
    glRotatef(camera.pitch,1,0,0); glRotatef(camera.yaw,0,1,0);
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
    bind_vertex_pointers(scene.sky_scratch);
    for (uint32_t i=0; ok && i<scene.nb; ++i) {
        const Batch *b=scene.batches+i;
        if (!b->sky_type) continue;
        if (!scene.sky_scratch || b->count>scene.sky_max_vertices || b->count>scene.nv-b->first) { ok=0; break; }
        for (uint32_t j=0; j<b->count; ++j) {
            const Vertex *src=scene.vertices+b->first+j;
            Vertex *dst=scene.sky_scratch+j;
            int16_t in[3], out[3];
            if (!sky_to_s16(src->x,0,in) || !sky_to_s16(src->y,1,in+1) || !sky_to_s16(src->z,0,in+2) ||
                !ae_sky_vertex((int)b->sky_type,0,player_y,in,out)) { ok=0; break; }
            *dst=*src;
            dst->x=out[0]/256.f; dst->y=-out[1]/256.f; dst->z=out[2]/256.f;
        }
        if (ok) draw_batch(b,0);
    }
    blend_restore();
    glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glPopMatrix();
    bind_vertex_pointers(scene.vertices);
    return ok ? 1 : fail("sky batch transform failed");
}
#ifdef AE_NATIVE_PLAYER
static void game_debug_marker(void *user,const char *kind,const int32_t pos[3]){
    (void)user;float x=pos[0]/256.f,y=-pos[1]/256.f,z=pos[2]/256.f;
    if(!strcmp(kind,"enemy"))glColor3f(1,.2f,.2f);
    else if(!strcmp(kind,"item"))glColor3f(1,1,0);
    else if(!strcmp(kind,"projectile"))glColor3f(1,0,1);
    else glColor3f(0,1,1);
    glBegin(GL_LINES);glVertex3f(x-.2f,y,z);glVertex3f(x+.2f,y,z);
    glVertex3f(x,y-.2f,z);glVertex3f(x,y+.2f,z);glVertex3f(x,y,z-.2f);glVertex3f(x,y,z+.2f);glEnd();
}
#endif
static int draw(void) {
    double near_plane=.1, far_plane=4096, top, aspect;
    if (width<1 || height<1) return 1;
    aspect=(double)width/height; top=near_plane*tan(55.0*3.141592653589793/360.0);
    glViewport(0,0,width,height); glClearColor(.12f,.18f,.26f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glFrustum(-top*aspect,top*aspect,-top,top,near_plane,far_plane);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    /* Assets map PSX (x,y,z) to (x,-y,z). Restore the camera's handedness
     * in view space, shared by scenery, actor and collider outlines. */
    glScalef(-1,1,1);
    glTranslatef(0,0,-camera.distance); glRotatef(camera.pitch,1,0,0); glRotatef(camera.yaw,0,1,0);
    glTranslatef(-camera.target[0],-camera.target[1],-camera.target[2]);
    if (!draw_sky()) return 0;
    bind_vertex_pointers(scene.vertices);
    for (uint32_t i=0; i<scene.nb; ++i) {
        Batch *b=scene.batches+i;
        if (b->sky_type) continue; /* drawn by the background pass above */
        draw_batch(b,b->first);
    }
    blend_restore();
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); glEnableClientState(GL_COLOR_ARRAY);
    /* World indicators: depth tested like the scene; each actor draw sets its own array pointers. */
    if (indicator_world && !(indicator_proximity?ae_indicator_world_draw_stepped(indicator_world,textured):ae_indicator_world_draw(indicator_world,textured))) return fail(ae_indicator_world_error());
    if (actor) {
        uint32_t frames=ae_actor_frames(actor,actor_clip);
        uint32_t frame=(uint32_t)(actor_time*ae_actor_fps(actor,actor_clip))%frames;
#ifdef AE_NATIVE_PLAYER
        if(native_player_mode) {
            const AePlayer *p=&player_host.player;
            const AePlayerStateEntry *state=ae_player_state_entry(p,&player_host.data,p->state);
            frame=state&&p->anim_frame>=state->first_frame?p->anim_frame-state->first_frame:0;
            if(frame>=frames)frame=frames-1;
        }
#endif
        if (!ae_actor_pose(actor,actor_clip,frame)) return fail(ae_actor_error());
        ae_actor_draw(actor,textured,actor_position,actor_yaw);
    }
#ifdef AE_NATIVE_PLAYER
    if(game_mode&&monkey_actor){
        AeRam *r=game_session.ram;
        for(uint32_t i=0;i<AE_MONKEY_COUNT;++i){uint32_t m=AE_G_MONKEYS+i*AE_MONKEY_SIZE;
            if(ae_rd8(r,m+AE_MK_ACTIVITY)==2||(ae_rd32(r,m+AE_MK_FLAGS)&0x80000000u)||(ae_rd32(r,m+AE_MK_FLAGS2)&0x4000u))continue;
            float pos[3]={ae_rds32(r,m+AE_MK_POS)/256.f,-ae_rds32(r,m+AE_MK_POS+4)/256.f,ae_rds32(r,m+AE_MK_POS+8)/256.f};
            uint32_t clip=0;char name[32];snprintf(name,sizeof(name),"monkey_set0_clip%02X",ae_rd16(r,m+AE_MK_ANIM_INDEX));
            uint32_t info[8];ae_actor_info(monkey_actor,info);
            for(uint32_t j=0;j<info[3];++j)if(!strcmp(ae_actor_clip_name(monkey_actor,j),name)){clip=j;break;}
            uint32_t n=ae_actor_frames(monkey_actor,clip);uint32_t f=ae_rd16(r,m+AE_MK_ANIM_FRAME);if(n&&f>=n)f=n-1;
            if(!ae_actor_pose(monkey_actor,clip,f))return fail(ae_actor_error());
            float yaw=180+atan2f((float)ae_rds16(r,m+AE_MK_MOVE_DIR),(float)ae_rds16(r,m+AE_MK_MOVE_DIR+4))*180.f/3.14159265358979323846f;
            ae_actor_draw(monkey_actor,textured,pos,yaw);
        }
    }
#endif
#ifdef AE_NATIVE_PLAYER
    if(game_mode){glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_LINE_BIT);glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);
        glLineWidth(3);viewer_game_markers(&game_session,game_debug_marker,NULL);glPopAttrib();}
#endif
    glFinish();
    return glGetError()==GL_NO_ERROR ? 1 : fail("OpenGL draw failed");
}
static void json_string(FILE *file, const unsigned char *value) {
    fputc('"',file);
    if (value) for (; *value; ++value) {
        if (*value=='"' || *value=='\\') fputc('\\',file);
        if (*value>=32) fputc(*value,file);
    }
    fputc('"',file);
}
static int capture(const char *path) {
    size_t n=(size_t)width*height, written=0;
    unsigned char *pixels=(unsigned char *)malloc(n*3);
    float *depth=(float *)malloc(n*sizeof(float));
    uint32_t covered=0;
    double sum=0, sum2=0;
    FILE *file;
    char *report;
    if (!pixels || !depth) { free(pixels); free(depth); return fail("capture allocation failed"); }
    glReadBuffer(GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels);
    glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depth);
    if (glGetError()!=GL_NO_ERROR) { free(pixels); free(depth); return fail("framebuffer read failed"); }
    for (size_t i=0; i<n; ++i) {
        double luma=(pixels[i*3]+pixels[i*3+1]+pixels[i*3+2])/3.0;
        sum+=luma; sum2+=luma*luma;
        if (depth[i]<.999999f) ++covered;
    }
    file=fopen(path,"wb");
    if (!file) { free(pixels); free(depth); return fail("cannot write capture"); }
    fprintf(file,"P6\n%d %d\n255\n",width,height);
    for (int y=height-1; y>=0; --y) written+=fwrite(pixels+(size_t)y*width*3,1,(size_t)width*3,file);
    int closed=fclose(file);
    free(pixels); free(depth);
    if (written!=n*3 || closed) return fail("capture write failed");
    report=(char *)malloc(strlen(path)+6);
    if (!report) return fail("report allocation failed");
    sprintf(report,"%s.json",path); file=fopen(report,"wb"); free(report);
    if (!file) return fail("cannot write capture report");
    fprintf(file,"{\n  \"area\": %u, \"member\": %u, \"width\": %d, \"height\": %d,\n"
                 "  \"terrain_triangles\": %u, \"prop_triangles\": %u, \"textures\": %u,\n"
                 "  \"covered_pixels\": %u, \"luma_variance\": %.6f,\n"
                 "  \"textured\": %s, \"colliders\": %s,\n"
                 "  \"yaw\": %.3f, \"pitch\": %.3f, \"distance\": %.3f,\n  \"renderer\": ",
            scene.area,scene.member,width,height,scene.terrain,scene.props,scene.nt,covered,sum2/n-(sum/n)*(sum/n),
            textured?"true":"false",colliders?"true":"false",camera.yaw,camera.pitch,camera.distance);
    json_string(file,glGetString(GL_RENDERER)); fprintf(file,",\n  \"vendor\": "); json_string(file,glGetString(GL_VENDOR));
    fprintf(file,",\n  \"opengl\": "); json_string(file,glGetString(GL_VERSION));
    fprintf(file,",\n  \"actor\": %s, \"actor_clip\": %u, \"actor_time\": %.6f",actor?"true":"false",actor_clip,actor_time);
#ifdef AE_NATIVE_PLAYER
    fprintf(file,",\n  \"game\": %s",game_mode?"true":"false");
    if(game_mode){
        AeRam *r=game_session.ram;
        fprintf(file,",\n  \"game_frames\": %u, \"game_ram_error\": %d, \"game_missing_count\": %u",
                game_session.frame,ae_ram_failed(r),viewer_game_missing_count(&game_session));
        fprintf(file,",\n  \"game_camera_eye\": [%d,%d,%d], \"game_camera_target\": [%d,%d,%d]",
                ae_rds16(r,0x800F45A8u),ae_rds16(r,0x800F45AAu),ae_rds16(r,0x800F45ACu),
                ae_rds16(r,0x800F45B0u),ae_rds16(r,0x800F45B2u),ae_rds16(r,0x800F45B4u));
    }
    fprintf(file,",\n  \"native_camera\": %s",camera_follow_on()?"true":"false");
    if (camera_follow_on())
        fprintf(file,",\n  \"camera_eye\": [%d,%d,%d], \"camera_target\": [%d,%d,%d], \"camera_status\": %u",
                ae_camera_i16(&follow_host.slot,AE_CAM_EYE),ae_camera_i16(&follow_host.slot,AE_CAM_EYE+2),ae_camera_i16(&follow_host.slot,AE_CAM_EYE+4),
                ae_camera_i16(&follow_host.slot,AE_CAM_TARGET),ae_camera_i16(&follow_host.slot,AE_CAM_TARGET+2),ae_camera_i16(&follow_host.slot,AE_CAM_TARGET+4),camera_status);
    stage_camera_json(file);
#else
    fprintf(file,",\n  \"native_camera\": false, \"stage_camera\": false");
#endif
    fprintf(file,"\n}\n");
    if (fclose(file)) return fail("capture report write failed");
    printf("CAPTURE area=%02X covered=%u/%u textures=%u file=%s\n",scene.area,covered,(unsigned)n,scene.nt,path);
    return covered>100 && sum2/n-(sum/n)*(sum/n)>1 ? 1 : fail("capture contains no meaningful geometry");
}
static void move_actor(float dx,float dz,float dt) {
    float length=sqrtf(dx*dx+dz*dz);
    AeGroundHit hit; AeSolidHit contact; AeMoveHit move;
    uint32_t clip=length>0 ? 3 : 0;
    if (actor_count<=3) clip=0;
    if (clip!=actor_clip) { actor_clip=clip; actor_time=0; }
    if (length>0) {
        int32_t pos[3]={(int32_t)lroundf(actor_position[0]*256),(int32_t)lroundf(-actor_position[1]*256),(int32_t)lroundf(actor_position[2]*256)};
        int32_t vel[3]={(int32_t)lroundf(dx/length*6*dt*256),0,(int32_t)lroundf(dz/length*6*dt*256)};
        if(!ae_collision_resolve(collision,pos,vel,128,512,&move)) { fail(ae_collision_error());running=0;return; }
        if(move.contact)++movement_contacts;
        if (ae_collision_ground_height(collision,ground,move.position[0],pos[1],move.position[2],0,-1,0,&hit,&contact) && abs(hit.height-pos[1])<=128) {
            actor_position[0]=move.position[0]/256.f; actor_position[1]=-hit.height/256.f; actor_position[2]=move.position[2]/256.f;
            actor_yaw=180+atan2f(dx,dz)*180/3.14159265358979323846f;
            if(move.position[0]==pos[0]&&move.position[2]==pos[2])++movement_blocked;else ++movement_accepted;
        } else ++movement_blocked;
    }
    for (int i=0; i<3; ++i) camera.target[i]=actor_position[i];
    camera.target[1]+=.6f;
}
#ifdef AE_NATIVE_PLAYER
/* One original 30 Hz world step, observed after PlayerUpdate and before body integration.
 * Stage exit 0 and message-busy (ae_ui_state) are host proxies; touchedBy is unknown (never
 * inferred from grid/player contact) so the library reports missing-touch bits instead.
 * Only the local animation (already applied by the step) and PlaySfx are executed. */
static int indicator_proximity_tick(void *user,const int32_t player[3]) {
    AeIndicatorRuntimeEvents events[4];uint32_t missing=0,count=ae_indicator_world_count(indicator_world),new_mask=0;
    (void)user;
    ae_indicator_world_tick(indicator_world);
    if(!ae_indicator_world_proximity_step(indicator_world,player,0,ae_ui_state()!=0,NULL,0,events,&missing)){
        prox.failed=1;fail(ae_indicator_world_error());return 0;
    }
    if(count>4)count=4;
    for(uint32_t i=0;i<count;++i){
        const AeIndicatorRuntimeEvents *e=&events[i];
        uint32_t request=0;
        if(e->trigger.activate){++prox.activations;request=1;}
        if(e->activation.hint){++prox.hints;new_mask|=PROX_REQ_HINT;request=1;}
        if(e->activation.pickups){++prox.pickups;new_mask|=PROX_REQ_PICKUPS;request=1;}
        if(e->trigger.effect){++prox.effects;new_mask|=PROX_REQ_EFFECT;request=1;}
        if(e->activation.sound)request=1;
        if(!request)continue;
        fprintf(stderr,"INDICATOR request tick=%u index=%u activate=%u hint=%u hint_sector=%d pickups=%u effect=%u sound=%u (hint/pickups/effect NOT executed)\n",
                prox.ticks+1,i,e->trigger.activate,e->activation.hint,e->activation.sector,e->activation.pickups,e->trigger.effect,e->activation.sound);
        if(e->activation.sound)ae_audio_play_effect((int)e->activation.sound);
    }
    if(new_mask&~prox.warned_mask){
        prox.warned_mask|=new_mask;
        fprintf(stderr,"INDICATOR unexecuted request mask now 0x%X (1=hint display, 2=pickups, 4=effect 0x112): hint/pickup/effect protocol is missing\n",prox.warned_mask);
    }
    prox.request_mask|=new_mask;
    if(missing!=prox.missing_mask)
        fprintf(stderr,"INDICATOR missing-touch mask 0x%X -> 0x%X tick=%u (touchedBy unknown; reachable touch branch not evaluated)\n",prox.missing_mask,missing,prox.ticks+1);
    prox.missing_mask=missing;
    memcpy(prox.position,player,sizeof(prox.position));
    ++prox.ticks;
    return 1;
}
#define VIEWER_MONKEY_NET_GADGET 1
/* g_CameraRotMatrix for the next PadUpdate: the original builds it in CameraLookAt (the end of the PREVIOUS frame) from the
 * camera eye / target, so it is computed from the camera state as it is before this tick's camera step. With the native follow
 * camera the eye / target are its slot's s16 values; with the viewer's free orbit camera there is no original integer
 * eye / target, so they are derived from the float orbit (target + dist * (-sin yaw * cos pitch, sin pitch, cos yaw * cos pitch),
 * x 256, y negative-up, rounded to s16). Only the horizontal direction eye -> target matters (a pure yaw matrix). */
static void pad_camera_update(void) {
    int16_t eye[3],target[3];
    if(camera_follow_on()){
        for(int i=0;i<3;++i){eye[i]=ae_camera_i16(&follow_host.slot,AE_CAM_EYE+2*i);target[i]=ae_camera_i16(&follow_host.slot,AE_CAM_TARGET+2*i);}
    }else{
        const float rad=3.14159265358979323846f/180.f,yaw=camera.yaw*rad,pitch=camera.pitch*rad;
        const float delta[3]={-sinf(yaw)*cosf(pitch)*camera.distance,sinf(pitch)*camera.distance,cosf(yaw)*cosf(pitch)*camera.distance};
        for(int i=0;i<3;++i){
            const float sign=i==1?-1.f:1.f; /* viewer y is up, PSX y is negative-up */
            target[i]=(int16_t)lroundf(sign*camera.target[i]*256.f);
            eye[i]=(int16_t)lroundf(sign*(camera.target[i]+delta[i])*256.f);
        }
    }
    ae_player_host_set_camera(&player_host,eye,target);
}
/* One 30 Hz tick of the original player: the raw pad packet (held word + analog bytes 4..7) goes through the ported PadUpdate. */
static void original_player_tick(uint16_t held,uint8_t rx,uint8_t ry,uint8_t lx,uint8_t ly) {
    uint32_t old_update=player_host.update_flags,old_body=player_host.body_flags,old_volume=player_host.volume.status;
    if(actor_paused&&!game_mode){
        /* The world clock is independent of the debug actor pause: one callback per logical tick. */
        if(indicator_proximity&&!indicator_proximity_tick(NULL,player_host.player.pos))running=0;
        return;
    }
    if(!game_mode)pad_camera_update();
    if(camera_follow_on()&&!native_camera_step()){running=0;return;}
    if(native_camera_requested)camera_cache_render_position();
    AePlayerHostPad hp;memset(&hp,0,sizeof(hp));
    hp.held=held;hp.rx=rx;hp.ry=ry;hp.lx=lx;hp.ly=ly;
    if(game_mode ? !viewer_game_step(&game_session,&hp) : !ae_player_host_step_input(&player_host,&hp,indicator_proximity?indicator_proximity_tick:NULL,NULL)){
        if(game_mode)fail(ae_ram_error(game_session.ram));
        if(!prox.failed)fail(ae_collision_error());
        running=0;return;
    }
    AePlayer *p=&player_host.player;
    if(!ae_player_state_entry(p,&player_host.data,p->state)){fail("native player state outside loaded table");running=0;return;}
    actor_position[0]=p->pos[0]/256.f;actor_position[1]=-p->pos[1]/256.f;actor_position[2]=p->pos[2]/256.f;
    actor_yaw=180+atan2f(p->facing[0],p->facing[2])*180/3.14159265358979323846f;
    /* clips are named setN_stateXX: set 0 = no gadget, set g+1 = gadget g (states 0x80 + XX) */
    char prefix[32];
    if(p->state>=0x80)snprintf(prefix,sizeof(prefix),"set%d_state%02x",p->current_gadget+1,p->state-0x80);
    else snprintf(prefix,sizeof(prefix),"set0_state%02x",p->state);
    for(uint32_t i=0;i<actor_count;++i)if(!strncmp(ae_actor_clip_name(actor,i),prefix,strlen(prefix))&&(ae_actor_clip_name(actor,i)[strlen(prefix)]==0||ae_actor_clip_name(actor,i)[strlen(prefix)]=='_')){actor_clip=i;break;}
    if(p->contact)++movement_contacts;
    ++movement_accepted;
    if(!game_mode&&!camera_follow_on()){
        for(int i=0;i<3;++i)camera.target[i]=actor_position[i];
        camera.target[1]+=.6f;
    }
    if(old_update!=player_host.update_flags||old_body!=player_host.body_flags||old_volume!=player_host.volume.status)
        fprintf(stderr,"PLAYER prototype omissions update=0x%X body=0x%X volume=0x%X state=0x%X frame=%u\n",player_host.update_flags,player_host.body_flags,player_host.volume.status,p->state,player_host.world.frame_counter);
}
#endif
static void view_motion(float right,float forward,float *dx,float *dz) {
    float angle=camera.yaw*3.14159265358979323846f/180.f;
    /* Inverse of the reflected view basis: screen-right is negative view X. */
    *dx=-right*cosf(angle)-forward*sinf(angle);
    *dz=-right*sinf(angle)+forward*cosf(angle);
}
static void input_update(float dt) {
    float speed=camera.base_distance*.35f*dt*((SDL_GetModState()&SDL_KMOD_SHIFT)?3:1);
    float forward=0, right=0;
    if (!game_mode&&!(SDL_GetWindowFlags(window_handle)&SDL_WINDOW_INPUT_FOCUS))return;
    const bool *keys=SDL_GetKeyboardState(NULL);
    AeInput sample,pad;
    input_sample(&sample,&pad);
    /* Pad edges advance once per 30 Hz tick: pressed bits drive the message box and the actor pause toggle. */
    input_accumulator+=dt;
    while(input_accumulator>=1/30.f){
        input_accumulator-=1/30.f;
        ae_input_tick(&input_state,&sample);
        if(input_state.pressed){
            if(ae_ui_state())ae_ui_press(input_state.pressed);
            else if(!game_mode&&(input_state.pressed&AE_PAD_START)&&actor)actor_paused=!actor_paused;
        }
    }
#ifdef AE_NATIVE_PLAYER
    /* Virtual camera stick for the 30 Hz follow tick: right stick (arrow keys are +-64). Orientation is a documented guess: LEFT/UP negative. */
    camera_input_x=camera_input_y=0;
    /* With the Monkey Net equipped the right stick (or the arrow keys, at full tilt) swings the net, as in the original;
       the follow camera then gets no stick input. */
    int gadget_x=0,gadget_y=0;
    if(native_player_mode&&player_host.player.current_gadget==VIEWER_MONKEY_NET_GADGET){
        gadget_x=(int)pad.rx-0x80;gadget_y=(int)pad.ry-0x80;
        if(gadget_x>-24&&gadget_x<24&&gadget_y>-24&&gadget_y<24){
            gadget_x=(keys[SDL_SCANCODE_RIGHT]?127:0)-(keys[SDL_SCANCODE_LEFT]?127:0);
            gadget_y=(keys[SDL_SCANCODE_DOWN]?127:0)-(keys[SDL_SCANCODE_UP]?127:0);
        }
    } else if (camera_follow_on()) {
        camera_input_x=(int)sample.rx-0x80;
        camera_input_y=(int)sample.ry-0x80;
    }
#endif
    if (keys[SDL_SCANCODE_W]) forward-=speed;
    if (keys[SDL_SCANCODE_S]) forward+=speed;
    if (keys[SDL_SCANCODE_A]) right-=speed;
    if (keys[SDL_SCANCODE_D]) right+=speed;
    {
        float gx,gy,rx,ry;
        ae_input_stick(pad.lx,pad.ly,&gx,&gy);
        right+=gx*speed;forward+=gy*speed;
        rx=ae_input_axis(pad.rx);ry=ae_input_axis(pad.ry);
#ifdef AE_NATIVE_PLAYER
        if(!camera_follow_on()){
#endif
        if(rx!=0)camera.yaw-=rx*90*dt;
        if(ry!=0)camera.pitch=fmaxf(-85,fminf(85,camera.pitch+ry*60*dt));
#ifdef AE_NATIVE_PLAYER
        }
#endif
    }
    float dx,dz;view_motion(right,forward,&dx,&dz);
    if (player_demo && actor && ground) {
        movement_accumulator+=dt;
        while (movement_accumulator>=1/30.f) {
#ifdef AE_NATIVE_PLAYER
            if(native_player_mode) {
                /* Physical left stick (or WASD) = movement stick bytes 6 / 7, right stick = gadget stick bytes 4 / 5: the
                   raw packet goes through the ported PadUpdate (magnitude, class, camera-relative direction, gestures). */
                uint8_t rx=sample.rx,ry=sample.ry;
                if(gadget_x||gadget_y){rx=(uint8_t)(0x80+gadget_x);ry=(uint8_t)(0x80+gadget_y);}
                original_player_tick(sample.held,rx,ry,sample.lx,sample.ly);
            } else
#endif
            move_actor(dx,dz,1/30.f);
            movement_accumulator-=1/30.f;
        }
    }
    else {
        camera.target[0]+=dx; camera.target[2]+=dz;
        if (keys[SDL_SCANCODE_Q]) camera.target[1]-=speed;
        if (keys[SDL_SCANCODE_E]) camera.target[1]+=speed;
    }
    if (!camera_follow_on()) {
        if (keys[SDL_SCANCODE_LEFT]) camera.yaw+=60*dt;
        if (keys[SDL_SCANCODE_RIGHT]) camera.yaw-=60*dt;
        if (keys[SDL_SCANCODE_UP]) camera.pitch=fmaxf(-85,camera.pitch-60*dt);
        if (keys[SDL_SCANCODE_DOWN]) camera.pitch=fminf(85,camera.pitch+60*dt);
    }
}
static int indicators_load(const char *pack_path,const char *exe_path) {
    FILE *file=fopen(exe_path,"rb");
    long length;
    uint8_t *exe;
    int ok;
    if(!file)return fail("indicators: cannot open collision EXE");
    if(fseek(file,0,SEEK_END)||(length=ftell(file))<=0||length>(16L<<20)){fclose(file);return fail("indicators: collision EXE size invalid (1 B to 16 MiB)");}
    rewind(file);
    exe=(uint8_t *)malloc((size_t)length);
    if(!exe){fclose(file);return fail("indicators: EXE allocation failed");}
    if(fread(exe,1,(size_t)length,file)!=(size_t)length){free(exe);fclose(file);return fail("indicators: EXE read failed");}
    fclose(file);
    ok=ae_indicator_world_load(&indicator_world,pack_path,exe,(size_t)length,scene.area);
    free(exe); /* only the sine table was needed, at registration */
    if(!ok){
        char message[sizeof(error_text)];
        snprintf(message,sizeof(message),"indicator pack: %s",ae_indicator_world_error());
        return fail(message);
    }
    printf("INDICATORS area=%02X count=%u (presentation only: no activation, shadows, culling, messages or progress)\n",scene.area,ae_indicator_world_count(indicator_world));
    return 1;
}
static int number(const char *text, float *out) {
    char *end; double value=strtod(text,&end);
    if (*end || end==text || !isfinite(value) || fabs(value)>10000) return 0;
    *out=(float)value; return 1;
}
int main(int argc, char **argv) {
    const char *path=NULL, *capture_path=NULL, *actor_path=NULL, *ground_path=NULL, *walk_test=NULL, *monkey_actor_path=NULL;
    const char *collision_exe="extract/disc/SCUS_944.23";
    const char *music_bank=NULL;
    const char *ui_pack=NULL,*indicators_pack=NULL;int initial_hint=-1,indicator_hint=-1,effect_test=-1,ui_test=0;
#ifdef AE_NATIVE_PLAYER
    const char *player_member="extract/members/008.bin";
#endif
    int check=0, frames=0, result=1, focus=0, fixed_time=0, explicit_position=0;
    uint32_t entry=0;
    FILE *trace=NULL;
    float yaw=NAN, pitch=NAN, distance=NAN;
    float target[3]={NAN,NAN,NAN};
    Uint64 previous,now;
    char title[256];
    for (int i=1; i<argc; ++i) {
        if (!strcmp(argv[i],"--check")) check=1;
        else if (!strcmp(argv[i],"--colliders")) colliders=1;
        else if (!strcmp(argv[i],"--untextured")) textured=0;
        else if (!strcmp(argv[i],"--keyboard-only")) keyboard_only=1;
        else if (!strcmp(argv[i],"--directinput")) directinput=1;
        else if (!strcmp(argv[i],"--focus-actor")) focus=1;
        else if (!strcmp(argv[i],"--actor") && i+1<argc) actor_path=argv[++i];
        else if (!strcmp(argv[i],"--ground") && i+1<argc) ground_path=argv[++i];
        else if (!strcmp(argv[i],"--monkey-actor") && i+1<argc) monkey_actor_path=argv[++i];
        else if (!strcmp(argv[i],"--collision-exe") && i+1<argc) collision_exe=argv[++i];
        else if (!strcmp(argv[i],"--music-bank") && i+1<argc) music_bank=argv[++i];
        else if (!strcmp(argv[i],"--ui-pack") && i+1<argc) ui_pack=argv[++i];
        else if (!strcmp(argv[i],"--indicators-pack") && i+1<argc) indicators_pack=argv[++i];
        else if (!strcmp(argv[i],"--ui-test")) ui_test=1;
        else if (!strcmp(argv[i],"--hint") && i+1<argc) initial_hint=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--indicator-hint") && i+1<argc) indicator_hint=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--effect") && i+1<argc) effect_test=atoi(argv[++i]);
#ifdef AE_NATIVE_PLAYER
        else if (!strcmp(argv[i],"--game-state") && i+1<argc) game_state_path=argv[++i];
        else if (!strcmp(argv[i],"--game")) {game_mode=1;native_player_mode=1;player_demo=1;}
        else if (!strcmp(argv[i],"--native-player")) {native_player_mode=1;player_demo=1;}
        else if (!strcmp(argv[i],"--indicator-proximity")) {indicator_proximity=1;native_player_mode=1;player_demo=1;}
        else if (!strcmp(argv[i],"--player-member")&&i+1<argc)player_member=argv[++i];
        else if (!strcmp(argv[i],"--native-camera")) {native_camera_requested=1;native_player_mode=1;player_demo=1;}
        else if (!strcmp(argv[i],"--stage-camera")) {stage_camera_requested=1;native_camera_requested=1;native_player_mode=1;player_demo=1;}
#endif
        else if (!strcmp(argv[i],"--player")) player_demo=1;
        else if (!strcmp(argv[i],"--walk-test") && i+1<argc) walk_test=argv[++i];
        else if (!strcmp(argv[i],"--entry") && i+1<argc) {
            float value; if (!number(argv[++i],&value) || value<0 || value>=16 || value!=(int)value) goto usage;
            entry=(uint32_t)value;
        }
        else if (!strcmp(argv[i],"--clip") && i+1<argc) {
            float value; if (!number(argv[++i],&value) || value<0 || value!=(int)value) goto usage;
            actor_clip=(uint32_t)value;
        } else if (!strcmp(argv[i],"--time") && i+1<argc) {
            if (!number(argv[++i],&actor_time) || actor_time<0) goto usage;
            fixed_time=1;
        } else if (!strcmp(argv[i],"--actor-pos") && i+3<argc) {
            explicit_position=1;
            for (int j=0; j<3; ++j) if (!number(argv[++i],actor_position+j)) goto usage;
        } else if (!strcmp(argv[i],"--target") && i+3<argc) {
            for (int j=0; j<3; ++j) if (!number(argv[++i],target+j)) goto usage;
        }
        else if (!strcmp(argv[i],"--capture") && i+1<argc) capture_path=argv[++i];
        else if (!strcmp(argv[i],"--frames") && i+1<argc) {
            float value; if (!number(argv[++i],&value) || value<1 || value!=(int)value) goto usage;
            frames=(int)value;
        } else if ((!strcmp(argv[i],"--yaw") || !strcmp(argv[i],"--pitch") || !strcmp(argv[i],"--distance")) && i+1<argc) {
            float *value=!strcmp(argv[i],"--yaw")?&yaw:!strcmp(argv[i],"--pitch")?&pitch:&distance;
            if (!number(argv[++i],value)) goto usage;
        } else if (!path && argv[i][0]!='-') path=argv[i];
        else goto usage;
    }
    if (!path || (!isnan(pitch) && fabsf(pitch)>85) || (!isnan(distance) && distance<.5f) ||
        ((ground_path || player_demo || walk_test) && !actor_path) || (player_demo && !ground_path) ||
        (walk_test && (!player_demo || !frames))) goto usage;
    (void)monkey_actor_path;
#ifdef AE_NATIVE_PLAYER
    /* The follow camera owns target / yaw / pitch / distance; manual overrides are rejected. */
    if (native_camera_requested && (!isnan(yaw) || !isnan(pitch) || !isnan(distance) || !isnan(target[0]))) goto usage;
#endif
    if (!scene_load(&scene,path)) goto error;
    if(ui_pack&&!ae_ui_load(ui_pack,collision_exe,scene.area)){fail("UI pack validation failed");goto error;}
    if(initial_hint>=0&&!ae_ui_hint(initial_hint)){fail("hint sector invalid");goto error;}
    if(indicators_pack&&!indicators_load(indicators_pack,collision_exe))goto error;
    if(indicator_proximity){
        /* Validated before --check returns. */
        if(!indicator_world){fail("--indicator-proximity requires --indicators-pack");goto error;}
        if(!ae_indicator_world_has_runtime(indicator_world)){fail("--indicator-proximity requires a runtime (AEIND2) indicator pack");goto error;}
        if(ae_indicator_world_count(indicator_world)>4){fail("--indicator-proximity supports at most 4 indicators");goto error;}
    }
    if (actor_path) {
        uint32_t info[4]; actor=ae_actor_load(actor_path);
        if (!actor) { fail(ae_actor_error()); goto error; }
        ae_actor_info(actor,info); actor_count=info[3];
        if (actor_clip>=actor_count) { fail("actor clip out of range"); goto error; }
        printf("ACTOR nodes=%u joints=%u vertices=%u clips=%u\n",info[0],info[1],info[2],info[3]);
    } else if (focus || actor_clip || fixed_time) goto usage;
    if (ground_path) {
        int32_t source_entry[9]; AeGroundHit hit; AeSolidHit contact;uint32_t info[3],missing[2];
        ground=ae_ground_load(ground_path);
        if (!ground) { fail(ae_ground_error()); goto error; }
        collision=ae_collision_load(ground_path);
        if(!collision||!ae_collision_math_load(collision_exe)) { fail(ae_collision_error());goto error; }
        wall_supported=ae_collision_response_supported(collision,missing);
        printf("COLLISION floor=terrain+solids+water wall_supported=%d missing_shapes=%u missing_colliders=%u\n",wall_supported,missing[0],missing[1]);
        if(player_demo&&!wall_supported) { fail("area has unimplemented advanced wall responses; walking disabled");goto error; }
        ae_ground_info(ground,info);
        if (!ae_ground_entry(ground,entry,source_entry)) { fail(ae_ground_error()); goto error; }
        if (!explicit_position) {
            if (!ae_collision_ground_height(collision,ground,source_entry[0],source_entry[1],source_entry[2],0,-1,0,&hit,&contact)) {
                fail("entry has no terrain or solid floor"); goto error;
            }
            actor_position[0]=source_entry[0]/256.f; actor_position[1]=-hit.height/256.f; actor_position[2]=source_entry[2]/256.f;
            actor_yaw=180+atan2f((float)source_entry[6],(float)source_entry[8])*180/3.14159265358979323846f;
        }
        printf("GROUND flags=%u grid=%ux%u entry=%u actor=%.4f,%.4f,%.4f\n",info[0],info[1],info[2],entry,
               actor_position[0],actor_position[1],actor_position[2]);
    }
#ifdef AE_NATIVE_PLAYER
    if(native_player_mode) {
        int32_t pos[3]={(int32_t)lroundf(actor_position[0]*256),(int32_t)lroundf(-actor_position[1]*256),(int32_t)lroundf(actor_position[2]*256)};
        int32_t source[9];if(!ae_ground_entry(ground,entry,source)){fail(ae_ground_error());goto error;}
        int16_t facing[3]={(int16_t)source[6],(int16_t)source[7],(int16_t)source[8]};
        if(!ae_player_host_init(&player_host,ground,collision,player_member,collision_exe,scene.area,pos,facing)){fail("original player data loading failed");goto error;}
        /* gadget slots / unlock mask / the equipped gadget are the original new-game defaults (ae_player_host_init) */
        printf("PLAYER gadgets: slots Triangle=%d Square=%d Circle=%d Cross=%d unlock mask 0x%X, equipped %d (new-game defaults)\n",
               player_host.player.gadget_slots[0],player_host.player.gadget_slots[1],player_host.player.gadget_slots[2],
               player_host.player.gadget_slots[3],player_host.gadget_unlock_mask,player_host.player.current_gadget);
        printf("PLAYER original update+body mode=0 level=%u (prototype: volume queries wired; gameplay replay and player sound/effects pending)\n",player_host.world.current_level);
        if(indicator_proximity)
            printf("INDICATOR proximity EVENT/ANIMATION PROTOTYPE (not the complete gimmick protocol): world tick + trigger update after PlayerUpdate, before body integration; stage exit 0 and message-busy (ui state) are host proxies; touchedBy unknown (missing-touch mask reported); only local animation and PlaySfx executed, hint/pickup/effect requests are counted but NOT executed\n");
        if(native_camera_requested&&!game_mode){
            if(!native_camera_init(collision_exe,ground_path))goto error;
            if(stage_camera_requested)
                printf("CAMERA stage camera prototype: local regions / scripts / blend (CameraResetAll + CameraUpdate subsets; V toggles inspection)\n"
                       "CAMERA snapshots previous pre-collision render position; camera advances before movement/body integration\n"
                       "CAMERA projection is APPROXIMATE (float basis, focal 328, offset 192/120; not GTE) with a previous-value fallback\n"
                       "CAMERA INCOMPLETE: global camera events, indexed handlers except 3 and area05/11, selected aim/fixed slots, flight/RC/boss dispatch, full GTE projection; areas 0x48-0x4C rejected; pad bits and stage exit code are host proxies\n");
            else
            printf("CAMERA original follow (CameraFollowUpdate mode 0, verified AeCameraHost; V toggles inspection)\n"
                   "CAMERA snapshots previous pre-collision render position; camera advances before movement/body integration\n"
                   "CAMERA projection is APPROXIMATE (float basis, focal 328, offset 192/120; not GTE) with a previous-value fallback\n"
                   "CAMERA stage / region / script camera event dispatch is PENDING; pad bits and stage exit code are host proxies\n");
        }
    }
#endif
#ifdef AE_NATIVE_PLAYER
    if(game_mode){
        if(indicator_proximity){fail("--game uses RAM indicators; --indicator-proximity is a legacy prototype");goto error;}
        native_camera_requested=native_camera_mode=stage_camera_requested=0;
        if(!viewer_game_start(&game_session,&player_host,collision_exe,(int)scene.area,(int)entry)){fail(game_session.ram?ae_ram_error(game_session.ram):"game allocation failed");goto error;}
        game_session.audio_sync=ae_audio_sync;game_session.audio_stop=ae_audio_stop;
        if(monkey_actor_path){monkey_actor=ae_actor_load(monkey_actor_path);if(!monkey_actor){fail(ae_actor_error());goto error;}}
    }
#endif
    printf("LOADED area=%02X member=%03X vertices=%u batches=%u textures=%u terrain=%u props=%u\n",
           scene.area,scene.member,scene.nv,scene.nb,scene.nt,scene.terrain,scene.props);
    if (scene.version==3)
        printf("SKY batches=%u triangles=%u (background pass; host depth/order approximation, see native_sky_rendering.md)\n",
               scene.sky_batches,scene.sky_triangles);
    fflush(stdout);
    if (check) {
#ifdef AE_NATIVE_PLAYER
        if(game_session.ram){viewer_game_report(&game_session);viewer_game_stop(&game_session);}
#endif
        graphics_free();return 0;
    }
    reset_camera();
    if (focus) focus_actor();
    if (!isnan(yaw)) camera.yaw=yaw;
    if (!isnan(pitch)) camera.pitch=pitch;
    if (!isnan(distance)) camera.distance=distance;
    if (!isnan(target[0])) memcpy(camera.target,target,sizeof(target));
#ifdef AE_NATIVE_PLAYER
    /* reset_camera / focus_actor above would overwrite the follow orbit. */
    if (camera_follow_on() && !camera_apply_orbit()) goto error;
#endif
    if (!graphics_init(capture_path!=NULL || frames!=0)) goto error;
    if(music_bank&&!ae_audio_start(music_bank,collision_exe)){fail(ae_audio_error());goto error;}
    if(indicator_hint>=0&&!indicator_hint_preview(indicator_hint)){fail("indicator hint preview unavailable");goto error;}
    if(effect_test>=0&&ae_audio_play_effect(effect_test)<0){fail("effect id unavailable in area bank");goto error;}
    if (walk_test) {
        trace=fopen(walk_test,"wb");
        if (!trace) { fail("cannot open walking trace"); goto error; }
        fprintf(trace,"{\"area\":%u,\"entry\":%u,\"frames\":[\n",scene.area,entry);
    }
    snprintf(title,sizeof(title),"Ape Escape | Area %02X | WASD move | P camera/player | RMB orbit | F actor, N clip, Space pause | collision demo",scene.area);
#ifdef AE_NATIVE_PLAYER
    if(native_player_mode)snprintf(title,sizeof(title),"Ape Escape | Area %02X | Movement prototype | WASD move, Shift walk, Space jump | RMB camera",scene.area);
    if(indicator_proximity)snprintf(title,sizeof(title),"Ape Escape | Area %02X | Movement + indicator proximity event/animation prototype (hints/pickups/effects not executed) | WASD move, Shift walk, Space jump",scene.area);
    if(native_camera_requested)snprintf(title,sizeof(title),"Ape Escape | Area %02X | Movement + follow camera prototype | WASD move, Shift walk, Space jump | arrows/RMB orbit, V inspect",scene.area);
    if(stage_camera_requested)snprintf(title,sizeof(title),"Ape Escape | Area %02X | Stage camera prototype (local regions/scripts/blend; events, aim/fixed, flight incomplete) | WASD move, Space jump | arrows/RMB orbit, V inspect",scene.area);
#endif
    SDL_SetWindowTitle(window_handle,title);
    if(ui_pack){snprintf(title,sizeof(title),"Ape Escape | %s | Area %02X | H %s, [ ] select, Enter next, Backspace close, Y stage sound",ae_ui_title(),scene.area,indicator_world?"preview nearest marker":"hints");SDL_SetWindowTitle(window_handle,title);}
    previous=SDL_GetTicksNS();
    printf("PLATFORM SDL %d\n",SDL_GetVersion());
    printf("OPENGL %s\n",glGetString(GL_RENDERER)); fflush(stdout);
    int frame=0;
    while (running) {
        events_update();
        if(!running){if(error_text[0])goto error;break;} /* a failed camera reset must not look like a clean exit */
        now=SDL_GetTicksNS();
        float dt=(float)((double)(now-previous)/1000000000.0);
        previous=now;
        if(ui_test||(game_mode&&frames))dt=1/30.f;
        if (walk_test) {
            dt=1/30.f;
            int segment=(frame/30)%4;
            float dx,dz;
            view_motion(segment==1?1:segment==3?-1:0,segment==0?1:segment==2?-1:0,&dx,&dz);
#ifdef AE_NATIVE_PLAYER
            if(native_player_mode){
                if(game_mode){
                    AePlayerHostPad scripted;
                    viewer_game_script_input((uint32_t)frame,&scripted);
                    original_player_tick(scripted.held,scripted.rx,scripted.ry,scripted.lx,scripted.ly);
                }else{
                /* segment 0 / 2: stick down / up, 1 / 3: right / left. Frames 0..29 tilt 114 (PadUpdate magnitude 95, the old walk
                   test's 96), then the full deflection: raw byte 0xFF / 0x00 (the history stores 0x7F - raw for y, so only 0x00 up
                   reaches +127 and the magnitude 0x7F; a byte of 0x01 would stay below the run threshold 0x7D). */
                int p=frame<30?0x80+120:0xFF,n=frame<30?0x80-120:0x00;
                original_player_tick((frame>=60&&frame<66)?0x0A:0,0x80,0x80,(uint8_t)(segment==1?p:(segment==3&&frame%30>=12&&frame%30<26)?n:0x80),
                                     (uint8_t)((segment==0||(segment==3&&frame%30>=26))?p:(segment==2||(segment==3&&frame%30<12))?n:0x80));
                }
            }else
#endif
            move_actor(dx,dz,dt);
        } else input_update(fminf(dt,.1f));
#ifdef AE_NATIVE_PLAYER
        if(game_mode){char game_title[180];snprintf(game_title,sizeof(game_title),"Ape Escape | Game area %02X | missing %u (name/address in console) | V free camera | debug: enemy red, item yellow, projectile magenta, effect cyan",scene.area,viewer_game_missing_count(&game_session));SDL_SetWindowTitle(window_handle,game_title);}
        if(game_mode&&!game_free_camera){
            AeRam *r=game_session.ram;float delta[3];
            for(int i=0;i<3;++i){float sign=i==1?-1.f:1.f;camera.target[i]=sign*ae_rds16(r,0x800F45B0u+2u*i)/256.f;
                delta[i]=sign*ae_rds16(r,0x800F45A8u+2u*i)/256.f-camera.target[i];}
            camera.distance=sqrtf(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
            if(camera.distance>0){camera.yaw=-atan2f(delta[0],delta[2])*180.f/3.14159265358979323846f;camera.pitch=asinf(delta[1]/camera.distance)*180.f/3.14159265358979323846f;}
        }
#endif
        if(!running)goto error; /* movement failure must not become a successful smoke run */
        if (actor && !actor_paused && !fixed_time) actor_time+=fminf(dt,.1f);
        ui_accumulator+=fminf(dt,.1f);
        while(ui_accumulator>=1/30.0){if(ae_ui_tick())ae_audio_play_effect(0);ui_accumulator-=1/30.0;}
        /* Independent of player on/off and pause: every logical 30 Hz tick advances the world once. */
        if(indicator_world&&!indicator_proximity){
            indicator_accumulator+=fminf(dt,.1f);
            while(indicator_accumulator>=1/30.0){ae_indicator_world_tick(indicator_world);indicator_accumulator-=1/30.0;}
        }
        if (!draw()) goto error;
        ae_ui_draw(width,height);
        ++frame;
        if (trace) fprintf(trace,"%s{\"frame\":%d,\"xyz\":[%d,%d,%d],\"clip\":%u,\"accepted\":%u,\"blocked\":%u,\"contacts\":%u",
                           frame==1?"":",\n",frame,(int)lroundf(actor_position[0]*256),(int)lroundf(-actor_position[1]*256),
                           (int)lroundf(actor_position[2]*256),actor_clip,movement_accepted,movement_blocked,movement_contacts);
        if (trace&&indicator_world) {
            fprintf(trace,",\"indicator_frames\":[");
            for (uint32_t k=0;k<ae_indicator_world_count(indicator_world);++k)
                fprintf(trace,"%s%u",k?",":"",ae_indicator_world_frame(indicator_world,k));
            fprintf(trace,"]");
            if(indicator_proximity){
                fprintf(trace,",\"indicator_render_frames\":[");
                for(uint32_t k=0;k<ae_indicator_world_count(indicator_world);++k)fprintf(trace,"%s%u",k?",":"",ae_indicator_world_model_frame(indicator_world,k));
                fprintf(trace,"]");
                fprintf(trace,",\"indicator_proximity_ticks\":%u,\"indicator_tick_position\":[%d,%d,%d],\"indicator_missing_touch_mask\":%u,\"indicator_request_mask\":%u,"
                              "\"indicator_activation_requests\":%u,\"indicator_hint_requests\":%u,\"indicator_pickup_requests\":%u,\"indicator_effect_requests\":%u,\"indicator_trigger_states\":[",
                        prox.ticks,prox.position[0],prox.position[1],prox.position[2],prox.missing_mask,prox.request_mask,
                        prox.activations,prox.hints,prox.pickups,prox.effects);
                for (uint32_t k=0;k<ae_indicator_world_count(indicator_world);++k) {
                    AeIndicatorTrigger trigger;uint8_t collider[56];AeNativeIndicator slot;
                    if(!ae_indicator_world_runtime_record(indicator_world,k,&trigger,collider,&slot)){fail(ae_indicator_world_error());goto error;}
                    fprintf(trace,"%s%d",k?",":"",(int)(int16_t)(uint16_t)(trigger.raw[2]|trigger.raw[3]<<8));
                }
                fprintf(trace,"]");
            }
        }
#ifdef AE_NATIVE_PLAYER
        if(trace&&native_player_mode)fprintf(trace,",\"state\":%u,\"vel\":[%d,%d,%d],\"anim_frame\":%u,\"update_flags\":%u,\"body_flags\":%u,\"volume_flags\":%u",
            player_host.player.state,player_host.player.vel[0],player_host.player.vel[1],player_host.player.vel[2],
            player_host.player.anim_frame,player_host.update_flags,player_host.body_flags,player_host.volume.status);
        if(trace&&native_player_mode&&camera_follow_on())fprintf(trace,",\"camera_eye\":[%d,%d,%d],\"camera_target\":[%d,%d,%d],\"camera_yaw\":%d,\"camera_distance\":%d,\"camera_status\":%u",
            ae_camera_i16(&follow_host.slot,AE_CAM_EYE),ae_camera_i16(&follow_host.slot,AE_CAM_EYE+2),ae_camera_i16(&follow_host.slot,AE_CAM_EYE+4),
            ae_camera_i16(&follow_host.slot,AE_CAM_TARGET),ae_camera_i16(&follow_host.slot,AE_CAM_TARGET+2),ae_camera_i16(&follow_host.slot,AE_CAM_TARGET+4),
            (int)ae_camera_i32(&follow_host.slot,AE_CAM_YAW),(int)ae_camera_i32(&follow_host.slot,AE_CAM_DIST),camera_status);
        if(trace&&native_player_mode&&camera_follow_on())fprintf(trace,",\"camera_body_at_update\":[%d,%d,%d],\"camera_render_at_update\":[%d,%d,%d]",
            camera_read16(follow_host.frame.scratch+0x18),camera_read16(follow_host.frame.scratch+0x1A),camera_read16(follow_host.frame.scratch+0x1C),
            camera_read16(follow_host.frame.scratch+0x10),camera_read16(follow_host.frame.scratch+0x12),camera_read16(follow_host.frame.scratch+0x14));
        if(trace&&native_player_mode&&camera_follow_on())stage_camera_json(trace);
#endif
        if(trace)fprintf(trace,"}");
        if (capture_path && (!frames || frame>=frames)) {
            if (!capture(capture_path)) goto error;
            break;
        }
        if(!SDL_GL_SwapWindow(window_handle)){fail(SDL_GetError());goto error;}
        if (frames && frame>=frames) break;
        SDL_Delay(1);
    }
    result=0; goto done;
usage:
    fprintf(stderr,"Usage: native_viewer.exe area.aev [--check | --capture frame.ppm] [--frames N]\n"
                   "       [--yaw degrees] [--pitch -85..85] [--distance units] [--colliders] [--untextured]\n");
    fprintf(stderr,"       [--actor actor.aea] [--clip index] [--time seconds] [--actor-pos x y z] [--focus-actor]\n");
    fprintf(stderr,"       [--target x y z] [--indicators-pack indicators_AA.aei] (requires --collision-exe)\n");
    fprintf(stderr,"       [--indicator-hint index] manual marker hint preview (requires indicators and UI packs)\n");
#ifdef AE_NATIVE_PLAYER
    fprintf(stderr,"       [--indicator-proximity] event/animation prototype, not the complete gimmick protocol (implies --native-player; requires a runtime AEIND2 --indicators-pack);\n"
                   "       stage exit 0 / message-busy are host proxies, touchedBy unknown; hint/pickup/effect requests are counted, not executed\n");
    fprintf(stderr,"       [--game] integrated game host; [--game-state output.ram] dumps game RAM for validation\n");
    fprintf(stderr,"       [--native-player] [--native-camera] (implies --native-player; rejects --yaw/--pitch/--distance/--target; V toggles follow/inspection)\n");
    fprintf(stderr,"       [--stage-camera] stage camera prototype (implies --native-camera): local regions/scripts/blend only; global camera events,\n"
                   "       indexed handlers except 3 and area05/11, selected aim/fixed, flight/RC/boss dispatch and GTE projection remain incomplete; areas 0x48-0x4C rejected\n");
#else
    fprintf(stderr,"       (--native-player / --native-camera / --stage-camera need a build with AE_NATIVE_PLAYER)\n");
#endif
    fprintf(stderr,"       [--directinput] allow SDL's DirectInput joystick backend (default off: can stall startup; DualShock 4 uses HIDAPI, Xbox pads XInput)\n");
    fprintf(stderr,"       [--keyboard-only] explicit controller-free launch: SDL gamepad subsystem is not initialised and no gamepads are enumerated or opened (default unchanged)\n");
    fprintf(stderr,"       [--music-bank original_bank.bin] (requires --collision-exe)\n");
    fprintf(stderr,"       [--ground original_member.bin] [--collision-exe SCUS_944.23] [--entry 0..15] [--player] [--walk-test trace.json --frames N]\n");
    graphics_free();
    return 2;
error:
    fprintf(stderr,"ERROR: %s\n",error_text);
done:
    if(ui_pack)printf("UI RESULT state=%d hint=%d\n",ae_ui_state(),initial_hint);
#ifdef AE_NATIVE_PLAYER
    if(game_session.ram){if(game_state_path&&!viewer_game_dump(&game_session,game_state_path))result=1;viewer_game_report(&game_session);viewer_game_stop(&game_session);}
#endif
    if(music_bank)printf("AUDIO stereo_float rate=44100 frames=%d\n",ae_audio_frames());
    if (trace) {
        fprintf(trace,"\n],\"accepted\":%u,\"blocked\":%u,\"contacts\":%u}\n",movement_accepted,movement_blocked,movement_contacts);
#ifdef AE_NATIVE_PLAYER
        if(native_player_mode)fprintf(stderr,"PLAYER RESULT frames=%u state=%u update_flags=0x%X body_flags=0x%X pos=%d,%d,%d\n",player_host.world.frame_counter,player_host.player.state,player_host.update_flags,player_host.body_flags,player_host.player.pos[0],player_host.player.pos[1],player_host.player.pos[2]);
#endif
        if (fclose(trace) && !result) { fprintf(stderr,"ERROR: walking trace write failed\n"); result=1; }
    }
    graphics_free(); return result;
}
