#ifndef AE_NATIVE_UI_H
#define AE_NATIVE_UI_H
#include <stdint.h>
int ae_ui_load(const char *pack,const char *exe,uint32_t area);
int ae_ui_upload(void);
void ae_ui_free(void);
const char *ae_ui_title(void);
int ae_ui_hint(int sector);
void ae_ui_press(unsigned pressed);
int ae_ui_tick(void);
void ae_ui_draw(int width,int height);
int ae_ui_state(void);
#endif
