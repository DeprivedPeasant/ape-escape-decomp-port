/* Integer SPU envelope counters; implementation from register rules, not emulator code.
 * Independent test oracle remains outside the port under ignored extract/.
 */
#ifndef SP_ADSR_H
#define SP_ADSR_H
#include <stdint.h>
typedef struct {uint16_t a,b;uint32_t counter;int level,phase;} SpAdsr;
enum {SP_ADSR_ATTACK,SP_ADSR_DECAY,SP_ADSR_SUSTAIN,SP_ADSR_RELEASE,SP_ADSR_OFF};
static inline void sp_adsr_start(SpAdsr *e,uint16_t a,uint16_t b){e->a=a;e->b=b;e->counter=0;e->level=0;e->phase=SP_ADSR_ATTACK;}
static inline void sp_adsr_release(SpAdsr *e){if(e->phase!=SP_ADSR_OFF&&e->phase!=SP_ADSR_RELEASE){e->phase=SP_ADSR_RELEASE;e->counter=0;}}
static inline int sp_adsr_step(SpAdsr *e){
    if(e->phase==SP_ADSR_OFF)return 0;
    unsigned shift,low=0,mask=127;int down=0,exponential=0,target=0;
    switch(e->phase){
    case SP_ADSR_ATTACK:shift=(e->a>>10)&31;low=(e->a>>8)&3;exponential=e->a>>15;target=32767;break;
    case SP_ADSR_DECAY:shift=(e->a>>4)&15;mask=124;down=exponential=1;target=((e->a&15)+1)*2048;if(target>32767)target=32767;break;
    case SP_ADSR_SUSTAIN:shift=(e->b>>8)&31;low=(e->b>>6)&3;down=(e->b>>14)&1;exponential=e->b>>15;break;
    default:shift=e->b&31;mask=124;down=1;exponential=(e->b>>5)&1;break;
    }
    unsigned rate=shift*4+low,increment=32768u>>(shift>11?shift-11:0);
    if(!increment&&(rate&mask)!=mask)increment=1;
    int step=(down?-8+(int)low:7-(int)low)*(1<<(shift<11?11-shift:0));
    if(exponential){
        if(down)step=(step*e->level)>>15;
        else if(e->level>=24576){
            if(shift<10)step>>=2;
            else if(shift>10)increment>>=2;
            else{step>>=1;increment>>=1;}
        }
    }
    e->counter+=increment;
    if(e->counter&32768){
        e->counter=0;e->level+=step;
        if(e->level<0)e->level=0;
        if(e->level>32767)e->level=32767;
    }
    if(e->phase!=SP_ADSR_SUSTAIN&&((down&&e->level<=target)||(!down&&e->level>=target))){
        ++e->phase;e->counter=0;
    }
    return e->level;
}
#endif
