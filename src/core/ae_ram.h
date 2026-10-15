#ifndef AE_RAM_H
#define AE_RAM_H
/*
 * AeRam: a byte-exact image of PlayStation main RAM (2 MiB) plus the 1 KiB scratchpad.
 *
 * Gameplay modules ported 1:1 from the original code run on this image so the original data
 * (absolute pointers, struct layouts) is used unchanged and every ported function can be diffed
 * against the original instructions.  No game data lives in the repository: callers load the
 * EXE / area members from extract/ at run time.
 *
 * Addresses: main RAM 0x80000000..0x801FFFFF (also the KUSEG 0x00000000 and KSEG1 0xA0000000
 * mirrors), scratchpad 0x1F800000..0x1F8003FF (also its KSEG0 / KSEG1 mirrors).
 * Multi-byte accesses are little endian and must be naturally aligned (the R3000 faults on
 * unaligned lw/lh/sw/sh).
 *
 * Failure model: an access outside RAM / scratchpad, an unaligned access, or any module-level
 * "cannot reproduce" condition sets the sticky error flag (first message kept) instead of reading
 * or writing out of bounds.  Failed reads return 0 and failed writes are dropped.  Hosts and tests
 * must check ae_ram_failed().
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AE_RAM_BASE 0x80000000u
#define AE_RAM_SIZE 0x200000u
#define AE_SCRATCH_BASE 0x1F800000u
#define AE_SCRATCH_SIZE 0x400u
#define AE_AREA_BASE 0x80154000u      /* g_AreaData: where the area member is loaded */

typedef struct AeRam {
    uint8_t main[AE_RAM_SIZE];
    uint8_t scratch[AE_SCRATCH_SIZE];
    int error;                         /* sticky: 0 = ok */
    char error_text[192];
} AeRam;

AeRam *ae_ram_create(void);            /* zeroed image, or NULL */
void ae_ram_destroy(AeRam *r);

/* Sticky error flag. ae_ram_fail keeps the first message. */
void ae_ram_fail(AeRam *r, const char *fmt, ...);
int ae_ram_failed(const AeRam *r);
const char *ae_ram_error(const AeRam *r);
void ae_ram_clear_error(AeRam *r);

/* Pointer to `size` bytes at `addr`, or NULL (and the error flag) if any byte is outside one region. */
uint8_t *ae_ptr(AeRam *r, uint32_t addr, uint32_t size);

uint32_t ae_rd8(AeRam *r, uint32_t addr);
uint32_t ae_rd16(AeRam *r, uint32_t addr);
uint32_t ae_rd32(AeRam *r, uint32_t addr);
int32_t ae_rds8(AeRam *r, uint32_t addr);
int32_t ae_rds16(AeRam *r, uint32_t addr);
int32_t ae_rds32(AeRam *r, uint32_t addr);
void ae_wr8(AeRam *r, uint32_t addr, uint32_t value);
void ae_wr16(AeRam *r, uint32_t addr, uint32_t value);
void ae_wr32(AeRam *r, uint32_t addr, uint32_t value);

/* PS-X EXE: 0x800 byte header, text copied to the header's t_addr (0x80010000 for SCUS_944.23). */
int ae_ram_load_exe(AeRam *r, const uint8_t *exe, size_t size);
/* Copies a blob to `addr` (the area member goes to AE_AREA_BASE). */
int ae_ram_load(AeRam *r, uint32_t addr, const uint8_t *bytes, size_t size);

#ifdef __cplusplus
}
#endif
#endif
