#ifndef AE_MONKEY_INL_H
#define AE_MONKEY_INL_H
/*
 * Small helpers shared by the top levels of the monkey port (ae_monkey_level8.c .. ae_monkey_level10.c): the RAM access
 * shorthands and the MIPS arithmetic idioms (div / divu traps, 32 bit wrap, arithmetic shifts, multiply-high).
 * Included only by the .c files of the port (static inline: no symbols of its own).
 */
#include "ae_monkey.h"

#define RD8(a) ae_rd8(r, (a))
#define RD16(a) ae_rd16(r, (a))
#define RD32(a) ae_rd32(r, (a))
#define RDS8(a) ae_rds8(r, (a))
#define RDS16(a) ae_rds16(r, (a))
#define RDS32(a) ae_rds32(r, (a))
#define WR8(a, v) ae_wr8(r, (a), (uint32_t)(v))
#define WR16(a, v) ae_wr16(r, (a), (uint32_t)(v))
#define WR32(a, v) ae_wr32(r, (a), (uint32_t)(v))

static inline int32_t s16(uint32_t v) { return (int32_t)(int16_t)(uint16_t)v; }
static inline int32_t sra32(int32_t v, uint32_t n) {
    n &= 31u;
    return v < 0 ? (int32_t)~((~(uint32_t)v) >> n) : (int32_t)((uint32_t)v >> n);
}
static inline int32_t mul_lo(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static inline int32_t mul_hi(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * (int64_t)b) >> 32); }
static inline uint32_t mulu_hi(uint32_t a, uint32_t b) { return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32); }
static inline int32_t neg32(int32_t v) { return (int32_t)(0u - (uint32_t)v); }
static inline int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static inline int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static inline int32_t L(int32_t v, uint32_t n) { return (int32_t)((uint32_t)v << n); }   /* sll */
static inline uint32_t cur_monkey(AeRam *r) { return RD32(AE_G_CUR_MONKEY); }
static inline uint32_t info_of(AeRam *r, uint32_t m) {          /* MonkeyInfo of the monkey: table + id * 0x88 */
    return RD32(AE_G_MONKEY_INFO_TABLE) + RD8(m + AE_MK_ID) * AE_MONKEY_INFO_SIZE;
}
static inline uint32_t node_of(AeRam *r, uint32_t index) {      /* nav node address: table + index * 0x18 */
    return index * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES);
}
/* MIPS div / divu: break (a trap) on a zero divisor and, signed, INT_MIN / -1 */
static inline int32_t div_trap(AeRam *r, int32_t a, int32_t b) {
    if (b == 0 || (b == -1 && a == INT32_MIN)) {
        ae_ram_fail(r, "divide trap (div by zero / INT_MIN / -1; the original executes break)");
        return 0;
    }
    return a / b;
}
static inline int32_t rem_trap(AeRam *r, int32_t a, int32_t b) {
    if (b == 0 || (b == -1 && a == INT32_MIN)) {
        ae_ram_fail(r, "divide trap (div by zero / INT_MIN / -1; the original executes break)");
        return 0;
    }
    return a % b;
}
static inline uint32_t divu_trap(AeRam *r, uint32_t a, uint32_t b) {
    if (b == 0) {
        ae_ram_fail(r, "divide trap (divu by zero; the original executes break)");
        return 0;
    }
    return a / b;
}

#endif
