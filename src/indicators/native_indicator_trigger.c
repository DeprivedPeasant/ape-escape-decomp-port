#include "native_indicator_trigger.h"
#include "../collision/native_collision.h"
#include <string.h>
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static int32_t s16(uint32_t v) { v&=65535u; return v>=32768u?(int32_t)v-65536:(int32_t)v; }
static int32_t s32(uint32_t v) { return v<=2147483647u?(int32_t)v:-1-(int32_t)(~v); }
static void put16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }
int32_t ae_indicator_trigger_distance(const int32_t player[3],const int16_t center[3]) {
    uint32_t sum=0;
    if(!player||!center)return 0;
    for(int i=0;i<3;++i){int32_t v=s16((uint32_t)player[i]-(uint32_t)(int32_t)center[i]);sum+=(uint32_t)(v*v);}
    return ae_collision_sqrt(s32(sum),0);
}
int ae_indicator_trigger_update(AeIndicatorTrigger *trigger,const int16_t center[3],
    int16_t touched_by,int32_t distance,uint32_t area,uint32_t exit_code,
    AeIndicatorTriggerEvents *events) {
    if(!trigger||!center||!events)return 0;
    memset(events,0,sizeof(*events));
    exit_code&=255u;
    if(exit_code==0x17||exit_code==7||exit_code==0x15)return 1;
    int32_t state=s16(u16(trigger->raw+2)),radius=s16(u16(trigger->raw+14));
    if(state==0){
        if(distance>=radius)return 1;
        events->activate=1;
        put16(trigger->raw+2,((uint32_t)(uint16_t)area-0x5Cu)<7u?3:1);
    } else if(state==1){
        if(distance>=radius||touched_by<0||u16(trigger->raw+16)!=0)return 1;
        events->activate=events->effect=1;
    } else if(state==3){
        if(distance>radius){put16(trigger->raw+2,0);return 1;}
        if(touched_by<0)return 1;
        events->activate=events->effect=1;
    } else return 1;
    events->index=s16(u16(trigger->raw+20));events->sector=s16(u16(trigger->raw+18));
    if(events->effect){events->effect_position[0]=center[0];events->effect_position[1]=s16((uint32_t)(int32_t)center[1]-0xBEu);events->effect_position[2]=center[2];}
    return 1;
}
