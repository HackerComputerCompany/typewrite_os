#include "tw_core.h"
#include "tw_bitmapfont_uefi.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

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

typedef struct {
    int margin_px;
    int gutter_px;
    int paper_x, paper_y, paper_w, paper_h;
    int text_x0, text_y0;
    int cols, rows;
} ViewLayout;

static void compute_view_layout(int win_w, int win_h, const TwBitmapFont *font, int page_margins,
                                int show_line_numbers, int cols_margined, ViewLayout *L) {
    const int cell_w = (int)font->max_width + 1;
    const int cell_h = (int)font->line_box;

    int gutter_px = 0;
    if (show_line_numbers) {
        gutter_px = cell_w * 3 + 10;
        if (gutter_px < cell_w * 4)
            gutter_px = cell_w * 4;
        if (gutter_px < 24)
            gutter_px = 24;
    }

    int margin_px = 0;
    if (page_margins) {
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

    L->margin_px = margin_px;
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

static void render(uint32_t *pix, int w, int h, const TwCore *tw, const TwBitmapFont *font,
                   CursorMode cursor_mode, int cursor_visible, int bg_idx, const ViewLayout *lay,
                   int page_margins, int show_line_numbers) {
    const uint32_t paper_bg = pack_xrgb8888(kBgColors[bg_idx % 10]);
    const uint32_t outer_bg = page_margins ? pack_xrgb8888(0x0a0a0au) : paper_bg;
    const uint32_t fg = (bg_idx == 1) ? pack_xrgb8888(0x1e1e1eu) : pack_xrgb8888(0xf0f0e6u);
    const uint32_t cur = fg;

    const int cell_w = (int)font->max_width + 1;
    const int cell_h = (int)font->line_box;
    const int cols = (tw->cols < lay->cols) ? tw->cols : lay->cols;
    const int rows = (tw->rows < lay->rows) ? tw->rows : lay->rows;

    for (int i = 0; i < w * h; i++)
        pix[i] = outer_bg;

    if (page_margins)
        fill_rect(pix, w, h, lay->paper_x, lay->paper_y, lay->paper_w, lay->paper_h, paper_bg);

    if (show_line_numbers && lay->gutter_px > 0) {
        uint32_t ln_fg = line_number_ink(bg_idx);
        for (int y = 0; y < rows; y++) {
            char nb[16];
            snprintf(nb, sizeof(nb), "%d", y + 1);
            int baselineY = lay->text_y0 + y * cell_h + (int)font->max_top;
            int nx = lay->text_x0 - (int)strlen(nb) * cell_w - 4;
            if (nx < lay->paper_x + lay->margin_px + 2)
                nx = lay->paper_x + lay->margin_px + 2;
            draw_text_mono(pix, w, h, nx, baselineY, font, nb, ln_fg, paper_bg);
        }
    }

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            unsigned char c = (unsigned char)tw->cells[y * tw->cols + x];
            int px = lay->text_x0 + x * cell_w;
            int baselineY = lay->text_y0 + y * cell_h + (int)font->max_top;
            tw_uefi_font_draw_char(font, pix, w, h, px, baselineY, c, fg, paper_bg);
        }
    }

    if (cursor_mode != CURSOR_HIDDEN && cursor_visible) {
        int cx = tw->cx;
        int cy = tw->cy;
        if ((unsigned)cx < (unsigned)cols && (unsigned)cy < (unsigned)rows) {
            int x0 = lay->text_x0 + cx * cell_w;
            int y0 = lay->text_y0 + cy * cell_h;
            if (cursor_mode == CURSOR_BLOCK || cursor_mode == CURSOR_BLOCK_BLINK) {
                fill_rect(pix, w, h, x0, y0, cell_w, cell_h, cur);
                unsigned char c = (unsigned char)tw->cells[cy * tw->cols + cx];
                int baselineY = lay->text_y0 + cy * cell_h + (int)font->max_top;
                tw_uefi_font_draw_char(font, pix, w, h, x0, baselineY, c, paper_bg, cur);
            } else if (cursor_mode == CURSOR_BAR || cursor_mode == CURSOR_BAR_BLINK) {
                fill_rect(pix, w, h, x0, y0, 2, cell_h, cur);
            }
        }
    }
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

static int save_to_file(const char *path, const TwCore *tw) {
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return -1;
    for (int y = 0; y < tw->rows; y++) {
        int end = tw->cols;
        while (end > 0 && tw->cells[y * tw->cols + (end - 1)] == ' ')
            end--;
        if (end == 0) {
            fputc('\n', fp);
            continue;
        }
        fwrite(tw->cells + y * tw->cols, 1, (size_t)end, fp);
        fputc('\n', fp);
    }
    fclose(fp);
    return 0;
}

static int load_from_file(const char *path, TwCore *tw) {
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;
    tw_core_clear(tw);
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\r')
            continue;
        if (ch == '\n') {
            tw_core_newline(tw);
            continue;
        }
        if (ch == '\t') {
            for (int i = 0; i < 4; i++)
                tw_core_putc(tw, ' ');
            continue;
        }
        if (is_printable_ascii((unsigned char)ch))
            tw_core_putc(tw, (char)ch);
    }
    fclose(fp);
    return 0;
}

static void usage(const char *argv0) {
    fprintf(stderr, "Usage: %s [--fullscreen] [-f FILE] [FILE]\n", argv0);
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

int main(int argc, char **argv) {
    Display *dpy = NULL;
    int screen = 0;
    Window win;
    GC gc;
    Atom wm_delete;
    Atom a_wm_state;
    Atom a_wm_state_fullscreen;

    int w = 960;
    int h = 540;

    int start_fullscreen = 0;
    char filename[PATH_MAX];
    filename[0] = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--fullscreen") == 0) {
            start_fullscreen = 1;
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

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "XOpenDisplay failed\n");
        return 1;
    }
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

    TwCore tw;
    uint32_t *back = NULL;
    XImage *img = NULL;
    int font_idx = 2; /* Special Elite */
    const TwBitmapFont *font = tw_uefi_font_get(font_idx);
    CursorMode cursor_mode = CURSOR_BLOCK_BLINK;
    int bg_idx = 1; /* Cream */
    int show_help = 0;
    int dirty = 0;
    uint64_t last_autosave_ms = mono_ms();
    int page_margins = 1;
    int show_line_numbers = 0;
    int cols_margined = COLS_MARGINED_DEFAULT;

    ViewLayout init_lay;
    compute_view_layout(w, h, font, page_margins, show_line_numbers, cols_margined, &init_lay);
    if (tw_core_init(&tw, init_lay.cols, init_lay.rows) != 0) {
        fprintf(stderr, "tw_core_init failed\n");
        XCloseDisplay(dpy);
        return 1;
    }

    if (filename[0] == 0) {
        /* Autoload default file if present. */
        snprintf(filename, sizeof(filename), "Typewriter.txt");
        if (load_from_file(filename, &tw) == 0) {
            dirty = 0;
        } else {
            filename[0] = 0;
        }
    } else {
        (void)load_from_file(filename, &tw);
        dirty = 0;
    }

    update_title(dpy, win, filename);

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

                if (ks == XK_Escape)
                    goto out;
                if ((ks == XK_F11 || ks1 == XK_F11) && a_wm_state != None && a_wm_state_fullscreen != None) {
                    net_wm_state(dpy, screen, win, a_wm_state, a_wm_state_fullscreen, 2);
                }
                if (ks == XK_F1) {
                    show_help = !show_help;
                }
                if (ks == XK_F2) {
                    font_idx = (font_idx + 1) % tw_uefi_font_count();
                    font = tw_uefi_font_get(font_idx);
                    update_title(dpy, win, filename);
                }
                if (ks == XK_F3) {
                    cursor_mode = (CursorMode)((cursor_mode + 1) % CURSOR_MODE_NUM);
                }
                if (ks == XK_F4) {
                    bg_idx = (bg_idx + 1) % 10;
                }
                if (ks == XK_F5) {
                    show_line_numbers = !show_line_numbers;
                }
                if (ks == XK_F6) {
                    page_margins = !page_margins;
                }
                if (ks == XK_F7) {
                    cols_margined++;
                    if (cols_margined > COLS_MARGINED_MAX)
                        cols_margined = COLS_MARGINED_MIN;
                }
                if ((ev.xkey.state & ControlMask) && (ks == XK_s || ks == XK_S)) {
                    if (filename[0] == 0) {
                        snprintf(filename, sizeof(filename), "Typewriter.txt");
                    }
                    if (save_to_file(filename, &tw) == 0) {
                        dirty = 0;
                        last_autosave_ms = mono_ms();
                    }
                    update_title(dpy, win, filename);
                }
                if (ks == XK_BackSpace) {
                    tw_core_backspace(&tw);
                    dirty = 1;
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    tw_core_newline(&tw);
                    dirty = 1;
                } else if (n > 0 && is_printable_ascii((unsigned char)buf[0])) {
                    tw_core_putc(&tw, buf[0]);
                    dirty = 1;
                }
            }
        }

        ViewLayout lay;
        compute_view_layout(w, h, font, page_margins, show_line_numbers, cols_margined, &lay);

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

        if (tw_core_resize(&tw, lay.cols, lay.rows) != 0) {
            fprintf(stderr, "tw_core_resize failed\n");
            goto out;
        }

        uint64_t now = mono_ms();
        int cursor_visible = 1;
        if (cursor_mode == CURSOR_BAR_BLINK || cursor_mode == CURSOR_BLOCK_BLINK) {
            cursor_visible = ((now / 500u) % 2u) == 0u;
        }

        if (filename[0] != 0 && dirty && (now - last_autosave_ms) >= 30000u) {
            if (save_to_file(filename, &tw) == 0) {
                dirty = 0;
                last_autosave_ms = now;
            } else {
                last_autosave_ms = now; /* avoid tight retry loop */
            }
        }

        render(back, w, h, &tw, font, cursor_mode, cursor_visible, bg_idx, &lay, page_margins,
               show_line_numbers);

        if (show_help) {
            const uint32_t panel = pack_xrgb8888(0x1c2028u);
            const uint32_t panel2 = pack_xrgb8888(0x3a404cu);
            const uint32_t fg = pack_xrgb8888(0xfff5c8u);
            const uint32_t bg = panel;

            int step = (int)font->line_box;
            int inner = 8;
            int bw = (int)(font->max_width + 1) * 54 + inner * 2;
            int bh = step * 13 + inner * 2;
            int bx = (w > bw) ? (w - bw) / 2 : 10;
            int by = (h > bh) ? (h - bh) / 2 : 10;

            fill_rect(back, w, h, bx - 2, by - 2, bw + 4, bh + 4, panel2);
            fill_rect(back, w, h, bx, by, bw, bh, panel);

            int x0 = bx + inner;
            int y0 = by + inner + (int)font->max_top;
            draw_text_mono(back, w, h, x0, y0, font, "Help", fg, bg);
            y0 += step;
            draw_text_mono(back, w, h, x0, y0, font, "F1    toggle help", fg, bg); y0 += step;
            draw_text_mono(back, w, h, x0, y0, font, "F2    cycle font", fg, bg); y0 += step;
            draw_text_mono(back, w, h, x0, y0, font, "F3    cycle cursor", fg, bg); y0 += step;
            {
                char b[128];
                snprintf(b, sizeof(b), "F4    background: %s", kBgLabels[bg_idx % 10]);
                draw_text_mono(back, w, h, x0, y0, font, b, fg, bg);
            }
            y0 += step;
            draw_text_mono(back, w, h, x0, y0, font, "F5    line numbers on/off", fg, bg); y0 += step;
            draw_text_mono(back, w, h, x0, y0, font, "F6    page margins (Letter) on/off", fg, bg); y0 += step;
            {
                char b[128];
                snprintf(b, sizeof(b), "F7    chars/line %d-%d (margins on): %d", COLS_MARGINED_MIN,
                         COLS_MARGINED_MAX, cols_margined);
                draw_text_mono(back, w, h, x0, y0, font, b, fg, bg);
            }
            y0 += step;
            draw_text_mono(back, w, h, x0, y0, font, "F11   fullscreen (often taken by WM)", fg, bg); y0 += step;
            draw_text_mono(back, w, h, x0, y0, font, "      use: x11typewrite --fullscreen", fg, bg); y0 += step;
            draw_text_mono(back, w, h, x0, y0, font, "Ctrl+S save (autosave every 30s after)", fg, bg); y0 += step;
            draw_text_mono(back, w, h, x0, y0, font, "Esc   exit", fg, bg);
        }

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
    if (img) {
        img->data = NULL;
        XDestroyImage(img);
    }
    free(back);
    tw_core_destroy(&tw);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}

