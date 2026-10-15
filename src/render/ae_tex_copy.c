#include "ae_tex_copy.h"

#include <stdint.h>

#define AE_TEX_DEFAULT_STACK_TOP 0x801FFF00u
#define AE_TEX_VARIANT_RECT 0x800A5A00u
#define AE_TEX_COPY_RECT_TABLE 0x800B98D0u
#define AE_TEX_HEAP_POINTER_TABLE 0x800BF690u
#define AE_TEX_HEAP_POINTER_COUNT 64u

#define AE_TEX_RECT_X 0x00u
#define AE_TEX_RECT_Y 0x02u
#define AE_TEX_RECT_WIDTH 0x04u
#define AE_TEX_RECT_HEIGHT 0x06u
#define AE_TEX_RECT_WORD_COUNT 2u

#define AE_TEX_FRAME_SIZE 56u
#define AE_TEX_STACK_RECT_OFFSET 16u
#define AE_TEX_HEAP_BUFFER_SIZE 4096u
#define AE_TEX_PIXEL_SIZE 2u
#define AE_TEX_COLOR_RED_BYTE 0u
#define AE_TEX_COLOR_GREEN_BYTE 1u
#define AE_TEX_COLOR_BLUE_BYTE 2u
#define AE_TEX_COLOR_COMPONENT_MASK 0xF8u
#define AE_TEX_COLOR_ALPHA_MASK 0x8000u
#define AE_TEX_VARIANT_COUNT 8u
#define AE_TEX_VARIANT_FIRST 1u
#define AE_TEX_VARIANT_DIVISOR 8
#define AE_TEX_VARIANT_ROW_WIDTH 4u
#define AE_TEX_VARIANT_X_BASE 576
#define AE_TEX_VARIANT_Y_BASE 256
#define AE_TEX_VARIANT_X_STEP 16
#define AE_TEX_VARIANT_Y_STEP 128

static uint32_t tex_stack_pointer(const AeTexCopyHooks *hooks) {
    if (hooks && hooks->stack_pointer != 0u) return hooks->stack_pointer;
    return AE_TEX_DEFAULT_STACK_TOP;
}

static uint32_t tex_frame_pointer(const AeTexCopyHooks *hooks, uint32_t frame_size) {
    return tex_stack_pointer(hooks) - frame_size;
}

static uint32_t tex_call_hook(AeRam *r, const AeTexCopyHooks *hooks,
                              AeTexCopyHookFn function, const char *name,
                              uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (!hooks || !function) {
        ae_ram_fail(r, "AeTexCopyHooks.%s not provided", name);
        return 0;
    }
    return function(hooks->user, r, a0, a1, a2, a3);
}

#define TEX_COPY_CALL(hooks, name, a0, a1, a2, a3) \
    tex_call_hook(r, hooks, hooks ? hooks->name : NULL, #name, \
                  (uint32_t)(a0), (uint32_t)(a1), (uint32_t)(a2), (uint32_t)(a3))

/* Public SDK calls are forwarded unchanged to the native renderer. */
static int32_t tex_draw_sync(AeRam *r, const AeTexCopyHooks *h, int32_t mode) {
    if (!h || !h->DrawSync) { ae_ram_fail(r, "AeTexCopyHooks.DrawSync not provided"); return 0; }
    return h->DrawSync(h->user, r, mode);
}
static int32_t tex_load_image(AeRam *r, const AeTexCopyHooks *h, uint32_t rect, uint32_t image) {
    if (!h || !h->LoadImage) { ae_ram_fail(r, "AeTexCopyHooks.LoadImage not provided"); return 0; }
    return h->LoadImage(h->user, r, rect, image);
}
static int32_t tex_store_image(AeRam *r, const AeTexCopyHooks *h, uint32_t rect, uint32_t image) {
    if (!h || !h->StoreImage) { ae_ram_fail(r, "AeTexCopyHooks.StoreImage not provided"); return 0; }
    return h->StoreImage(h->user, r, rect, image);
}

int32_t ae_HeapFree(AeRam *r, uint32_t pointer) {
    uint32_t slot = AE_TEX_HEAP_POINTER_TABLE;
    for (uint32_t index = 0; index < AE_TEX_HEAP_POINTER_COUNT; ++index) {
        if (ae_rd32(r, slot) == pointer) ae_wr32(r, slot, 0u);
        slot += sizeof(uint32_t);
    }
    return 0;
}

static int32_t tex_blend_component(uint32_t source, uint32_t target, int32_t weight) {
    int32_t difference = (int32_t)target - (int32_t)source;
    int32_t blended = (int32_t)source + (difference * weight) / AE_TEX_VARIANT_DIVISOR;
    if (blended < 0) return 0;
    if (blended > 255) return 255;
    return blended;
}

static uint16_t tex_color_variant(uint16_t source_pixel, uint32_t color_rgba,
                                 int32_t weight) {
    uint32_t alpha = source_pixel & AE_TEX_COLOR_ALPHA_MASK;
    if (source_pixel == 0u) return 0u;

    uint32_t source_red = (uint32_t)(source_pixel & 0x1Fu) << 3;
    uint32_t source_green = ((uint32_t)source_pixel >> 2) & AE_TEX_COLOR_COMPONENT_MASK;
    uint32_t source_blue = ((uint32_t)source_pixel >> 7) & AE_TEX_COLOR_COMPONENT_MASK;
    uint32_t target_red = (color_rgba >> (AE_TEX_COLOR_RED_BYTE * 8u)) & 0xFFu;
    uint32_t target_green = (color_rgba >> (AE_TEX_COLOR_GREEN_BYTE * 8u)) & 0xFFu;
    uint32_t target_blue = (color_rgba >> (AE_TEX_COLOR_BLUE_BYTE * 8u)) & 0xFFu;

    uint32_t red = (uint32_t)tex_blend_component(source_red, target_red, weight);
    uint32_t green = (uint32_t)tex_blend_component(source_green, target_green, weight);
    uint32_t blue = (uint32_t)tex_blend_component(source_blue, target_blue, weight);
    return (uint16_t)(alpha |
                      ((blue & AE_TEX_COLOR_COMPONENT_MASK) << 7) |
                      ((green & AE_TEX_COLOR_COMPONENT_MASK) << 2) |
                      ((red & AE_TEX_COLOR_COMPONENT_MASK) >> 3));
}

int32_t ae_AreaTexCopyInit(AeRam *r, const AeTexCopyHooks *hooks,
                           uint32_t rect_address, uint32_t table_index,
                           uint32_t color_rgba, uint32_t unused) {
    (void)unused;
    uint32_t frame_pointer = tex_frame_pointer(hooks, AE_TEX_FRAME_SIZE);
    uint32_t variant_rect = frame_pointer + AE_TEX_STACK_RECT_OFFSET;
    for (uint32_t word = 0; word < AE_TEX_RECT_WORD_COUNT; ++word) {
        uint32_t offset = word * sizeof(uint32_t);
        ae_wr32(r, variant_rect + offset, ae_rd32(r, AE_TEX_VARIANT_RECT + offset));
    }
    if (ae_ram_failed(r)) return 0;

    uint32_t source_image = TEX_COPY_CALL(hooks, HeapAlloc, AE_TEX_HEAP_BUFFER_SIZE, 0u, 0u, 0u);
    if (ae_ram_failed(r)) return 0;
    uint32_t variant_image = TEX_COPY_CALL(hooks, HeapAlloc, AE_TEX_HEAP_BUFFER_SIZE, 0u, 0u, 0u);
    if (ae_ram_failed(r)) return 0;

    (void)tex_store_image(r, hooks, rect_address, source_image);
    if (ae_ram_failed(r)) return 0;

    uint32_t table_slot = AE_TEX_COPY_RECT_TABLE + (table_index << 3);
    ae_wr32(r, table_slot, ae_rd32(r, rect_address));
    ae_wr32(r, table_slot + sizeof(uint32_t), ae_rd32(r, rect_address + sizeof(uint32_t)));
    if (ae_ram_failed(r)) return 0;

    (void)tex_draw_sync(r, hooks, 0);
    if (ae_ram_failed(r)) return 0;
    (void)tex_load_image(r, hooks, variant_rect, source_image);
    if (ae_ram_failed(r)) return 0;

    int32_t rect_width = ae_rds16(r, rect_address + AE_TEX_RECT_WIDTH);
    int32_t rect_height = ae_rds16(r, rect_address + AE_TEX_RECT_HEIGHT);
    int32_t pixel_count = rect_width * rect_height;
    if (ae_ram_failed(r)) return 0;

    for (int32_t weight = (int32_t)AE_TEX_VARIANT_FIRST;
         weight < (int32_t)AE_TEX_VARIANT_COUNT; ++weight) {
        for (int32_t pixel = 0; pixel < pixel_count; ++pixel) {
            uint32_t pixel_offset = (uint32_t)pixel * AE_TEX_PIXEL_SIZE;
            uint16_t source_pixel = (uint16_t)ae_rd16(r, source_image + pixel_offset);
            uint16_t output_pixel = tex_color_variant(source_pixel, color_rgba, weight);
            ae_wr16(r, variant_image + pixel_offset, output_pixel);
        }
        if (ae_ram_failed(r)) return 0;

        int32_t row = (weight + (int32_t)AE_TEX_VARIANT_ROW_WIDTH - 1) /
                      (int32_t)AE_TEX_VARIANT_ROW_WIDTH;
        int32_t column = weight - row * (int32_t)AE_TEX_VARIANT_ROW_WIDTH;
        int32_t x = AE_TEX_VARIANT_X_BASE + column * AE_TEX_VARIANT_X_STEP;
        int32_t y = AE_TEX_VARIANT_Y_BASE + row * AE_TEX_VARIANT_Y_STEP;
        ae_wr16(r, variant_rect + AE_TEX_RECT_X, (uint32_t)x);
        ae_wr16(r, variant_rect + AE_TEX_RECT_Y, (uint32_t)y);

        (void)tex_load_image(r, hooks, variant_rect, variant_image);
        if (ae_ram_failed(r)) return 0;
        (void)tex_draw_sync(r, hooks, 0);
        if (ae_ram_failed(r)) return 0;
    }

    (void)ae_HeapFree(r, source_image);
    (void)ae_HeapFree(r, variant_image);
    return 0;
}

