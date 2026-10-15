/* Source-backed terrain half of GetGroundHeight (0x80043400).
 * Reads original extracted area members, retaining PSX negative-up integer coordinates.
 * Solid shapes/collider volumes and contact selection are intentionally not implemented.
 */
#include "native_ground.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

struct AeGround {
    unsigned char *bytes;
    size_t size, cells, heights;
    uint32_t flags, width, depth;
};
static char message[256];
static int bad(const char *text) { snprintf(message,sizeof(message),"%s",text); return 0; }
const char *ae_ground_error(void) { return message; }
static uint32_t u32(const unsigned char *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t u16(const unsigned char *p) { return p[0]|(uint32_t)p[1]<<8; }
static int32_t s16(int32_t value) { uint32_t v=(uint32_t)value&65535; return v>=32768?(int32_t)v-65536:(int32_t)v; }
static int32_t height(const AeGround *g,uint32_t index) { return s16((int32_t)u16(g->bytes+g->heights+(size_t)index*2)); }
static int range(size_t size,size_t offset,size_t n,size_t stride) { return offset<=size && (!n || stride<=(size-offset)/n); }
void ae_ground_free(AeGround *g) { if (g) { free(g->bytes); free(g); } }
static int parse(AeGround *g) {
    size_t ah, grid, colors, tiles, count;
    if (g->size<0x508) return bad("area header truncated");
    ah=u32(g->bytes+0x4D0);
    if (!range(g->size,ah,1,0x54)) return bad("area header offset invalid");
    g->flags=u32(g->bytes+ah);
    if (g->flags&2) return 1; /* Disabled grids may contain junk. */
    grid=ah+(size_t)u32(g->bytes+ah+4)*4;
    if (!range(g->size,grid,1,8)) return bad("terrain grid offset invalid");
    g->width=u16(g->bytes+grid); g->depth=u16(g->bytes+grid+2);
    count=(size_t)g->width*g->depth;
    if (!g->width || !g->depth || g->width>256 || g->depth>256 || u32(g->bytes+grid+4)>256) return bad("terrain grid dimensions/colors invalid");
    g->cells=grid+8;
    if (!range(g->size,g->cells,count,4)) return bad("terrain cells truncated");
    colors=g->cells+count*4;
    if (!range(g->size,colors,u32(g->bytes+grid+4),4)) return bad("terrain colors truncated");
    tiles=colors+(size_t)u32(g->bytes+grid+4)*4;
    if (!range(g->size,tiles,count,8)) return bad("terrain tile bytes truncated");
    g->heights=tiles+count*8;
    if (!range(g->size,g->heights,(size_t)(g->width+1)*(g->depth+1)+count,2)) return bad("terrain heights truncated");
    return 1;
}
AeGround *ae_ground_load(const char *path) {
    FILE *file=fopen(path,"rb"); long length; AeGround *g;
    if (!file) { bad("cannot open original area member"); return NULL; }
    if (fseek(file,0,SEEK_END) || (length=ftell(file))<0x508 || length>16777216) {
        fclose(file); bad("original area file size invalid"); return NULL;
    }
    rewind(file); g=(AeGround *)calloc(1,sizeof(AeGround));
    if (!g) { fclose(file); bad("ground allocation failed"); return NULL; }
    g->size=(size_t)length; g->bytes=(unsigned char *)malloc(g->size);
    if (!g->bytes || fread(g->bytes,1,g->size,file)!=g->size) {
        fclose(file); ae_ground_free(g); bad("ground read/allocation failed"); return NULL;
    }
    fclose(file);
    if (!parse(g)) { ae_ground_free(g); return NULL; }
    return g;
}
void ae_ground_info(const AeGround *g,uint32_t out[3]) { out[0]=g->flags; out[1]=g->width; out[2]=g->depth; }
int ae_ground_entry(const AeGround *g,uint32_t entry,int32_t out[9]) {
    if (entry>=16) return bad("area entry index exceeds 16 fixed records");
    for (uint32_t i=0; i<9; ++i) out[i]=(int32_t)u32(g->bytes+entry*0x24+i*4);
    return 1;
}
int32_t ae_ground_raw_surface(const AeGround *g,int32_t x,int32_t z) {
    if(g->flags&2||x<-(int32_t)g->width*256||x>=(int32_t)g->width*256||z<-(int32_t)g->depth*256||z>=(int32_t)g->depth*256)return 0;
    size_t index=(size_t)((z+(int32_t)g->depth*256)/512)*g->width+(uint32_t)((x+(int32_t)g->width*256)/512);
    return (int32_t)(u32(g->bytes+g->cells+index*4)>>16);
}
int ae_ground_query(const AeGround *g,int32_t x,int32_t y,int32_t z,AeGroundHit *out) {
    int32_t cx,cz,ux,uz,a,b,c,d,center,dx=0,dz=0,h=0x4000,selector;
    uint32_t index, word;
    memset(out,0,sizeof(*out)); out->height=0x4000; out->selector=-1;
    if (g->flags&2) return 0;
    if (x<-(int32_t)g->width*256 || x>=(int32_t)g->width*256 ||
        z<-(int32_t)g->depth*256 || z>=(int32_t)g->depth*256) {
        out->height=0; return 0;
    }
    ux=x+(int32_t)g->width*256; uz=z+(int32_t)g->depth*256;
    cx=ux/512; cz=uz/512; ux&=511; uz&=511;
    index=(uint32_t)cz*g->width+(uint32_t)cx;
    word=u32(g->bytes+g->cells+(size_t)index*4);
    selector=(ux<uz?2:0)|(512-ux<uz?1:0);
    if (word&0x800) selector|=0x80|(int32_t)((word>>8)&0x70);
    out->selector=selector; out->surface=(int32_t)(word>>16);
    index=(uint32_t)cz*(g->width+1)+(uint32_t)cx;
    a=height(g,index); b=height(g,index+1); c=height(g,index+g->width+1); d=height(g,index+g->width+2);
    center=height(g,(g->width+1)*(g->depth+1)+(uint32_t)cz*g->width+(uint32_t)cx);
    switch (selector) {
    case 0: case 0x90: case 0xA0:
        dx=s16(b-a); dz=s16((center-dx/2-a)*2);
        h=s16(a+ux*dx/512+uz*dz/512); break;
    case 1: case 0xA1: case 0xB1:
        dz=s16(d-b); dx=s16((b+dz/2-center)*2);
        h=s16(b-dx*(512-ux)/512+uz*dz/512); break;
    case 2: case 0x92: case 0xC2:
        dz=s16(c-a); dx=s16((center-a-dz/2)*2);
        h=s16(a+ux*dx/512+uz*dz/512); break;
    case 3: case 0xB3: case 0xC3:
        dx=s16(d-c); dz=s16((c+dx/2-center)*2);
        h=s16(c+ux*dx/512-dz*(512-uz)/512); break;
    default: break;
    }
    out->height=h; out->slope_x=dx; out->slope_z=dz;
    if (y<h) out->surface&=0xFFE0;
    return h!=0x4000;
}
