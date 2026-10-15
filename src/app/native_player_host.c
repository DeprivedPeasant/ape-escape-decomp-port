/* Host adapter only. Cloud-owned movement/body sources are linked unchanged.
 * Volume queries delegate to instruction-checked native shape/probe handlers.
 * Unsupported queries remain explicitly flagged and blocked.
 */
#include "native_player_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");long n;unsigned char *p;
    if(!f)return NULL;
    if(fseek(f,0,SEEK_END)||(n=ftell(f))<1||n>67108864){fclose(f);return NULL;}
    rewind(f);p=malloc((size_t)n);
    if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}
    fclose(f);*size=(size_t)n;return p;
}
static int32_t ground_cb(void *user,int32_t x,int32_t y,int32_t z,AePlayerBodyGroundHit *out) {
    AePlayerHost *h=user;AeGroundHit g;AeSolidHit s;
    memset(&g,0,sizeof(g));memset(&s,0,sizeof(s));
    /* The original height sentinel and scratch words remain meaningful on no floor. */
    ae_collision_ground_height(h->collision,h->ground,x,y,z,0,-1,0,&g,&s);
    out->slope_x=(int16_t)g.slope_x;out->slope_z=(int16_t)g.slope_z;
    out->surface_flags=(uint16_t)g.surface;out->solid_kind=(int16_t)s.kind;
    out->solid_shape=(uint16_t)s.type;out->solid_type=(uint16_t)s.surface;
    return (int16_t)g.height;
}
static int walls_cb(void *user,const int32_t pos[3],const int32_t vel[3],int32_t radius,int32_t height,AePlayerBodyWallHit *out) {
    AePlayerHost *h=user;AeMoveHit m;
    if(!ae_collision_resolve(h->collision,pos,vel,radius,height,&m)){h->failed=1;return 0;}
    out->push_x=m.push_x;out->push_z=m.push_z;out->vel_y_add=m.velocity[1]-vel[1];
    out->ceiling=m.ceiling;out->contact=(uint32_t)m.contact;return 1;
}
static int32_t volume_ground(void *user,int32_t x,int32_t y,int32_t z,AeVolumeGroundHit *out) {
    AePlayerHost *h=user;AeGroundHit g;AeSolidHit s;
    memset(&g,0,sizeof(g));memset(&s,0,sizeof(s));
    ae_collision_ground_height(h->collision,h->ground,x,y,z,h->volume.wall_probe,-1,0,&g,&s);
    out->hit.slope_x=(int16_t)g.slope_x;out->hit.slope_z=(int16_t)g.slope_z;
    out->hit.surface_flags=(uint16_t)g.surface;out->hit.solid_kind=(int16_t)s.kind;
    out->hit.solid_shape=(uint16_t)s.type;out->hit.solid_type=(uint16_t)s.surface;
    out->word_3fa=(int16_t)s.extra_b;out->word_3fa_known=1;
    return (int16_t)g.height;
}
static int volume_shape(void *user,int16_t shape,const int16_t half[3],const int32_t local[3],int32_t io[2]) {
    AePlayerHost *h=user;AeShapeHit s={io[0],io[1],h->volume.contact_slope_x,h->volume.contact_slope_z};
    int r=ae_collision_shape(shape,half,local,h->volume.wall_probe,&s);
    io[0]=s.height;io[1]=s.extra;h->volume.contact_slope_x=s.slope_x;h->volume.contact_slope_z=s.slope_z;return r;
}
static int32_t volume_grid_probe(void *user,const AeVolumeProbe *e,const int16_t shape[10],const int32_t params[3],const int32_t point[3],int32_t out[6]) {
    AePlayerHost *h=user;int16_t b[28]={e->x,e->y,e->z,e->half[0],e->half[1],e->half[2]};
    (void)shape;int r=ae_collision_probe(b,(uint16_t)params[1],0,params[0],params[2],point,h->volume.build_pos,out);
    if(r<0){h->volume.status|=AE_VOLUME_GRID_PROBE;return 0;}return r;
}
static int32_t volume_collider_probe(void *user,const int16_t record[28],const int32_t params[3],const int32_t point[3],int32_t out[6]) {
    AePlayerHost *h=user;int16_t b[28];memcpy(b,record,sizeof(b));
    int r=ae_collision_probe(b,(uint16_t)params[1],1,params[0],params[2],point,h->volume.build_pos,out);
    if(r<0){h->volume.status|=AE_VOLUME_COLLIDER_PROBE;return 0;}return r;
}
static int32_t volume_body_ground(void *user,int32_t x,int32_t y,int32_t z,AePlayerBodyGroundHit *out) {
    /* Ordinary body ground probes run wall-mode 0; ledge-ray fallback uses 1. */
    AeVolume *v=user;return ground_cb(v->user,x,y,z,out);
}
static int32_t volume_ray(void *user,uint32_t mode,const int32_t probe[5]) {
    AeVolume *v=user;uint32_t saved=v->status;v->status=0;
    int32_t r=ae_volume_ray_test(v,mode,probe);uint32_t query_status=v->status;
    v->status|=saved;
    return query_status?0:r; /* rejected host dispatch cannot become a clear ray */
}
static void gadget_select_hook(void *user,AeRam *r,int32_t gadget,int32_t old) {
    AePlayerHost *h=user;(void)r;
    ae_player_select_gadget_from(&h->player,&h->data,gadget,old); /* state part only: sounds / effects are not played */
}
static void gadget_hud_hook(void *user,AeRam *r){(void)user;(void)r;} /* HudFindGadgetIconPos: HUD only */
int ae_player_host_init(AePlayerHost *h,AeGround *g,AeCollision *c,const char *member,const char *exe,uint32_t area,const int32_t pos[3],const int16_t facing[3]) {
    size_t n=0;unsigned char *bytes;uint32_t info[4];
    memset(h,0,sizeof(*h));ae_player_data_init(&h->data);h->ground=g;h->collision=c;
    bytes=read_file(member,&n);if(!bytes)return 0;
    int ok=ae_player_data_load_member(&h->data,bytes,n);free(bytes);if(!ok)return 0;
    bytes=read_file(exe,&n);if(!bytes)return 0;
    ok=ae_player_data_load_exe(&h->data,bytes,n);
    size_t offset=(size_t)(0x800A81B0u-0x80010000u)+0x800+area*8+4;
    if(offset+4>n){free(bytes);return 0;}
    h->world.current_level=(uint16_t)(bytes[offset]|(uint16_t)bytes[offset+1]<<8);
    if(ok)ok=ae_player_pad_init(&h->pad,bytes,n); /* the pad globals live on a RAM image of the EXE (VectorNormal table) */
    free(bytes);if(!ok)return 0;
    ae_player_init(&h->player,pos,facing);ae_player_body_state_init(&h->body);
    {   /* New-game gadget defaults: GameStateReset (g_StageExitCode = 0) -> GadgetSlotsInitForExit, ported and diffed in
           native_gadget_slots.c: slots {club, -, -, net}, unlock mask 3, then PlayerSelectGadget(club, -1). */
        AeRam *r=h->pad.ram;AeGadgetHooks gh={h,gadget_select_hook,gadget_hud_hook};
        ae_wr8(r,AE_G_PLAYER_CURRENT_GADGET,(uint32_t)h->player.current_gadget);ae_wr8(r,AE_G_STAGE_EXIT_CODE,0);
        ae_GadgetSlotsInitForExit(r,&gh);
        if(ae_ram_failed(r))return 0;
        for(uint32_t i=0;i<4;++i)h->player.gadget_slots[i]=(int8_t)ae_rds8(r,AE_G_GADGET_SLOTS+i);
        h->gadget_unlock_mask=ae_rd32(r,AE_G_GADGET_UNLOCK_MASK);
    }
    h->player.damage_enabled=h->world.current_level!=31&&h->world.current_level!=32;
    ae_collision_info(c,info);h->world.grid_width=(int16_t)info[0];h->world.grid_depth=(int16_t)info[1];
    h->world.collider_count=(uint16_t)info[3];h->world.current_area=(uint16_t)area;
    ae_volume_init(&h->volume);h->volume.user=h;
    ae_collision_arrays(c,&h->volume.shape_grid,&h->volume.shapes,&h->volume.colliders);
    h->volume.shape_grid_count=(size_t)info[0]*info[1];h->volume.shape_count=info[2];h->volume.collider_count=(uint16_t)info[3];
    h->volume.coll_half_x=(int32_t)info[0]*256;h->volume.coll_half_z=(int32_t)info[1]*256;h->volume.coll_grid_w=(int32_t)info[0];
    h->volume.ground=volume_ground;h->volume.walls=walls_cb;h->volume.shape_handler=volume_shape;
    h->volume.grid_probe=volume_grid_probe;h->volume.collider_probe=volume_collider_probe;
    h->world.user=&h->volume;h->world.data=&h->data;h->world.state=&h->body;
    h->world.ground=volume_body_ground;h->world.walls=ae_volume_body_walls;
    h->world.ray_test=volume_ray;h->world.collider_at=ae_volume_collider_at;
    return 1;
}
void ae_player_host_free(AePlayerHost *h){ae_player_pad_free(&h->pad);ae_player_data_free(&h->data);}
void ae_player_host_set_camera(AePlayerHost *h,const int16_t eye[3],const int16_t target[3]){ae_player_pad_set_camera(&h->pad,eye,target);}
static int step_decoded(AePlayerHost *h,AePlayerInput in,AePlayerHostWorldTick tick,void *user) {
    AePlayerGround g;AePlayerBodyGroundHit hit;memset(&g,0,sizeof(g));
    in.player_mode=h->world.player_mode;in.frame_counter=h->world.frame_counter;
    g.ground_y=ground_cb(h,h->player.pos[0],h->player.pos[1],h->player.pos[2],&hit);
    g.slope_x=hit.slope_x;g.slope_z=hit.slope_z;g.surface_flags=hit.surface_flags;
    g.ground_type=ae_collision_surface_type(h->collision,h->player.pos,&g.water_y);
    h->update_flags|=ae_player_update(&h->player,&h->data,&in,&g);
    /* Original order: PlayerUpdate, then indicators, then body/collision integration. */
    if(tick&&!tick(user,h->player.pos))return 0; /* player update already ran; not rolled back */
    h->body_flags|=ae_player_body_step(&h->player,&h->world);
    ++h->world.frame_counter;
    return !h->failed;
}
int ae_player_host_step_game_input(AePlayerHost *h,AeRam *ram) {
    AePlayerInput in;memset(&in,0,sizeof(in));AePlayerPad view=h->pad;view.ram=ram;
    ae_player_pad_read(&view,&in);return step_decoded(h,in,NULL,NULL);
}
int ae_player_host_step_input(AePlayerHost *h,const AePlayerHostPad *pad,AePlayerHostWorldTick tick,void *user) {
    AePlayerInput in;AePlayerPadRaw raw;
    memset(&in,0,sizeof(in));memset(&raw,0,sizeof(raw));
    /* Original order (Mode_Stage): PadUpdate(1) first; everything the player reads comes out of it. */
    raw.held=pad->held;raw.rx=pad->rx;raw.ry=pad->ry;raw.lx=pad->lx;raw.ly=pad->ly;
    if(!ae_player_pad_update(&h->pad,&raw,h->player.facing,&in)){h->failed=1;return 0;}
    if(!step_decoded(h,in,tick,user))return 0;
    ae_player_pad_frame_end(&h->pad);
    return !h->failed;
}
