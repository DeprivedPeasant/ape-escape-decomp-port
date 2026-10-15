/* Integer 22.05 kHz SPU reverb network. 44.1 kHz FIR and mixer integration
 * are separate; this core is not yet selected by the native viewer. */
#ifndef SP_REVERB_H
#define SP_REVERB_H
#include <stdint.h>
typedef struct {uint16_t reg[32];uint32_t base,cursor;int16_t *ram;} SpReverb;
static inline int sp_rv_clip(int v){return v>32767?32767:v< -32768?-32768:v;}
/* SPU addresses wrap once at RAM end, adding the work-area base. */
static inline uint32_t sp_rv_address(const SpReverb *r,int words){
    uint32_t v=r->cursor+((uint32_t)words&262143u);
    if(v&262144u)v+=r->base;
    return v&262143u;
}
static inline int sp_rv_read(const SpReverb *r,int offset,int adjust){return r->ram[sp_rv_address(r,offset*4+adjust)];}
static inline void sp_rv_write(SpReverb *r,int offset,int value){r->ram[sp_rv_address(r,offset*4)]=(int16_t)value;}
static inline int sp_rv_coef(const SpReverb *r,int i){return (int16_t)r->reg[i];}
static inline int sp_rv_complement(int input,int alpha){
    if(alpha==-32768)return input==-32768?0:input*-65536;
    return input*(32768-alpha);
}
static inline int sp_rv_negate(int v){return v==-32768?32767:-v;}
static inline void sp_reverb_step(SpReverb *r,int left,int right,int out[2]){
    const int input[2]={left,right};
    const int reflections[2][2]={{10,16},{18,24}};
    const int comb[4]={12,14,20,22};
    int alpha=sp_rv_coef(r,2),wall=sp_rv_coef(r,7);
    int ap1=sp_rv_coef(r,8),ap2=sp_rv_coef(r,9);
    for(int channel=0;channel<2;channel++){
        int filtered[2];
        for(int kind=0;kind<2;kind++){
            int dest=reflections[kind][0]+channel;
            int source=reflections[kind][1]+(channel^kind);
            int excitation=sp_rv_clip(((sp_rv_read(r,r->reg[source],0)*wall>>14)
                +(input[channel]*sp_rv_coef(r,30+channel)>>14))>>1);
            int old=sp_rv_read(r,r->reg[dest],-1);
            filtered[kind]=sp_rv_clip(((excitation*alpha>>14)+(sp_rv_complement(old,alpha)>>14))>>1);

        }
        for(int kind=0;kind<2;kind++)sp_rv_write(r,r->reg[reflections[kind][0]+channel],filtered[kind]);
        int sum=0;
        for(int tap=0;tap<4;tap++)sum+=sp_rv_read(r,r->reg[comb[tap]+channel],0)*sp_rv_coef(r,3+tap)>>14;
        int delay1=sp_rv_read(r,(int)r->reg[26+channel]-r->reg[0],0);
        int delay2=sp_rv_read(r,(int)r->reg[28+channel]-r->reg[1],0);
        int stage1=sp_rv_clip((sum+(delay1*sp_rv_negate(ap1)>>14))>>1);
        int stage2=sp_rv_clip(delay1+(((stage1*ap1>>14)+(delay2*sp_rv_negate(ap2)>>14))>>1));
        out[channel]=sp_rv_clip(delay2+(stage2*ap2>>15));
        sp_rv_write(r,r->reg[26+channel],stage1);
        sp_rv_write(r,r->reg[28+channel],stage2);
    }
    r->cursor=(r->cursor+1)&262143u;if(!r->cursor)r->cursor=r->base;
}
#endif
