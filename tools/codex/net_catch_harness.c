/* Real viewer host, with a close-range encounter as test setup only. */
#include "../../src/app/viewer_game.h"
#include <stdio.h>

#define PLAYER 0x800EC1E0u
int main(void) {
    const char *exe="extract/disc/SCUS_944.23", *member="extract/members/05B.bin";
    AeGround *ground=ae_ground_load(member);
    AeCollision *collision=ae_collision_load(member);
    int32_t entry[9]; AeGroundHit hit; AeSolidHit contact;
    if(!ground || !collision || !ae_collision_math_load(exe) ||
       !ae_ground_entry(ground,0,entry) ||
       !ae_collision_ground_height(collision,ground,entry[0],entry[1],entry[2],0,-1,0,&hit,&contact))return 2;
    int32_t pos[3]={entry[0],hit.height,entry[2]};
    int16_t facing[3]={(int16_t)entry[6],(int16_t)entry[7],(int16_t)entry[8]};
    AePlayerHost player; ViewerGame session;
    if(!ae_player_host_init(&player,ground,collision,"extract/members/008.bin",exe,1,pos,facing))return 2;
    int ok=viewer_game_start(&session,&player,exe,1,0);
    AeRam *r=session.ram;
    AePlayerHostPad neutral={0,128,128,128,128};
    for(int i=0;ok && i<60;++i)ok=viewer_game_step(&session,&neutral);
    /* Approach from -Z, 150 units away, without changing monkey AI/contact. */
    for(uint32_t axis=0;axis<3;++axis)
        ae_wr32(r,PLAYER+0x24+axis*4,ae_rd32(r,AE_G_MONKEYS+AE_MK_POS+axis*4)-(axis==2?150u:0u));
    const uint32_t directions[]={0x3C,0x60,0x74};
    for(unsigned i=0;i<3;++i)for(uint32_t axis=0;axis<3;++axis)
        ae_wr16(r,PLAYER+directions[i]+axis*2,axis==2?4096:0);
    int caught=0, capture=0, removed=0;
    uint32_t id=ae_rd8(r,AE_G_MONKEYS+AE_MK_ID);
    ae_wr16(r,AE_G_CAUGHT_MONKEY,65535);
    for(int i=0;ok && i<49;++i) {
        AePlayerHostPad pad=neutral;
        if(i<10)pad.ry=0;
        ok=viewer_game_step(&session,&pad);
        if(ae_rd8(r,AE_G_MONKEYS+AE_MK_ACTIVITY)==3) {
            caught=ae_rd16(r,AE_G_CAUGHT_MONKEY)==id;
            removed=(ae_rd32(r,AE_G_MONKEYS+AE_MK_FLAGS3)&2u)==0;
        }
        if(ae_rd16(r,0x800F44A0u)==8)capture=1;
    }
    /* The original persists caught activity 3 as saved activity 2. */
    ae_MonkeyStoreActivities(r);
    printf("NET caught=%d id=%u caught_id=%u capture=%d removed=%d activity=%u saved=%u mode=%u error=%s\n",
        caught,id,ae_rd16(r,AE_G_CAUGHT_MONKEY),capture,removed,
        ae_rd8(r,AE_G_MONKEYS+AE_MK_ACTIVITY),ae_rd8(r,AE_G_MONKEY_ACTIVITY),
        ae_rd16(r,0x800F44A0u),ae_ram_error(r));
    ok=ok && caught && capture && removed && ae_rd8(r,AE_G_MONKEY_ACTIVITY)==2;
    viewer_game_report(&session);
    viewer_game_stop(&session);ae_player_host_free(&player);
    ae_ground_free(ground);ae_collision_free(collision);
    return ok?0:1;
}
