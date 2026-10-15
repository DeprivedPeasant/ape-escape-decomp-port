/* SDL3 output adapter for Claude's separately owned, portable sound engine. */
#include "native_audio_output.h"
#include "../audio/sp_player.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static SDL_AudioStream *stream;
static SpPlayer *music;
static SpSfx *effects;
static SpMix *mixer;
static unsigned char *common_shd,*common_vb;
static int stage_effect=-1;
static unsigned char *bank_bytes,*exe_bytes;
static SDL_AtomicInt rendered;
static char message[256];
static unsigned read32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
const char *ae_audio_error(void){return message;}
int ae_audio_frames(void){return SDL_GetAtomicInt(&rendered);}
static unsigned char *read_asset(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");long n;
    if(!f)return NULL;
    if(fseek(f,0,SEEK_END)||(n=ftell(f))<16||n>33554432){fclose(f);return NULL;}
    rewind(f);unsigned char *b=malloc((size_t)n);
    if(!b){fclose(f);return NULL;}
    if(fread(b,1,(size_t)n,f)!=(size_t)n){free(b);fclose(f);return NULL;}
    fclose(f);*size=(size_t)n;return b;
}
static void SDLCALL provide(void *unused,SDL_AudioStream *out,int additional,int total){
    (void)unused;(void)total;
    float samples[1024];int frames=(additional+7)/8;
    while(frames>0){
        int n=frames>512?512:frames;sp_mix_render(mixer,samples,n);
        for(int i=0;i<n*2;++i)samples[i]=fmaxf(-1,fminf(1,samples[i]));
        if(!SDL_PutAudioStreamData(out,samples,n*8))break;
        SDL_AddAtomicInt(&rendered,n);frames-=n;
    }
}
int ae_audio_sync(void){
    if(!stream)return 0;
    if(!SDL_LockAudioStream(stream))return 1;
    return SDL_UnlockAudioStream(stream)?0:1;
}
void ae_audio_stop(void){
    if(stream){SDL_DestroyAudioStream(stream);stream=NULL;}
    sp_mix_destroy(mixer);mixer=NULL;
    if(music){sp_destroy(music);music=NULL;}
    sp_sfx_destroy(effects);effects=NULL;
    free(common_shd);free(common_vb);common_shd=common_vb=NULL;stage_effect=-1;
    free(bank_bytes);free(exe_bytes);bank_bytes=exe_bytes=NULL;
}
int ae_audio_start(const char *bank_path,const char *exe_path){
    size_t bank_size=0,exe_size=0;ae_audio_stop();SDL_SetAtomicInt(&rendered,0);
    bank_bytes=read_asset(bank_path,&bank_size);exe_bytes=read_asset(exe_path,&exe_size);
    if(!bank_bytes||!exe_bytes){snprintf(message,sizeof(message),"cannot read music bank or original EXE");goto fail;}
    music=sp_create(bank_bytes,bank_size,exe_bytes,exe_size,-1,SP_FRAME_TIMING|SP_COUNTER_ADSR|SP_INTEGER_REVERB,4,10);
    if(!music){snprintf(message,sizeof(message),"invalid music bank");goto fail;}
    effects=sp_sfx_create(exe_bytes,exe_size);
    if(effects)sp_sfx_frame_timing(effects,1);
    if(effects)sp_sfx_counter_adsr(effects,1);
    size_t shd_size=0,vb_size=0;
    common_shd=read_asset("extract/members/00B.bin",&shd_size);common_vb=read_asset("extract/members/00A.bin",&vb_size);
    if(!effects||!common_shd||!common_vb||sp_sfx_bank(effects,0,common_shd,shd_size,common_vb,vb_size)){
        snprintf(message,sizeof(message),"cannot load common effect bank");goto fail;
    }
    unsigned so=read32(bank_bytes+24),ss=read32(bank_bytes+4),vo=read32(bank_bytes+36),vs=read32(bank_bytes+16);
    if(ss&&so<=bank_size&&ss<=bank_size-so&&vo<=bank_size&&vs<=bank_size-vo&&sp_sfx_bank(effects,2,bank_bytes+so,ss,bank_bytes+vo,vs)==0){
        /* Choose a valid stage-bank audition id; gameplay trigger wiring remains separate. */
        size_t table=0x800B2D08u-0x80010000u+0x800;
        for(int i=0;i<198&&table+(size_t)i*8+8<=exe_size;i++){
            const unsigned char *r=exe_bytes+table+i*8;
            if(r[0]==2&&r[1]==0&&sp_sfx_play_id(effects,i)>=0){stage_effect=i;break;}
        }
        /* Discard audition discovery voices before playback starts. */
        sp_sfx_destroy(effects);effects=sp_sfx_create(exe_bytes,exe_size);
        if(!effects){snprintf(message,sizeof(message),"effect mixer allocation failed");goto fail;}
        sp_sfx_frame_timing(effects,1);
        sp_sfx_counter_adsr(effects,1);
        sp_sfx_bank(effects,0,common_shd,shd_size,common_vb,vb_size);
        sp_sfx_bank(effects,2,bank_bytes+so,ss,bank_bytes+vo,vs);
    }
    mixer=sp_mix_create(music,effects);
    if(!mixer){snprintf(message,sizeof(message),"shared voice mixer creation failed");goto fail;}
    if(!SDL_InitSubSystem(SDL_INIT_AUDIO))goto sdl_fail;
    SDL_AudioSpec spec={SDL_AUDIO_F32,2,SP_RATE};
    stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,provide,NULL);
    if(!stream||!SDL_SetAudioStreamGain(stream,.5f)||!SDL_ResumeAudioStreamDevice(stream))goto sdl_fail;
    return 1;
sdl_fail:snprintf(message,sizeof(message),"%s",SDL_GetError());
fail:ae_audio_stop();return 0;
}
int ae_audio_stage_effect(void){return stage_effect;}
int ae_audio_play_effect(int id){
    if(!stream||!effects)return -1;
    SDL_LockAudioStream(stream);int handle=sp_sfx_play_id(effects,id);SDL_UnlockAudioStream(stream);
    return handle;
}
