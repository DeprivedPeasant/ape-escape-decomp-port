#include "ae_spawn.h"
#include "../core/ae_sdk.h"
#include <string.h>

/* Temporary RAM buffers marshal native locals to address-taking SDK/host APIs.
 * They are disjoint from game storage and each other; they do not recreate CPU
 * frames. Hosts must treat them as borrowed for the duration of the call.
 * Group/burst padding comes from an explicit hook, never from these buffers. */
#define LOCAL_GROUP 0x801FF020u
#define LOCAL_BURST 0x801FF030u
#define LOCAL_SPARK 0x801FF040u
#define LOCAL_DIRECTION 0x801FF050u
#define LOCAL_VELOCITY 0x801FF058u
#define LOCAL_MATRIX 0x801FF080u
#define LOCAL_EFFECT_MATRIX 0x801FF0A0u
#define PLAYER 0x800EC1E0u
#define PLAYER_POS 0x24u
#define PLAYER_VEL 0x30u

static int32_t s16(uint32_t n) { return n&0x8000u ? (int32_t)(n&0xFFFFu)-65536 : (int32_t)(n&0xFFFFu); }
static int32_t s32(uint32_t n) { return n<=0x7FFFFFFFu ? (int32_t)n : -1-(int32_t)(~n); }
static int32_t asr(int64_t n, unsigned bits) {
    int64_t d=(int64_t)1<<bits;
    return (int32_t)(n>=0 ? n/d : -((-n+d-1)/d));
}
static uint32_t rand8(AeRam *r) {
    uint32_t seed=(ae_rd16(r,AE_SPAWN_RAND_SEED)*5+1)&0xFFFF;
    ae_wr16(r,AE_SPAWN_RAND_SEED,seed);
    return seed>>8;
}
/* The original unaligned eight-byte copies load both words before storing. */
static void copy_position(AeRam *r, uint32_t dst, uint32_t src) {
    uint8_t bytes[8]; uint8_t *p=ae_ptr(r,src,8);
    if(!p) return;
    memcpy(bytes,p,8); p=ae_ptr(r,dst,8);
    if(p) memcpy(p,bytes,8);
}
static int32_t call_hook(AeRam *r, const AeSpawnHooks *h, AeSpawnHookFn fn, const char *name,
                         uint32_t a, uint32_t b) {
    if(!h || !fn) { ae_ram_fail(r,"AeSpawnHooks.%s not provided",name); return 0; }
    return fn(h->user,r,a,b,0,0);
}
static int position_padding(AeRam *r, const AeSpawnHooks *h, uint32_t local, uint32_t function) {
    if(!h || !h->position_padding) { ae_ram_fail(r,"AeSpawnHooks.position_padding not provided"); return 0; }
    ae_wr16(r,local+6,h->position_padding(h->user,r,function));
    return !ae_ram_failed(r);
}

/* ParticleAlloc @80069548: free first, then replace a priority-zero particle,
 * finally slot zero. The cursor advances before each probe, even on failure. */
uint32_t ae_ParticleAlloc(AeRam *r) {
    for(unsigned pass=0;pass<2;pass++) {
        for(unsigned n=0;n<AE_PARTICLE_COUNT;n++) {
            uint32_t cursor=(ae_rd8(r,AE_PARTICLE_CURSOR)+1)&255;
            if(cursor>=AE_PARTICLE_COUNT) cursor=0;
            ae_wr8(r,AE_PARTICLE_CURSOR,cursor);
            uint32_t particle=AE_PARTICLES+cursor*AE_PARTICLE_SIZE;
            if(!ae_rd8(r,particle+(pass ? AE_P_PRIORITY : AE_P_KIND))) return particle;
        }
    }
    return AE_PARTICLES;
}
/* SpawnOneParticle @8006962C */
void ae_SpawnOneParticle(AeRam *r, int32_t type, uint32_t pos, uint32_t table) {
    uint32_t p=ae_ParticleAlloc(r);
    ae_wr8(r,p+AE_P_KIND,(uint32_t)type);
    copy_position(r,p+AE_P_POS,pos);
    uint32_t index=(uint32_t)type-(type<200 ? 0u : 200u);
    ae_wr32(r,p+AE_P_SCRIPT,ae_rd32(r,table+index*4));
    ae_wr8(r,p+AE_P_LOOP_DEPTH,255);
    ae_wr32(r,p+AE_P_RGB,0x2C000000);
    ae_wr8(r,p+AE_P_TIMER,0);
    ae_wr16(r,p+AE_P_SIZE,0);
    ae_wr8(r,p+AE_P_FADE,0); ae_wr8(r,p+AE_P_MODE,0);
    ae_wr8(r,p+AE_P_VEL_X,0); ae_wr8(r,p+AE_P_PRIORITY,0);
    ae_wr16(r,p+AE_P_TPAGE,124);
}
/* TrailSpawn @8006ED78 */
void ae_TrailSpawn(AeRam *r, uint32_t pos, int32_t color, int32_t width, int32_t mode) {
    uint32_t cursor=ae_rd8(r,AE_TRAIL_CURSOR), t=AE_TRAILS+cursor*AE_TRAIL_SIZE;
    ae_wr8(r,t+AE_T_LIFE,15); ae_wr8(r,t+AE_T_MODE,(uint32_t)mode);
    ae_wr16(r,t+AE_T_COLOR,(uint32_t)color); copy_position(r,t+AE_T_POS,pos);
    ae_wr16(r,t+AE_T_WIDTH,width<0 ? 0 : (uint32_t)width);
    ae_wr16(r,t+AE_T_STEP,width<0 ? 0u-(uint32_t)width : (uint32_t)asr(width,2));
    ae_wr8(r,AE_TRAIL_CURSOR,(cursor+1)&7);
}
/* EffectObjAlloc @8006FE7C */
uint32_t ae_EffectObjAlloc(AeRam *r) {
    uint32_t cursor=ae_rd8(r,AE_EFFECT_CURSOR), e=AE_EFFECTS+cursor*AE_EFFECT_SIZE;
    ae_wr8(r,e+AE_E_HEADER,16); ae_wr8(r,e+AE_E_DELAY,0);
    ae_wr16(r,e+AE_E_TIMER,0); ae_wr16(r,e+AE_E_FLAGS,0); ae_wr16(r,e+AE_E_SPIN,0);
    ae_wr8(r,AE_EFFECT_CURSOR,(cursor+1)&15);
    return e;
}
/* EffectObjSpawn @80070130 */
void ae_EffectObjSpawn(AeRam *r, const AeSpawnHooks *h, uint32_t pos, uint32_t def,
                       int32_t count, int32_t delay, int32_t spacing, uint32_t direction) {
    uint32_t matrix=AE_EFFECT_MATRIX;
    if(direction) {
        matrix=LOCAL_EFFECT_MATRIX;
        call_hook(r,h,h ? h->MatrixLookDir : NULL,"MatrixLookDir",direction,matrix);
        if(ae_ram_failed(r)) return;
    }
    while(count>0) {
        uint32_t e=ae_EffectObjAlloc(r);
        /* Copy in four-word blocks like the original (including possible overlap). */
        for(uint32_t off=0;off<80;off+=16) {
            uint32_t words[4];
            for(unsigned k=0;k<4;k++) words[k]=ae_rd32(r,def+off+4*k);
            for(unsigned k=0;k<4;k++) ae_wr32(r,e+off+4*k,words[k]);
        }
        ae_wr32(r,e+80,ae_rd32(r,def+80));
        count--;
        copy_position(r,e+AE_E_POS,pos);
        ae_wr8(r,e+AE_E_DELAY,(uint32_t)delay+(uint32_t)count*(uint32_t)spacing);
        for(uint32_t off=0;off<32;off+=16) {
            uint32_t words[4];
            for(unsigned k=0;k<4;k++) words[k]=ae_rd32(r,matrix+off+4*k);
            for(unsigned k=0;k<4;k++) ae_wr32(r,e+AE_E_MATRIX+off+4*k,words[k]);
        }
    }
}
/* EffectSpawnFromDef @80070284 */
void ae_EffectSpawnFromDef(AeRam *r, const AeSpawnHooks *h, uint32_t pos, uint32_t def,
                           int32_t count, int32_t delay, uint32_t direction) {
    ae_EffectObjSpawn(r,h,pos,def,count,delay,6,direction);
}
/* SpawnEffectGroup @800696EC */
void ae_SpawnEffectGroup(AeRam *r, const AeSpawnHooks *h, int32_t type, uint32_t pos,
                        uint32_t particles, uint32_t groups) {
    if(type<256) { ae_SpawnOneParticle(r,type,pos,particles); return; }
    uint32_t index=(uint32_t)type-(type<456 ? 256u : 456u);
    uint32_t stream=ae_rd32(r,groups+4*index);
    uint32_t entry=ae_rd16(r,stream); stream+=2;
    if(!entry) return;
    if(!position_padding(r,h,LOCAL_GROUP,0x800696EC)) return;
    while(entry && !ae_ram_failed(r)) {
        ae_wr16(r,LOCAL_GROUP,ae_rd16(r,pos));
        ae_wr16(r,LOCAL_GROUP+2,ae_rd16(r,pos+2)+ae_rd16(r,stream));
        ae_wr16(r,LOCAL_GROUP+4,ae_rd16(r,pos+4)); stream+=2;
        if(entry==50) {
            int32_t id=ae_rds16(r,stream), count=ae_rds16(r,stream+2), delay=ae_rds16(r,stream+4);
            stream+=6;
            ae_EffectSpawnFromDef(r,h,LOCAL_GROUP,AE_EFFECT_DEFS+(uint32_t)id*AE_EFFECT_SIZE,count,delay,0);
        } else if(entry==51) {
            int32_t color=ae_rds16(r,stream), width=ae_rds16(r,stream+2), mode=ae_rds16(r,stream+4);
            stream+=6; ae_TrailSpawn(r,LOCAL_GROUP,color,width,mode);
        } else {
            int32_t count=ae_rds16(r,stream);
            uint32_t random=rand8(r);
            count+=asr((int64_t)random*ae_rds16(r,stream+2),8); stream+=4;
            for(int32_t n=0;n<count;n++) ae_SpawnOneParticle(r,(int32_t)entry,LOCAL_GROUP,particles);
        }
        entry=ae_rd16(r,stream); stream+=2;
    }
}
/* SpawnParticle @800698B8 */
void ae_SpawnParticle(AeRam *r, const AeSpawnHooks *h, int32_t type, uint32_t pos) {
    ae_SpawnEffectGroup(r,h,type,pos,AE_PARTICLE_SCRIPTS,AE_EFFECT_GROUPS);
}
/* ProjectileAllocSlot @80084AC8 */
uint32_t ae_ProjectileAllocSlot(AeRam *r) {
    for(uint32_t i=0;i<32;i++) {
        uint32_t p=AE_PROJECTILE_RECORDS+i*AE_RECORD_SIZE;
        if(ae_rd8(r,p+AE_R_STATE)) continue;
        ae_wr32(r,p+AE_R_UPDATE,0); ae_wr8(r,p+AE_R_KIND,0);
        ae_wr8(r,p+AE_R_BYTE2,128); ae_wr8(r,p+AE_R_AGE,0); ae_wr8(r,p+AE_R_STATE,3);
        ae_wr32(r,p+AE_R_OWNER,0); ae_wr32(r,p+AE_R_MISC,0);
        ae_wr16(r,p+AE_R_FLAGS,1); ae_wr16(r,p+AE_R_SCALE,512);
        ae_wr16(r,p+AE_R_FIELD30,64);
        ae_wr8(r,p+AE_R_PARAM_C,255); ae_wr8(r,p+AE_R_PARAM_B,255);
        return p;
    }
    return 0;
}
/* ProjectileInit @80084B48 */
void ae_ProjectileInit(AeRam *r, uint32_t p, uint32_t origin, uint32_t pos, uint32_t kind) {
    ae_wr8(r,p+AE_R_KIND,kind); ae_wr32(r,p+AE_R_ORIGIN,origin);
    for(unsigned k=0;k<3;k++) ae_wr16(r,p+AE_R_POS+2*k,ae_rd16(r,pos+2*k));
}
/* ProjectileSetByte2 @80084BB8 */
void ae_ProjectileSetByte2(AeRam *r, uint32_t p, uint32_t value) { ae_wr8(r,p+AE_R_BYTE2,value); }
/* ProjectileSetColor @80084B74 */
void ae_ProjectileSetColor(AeRam *r, uint32_t p, uint32_t index) {
    ae_wr8(r,p+AE_R_KIND,ae_rd8(r,p+AE_R_KIND)|16);
    for(unsigned k=0;k<3;k++) ae_wr8(r,p+AE_R_RGB+k,ae_rd8(r,AE_PROJECTILE_COLORS+3*index+k));
}
/* ProjectileSetUpdate @80084C78 */
void ae_ProjectileSetUpdate(AeRam *r, uint32_t p, uint32_t a, uint32_t b, uint32_t c) {
    ae_wr32(r,p+AE_R_UPDATE,0x80084C04);
    ae_wr16(r,p+AE_R_PARAM_A,a); ae_wr8(r,p+AE_R_PARAM_B,b); ae_wr8(r,p+AE_R_PARAM_C,c);
}
/* SparkAdd @8006E054: color is three halfwords, only their low bytes are used. */
void ae_SparkAdd(AeRam *r, uint32_t pos, uint32_t color) {
    uint32_t cursor=ae_rd8(r,AE_SPARK_CURSOR), s=AE_SPARKS+cursor*AE_SPARK_SIZE;
    ae_wr8(r,s+AE_S_LIFE,6);
    for(unsigned k=0;k<3;k++) ae_wr16(r,s+AE_S_POS+2*k,ae_rd16(r,pos+2*k));
    for(unsigned k=0;k<3;k++) ae_wr8(r,s+AE_S_RGB+k,ae_rd8(r,color+2*k));
    ae_wr8(r,AE_SPARK_CURSOR,(cursor+1)&7);
}
/* SparkAddRandom @8006E0DC */
void ae_SparkAddRandom(AeRam *r, uint32_t pos, uint32_t color, int32_t mode) {
    uint32_t random=rand8(r);
    if(mode==1) {
        ae_wr16(r,LOCAL_SPARK,ae_rd16(r,pos)+(random&127)-63);
        ae_wr16(r,LOCAL_SPARK+2,ae_rd16(r,pos+2)-128);
        random=rand8(r);
        ae_wr16(r,LOCAL_SPARK+4,ae_rd16(r,pos+4)+(random&127)-63);
    } else {
        ae_wr16(r,LOCAL_SPARK,ae_rd16(r,pos));
        ae_wr16(r,LOCAL_SPARK+2,ae_rd16(r,pos+2)-random);
        ae_wr16(r,LOCAL_SPARK+4,ae_rd16(r,pos+4));
    }
    ae_SparkAdd(r,LOCAL_SPARK,color);
}
/* CameraShake @80048A00: included because EffectBigBurst calls this leaf. */
void ae_CameraShake(AeRam *r, int32_t level) {
    if(ae_rd8(r,AE_SHAKE+8) && level<(int32_t)ae_rd8(r,AE_SHAKE+9)) return;
    ae_wr8(r,AE_SHAKE+9,(uint32_t)level);
    unsigned strength=level==1 ? 1 : level==2 ? 2 : 0;
    ae_wr8(r,AE_SHAKE+8,10+strength);
    uint32_t mask=(4u<<strength)-1, bias=2u<<strength;
    ae_wr16(r,AE_SHAKE,(rand8(r)&mask)-bias);
    ae_wr16(r,AE_SHAKE+2,(rand8(r)&(8u<<strength))-(4u<<strength));
    ae_wr16(r,AE_SHAKE+4,(rand8(r)&mask)-bias);
}
/* SpawnPuff @8006B3CC */
void ae_SpawnPuff(AeRam *r, const AeSpawnHooks *h, uint32_t pos) { ae_SpawnParticle(r,h,0x113,pos); }
/* EffectBigBurst @8006B3F0 */
void ae_EffectBigBurst(AeRam *r, const AeSpawnHooks *h, int32_t x, int32_t y, int32_t z) {
    if(!position_padding(r,h,LOCAL_BURST,0x8006B3F0)) return;
    ae_wr16(r,LOCAL_BURST,(uint32_t)x); ae_wr16(r,LOCAL_BURST+2,(uint32_t)y); ae_wr16(r,LOCAL_BURST+4,(uint32_t)z);
    ae_SpawnParticle(r,h,0x10D,LOCAL_BURST);
    if(ae_ram_failed(r)) return;
    ae_TrailSpawn(r,LOCAL_BURST,0,800,0);
    ae_EffectSpawnFromDef(r,h,LOCAL_BURST,0x800B1F1C,1,0,0);
    ae_wr16(r,LOCAL_BURST+2,ae_rd16(r,LOCAL_BURST+2)-32);
    ae_EffectSpawnFromDef(r,h,LOCAL_BURST,0x800B1CD0,1,0,0);
    for(unsigned n=0;n<16;n++) ae_SpawnParticle(r,h,23,LOCAL_BURST);
    for(unsigned n=0;n<16;n++) {
        ae_wr16(r,LOCAL_BURST,(uint32_t)x+rand8(r)-128);
        ae_wr16(r,LOCAL_BURST+2,(uint32_t)y-rand8(r));
        ae_wr16(r,LOCAL_BURST+4,(uint32_t)z+rand8(r)-128);
        ae_SpawnParticle(r,h,7,LOCAL_BURST);
    }
    ae_CameraShake(r,1);
}

/* ProjectileSpawn @80059304. The primary gameplay slot and secondary visual/
 * impact record are separate pools. Failure in the second pool deliberately
 * retains the preceding primary-slot writes, including a zero record pointer. */
int32_t ae_ProjectileSpawn(AeRam *r, const AeSpawnHooks *h, uint32_t pos,
                           uint32_t owner, int32_t kind, uint32_t velocity) {
    if(kind==6) {
        unsigned count=0;
        for(unsigned i=0;i<16;i++) if(ae_rd8(r,AE_PROJECTILES+i*AE_PROJECTILE_SIZE+AE_J_KIND)==6) count++;
        if(count>=4) return 1;
    }
    int32_t ground=ae_GetGroundHeight(r,h ? h->world : NULL,
        ae_rds16(r,pos),ae_rds16(r,pos+2),ae_rds16(r,pos+4));
    if(ae_ram_failed(r)) return 1;
    uint32_t p=0;
    for(unsigned i=0;i<16;i++) {
        uint32_t candidate=AE_PROJECTILES+i*AE_PROJECTILE_SIZE;
        if(!ae_rd8(r,candidate+AE_J_KIND)) { p=candidate; break; }
    }
    if(!p) return 1;
    ae_wr16(r,p+AE_J_TAIL,0); ae_wr32(r,p+AE_J_HEADER,0); ae_wr32(r,p+AE_J_OWNER,owner);
    for(unsigned k=0;k<3;k++) ae_wr16(r,p+AE_J_POS+2*k,ae_rd16(r,pos+2*k));
    ae_wr8(r,p+AE_J_DRAW,1); ae_wr8(r,p+AE_J_TIMER,0);
    ae_wr16(r,p+AE_J_GROUND,(uint32_t)ground); ae_wr8(r,p+AE_J_FLAG,0); ae_wr8(r,p+AE_J_AGE,0);
    ae_wr16(r,p+AE_J_POS_PAD,0); ae_wr8(r,p+AE_J_VARIANT,0); ae_wr8(r,p+AE_J_MISC,0);
    switch(kind) {
    case 6: {
        uint32_t variant=velocity;
        if((variant&255)>=5) variant=0;
        uint32_t def=AE_PROJECTILE_VARIANTS+12*(variant&255);
        ae_wr16(r,LOCAL_DIRECTION,ae_rd16(r,PLAYER+PLAYER_POS)-ae_rd16(r,pos));
        ae_wr16(r,LOCAL_DIRECTION+2,0);
        ae_wr16(r,LOCAL_DIRECTION+4,ae_rd16(r,PLAYER+PLAYER_POS+8)-ae_rd16(r,pos+4));
        ae_sdk_VectorNormalSS(r,LOCAL_DIRECTION,LOCAL_DIRECTION);
        if(ae_ram_failed(r)) return 1;
        call_hook(r,h,h ? h->MatrixFromDirXZ : NULL,"MatrixFromDirXZ",LOCAL_DIRECTION,LOCAL_MATRIX);
        if(ae_ram_failed(r)) return 1;
        ae_sdk_ApplyMatrixSV(r,LOCAL_MATRIX,def,LOCAL_DIRECTION);
        for(unsigned k=0;k<3;k++) ae_wr16(r,p+AE_J_VEL+2*k,ae_rd16(r,LOCAL_DIRECTION+2*k));
        ae_wr8(r,p+AE_J_VARIANT,ae_rd8(r,def+8));
        ae_wr8(r,p+AE_J_BEHAVIOR,1); ae_wr8(r,p+AE_J_TIMER,ae_rd8(r,def+9));
        call_hook(r,h,h ? h->PlaySfx : NULL,"PlaySfx",0x4C,0);
        if(ae_ram_failed(r)) return 1;
        break;
    }
    case 7: {
        ae_wr8(r,p+AE_J_BEHAVIOR,3);
        int32_t delta_y=s32(ae_rd32(r,PLAYER+PLAYER_POS+4)-(uint32_t)ae_rds16(r,pos+2));
        int32_t height=delta_y/3;
        if(s16((uint32_t)height)<-1024) height=-1024;
        else if(s16((uint32_t)height)>1024) height=1024;
        int32_t scale=1024-s16((uint32_t)height);
        for(unsigned axis=0;axis<3;axis+=2) {
            uint32_t delta=ae_rd16(r,PLAYER+PLAYER_POS+axis*4)+
                (ae_rd32(r,PLAYER+PLAYER_VEL+axis*4)<<4)-ae_rd16(r,pos+axis*2);
            int32_t scaled=s16((uint32_t)asr((int64_t)s16(delta)*scale,10));
            ae_wr16(r,p+AE_J_VEL+axis*2,(uint32_t)(scaled/24));
        }
        ae_wr16(r,p+AE_J_VEL+2,(uint32_t)-48);
        break;
    }
    case 8: case 9: {
        ae_wr8(r,p+AE_J_BEHAVIOR,4);
        int32_t divisor=kind==8 ? 128 : 32;
        int32_t vy=kind==8 ? -6 : -15;
        for(unsigned axis=0;axis<3;axis+=2) {
            int32_t delta=s16(ae_rd16(r,PLAYER+PLAYER_POS+axis*4)-ae_rd16(r,pos+axis*2));
            ae_wr16(r,p+AE_J_VEL+axis*2,(uint32_t)(delta/divisor));
        }
        ae_wr16(r,p+AE_J_VEL+2,(uint32_t)vy);
        break;
    }
    case 5:
        ae_wr16(r,p+AE_J_VEL+2,(uint32_t)-20);
        ae_wr16(r,p+AE_J_TPAGE,0x5B); ae_wr16(r,p+AE_J_CLUT,0x7CAD);
        ae_wr8(r,p+AE_J_DRAW,3); ae_wr16(r,p+AE_J_VEL,0); ae_wr16(r,p+AE_J_VEL+4,0);
        ae_wr8(r,p+AE_J_U,31); ae_wr8(r,p+AE_J_V,31);
        break;
    case 1: case 2: {
        for(unsigned k=0;k<3;k++) ae_wr16(r,LOCAL_VELOCITY+2*k,ae_rd16(r,velocity+2*k));
        uint32_t record=ae_ProjectileAllocSlot(r); ae_wr32(r,p+AE_J_RECORD,record);
        if(!record) return 1;
        if(kind==1) ae_ProjectileSetByte2(r,record,0x38);
        ae_ProjectileInit(r,record,p+AE_J_POS,LOCAL_VELOCITY,0);
        ae_ProjectileSetColor(r,record,kind==1 ? 1 : 2);
        ae_ProjectileSetUpdate(r,record,kind==1 ? 0x10A : 0x112,kind==1 ? 0x6B : 0x5A,kind==1 ? 0x59 : 0xFF);
        ae_wr32(r,record+AE_R_OWNER,ae_rd32(r,p+AE_J_OWNER));
        ae_SpawnParticle(r,h,kind==1 ? 0x10B : 0x118,p+AE_J_POS);
        if(ae_ram_failed(r)) return 1;
        break;
    }
    case 10: {
        ae_wr8(r,p+AE_J_BEHAVIOR,12);
        ae_wr16(r,LOCAL_VELOCITY,ae_rd16(r,PLAYER+PLAYER_POS)-ae_rd16(r,p+AE_J_POS));
        ae_wr16(r,LOCAL_VELOCITY+2,0);
        ae_wr16(r,LOCAL_VELOCITY+4,ae_rd16(r,PLAYER+PLAYER_POS+8)-ae_rd16(r,p+AE_J_POS+4));
        ae_sdk_VectorNormalSS(r,LOCAL_VELOCITY,LOCAL_VELOCITY);
        if(ae_ram_failed(r)) return 1;
        ae_wr16(r,LOCAL_VELOCITY,(uint32_t)(ae_rds16(r,LOCAL_VELOCITY)/128));
        ae_wr16(r,LOCAL_VELOCITY+4,(uint32_t)(ae_rds16(r,LOCAL_VELOCITY+4)/128));
        ae_wr16(r,LOCAL_VELOCITY+2,46);
        for(unsigned k=0;k<3;k++) ae_wr16(r,p+AE_J_VEL+2*k,ae_rd16(r,LOCAL_VELOCITY+2*k));
        uint32_t record=ae_ProjectileAllocSlot(r); ae_wr32(r,p+AE_J_RECORD,record);
        if(!record) return 1;
        ae_ProjectileInit(r,record,p+AE_J_POS,LOCAL_VELOCITY,velocity==1 ? 65 : 33);
        ae_wr32(r,record+AE_R_OWNER,ae_rd32(r,p+AE_J_OWNER));
        ae_wr8(r,record+AE_R_SIZE,8); ae_wr16(r,record+AE_R_FLAGS,0xC01);
        break;
    }
    case 3: case 4:
        for(unsigned k=0;k<3;k++) ae_wr16(r,p+AE_J_VEL+2*k,0);
        break;
    default: break;
    }
    ae_wr8(r,p+AE_J_KIND,(uint32_t)kind);
    return 0;
}
