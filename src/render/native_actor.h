#ifndef AE_NATIVE_ACTOR_H
#define AE_NATIVE_ACTOR_H
#include <stdint.h>
typedef struct AeActor AeActor;
AeActor *ae_actor_load(const char *path);
const char *ae_actor_error(void);
void ae_actor_free(AeActor *actor);
void ae_actor_info(const AeActor *actor, uint32_t out[4]); /* nodes, joints, vertices, clips */
uint32_t ae_actor_frames(const AeActor *actor, uint32_t clip);
float ae_actor_fps(const AeActor *actor, uint32_t clip);
const char *ae_actor_clip_name(const AeActor *actor, uint32_t clip);
int ae_actor_pose(AeActor *actor, uint32_t clip, uint32_t frame);
int ae_actor_positions(const AeActor *actor, float *out, uint32_t capacity);
int ae_actor_upload(AeActor *actor); /* requires current OpenGL context */
void ae_actor_draw(AeActor *actor, int textured, const float position[3], float yaw);
#endif
