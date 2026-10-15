/* Native application boundary for the integrated, readable game ports. */
#define AeGadgetHooks ViewerLegacyGadgetHooks
#include "viewer_game.h"
#undef AeGadgetHooks
#undef AE_G_PLAYER_CURRENT_GADGET
#undef AE_G_STAGE_EXIT_CODE
#include "../game/ae_game.h"
#include "../core/ae_pad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb"); if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f); if(n<=0||n>(64L<<20)){fclose(f);return NULL;}
    rewind(f);uint8_t *b=malloc((size_t)n);
    if(!b||fread(b,1,(size_t)n,f)!=(size_t)n){free(b);fclose(f);return NULL;}
    fclose(f);*size=(size_t)n;return b;
}
static uint8_t *read_member(uint32_t id,size_t *size) {
    char path[128];snprintf(path,sizeof(path),"extract/members/%03X.bin",id);
    return read_file(path,size);
}
static int area_member(void *u,int32_t area,const uint8_t **b,size_t *n) {
    ViewerGame *v=u;(void)area;*b=v->member;*n=v->member_size;return *b!=NULL;
}
static int area_overlay(void *u,int32_t area,uint32_t *id,uint32_t *address,const uint8_t **b,size_t *n) {
    ViewerGame *v=u;(void)area;*id=v->overlay_id;*address=v->overlay_address;
    *b=v->overlay;*n=v->overlay_size;return v->overlay_id?(v->overlay?1:-1):0;
}
static int32_t missing(void *u,AeRam *r,const char *name,uint32_t address,uint32_t a,uint32_t b,uint32_t c,uint32_t d) {
    ViewerGame *v=u;(void)r;(void)a;(void)b;(void)c;(void)d;
    /* An installed missing callback owns its list and reports each pair once. */
    for(size_t i=0;i<v->game->missing_address_count;++i)
        if(v->game->missing_addresses[i]==address&&!strcmp(v->game->missing_names[i],name))return 0;
    if(v->game->missing_address_count<1024){size_t i=v->game->missing_address_count++;
        v->game->missing_addresses[i]=address;v->game->missing_names[i]=name;}
    printf("GAME missing %s @%08X\n",name,address);return 0;
}
static int32_t vsync(void *u,AeRam *r,uint32_t mode){(void)r;return mode==UINT32_MAX?(int32_t)((ViewerGame *)u)->frame:1;}
static int32_t pad_state(void *u,AeRam *r,uint32_t port){(void)u;(void)r;(void)port;return 6;}
static int32_t pad_mode(void *u,AeRam *r,uint32_t a,uint32_t b,uint32_t c){(void)u;(void)r;(void)a;(void)b;(void)c;return 7;}
#define VIEWER_PLAYER 0x800EC1E0u
#define VIEWER_PLAYER_DATA 0x80103000u
#define VIEWER_PLAYER_SETS 0x800F4DE0u
#define VIEWER_PLAYER_PARAMS 0x800EC010u
#define VIEWER_PLAYER_SLOTS 0x800F51A8u
#define VIEWER_MEMBER_SETS 0x14u
#define VIEWER_POSITION 0x24u
#define VIEWER_VELOCITY 0x30u
#define VIEWER_FACING 0x60u
#define VIEWER_PREV_FACING 0x3Cu
#define VIEWER_UP 0x68u
#define VIEWER_ANIMATION 0x18u
#define VIEWER_FRAME 0x5Cu
#define VIEWER_STATE 0x5Eu
#define VIEWER_FLAGS 0x70u
#define VIEWER_HEALTH 0xE8u
#define VIEWER_GADGET 0xF2u
#define VIEWER_LAST_POSITION 0x130u
#define VIEWER_RADIUS 0x44u
#define VIEWER_HEIGHT 0x46u
#define VIEWER_ANIM_SPEED 0x8Eu
#define VIEWER_NEXT_FRAME 0x88u
#define VIEWER_ANIM_LERP 0x8Au
#define VIEWER_ANIM_BLEND 0x8Cu
#define VIEWER_CONTACT 0x50u
#define VIEWER_FRAME_COUNTER 0x800EC350u

/* Boot owns file loading and the initial spawn. Runtime gameplay remains in
 * AeRam; this never copies native movement results into the game image. */
static int player_boot_data(ViewerGame *v) {
    size_t size=0;uint8_t *bytes=read_member(8,&size);
    if(!bytes)return 0;
    int ok=ae_ram_load(v->ram,VIEWER_PLAYER_DATA,bytes,size);free(bytes);
    if(!ok)return 0;
    AeRam *r=v->ram;const AePlayer *initial=&v->player->player;
    for(uint32_t set=0;set<10;++set)
        for(uint32_t field=0;field<4;++field)
            ae_wr32(r,VIEWER_PLAYER_SETS+set*16u+field*4u,
                VIEWER_PLAYER_DATA+ae_rd16(r,VIEWER_PLAYER_DATA+VIEWER_MEMBER_SETS+set*16u+field*4u)*4u);
    /* Boot supplies the real actor header and bone records (PlayerInit,
     * 80032840..80032854); animation cannot run against a null model. */
    ae_ModelInitAuto(r, VIEWER_PLAYER,
        VIEWER_PLAYER_DATA + ae_rd16(r, VIEWER_PLAYER_DATA + 4u) * 4u);
    const AePlayerParams *params=&v->player->data.params;
    const int32_t values[]={params->gravity,params->jump_impulse,params->jump_hold_boost,
        params->jump_vel2,params->drop_vel,params->squash_frames,params->unk_1c,params->unk_20,
        params->move_speed,params->jump_speed_pct,params->jump_speed_pct2,params->jump_speed_pct3};
    for(uint32_t i=0;i<12;++i)ae_wr32(r,VIEWER_PLAYER_PARAMS+(i+1u)*4u,(uint32_t)values[i]);
    for(uint32_t axis=0;axis<3;++axis){
        ae_wr32(r,VIEWER_PLAYER+VIEWER_POSITION+axis*4u,(uint32_t)initial->pos[axis]);
        ae_wr16(r,VIEWER_PLAYER+VIEWER_LAST_POSITION+axis*2u,(uint16_t)initial->pos[axis]);
        ae_wr16(r,VIEWER_PLAYER+VIEWER_FACING+axis*2u,(uint16_t)initial->facing[axis]);
    }
    ae_wr16(r,VIEWER_PLAYER+VIEWER_UP+2u,4096);
    ae_wr16(r,VIEWER_PLAYER+VIEWER_HEALTH,5);
    ae_wr16(r,VIEWER_PLAYER+VIEWER_RADIUS,(uint16_t)initial->body_radius);
    ae_wr16(r,VIEWER_PLAYER+VIEWER_HEIGHT,256);
    ae_wr16(r,VIEWER_PLAYER+VIEWER_ANIM_SPEED,4096);
    ae_wr8(r,VIEWER_PLAYER+VIEWER_GADGET,1); /* debug --game starts with the net equipped */
    ae_wr32(r,VIEWER_PLAYER+VIEWER_ANIMATION,ae_rd32(r,VIEWER_PLAYER_SETS));
    ae_PlayerSetState(r,&v->game->player_act,0);
    /* --game is a debug area entry. Leave the startup mode selected by the
     * real StageRequest, then apply ordinary mode 0's entry reset. */
    ae_PlayerSetMode(r,&v->game->player_act,0);
    ae_PlayerSetMode(r,&v->game->player_act,0);
    for(uint32_t slot=0;slot<4;++slot)ae_wr8(r,VIEWER_PLAYER_SLOTS+slot,(uint8_t)initial->gadget_slots[slot]);
    /* Audio is native; no PS1 bank header has been loaded into this RAM image. */
    for(uint32_t slot=0;slot<12;++slot)ae_wr16(r,AE_SOUND_BANK_IDS+slot*2u,65535);
    return !ae_ram_failed(r);
}

static void player_presentation(ViewerGame *v) {
    AeRam *r=v->ram;AePlayer *p=&v->player->player;
    for(uint32_t axis=0;axis<3;++axis){
        p->pos[axis]=ae_rds32(r,VIEWER_PLAYER+VIEWER_POSITION+axis*4u);
        p->vel[axis]=ae_rds32(r,VIEWER_PLAYER+VIEWER_VELOCITY+axis*4u);
        p->facing[axis]=(int16_t)ae_rd16(r,VIEWER_PLAYER+VIEWER_FACING+axis*2u);
        p->prev_facing[axis]=(int16_t)ae_rd16(r,VIEWER_PLAYER+VIEWER_PREV_FACING+axis*2u);
    }
    p->state=(uint16_t)ae_rd16(r,VIEWER_PLAYER+VIEWER_STATE);
    p->state_flags=ae_rd32(r,VIEWER_PLAYER+VIEWER_FLAGS);
    p->anim_frame=(uint16_t)ae_rd16(r,VIEWER_PLAYER+VIEWER_FRAME);
    p->next_frame=(int16_t)ae_rd16(r,VIEWER_PLAYER+VIEWER_NEXT_FRAME);
    p->anim_lerp=(uint16_t)ae_rd16(r,VIEWER_PLAYER+VIEWER_ANIM_LERP);
    p->anim_blend=(uint16_t)ae_rd16(r,VIEWER_PLAYER+VIEWER_ANIM_BLEND);
    p->anim_speed=(uint16_t)ae_rd16(r,VIEWER_PLAYER+VIEWER_ANIM_SPEED);
    v->player->world.frame_counter=ae_rd32(r,VIEWER_FRAME_COUNTER);
    p->contact=ae_rd32(r,VIEWER_PLAYER+VIEWER_CONTACT);
    p->current_gadget=(int8_t)ae_rds8(r,VIEWER_PLAYER+VIEWER_GADGET);
    p->health=(int16_t)ae_rd16(r,VIEWER_PLAYER+VIEWER_HEALTH);
}
/* Only the verified yaw part of CameraLookAt is needed by PadUpdate.
 * Native GL consumes the RAM eye/target directly; GPU projection hooks remain
 * presentation boundaries and do not execute original packet emitters. */
static int32_t camera_look_at(void *u,AeRam *r,uint32_t eye_address,uint32_t target_address,uint32_t view,uint32_t yaw,uint32_t roll){
    (void)u;(void)view;(void)roll;int16_t eye[3],target[3];
    for(uint32_t i=0;i<3;++i){eye[i]=(int16_t)ae_rds16(r,eye_address+2*i);target[i]=(int16_t)ae_rds16(r,target_address+2*i);}
    ae_pad_CameraRotMatrix(r,eye,target,yaw);return 0;
}
static int32_t memory_set(void *u,AeRam *r,uint32_t address,uint32_t value,uint32_t n) {
    (void)u;uint8_t *p=ae_ptr(r,address,n);if(p)memset(p,(int)value,n);return (int32_t)address;
}
/* HUD logic runs in the RAM port; image-space drawing awaits the native UI pass. */
static void hud_draw(void *user,AeRam *ram,const AeHudDraw *draw) {
    (void)user;(void)ram;(void)draw;
}
static int32_t presentation(void *u,AeRam *r,uint32_t a,uint32_t b,uint32_t c,uint32_t d){(void)u;(void)r;(void)a;(void)b;(void)c;(void)d;return 0;}
static int32_t display_init(void *u,AeRam *r){return presentation(u,r,0,0,0,0);}
static int32_t display_command(void *u,AeRam *r,uint32_t command){return presentation(u,r,command,0,0,0);}
/* The GL scene consumes the resulting actor snapshot after each frame.
 * Original packet emission remains a named renderer notification. */
static int32_t render_notification(void *u,AeRam *r,const char *name,
                                   uint32_t a,uint32_t b,uint32_t c,uint32_t d) {
    (void)name;return presentation(u,r,a,b,c,d);
}
static int32_t render_words(void *u,AeRam *r,const char *name,uint32_t address,uint32_t count,const uint32_t *args) {
    if(address==AE_OVL18C_EFFECT_DRAW_RENDER_HOOK && count==8) return (int32_t)args[7];
    uint32_t words[4]={0}; (void)address;
    for(uint32_t i=0;i<count && i<4;++i) words[i]=args[i];
    return render_notification(u,r,name,words[0],words[1],words[2],words[3]);
}
static int32_t fill_colors(void *u,AeRam *r,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e) {
    (void)e;return presentation(u,r,a,b,c,d);
}
/* Hardware voices are allocated by a separate sequencer in the native engine.
 * Its handles cannot be equated with RAM sequencer channels. These boundaries
 * preserve RAM and are explicitly logged until that mapping is implemented. */
static int32_t sound_idle(void *u,AeRam *r){ViewerGame *v=u;(void)r;return v->audio_sync?v->audio_sync():0;}
static void sound_delay(void *u,AeRam *r,uint32_t n){(void)u;(void)r;(void)n;}
static void sound_adsr(void *u,AeRam *r,uint32_t n,uint16_t a,uint16_t b){(void)u;(void)r;(void)n;(void)a;(void)b;}
static void sound_off(void *u,AeRam *r,uint32_t n,uint16_t a){(void)u;(void)r;(void)n;(void)a;}
static void sound_volume(void *u,AeRam *r,uint32_t n,uint32_t side,uint16_t a){(void)u;(void)r;(void)n;(void)side;(void)a;}
static int32_t sound_pan_volume(void *u,AeRam *r,uint32_t voice,uint32_t pan,uint32_t volume){(void)u;(void)r;(void)voice;(void)pan;(void)volume;return 0;}
static int32_t sound_bank_volume(void *u,AeRam *r,uint32_t bank,uint32_t volume){(void)u;(void)r;(void)bank;(void)volume;return 0;}
static void sound_error(void *u,AeRam *r,uint32_t fmt,uint32_t argc,int32_t a,int32_t b,int32_t c){(void)u;(void)r;(void)argc;(void)a;(void)b;(void)c;static int reported;if(!reported++){printf("GAME sound diagnostic @%08X (further diagnostics suppressed)\n",fmt);}}
int viewer_game_start(ViewerGame *v,AePlayerHost *player,const char *exe,int area,int entry) {
    memset(v,0,sizeof(*v));v->player=player;v->ram=ae_ram_create();v->game=calloc(1,sizeof(*v->game));if(!v->ram||!v->game)return 0;
    size_t n=0;uint8_t *bytes=read_file(exe,&n);
    if(!bytes){ae_ram_fail(v->ram,"viewer game: cannot read EXE %s",exe);return 0;}
    if(!ae_ram_load_exe(v->ram,bytes,n)){free(bytes);return 0;}free(bytes);
    AeRam *r=v->ram;
    /* Boot-owned persistent heap cursor, as opposed to area-owned allocation. */
    ae_wr32(r,0x800E38E4u,0x80100000u);
    uint32_t chain=ae_rd32(r,0x800A81B0u+(uint32_t)area*8);
    for(unsigned i=0;i<32&&chain;++i,chain+=8) {
        uint32_t id=ae_rd16(r,chain),dest=ae_rd32(r,chain+4);
        if(ae_rds16(r,chain+2)<0)break;
        if(dest==AE_AREA_BASE){v->member=read_member(id,&v->member_size);}
        if(id>=0x180&&dest==0x8013D000u){v->overlay_id=id;v->overlay_address=dest;v->overlay=read_member(id,&v->overlay_size);}
    }
    AeGameHost h={0};h.user=v;h.area_member=area_member;h.area_overlay=area_overlay;
    h.missing=missing;h.platform.VSync=vsync;h.platform.PadGetState=pad_state;h.platform.PadInfoMode=pad_mode;
    h.renderer.CameraLookAt=camera_look_at;h.platform.memset=memory_set;
    h.platform.SfxVoiceSetPanVol=sound_pan_volume;h.platform.SoundBankSetVolume=sound_bank_volume;
    h.render=render_notification;h.render_words=render_words;h.renderer.ModelFillVertexColors=fill_colors;
    h.hud_draw=hud_draw;
    h.frame.DrawTerrain=presentation;h.frame.DrawAreaProps=presentation;
    h.frame.BeginFrame=presentation;h.frame.QueueFrameFlip=presentation;
    h.frame.WaitFrameFlip=presentation;
    h.platform.InitDisplay=display_init;h.platform.DrawSync=display_command;
    h.platform.ClearImage=presentation;
    h.platform.PutDispEnv=display_command;h.platform.PutDrawEnv=display_command;
    h.platform.DrawOTag=display_command;
    h.sound_hw=(AeSoundHw){v,sound_idle,sound_delay,sound_adsr,sound_off,sound_error,sound_volume};
    puts("GAME sound: wait_idle native synchronization; SoundStop ported; native voice mapping pending; CPU wait_loop no-op; ADSR/key-off/volume/pan/bank-volume logged no-ops (native voice mapping pending); BGM remains native bank playback");
    ae_game_init(v->game,r,&h);
    ae_game_load_area(v->game,area,entry);
    if(!player_boot_data(v))return 0;
    player_presentation(v);
    ae_wr32(r,AE_PAD_DUALSHOCK_STATE,3);
    return !ae_ram_failed(r);
}
int viewer_game_step(ViewerGame *v,const AePlayerHostPad *p) {
    v->input=*p;AeRam *r=v->ram;
    ae_wr8(r,AE_PAD_BUF1,0);ae_wr8(r,AE_PAD_BUF1+1,0x73);
    ae_wr8(r,AE_PAD_BUF1+2,(uint8_t)~(p->held>>8));ae_wr8(r,AE_PAD_BUF1+3,(uint8_t)~p->held);
    ae_wr8(r,AE_PAD_BUF1+4,p->rx);ae_wr8(r,AE_PAD_BUF1+5,p->ry);
    ae_wr8(r,AE_PAD_BUF1+6,p->lx);ae_wr8(r,AE_PAD_BUF1+7,p->ly);
    ae_wr8(r,AE_PAD_BUF2,0xFF);ae_wr8(r,AE_PAD_BUF2+1,0);
    ++v->frame;int ok=ae_game_frame(v->game);ae_pad_PadSampleStickHistory(r);player_presentation(v);return ok;
}
void viewer_game_report(ViewerGame *v) {
    printf("GAME frames=%u ram_error=%d missing=%u\n",v->frame,ae_ram_failed(v->ram),(unsigned)v->game->missing_address_count);
    for(size_t i=0;i<v->game->missing_address_count;++i)
        printf("GAME missing %s @%08X\n",v->game->missing_names[i],v->game->missing_addresses[i]);
}
void viewer_game_stop(ViewerGame *v){free(v->member);free(v->overlay);ae_ram_destroy(v->ram);free(v->game);memset(v,0,sizeof(*v));}

void viewer_game_script_input(uint32_t frame,AePlayerHostPad *p){
    int segment=(int)(frame/30)%4,hi=frame<30?248:255,lo=frame<30?8:0;
    *p=(AePlayerHostPad){(frame>=60&&frame<66)?0x0A:0,128,128,128,128};
    p->lx=(uint8_t)(segment==1?hi:(segment==3&&frame%30>=12&&frame%30<26)?lo:128);
    p->ly=(uint8_t)((segment==0||(segment==3&&frame%30>=26))?hi:(segment==2||(segment==3&&frame%30<12))?lo:128);
    if(frame>=90&&frame<100)p->rx=255; /* net swing */
}
int viewer_game_dump(ViewerGame *v,const char *path){
    FILE *f=fopen(path,"wb");if(!f)return 0;
    int ok=fwrite(v->ram->main,1,AE_RAM_SIZE,f)==AE_RAM_SIZE&&fwrite(v->ram->scratch,1,AE_SCRATCH_SIZE,f)==AE_SCRATCH_SIZE;
    return fclose(f)==0&&ok;
}

unsigned viewer_game_missing_count(ViewerGame *v){return (unsigned)v->game->missing_address_count;}
static void marker(AeRam *r,ViewerGameMarker draw,void *u,const char *kind,uint32_t address,int words){
    int32_t pos[3];for(uint32_t i=0;i<3;++i)pos[i]=words?ae_rds32(r,address+4*i):ae_rds16(r,address+2*i);
    draw(u,kind,pos);
}
void viewer_game_markers(ViewerGame *v,ViewerGameMarker draw,void *u){
    AeRam *r=v->ram;
    for(uint32_t i=0;i<16;++i){uint32_t obj=AE_G_ZAKO_OBJECTS+i*AE_ZAKO_OBJECT_SIZE;
        if(ae_rd32(r,obj+AE_OBJ_FLAGS)&AE_OBJ_FLAG_SPAWNED)marker(r,draw,u,"enemy",obj+AE_OBJ_POS_X,1);}
    for(uint32_t i=0;i<32;++i){uint32_t item=0x800BEFA0u+i*24;
        if(ae_rd8(r,item+10))marker(r,draw,u,"item",item+4,0);}
    for(uint32_t i=0;i<16;++i){uint32_t projectile=AE_PROJECTILES+i*AE_PROJECTILE_SIZE;
        if(ae_rd8(r,projectile+AE_J_KIND))marker(r,draw,u,"projectile",projectile+AE_J_POS,0);}
    for(uint32_t i=0;i<AE_PARTICLE_COUNT;++i){uint32_t particle=AE_PARTICLES+i*AE_PARTICLE_SIZE;
        if(ae_rd8(r,particle+AE_P_KIND))marker(r,draw,u,"particle",particle+AE_P_POS,0);}
    for(uint32_t i=0;i<16;++i){uint32_t effect=AE_EFFECTS+i*AE_EFFECT_SIZE;
        if(ae_rd8(r,effect+AE_E_HEADER))marker(r,draw,u,"effect",effect+AE_E_POS,0);}
}
