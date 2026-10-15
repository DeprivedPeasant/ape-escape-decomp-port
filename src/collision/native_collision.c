/* Reconstructed from collision shape handlers 0x8004E33C..0x8004F438.
 * No original instructions or game lookup-table bytes are embedded here.
 */
#include "native_collision.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct AeCollision {
    unsigned char *bytes;
    size_t size,grid,shapes,colliders;
    uint32_t width,depth,shape_count,collider_count;
};
static char message[256];
static int16_t atan_table[1025];
static int math_ready;
const char *ae_collision_error(void) { return message; }
static int bad(const char *text) { snprintf(message,sizeof(message),"%s",text); return 0; }
static uint32_t u16(const unsigned char *p) { return p[0]|(uint32_t)p[1]<<8; }
static uint32_t u32(const unsigned char *p) { return u16(p)|u16(p+2)<<16; }
static int32_t read16(const unsigned char *p) { uint32_t v=u16(p); return v>=32768?(int32_t)v-65536:(int32_t)v; }
static int range(size_t size,size_t offset,size_t count,size_t stride) { return offset<=size&&(!count||stride<=(size-offset)/count); }
void ae_collision_free(AeCollision *c) { if(c) { free(c->bytes); free(c); } }
static int parse(AeCollision *c) {
    if(c->size<0x508) return bad("area header truncated");
    size_t ah=u32(c->bytes+0x4D0);
    if(!range(c->size,ah,1,0x54)) return bad("area header offset invalid");
    size_t co=ah+(size_t)u32(c->bytes+ah+16)*4;
    if(!range(c->size,co,1,20)) return bad("collision block offset invalid");
    size_t grid=co+u32(c->bytes+co),shapes=co+u32(c->bytes+co+4),boxes=co+u32(c->bytes+co+8);
    if(!range(c->size,grid,1,4)||!range(c->size,shapes,1,2)||!range(c->size,boxes,1,2)) return bad("collision table header truncated");
    c->width=u16(c->bytes+grid); c->depth=u16(c->bytes+grid+2);
    c->shape_count=u16(c->bytes+shapes); c->collider_count=u16(c->bytes+boxes);
    c->grid=grid+4; c->shapes=shapes+2; c->colliders=boxes+2;
    if(!c->width||!c->depth||c->width>256||c->depth>256||c->shape_count>32767||c->collider_count>32767) return bad("collision counts/dimensions invalid");
    if(!range(c->size,c->grid,(size_t)c->width*c->depth,2)||!range(c->size,c->shapes,c->shape_count,20)||!range(c->size,c->colliders,c->collider_count,56)) return bad("collision records truncated");
    for(uint32_t i=0;i<c->width*c->depth;++i) {
        int32_t index=read16(c->bytes+c->grid+i*2);
        if(index< -1||index>=(int32_t)c->shape_count) return bad("shape grid index invalid");
    }
    for(uint32_t i=0;i<c->shape_count;++i) {
        const unsigned char *s=c->bytes+c->shapes+i*20;
        int32_t next=read16(s+16);
        if(next< -1||next>=(int32_t)c->shape_count||s[18]>88) return bad("shape link/type invalid");
        uint32_t steps=0;
        while(next>=0) {
            if(next>=(int32_t)c->shape_count) return bad("shape column index invalid");
            if(++steps>c->shape_count) return bad("shape column cycle");
            next=read16(c->bytes+c->shapes+(size_t)next*20+16);
        }
    }
    for(uint32_t i=0;i<c->collider_count;++i) {
        uint32_t type=u16(c->bytes+c->colliders+(size_t)i*56+16),high=type&0xF000;
        if(high>0x9000||((high==0x2000||high==0x4000)&&(type&255)>88))return bad("collider geometry handler invalid");
    }
    return 1;
}
AeCollision *ae_collision_load(const char *path) {
    FILE *file=fopen(path,"rb"); long length;
    if(!file) { bad("cannot open original collision member"); return NULL; }
    if(fseek(file,0,SEEK_END)||(length=ftell(file))<0x508||length>16777216) { fclose(file); bad("collision file size invalid"); return NULL; }
    rewind(file); AeCollision *c=(AeCollision *)calloc(1,sizeof(*c));
    if(!c) { fclose(file); bad("collision allocation failed"); return NULL; }
    c->size=(size_t)length; c->bytes=(unsigned char *)malloc(c->size);
    if(!c->bytes||fread(c->bytes,1,c->size,file)!=c->size) { fclose(file); ae_collision_free(c); bad("collision read/allocation failed"); return NULL; }
    fclose(file);
    if(!parse(c)) { ae_collision_free(c); return NULL; }
    return c;
}
void ae_collision_info(const AeCollision *c,uint32_t out[4]) { out[0]=c->width; out[1]=c->depth; out[2]=c->shape_count; out[3]=c->collider_count; }
void ae_collision_arrays(const AeCollision *c,const int16_t **grid,const int16_t (**shapes)[10],const int16_t (**colliders)[28]) {
    *grid=(const int16_t *)(const void *)(c->bytes+c->grid);
    *shapes=(const int16_t (*)[10])(const void *)(c->bytes+c->shapes);
    *colliders=(const int16_t (*)[28])(const void *)(c->bytes+c->colliders);
}

static int32_t narrow(int32_t n) { uint32_t v=(uint32_t)n&65535; return v>=32768?(int32_t)v-65536:(int32_t)v; }
static uint32_t isqrt(uint32_t n) {
    uint32_t result=0, bit=1u<<30;
    while (bit>n) bit>>=2;
    while (bit) { if (n>=result+bit) { n-=result+bit; result=(result>>1)+bit; } else result>>=1; bit>>=2; }
    return result;
}
int ae_collision_math_load(const char *path) {
    math_ready=0;
    FILE *file=fopen(path,"rb"); unsigned char bytes[2050];
    if(!file) return bad("cannot open original EXE for angle lookup asset");
    if(fseek(file,0xB4430-0x10000+0x800,SEEK_SET)||fread(bytes,1,sizeof(bytes),file)!=sizeof(bytes)) { fclose(file); return bad("original angle lookup truncated"); }
    fclose(file);
    for(int i=0;i<1025;++i) {
        atan_table[i]=(int16_t)read16(bytes+i*2);
        if(atan_table[i]<0||atan_table[i]>512||(i&&atan_table[i]<atan_table[i-1])) return bad("angle lookup invalid");
    }
    if(atan_table[0]||atan_table[1024]!=512) return bad("angle lookup endpoints invalid");
    math_ready=1; return 1;
}
int32_t ae_collision_atan(int32_t y,int32_t x) {
    int sx=x<0,sy=y<0; uint32_t ax=sx?0u-(uint32_t)x:(uint32_t)x,ay=sy?0u-(uint32_t)y:(uint32_t)y;
    if(!ax&&!ay) return 0;
    uint32_t num=ay<ax?ay:ax,den=ay<ax?ax:ay;
    uint32_t index=(num&0x7FE00000u)?num/(den>>10):(num<<10)/den;
    int32_t angle=atan_table[index];
    if(ay>=ax) angle=1024-angle;
    if(sx) angle=2048-angle;
    return sy?-angle:angle;
}
void ae_collision_normalize(int32_t x,int32_t z,int32_t out[2]) {
    int32_t xx=narrow(x),zz=narrow(z); uint32_t n=(uint32_t)(xx*xx)+(uint32_t)(zz*zz);
    if(!n) { out[0]=out[1]=0; return; }
    uint32_t zeros=0,temp=n;
    while(!(temp&0x80000000u)) { ++zeros; temp<<=1; }
    zeros&=~1u;
    uint32_t index=zeros<24?n>>(24-zeros):n<<(zeros-24);
    int32_t factor=(int32_t)isqrt(1073741824u/index);
    int shift=(31-(int)zeros)/2;
    out[0]=narrow((xx*factor)>>shift);out[1]=narrow((zz*factor)>>shift);
}
int32_t ae_collision_sqrt(int32_t n,int32_t sdk_lookup) {
    if (n<=0) return 0;
    if (!sdk_lookup) {
        int bits=0; uint32_t temp=(uint32_t)n,root=0,remainder=0;
        while (temp) { ++bits; temp>>=1; }
        int pairs=(bits+1)/2;
        for(int i=1;i<=pairs+1;++i) {
            remainder=remainder*4+(((uint32_t)n>>(((pairs-i)*2)&31))&3);
            uint32_t trial=root*4+1;
            root*=2;
            if(trial<=remainder) { remainder-=trial; ++root; }
        }
        return (int32_t)((root>>1)+(root&1));
    }
    uint32_t value=(uint32_t)n, zeros=0, temp=value;
    while (!(temp&0x80000000u)) { ++zeros; temp<<=1; }
    zeros&=~1u;
    uint32_t index=zeros<24?value>>(24-zeros):value<<(zeros-24);
    /* All 192 original LUT entries equal floor(sqrt(index*262144)). */
    uint32_t table=isqrt(index*262144u);
    return (int32_t)((table<<((31-zeros)/2))>>12);
}
static int inside(int32_t x,int32_t z,int32_t hx,int32_t hz) { return x>=-hx&&x<=hx&&z>=-hz&&z<=hz; }
static int diagonal(int32_t x,int32_t z,int orientation) {
    int32_t px,pz,positive;
    if (orientation==1||orientation==3) { px=(x+z)/2; pz=(x+z)/2; positive=orientation==1?z>x:z<x; }
    else { px=(x-z)/2; pz=(z-x)/2; positive=orientation==0?z<-x:z>-x; }
    int32_t dx=x-px,dz=z-pz;
    return (positive?1:-1)*ae_collision_sqrt(dx*dx+dz*dz,1);
}
int ae_collision_shape(int32_t type,const int16_t half[3],const int32_t p[3],int32_t wall,AeShapeHit *out) {
    int32_t x=p[0],y=p[1],z=p[2],hx=half[0],h=half[1],hz=half[2],base=out->height;
    int mode=0,cx=0,cz=0;
    if (type<0||type>88) return -1;
    if ((type>=11&&type<=18)||(type>=85&&type<=88)) {
        if (!inside(x,z,hx,hz)) return 0;
    }
    out->height=0;
    if ((type>=3&&type<=6)||(type>=42&&type<=53)||(type>=74&&type<=77)) {
        if (wall!=1) return 0;
        if (type==3||type==5) mode=2;
        if (type==4||type==6) mode=1;
        if ((type>=42&&type<=45)||(type>=74&&type<=77)) {
            if (abs(x)<=64&&abs(z)<=64) mode=3;
            else if (type==42) mode=x<z?1:2;
            else if (type==43) mode=-x<z?2:1;
            else if (type==44) mode=x<z?2:1;
            else if (type==45) mode=-x<z?1:2;
            /* 74..77 leave caller t0 unchanged outside the center. The shape grid
             * contains none of these: these types use ColliderStripHeight instead. */
        }
        if (type>=46&&type<=49) {
            mode=(type&1)?1:2;
            if (type==46) cz=-256;
            if (type==47) cx=-256;
            if (type==48) cz=256;
            if (type==49) cx=256;
        }
        if (type>=50&&type<=53) {
            cx=type<=51?-256:256; cz=(type==50||type==53)?-256:256;
            if (abs(x-cx)<=64&&abs(z-cz)<=64) mode=3;
            else if (type==50) mode=x<z?1:2;
            else if (type==51) mode=-x<z?2:1;
            else if (type==52) mode=x<z?2:1;
            else mode=-x<z?1:2;
        }
        if (mode==1&&!(abs(x-cx)<=64&&abs(z)<=hz)) return 0;
        if (mode==2&&!(abs(z-cz)<=64&&abs(x)<=hx)) return 0;
        out->height=h; return 1;
    }
    if (type>=55&&type<=58) {
        if (wall!=1||!inside(x,z,hx,hz)||abs(diagonal(x,z,type-55))>64) return 0;
        out->height=h; return 1;
    }
    if (type==35||type==36||type==37||type==59||(type>=69&&type<=73)||(type>=82&&type<=84)) {
        if ((type>=70&&type<=73)||(type>=82&&type<=84)) return 0;
        if ((type==36||type==37)&&inside(x,z,hx,hz)) {
            int32_t r=type==36?hz:hx,d=type==36?z:x;
            int32_t curved=ae_collision_sqrt(r*r-d*d,1);
            if (curved>=y) { out->height=curved; if(type==36) out->slope_x=0; else out->slope_z=0; return 1; }
        }
        if (ae_collision_sqrt(x*x+z*z,0)>=hx) return 0;
        out->height=h; out->slope_x=out->slope_z=0; return 1;
    }
    if (type>=60&&type<=63) {
        if(type==60) cz=-hz;
        if(type==61) cx=-hx;
        if(type==62) cz=hz;
        if(type==63) cx=hx;
    }
    if (!inside(x-cx,z-cz,hx,hz)) return 0;
    if (type>=38&&type<=41&&diagonal(x,z,type-38)>0) return 0;
    if (type>=64&&type<=67) {
        int32_t d=type==64?z:type==65?-x:type==66?-z:x;
        if (y>=base-256-d) return 0;
        out->height=h; out->extra=d+256; out->slope_x=out->slope_z=0; return 1;
    }
    if (type>=7&&type<=34) {
        int direction,scale,offset;
        if(type<=10) { direction=type-7; scale=1; offset=256; }
        else if(type<=18) { direction=(type-11)/2; scale=2; offset=((type-11)%2)*256+128; }
        else { direction=(type-19)/4; scale=4; offset=((type-19)%4)*128+64; }
        /* Original 45 degree formula is 256 +/- coordinate; half/quarter offsets
         * start at 128/64, preserving signed division towards zero. */
        int32_t d=direction==0?z:direction==1?-x:direction==2?-z:x;
        out->height=offset+d/scale;
        out->slope_x=(direction==1?512/scale:direction==3?-512/scale:0);
        out->slope_z=(direction==0?-512/scale:direction==2?512/scale:0);
        return 1;
    }
    if(type>=85&&type<=88&&hx>64&&hz>64) {
        int direction=type-85,extent=(direction&1)?hx:hz;
        int32_t d=direction==0?z:direction==1?-x:direction==2?-z:x;
        out->height=h/2+(h/2)*d/extent;
        out->slope_x=narrow(direction==1?h*512/(hx*2):direction==3?-h*512/(hx*2):0);
        out->slope_z=narrow(direction==0?-h*512/(hz*2):direction==2?h*512/(hz*2):0);
        return 1;
    }
    if ((!hx||!hz)&&!(type>=38&&type<=41)&&!(type>=85&&type<=88)) return 0;
    out->height=h; out->slope_x=out->slope_z=0; return 1;
}

int ae_collision_grid_height(const AeCollision *c,int32_t x,int32_t y,int32_t z,int32_t wall,AeSolidHit *out) {
    memset(out,0,sizeof(*out)); out->height=32767; out->index=-1;
    int32_t hx=(int32_t)c->width*256,hz=(int32_t)c->depth*256;
    if(x<=-hx||x>=hx||z<=-hz||z>=hz||y>0) return 0;
    int32_t gx=(x+hx)/512,gz=(z+hz)/512,band=-y/512;
    int32_t p[3]={x+hx-gx*512-256,y,z+hz-gz*512-256};
    int32_t index=read16(c->bytes+c->grid+((size_t)gz*c->width+gx)*2),selected=-1,selected_band=-1;
    AeShapeHit hit={0,0,0,0}; int32_t selected_height=512;
    uint32_t count=0; int continuation=0;
    while(index>=0) {
        if(++count>c->shape_count) return bad("shape traversal exceeded count");
        const unsigned char *s=c->bytes+c->shapes+(size_t)index*20;
        int32_t sb=read16(s+2),next=read16(s+16);
        if(band<sb&&!continuation) break;
        continuation=0;
        int16_t half[3]={(int16_t)read16(s+6),(int16_t)read16(s+8),(int16_t)read16(s+10)};
        hit.height=-sb*512;
        int status=ae_collision_shape(s[18],half,p,wall,&hit);
        if(status) {
            selected=index; selected_band=sb; selected_height=hit.height;
            if(next>=0&&hit.height>256&&sb+(hit.height>511?hit.height/512:1)==read16(c->bytes+c->shapes+(size_t)next*20+2)) { index=next; continuation=1; continue; }
        } else if(band<=sb) break;
        index=next;
    }
    if(selected<0) return 0;
    const unsigned char *s=c->bytes+c->shapes+(size_t)selected*20;
    if(selected_band>=0)out->height=-selected_band*512-selected_height;
    out->index=selected;
    out->type=(int32_t)u16(s+18); out->slope_x=hit.slope_x; out->slope_z=hit.slope_z;
    out->extra_a=read16(s+12); out->extra_b=read16(s+14);
    return out->height!=32767;
}

static int height_overlap(int32_t y,int32_t body,int32_t top,int32_t height) {
    if(y-96<=top-height||top<=y-96) {
        if(body<96) return 0;
        int32_t mid=y-body/2;
        if((mid<=top-height||top<=mid)&&(y-body<=top-height||top<=y-body)) {
            if(y-96<=top-height||top-height<=y-body) {
                mid=top-height/2;
                if(y-96<=mid||mid<=y-body) return y-96>top&&top>y-body;
            }
        }
    }
    return 1;
}
int ae_collision_triangle_probe(const int16_t b[6],int32_t type,int32_t radius,int32_t body,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    int32_t dx=p[0]-b[0],dz=p[2]-b[2],hx=narrow(b[3]+radius),hz=narrow(b[5]+radius);
    if(!inside(dx,dz,hx,hz)||!height_overlap(p[1],body,b[1],b[4])||!b[3])return 0;
    int orientation=(type&0xF00)>>10,px,pz,positive;
    if(orientation==1||orientation==3) {
        px=(p[0]+p[2]-(b[2]-b[0]))/2;pz=(p[0]+p[2]+(b[2]-b[0]))/2;
        positive=orientation==1?p[2]>p[0]+b[2]-b[0]:p[2]<p[0]+b[2]-b[0];
    } else {
        px=(b[0]+b[2]-(p[2]-p[0]))/2;pz=(b[0]+b[2]+p[2]-p[0])/2;
        positive=orientation==0?p[2]<b[0]-p[0]+b[2]:p[2]>b[0]-p[0]+b[2];
    }
    int32_t distance=ae_collision_sqrt((p[0]-px)*(p[0]-px)+(p[2]-pz)*(p[2]-pz),1)*(positive?1:-1);
    if(distance>radius+32||abs(px-b[0])+abs(pz-b[2])>b[3]+b[5])return 0;
    int sector=(dx*b[5]/b[3]<dz?2:0)|(-dx*b[5]/b[3]<dz?1:0);
    if((orientation==0&&(sector==0||sector==2))||(orientation==1&&sector>=2)||
       (orientation==2&&(sector==1||sector==3))||(orientation==3&&sector<2)||!distance)sector=4;
    if(sector==0||sector==3)out[2]=-dz+(p[2]<b[2]?-hz:hz);
    else if(sector==1||sector==2)out[0]=-dx+(p[0]<b[0]?-hx:hx);
    else {
        int sx=orientation<2?-1:1,sz=(orientation==0||orientation==3)?-1:1;
        if(!distance) { out[0]=sx*radius*71/100;out[2]=sz*body*71/100; }
        else {
            int32_t ax=abs(px-p[0]),az=abs(pz-p[2]);
            out[0]=sx*(ax*(radius+32)/distance+(distance<0?ax:-ax));
            out[2]=sz*(az*(radius+32)/distance+(distance<0?az:-az));
        }
    }
    if(p[1]-body<b[1]&&b[1]-b[4]<p[1]&&inside(dx,dz,b[3],b[5])&&
       inside(old[0]-b[0],old[2]-b[2],b[3],b[5])&&distance<0)out[3]=b[1]-(p[1]-body);
    return 1;
}
static int32_t wrap32(int64_t n) { uint32_t u=(uint32_t)n;return u>=0x80000000u?(int32_t)((int64_t)u-4294967296LL):(int32_t)u; }
static int halfpipe_response(const int16_t b[6],int type,int radius,int body,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    int axis=(type&255)==36||(type&255)==83?2:0;
    int32_t d=p[axis]-b[axis],r=b[axis+3];
    if(((type&255)==36||(type&255)==37)&&abs(d)>r)return 0;
    int32_t arc=ae_collision_sqrt(wrap32((int64_t)r*r-(int64_t)d*d),1);
    if(!height_overlap(p[1],body,b[1]+arc,arc*2)||!b[3])return 0;
    int32_t dx=p[0]-b[0],dz=p[2]-b[2];int sector;
    if(abs(dx)>b[3]&&abs(dz)>b[5]) {
        int32_t x=dx+(dx>b[3]?-b[3]:b[3]),z=dz+(dz>b[5]?-b[5]:b[5]);
        sector=(x<z?2:0)|(-x<z?1:0);
    } else sector=(dx*b[5]/b[3]<dz?2:0)|(-dx*b[5]/b[3]<dz?1:0);
    if(sector==0||sector==3)out[2]=-dz+(dz<0?-narrow(b[5]+radius):narrow(b[5]+radius));
    else out[0]=-dx+(dx<0?-narrow(b[3]+radius):narrow(b[3]+radius));
    if(p[1]-body<b[1]+arc&&p[1]>b[1]-arc&&inside(dx,dz,b[3],b[5])&&inside(old[0]-b[0],old[2]-b[2],b[3],b[5])) {
        out[3]=b[1]+arc-(p[1]-body);out[4]=out[5]=arc;
    }
    return 1;
}
int ae_collision_radial_probe(const int16_t b[6],int32_t type,int32_t radius,int32_t body,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    if((type&255)==36||(type&255)==37) {
        if(!inside(p[0]-b[0],p[2]-b[2],narrow(b[3]+radius),narrow(b[5]+radius)))return 0;
        return halfpipe_response(b,type,radius,body,p,old,out);
    }
    int32_t dx=p[0]-b[0],dz=p[2]-b[2],distance=ae_collision_sqrt(dx*dx+dz*dz,1),r=radius+b[3];
    if(!distance)distance=1;
    if(distance>=r||!height_overlap(p[1],body,b[1],b[4]))return 0;
    int32_t xx,zz;
    if(r<4097||(type&255)==59||(type&255)==89) { xx=wrap32((int64_t)r*dx*100)/distance/100;zz=wrap32((int64_t)r*dz*100)/distance/100; }
    else { xx=wrap32((int64_t)r*dx)/distance;zz=wrap32((int64_t)r*dz)/distance; }
    out[0]=xx-dx;out[2]=zz-dz;
    if(p[1]-body<b[1]&&b[1]-b[4]<p[1]&&inside(old[0]-b[0],old[2]-b[2],b[3],b[5])&&distance<b[3])out[3]=b[1]-(p[1]-body);
    return (type&255)==59||(type&255)==89?2:1;
}
static int ramp_probe(const int16_t b[6],int32_t type,int32_t radius,int32_t body,const int32_t p[3],const int32_t old[3],int32_t out[6],int ray_alias) {
    int32_t dx=p[0]-b[0],dz=p[2]-b[2],hx=narrow(b[3]+radius),hz=narrow(b[5]+radius);
    if(!inside(dx,dz,hx,hz)||!b[3])return 0;
    int32_t pp[3]={p[0],p[1],p[2]};
    if(pp[0]>=b[0]+b[3])pp[0]=b[0]+b[3]-1;else if(pp[0]<=b[0]-b[3])pp[0]=b[0]-b[3]+1;
    if(pp[2]>=b[2]+b[5])pp[2]=b[2]+b[5]-1;else if(pp[2]<=b[2]-b[5])pp[2]=b[2]-b[5]+1;
    int kind=type&255;int32_t local[3]={pp[0]-b[0],pp[1],pp[2]-b[2]};
    AeShapeHit hit={b[1],0,0,0};if(ae_collision_shape(kind,b+3,local,0,&hit)<=0)return 0;
    int32_t top=b[1],height=hit.height;
    if(kind>=64&&kind<=67) { top-=hit.extra;height-=hit.extra; }
    /* CollisionRayTest passes 1F800124 as output, aliasing this helper's
     * top/height scratch. ResolveProbes and direct helper callers do not. */
    if(ray_alias){out[0]=top;out[1]=height;}
    if(!height_overlap(p[1],body,top,height))return 0;
    int sector=(dx*b[5]/b[3]<dz?2:0)|(-dx*b[5]/b[3]<dz?1:0);
    if(sector==0||sector==3)out[2]=-dz+(p[2]<b[2]?-hz:hz);
    else out[0]=-dx+(p[0]<b[0]?-hx:hx);
    if(ray_alias){top=out[0];height=out[1];}
    int overlap;
    if(kind>=64&&kind<=67)overlap=p[1]-body<top&&top-height<p[1];
    else overlap=p[1]-body<top&&top<p[1]&&p[1]-body>top-height;
    if(overlap&&inside(dx,dz,b[3],b[5])&&inside(old[0]-b[0],old[2]-b[2],b[3],b[5]))out[3]=top-(p[1]-body);
    int32_t edge=(kind>=7&&kind<=10)?top:top-height;
    if(p[1]-96<edge) {
        if(kind>=64&&kind<=67)return 0;
        int orientation=(type&0xF00)>>10;
        if(sector==(orientation==2?3:orientation==3?2:orientation==1?1:0))return 0;
    }
    return 1;
}
int ae_collision_ramp_probe(const int16_t b[6],int32_t type,int32_t radius,int32_t body,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    return ramp_probe(b,type,radius,body,p,old,out,0);
}
int ae_collision_wall_band_probe(const int16_t b[6],int32_t type,int32_t radius,int32_t body,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    (void)old;
    int32_t dx=p[0]-b[0],dz=p[2]-b[2],r=radius+12;
    int high=type&0xF000,o=(type&0xF00)>>10,axis=0,cx=0,cz=0;
    if(!height_overlap(p[1],body,b[1],b[4]))return 0;
    if(high==0x8000) {
        if(!inside(dx,dz,b[3]+radius,b[5]+radius))return 0;
        int32_t px,pz,positive;
        if(o==1||o==3) { px=(p[0]+p[2]-(b[2]-b[0]))/2;pz=(p[0]+p[2]+(b[2]-b[0]))/2;positive=o==1?dz>dx:dz<dx; }
        else { px=(b[0]+b[2]-(p[2]-p[0]))/2;pz=(b[0]+b[2]+p[2]-p[0])/2;positive=o==0?dz<-dx:dz>-dx; }
        int32_t ax=abs(px-p[0]),az=abs(pz-p[2]),distance=ae_collision_sqrt(ax*ax+az*az,1)*(positive?1:-1);
        if(abs(distance)>r)return 0;
        if(!distance)distance=1;
        int sx=o<2?-1:1,sz=(o==0||o==3)?-1:1;
        out[0]=sx*(ax*r/distance+(distance<0?ax:-ax));
        out[2]=sz*(az*r/distance+(distance<0?az:-az));
        return 1;
    }
    if(high==0x1000)axis=(type&0xC00)?1:2;
    else if(high==0x6000) {
        if(o==0) { axis=2;cz=-256; }
        if(o==1) { axis=1;cx=-256; }
        if(o==2) { axis=2;cz=256; }
        if(o==3) { axis=1;cx=256; }
    } else if(high==0x5000||high==0x7000) {
        if(o==0)axis=dx<dz?1:2;
        if(o==1)axis=-dx<dz?2:1;
        if(o==2)axis=dx<dz?2:1;
        if(o==3)axis=-dx<dz?1:2;
        if(high==0x7000) { cx=o<2?-256:256;cz=(o==0||o==3)?-256:256; }
    } else return 0;
    if(axis==1) {
        if(abs(dx-cx)>r||abs(dz)>b[5]+(high==0x6000||high==0x7000?r:0))return 0;
        out[0]=cx+(dx<=cx?-r:r)-dx;return 1;
    }
    if(axis==2) {
        if(abs(dz-cz)>r||abs(dx)>b[3]+(high==0x6000||high==0x7000?r:0))return 0;
        out[2]=cz+(dz<=cz?-r:r)-dz;return 2;
    }
    return 0;
}
int ae_collision_ring_probe(const int16_t b[6],int32_t type,int32_t radius,int32_t body,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    (void)radius;(void)old;
    int kind=type&255;int32_t dx=p[0]-b[0],dz=p[2]-b[2],distance=ae_collision_sqrt(dx*dx+dz*dz,1);
    if(!distance)distance=1;
    if(p[1]<b[1]-b[4]+64||p[1]-body>b[1])return 0;
    int32_t width=b[3]*75/1000;if(width<64)width=64;
    if(abs(b[3]-distance)>width)return 0;
    if((kind==78&&p[2]>=b[2])||(kind==79&&p[0]>=b[0])||(kind==80&&p[2]<=b[2])||(kind==81&&p[0]<=b[0]))return 0;
    int32_t r=b[3]+(distance<b[3]?-width:width);
    out[0]=wrap32((int64_t)r*dx*100)/distance/100-dx;
    out[2]=wrap32((int64_t)r*dz*100)/distance/100-dz;
    return 1;
}
int ae_collision_box_probe(int16_t box[6],int32_t type,int32_t radius,int32_t body_height,const int32_t p[3],const int32_t previous[3],int32_t out[6]) {
    type&=255;
    if(type==60) box[2]=(int16_t)narrow(box[2]-box[5]);
    if(type==61) box[0]=(int16_t)narrow(box[0]-box[3]);
    if(type==62) box[2]=(int16_t)narrow(box[2]+box[5]);
    if(type==63) box[0]=(int16_t)narrow(box[0]+box[3]);
    if(!box[3]) box[3]=1;
    int32_t hx=narrow(box[3]+radius),hz=narrow(box[5]+radius);
    int32_t dx=p[0]-box[0],dz=p[2]-box[2];
    if(dx>hx||dx< -hx||dz>hz||dz< -hz||!height_overlap(p[1],body_height,box[1],box[4])) return 0;
    int sector;
    if(abs(dx)>box[3]&&abs(dz)>box[5]) {
        int32_t xx=dx+(dx>box[3]?-box[3]:box[3]);
        int32_t zz=dz+(dz>box[5]?-box[5]:box[5]);
        sector=((xx<zz)?2:0)|(-xx<zz?1:0);
    } else sector=((dx*box[5]/box[3]<dz)?2:0)|(-dx*box[5]/box[3]<dz?1:0);
    if(sector==0||sector==3) out[2]=-dz+(p[2]<box[2]?-hz:hz);
    else out[0]=-dx+(p[0]<box[0]?-hx:hx);
    if(p[1]-body_height<box[1]&&box[1]-box[4]<p[1]&&
       inside(dx,dz,box[3],box[5])&&inside(previous[0]-box[0],previous[2]-box[2],box[3],box[5])) out[3]=box[1]-(p[1]-body_height);
    return 1;
}

static int32_t lengthxz(int32_t x,int32_t z) { x=narrow(x);z=narrow(z);return ae_collision_sqrt((int32_t)((uint32_t)(x*x)+(uint32_t)(z*z)),0); }
static int collider_active(uint32_t type,int32_t solid,int32_t wall) {
    if(wall==1) return 1;
    uint32_t high=type&0xF000,low=type&255;
    int special=high==0x1000||high==0x5000||high==0x6000||high==0x7000||high==0x8000||
                (high==0x3000&&((low>=78&&low<=81)||low==54));
    if(wall==0&&!special) return 1;
    return special?solid!=0:solid==0;
}
static int32_t curved_slope(const int16_t b[28],const int32_t p[3],int32_t radius,int32_t body,AeShapeHit *out) {
    int kind=b[8]&255;
    if(b[1]<p[1]-body) return -1;
    int32_t cx=b[0]+((kind==70||kind==71)?b[3]:-b[3]);
    int32_t cz=b[2]+((kind==70||kind==73)?b[5]:-b[5]);
    int32_t r=b[3]+b[18]; if(!r) r=1;
    if(abs(r-lengthxz(cx-p[0],cz-p[2]))>b[3]-b[18]+radius) return -2;
    int32_t x=p[0]-b[0],z=p[2]-b[2];
    if(x< -b[3]-((kind==70||kind==71)?radius:0)||x>b[3]+((kind==72||kind==73)?radius:0)||
       z< -b[5]-((kind==70||kind==73)?radius:0)||z>b[5]+((kind==71||kind==72)?radius:0))return -3;
    if(!math_ready) { bad("curved slope requires original angle lookup asset"); return -4; }
    int32_t n[2]; ae_collision_normalize(p[0]-cx,p[2]-cz,n);
    int32_t angle=ae_collision_atan(n[1],n[0]);if(angle<0) angle+=4096;
    int start=kind==70?2048:kind==71?1024:kind==72?0:3072,end=(start+1024)&4095;
    if(!(angle&1023)) { if(angle==start) ++angle;else if(angle==end) --angle;if(angle<0)angle+=4096; }
    int32_t h=b[19]+(b[4]-b[19])*(angle%1024)/1024;
    ae_collision_normalize(p[2]-cz,p[0]-cx,n);
    out->slope_x=narrow(((b[4]-b[19])*n[0]/4096)*204800/(r*628));
    out->slope_z=narrow(((b[19]-b[4])*n[1]/4096)*204800/(r*628));
    return h;
}
static int32_t collider_height(const int16_t b[28],const int32_t p[3],int wall,AeShapeHit *out) {
    uint32_t type=(uint16_t)b[8],high=type&0xF000,kind=type&255;
    int32_t x=p[0]-b[0],y=p[1]-b[1],z=p[2]-b[2];
    if(!collider_active(type,(uint16_t)b[10]&0x8000,wall)) return 0;
    if(high==0x3000||high==0x9000) {
        if(kind==54||(kind>=78&&kind<=81)) return 0; /* Original early branch rejects these. */
        if(kind==68) {
            int32_t distance=lengthxz(x,z),thickness=b[3]*75/1000;
            if(thickness<64) thickness=64;
            if(distance>=b[3]+thickness/2||distance<=b[3]-thickness/2) return 0;
            out->slope_x=out->slope_z=0;return b[4];
        }
        if(kind>=70&&kind<=73) { int32_t h=curved_slope(b,p,0,0,out);return h<0?0:h; }
        if(kind==36||kind==37) {
            if(!inside(x,z,b[3],b[5])) return 0;
            int32_t r=kind==36?b[5]:b[3],d=kind==36?z:x;
            int32_t h=ae_collision_sqrt(r*r-d*d,1);
            if(h<y) return 0;
            if(kind==36)out->slope_x=0;else out->slope_z=0;
            return h;
        }
        if(kind==83||kind==84) {
            int axis=kind==83?2:0;int32_t d=axis==2?z:x,r=axis==2?b[5]:b[3];
            if(abs(d)>r+256||(axis==2?abs(x)>b[3]:abs(z)>b[5])) return 0;
            int32_t distance=lengthxz(y,d),h=0;
            if(y<0) {
                int32_t rr=distance>r-256?r+256:r-256;
                h=ae_collision_sqrt(rr*rr-d*d,1)*(distance>r-256?1:-1);
            } else if(distance<=r+256) {
                int32_t rr=distance>=r-192?r+256:r-256;
                h=-ae_collision_sqrt(rr*rr-d*d,1);
            }
            int32_t slope=0;
            if(h<0&&r!=256) {
                slope=(r-(-h+256))*3072/(r-256);
                if(d>0)slope=-slope;
                if(abs(slope)<128)slope=0;
            }
            out->slope_x=axis==0?slope:0;out->slope_z=axis==2?slope:0;return h;
        }
        if(kind==82||lengthxz(x,z)>=b[3]||y>0) return 0;
        out->slope_x=out->slope_z=0;return b[4];
    }
    if(high==0x1000) {
        if(kind<74||kind>77) return 0;
        int32_t d=(kind==74||kind==76)?x:z;
        int32_t side=(kind==74||kind==76)?z:x;
        if(abs(d)>b[3]||abs(side)>72||!b[3]) return 0;
        int reverse=(kind==74||kind==76)?b[22]==0:b[22]==90;
        int32_t offset=reverse?b[3]-d:d+b[3];
        int32_t h=b[5]*2+(b[4]-b[5]*2)*offset/(b[3]*2);
        if(h)out->slope_x=out->slope_z=0;
        return h;
    }
    if(!inside(x,z,b[3],b[5])||y>0) return 0;
    if(high==0x2000||high==0x4000) {
        int32_t local[3]={x,p[1],z};AeShapeHit hit=*out;hit.height=b[1];
        int status=ae_collision_shape((int32_t)kind,b+3,local,wall,&hit);
        out->slope_x=hit.slope_x;out->slope_z=hit.slope_z;
        return status>0?hit.height:0;
    }
    out->slope_x=out->slope_z=0;return b[4];
}
int ae_collision_collider(const int16_t b[28],const int32_t p[3],int32_t wall,AeShapeHit *out) {
    int32_t h=collider_height(b,p,wall,out);out->height=h;return h;
}
int ae_collision_trench_probe(const int16_t b[28],int32_t type,int32_t radius,int32_t body,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    int axis=(type&255)==83?2:0;
    if(!inside(p[0]-b[0],p[2]-b[2],narrow(b[3]+radius+(axis==0?256:0)),narrow(b[5]+radius+(axis==2?256:0))))return 0;
    int32_t d=p[axis]-b[axis],y=p[1]-b[1],r=b[axis+3];
    AeShapeHit foot={0,0,0,0},head=foot;int32_t hp[3]={p[0],p[1]-body,p[2]};
    int32_t fh=collider_height(b,p,1,&foot),hh=collider_height(b,hp,1,&head);
    int32_t hd=ae_collision_sqrt(wrap32((int64_t)(y-body)*(y-body)+(int64_t)d*d),1);
    int32_t fd=ae_collision_sqrt(wrap32((int64_t)y*y+(int64_t)d*d),1);
    if(hd<=r-192&&fd<=r-192) {
        int32_t edge=ae_collision_sqrt(wrap32((int64_t)(r-256)*(r-256)-(int64_t)body*body),1)-64;
        if(fh==hh&&-edge<d&&d<edge)return 0;
        if(d< -edge)out[axis]=-edge-d;else if(d>edge)out[axis]=edge-d;
        if(fh<0&&hh>0) { out[3]=b[1]+fh-(p[1]-body);out[4]=fh;out[5]=abs(fh); }
        return 1;
    }
    int16_t box[6];memcpy(box,b,sizeof(box));box[axis+3]=(int16_t)narrow(box[axis+3]+256);
    return halfpipe_response(box,type,radius,body,p,old,out);
}
int ae_collision_strip_probe(const int16_t b[28],int32_t type,int32_t radius,int32_t body,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    (void)type;(void)old;AeShapeHit hit={0,0,0,0};
    int32_t h=ae_collision_collider(b,p,1,&hit);
    if(!h||!height_overlap(p[1],body,b[1],h))return 0;
    int kind=b[8]&255;
    if(kind==74||kind==76) { int32_t dz=p[2]-b[2];out[2]=(dz<1?-(radius+12):radius+12)-dz;return 2; }
    int32_t dx=p[0]-b[0];out[0]=(dx<1?-(radius+12):radius+12)-dx;return 1;
}
int ae_collision_curved_probe(const int16_t b[28],int32_t type,int32_t radius,int32_t body,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    (void)type;(void)old;AeShapeHit hit={0,0,0,0};
    int32_t h=curved_slope(b,p,radius,body,&hit);
    if(h<0||p[1]<b[1]-h+64)return 0;
    int kind=b[8]&255;
    int32_t cx=b[0]+((kind==70||kind==71)?b[3]:-b[3]),cz=b[2]+((kind==70||kind==73)?b[5]:-b[5]);
    int32_t dx=p[0]-cx,dz=p[2]-cz,r=b[3]+b[18],thickness=b[3]-b[18];
    int32_t distance=ae_collision_sqrt(dx*dx+dz*dz,1);if(!distance)distance=1;
    if(abs(r-distance)<=thickness) {
        int minimum=b[4]<b[19]?b[4]:b[19];
        if(b[1]<=p[1]-body||p[1]<=b[1]-minimum)return 0;
        out[3]=b[1]-(p[1]-body);return 1;
    }
    int inner=distance<r,new_radius=inner?r-thickness-radius:r+thickness+radius;
    int32_t xx=wrap32((int64_t)new_radius*dx)/distance,zz=wrap32((int64_t)new_radius*dz)/distance;
    if(xx==dx&&zz==dz) {
        xx+=inner?(cx<p[0]?-1:1):(cx<p[0]?1:-1);
        zz+=inner?(p[2]<=cz?1:-1):(cz<p[2]?1:-1);
    }
    out[0]=xx-dx;out[2]=zz-dz;return 1;
}
static uint32_t quadrant(int32_t x,int32_t z) { x=narrow(x);z=narrow(z);return x<0?(z<0?1u:0x1000u):(z<0?0x10u:0x100u); }
static void collider_record(const AeCollision *c,uint32_t index,int16_t out[28]) {
    const unsigned char *bytes=c->bytes+c->colliders+(size_t)index*56;
    for(int i=0;i<28;++i)out[i]=(int16_t)read16(bytes+i*2);
}
int ae_collision_record(const AeCollision *c,uint32_t index,int16_t out[28]) {
    if(!c||!out||index>=c->collider_count)return 0;
    collider_record(c,index,out);return 1;
}
/* GetSurfaceType 800797F8: descending first matching type-9 volume.
 * Y top narrowing is deliberate (s16 original scratch/return words).
 */
uint16_t ae_collision_surface_type(const AeCollision *c,const int32_t pos[3],int16_t *water_y) {
    for(int i=(int)c->collider_count-1;i>=0;--i) {
        int16_t b[28];ae_collision_record(c,(uint32_t)i,b);
        if((b[9]&255)!=9||pos[0]<b[0]-b[3]||pos[0]>b[0]+b[3]||
           pos[2]<b[2]-b[5]||pos[2]>b[2]+b[5]||pos[1]<b[1]-b[4]||pos[1]>b[1])continue;
        int32_t top=(int16_t)(b[1]-b[4]);uint16_t depth=2;
        if(pos[1]<top||top<=pos[1]-64){depth=0;if(top<=pos[1]-64)depth=(uint16_t)(pos[1]-192<top);}
        *water_y=(int16_t)top;return (uint16_t)(((b[9]&0xF00)<<4)|depth);
    }
    *water_y=0;return 0;
}
static uint32_t collider_quadrants(const int16_t b[28]) {
    return quadrant(b[0]-b[3],b[2]-b[5])|quadrant(b[0]+b[3],b[2]-b[5])|
           quadrant(b[0]-b[3],b[2]+b[5])|quadrant(b[0]+b[3],b[2]+b[5]);
}
int ae_collision_solid_height(const AeCollision *c,int32_t x,int32_t y,int32_t z,int32_t wall,int32_t ignore,int32_t skip_water,AeSolidHit *out) {
    AeSolidHit grid; ae_collision_grid_height(c,x,y,z,wall,&grid);*out=grid;
    int32_t hx=(int32_t)c->width*256,hz=(int32_t)c->depth*256;
    if(x<=-hx||x>=hx||z<=-hz||z>=hz) return 0;
    int32_t point[3]={x,y,z},best=32767,index=-1,sx=0,sz=0;uint32_t mask=quadrant(x,z);
    if(grid.height<point[1])point[1]=grid.height;
    AeShapeHit hit={0,0,grid.slope_x,grid.slope_z};uint32_t passes=0;
    for(;;) {
        int32_t previous=best;
        for(int32_t i=(int32_t)c->collider_count-1;i>=0;--i) {
            int16_t b[28];collider_record(c,(uint32_t)i,b);
            if(i==ignore||((uint16_t)b[12]&0x8000)||!(collider_quadrants(b)&mask))continue;
            int32_t height=ae_collision_collider(b,point,wall,&hit);
            if(!height)continue;
            int32_t floor=b[1]-height;
            if(floor>=best)continue;
            best=floor;index=i;sx=hit.slope_x;sz=hit.slope_z;
            if(floor<point[1]) { point[1]=floor-32;break; }
        }
        if(best==previous)break;
        if(++passes>c->collider_count+1) { out->height=32767;return bad("collider selection failed to converge"); }
    }
    if(!skip_water) {
        for(int32_t i=(int32_t)c->collider_count-1;i>=0;--i) {
            int16_t b[28];collider_record(c,(uint32_t)i,b);
            if((b[9]&255)!=9||!inside(x-b[0],z-b[2],b[3],b[5])||point[1]>b[1]||point[1]<b[1]-b[4])continue;
            int kind=b[9]&0xF00;
            if((kind==0x400||kind==0x500)&&b[1]-b[4]<best) { best=b[1]-b[4];index=i;sx=sz=0; }
            break;
        }
    }
    if(best<grid.height) {
        int16_t b[28];if(index<0)return 0;collider_record(c,(uint32_t)index,b);
        out->height=best;out->kind=1;out->index=index;out->type=(uint16_t)b[8];out->surface=(uint16_t)b[9];
        out->extra_a=b[10];out->extra_b=b[11];out->slope_x=sx;out->slope_z=sz;
    }
    return out->height!=32767;
}
int ae_collision_ground_height(const AeCollision *c,const AeGround *ground,int32_t x,int32_t y,int32_t z,int32_t wall,int32_t ignore,int32_t skip,AeGroundHit *hit,AeSolidHit *contact) {
    ae_ground_query(ground,x,y,z,hit);
    memset(contact,0,sizeof(*contact));contact->height=32767;contact->kind=-1;
    if(hit->height==0&&hit->selector<0)return 0; /* Original terrain out-of-bounds early return. */
    ae_collision_solid_height(c,x,y<hit->height?y:hit->height,z,wall,ignore,skip,contact);
    if(contact->height==32767&&contact->index<0) { memset(contact,0,sizeof(*contact));contact->height=32767;contact->kind=-1; }
    int32_t solid=narrow(contact->height);
    hit->surface=contact->kind<0?ae_ground_raw_surface(ground,x,z):(uint16_t)contact->extra_a;
    if(solid<=hit->height) { hit->height=solid;hit->slope_x=contact->slope_x;hit->slope_z=contact->slope_z; }
    if(y<hit->height)hit->surface&=0xFFE0;
    return hit->height!=0x4000&&hit->height!=32767;
}
static int response_supported(uint32_t type,int collider) {
    uint32_t high=type&0xF000,kind=type&255;
    /* These helpers need the full collider record. A six-word grid probe cannot
     * supply auxiliary corner/ramp fields; reject invented grid combinations. */
    if(!collider&&(high==0x3000||high==0x9000)&&((kind>=70&&kind<=73)||(kind>=82&&kind<=84)))return 0;
    if(!high)return 1;
    if(collider&&high==0x1000)return kind>=74&&kind<=77;
    if(!collider&&(high==0x1000||(high>=0x5000&&high<=0x8000)))return 1;
    if(collider&&high>=0x5000&&high<=0x8000)return 1; /* Original no-contact stubs. */
    if(high==0x2000)return (kind>=7&&kind<=34)||(kind>=64&&kind<=67)||(kind>=85&&kind<=88);
    if(high==0x4000)return kind>=38&&kind<=41;
    if(high==0x3000||high==0x9000)return kind==0||(kind>=35&&kind<=37)||kind==54||kind==59||kind==68||(kind>=70&&kind<=73)||(kind>=78&&kind<=84)||kind==89;
    return 0;
}
int ae_collision_response_supported(const AeCollision *c,uint32_t out[2]) {
    out[0]=out[1]=0;
    for(uint32_t i=0;i<c->shape_count;++i)if(!response_supported(u16(c->bytes+c->shapes+i*20+18),0))++out[0];
    for(uint32_t i=0;i<c->collider_count;++i) {
        const unsigned char *b=c->bytes+c->colliders+i*56;
        if(!(u16(b+24)&0x8000)&&!response_supported(u16(b+16),1))++out[1];
    }
    return !out[0]&&!out[1];
}
static int shape_at_point(const AeCollision *c,int32_t x,int32_t y,int32_t z) {
    x=narrow(x);y=narrow(y);z=narrow(z);
    int32_t hx=(int32_t)c->width*256,hz=(int32_t)c->depth*256;
    if(x<=-hx||x>=hx||z<=-hz||z>=hz||y>0)return -1;
    int32_t gx=(x+hx)/512,gz=(z+hz)/512,band=-y/512;
    int index=read16(c->bytes+c->grid+((size_t)gz*c->width+gx)*2);
    while(index>=0) {
        const unsigned char *s=c->bytes+c->shapes+(size_t)index*20;
        int32_t sb=read16(s+2);
        if(band==sb|| (band>sb&&band<=sb+read16(s+8)/512-1))return index;
        if(band<sb)return -1;
        index=read16(s+16);
    }
    return -1;
}
static int response(int16_t b[6],uint32_t type,int collider,int radius,int height,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    uint32_t high=type&0xF000;
    if(!high)return ae_collision_box_probe(b,(int32_t)type,radius,height,p,old,out);
    if(collider&&high==0x1000)return ae_collision_strip_probe(b,(int32_t)type,radius,height,p,old,out);
    if(!collider&&(high==0x1000||(high>=0x5000&&high<=0x8000)))return ae_collision_wall_band_probe(b,(int32_t)type,radius,height,p,old,out);
    if(high==0x2000)return ae_collision_ramp_probe(b,(int32_t)type,radius,height,p,old,out);
    if(high==0x4000)return ae_collision_triangle_probe(b,(int32_t)type,radius,height,p,old,out);
    if(high==0x3000||high==0x9000) {
        int kind=type&255;
        if(kind==82)return 0; /* Ordinary body: rope handler only changes player mode state. */
        if(kind==83||kind==84)return ae_collision_trench_probe(b,(int32_t)type,radius,height,p,old,out);
        if(kind>=70&&kind<=73)return ae_collision_curved_probe(b,(int32_t)type,radius,height,p,old,out);
        if(kind==54||kind==68||(kind>=78&&kind<=81))return ae_collision_ring_probe(b,(int32_t)type,radius,height,p,old,out);
        return ae_collision_radial_probe(b,(int32_t)type,radius,height,p,old,out);
    }
    if(collider&&high>=0x5000&&high<=0x8000)return 0;
    return -1;
}
static int signed_overlap(int32_t y,int32_t h,int32_t top,int32_t height) {
    y=narrow(y);h=narrow(h);top=narrow(top);height=narrow(height);
    int bottom=top-height;
    if(y-h<top&&bottom<y-h)return bottom<y;
    return bottom>y-h&&y>top;
}
int ae_collision_probe(int16_t b[28],uint32_t type,int collider,int32_t radius,int32_t height,const int32_t p[3],const int32_t old[3],int32_t out[6]) {
    if(!response_supported(type,collider))return -1;
    if((type&0xF000)==0x2000)return ramp_probe(b,(int32_t)type,radius,height,p,old,out,1);
    return response(b,type,collider,radius,height,p,old,out);
}
int ae_collision_resolve(const AeCollision *c,const int32_t pos[3],const int32_t vel[3],int32_t radius,int32_t height,AeMoveHit *out) {
    memset(out,0,sizeof(*out));
    for(int i=0;i<3;++i) { out->position[i]=pos[i];out->velocity[i]=vel[i]; }
    out->position[0]+=vel[0];out->position[2]+=vel[2];
    int32_t hx=(int32_t)c->width*256,hz=(int32_t)c->depth*256;
    int32_t ux=(pos[0]+hx)%512-256,uz=(pos[2]+hz)%512-256;
    int xo=ux<0?-512:512,zo=uz<0?-512:512;
    for(int i=0;i<8;++i) {
        int x=pos[0]+((i==2||i==3||i==5||i==6)?xo:0);
        int z=pos[2]+((i==1||i==2||i==4||i==5)?zo:0);
        int y=pos[1]+(vel[1]>0?vel[1]:0)-(i>=4?512:0);
        int index=shape_at_point(c,x,y,z);if(index<0)continue;
        const unsigned char *s=c->bytes+c->shapes+(size_t)index*20;
        uint32_t type=u16(s+18);
        if(!response_supported(type,0))return bad("grid wall response type not implemented");
        int16_t b[6]={(int16_t)narrow(read16(s)*512-hx+256),(int16_t)narrow(-read16(s+2)*512),
                       (int16_t)narrow(read16(s+4)*512-hz+256),(int16_t)read16(s+6),(int16_t)read16(s+8),(int16_t)read16(s+10)};
        int32_t p[3]={out->position[0]+out->push_x,pos[1]+out->velocity[1],out->position[2]+out->push_z},hit[6]={0};
        if(response(b,type,0,radius,height,p,pos,hit)>0) {
            if(i==7&&hit[3]) { out->velocity[1]+=hit[3];if(hit[3]>out->ceiling)out->ceiling=hit[3]; }
            /* CollisionResolveProbes 80073630 uses lh for grid contact ids.
             * Collider ids below deliberately retain their lhu semantics. */
            else { out->push_x+=hit[0];out->push_z+=hit[2];out->contact=(int32_t)(0xA0000000u|(uint32_t)(int32_t)(int16_t)read16(s+14)); }
        }
    }
    for(int32_t i=(int32_t)c->collider_count-1;i>=0;--i) {
        int16_t b[28];collider_record(c,(uint32_t)i,b);
        if((uint16_t)b[12]&0x8000)continue;
        if(b[9]!=14&&(abs(b[0]-pos[0])>=b[3]+512||abs(b[1]-pos[1])>=b[4]+512||abs(b[2]-pos[2])>=b[5]+512))continue;
        uint32_t type=(uint16_t)b[8];if(!response_supported(type,1))return bad("collider wall response type not implemented");
        int32_t p[3]={out->position[0]+out->push_x,pos[1]+out->velocity[1],out->position[2]+out->push_z},hit[6]={0};
        if(response(b,type,1,radius,height,p,pos,hit)>0) {
            if(hit[3]>0&&signed_overlap(p[1],height,b[1]+hit[4],b[4]+hit[5]))out->velocity[1]+=hit[3];
            else { out->push_x+=hit[0];out->push_z+=hit[2]; }
            out->contact=(int32_t)(0xA0000000u|(uint16_t)b[11]);
            if((b[9]&255)==1)out->contact=(int32_t)(0xA8000000u|(uint16_t)b[11]);
        }
    }
    out->position[0]+=out->push_x;out->position[2]+=out->push_z;
    out->velocity[0]+=out->push_x;out->velocity[2]+=out->push_z;
    return 1;
}
