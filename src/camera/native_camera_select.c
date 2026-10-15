#include "native_camera_select.h"
#include <string.h>

static int16_t s16(uint32_t x) {
    x &= 65535u;
    return (int16_t)(x < 32768u ? (int32_t)x : (int32_t)x - 65536);
}
static int32_t s32(uint32_t x) {
    return x <= 0x7FFFFFFFu ? (int32_t)x : (int32_t)((int64_t)x - 0x100000000LL);
}

int ae_camera_select_mode(uint8_t c[0x50], const AeCameraSlot *slots,
                           uint32_t count, const int32_t player[3],
                           const uint8_t eye8[8], const uint8_t target8[8],
                           int is_flight_area, const AeCameraMathTables *tables,
                           AeCameraSelection *events) {
    uint32_t chosen, kind=1, previous, style=0, snapshot=0;
    AeCameraSelection result={0,0,0};
    if(!c||!slots||!count||!player||!eye8||!target8||!events)return 0;
    chosen=c[0x18];
    if(c[0x1B]!=255){
        chosen=c[0x1B];
        if(chosen>=count)return 0;
        kind=slots[chosen].raw[0x8E]==1?5:4;
    }
    if(c[0x42]){chosen=c[0x18];kind=1;}
    if(c[0x38]==1){chosen=c[0x1A];kind=3;}
    if((c[0x32]==1||c[0x46]==1)&&!is_flight_area){chosen=c[0x19];kind=2;}
    if(c[0x41]){chosen=c[0x1B];kind=4;}
    previous=c[0x0E];
    if(chosen>=count||previous>=count)return 0;
    if(chosen==previous){*events=result;return 1;}
    if(previous==c[0x19]||chosen==c[0x19])style=2;
    else if(kind==3||c[0x0F]==3)style=3;
    else if(kind==5){
        if(slots[chosen].raw[0x33])style=3;
        else {
            int16_t delta[3]={s16((uint32_t)player[0]-(uint32_t)ae_camera_i16(&slots[previous],0x0A)),
                             0,s16((uint32_t)player[2]-(uint32_t)ae_camera_i16(&slots[previous],0x0E))};
            uint32_t length;
            if(!tables||!ae_camera_normalize(tables,delta,&length))return 0;
            uint32_t dot=(uint32_t)((int32_t)ae_camera_i16(&slots[chosen],0x94)*delta[0])+
                         (uint32_t)((int32_t)ae_camera_i16(&slots[chosen],0x98)*delta[2]);
            if(s32(dot)<0){*events=result;return 1;}
        }
    } else if(kind==1&&(uint8_t)(c[0x0F]-4u)<2){style=1;snapshot=1;}
    c[0x30]=(kind==4&&c[0x41])?0:0x80;c[0x31]=0;
    c[0x2E]=(uint8_t)style;
    memcpy(c+0x1C,eye8,8);memcpy(c+0x24,target8,8);
    c[0x0E]=(uint8_t)chosen;c[0x0F]=(uint8_t)kind;
    result.snapshot_follow=snapshot;result.activate=1;result.slot=(int32_t)chosen;
    *events=result;
    return 1;
}

int ae_camera_snapshot_eye(AeCameraSlot *follow, const int32_t player[3],
                            const uint8_t eye8[8], const AeCameraMathTables *tables) {
    AeCameraSlot copy;
    int16_t delta[3];
    int32_t yaw;
    uint32_t length;
    if(!follow||!player||!eye8||!tables)return 0;
    copy=*follow;
    memcpy(copy.raw+0x0A,eye8,8);
    delta[0]=s16((uint32_t)player[0]-(uint32_t)ae_camera_i16(&copy,0x0A));
    delta[1]=0;
    delta[2]=s16((uint32_t)player[2]-(uint32_t)ae_camera_i16(&copy,0x0E));
    if(!ae_camera_normalize(tables,delta,&length)||
       !ae_camera_atan2_safe(tables,-(int32_t)delta[0],-(int32_t)delta[2],&yaw))return 0;
    copy.raw[0x33]=1;copy.raw[0x32]=0;copy.raw[0x30]=0;
    ae_camera_set32(&copy,0x38,0x460);ae_camera_set16(&copy,0x04,0x460);
    ae_camera_set32(&copy,0x40,yaw);
    *follow=copy;
    return 1;
}

int ae_camera_activate_slot(uint8_t c[0x50], const AeCameraSlot *slots,
                             uint32_t count, uint32_t index, uint32_t base) {
    uint64_t address=(uint64_t)base+(uint64_t)index*AE_CAMERA_SLOT_SIZE;
    if(!c||!slots||index>=count||address>0xFFFFFFFFu)return 0;
    c[4]=(uint8_t)index;c[5]=(uint8_t)(index>>8);
    c[6]=slots[index].raw[0];c[7]=0;
    for(unsigned i=0;i<4;++i)c[8+i]=(uint8_t)(address>>(8*i));
    return 1;
}
