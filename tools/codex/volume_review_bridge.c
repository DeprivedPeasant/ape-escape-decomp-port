/* Offline original-instruction comparison entry points; no emulator in runtime. */
#include "../../src/app/native_player_host.h"
#include <stdlib.h>
#ifdef _WIN32
#define API __declspec(dllexport)
#else
#define API
#endif
API AePlayerHost *volume_review_load(const char *member,const char *exe,const char *player_member) {
    AePlayerHost *h=calloc(1,sizeof(*h));int32_t pos[3]={0};int16_t dir[3]={0,0,4096};
    if(!h)return NULL;
    AeGround *g=ae_ground_load(member);AeCollision *c=ae_collision_load(member);
    if(!g||!c||!ae_collision_math_load(exe)||!ae_player_host_init(h,g,c,player_member,exe,1,pos,dir)) {
        ae_player_host_free(h);ae_ground_free(g);ae_collision_free(c);free(h);return NULL;
    }
    return h;
}
API void volume_review_free(AePlayerHost *h){ae_player_host_free(h);ae_ground_free(h->ground);ae_collision_free(h->collision);free(h);}
API int32_t volume_review_scan(AePlayerHost *h,const int32_t pos[3],uint32_t *status){h->volume.status=0;int32_t r=ae_volume_collider_at(&h->volume,pos);*status=h->volume.status;return r;}
API int32_t volume_review_ray(AePlayerHost *h,const int32_t pos[3],const int32_t vel[3],const int32_t probe[5],uint32_t *status){
    h->volume.status=0;ae_volume_build_probes(&h->volume,pos,vel[1]);
    int32_t r=h->world.ray_test(&h->volume,0,probe);*status=h->volume.status;return r;
}
