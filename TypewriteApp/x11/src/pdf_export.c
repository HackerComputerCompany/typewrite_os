#include "pdf_export.h"

#include "tw_core.h"

#include <cairo.h>
#include <cairo-pdf.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A4 in points (ISO 216) */
#define PDF_PAGE_W_PT 595.28
#define PDF_PAGE_H_PT 841.89

static uint32_t pack_xrgb8888(uint32_t rgb) {
    return 0xff000000u | (rgb & 0x00ffffffu);
}

static const uint32_t kBgColors[10] = {
    0x001e1e1e, 0x00f0f0e6, 0x00003060, 0x00603000, 0x00006030,
    0x00600030, 0x00300060, 0x00006060, 0x00606000, 0x00003030,
};

typedef struct {
    int margin_px;
    int gutter_px;
    int paper_x, paper_y, paper_w, paper_h;
    int text_x0, text_y0;
    int cols, rows;
} PdfLayout;

static void pdf_compute_layout(const TwCore *tw, const TwBitmapFont *font, int page_margins, TwPdfLineNum lnm,
                               PdfLayout *L) {
    const int cell_w = (int)font->max_width + 1;
    const int cell_h = (int)font->line_box;

    int gutter_px = 0;
    if (lnm != TW_PDF_LINE_OFF) {
        gutter_px = cell_w * 4 + 12;
        if (gutter_px < cell_w * 5)
            gutter_px = cell_w * 5;
        if (gutter_px < 28)
            gutter_px = 28;
    }

    int margin_px = 0;
    if (page_margins) {
        margin_px = 96;
        if (margin_px < 16)
            margin_px = 16;
    }

    L->cols = tw->cols;
    L->rows = tw->rows;
    L->margin_px = margin_px;
    L->gutter_px = gutter_px;
    L->paper_w = (page_margins ? 2 * margin_px : 0) + gutter_px + L->cols * cell_w;
    L->paper_h = (page_margins ? 2 * margin_px : 0) + L->rows * cell_h;
    L->paper_x = 0;
    L->paper_y = 0;
    L->text_x0 = (page_margins ? margin_px : 0) + gutter_px;
    L->text_y0 = page_margins ? margin_px : 0;
}

static void fill_rect(uint32_t *pix, int w, int h, int x0, int y0, int rw, int rh, uint32_t col) {
    if (rw <= 0 || rh <= 0)
        return;
    for (int y = 0; y < rh; y++) {
        int yy = y0 + y;
        if ((unsigned)yy >= (unsigned)h)
            continue;
        uint32_t *row = pix + yy * w;
        for (int x = 0; x < rw; x++) {
            int xx = x0 + x;
            if ((unsigned)xx >= (unsigned)w)
                continue;
            row[xx] = col;
        }
    }
}

static void draw_text_mono(uint32_t *pix, int w, int h, int x, int baselineY, const TwBitmapFont *font,
                           const char *s, uint32_t fg, uint32_t bg) {
    if (!s || !font)
        return;
    int cell_w = (int)font->max_width + 1;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\n') {
            baselineY += (int)font->line_box;
            x = 0;
            continue;
        }
        tw_uefi_font_draw_char(font, pix, w, h, x, baselineY, c, fg, bg);
        x += cell_w;
    }
}

static uint32_t line_number_ink(int bg_idx) {
    return (bg_idx % 10 == 1) ? pack_xrgb8888(0x606e8au) : pack_xrgb8888(0x8c96afu);
}

static void draw_page_footer(uint32_t *pix, int w, int h, const TwBitmapFont *font, int bg_idx,
                             const PdfLayout *lay, int view_rows, int cur_page0, int n_pages) {
    if (n_pages < 1)
        return;
    const int cell_w = (int)font->max_width + 1;
    const int cell_h = (int)font->line_box;
    const uint32_t paper_bg = pack_xrgb8888(kBgColors[bg_idx % 10]);
    uint32_t stamp = line_number_ink(bg_idx);

    char f[48];
    snprintf(f, sizeof(f), "Page %d of %d", cur_page0 + 1, n_pages);
    int fw = (int)strlen(f) * cell_w;

    int text_bot = lay->text_y0 + view_rows * cell_h;
    int paper_bot = lay->paper_y + lay->paper_h;
    int fx, fy;

    if (paper_bot - text_bot >= cell_h + 2) {
        fy = text_bot + (paper_bot - text_bot - cell_h) / 2 + (int)font->max_top;
        fx = lay->paper_x + lay->paper_w - lay->margin_px - fw;
    } else {
        fy = lay->text_y0 + (int)font->max_top;
        fx = lay->paper_x + lay->paper_w - fw - 4;
        if (fx < lay->text_x0)
            fx = lay->text_x0;
    }

    draw_text_mono(pix, w, h, fx, fy, font, f, stamp, paper_bg);
}

/* Full-page raster (no typewriter transform, no cursor) for print/PDF. */
static void render_pdf_page(uint32_t *pix, int w, int h, const TwCore *tw, const TwBitmapFont *font, int bg_idx,
                            const PdfLayout *lay, int page_margins, TwPdfLineNum line_num_mode, int cur_page0,
                            int n_pages) {
    const uint32_t paper_bg = pack_xrgb8888(kBgColors[bg_idx % 10]);
    const uint32_t outer_bg = page_margins ? pack_xrgb8888(0x0a0a0au) : paper_bg;
    const uint32_t fg = (bg_idx == 1) ? pack_xrgb8888(0x1e1e1eu) : pack_xrgb8888(0xf0f0e6u);

    const int cell_w = (int)font->max_width + 1;
    const int cell_h = (int)font->line_box;
    const int cols = (tw->cols < lay->cols) ? tw->cols : lay->cols;
    const int rows = (tw->rows < lay->rows) ? tw->rows : lay->rows;

    for (int i = 0; i < w * h; i++)
        pix[i] = outer_bg;

    if (page_margins)
        fill_rect(pix, w, h, lay->paper_x, lay->paper_y, lay->paper_w, lay->paper_h, paper_bg);

    if (line_num_mode != TW_PDF_LINE_OFF && lay->gutter_px > 0) {
        uint32_t ln_fg = line_number_ink(bg_idx);
        for (int sy = 0; sy < rows; sy++) {
            int br = sy;
            int val = (line_num_mode == TW_PDF_LINE_ASC) ? (br + 1) : (tw->rows - br);
            char nb[16];
            snprintf(nb, sizeof(nb), "%d", val);
            int baselineY = lay->text_y0 + sy * cell_h + (int)font->max_top;
            int nx = lay->text_x0 - (int)strlen(nb) * cell_w - 4;
            if (nx < lay->paper_x + lay->margin_px + 2)
                nx = lay->paper_x + lay->margin_px + 2;
            draw_text_mono(pix, w, h, nx, baselineY, font, nb, ln_fg, paper_bg);
        }
    }

    for (int sy = 0; sy < rows; sy++) {
        int br = sy;
        for (int x = 0; x < cols; x++) {
            unsigned char c = ' ';
            if (br >= 0 && br < tw->rows)
                c = (unsigned char)tw->cells[br * tw->cols + x];
            int px = lay->text_x0 + x * cell_w;
            int baselineY = lay->text_y0 + sy * cell_h + (int)font->max_top;
            tw_uefi_font_draw_char(font, pix, w, h, px, baselineY, c, fg, paper_bg);
        }
    }

    draw_page_footer(pix, w, h, font, bg_idx, lay, rows, cur_page0, n_pages);
}

int tw_export_pdf(const TwDoc *doc, const char *pdf_path, const TwBitmapFont *font, const TwPdfOpts *opts) {
    if (!doc || !pdf_path || !font || !opts || doc->n_pages < 1)
        return -1;
    (void)opts->cols_margined; /* parity with UI (F7); page raster uses each page's tw->cols */

    cairo_surface_t *pdf = cairo_pdf_surface_create(pdf_path, PDF_PAGE_W_PT, PDF_PAGE_H_PT);
    if (cairo_surface_status(pdf) != CAIRO_STATUS_SUCCESS)
        return -1;

    for (int p = 0; p < doc->n_pages; p++) {
        const TwCore *tw = &doc->pages[p];
        PdfLayout lay;
        pdf_compute_layout(tw, font, opts->page_margins, opts->line_num_mode, &lay);

        int w = lay.paper_w;
        int h = lay.paper_h;
        if (w <= 0 || h <= 0) {
            cairo_surface_destroy(pdf);
            return -1;
        }

        uint32_t *buf = (uint32_t *)calloc((size_t)w * (size_t)h, sizeof(uint32_t));
        if (!buf) {
            cairo_surface_destroy(pdf);
            return -1;
        }

        render_pdf_page(buf, w, h, tw, font, opts->bg_idx, &lay, opts->page_margins, opts->line_num_mode, p,
                        doc->n_pages);

        int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w);
        if (stride != w * (int)sizeof(uint32_t)) {
            /* Unusual stride; allocate Cairo buffer and copy. */
            unsigned char *tmp = (unsigned char *)calloc(1, (size_t)stride * (size_t)h);
            if (!tmp) {
                free(buf);
                cairo_surface_destroy(pdf);
                return -1;
            }
            for (int y = 0; y < h; y++)
                memcpy(tmp + (size_t)y * (size_t)stride, buf + (size_t)y * (size_t)w, (size_t)w * sizeof(uint32_t));
            free(buf);
            buf = (uint32_t *)tmp;
        }

        cairo_surface_t *img =
            cairo_image_surface_create_for_data((unsigned char *)buf, CAIRO_FORMAT_ARGB32, w, h, stride);
        if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
            free(buf);
            cairo_surface_destroy(pdf);
            return -1;
        }

        cairo_t *cr = cairo_create(pdf);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_paint(cr);

        double sc = fmin(PDF_PAGE_W_PT / (double)w, PDF_PAGE_H_PT / (double)h);
        cairo_translate(cr, (PDF_PAGE_W_PT - (double)w * sc) / 2.0, (PDF_PAGE_H_PT - (double)h * sc) / 2.0);
        cairo_scale(cr, sc, sc);
        cairo_set_source_surface(cr, img, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);

        cairo_surface_destroy(img);
        free(buf);

        cairo_surface_show_page(pdf);
    }

    cairo_status_t st = cairo_surface_status(pdf);
    cairo_surface_destroy(pdf);
    return (st == CAIRO_STATUS_SUCCESS) ? 0 : -1;
}
