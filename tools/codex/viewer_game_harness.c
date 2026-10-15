/* Standalone game host: no SDL, GL, presentation loop or viewer globals. */
#include "../../src/app/viewer_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
    if(argc!=6)return 2;
    const char *exe="extract/disc/SCUS_944.23";
    int area=(int)strtol(argv[1],NULL,0),frames=atoi(argv[3]);
    AeGround *ground=ae_ground_load(argv[2]);AeCollision *collision=ae_collision_load(argv[2]);
    if(!ground||!collision||!ae_collision_math_load(exe))return 2;
    int32_t entry[9];AeGroundHit hit;AeSolidHit contact;
    if(!ae_ground_entry(ground,0,entry)||!ae_collision_ground_height(collision,ground,entry[0],entry[1],entry[2],0,-1,0,&hit,&contact))return 2;
    int32_t pos[3]={entry[0],hit.height,entry[2]};int16_t facing[3]={(int16_t)entry[6],(int16_t)entry[7],(int16_t)entry[8]};
    AePlayerHost player;ViewerGame session;
    if(!ae_player_host_init(&player,ground,collision,"extract/members/008.bin",exe,(uint32_t)area,pos,facing))return 2;
    int ok=viewer_game_start(&session,&player,exe,area,0);
    for(int i=0;ok&&i<frames;++i){AePlayerHostPad pad={0,128,128,128,128};if(strcmp(argv[5],"neutral"))viewer_game_script_input((uint32_t)i,&pad);ok=viewer_game_step(&session,&pad);}
    if(ae_ram_failed(session.ram)){fprintf(stderr,"%s\n",ae_ram_error(session.ram));ok=0;}
    viewer_game_report(&session);
    if(!viewer_game_dump(&session,argv[4]))ok=0;
    viewer_game_stop(&session);ae_player_host_free(&player);ae_ground_free(ground);ae_collision_free(collision);
    return ok?0:1;
}
