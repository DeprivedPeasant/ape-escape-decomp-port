/* Original driver allocation policy, independent of oscillator/envelope storage.
 * Allocation mutates order/counts; caller then installs a voice with age=count
 * and increments count (saturating at24), as the original note-on handlers do.
 */
#ifndef SP_VOICE_POOL_H
#define SP_VOICE_POOL_H
#include <stdint.h>
typedef struct {uint16_t active,freeable,age,effect,priority,effect_id;} SpPoolVoice;
typedef struct {SpPoolVoice voice[24];uint8_t cursor,count,effects,effect_limit;} SpVoicePool;
static inline int sp_pool_music(SpVoicePool *p){
    for(int n=0;n<24;n++){
        p->cursor=p->cursor<23?p->cursor+1:0;
        if(!p->voice[p->cursor].active)return p->cursor;
    }
    unsigned oldest=24;
    for(int i=0;i<24;i++)if(p->voice[i].effect==0&&p->voice[i].freeable==1&&p->voice[i].age<oldest){oldest=p->voice[i].age;p->cursor=(uint8_t)i;}
    if(oldest==24){
        p->cursor=255;
        for(int i=0;i<24;i++)if(p->voice[i].effect==0&&p->voice[i].age<24){p->cursor=(uint8_t)i;oldest=p->voice[i].age;break;}
        if(p->cursor==255)return 255;
    }
    for(int i=0;i<24;i++)if(oldest<p->voice[i].age)--p->voice[i].age;
    --p->count;return p->cursor;
}
static inline int sp_pool_effect(SpVoicePool *p,unsigned effect_id,unsigned priority){
    effect_id&=255;priority&=255;
    if(effect_id)for(int i=0;i<24;i++)if(p->voice[i].effect==1&&p->voice[i].effect_id==effect_id){
        unsigned age=p->voice[i].age;
        for(int j=0;j<24;j++)if(age!=64&&age<p->voice[j].age)--p->voice[j].age;
        --p->count;return i;
    }
    int pick=0;unsigned age=24;
    if(p->effects<p->effect_limit){
        for(int n=0;n<24;n++){
            p->cursor=p->cursor<23?p->cursor+1:0;
            if(!p->voice[p->cursor].active){++p->effects;return p->cursor;}
        }
        int freeable=0;
        for(int i=0;i<24;i++)if(p->voice[i].effect!=1&&p->voice[i].freeable==1)freeable=1;
        for(int i=0;i<24;i++)if(p->voice[i].effect!=1&&(!freeable||p->voice[i].freeable==1)&&p->voice[i].age<age){pick=i;age=p->voice[i].age;}
        for(int i=0;i<24;i++)if(age<p->voice[i].age&&p->voice[i].age!=65535)--p->voice[i].age;
        ++p->effects;--p->count;return pick;
    }
    for(unsigned lower=0;lower<24;lower++){
        age=24;
        for(int i=0;i<24;i++)if(p->voice[i].effect==1&&lower<=p->voice[i].age&&p->voice[i].age<age){pick=i;age=p->voice[i].age;}
        if(p->voice[pick].priority<=priority){
            for(int i=0;i<24;i++)if(age<p->voice[i].age&&p->voice[i].age!=65535)--p->voice[i].age;
            --p->count;return pick;
        }
    }
    return 255;
}
#endif
