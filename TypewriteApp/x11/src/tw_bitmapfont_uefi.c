#include "tw_bitmapfont_uefi.h"

/*
 * Reuse the same generated font headers as the UEFI app.
 * They include <efi.h> for UINT8/UINT32, so the X11 build adds gnu-efi inc dirs.
 */
#include "../../fonts/virgil.h"
#include "../../fonts/helvetica.h"
#include "../../fonts/special_elite.h"
#include "../../fonts/courier_prime.h"
#include "../../fonts/vt323.h"
#include "../../fonts/press_start_2p.h"
#include "../../fonts/ibm_plex_mono.h"
#include "../../fonts/share_tech_mono.h"

static const TwBitmapFont g_fonts[] = {
    {virgil_offsets, virgil_widths, virgil_advances, virgil_bitmap_top, virgil_bitmap, sizeof(virgil_bitmap),
     VIRGIL_ASC_MIN, VIRGIL_ASC_MAX, VIRGIL_LINE_BOX, VIRGIL_MAX_TOP, VIRGIL_HEIGHT, VIRGIL_MAX_WIDTH, "Virgil"},
    {helvetica_offsets, helvetica_widths, helvetica_advances, helvetica_bitmap_top, helvetica_bitmap, sizeof(helvetica_bitmap),
     HELVETICA_ASC_MIN, HELVETICA_ASC_MAX, HELVETICA_LINE_BOX, HELVETICA_MAX_TOP, HELVETICA_HEIGHT, HELVETICA_MAX_WIDTH, "Inter"},
    {special_elite_offsets, special_elite_widths, special_elite_advances, special_elite_bitmap_top, special_elite_bitmap, sizeof(special_elite_bitmap),
     SPECIAL_ELITE_ASC_MIN, SPECIAL_ELITE_ASC_MAX, SPECIAL_ELITE_LINE_BOX, SPECIAL_ELITE_MAX_TOP, SPECIAL_ELITE_HEIGHT, SPECIAL_ELITE_MAX_WIDTH, "Special Elite"},
    {courier_prime_offsets, courier_prime_widths, courier_prime_advances, courier_prime_bitmap_top, courier_prime_bitmap, sizeof(courier_prime_bitmap),
     COURIER_PRIME_ASC_MIN, COURIER_PRIME_ASC_MAX, COURIER_PRIME_LINE_BOX, COURIER_PRIME_MAX_TOP, COURIER_PRIME_HEIGHT, COURIER_PRIME_MAX_WIDTH, "Courier Prime"},
    {vt323_offsets, vt323_widths, vt323_advances, vt323_bitmap_top, vt323_bitmap, sizeof(vt323_bitmap),
     VT323_ASC_MIN, VT323_ASC_MAX, VT323_LINE_BOX, VT323_MAX_TOP, VT323_HEIGHT, VT323_MAX_WIDTH, "VT323"},
    {press_start_2p_offsets, press_start_2p_widths, press_start_2p_advances, press_start_2p_bitmap_top, press_start_2p_bitmap, sizeof(press_start_2p_bitmap),
     PRESS_START_2P_ASC_MIN, PRESS_START_2P_ASC_MAX, PRESS_START_2P_LINE_BOX, PRESS_START_2P_MAX_TOP, PRESS_START_2P_HEIGHT, PRESS_START_2P_MAX_WIDTH, "Press Start 2P"},
    {ibm_plex_mono_offsets, ibm_plex_mono_widths, ibm_plex_mono_advances, ibm_plex_mono_bitmap_top, ibm_plex_mono_bitmap, sizeof(ibm_plex_mono_bitmap),
     IBM_PLEX_MONO_ASC_MIN, IBM_PLEX_MONO_ASC_MAX, IBM_PLEX_MONO_LINE_BOX, IBM_PLEX_MONO_MAX_TOP, IBM_PLEX_MONO_HEIGHT, IBM_PLEX_MONO_MAX_WIDTH, "IBM Plex Mono"},
    {share_tech_mono_offsets, share_tech_mono_widths, share_tech_mono_advances, share_tech_mono_bitmap_top, share_tech_mono_bitmap, sizeof(share_tech_mono_bitmap),
     SHARE_TECH_MONO_ASC_MIN, SHARE_TECH_MONO_ASC_MAX, SHARE_TECH_MONO_LINE_BOX, SHARE_TECH_MONO_MAX_TOP, SHARE_TECH_MONO_HEIGHT, SHARE_TECH_MONO_MAX_WIDTH, "Share Tech Mono"},
};

int tw_uefi_font_count(void) {
    return (int)(sizeof(g_fonts) / sizeof(g_fonts[0]));
}

const TwBitmapFont *tw_uefi_font_get(int idx) {
    if (idx < 0)
        idx = 0;
    if (idx >= tw_uefi_font_count())
        idx = tw_uefi_font_count() - 1;
    return &g_fonts[idx];
}

static inline void put_px(uint32_t *pix, int w, int h, int x, int y, uint32_t col) {
    if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h)
        return;
    pix[y * w + x] = col;
}

void tw_uefi_font_draw_char(const TwBitmapFont *f, uint32_t *pix, int w, int h,
                            int x, int baselineY, unsigned char ch,
                            uint32_t fg_xrgb, uint32_t bg_xrgb) {
    if (!f || !pix)
        return;
    if (ch < f->asc_min || ch > f->asc_max)
        return;

    uint32_t idx = (uint32_t)ch - f->asc_min;
    uint32_t off = f->offsets[idx];
    uint32_t next = (idx < 94) ? f->offsets[idx + 1] : (uint32_t)f->bitmap_size;
    uint32_t glyph_bytes = next - off;
    if (glyph_bytes == 0 || (size_t)(off + glyph_bytes) > f->bitmap_size)
        return;

    uint32_t gw = f->widths[idx];
    if (gw < 1)
        gw = 1;
    uint32_t rowB = (gw + 7) / 8;
    if (rowB < 1)
        rowB = 1;
    uint32_t gh = glyph_bytes / rowB;
    if (gh < 1)
        gh = 1;
    if (gh > f->max_glyph_height)
        gh = f->max_glyph_height;
    uint32_t bmpTop = f->bitmap_top[idx];

    for (uint32_t py = 0; py < gh; py++) {
        int dy = baselineY - (int)bmpTop + (int)py;
        if (dy < 0 || dy >= h)
            continue;
        uint32_t rowOff = off + py * rowB;
        for (uint32_t byteIdx = 0; byteIdx < rowB && (size_t)(rowOff + byteIdx) < f->bitmap_size; byteIdx++) {
            uint8_t byte = f->bitmap[rowOff + byteIdx];
            for (uint32_t bit = 0; bit < 8; bit++) {
                uint32_t px = byteIdx * 8 + bit;
                if (px >= gw)
                    break;
                int dx = x + (int)px;
                uint32_t col = (byte & (1u << bit)) ? fg_xrgb : bg_xrgb; /* LSB-first */
                put_px(pix, w, h, dx, dy, col);
            }
        }
    }
}

