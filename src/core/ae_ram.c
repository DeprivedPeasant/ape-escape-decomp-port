#include "ae_ram.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AeRam *ae_ram_create(void) {
    return (AeRam *)calloc(1, sizeof(AeRam));
}

void ae_ram_destroy(AeRam *r) {
    free(r);
}

void ae_ram_fail(AeRam *r, const char *fmt, ...) {
    if (!r || r->error) return;
    r->error = 1;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->error_text, sizeof(r->error_text), fmt, ap);
    va_end(ap);
}

int ae_ram_failed(const AeRam *r) { return r ? r->error : 1; }
const char *ae_ram_error(const AeRam *r) { return r && r->error ? r->error_text : ""; }
void ae_ram_clear_error(AeRam *r) { if (r) { r->error = 0; r->error_text[0] = 0; } }

/* Region lookup. Returns the byte pointer for [addr, addr+size) or NULL without touching the error flag. */
static uint8_t *locate(AeRam *r, uint32_t addr, uint32_t size) {
    uint32_t seg = addr >> 29;
    uint32_t phys = addr & 0x1FFFFFFFu;
    if (seg != 0 && seg != 4 && seg != 5) return NULL;
    if (phys >= AE_SCRATCH_BASE) {
        uint32_t off = phys - AE_SCRATCH_BASE;
        if (off >= AE_SCRATCH_SIZE || size > AE_SCRATCH_SIZE - off) return NULL;
        return r->scratch + off;
    }
    if (phys >= AE_RAM_SIZE || size > AE_RAM_SIZE - phys) return NULL;
    return r->main + phys;
}

uint8_t *ae_ptr(AeRam *r, uint32_t addr, uint32_t size) {
    if (!r) return NULL;
    uint8_t *p = locate(r, addr, size);
    if (!p) ae_ram_fail(r, "access outside RAM: %08X size %u", (unsigned)addr, (unsigned)size);
    return p;
}

static uint8_t *aligned(AeRam *r, uint32_t addr, uint32_t size) {
    if (addr & (size - 1)) {
        ae_ram_fail(r, "unaligned %u-byte access at %08X", (unsigned)size, (unsigned)addr);
        return NULL;
    }
    return ae_ptr(r, addr, size);
}

uint32_t ae_rd8(AeRam *r, uint32_t addr) {
    const uint8_t *p = ae_ptr(r, addr, 1);
    return p ? p[0] : 0;
}
uint32_t ae_rd16(AeRam *r, uint32_t addr) {
    const uint8_t *p = aligned(r, addr, 2);
    return p ? (uint32_t)p[0] | (uint32_t)p[1] << 8 : 0;
}
uint32_t ae_rd32(AeRam *r, uint32_t addr) {
    const uint8_t *p = aligned(r, addr, 4);
    return p ? (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24 : 0;
}
int32_t ae_rds8(AeRam *r, uint32_t addr) { return (int8_t)ae_rd8(r, addr); }
int32_t ae_rds16(AeRam *r, uint32_t addr) { return (int16_t)ae_rd16(r, addr); }
int32_t ae_rds32(AeRam *r, uint32_t addr) { return (int32_t)ae_rd32(r, addr); }

void ae_wr8(AeRam *r, uint32_t addr, uint32_t v) {
    uint8_t *p = ae_ptr(r, addr, 1);
    if (p) p[0] = (uint8_t)v;
}
void ae_wr16(AeRam *r, uint32_t addr, uint32_t v) {
    uint8_t *p = aligned(r, addr, 2);
    if (p) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
}
void ae_wr32(AeRam *r, uint32_t addr, uint32_t v) {
    uint8_t *p = aligned(r, addr, 4);
    if (p) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
}

int ae_ram_load_exe(AeRam *r, const uint8_t *exe, size_t size) {
    if (!r || !exe || size < 0x800 || memcmp(exe, "PS-X EXE", 8) != 0) {
        ae_ram_fail(r, "not a PS-X EXE image");
        return 0;
    }
    uint32_t t_addr = (uint32_t)exe[0x18] | (uint32_t)exe[0x19] << 8 | (uint32_t)exe[0x1A] << 16 | (uint32_t)exe[0x1B] << 24;
    uint32_t t_size = (uint32_t)exe[0x1C] | (uint32_t)exe[0x1D] << 8 | (uint32_t)exe[0x1E] << 16 | (uint32_t)exe[0x1F] << 24;
    size_t n = size - 0x800;
    if (t_size < n) n = t_size;
    return ae_ram_load(r, t_addr, exe + 0x800, n);
}

int ae_ram_load(AeRam *r, uint32_t addr, const uint8_t *bytes, size_t size) {
    if (!r || (!bytes && size)) return 0;
    if (size > 0xFFFFFFFFu) { ae_ram_fail(r, "load too large"); return 0; }
    uint8_t *p = ae_ptr(r, addr, (uint32_t)size);
    if (!p && size) return 0;
    if (size) memcpy(p, bytes, size);
    return 1;
}
