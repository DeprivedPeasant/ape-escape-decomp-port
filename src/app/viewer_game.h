#ifndef VIEWER_GAME_H
#define VIEWER_GAME_H
#include "../monkeys/ae_monkey.h"
typedef struct AeGame AeGame;
#include "native_player_host.h"
typedef struct {
    AeGame *game;
    AeRam *ram;
    AePlayerHost *player;
    AePlayerHostPad input;
    int (*audio_sync)(void);
    void (*audio_stop)(void);
    uint8_t *member,*overlay;
    size_t member_size,overlay_size;
    uint32_t overlay_id,overlay_address,frame;
} ViewerGame;
int viewer_game_start(ViewerGame *v,AePlayerHost *player,const char *exe,int area,int entry);
int viewer_game_step(ViewerGame *v,const AePlayerHostPad *input);
void viewer_game_stop(ViewerGame *v);
typedef void (*ViewerGameMarker)(void *user,const char *kind,const int32_t pos[3]);
void viewer_game_markers(ViewerGame *v,ViewerGameMarker draw,void *user);
unsigned viewer_game_missing_count(ViewerGame *v);
void viewer_game_script_input(uint32_t frame,AePlayerHostPad *pad);
int viewer_game_dump(ViewerGame *v,const char *path);
void viewer_game_report(ViewerGame *v);
#endif
