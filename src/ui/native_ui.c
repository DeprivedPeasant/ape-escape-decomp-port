/* Viewer presentation only. Claude's text/layout/message state machine linked unchanged. */
#include "native_ui.h"
#include "../text/tl_text.h"
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *bytes,*title_codes,*hints;
static const char *title="";
static size_t size;
static unsigned sectors,nt,pressed;
static GLuint *textures;
static const unsigned char *pages;
static TlFont font;
static TlMsgBox box;
static uint32_t frame;
static unsigned read32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static unsigned char *read_file(const char *path,size_t *n){
    FILE *f=fopen(path,"rb");long length;if(!f)return NULL;
    if(fseek(f,0,SEEK_END)||(length=ftell(f))<1||length>33554432){fclose(f);return NULL;}
    rewind(f);unsigned char *b=malloc((size_t)length);
    if(!b||fread(b,1,(size_t)length,f)!=(size_t)length){free(b);fclose(f);return NULL;}
    fclose(f);*n=(size_t)length;return b;
}
void ae_ui_free(void){
    if(textures)glDeleteTextures((int)nt,textures);
    free(textures);free(bytes);textures=NULL;bytes=NULL;title="";hints=title_codes=NULL;
    nt=sectors=pressed=frame=0;memset(&box,0,sizeof(box));
}
int ae_ui_load(const char *path,const char *exe_path,uint32_t area){
    size_t en;unsigned char *exe=read_file(exe_path,&en);if(!exe)return 0;
    int valid=tl_font_init(&font,exe,en)==0;free(exe);if(!valid)return 0;
    bytes=read_file(path,&size);if(!bytes||size<28||memcmp(bytes,"AEUI1\0\0\0",8)||read32(bytes+8)!=area)return 0;
    unsigned name=read32(bytes+12),codes=read32(bytes+16),hs=read32(bytes+20);nt=read32(bytes+24);
    if(!name||name>256||!codes||codes>256||!hs||hs%2048||hs>1048576||!nt||nt>64)return 0;
    size_t off=28+(size_t)name+codes+hs;
    if(off>size||nt>(size-off)/262152||off+(size_t)nt*262152!=size)return 0;
    if(bytes[28+name-1]||bytes[28+name+codes-1])return 0;
    title=(const char *)bytes+28;title_codes=bytes+28+name;hints=title_codes+codes;sectors=hs/2048;pages=bytes+off;
    for(unsigned i=0;i<sectors;i++)if(!memchr(hints+i*2048,0,2048))return 0;
    printf("UI name=%s hints=%u texture_pages=%u\n",title,sectors,nt);return 1;
}
int ae_ui_upload(void){
    if(!bytes)return 1;
    textures=calloc(nt,sizeof(*textures));if(!textures)return 0;glGenTextures((int)nt,textures);
    for(unsigned i=0;i<nt;i++){
        glBindTexture(GL_TEXTURE_2D,textures[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,pages+i*262152+8);
    }
    return glGetError()==GL_NO_ERROR;
}
const char *ae_ui_title(void){return title;}
int ae_ui_state(void){return box.state;}
int ae_ui_hint(int sector){
    if(!hints||sector<0||(unsigned)sector>=sectors||!hints[sector*2048])return 0;
    tl_msgbox_open(&box);tl_msgbox_set_text(&box,hints+sector*2048);return 1;
}
void ae_ui_press(unsigned p){pressed|=p;}
int ae_ui_tick(void){++frame;int sound=tl_msgbox_update(&box,pressed);pressed=0;return sound;}
static void rect(float x,float y,float w,float h,float u,float v,float tw,float th){
    glBegin(GL_QUADS);glTexCoord2f(u,v);glVertex2f(x,y);glTexCoord2f(u+tw,v);glVertex2f(x+w,y);
    glTexCoord2f(u+tw,v+th);glVertex2f(x+w,y+h);glTexCoord2f(u,v+th);glVertex2f(x,y+h);glEnd();
}
static void quads(TlQuad *q,int n){
    glEnable(GL_TEXTURE_2D);glColor4f(1,1,1,1);
    for(int j=0;j<n;j++)for(unsigned i=0;i<nt;i++)if(read32(pages+i*262152)==(unsigned)q[j].tpage&&read32(pages+i*262152+4)==(unsigned)q[j].clut){
        glBindTexture(GL_TEXTURE_2D,textures[i]);
        rect((float)q[j].x,(float)q[j].y,(float)q[j].w,(float)q[j].h,q[j].u/256.f,q[j].v/256.f,q[j].w/256.f,q[j].h/256.f);
        if(q[j].bold)rect((float)q[j].x+1,(float)q[j].y,(float)q[j].w,(float)q[j].h,q[j].u/256.f,q[j].v/256.f,q[j].w/256.f,q[j].h/256.f);
        break;
    }
}
void ae_ui_draw(int w,int h){
    if(!textures||!w||!h)return;
    glPushAttrib(GL_ALL_ATTRIB_BITS);glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);
    glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_COLOR_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,384,240,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    TlQuad q[1024];int n;
    if(title_codes[0]){n=tl_layout(&font,title_codes,16,8,496,frame,q,1024,NULL,NULL);quads(q,n);}
    if(box.state){
        int x=tl_msgbox_window_x(&box);
        glDisable(GL_TEXTURE_2D);glColor4f(.02f,.05f,.16f,.9f);rect((float)x,24,352,176,0,0,0,0);
        n=tl_msgbox_layout(&font,&box,frame,q,1024);quads(q,n);
    }
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
    glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_COLOR_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}
