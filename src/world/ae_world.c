#include "ae_world.h"

#define OBJS 0x800E6960u
#define SLOTS 0x800E77B0u
#define TARGETS 0x800F51F0u
#define GIMMICKS 0x800F4570u
#define SCR 0x1F8003F0u

static int32_t s16(uint32_t x) { return (int16_t)x; }
static int32_t hook(AeRam *r, const AeWorldHooks *h, AeWorldHookFn f,
                    const char *name, uint32_t a, uint32_t b, uint32_t c) {
    if (!f) { ae_ram_fail(r, "world hook %s not provided", name); return 0; }
    return f(h->user, r, a, b, c, 0);
}
#define CALL(name,a,b,c) hook(r,h,h ? h->name : 0,#name,(uint32_t)(a),(uint32_t)(b),(uint32_t)(c))

/* 80032708: signed word count, not a byte count. Original has no defined return. */
void ae_ClearWords(AeRam *r, uint32_t p, int32_t n) {
    for (int32_t i = 0; i < n && !ae_ram_failed(r); ++i, p += 4) ae_wr32(r,p,0);
}
/* 8005DCE4 */
int32_t ae_StageExitIsActive(AeRam *r) {
    uint32_t code = ae_rd8(r,0x800F447Cu);
    return code != 7 && code != 28;
}
/* 8007CD88 */
int32_t ae_TargetListAdd(AeRam *r, uint32_t p) {
    for (int32_t i=0;i<32;++i) if (!ae_rd32(r,TARGETS+4u*i)) {
        ae_wr32(r,TARGETS+4u*i,p); return i;
    }
    return -1;
}
/* 8007CDC4: remove every occurrence, including matches to zero. */
void ae_TargetListRemove(AeRam *r, uint32_t p) {
    for (uint32_t i=0;i<32;++i) if (ae_rd32(r,TARGETS+4*i)==p) ae_wr32(r,TARGETS+4*i,0);
}
/* 80052C4C */
int32_t ae_ObjSlotGetAnimIndex(AeRam *r, int32_t slot) {
    int32_t i=ae_rds8(r,SLOTS+(uint32_t)s16((uint32_t)slot));
    return i==-1 ? 0 : (int32_t)ae_rd16(r,OBJS+(uint32_t)i*0xE4u+0x5E);
}
/* 80052CA0 */
void ae_ObjSlotSetField_CF(AeRam *r, int32_t slot, uint32_t value) {
    int32_t i=ae_rds8(r,SLOTS+(uint32_t)s16((uint32_t)slot));
    if(i!=-1) ae_wr8(r,OBJS+(uint32_t)i*0xE4u+0xCF,value);
}
/* 80052D04: unlike the other slot queries, a0 is NOT truncated to s16. */
int32_t ae_ObjSlotGetTarget(AeRam *r, uint32_t slot, uint32_t out) {
    int32_t i=ae_rds8(r,SLOTS+slot);
    if(i==-1) return 0;
    uint32_t obj=OBJS+(uint32_t)i*0xE4u;
    for(uint32_t k=0;k<3;++k) ae_wr32(r,out+4*k,(uint32_t)ae_rds16(r,obj+0x9A+2*k));
    return 1;
}
/* 80052CEC */
void ae_SetDAT_800E77E2(AeRam *r, uint32_t value) { ae_wr8(r,0x800E77E2u,value); }
/* 8005B7BC: only the upper bound is checked; negative indices are accepted. */
int32_t ae_GimmickGetInfo(AeRam *r, int32_t index, uint32_t out) {
    uint32_t count=0,groups=ae_rd32(r,0x800F456Cu);
    int32_t n=ae_rds32(r,0x800F45D8u);
    for(int32_t i=0;i<n && !ae_ram_failed(r);++i) count+=(uint32_t)ae_rds16(r,groups+8u*i);
    int32_t ix=s16((uint32_t)index);
    if(ix>=(int32_t)count) return -1;
    uint32_t ofs=(uint32_t)ix*24u;
    for(uint32_t k=0;k<3;++k) {
        uint32_t v=ae_rd16(r,ae_rd32(r,GIMMICKS)+ofs+2*k);
        ae_wr16(r,out+2*k,v);
    }
    return 0;
}
/* 8005B880: dispatch numbers taken from the EXE jump table, not guessed from types. */
int32_t ae_GimmickEvent(AeRam *r, const AeWorldHooks *h, uint32_t event, int32_t index, uint32_t arg) {
    if(index<0) return -1;
    uint32_t g=ae_rd32(r,GIMMICKS)+(uint32_t)index*24u;
    uint32_t col=ae_rd32(r,0x800F455Cu)+(uint32_t)ae_rds16(r,g+4)*56u;
    if(event>=16) return 0;
    uint32_t dest=ae_rd32(r,0x800A5620u+4*event);
    int32_t state;
    switch(dest) {
    case 0x8005B908u:
        if(ae_rds16(r,g)==43) {
            if(ae_rds16(r,g+2)==4) { ae_wr16(r,g+2,5); CALL(PlaySfxAt,176,col,0); }
            break;
        }
        if(ae_rds16(r,g+2)!=0) break;
        CALL(GimmickActivate,ae_rds16(r,g+12),g,0);
        if(ae_ram_failed(r)) return 0;
        ae_wr16(r,g+2,1);
        ae_wr16(r,col+24,ae_rd16(r,col+24)|0x8000);
        CALL(PlaySfxAt,117,col,0);
        if(ae_ram_failed(r)) return 0;
        if(ae_rds16(r,g+16)>=0 && ae_rds16(r,col+14)>=0)
            CALL(MeshInitFromAreaTable,ae_rd32(r,0x800B4D64u)+(uint32_t)ae_rds16(r,col+14)*32u,ae_rds16(r,g+16),0);
        break;
    case 0x8005B9B8u:
        if(ae_rds16(r,g+2)!=0) break;
        ae_wr16(r,g+2,1); ae_wr16(r,g+6,ae_rd16(r,g+10));
        if(ae_rd16(r,0x800F4476u)==41) {
            CALL(PlaySfxAt,172,col,0);
            if(ae_ram_failed(r)) return 0;
            CALL(FUN_8013d358,ae_rds16(r,col),ae_rds16(r,col+2),ae_rds16(r,col+4));
        } else CALL(PlaySfxAt,175,col,0);
        if(ae_ram_failed(r)) return 0;
        CALL(CameraShake,0,0,0);
        if(!ae_ram_failed(r)) ae_SetDAT_800E77E2(r,1);
        break;
    case 0x8005BA40u:
        state=ae_rds16(r,g+2);
        if(state==4) ae_wr16(r,g+2,5); else if(state==1) ae_wr16(r,g+2,2);
        break;
    case 0x8005BA60u:
        ae_wr16(r,g+2,1); CALL(EventCamStart,0,0,0);
        if(!ae_ram_failed(r)) ae_wr32(r,0x800F4450u,7);
        break;
    case 0x8005BA8Cu: ae_wr16(r,g+16,1); break;
    case 0x8005BA98u: ae_wr16(r,g+14,1); break;
    case 0x8005BAA4u: if(!ae_rds16(r,g+2)) ae_wr16(r,g+2,1); break;
    case 0x8005BAC4u: ae_wr16(r,g+2,1); break;
    case 0x8005BABCu: if(arg<4) ae_wr16(r,g+2,arg+1); break;
    case 0x8005BB0Cu: if(ae_rds16(r,g+2)==4) ae_wr16(r,g+2,5); break;
    case 0x8005BB28u:
        if(!ae_rds16(r,g+2)) { ae_wr16(r,g+2,1); ae_wr16(r,g+6,0); } break;
    case 0x8005BB44u: break;
    default: ae_ram_fail(r,"GimmickEvent invalid dispatch address %08x",dest); break;
    }
    return 0;
}

/* 80043400: four triangle regions; signed halfword slope differences and division
 * toward zero are essential, even when input heights span the full s16 range. */
int32_t ae_GetGroundHeight(AeRam *r, const AeWorldHooks *h, int32_t x, int32_t y, int32_t z) {
    uint32_t header=ae_rd32(r,0x800F44E4u);
    ae_wr16(r,SCR,0xFFFF);
    for(uint32_t k=2;k<16;k+=2) ae_wr16(r,SCR+k,0);
    int32_t ground=16384;
    int32_t sx=h ? h->ground_stale_x : 0, sz=h ? h->ground_stale_z : 0;
    if(!(ae_rd32(r,header)&2)) {
        uint32_t w=ae_rd16(r,0x800EBFA0u), d=ae_rd16(r,0x800EBFA2u);
        uint32_t xx=(uint32_t)x+(w<<8), zz=(uint32_t)z+(d<<8);
        int32_t ix=(int32_t)(xx<<7)>>16, iz=s16((uint32_t)((int32_t)zz>>9));
        if(ix>=(int32_t)w || iz>=(int32_t)d || ix<0 || iz<0) return 0;
        int32_t fx=(int32_t)(xx&511), fz=(int32_t)(zz&511);
        uint32_t cell=ae_rd32(r,ae_rd32(r,0x800EBFA8u)+4u*((uint32_t)iz*w+(uint32_t)ix));
        ae_wr16(r,SCR+8,cell>>16);
        uint32_t region=(uint32_t)((fx<fz)*2+(512-fx<fz));
        if(cell&0x800) region|=0x80|((cell>>8)&0x70);
        uint32_t ht=ae_rd32(r,0x800EBFB4u), k=(w+1)*(uint32_t)iz+(uint32_t)ix;
        int32_t a=ae_rds16(r,ht+2*k), b=ae_rds16(r,ht+2*(k+1));
        int32_t c=ae_rds16(r,ht+2*(k+w+1)), e=ae_rds16(r,ht+2*(k+w+2));
        int32_t mid=ae_rds16(r,ht+2*((w+1)*(d+1)+(uint32_t)iz*w+(uint32_t)ix));
        sx=0; sz=0;
        switch(region) {
        case 0: case 0x90: case 0xA0:
            sx=s16((uint32_t)(b-a)); sz=s16((uint32_t)(2*(mid-sx/2-a)));
            ground=a+fx*sx/512+fz*sz/512; break;
        case 1: case 0xA1: case 0xB1:
            sz=s16((uint32_t)(e-b)); sx=s16((uint32_t)(2*(b+sz/2-mid)));
            ground=b+(-sx)*(512-fx)/512+fz*sz/512; break;
        case 2: case 0x92: case 0xC2:
            sz=s16((uint32_t)(c-a)); sx=s16((uint32_t)(2*(mid-a-sz/2)));
            ground=a+fx*sx/512+fz*sz/512; break;
        case 3: case 0xB3: case 0xC3:
            sx=s16((uint32_t)(e-c)); sz=s16((uint32_t)(2*(c+sx/2-mid)));
            ground=c+fx*sx/512+(-sz)*(512-fz)/512; break;
        default: break;
        }
        /* Scratch normals are truncated separately from the multipliers above. */
    }
    int32_t terrain=s16((uint32_t)ground);
    int32_t solid=s16((uint32_t)CALL(GetSolidHeight,x,y<terrain?y:terrain,z));
    if(ae_ram_failed(r)) return 0;
    if(terrain<solid) {
        ae_wr16(r,SCR+12,(uint32_t)sx); ae_wr16(r,SCR+14,(uint32_t)sz);
        if(y<terrain) ae_wr16(r,SCR+8,ae_rd16(r,SCR+8)&0xFFE0);
        return terrain;
    }
    if(y<solid) ae_wr16(r,SCR+8,ae_rd16(r,SCR+8)&0xFFE0);
    return solid;
}
/* 800433B0 */
int32_t ae_GetGroundHeightEx(AeRam *r, const AeWorldHooks *h, int32_t x, int32_t y, int32_t z, uint32_t out) {
    int32_t v=ae_GetGroundHeight(r,h,x,y,z);
    ae_wr16(r,out+2,ae_rd16(r,SCR+14)); ae_wr16(r,out,ae_rd16(r,SCR+12));
    return s16((uint32_t)v);
}
