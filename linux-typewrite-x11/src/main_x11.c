#include "tw_core.h"
#include "tw_doc.h"
#include "tw_bitmapfont_uefi.h"
#include "pdf_export.h"
#include "tw_x11_settings.h"
#include "tw_sound.h"

#include <SDL2/SDL.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

static uint32_t pack_xrgb8888(uint32_t rgb) {
    return 0xff000000u | (rgb & 0x00ffffffu);
}

static const uint32_t kBgColors[10] = {
    0x001e1e1e, /* Dark */
    0x00f0f0e6, /* Cream/Paper */
    0x00003060, /* Blue */
    0x00603000, /* Brown */
    0x00006030, /* Green */
    0x00600030, /* Maroon */
    0x00300060, /* Purple */
    0x00006060, /* Teal */
    0x00606000, /* Olive */
    0x00003030, /* Navy */
};

static const char *const kBgLabels[10] = {
    "Dark", "Cream", "Blue", "Brown", "Green", "Maroon", "Purple", "Teal", "Olive", "Navy",
};

/* UEFI parity: active column count with Letter-style margins (see uefi-app/main.c). */
#define COLS_MARGINED_MIN  50
#define COLS_MARGINED_MAX  65
#define COLS_MARGINED_DEFAULT 58
#define PAGE_COLS_FULL_WIDTH 80 /* uefi-app/main.c active cols when margins off */

/* Save after this many ms with no document edit (typing, delete, newline, etc.). */
#define AUTOSAVE_IDLE_MS (10u * 1000u)

#define STATUS_PULSE_NUM 6

static const uint32_t k_status_pulse_ms[STATUS_PULSE_NUM] = {
    1u * 60u * 1000u,  /* 1 min (default) */
    5u * 60u * 1000u,
    10u * 60u * 1000u,
    15u * 60u * 1000u,
    30u * 60u * 1000u,
    60u * 60u * 1000u, /* 1 hr */
};

static const char *const k_status_pulse_label[STATUS_PULSE_NUM] = {
    "1 min", "5 min", "10 min", "15 min", "30 min", "1 hr",
};

#define HELP_MENU_ROWS 24

/* =============================================================================
 * SECTION: LAYOUT TYPES
 * LineNumMode, ViewLayout, CursorMode, FooterLayout
 * ============================================================================= */

typedef enum {
    LINE_NUM_OFF = 0,
    LINE_NUM_ASC,  /* 1..n top→bottom: line index on the page */
    LINE_NUM_DESC, /* n..1 top→bottom: rows left from here through buffer end */
    LINE_NUM_MODE_NUM
} LineNumMode;

typedef struct {
    int margin_left_px;
    int margin_right_px;
    int margin_top_px;
    int margin_bottom_px;
    int gutter_px;
    int paper_x, paper_y, paper_w, paper_h;
    int text_x0, text_y0;
    int cols, rows;
} ViewLayout;

/* =============================================================================
 * SECTION: LAYOUT COMPUTATION
 * compute_view_layout, footer_layout, toast_top_baseline_y
 * ============================================================================= */

static void compute_view_layout(int win_w, int win_h, const TwBitmapFont *font, int page_margins,
                                LineNumMode line_num_mode, int cols_margined, ViewLayout *L) {
    const int cell_w = (int)font->max_width + 1;
    const int cell_h = (int)font->line_box;

    int gutter_px = 0;
    if (line_num_mode != LINE_NUM_OFF) {
        /* Room for multi-digit labels (asc index or remaining count). */
        gutter_px = cell_w * 4 + 12;
        if (gutter_px < cell_w * 5)
            gutter_px = cell_w * 5;
        if (gutter_px < 28)
            gutter_px = 28;
    }

    int margin_px = 0;
    if (page_margins) {
        /* ~1 inch inset at 96 px/in; may shrink on small windows to keep the grid visible. */
        margin_px = 96;
        if (margin_px > win_w / 6)
            margin_px = win_w / 6;
        if (margin_px > win_h / 6)
            margin_px = win_h / 6;
        if (margin_px < 16)
            margin_px = 16;
        while (margin_px > 0 && (win_w < 2 * margin_px + gutter_px + cell_w ||
                                win_h < 2 * margin_px + cell_h))
            margin_px -= 2;
        if (margin_px < 0)
            margin_px = 0;

        int max_cols_fit = (win_w - 2 * margin_px - gutter_px) / cell_w;
        if (max_cols_fit < 1)
            max_cols_fit = 1;
        int cols = cols_margined <= max_cols_fit ? cols_margined : max_cols_fit;
        if (cols < 1)
            cols = 1;

        int max_rows = (win_h - 2 * margin_px) / cell_h;
        if (max_rows < 1)
            max_rows = 1;

        L->paper_w = 2 * margin_px + gutter_px + cols * cell_w;
        L->paper_h = 2 * margin_px + max_rows * cell_h;
        L->paper_x = (win_w > L->paper_w) ? (win_w - L->paper_w) / 2 : 0;
        L->paper_y = (win_h > L->paper_h) ? (win_h - L->paper_h) / 2 : 0;
        L->text_x0 = L->paper_x + margin_px + gutter_px;
        L->text_y0 = L->paper_y + margin_px;
        L->cols = cols;
        L->rows = max_rows;
    } else {
        int max_cols = (win_w - gutter_px) / cell_w;
        if (max_cols > PAGE_COLS_FULL_WIDTH)
            max_cols = PAGE_COLS_FULL_WIDTH;
        if (max_cols < 1)
            max_cols = 1;

        int max_rows = win_h / cell_h;
        if (max_rows < 1)
            max_rows = 1;

        L->paper_w = gutter_px + max_cols * cell_w;
        L->paper_h = max_rows * cell_h;
        L->paper_x = (win_w > L->paper_w) ? (win_w - L->paper_w) / 2 : 0;
        L->paper_y = (win_h > L->paper_h) ? (win_h - L->paper_h) / 2 : 0;
        L->text_x0 = L->paper_x + gutter_px;
        L->text_y0 = L->paper_y;
        L->cols = max_cols;
        L->rows = max_rows;
    }

    L->margin_left_px = margin_px;
    L->margin_right_px = margin_px;
    L->margin_top_px = margin_px;
    L->margin_bottom_px = margin_px;
    L->gutter_px = gutter_px;
}

typedef enum {
    CURSOR_BAR = 0,
    CURSOR_BAR_BLINK,
    CURSOR_BLOCK,
    CURSOR_BLOCK_BLINK,
    CURSOR_HIDDEN,
    CURSOR_MODE_NUM
} CursorMode;

static uint64_t mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static void mark_document_edited(int *dirty_flag, uint64_t *last_edit_ms) {
    *dirty_flag = 1;
    *last_edit_ms = mono_ms();
}

/* =============================================================================
 * SECTION: DRAWING HELPERS
 * fill_rect, fill_rect_soft_edge, draw_text_mono
 * ============================================================================= */

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

/* Footer band: same vertical band as "Page N of M"; action toasts sit on the left. */
typedef struct {
    int fy;       /* baseline Y for stamp text */
    int page_fx; /* left edge of "Page …" */
} FooterLayout;

/* Top margin band (between paper top and first text row); status pulse baseline. */
static int toast_top_baseline_y(const ViewLayout *lay, const TwBitmapFont *font) {
    const int cell_h = (int)font->line_box;
    int paper_top = lay->paper_y;
    int text_top = lay->text_y0;
    int fy;

    if (text_top - paper_top >= cell_h + 2) {
        fy = paper_top + (text_top - paper_top - cell_h) / 2 + (int)font->max_top;
    } else {
        fy = paper_top + (int)font->max_top;
        if (text_top > paper_top && fy + cell_h > text_top)
            fy = text_top - cell_h + (int)font->max_top;
    }
    if (fy < (int)font->max_top + 1)
        fy = (int)font->max_top + 1;
    return fy;
}

static void footer_layout(const ViewLayout *lay, const TwBitmapFont *font, int view_rows, int cur_page0, int n_pages,
                          FooterLayout *out) {
    const int cell_w = (int)font->max_width + 1;
    const int cell_h = (int)font->line_box;
    char f[48];
    snprintf(f, sizeof(f), "Page %d of %d", cur_page0 + 1, n_pages);
    int stamp_w = (int)strlen(f) * cell_w;

    int text_bot = lay->text_y0 + view_rows * cell_h;
    int paper_bot = lay->paper_y + lay->paper_h;
    int fx, fy;

    if (paper_bot - text_bot >= cell_h + 2) {
        fy = text_bot + (paper_bot - text_bot - cell_h) / 2 + (int)font->max_top;
        fx = lay->paper_x + lay->paper_w - lay->margin_right_px - stamp_w;
    } else {
        fy = lay->text_y0 + (int)font->max_top;
        fx = lay->paper_x + lay->paper_w - stamp_w - 4;
        if (fx < lay->text_x0)
            fx = lay->text_x0;
    }
    out->fy = fy;
    out->page_fx = fx;
}

#define TOAST_MAX 220
#define TOAST_HOLD_MS 2400u
#define TOAST_FADE_MS 4800u

/*
 * Learn typing pace from gaps between characters (printable keys, Tab, Enter).
 * Long pauses do not skew the average; toast uses a snapshot ms/char when shown.
 */
typedef struct {
    uint64_t last_char_ms;
    float ema_ms_per_char; /* 0 = not enough data yet */
} TypingPace;

#define TYPING_PAUSE_MS 2800u
#define TYPING_MIN_GAP_MS 40u
#define TYPING_GAP_CAP_MS 720u
#define TYPING_DEFAULT_MS_PER_CHAR 165u

static void typing_pace_note_char(TypingPace *p, uint64_t now) {
    if (!p)
        return;
    if (p->last_char_ms != 0u && now > p->last_char_ms) {
        uint64_t dt = now - p->last_char_ms;
        if (dt >= TYPING_MIN_GAP_MS && dt < TYPING_PAUSE_MS) {
            float dtf = (float)dt;
            if (dtf > (float)TYPING_GAP_CAP_MS)
                dtf = (float)TYPING_GAP_CAP_MS;
            if (p->ema_ms_per_char <= 0.f)
                p->ema_ms_per_char = dtf;
            else
                p->ema_ms_per_char = 0.88f * p->ema_ms_per_char + 0.12f * dtf;
        }
    }
    p->last_char_ms = now;
}

static uint32_t typing_pace_toast_ms_per_char(const TypingPace *p) {
    float m = (p && p->ema_ms_per_char > 0.f) ? p->ema_ms_per_char : (float)TYPING_DEFAULT_MS_PER_CHAR;
    if (m < 40.f)
        m = 40.f;
    if (m > 600.f)
        m = 600.f;
    return (uint32_t)(m + 0.5f);
}

typedef struct {
    char text[TOAST_MAX];
    size_t len;
    uint64_t t0_ms;
    uint32_t ms_per_char;
    int top_band; /* 1: status pulse in top margin; 0: footer (left of page stamp) */
    int active;
} ToastState;

static void toast_show(ToastState *ts, const TypingPace *pace, const char *msg, int top_band) {
    if (!ts || !msg)
        return;
    snprintf(ts->text, sizeof(ts->text), "%s", msg);
    ts->len = strnlen(ts->text, sizeof(ts->text));
    ts->t0_ms = mono_ms();
    ts->ms_per_char = typing_pace_toast_ms_per_char(pace);
    if (ts->ms_per_char < 40u)
        ts->ms_per_char = 40u;
    ts->top_band = top_band ? 1 : 0;
    ts->active = (ts->len > 0);
}

static uint32_t lerp_ink_to_paper(uint32_t ink_argb, uint32_t paper_argb, float t) {
    t = fminf(1.f, fmaxf(0.f, t));
    float ir = (float)((ink_argb >> 16) & 0xff), ig = (float)((ink_argb >> 8) & 0xff), ib = (float)(ink_argb & 0xff);
    float pr = (float)((paper_argb >> 16) & 0xff), pg = (float)((paper_argb >> 8) & 0xff), pb = (float)(paper_argb & 0xff);
    unsigned r = (unsigned)(ir + (pr - ir) * t + 0.5f);
    unsigned g = (unsigned)(ig + (pg - ig) * t + 0.5f);
    unsigned b = (unsigned)(ib + (pb - ib) * t + 0.5f);
    if (r > 255u)
        r = 255u;
    if (g > 255u)
        g = 255u;
    if (b > 255u)
        b = 255u;
    return pack_xrgb8888((r << 16) | (g << 8) | b);
}

/* =============================================================================
 * SECTION: TOAST & TYPING PACE
 * ToastState, TypingPace, toast_show, typing_pace_note_char, draw_toast
 * ============================================================================= */

static void draw_toast(uint32_t *pix, int w, int h, const TwBitmapFont *font, int bg_idx, const ViewLayout *lay,
                       int view_rows, int cur_page0, int n_pages, ToastState *toast, uint64_t now) {
    if (!toast || !toast->active || toast->len == 0 || !font)
        return;

    const uint32_t paper_bg = pack_xrgb8888(kBgColors[bg_idx % 10]);
    const uint32_t toast_bg = paper_bg;
    const uint32_t ink_full = line_number_ink(bg_idx);

    uint64_t elapsed = (now >= toast->t0_ms) ? (now - toast->t0_ms) : 0u;
    uint32_t mpc = toast->ms_per_char ? toast->ms_per_char : TYPING_DEFAULT_MS_PER_CHAR;
    uint64_t typing_ms = (toast->len <= 1) ? 0u : (uint64_t)(toast->len - 1u) * (uint64_t)mpc;

    size_t nvis;
    uint32_t fg = ink_full;

    if (elapsed < typing_ms) {
        nvis = 1u + (size_t)(elapsed / (uint64_t)mpc);
        if (nvis > toast->len)
            nvis = toast->len;
    } else {
        nvis = toast->len;
        uint64_t post = elapsed - typing_ms;
        if (post < TOAST_HOLD_MS) {
            fg = ink_full;
        } else if (post < TOAST_HOLD_MS + TOAST_FADE_MS) {
            float u = (float)(post - TOAST_HOLD_MS) / (float)TOAST_FADE_MS;
            fg = lerp_ink_to_paper(ink_full, toast_bg, u);
        } else {
            toast->active = 0;
            return;
        }
    }

    const int cell_w = (int)font->max_width + 1;
    int tx = lay->paper_x + (lay->margin_left_px > 0 ? lay->margin_left_px : 4);
    int ty;
    int max_right;

    if (toast->top_band) {
        ty = toast_top_baseline_y(lay, font);
        max_right = lay->paper_x + lay->paper_w - (lay->margin_right_px > 0 ? lay->margin_right_px : 4) - 4;
    } else {
        FooterLayout fl;
        footer_layout(lay, font, view_rows, cur_page0, n_pages, &fl);
        ty = fl.fy;
        max_right = fl.page_fx - 8;
        if (max_right <= tx + cell_w * 4)
            max_right = tx + cell_w * 40; /* narrow overlap: still show something */
    }
    if (max_right <= tx + cell_w * 4)
        max_right = tx + cell_w * 48;
    int max_chars = (max_right - tx) / cell_w;
    if (max_chars < 4)
        max_chars = 4;

    char buf[TOAST_MAX + 8];
    size_t copy = nvis;
    if (copy > sizeof(buf) - 8)
        copy = sizeof(buf) - 8;
    memcpy(buf, toast->text, copy);
    buf[copy] = 0;

    if ((int)strlen(buf) > max_chars) {
        if (max_chars > 3) {
            buf[(size_t)max_chars - 3u] = 0;
            size_t L = strlen(buf);
            if (L + 3u < sizeof(buf))
                memcpy(buf + L, "...", 4);
        } else {
            memcpy(buf, "...", 4);
        }
    }

    draw_text_mono(pix, w, h, tx, ty, font, buf, fg, toast_bg);
}

/* Map screen row sy -> buffer row; -1 = blank row above the growing document. */
static int typewriter_buf_row_for_sy(int sy, int cursor_y, int view_rows) {
    int V = view_rows;
    if (V <= 0)
        return -1;
    int start_sy = (cursor_y + 1 <= V) ? (V - (cursor_y + 1)) : 0;
    if (sy < start_sy)
        return -1;
    int buf0 = (cursor_y + 1 <= V) ? 0 : (cursor_y - (V - 1));
    return buf0 + (sy - start_sy);
}

/* Screen row where the cursor sits (always bottom row of the viewport when typewriter mode). */
static int typewriter_sy_for_cursor(int cursor_y, int view_rows) {
    int V = view_rows;
    if (V <= 0)
        return 0;
    int start_sy = (cursor_y + 1 <= V) ? (V - (cursor_y + 1)) : 0;
    int buf0 = (cursor_y + 1 <= V) ? 0 : (cursor_y - (V - 1));
    return start_sy + (cursor_y - buf0);
}

static void draw_page_footer(uint32_t *pix, int w, int h, const TwBitmapFont *font, int bg_idx,
                             const ViewLayout *lay, int view_rows, int cur_page0, int n_pages) {
    if (n_pages < 1)
        return;
    const uint32_t paper_bg = pack_xrgb8888(kBgColors[bg_idx % 10]);
    uint32_t stamp = line_number_ink(bg_idx);

    FooterLayout fl;
    footer_layout(lay, font, view_rows, cur_page0, n_pages, &fl);

    char f[48];
    snprintf(f, sizeof(f), "Page %d of %d", cur_page0 + 1, n_pages);
    draw_text_mono(pix, w, h, fl.page_fx, fl.fy, font, f, stamp, paper_bg);
}

/* =============================================================================
 * SECTION: RENDER
 * render, typewriter mapping functions
 * ============================================================================= */

static void render(uint32_t *pix, int w, int h, const TwCore *tw, const TwBitmapFont *font,
                   CursorMode cursor_mode, int cursor_visible, int bg_idx, const ViewLayout *lay,
                   int page_margins, LineNumMode line_num_mode, int typewriter_mode, int cur_page0,
                   int n_pages) {
    const uint32_t paper_bg = pack_xrgb8888(kBgColors[bg_idx % 10]);
    const uint32_t outer_bg = page_margins ? pack_xrgb8888(0x0a0a0au) : paper_bg;
    const uint32_t fg = (bg_idx == 1) ? pack_xrgb8888(0x1e1e1eu) : pack_xrgb8888(0xf0f0e6u);
    const uint32_t cur = fg;
    const uint32_t typewriter_rule = pack_xrgb8888(0xc01818u);

    const int cell_w = (int)font->max_width + 1;
    const int cell_h = (int)font->line_box;
    const int cols = (tw->cols < lay->cols) ? tw->cols : lay->cols;
    const int rows = (tw->rows < lay->rows) ? tw->rows : lay->rows;

    for (int i = 0; i < w * h; i++)
        pix[i] = outer_bg;

    if (page_margins)
        fill_rect(pix, w, h, lay->paper_x, lay->paper_y, lay->paper_w, lay->paper_h, paper_bg);

    /*
     * Typewriter mode + page margins: fill the area above text_y0 with outer_bg
     * to avoid the old fixed top margin strip staying paper-colored.
     * The floating margin band will be drawn after text rows.
     */
    if (page_margins && typewriter_mode) {
        fill_rect(pix, w, h, lay->paper_x, lay->paper_y, lay->paper_w, lay->text_y0 - lay->paper_y, outer_bg);
    }

    /*
     * Typewriter mode: screen rows above the document (br < 0) use outer_bg like the side pillars.
     * The band from paper top to text_y0 stays paper-colored (~1 inch Letter top margin).
     */

    if (line_num_mode != LINE_NUM_OFF && lay->gutter_px > 0) {
        uint32_t ln_fg = line_number_ink(bg_idx);
        for (int sy = 0; sy < rows; sy++) {
            int br = typewriter_mode ? typewriter_buf_row_for_sy(sy, tw->cy, rows) : sy;
            if (br < 0)
                continue;
            int val = (line_num_mode == LINE_NUM_ASC) ? (br + 1) : (tw->rows - br);
            char nb[16];
            snprintf(nb, sizeof(nb), "%d", val);
            int baselineY = lay->text_y0 + sy * cell_h + (int)font->max_top;
            int nx = lay->text_x0 - (int)strlen(nb) * cell_w - 4;
            if (nx < lay->paper_x + lay->margin_left_px + 2)
                nx = lay->paper_x + lay->margin_left_px + 2;
            draw_text_mono(pix, w, h, nx, baselineY, font, nb, ln_fg, paper_bg);
        }
    }

    for (int sy = 0; sy < rows; sy++) {
        int br = typewriter_mode ? typewriter_buf_row_for_sy(sy, tw->cy, rows) : sy;
        if (typewriter_mode && br < 0) {
            int y0 = lay->text_y0 + sy * cell_h;
            fill_rect(pix, w, h, lay->paper_x, y0, lay->paper_w, cell_h, outer_bg);
            continue;
        }
        for (int x = 0; x < cols; x++) {
            unsigned char c = ' ';
            if (br >= 0 && br < tw->rows)
                c = (unsigned char)tw->cells[br * tw->cols + x];
            int px = lay->text_x0 + x * cell_w;
            int baselineY = lay->text_y0 + sy * cell_h + (int)font->max_top;
            tw_uefi_font_draw_char(font, pix, w, h, px, baselineY, c, fg, paper_bg);
        }
    }

    /* Typewriter mode + page margins: floating margin band */
    if (page_margins && typewriter_mode && rows > 0) {
        int first_sy = -1;
        for (int sy = 0; sy < rows; sy++) {
            if (typewriter_buf_row_for_sy(sy, tw->cy, rows) >= 0) {
                first_sy = sy;
                break;
            }
        }
        if (first_sy >= 0) {
            int y_first = lay->text_y0 + first_sy * cell_h;
            int y0 = (y_first - lay->margin_top_px < lay->paper_y) ? lay->paper_y : y_first - lay->margin_top_px;
            if (y0 > lay->paper_y) {
                fill_rect(pix, w, h, lay->paper_x, y0, lay->paper_w, y_first - y0, paper_bg);
            }
        }
    }

    if (cursor_mode != CURSOR_HIDDEN && cursor_visible) {
        int cx = tw->cx;
        int cy = tw->cy;
        if ((unsigned)cx < (unsigned)cols && (unsigned)cy < (unsigned)tw->rows) {
            int sy = typewriter_mode ? typewriter_sy_for_cursor(cy, rows) : cy;
            if ((unsigned)sy >= (unsigned)rows)
                sy = rows - 1;
            int x0 = lay->text_x0 + cx * cell_w;
            int y0 = lay->text_y0 + sy * cell_h;
            if (cursor_mode == CURSOR_BLOCK || cursor_mode == CURSOR_BLOCK_BLINK) {
                fill_rect(pix, w, h, x0, y0, cell_w, cell_h, cur);
                unsigned char c = (unsigned char)tw->cells[cy * tw->cols + cx];
                int baselineY = lay->text_y0 + sy * cell_h + (int)font->max_top;
                tw_uefi_font_draw_char(font, pix, w, h, x0, baselineY, c, paper_bg, cur);
            } else if (cursor_mode == CURSOR_BAR || cursor_mode == CURSOR_BAR_BLINK) {
                fill_rect(pix, w, h, x0, y0, 2, cell_h, cur);
            }
        }
    }

    /* Typewriter “margin” rule: thin horizontal line on the active row (drawn after cursor). */
    if (typewriter_mode && rows > 0 && (unsigned)tw->cy < (unsigned)tw->rows &&
        (unsigned)tw->cx < (unsigned)cols) {
        int sy_cur = typewriter_sy_for_cursor(tw->cy, rows);
        if ((unsigned)sy_cur < (unsigned)rows) {
            int y_rule = lay->text_y0 + sy_cur * cell_h + cell_h - 2;
            fill_rect(pix, w, h, lay->text_x0, y_rule, cols * cell_w, 2, typewriter_rule);
        }
    }

    draw_page_footer(pix, w, h, font, bg_idx, lay, rows, cur_page0, n_pages);
}

static int is_printable_ascii(unsigned char c) {
    return c >= 32 && c < 127;
}

static void update_title(Display *dpy, Window win, const char *filename) {
    char title[PATH_MAX + 32];
    if (filename && filename[0])
        snprintf(title, sizeof(title), "Typewrite - %s", filename);
    else
        snprintf(title, sizeof(title), "Typewrite");
    XStoreName(dpy, win, title);
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "Usage: %s [--fullscreen] [--config PATH] [-f FILE] [FILE]\n"
            "  --config   JSON settings file (default: ~/.typewriter-settings.json)\n",
            argv0);
}

static void tw_x11_settings_clamp(TwX11AppSettings *cf) {
    int nf = tw_uefi_font_count();
    if (nf < 1)
        nf = 1;
    if (cf->font_index < 0 || cf->font_index >= nf)
        cf->font_index = 0;
    if (cf->background < 0 || cf->background > 9)
        cf->background = 1;
    if (cf->cursor_mode < 0 || cf->cursor_mode >= CURSOR_MODE_NUM)
        cf->cursor_mode = (int)CURSOR_BLOCK_BLINK;
    if (cf->gutter_mode < 0 || cf->gutter_mode >= LINE_NUM_MODE_NUM)
        cf->gutter_mode = LINE_NUM_OFF;
    cf->page_margins = cf->page_margins ? 1 : 0;
    if (cf->cols_margined < COLS_MARGINED_MIN)
        cf->cols_margined = COLS_MARGINED_MIN;
    if (cf->cols_margined > COLS_MARGINED_MAX)
        cf->cols_margined = COLS_MARGINED_MAX;
    cf->typewriter_view = cf->typewriter_view ? 1 : 0;
    cf->word_wrap = cf->word_wrap ? 1 : 0;
    if (cf->status_pulse < 0 || cf->status_pulse >= STATUS_PULSE_NUM)
        cf->status_pulse = 0;
    cf->insert_mode = cf->insert_mode ? 1 : 0;
    cf->start_fullscreen = cf->start_fullscreen ? 1 : 0;
    if (cf->window_width < 320)
        cf->window_width = 320;
    if (cf->window_width > 5600)
        cf->window_width = 5600;
    if (cf->window_height < 240)
        cf->window_height = 240;
    if (cf->window_height > 5600)
        cf->window_height = 5600;
}

static void fill_settings_snap(TwX11AppSettings *s, int font_idx, int bg_idx, CursorMode cursor_mode,
                               LineNumMode line_num_mode, int page_margins, int cols_margined,
                               int typewriter_mode, const TwDoc *doc, int status_pulse_idx,
                               int prefs_start_fullscreen, int win_w, int win_h) {
    if (!s || !doc)
        return;
    tw_x11_settings_defaults(s);
    s->font_index = font_idx;
    s->background = bg_idx;
    s->cursor_mode = (int)cursor_mode;
    s->gutter_mode = (int)line_num_mode;
    s->page_margins = page_margins;
    s->cols_margined = cols_margined;
    s->typewriter_view = typewriter_mode;
    s->word_wrap = doc->word_wrap ? 1 : 0;
    s->status_pulse = status_pulse_idx;
    s->insert_mode = doc->insert_mode ? 1 : 0;
    s->start_fullscreen = prefs_start_fullscreen ? 1 : 0;
    s->window_width = win_w;
    s->window_height = win_h;
}

static void path_basename(const char *path, char *out, size_t osz) {
    const char *s = strrchr(path, '/');
    snprintf(out, osz, "%s", s ? s + 1 : path);
}

static const char *cursor_mode_toast_label(CursorMode m) {
    switch (m) {
    case CURSOR_BAR:
        return "Cursor: bar";
    case CURSOR_BAR_BLINK:
        return "Cursor: blinking bar";
    case CURSOR_BLOCK:
        return "Cursor: block";
    case CURSOR_BLOCK_BLINK:
        return "Cursor: blinking block";
    case CURSOR_HIDDEN:
        return "Cursor: hidden";
    default:
        return "Cursor changed";
    }
}

static const char *gutter_toast_label(LineNumMode m) {
    switch (m) {
    case LINE_NUM_OFF:
        return "Gutter: off";
    case LINE_NUM_ASC:
        return "Gutter: line 1, 2, 3...";
    case LINE_NUM_DESC:
        return "Gutter: rows remaining";
    default:
        return "Gutter changed";
    }
}

/* Derive sibling .pdf path from save path, or default name when no file yet. */
static void pdf_out_path(char *out, size_t osz, const char *src) {
    if (!src || !src[0]) {
        snprintf(out, osz, "Typewriter.pdf");
        return;
    }
    const char *dot = strrchr(src, '.');
    if (dot && dot > src) {
        snprintf(out, osz, "%.*s.pdf", (int)(dot - src), src);
    } else {
        snprintf(out, osz, "%s.pdf", src);
    }
}

/*
 * EWMH _NET_WM_STATE client message: format 32, data.l[0]=action, l[1]=first atom, l[2]=second or 0.
 * action: _NET_WM_STATE_REMOVE=0, ADD=1, TOGGLE=2.
 */
static void net_wm_state(Display *dpy, int screen, Window win, Atom a_wm_state, Atom a_fullscreen, long action) {
    XEvent e;
    memset(&e, 0, sizeof(e));
    e.xclient.type = ClientMessage;
    e.xclient.send_event = True;
    e.xclient.window = win;
    e.xclient.message_type = a_wm_state;
    e.xclient.format = 32;
    e.xclient.data.l[0] = action;
    e.xclient.data.l[1] = (long)a_fullscreen;
    e.xclient.data.l[2] = 0;

    XSendEvent(dpy, RootWindow(dpy, screen), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XFlush(dpy);
}

typedef struct {
    Display *dpy;
    Window win;
    int screen;
    TwDoc *doc;
    char *filename;
    int *dirty;
    uint64_t *last_doc_edit_ms;
    int *font_idx;
    const TwBitmapFont **font;
    CursorMode *cursor_mode;
    int *bg_idx;
    LineNumMode *line_num_mode;
    int *page_margins;
    int *cols_margined;
    int *typewriter_mode;
    int *status_pulse_idx;
    uint64_t *next_status_pulse_ms;
    uint64_t *session_typing_units;
    ToastState *toast;
    TypingPace *typing_pace;
    Atom a_wm_state;
    Atom a_wm_state_fullscreen;
    int *show_help;
    int *ui_settings_dirty;
    int *prefs_start_fullscreen;
} HelpMenuEnv;

/*
 * Execute the highlighted help line (same effect as the shortcut where applicable).
 * Returns 1 if the caller should exit the app (save & quit).
 */
static int help_menu_activate_selection(int sel, HelpMenuEnv *e) {
    if (!e || sel < 0 || sel >= HELP_MENU_ROWS)
        return 0;

    switch (sel) {
    case 22: { /* Ctrl+Q / Ctrl+X */
        if (e->filename[0] == 0)
            snprintf(e->filename, PATH_MAX, "Typewriter.txt");
        (void)twdoc_save(e->filename, e->doc);
        *e->dirty = 0;
        update_title(e->dpy, e->win, e->filename);
        return 1;
    }
    case 2: { /* F2 font */
        *e->font_idx = (*e->font_idx + 1) % tw_uefi_font_count();
        *e->font = tw_uefi_font_get(*e->font_idx);
        update_title(e->dpy, e->win, e->filename);
        char tmsg[TOAST_MAX];
        snprintf(tmsg, sizeof(tmsg), "Font: %s", (*e->font)->name);
        toast_show(e->toast, e->typing_pace, tmsg, 0);
        if (e->ui_settings_dirty)
            *e->ui_settings_dirty = 1;
        break;
    }
    case 3:
        *e->cursor_mode = (CursorMode)((*e->cursor_mode + 1) % CURSOR_MODE_NUM);
        toast_show(e->toast, e->typing_pace, cursor_mode_toast_label(*e->cursor_mode), 0);
        if (e->ui_settings_dirty)
            *e->ui_settings_dirty = 1;
        break;
    case 4:
        *e->bg_idx = (*e->bg_idx + 1) % 10;
        {
            char tmsg[TOAST_MAX];
            snprintf(tmsg, sizeof(tmsg), "Background: %s", kBgLabels[*e->bg_idx % 10]);
            toast_show(e->toast, e->typing_pace, tmsg, 0);
        }
        if (e->ui_settings_dirty)
            *e->ui_settings_dirty = 1;
        break;
    case 5:
        *e->line_num_mode = (LineNumMode)((*e->line_num_mode + 1) % LINE_NUM_MODE_NUM);
        toast_show(e->toast, e->typing_pace, gutter_toast_label(*e->line_num_mode), 0);
        if (e->ui_settings_dirty)
            *e->ui_settings_dirty = 1;
        break;
    case 6: { /* Tab */
        uint64_t t = mono_ms();
        for (int ti = 0; ti < 4; ti++)
            twdoc_putc(e->doc, ' ');
        typing_pace_note_char(e->typing_pace, t);
        *e->session_typing_units += 4u;
        *e->dirty = 1;
        if (e->last_doc_edit_ms)
            *e->last_doc_edit_ms = mono_ms();
        break;
    }
    case 9:
        *e->page_margins = !*e->page_margins;
        toast_show(e->toast, e->typing_pace, *e->page_margins ? "Letter margins: on" : "Letter margins: off", 0);
        if (e->ui_settings_dirty)
            *e->ui_settings_dirty = 1;
        break;
    case 10:
        (*e->cols_margined)++;
        if (*e->cols_margined > COLS_MARGINED_MAX)
            *e->cols_margined = COLS_MARGINED_MIN;
        {
            char tmsg[TOAST_MAX];
            snprintf(tmsg, sizeof(tmsg), "Chars per line: %d", *e->cols_margined);
            toast_show(e->toast, e->typing_pace, tmsg, 0);
        }
        if (e->ui_settings_dirty)
            *e->ui_settings_dirty = 1;
        break;
    case 11:
        *e->typewriter_mode = !*e->typewriter_mode;
        toast_show(e->toast, e->typing_pace, *e->typewriter_mode ? "Typewriter view: on" : "Typewriter view: off", 0);
        if (e->ui_settings_dirty)
            *e->ui_settings_dirty = 1;
        break;
    case 12: {
        uint64_t tnow = mono_ms();
        *e->status_pulse_idx = (*e->status_pulse_idx + 1) % STATUS_PULSE_NUM;
        *e->next_status_pulse_ms = tnow + k_status_pulse_ms[*e->status_pulse_idx];
        char tmsg[TOAST_MAX];
        snprintf(tmsg, sizeof(tmsg), "Status toast: every %s", k_status_pulse_label[*e->status_pulse_idx]);
        toast_show(e->toast, e->typing_pace, tmsg, 0);
        if (e->ui_settings_dirty)
            *e->ui_settings_dirty = 1;
        break;
    }
    case 13:
        e->doc->word_wrap = !e->doc->word_wrap;
        toast_show(e->toast, e->typing_pace, e->doc->word_wrap ? "Word wrap: on" : "Word wrap: off", 0);
        *e->dirty = 1;
        if (e->last_doc_edit_ms)
            *e->last_doc_edit_ms = mono_ms();
        if (e->ui_settings_dirty)
            *e->ui_settings_dirty = 1;
        break;
    case 16:
        e->doc->insert_mode = !e->doc->insert_mode;
        toast_show(e->toast, e->typing_pace, e->doc->insert_mode ? "Insert mode" : "Typeover mode", 0);
        *e->dirty = 1;
        if (e->ui_settings_dirty)
            *e->ui_settings_dirty = 1;
        break;
    case 17:
        twdoc_delete_forward(e->doc);
        *e->dirty = 1;
        if (e->last_doc_edit_ms)
            *e->last_doc_edit_ms = mono_ms();
        break;
    case 18:
        if (e->a_wm_state != None && e->a_wm_state_fullscreen != None) {
            net_wm_state(e->dpy, e->screen, e->win, e->a_wm_state, e->a_wm_state_fullscreen, 2);
            toast_show(e->toast, e->typing_pace, "Fullscreen: toggled (WM may override)", 0);
        }
        break;
    case 20: /* Ctrl+S */
        if (e->filename[0] == 0)
            snprintf(e->filename, PATH_MAX, "Typewriter.txt");
        {
            char base[PATH_MAX];
            path_basename(e->filename, base, sizeof(base));
            if (twdoc_save(e->filename, e->doc) == 0) {
                *e->dirty = 0;
                char tmsg[TOAST_MAX];
                snprintf(tmsg, sizeof(tmsg), "Saved %.150s", base);
                toast_show(e->toast, e->typing_pace, tmsg, 0);
            } else {
                char tmsg[TOAST_MAX];
                snprintf(tmsg, sizeof(tmsg), "Save failed: %.150s", base);
                toast_show(e->toast, e->typing_pace, tmsg, 0);
            }
        }
        update_title(e->dpy, e->win, e->filename);
        break;
    case 21: { /* Ctrl+P */
        char pdf_path[PATH_MAX];
        pdf_out_path(pdf_path, sizeof(pdf_path), e->filename[0] ? e->filename : NULL);
        TwPdfOpts po = {.page_margins = *e->page_margins,
                        .line_num_mode = (TwPdfLineNum)(int)*e->line_num_mode,
                        .cols_margined = *e->cols_margined,
                        .bg_idx = *e->bg_idx};
        char pdf_base[PATH_MAX];
        path_basename(pdf_path, pdf_base, sizeof(pdf_base));
        if (tw_export_pdf(e->doc, pdf_path, *e->font, &po) == 0) {
            char tmsg[TOAST_MAX];
            snprintf(tmsg, sizeof(tmsg), "Printed PDF: %.150s", pdf_base);
            toast_show(e->toast, e->typing_pace, tmsg, 0);
        } else {
            char tmsg[TOAST_MAX];
            snprintf(tmsg, sizeof(tmsg), "PDF failed: %.150s", pdf_base);
            toast_show(e->toast, e->typing_pace, tmsg, 0);
        }
        break;
    }
    default:
        /* 0,1,7,8,14,15,19,23 — informational or duplicate close */
        break;
    }

    *e->show_help = 0;
    return 0;
}

static int row_end_cx_for_move(const TwCore *tw, int row) {
    int c = tw->cols - 1;
    while (c >= 0 && tw->cells[(size_t)row * (size_t)tw->cols + (size_t)c] == ' ')
        c--;
    return (c < 0) ? 0 : c + 1;
}

static void doc_cursor_left(TwDoc *d) {
    TwCore *t = twdoc_cur(d);
    if (!t)
        return;
    if (t->cx > 0) {
        t->cx--;
        return;
    }
    if (t->cy > 0) {
        t->cy--;
        t->cx = t->cols - 1;
        return;
    }
    if (d->cur_page > 0) {
        d->cur_page--;
        t = twdoc_cur(d);
        t->cy = t->rows - 1;
        t->cx = t->cols - 1;
    }
}

static void doc_cursor_right(TwDoc *d) {
    TwCore *t = twdoc_cur(d);
    if (!t)
        return;
    if (t->cx < t->cols - 1) {
        t->cx++;
        return;
    }
    if (t->cy < t->rows - 1) {
        t->cy++;
        t->cx = 0;
        return;
    }
    if (d->cur_page < d->n_pages - 1) {
        d->cur_page++;
        t = twdoc_cur(d);
        t->cy = 0;
        t->cx = 0;
    }
}

static void doc_cursor_up(TwDoc *d) {
    TwCore *t = twdoc_cur(d);
    if (!t)
        return;
    if (t->cy > 0) {
        t->cy--;
        return;
    }
    if (d->cur_page > 0) {
        d->cur_page--;
        t = twdoc_cur(d);
        t->cy = t->rows - 1;
    }
}

static void doc_cursor_down(TwDoc *d) {
    TwCore *t = twdoc_cur(d);
    if (!t)
        return;
    if (t->cy < t->rows - 1) {
        t->cy++;
        return;
    }
    if (d->cur_page < d->n_pages - 1) {
        d->cur_page++;
        t = twdoc_cur(d);
        t->cy = 0;
    }
}

static void doc_cursor_home(TwDoc *d) {
    TwCore *t = twdoc_cur(d);
    if (t)
        t->cx = 0;
}

static void doc_cursor_end(TwDoc *d) {
    TwCore *t = twdoc_cur(d);
    if (!t)
        return;
    int x = row_end_cx_for_move(t, t->cy);
    if (x >= t->cols)
        x = t->cols - 1;
    t->cx = x;
}

static void doc_page_prev(TwDoc *d) {
    if (d->cur_page > 0)
        d->cur_page--;
}

static void doc_page_next(TwDoc *d) {
    if (d->cur_page < d->n_pages - 1)
        d->cur_page++;
}

/* Whitespace-separated tokens; each grid row ends a word (typewriter layout). */
static unsigned long twdoc_count_words(const TwDoc *doc) {
    unsigned long n = 0;
    if (!doc || !doc->pages)
        return 0;
    for (int pi = 0; pi < doc->n_pages; pi++) {
        const TwCore *tw = &doc->pages[pi];
        if (!tw->cells || tw->rows <= 0 || tw->cols <= 0)
            continue;
        for (int row = 0; row < tw->rows; row++) {
            int in_word = 0;
            for (int col = 0; col < tw->cols; col++) {
                unsigned char c = (unsigned char)tw->cells[(size_t)row * (size_t)tw->cols + (size_t)col];
                if (c != ' ') {
                    in_word = 1;
                } else if (in_word) {
                    n++;
                    in_word = 0;
                }
            }
            if (in_word)
                n++;
        }
    }
    return n;
}

static unsigned typing_pace_to_wpm(const TypingPace *p) {
    if (!p || p->ema_ms_per_char <= 0.f)
        return 0u;
    float w = 12000.f / p->ema_ms_per_char;
    if (w < 0.f)
        return 0u;
    if (w > 999.f)
        return 999u;
    return (unsigned)(w + 0.5f);
}

/*
 * session_typing_units: printable +1, Tab +4, Enter +1 (standard 5-keystroke "word" for session tally).
 */
static void format_status_pulse_toast(char *buf, size_t buflen, const TwDoc *doc, const TypingPace *pace,
                                      uint64_t session_typing_units) {
    time_t clk = time(NULL);
    struct tm tm_buf;
    struct tm *tp = localtime_r(&clk, &tm_buf);
    char timestr[16];
    if (tp)
        strftime(timestr, sizeof(timestr), "%H:%M", tp);
    else
        snprintf(timestr, sizeof(timestr), "--:--");

    unsigned wpm = typing_pace_to_wpm(pace);
    unsigned long sess_words = (unsigned long)((session_typing_units + 4u) / 5u);
    unsigned long doc_words = twdoc_count_words(doc);

    snprintf(buf, buflen, "%u wpm | session %lu words | doc %lu words | %s", wpm, sess_words, doc_words, timestr);
}

/* =============================================================================
 * SECTION: HELP MENU
 * help_fill_lines, help selection handlers
 * ============================================================================= */

static void help_fill_lines(char lines[HELP_MENU_ROWS][160], int bg_idx, int cols_margined, int status_pulse_idx,
                            int word_wrap_on) {
    snprintf(lines[0], sizeof(lines[0]),
             "Help  Up/Dn select  Home/End  PgUp/Dn  Enter run row  Esc close");
    snprintf(lines[1], sizeof(lines[1]), "F1    toggle this menu");
    snprintf(lines[2], sizeof(lines[2]), "F2    cycle font");
    snprintf(lines[3], sizeof(lines[3]), "F3    cycle cursor");
    snprintf(lines[4], sizeof(lines[4]), "F4    background: %s", kBgLabels[bg_idx % 10]);
    snprintf(lines[5], sizeof(lines[5]), "F5    gutter: off / 1..n / rows-left");
    snprintf(lines[6], sizeof(lines[6]), "Tab   insert 4 spaces");
    snprintf(lines[7], sizeof(lines[7]),
             "      Return: new row (typeover); insert mode splits line & pushes text down");
    snprintf(lines[8], sizeof(lines[8]), "      saves use Ctrl-L between pages");
    snprintf(lines[9], sizeof(lines[9]), "F6    page margins (Letter) on/off");
    snprintf(lines[10], sizeof(lines[10]), "F7    chars/line %d-%d (margins on): %d", COLS_MARGINED_MIN,
             COLS_MARGINED_MAX, cols_margined);
    snprintf(lines[11], sizeof(lines[11]), "F8    typewriter view (default on)");
    if (status_pulse_idx < 0 || status_pulse_idx >= STATUS_PULSE_NUM)
        status_pulse_idx = 0;
    snprintf(lines[12], sizeof(lines[12]), "F9    status toast: every %s (cycle)", k_status_pulse_label[status_pulse_idx]);
    snprintf(lines[13], sizeof(lines[13]), "F10   word wrap: %s (break at last space)", word_wrap_on ? "on" : "off");
    snprintf(lines[14], sizeof(lines[14]), "Arrows move cursor (cross pages at edges)");
    snprintf(lines[15], sizeof(lines[15]), "Home/End  line; PgUp/PgDn  page");
    snprintf(lines[16], sizeof(lines[16]), "Insert  toggle insert / typeover (default typeover)");
    snprintf(lines[17], sizeof(lines[17]),
             "Delete  insert: Bs/Del at line start joins row above; end-of-line Del pulls next up");
    snprintf(lines[18], sizeof(lines[18]), "F11   fullscreen (often taken by WM)");
    snprintf(lines[19], sizeof(lines[19]), "      use: x11typewrite --fullscreen");
    snprintf(lines[20], sizeof(lines[20]), "Ctrl+S save (~10s idle autosave when dirty)");
    snprintf(lines[21], sizeof(lines[21]), "Ctrl+P export PDF (Cairo, linked library)");
    snprintf(lines[22], sizeof(lines[22]), "Ctrl+Q / Ctrl+X  save and exit");
    snprintf(lines[23], sizeof(lines[23]), "Esc   close menu (when open)");
}

/* =============================================================================
 * SECTION: MAIN
 * main, X11 initialization, event loop
 * ============================================================================= */

int main(int argc, char **argv) {
    Display *dpy = NULL;
    int screen = 0;
    Window win;
    GC gc;
    Atom wm_delete;
    Atom a_wm_state;
    Atom a_wm_state_fullscreen;

    char filename[PATH_MAX];
    char cfg_save_path[PATH_MAX] = {0};
    int cfg_from_arg = 0;
    int cli_fullscreen = 0;

    filename[0] = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--fullscreen") == 0) {
            cli_fullscreen = 1;
            continue;
        }
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            snprintf(cfg_save_path, sizeof(cfg_save_path), "%s", argv[++i]);
            cfg_from_arg = 1;
            continue;
        }
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            snprintf(filename, sizeof(filename), "%s", argv[++i]);
            continue;
        }
        if (argv[i][0] != '-') {
            snprintf(filename, sizeof(filename), "%s", argv[i]);
            continue;
        }
        fprintf(stderr, "%s: unknown option %s\n", argv[0], argv[i]);
        usage(argv[0]);
        return 1;
    }

    TwX11AppSettings cf;
    tw_x11_settings_defaults(&cf);
    if (cfg_save_path[0]) {
        if (cfg_from_arg && access(cfg_save_path, R_OK) != 0)
            fprintf(stderr, "%s: config %s not readable (%s); using defaults\n", argv[0], cfg_save_path,
                    strerror(errno));
        if (tw_x11_settings_load(cfg_save_path, &cf) != 0 && cfg_from_arg)
            fprintf(stderr, "%s: invalid JSON in %s; using defaults\n", argv[0], cfg_save_path);
    } else {
        tw_x11_settings_default_path(cfg_save_path, sizeof(cfg_save_path));
        (void)tw_x11_settings_load(cfg_save_path, &cf);
    }
    tw_x11_settings_clamp(&cf);
    int saved_start_fullscreen = cf.start_fullscreen;

    int w = cf.window_width;
    int h = cf.window_height;
    int start_fullscreen = (cf.start_fullscreen || cli_fullscreen) ? 1 : 0;
    int font_idx = cf.font_index;

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "XOpenDisplay failed\n");
        return 1;
    }

    TwSoundSetBasePath(argv[0]);
    TwSoundInit("sounds.assets");
    fprintf(stderr, "MAIN: Sound initialized (no startup sounds for debug)\n");

    screen = DefaultScreen(dpy);

    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, (unsigned)w, (unsigned)h, 0,
                              BlackPixel(dpy, screen), BlackPixel(dpy, screen));
    XStoreName(dpy, win, "Typewrite");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(dpy, win);

    gc = XCreateGC(dpy, win, 0, NULL);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    a_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    a_wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    /* Apply --fullscreen on MapNotify; immediate _NET_WM_STATE is often ignored. */
    int pending_startup_fullscreen = (start_fullscreen && a_wm_state != None && a_wm_state_fullscreen != None);

    TwDoc doc;
    uint32_t *back = NULL;
    XImage *img = NULL;
    const TwBitmapFont *font = tw_uefi_font_get(font_idx);
    CursorMode cursor_mode = (CursorMode)cf.cursor_mode;
    int bg_idx = cf.background;
    int show_help = 0;
    int help_sel = 0;
    int dirty = 0;
    uint64_t last_doc_edit_ms = mono_ms();
    int page_margins = cf.page_margins;
    LineNumMode line_num_mode = (LineNumMode)cf.gutter_mode;
    int cols_margined = cf.cols_margined;
    int typewriter_mode = cf.typewriter_view;
    ToastState toast = {0};
    TypingPace typing_pace = {0};
    uint64_t session_typing_units = 0;
    uint64_t next_status_pulse_ms = 0;
    int status_pulse_idx = cf.status_pulse;

    ViewLayout init_lay;
    compute_view_layout(w, h, font, page_margins, line_num_mode, cols_margined, &init_lay);
    if (twdoc_init(&doc, init_lay.cols, init_lay.rows) != 0) {
        fprintf(stderr, "twdoc_init failed\n");
        XCloseDisplay(dpy);
        return 1;
    }
    doc.insert_mode = cf.insert_mode;
    doc.word_wrap = cf.word_wrap;

    if (filename[0] == 0) {
        /* Autoload default file if present. */
        snprintf(filename, sizeof(filename), "Typewriter.txt");
        if (twdoc_load(filename, &doc) == 0) {
            dirty = 0;
        } else {
            filename[0] = 0;
        }
    } else {
        (void)twdoc_load(filename, &doc);
        dirty = 0;
    }

    update_title(dpy, win, filename);

    next_status_pulse_ms = mono_ms() + k_status_pulse_ms[status_pulse_idx];

    for (;;) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) {
                goto out;
            }

            if (ev.type == MapNotify && ev.xmap.window == win && pending_startup_fullscreen) {
                pending_startup_fullscreen = 0;
                net_wm_state(dpy, screen, win, a_wm_state, a_wm_state_fullscreen, 1);
            }

            if (ev.type == ConfigureNotify) {
                if (ev.xconfigure.width != w || ev.xconfigure.height != h) {
                    w = ev.xconfigure.width;
                    h = ev.xconfigure.height;
                }
            }

            if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == NoSymbol)
                    ks = 0;
                /* Shifted keymap group (e.g. some layouts expose F11 only on second keysym). */
                KeySym ks1 = XLookupKeysym(&ev.xkey, 1);
                char buf[32];
                int n = XLookupString(&ev.xkey, buf, (int)sizeof(buf), NULL, NULL);

                if (ks == XK_Escape) {
                    if (show_help)
                        show_help = 0;
                }
                if (!show_help && (ks == XK_F11 || ks1 == XK_F11) && a_wm_state != None &&
                    a_wm_state_fullscreen != None) {
                    net_wm_state(dpy, screen, win, a_wm_state, a_wm_state_fullscreen, 2);
                    toast_show(&toast, &typing_pace, "Fullscreen: toggled (WM may override)", 0);
                }
                if (ks == XK_F1) {
                    int was_open = show_help;
                    show_help = !show_help;
                    if (show_help && !was_open)
                        help_sel = 0;
                }
                if (!show_help && ks == XK_F2) {
                    font_idx = (font_idx + 1) % tw_uefi_font_count();
                    font = tw_uefi_font_get(font_idx);
                    update_title(dpy, win, filename);
                    char tmsg[TOAST_MAX];
                    snprintf(tmsg, sizeof(tmsg), "Font: %s", font->name);
                    toast_show(&toast, &typing_pace, tmsg, 0);
                }
                if (!show_help && ks == XK_F3) {
                    cursor_mode = (CursorMode)((cursor_mode + 1) % CURSOR_MODE_NUM);
                    toast_show(&toast, &typing_pace, cursor_mode_toast_label(cursor_mode), 0);
                }
                if (!show_help && ks == XK_F4) {
                    bg_idx = (bg_idx + 1) % 10;
                    char tmsg[TOAST_MAX];
                    snprintf(tmsg, sizeof(tmsg), "Background: %s", kBgLabels[bg_idx % 10]);
                    toast_show(&toast, &typing_pace, tmsg, 0);
                }
                if (!show_help && ks == XK_F5) {
                    line_num_mode = (LineNumMode)((line_num_mode + 1) % LINE_NUM_MODE_NUM);
                    toast_show(&toast, &typing_pace, gutter_toast_label(line_num_mode), 0);
                }
                if (!show_help && ks == XK_F6) {
                    page_margins = !page_margins;
                    toast_show(&toast, &typing_pace, page_margins ? "Letter margins: on" : "Letter margins: off", 0);
                }
                if (!show_help && ks == XK_F7) {
                    cols_margined++;
                    if (cols_margined > COLS_MARGINED_MAX)
                        cols_margined = COLS_MARGINED_MIN;
                    char tmsg[TOAST_MAX];
                    snprintf(tmsg, sizeof(tmsg), "Chars per line: %d", cols_margined);
                    toast_show(&toast, &typing_pace, tmsg, 0);
                }
                if (!show_help && ks == XK_F8) {
                    typewriter_mode = !typewriter_mode;
                    toast_show(&toast, &typing_pace, typewriter_mode ? "Typewriter view: on" : "Typewriter view: off", 0);
                }
                if (!show_help && ks == XK_F9) {
                    uint64_t tnow = mono_ms();
                    status_pulse_idx = (status_pulse_idx + 1) % STATUS_PULSE_NUM;
                    next_status_pulse_ms = tnow + k_status_pulse_ms[status_pulse_idx];
                    char tmsg[TOAST_MAX];
                    snprintf(tmsg, sizeof(tmsg), "Status toast: every %s", k_status_pulse_label[status_pulse_idx]);
                    toast_show(&toast, &typing_pace, tmsg, 0);
                }
                if (!show_help && ks == XK_F10) {
                    doc.word_wrap = !doc.word_wrap;
                    toast_show(&toast, &typing_pace, doc.word_wrap ? "Word wrap: on" : "Word wrap: off", 0);
                    mark_document_edited(&dirty, &last_doc_edit_ms);
                }
                if ((ev.xkey.state & ControlMask) && (ks == XK_s || ks == XK_S)) {
                    if (filename[0] == 0) {
                        snprintf(filename, sizeof(filename), "Typewriter.txt");
                    }
                    char base[PATH_MAX];
                    path_basename(filename, base, sizeof(base));
                    if (twdoc_save(filename, &doc) == 0) {
                        dirty = 0;
                        char tmsg[TOAST_MAX];
                        snprintf(tmsg, sizeof(tmsg), "Saved %.150s", base);
                        toast_show(&toast, &typing_pace, tmsg, 0);
                    } else {
                        char tmsg[TOAST_MAX];
                        snprintf(tmsg, sizeof(tmsg), "Save failed: %.150s", base);
                        toast_show(&toast, &typing_pace, tmsg, 0);
                    }
                    update_title(dpy, win, filename);
                }
                if (!show_help && (ev.xkey.state & ControlMask) && (ks == XK_p || ks == XK_P)) {
                    char pdf_path[PATH_MAX];
                    pdf_out_path(pdf_path, sizeof(pdf_path), filename[0] ? filename : NULL);
                    TwPdfOpts po = {.page_margins = page_margins,
                                    .line_num_mode = (TwPdfLineNum)(int)line_num_mode,
                                    .cols_margined = cols_margined,
                                    .bg_idx = bg_idx};
                    char pdf_base[PATH_MAX];
                    path_basename(pdf_path, pdf_base, sizeof(pdf_base));
                    if (tw_export_pdf(&doc, pdf_path, font, &po) == 0) {
                        char tmsg[TOAST_MAX];
                        snprintf(tmsg, sizeof(tmsg), "Printed PDF: %.150s", pdf_base);
                        toast_show(&toast, &typing_pace, tmsg, 0);
                    } else {
                        char tmsg[TOAST_MAX];
                        snprintf(tmsg, sizeof(tmsg), "PDF failed: %.150s", pdf_base);
                        toast_show(&toast, &typing_pace, tmsg, 0);
                    }
                }
                if ((ev.xkey.state & ControlMask) && (ks == XK_q || ks == XK_Q || ks == XK_x || ks == XK_X)) {
                    if (filename[0] == 0)
                        snprintf(filename, sizeof(filename), "Typewriter.txt");
                    (void)twdoc_save(filename, &doc);
                    dirty = 0;
                    update_title(dpy, win, filename);
                    goto out;
                }
                if (ks == XK_BackSpace && !show_help) {
                    twdoc_backspace(&doc);
                    /* TwPlaySoundForFont(font_idx); */
                    mark_document_edited(&dirty, &last_doc_edit_ms);
                } else if ((ks == XK_Delete || ks == XK_KP_Delete) && !show_help) {
                    twdoc_delete_forward(&doc);
                    /* TwPlaySoundForFont(font_idx); */
                    mark_document_edited(&dirty, &last_doc_edit_ms);
                } else if ((ks == XK_Left || ks == XK_KP_Left) && !show_help) {
                    doc_cursor_left(&doc);
                    dirty = 1;
                } else if ((ks == XK_Right || ks == XK_KP_Right) && !show_help) {
                    doc_cursor_right(&doc);
                    dirty = 1;
                } else if (ks == XK_Up || ks == XK_KP_Up) {
                    if (show_help) {
                        if (help_sel > 0)
                            help_sel--;
                    } else {
                        doc_cursor_up(&doc);
                        dirty = 1;
                    }
                } else if (ks == XK_Down || ks == XK_KP_Down) {
                    if (show_help) {
                        if (help_sel < HELP_MENU_ROWS - 1)
                            help_sel++;
                    } else {
                        doc_cursor_down(&doc);
                        dirty = 1;
                    }
                } else if (ks == XK_Home || ks == XK_KP_Home) {
                    if (show_help)
                        help_sel = 0;
                    else {
                        doc_cursor_home(&doc);
                        dirty = 1;
                    }
                } else if (ks == XK_End || ks == XK_KP_End) {
                    if (show_help)
                        help_sel = HELP_MENU_ROWS - 1;
                    else {
                        doc_cursor_end(&doc);
                        dirty = 1;
                    }
                } else if (ks == XK_Page_Up || ks == XK_KP_Page_Up) {
                    if (show_help)
                        help_sel = 0;
                    else {
                        doc_page_prev(&doc);
                        dirty = 1;
                    }
                } else if (ks == XK_Page_Down || ks == XK_KP_Page_Down) {
                    if (show_help)
                        help_sel = HELP_MENU_ROWS - 1;
                    else {
                        doc_page_next(&doc);
                        dirty = 1;
                    }
                } else if (ks == XK_Insert && !show_help) {
                    doc.insert_mode = !doc.insert_mode;
                    toast_show(&toast, &typing_pace, doc.insert_mode ? "Insert mode" : "Typeover mode", 0);
                    dirty = 1;
                } else if (ks == XK_Tab && !show_help) {
                    uint64_t t = mono_ms();
                    for (int ti = 0; ti < 4; ti++)
                        twdoc_putc(&doc, ' ');
                    typing_pace_note_char(&typing_pace, t);
                    session_typing_units += 4u;
                    mark_document_edited(&dirty, &last_doc_edit_ms);
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    if (show_help) {
                        HelpMenuEnv henv = {.dpy = dpy,
                                            .win = win,
                                            .screen = screen,
                                            .doc = &doc,
                                            .filename = filename,
                                            .dirty = &dirty,
                                            .last_doc_edit_ms = &last_doc_edit_ms,
                                            .font_idx = &font_idx,
                                            .font = &font,
                                            .cursor_mode = &cursor_mode,
                                            .bg_idx = &bg_idx,
                                            .line_num_mode = &line_num_mode,
                                            .page_margins = &page_margins,
                                            .cols_margined = &cols_margined,
                                            .typewriter_mode = &typewriter_mode,
                                            .status_pulse_idx = &status_pulse_idx,
                                            .next_status_pulse_ms = &next_status_pulse_ms,
                                            .session_typing_units = &session_typing_units,
                                            .toast = &toast,
                                            .typing_pace = &typing_pace,
                                            .a_wm_state = a_wm_state,
                                            .a_wm_state_fullscreen = a_wm_state_fullscreen,
                                            .show_help = &show_help};
                        if (help_menu_activate_selection(help_sel, &henv))
                            goto out;
                    } else {
                        uint64_t t = mono_ms();
                        twdoc_insert_newline(&doc);
                        typing_pace_note_char(&typing_pace, t);
                        session_typing_units += 1u;
                        mark_document_edited(&dirty, &last_doc_edit_ms);
                    }
                } else if (n > 0 && is_printable_ascii((unsigned char)buf[0]) && !show_help) {
                    uint64_t t = mono_ms();
                    twdoc_putc(&doc, buf[0]);
                    /* TwPlaySoundForFont(font_idx); */
                    typing_pace_note_char(&typing_pace, t);
                    session_typing_units += 1u;
                    mark_document_edited(&dirty, &last_doc_edit_ms);
                }
            }
        }

        ViewLayout lay;
        compute_view_layout(w, h, font, page_margins, line_num_mode, cols_margined, &lay);

        int need_recreate = !back || !img || img->width != w || img->height != h;
        if (need_recreate) {
            if (img) {
                img->data = NULL; /* data owned by us */
                XDestroyImage(img);
                img = NULL;
            }
            free(back);
            back = (uint32_t *)calloc((size_t)w * (size_t)h, sizeof(uint32_t));
            if (!back) {
                fprintf(stderr, "calloc backbuffer failed\n");
                goto out;
            }
        }

        if (twdoc_resize_reflow(&doc, lay.cols, lay.rows) != 0) {
            fprintf(stderr, "twdoc_resize_reflow failed\n");
            goto out;
        }

        uint64_t now = mono_ms();
        int cursor_visible = 1;
        if (cursor_mode == CURSOR_BAR_BLINK || cursor_mode == CURSOR_BLOCK_BLINK) {
            cursor_visible = ((now / 500u) % 2u) == 0u;
        }

        if (filename[0] != 0 && dirty && (now - last_doc_edit_ms) >= AUTOSAVE_IDLE_MS) {
            if (twdoc_save(filename, &doc) == 0) {
                dirty = 0;
                char base[PATH_MAX];
                path_basename(filename, base, sizeof(base));
                char tmsg[TOAST_MAX];
                snprintf(tmsg, sizeof(tmsg), "Autosaved %.150s", base);
                toast_show(&toast, &typing_pace, tmsg, 0);
            } else {
                last_doc_edit_ms = now; /* wait another idle period before retry */
                toast_show(&toast, &typing_pace, "Autosave failed", 0);
            }
        }

        if (now >= next_status_pulse_ms) {
            if (status_pulse_idx < 0 || status_pulse_idx >= STATUS_PULSE_NUM)
                status_pulse_idx = 0;
            next_status_pulse_ms = now + k_status_pulse_ms[status_pulse_idx];
            char pulse[TOAST_MAX];
            format_status_pulse_toast(pulse, sizeof(pulse), &doc, &typing_pace, session_typing_units);
            toast_show(&toast, &typing_pace, pulse, 1);
        }

        TwCore *tw_frame = twdoc_cur(&doc);
        if (!tw_frame)
            goto out;
        int view_rows = (tw_frame->rows < lay.rows) ? tw_frame->rows : lay.rows;
        render(back, w, h, tw_frame, font, cursor_mode, cursor_visible, bg_idx, &lay, page_margins, line_num_mode,
               typewriter_mode, twdoc_cur_page(&doc), twdoc_num_pages(&doc));

        if (show_help) {
            char hlines[HELP_MENU_ROWS][160];
            help_fill_lines(hlines, bg_idx, cols_margined, status_pulse_idx, doc.word_wrap);
            if (help_sel < 0)
                help_sel = 0;
            if (help_sel >= HELP_MENU_ROWS)
                help_sel = HELP_MENU_ROWS - 1;

            const uint32_t panel = pack_xrgb8888(0x1c2028u);
            const uint32_t panel2 = pack_xrgb8888(0x3a404cu);
            const uint32_t fg = pack_xrgb8888(0xfff5c8u);
            const uint32_t bg = panel;
            const uint32_t sel_bg = pack_xrgb8888(0x4a5568u);
            const uint32_t sel_fg = pack_xrgb8888(0xfffff0u);

            int step = (int)font->line_box;
            int inner = 8;
            int bw = (int)(font->max_width + 1) * 54 + inner * 2;
            int bh = step * HELP_MENU_ROWS + inner * 2;
            int bx = (w > bw) ? (w - bw) / 2 : 10;
            int by = (h > bh) ? (h - bh) / 2 : 10;

            fill_rect(back, w, h, bx - 2, by - 2, bw + 4, bh + 4, panel2);
            fill_rect(back, w, h, bx, by, bw, bh, panel);

            int x0 = bx + inner;
            for (int hi = 0; hi < HELP_MENU_ROWS; hi++) {
                int row_top = by + inner + hi * step;
                uint32_t row_bg = (hi == help_sel) ? sel_bg : bg;
                uint32_t row_fg = (hi == help_sel) ? sel_fg : fg;
                if (hi == help_sel)
                    fill_rect(back, w, h, bx + 2, row_top, bw - 4, step, sel_bg);
                draw_text_mono(back, w, h, x0, row_top + (int)font->max_top, font, hlines[hi], row_fg, row_bg);
            }
        }

        draw_toast(back, w, h, font, bg_idx, &lay, view_rows, twdoc_cur_page(&doc), twdoc_num_pages(&doc), &toast,
                   now);

        if (!img) {
            img = XCreateImage(dpy, DefaultVisual(dpy, screen), (unsigned)DefaultDepth(dpy, screen),
                               ZPixmap, 0, (char *)back, (unsigned)w, (unsigned)h, 32, 0);
            if (!img) {
                fprintf(stderr, "XCreateImage failed\n");
                goto out;
            }
        }

        XPutImage(dpy, win, gc, img, 0, 0, 0, 0, (unsigned)w, (unsigned)h);
        XFlush(dpy);
        /* crude pacing */
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 16 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }

out:
    if (cfg_save_path[0]) {
        TwX11AppSettings snap;
        fill_settings_snap(&snap, font_idx, bg_idx, cursor_mode, line_num_mode, page_margins, cols_margined,
                           typewriter_mode, &doc, status_pulse_idx, saved_start_fullscreen, w, h);
        (void)tw_x11_settings_save(cfg_save_path, &snap);
    }
    if (img) {
        img->data = NULL;
        XDestroyImage(img);
    }
    free(back);
    twdoc_destroy(&doc);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}

