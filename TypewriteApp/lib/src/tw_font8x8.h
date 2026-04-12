#pragma once

#include <stdint.h>

/* Returns pointer to 8 bytes (one per row), bit7 = leftmost pixel. */
const uint8_t *tw_font8x8_glyph(unsigned char c);

