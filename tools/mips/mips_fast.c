/* Test oracle only. Unsigned arithmetic gives defined MIPS wraparound; signed
 * overflow checks use int64_t. The caller owns all buffers and GTE state. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
typedef uint32_t (*GteStep)(uint32_t *, uint32_t, uint32_t);
typedef void (*WriteHook)(uint32_t, uint32_t, uint32_t);
typedef struct {
    uint32_t r[32], pc, branch, load_reg, load_value, hi, lo;
    uint32_t has_branch, has_load;
    uint64_t steps, remaining;
    char error[160];
} State;
typedef struct {
    uint8_t *ram, *scratch, *coverage;
    const uint32_t *special;
    uint32_t special_count;
    GteStep gte;
    uint32_t *gte_state;
    uint32_t *covered, covered_count;
    uint32_t exceptional_pc, has_exceptional_pc;
    uint32_t *code, *decoded;
    uint8_t *code_valid;
    uint32_t decoded_count;
    WriteHook write_hook;
} Memory;
static int fail(State *s, const char *fmt, ...) {
    va_list ap; va_start(ap,fmt); vsnprintf(s->error,sizeof(s->error),fmt,ap); va_end(ap); return -1;
}
static uint8_t *map(State *s, Memory *m, uint32_t a, unsigned n, int write) {
    /* Match _map before checking alignment: boundary/unmapped errors take
     * precedence over an unaligned access, including at scratchpad edges. */
    uint32_t off, len; uint8_t *p;
    if (a >= 0x1f800000 && a < 0x1f800400) { off=a-0x1f800000; len=1024; p=m->scratch; }
    else {
        unsigned seg=a>>29; off=a&0x1fffffff; len=0x200000; p=m->ram;
        if (!(seg==0 || seg==4 || seg==5) || off>=len) { fail(s,"unmapped address %08X",a); return NULL; }
    }
    if (off+n>len) { fail(s,"address %08X size %u out of bounds",a,n); return NULL; }
    if (off%n) { fail(s,"unaligned %s %08X/%u",write?"write":"read",a,n); return NULL; }
    return p+off;
}
static uint32_t readmem(State *s, Memory *m, uint32_t a, unsigned n) {
    uint8_t *p=map(s,m,a,n,0); uint32_t v=0;
    if (p) for(unsigned i=0;i<n;i++) v|=(uint32_t)p[i]<<(8*i);
    return v;
}
static void writemem(State *s, Memory *m, uint32_t a, unsigned n, uint32_t v) {
    /* A few checkers intercept SPU writes by overriding MipsRam.write. Keep
     * that Python surface while ordinary RAM accesses stay entirely in C. */
    if(m->write_hook) { m->write_hook(a,n,v); return; }
    uint8_t *p=map(s,m,a,n,1);
    if(p) for(unsigned i=0;i<n;i++) p[i]=(uint8_t)(v>>(8*i));
}
static int special(Memory *m,uint32_t pc) {
    /* Sorted addresses, binary search: no Python callback on ordinary instructions. */
    unsigned lo=0,hi=m->special_count;
    while(lo<hi) { unsigned mid=lo+(hi-lo)/2; if(m->special[mid]<pc) lo=mid+1; else hi=mid; }
    return lo<m->special_count && m->special[lo]==pc;
}
static int code_index(uint32_t pc) {
    if(pc&3) return -1;
    if(pc>=0x1f800000 && pc<0x1f800400) return 3*0x80000+(pc-0x1f800000)/4;
    unsigned seg=pc>>29,phys=pc&0x1fffffff;
    if(!(seg==0 || seg==4 || seg==5) || phys>=0x200000) return -1;
    return (seg==4?1:seg==5?2:0)*0x80000+phys/4;
}
/* 0: returned, 1: Python hook, -1: error. Hook visits consume budget,
 * but do not count as executed instructions, exactly like the reference. */
int mips_run(State *s, Memory *m) {
    while(s->remaining) {
        s->remaining--;
        uint32_t pc=s->pc;
        if(!s->has_branch) {
            if(pc==0xfffffff0) {
                if(s->has_load) { s->r[s->load_reg]=s->load_value; s->r[0]=0; s->has_load=0; }
                return 0;
            }
            if(special(m,pc)) {
                if(s->has_load) { s->r[s->load_reg]=s->load_value; s->r[0]=0; s->has_load=0; }
                return 1;
            }
        } else if(special(m,pc)) return fail(s,"hook at %08X lands in a branch delay slot",pc);
        if(m->coverage) {
            unsigned seg=pc>>29;
            int scratch=pc>=0x1f800000 && pc<0x1f800400;
            if((pc&3) || (!scratch && (!(seg==0 || seg==4 || seg==5) || (pc&0x1fffffff)>=0x200000))) {
                m->exceptional_pc=pc; m->has_exceptional_pc=1;
            } else {
                /* Exact virtual PCs, including all RAM aliases and scratchpad. */
                unsigned bank=seg==4?1:seg==5?2:0;
                unsigned index=scratch ? 3*0x80000+(pc-0x1f800000)/4 : bank*0x80000+(pc&0x1fffff)/4;
                uint8_t mask=1u<<(index&7);
                if(!(m->coverage[index>>3]&mask)) {
                    m->coverage[index>>3]|=mask;
                    m->covered[m->covered_count++]=index;
                }
            }
        }
        /* The reference caches by virtual PC, rather than invalidating on RAM
         * writes. Preserve this documented policy even when switching backends. */
        int index=code_index(pc);
        uint32_t ins;
        if(index>=0 && (m->code_valid[index>>3] & (1u<<(index&7)))) ins=m->code[index];
        else {
            ins=readmem(s,m,pc,4);
            if(!s->error[0] && index>=0) {
                m->code[index]=ins;
                m->code_valid[index>>3]|=1u<<(index&7);
                m->decoded[m->decoded_count++]=(uint32_t)index;
            }
        }
        if(s->error[0]) return -1;
        unsigned op=ins>>26,rs=(ins>>21)&31,rt=(ins>>16)&31,rd=(ins>>11)&31,sh=(ins>>6)&31,fn=ins&63,imm=ins&65535;
        int32_t simm=(int16_t)imm;
        uint32_t a=s->r[rs],b=s->r[rt],dest=0,val=0,oldbranch=s->branch,oldreg=s->load_reg,oldval=s->load_value;
        unsigned hadbranch=s->has_branch,hadload=s->has_load;
        s->has_branch=s->has_load=0; s->steps++;
        uint32_t addr=a+(uint32_t)simm,word,shift,old;
        int64_t wide;
#define DEST(d,v) do { dest=(d); val=(uint32_t)(v); } while(0)
#define BR(v) do { s->has_branch=1; s->branch=(v); } while(0)
#define LOAD(v) do { s->has_load=1; s->load_reg=rt; s->load_value=(v); } while(0)
#define TRAP(name) do { if(wide < INT32_MIN || wide > INT32_MAX) return fail(s,name " overflow at %08X",pc); } while(0)
        if(op==0) switch(fn) {
            case 0: DEST(rd,b<<sh); break;
            case 2: DEST(rd,b>>sh); break;
            case 3: DEST(rd,(int32_t)b>>sh); break;
            case 4: DEST(rd,b<<(a&31)); break;
            case 6: DEST(rd,b>>(a&31)); break;
            case 7: DEST(rd,(int32_t)b>>(a&31)); break;
            case 8: BR(a); break;
            case 9: BR(a); DEST(rd,pc+8); break;
            case 16: DEST(rd,s->hi); break;
            case 17: s->hi=a; break;
            case 18: DEST(rd,s->lo); break;
            case 19: s->lo=a; break;
            case 24: case 25: {
                uint64_t product=fn==24?(uint64_t)((int64_t)(int32_t)a*(int32_t)b):(uint64_t)a*b;
                s->lo=(uint32_t)product; s->hi=(uint32_t)(product>>32); break;
            }
            case 26: case 27:
                if(!b) { s->lo=fn==26 && (int32_t)a<0?1:UINT32_MAX; s->hi=a; }
                /* Widen before division: INT_MIN/-1 cannot overflow the host. */
                else if(fn==26) { int64_t x=(int32_t)a,y=(int32_t)b; s->lo=(uint32_t)(x/y); s->hi=(uint32_t)(x%y); }
                else { s->lo=a/b; s->hi=a%b; } break;
            case 32: wide=(int64_t)(int32_t)a+(int32_t)b; TRAP("add"); DEST(rd,wide); break;
            case 34: wide=(int64_t)(int32_t)a-(int32_t)b; TRAP("sub"); DEST(rd,wide); break;
            case 33: DEST(rd,a+b); break;
            case 35: DEST(rd,a-b); break;
            case 36: DEST(rd,a&b); break;
            case 37: DEST(rd,a|b); break;
            case 38: DEST(rd,a^b); break;
            case 39: DEST(rd,~(a|b)); break;
            case 42: DEST(rd,(int32_t)a<(int32_t)b); break;
            case 43: DEST(rd,a<b); break;
            case 12: case 13: return fail(s,"syscall/break at %08X",pc);
            default: return fail(s,"unsupported SPECIAL %#x at %08X",fn,pc);
        } else switch(op) {
            case 1:
                if(rt!=0 && rt!=1 && rt!=16 && rt!=17) return fail(s,"unsupported REGIMM %u at %08X",rt,pc);
                if(rt>=16) DEST(31,pc+8);
                if((rt&1)?(int32_t)a>=0:(int32_t)a<0) BR(pc+4+(uint32_t)(simm*4)); break;
            case 2: case 3: BR(((pc+4)&0xf0000000)|((ins&0x3ffffff)<<2)); if(op==3) DEST(31,pc+8); break;
            case 4: if(a==b) BR(pc+4+(uint32_t)(simm*4)); break;
            case 5: if(a!=b) BR(pc+4+(uint32_t)(simm*4)); break;
            case 6: if((int32_t)a<=0) BR(pc+4+(uint32_t)(simm*4)); break;
            case 7: if((int32_t)a>0) BR(pc+4+(uint32_t)(simm*4)); break;
            case 8: wide=(int64_t)(int32_t)a+simm; TRAP("addi"); DEST(rt,wide); break;
            case 9: DEST(rt,addr); break;
            case 10: DEST(rt,(int32_t)a<simm); break;
            case 11: DEST(rt,a<(uint32_t)simm); break;
            case 12: DEST(rt,a&imm); break;
            case 13: DEST(rt,a|imm); break;
            case 14: DEST(rt,a^imm); break;
            case 15: DEST(rt,imm<<16); break;
            case 16: return fail(s,"COP0 at %08X",pc);
            case 18:
                if(!m->gte) return fail(s,"COP2 %08X at %08X without gte_backend",ins,pc);
                word=m->gte(m->gte_state,ins,(ins&0x02000000)?0:b);
                if(!(ins&0x02000000) && (rs==0 || rs==2)) LOAD(word); break;
            case 32: LOAD((uint32_t)(int32_t)(int8_t)readmem(s,m,addr,1)); break;
            case 33: LOAD((uint32_t)(int32_t)(int16_t)readmem(s,m,addr,2)); break;
            case 35: LOAD(readmem(s,m,addr,4)); break;
            case 36: LOAD(readmem(s,m,addr,1)); break;
            case 37: LOAD(readmem(s,m,addr,2)); break;
            case 34: case 38:
                shift=(addr&3)*8; word=readmem(s,m,addr&~3u,4); old=hadload && oldreg==rt?oldval:b;
                /* Merge against an in-flight load to the same register. */
                if(op==34) LOAD((old&((1u<<(24-shift))-1))|(word<<(24-shift)));
                else LOAD((old&(shift?UINT32_MAX<<(32-shift):0))|(word>>shift)); break;
            case 40: writemem(s,m,addr,1,b); break;
            case 41: writemem(s,m,addr,2,b); break;
            case 43: writemem(s,m,addr,4,b); break;
            case 42: case 46:
                for(unsigned i=op==42?0:addr&3; i<(op==42?(addr&3)+1:4);i++) {
                    writemem(s,m,(addr&~3u)+i,1,b>>((op==42?3-(addr&3)+i:i-(addr&3))*8));
                    if(s->error[0]) return -1;
                } break;
            case 50:
                if(!m->gte) return fail(s,"lwc2 without gte_backend at %08X",pc);
                word=readmem(s,m,addr,4); if(s->error[0]) return -1;
                m->gte(m->gte_state,0x48800000|(rt<<11),word); break;
            case 58:
                if(!m->gte) return fail(s,"swc2 without gte_backend at %08X",pc);
                word=m->gte(m->gte_state,0x48000000|(rt<<11),0);
                if(s->error[0]) return -1;
                writemem(s,m,addr,4,word); break;
            default: return fail(s,"unsupported opcode %#x at %08X",op,pc);
        }
        if(s->error[0]) return -1;
        if(dest) s->r[dest]=val;
        /* The slot observes the old value. An explicit slot write cancels the
         * previous load, while a new load still commits on the following slot. */
        if(hadload && oldreg!=dest) s->r[oldreg]=oldval;
        s->r[0]=0;
        /* A branch decoded above waits until the next instruction; a branch
         * inherited from the previous instruction supplies this next PC. */
        s->pc=hadbranch?oldbranch:pc+4;
    }
    return fail(s,"instruction budget exceeded");
}
