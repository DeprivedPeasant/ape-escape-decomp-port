/* Original portable rigid skin/animation implementation for prepared local assets. */
#include "native_actor.h"
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

typedef struct { float xyz[3], uv[2], rgb[3]; uint32_t joint; } ActorVertex;
typedef struct { uint32_t first, count, texture, kind; } ActorBatch;
typedef struct { char name[64]; uint32_t frames; float fps; const float *samples; } Clip;
struct AeActor {
    unsigned char *bytes;
    size_t size;
    uint32_t nn, nj, nv, nb, nt, nc;
    int32_t *parent;
    uint32_t *joint_node;
    float *inverse, *world, *skin, *positions;
    unsigned char *ready;
    ActorVertex *vertices, *posed;
    ActorBatch *batches;
    Clip *clips;
    const unsigned char **pixels;
    uint32_t *width, *height;
    GLuint *textures;
    int uploaded;
};
static char actor_error[256];
static uint32_t word(const unsigned char *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static float real(const unsigned char *p) { uint32_t v=word(p); float f; memcpy(&f,&v,4); return f; }
static int bad(const char *text) { snprintf(actor_error,sizeof(actor_error),"%s",text); return 0; }
static int range(size_t size,size_t offset,size_t n,size_t stride) { return offset<=size && (!n || stride<=(size-offset)/n); }
const char *ae_actor_error(void) { return actor_error; }
void ae_actor_free(AeActor *a) {
    if (!a) return;
    if (a->uploaded) glDeleteTextures((GLsizei)a->nt,a->textures);
    free(a->bytes); free(a->parent); free(a->joint_node); free(a->inverse); free(a->world);
    free(a->skin); free(a->positions); free(a->ready); free(a->posed); free(a->batches);
    free(a->clips); free(a->pixels); free(a->width); free(a->height); free(a->textures); free(a);
}
static int trs_valid(const float *p) {
    double norm=0;
    for (int i=0; i<10; ++i) if (!isfinite(p[i]) || fabsf(p[i])>1e6f) return 0;
    for (int i=3; i<7; ++i) norm+=(double)p[i]*p[i];
    return fabs(norm-1)<.0001;
}
static int parse(AeActor *a) {
    size_t cursor=64;
    uint32_t expected=0;
    if (memcmp(a->bytes,"AEACTR1\0",8) || word(a->bytes+8)!=1 || word(a->bytes+36)) return bad("actor signature/version/flags invalid");
    a->nn=word(a->bytes+12); a->nj=word(a->bytes+16); a->nv=word(a->bytes+20);
    a->nb=word(a->bytes+24); a->nt=word(a->bytes+28); a->nc=word(a->bytes+32);
    if (!a->nn || a->nn>256 || !a->nj || a->nj>a->nn || !a->nv || a->nv>300000 || a->nv%3 ||
        !a->nb || a->nb>10000 || a->nt>4096 || !a->nc || a->nc>4096) return bad("actor count limits violated");
    for (int i=0; i<6; ++i) if (!isfinite(real(a->bytes+40+i*4))) return bad("actor bounds invalid");
    a->parent=(int32_t *)calloc(a->nn,sizeof(int32_t)); a->joint_node=(uint32_t *)calloc(a->nj,4);
    a->inverse=(float *)calloc(a->nj*16,sizeof(float)); a->world=(float *)calloc(a->nn*16,sizeof(float));
    a->skin=(float *)calloc(a->nj*16,sizeof(float)); a->positions=(float *)calloc(a->nv*3,sizeof(float));
    a->ready=(unsigned char *)calloc(a->nn,1); a->posed=(ActorVertex *)calloc(a->nv,sizeof(ActorVertex));
    a->batches=(ActorBatch *)calloc(a->nb,sizeof(ActorBatch)); a->clips=(Clip *)calloc(a->nc,sizeof(Clip));
    a->pixels=(const unsigned char **)calloc(a->nt+1,sizeof(*a->pixels)); a->width=(uint32_t *)calloc(a->nt+1,4);
    a->height=(uint32_t *)calloc(a->nt+1,4); a->textures=(GLuint *)calloc(a->nt+1,sizeof(GLuint));
    if (!a->parent || !a->joint_node || !a->inverse || !a->world || !a->skin || !a->positions || !a->ready ||
        !a->posed || !a->batches || !a->clips || !a->pixels || !a->width || !a->height || !a->textures) return bad("actor allocation failed");
    if (!range(a->size,cursor,a->nn,44)) return bad("actor nodes truncated");
    for (uint32_t i=0; i<a->nn; ++i) {
        a->parent[i]=(int32_t)word(a->bytes+cursor+i*44);
        if (a->parent[i]<-1 || a->parent[i]>=(int32_t)a->nn || !trs_valid((const float *)(const void *)(a->bytes+cursor+i*44+4))) return bad("actor parent/rest TRS invalid");
    }
    for (uint32_t i=0; i<a->nn; ++i) {
        int32_t node=(int32_t)i;
        for (uint32_t steps=0; node>=0; ++steps) {
            if (steps>=a->nn) return bad("actor hierarchy cycle");
            node=a->parent[node];
        }
    }
    cursor+=(size_t)a->nn*44;
    if (!range(a->size,cursor,a->nj,68)) return bad("actor joints truncated");
    for (uint32_t i=0; i<a->nj; ++i) {
        a->joint_node[i]=word(a->bytes+cursor+i*68);
        if (a->joint_node[i]>=a->nn) return bad("actor joint node invalid");
        for (uint32_t j=0; j<i; ++j) if (a->joint_node[j]==a->joint_node[i]) return bad("duplicate actor joint");
        for (int r=0; r<4; ++r) for (int c=0; c<4; ++c) {
            float v=real(a->bytes+cursor+i*68+4+(c*4+r)*4);
            if (!isfinite(v) || fabsf(v)>1e6f) return bad("inverse bind invalid");
            a->inverse[i*16+r*4+c]=v;
        }
    }
    cursor+=(size_t)a->nj*68;
    if (!range(a->size,cursor,a->nb,16)) return bad("actor batches truncated");
    for (uint32_t i=0; i<a->nb; ++i) {
        ActorBatch *b=a->batches+i; const unsigned char *p=a->bytes+cursor+i*16;
        b->first=word(p); b->count=word(p+4); b->texture=word(p+8); b->kind=word(p+12);
        if (b->first!=expected || !b->count || b->count%3 || b->count>a->nv-expected || b->kind!=1 ||
            (b->texture!=UINT32_MAX && b->texture>=a->nt)) return bad("actor batch invalid");
        expected+=b->count;
    }
    if (expected!=a->nv) return bad("actor batch coverage invalid");
    cursor+=(size_t)a->nb*16;
    if (!range(a->size,cursor,a->nv,36)) return bad("actor vertices truncated");
    a->vertices=(ActorVertex *)(void *)(a->bytes+cursor);
    for (uint32_t i=0; i<a->nv; ++i) {
        const float *v=(const float *)(const void *)(a->vertices+i);
        if (a->vertices[i].joint>=a->nj) return bad("actor vertex joint invalid");
        for (int j=0; j<8; ++j) if (!isfinite(v[j]) || fabsf(v[j])>1e6f || (j>=5 && (v[j]<0 || v[j]>1))) return bad("actor vertex invalid");
    }
    memcpy(a->posed,a->vertices,a->nv*sizeof(ActorVertex)); cursor+=(size_t)a->nv*36;
    for (uint32_t i=0; i<a->nc; ++i) {
        Clip *clip=a->clips+i;
        if (!range(a->size,cursor,1,72)) return bad("actor clip header truncated");
        memcpy(clip->name,a->bytes+cursor,64);
        if (!memchr(clip->name,0,64)) return bad("actor clip name unterminated");
        clip->frames=word(a->bytes+cursor+64); clip->fps=real(a->bytes+cursor+68); cursor+=72;
        if (!clip->frames || clip->frames>100000 || !isfinite(clip->fps) || clip->fps<=0 || clip->fps>1000 ||
            !range(a->size,cursor,clip->frames,(size_t)a->nn*40)) return bad("actor clip timing/length invalid");
        clip->samples=(const float *)(const void *)(a->bytes+cursor);
        for (size_t j=0; j<(size_t)clip->frames*a->nn; ++j) if (!trs_valid(clip->samples+j*10)) return bad("actor animation TRS invalid");
        cursor+=(size_t)clip->frames*a->nn*40;
    }
    for (uint32_t i=0; i<a->nt; ++i) {
        uint32_t n;
        if (!range(a->size,cursor,1,12)) return bad("actor texture header truncated");
        a->width[i]=word(a->bytes+cursor); a->height[i]=word(a->bytes+cursor+4); n=word(a->bytes+cursor+8); cursor+=12;
        if (!a->width[i] || !a->height[i] || a->width[i]>4096 || a->height[i]>4096 || n!=a->width[i]*a->height[i]*4 ||
            !range(a->size,cursor,1,n)) return bad("actor texture invalid/truncated");
        a->pixels[i]=a->bytes+cursor; cursor+=n;
    }
    return cursor==a->size ? 1 : bad("actor trailing bytes");
}
AeActor *ae_actor_load(const char *path) {
    FILE *file=fopen(path,"rb"); long length;
    AeActor *a;
    if (!file) { bad("cannot open actor pack"); return NULL; }
    if (fseek(file,0,SEEK_END) || (length=ftell(file))<64 || length>268435456) {
        fclose(file); bad("actor file size invalid"); return NULL;
    }
    rewind(file); a=(AeActor *)calloc(1,sizeof(AeActor));
    if (!a) { fclose(file); bad("actor allocation failed"); return NULL; }
    a->size=(size_t)length; a->bytes=(unsigned char *)malloc(a->size);
    if (!a->bytes || fread(a->bytes,1,a->size,file)!=a->size) {
        fclose(file); ae_actor_free(a); bad("actor read/allocation failed"); return NULL;
    }
    fclose(file);
    if (!parse(a)) { ae_actor_free(a); return NULL; }
    return a;
}
void ae_actor_info(const AeActor *a,uint32_t out[4]) { out[0]=a->nn; out[1]=a->nj; out[2]=a->nv; out[3]=a->nc; }
uint32_t ae_actor_frames(const AeActor *a,uint32_t clip) { return clip<a->nc ? a->clips[clip].frames : 0; }
float ae_actor_fps(const AeActor *a,uint32_t clip) { return clip<a->nc ? a->clips[clip].fps : 0; }
const char *ae_actor_clip_name(const AeActor *a,uint32_t clip) { return clip<a->nc ? a->clips[clip].name : "invalid"; }
static void multiply(float *out,const float *a,const float *b) {
    for (int r=0; r<4; ++r) for (int c=0; c<4; ++c) {
        out[r*4+c]=0;
        for (int k=0; k<4; ++k) out[r*4+c]+=a[r*4+k]*b[k*4+c];
    }
}
static void world_matrix(AeActor *a,uint32_t node,const float *trs) {
    float local[16]={0},x,y,z,w;
    const float *p=trs+node*10;
    if (a->ready[node]) return;
    x=p[3]; y=p[4]; z=p[5]; w=p[6];
    local[0]=(1-2*(y*y+z*z))*p[7]; local[1]=2*(x*y-z*w)*p[8]; local[2]=2*(x*z+y*w)*p[9];
    local[4]=2*(x*y+z*w)*p[7]; local[5]=(1-2*(x*x+z*z))*p[8]; local[6]=2*(y*z-x*w)*p[9];
    local[8]=2*(x*z-y*w)*p[7]; local[9]=2*(y*z+x*w)*p[8]; local[10]=(1-2*(x*x+y*y))*p[9];
    local[3]=p[0]; local[7]=p[1]; local[11]=p[2]; local[15]=1;
    if (a->parent[node]>=0) {
        world_matrix(a,(uint32_t)a->parent[node],trs);
        multiply(a->world+node*16,a->world+a->parent[node]*16,local);
    } else memcpy(a->world+node*16,local,sizeof(local));
    a->ready[node]=1;
}
int ae_actor_pose(AeActor *a,uint32_t clip,uint32_t frame) {
    if (clip>=a->nc || frame>=a->clips[clip].frames) return bad("actor clip/frame out of range");
    const float *trs=a->clips[clip].samples+(size_t)frame*a->nn*10;
    memset(a->ready,0,a->nn);
    for (uint32_t i=0; i<a->nn; ++i) world_matrix(a,i,trs);
    for (uint32_t i=0; i<a->nj; ++i) multiply(a->skin+i*16,a->world+a->joint_node[i]*16,a->inverse+i*16);
    for (uint32_t i=0; i<a->nv; ++i) {
        const float *matrix=a->skin+a->vertices[i].joint*16, *p=a->vertices[i].xyz;
        for (int r=0; r<3; ++r) {
            float value=matrix[r*4]*p[0]+matrix[r*4+1]*p[1]+matrix[r*4+2]*p[2]+matrix[r*4+3];
            if (!isfinite(value)) return bad("actor pose overflow");
            a->posed[i].xyz[r]=value; a->positions[i*3+r]=value;
        }
    }
    return 1;
}
int ae_actor_positions(const AeActor *a,float *out,uint32_t capacity) {
    if (capacity<a->nv*3) return bad("actor output buffer too small");
    memcpy(out,a->positions,a->nv*3*sizeof(float)); return 1;
}
int ae_actor_upload(AeActor *a) {
    if (a->uploaded) return 1;
    glGenTextures((GLsizei)a->nt,a->textures); a->uploaded=1;
    for (uint32_t i=0; i<a->nt; ++i) {
        glBindTexture(GL_TEXTURE_2D,a->textures[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,(GLsizei)a->width[i],(GLsizei)a->height[i],0,GL_RGBA,GL_UNSIGNED_BYTE,a->pixels[i]);
    }
    return glGetError()==GL_NO_ERROR ? 1 : bad("actor texture upload failed");
}
void ae_actor_draw(AeActor *a,int textured,const float position[3],float yaw) {
    glPushMatrix(); glTranslatef(position[0],position[1],position[2]); glRotatef(yaw,0,1,0);
    glVertexPointer(3,GL_FLOAT,sizeof(ActorVertex),a->posed[0].xyz);
    glTexCoordPointer(2,GL_FLOAT,sizeof(ActorVertex),a->posed[0].uv);
    glColorPointer(3,GL_FLOAT,sizeof(ActorVertex),a->posed[0].rgb);
    for (uint32_t i=0; i<a->nb; ++i) {
        ActorBatch *b=a->batches+i;
        if (textured && b->texture!=UINT32_MAX) {
            glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,a->textures[b->texture]);
        } else glDisable(GL_TEXTURE_2D);
        glDrawArrays(GL_TRIANGLES,(GLint)b->first,(GLsizei)b->count);
    }
    glPopMatrix();
}
