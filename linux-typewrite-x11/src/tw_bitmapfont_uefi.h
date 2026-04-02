#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    const uint32_t *offsets;
    const uint8_t *widths;
    const uint8_t *advances;
    const uint8_t *bitmap_top;
    const uint8_t *bitmap;
    size_t bitmap_size;
    uint32_t asc_min;
    uint32_t asc_max;
    uint32_t line_box;
    uint32_t max_top;
    uint32_t max_glyph_height;
    uint32_t max_width;
    const char *name;
} TwBitmapFont;

/* Returns number of bundled faces (UEFI fonts). */
int tw_uefi_font_count(void);
const TwBitmapFont *tw_uefi_font_get(int idx);

/* Draw a single ASCII char at baseline origin (x, baselineY). */
void tw_uefi_font_draw_char(const TwBitmapFont *f, uint32_t *pix, int w, int h,
                            int x, int baselineY, unsigned char ch,
                            uint32_t fg_xrgb, uint32_t bg_xrgb);

