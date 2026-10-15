/* One-frame PlayerUpdate comparison; world collision is a separate phase. */
#include "native_player.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define API __declspec(dllexport)
#else
#define API
#endif
typedef struct { const char *name; size_t native; int ram, width, count, sign; } Field;
#define F(n,r,w,c,s) {#n,offsetof(AePlayer,n),r,w,c,s}
static const Field fields[]={
 F(pos,0x24,4,3,1),F(vel,0x30,4,3,1),F(prev_facing,0x3c,2,4,1),
 F(body_radius,0x44,2,1,1),F(body_height,0x46,2,1,1),F(contact,0x50,4,1,0),
 F(anim_frame,0x5c,2,1,0),F(state,0x5e,2,1,0),F(facing,0x60,2,4,1),
 F(state_flags,0x70,4,1,0),F(desired_dir,0x74,2,4,1),F(saved_facing,0x80,2,4,1),
 F(next_frame,0x88,2,1,1),F(anim_lerp,0x8a,2,1,0),F(anim_blend,0x8c,2,1,0),
 F(anim_speed,0x8e,2,1,0),F(attach_dir,0xd8,2,3,1),F(attach_timer,0xde,2,1,0),
 F(attach_flags,0xe0,4,1,0),F(prev_attach_flags,0xe4,4,1,0),F(health,0xe8,2,1,1),
 F(invuln_timer,0xea,2,1,1),F(surface_kind,0xec,2,1,1),F(unk_ee,0xee,1,1,0),
 F(contact_frames,0xef,1,1,0),F(hurt,0xf0,1,1,0),F(contact_cooldown,0xf1,1,1,0),
 F(current_gadget,0xf2,1,1,1),F(fall_frames,0xfc,2,1,1),F(squash_timer,0xfe,2,1,1),
 F(slope_x,0x104,2,1,1),F(slope_z,0x106,2,1,1),F(vel_frac,0x108,4,3,1),
 F(target_move_vel,0x114,4,3,1),F(move_vel,0x120,4,3,1),F(ground_y,0x12c,4,1,1),
 F(has_ground,0x136,2,1,1),F(ground_type,0x138,2,1,0),F(prev_ground_type,0x13a,2,1,0)
};
static AePlayerData data;
static uint8_t *read_file(const char *path,size_t *size) {
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);if(n<0){fclose(f);return NULL;}
 uint8_t *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
 if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}
 fclose(f);*size=(size_t)n;return p;
}
API int review_load(const char *exe,const char *member) {
 size_t n;uint8_t *p;ae_player_data_init(&data);
 p=read_file(exe,&n);if(!p)return 0;
 int ok=ae_player_data_load_exe(&data,p,n);free(p);if(!ok)return 0;
 p=read_file(member,&n);if(!p)return 0;
 ok=ae_player_data_load_member(&data,p,n);free(p);return ok;
}
API int review_count(void){return (int)(sizeof(fields)/sizeof(fields[0]));}
API const char *review_name(int i){return fields[i].name;}
API int review_meta(int i,int which){const Field *f=&fields[i];return which==0?f->ram:which==1?f->width:which==2?f->count:f->sign;}
static int64_t value(const uint8_t *p,int width,int sign) {
 uint32_t v=0;for(int i=0;i<width;i++)v|=(uint32_t)p[i]<<(8*i);
 if(sign&&width==1)return (int8_t)v;
 if(sign&&width==2)return (int16_t)v;
 if(sign&&width==4)return (int32_t)v;
 return v;
}
API uint32_t review_step(const uint8_t *ram,const int32_t *input,const int32_t *ground,int64_t *out) {
 AePlayer p;int32_t pos[3]={0,0,0};int16_t dir[4]={0,0,0x1000,1};
 ae_player_init(&p,pos,dir);
 for(int i=0;i<review_count();i++) {
  const Field *f=&fields[i];memcpy((uint8_t *)&p+f->native,ram+f->ram,(size_t)(f->width*f->count));
 }
 /* RAM fields not represented in the original player object. */
 p.damage_enabled=input[13];p.hurt_blink=input[14];p.dash_boost=(int16_t)input[15];
 AePlayerInput in;memset(&in,0,sizeof(in));
 in.held=(uint32_t)input[0];in.changed=(uint32_t)input[1];in.stick_class=(uint16_t)input[2];in.stick_mag=(uint16_t)input[3];
 for(int i=0;i<4;i++)in.cam_dir[i]=(int16_t)input[4+i];
 in.stick_cam[0]=(int16_t)input[8];in.stick_cam[1]=(int16_t)input[9];
 in.right_stick_gesture=(uint16_t)input[10];in.locked=input[11];in.frame_counter=(uint32_t)input[12];
 AePlayerGround g={ground[0],(int16_t)ground[1],(int16_t)ground[2],(uint16_t)ground[3],(uint16_t)ground[4],(int16_t)ground[5]};
 uint32_t unsupported=ae_player_update(&p,&data,&in,&g);
 int n=0;for(int i=0;i<review_count();i++) {
  const Field *f=&fields[i];for(int k=0;k<f->count;k++)out[n++]=value((const uint8_t *)&p+f->native+k*f->width,f->width,f->sign);
 }
 return unsupported;
}
