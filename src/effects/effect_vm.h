/* Original portable implementation of the documented particle bytecode.
 * State is the PSX little-endian 52-byte image. Host pointers stay outside it.
 * No game bytes, decompiled code, renderer, allocator or audio dependencies.
 */
#ifndef AE_EFFECT_VM_H
#define AE_EFFECT_VM_H
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#define AE_VM_API __declspec(dllexport)
#else
#define AE_VM_API
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*AeParticleEvent)(void *user,uint32_t kind,int32_t operand,const uint8_t state[52]);
enum { AE_VM_EVENT_GROUND=1, AE_VM_EVENT_WATER=2, AE_VM_EVENT_RIBBON=3 };

typedef struct AeParticleVM {
    const uint8_t *blob;
    uint32_t blob_size, start, end, origin, pc;
    uint8_t state[52];
    uint16_t seed;
    int32_t ground, water;
    uint32_t frames, commands, rng_calls;
    uint32_t ground_events, water_events, ribbon_events, termination;
    AeParticleEvent event;
    void *event_user;
} AeParticleVM;

enum { AE_VM_END=1, AE_VM_GROUND=2, AE_VM_WATER=3 };
enum { AE_VM_BAD_PC=-1, AE_VM_BAD_OPCODE=-2, AE_VM_TRUNCATED=-3,
       AE_VM_BAD_LOOP=-4, AE_VM_BUDGET=-5, AE_VM_BAD_INPUT=-6 };
/* The borrowed blob must remain alive throughout the VM lifetime.
 * ground/water use game units. External callbacks are counted for the caller.
 * Returns 0 on success; invalid ranges return AE_VM_BAD_INPUT.
 */
AE_VM_API int ae_particle_init(AeParticleVM *,const uint8_t *,uint32_t,
    uint32_t start,uint32_t end,uint32_t origin,uint16_t seed,uint8_t kind,
    int32_t ground,int32_t water);
/* One script update, excluding the game's preceding ParticlesDraw pass.
 * Returns 1 while active, 0 when terminated, negative error otherwise.
 * A caller can detect per-frame event count changes and dispatch callbacks.
 * Errors are diagnostic stop conditions; discard/reset the context after one.
 */
AE_VM_API int ae_particle_step(AeParticleVM *);
/* Optional observer/dispatch hook, invoked before a collision kill or ribbon draw.
 * Ribbon operand is the table index; collision operand is zero. The state view
 * is borrowed for the callback only. Do not mutate this context or re-enter it.
 * Initialization clears the hook. Event counters work without a hook.
 */
AE_VM_API void ae_particle_set_event_hook(AeParticleVM *,AeParticleEvent,void *user);
/* FFI helpers: opaque allocation size, state bytes, and nine u32 statistics:
 * frames, commands, RNG calls, seed, pc, reason, ground/water/ribbon events.
 */
AE_VM_API size_t ae_particle_vm_size(void);
AE_VM_API void ae_particle_snapshot(const AeParticleVM *,uint8_t out[52]);
AE_VM_API void ae_particle_stats(const AeParticleVM *,uint32_t out[9]);
#ifdef __cplusplus
}
#endif
#endif
