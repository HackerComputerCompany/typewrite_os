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

static void render(uint32_t *pix, int w, int h, const TwCore *tw, const TwBitmapFont *font,
                   CursorMode cursor_mode, int cursor_visible) {
    const uint32_t bg = pack_xrgb8888(0x101010u);
    const uint32_t fg = pack_xrgb8888(0xe8e0d0u);
    const uint32_t cur = pack_xrgb8888(0x88c0ffu);

    const int cell_w = (int)font->max_width + 1;
    const int cell_h = (int)font->line_box;
    const int max_cols = (cell_w > 0) ? (w / cell_w) : 0;
    const int max_rows = (cell_h > 0) ? (h / cell_h) : 0;
    const int cols = (tw->cols < max_cols) ? tw->cols : max_cols;
    const int rows = (tw->rows < max_rows) ? tw->rows : max_rows;

    for (int i = 0; i < w * h; i++)
        pix[i] = bg;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            unsigned char c = (unsigned char)tw->cells[y * tw->cols + x];
            int baselineY = y * cell_h + (int)font->max_top;
            tw_uefi_font_draw_char(font, pix, w, h, x * cell_w, baselineY, c, fg, bg);
        }
    }

    if (cursor_mode != CURSOR_HIDDEN && cursor_visible) {
        int cx = tw->cx;
        int cy = tw->cy;
        if ((unsigned)cx < (unsigned)cols && (unsigned)cy < (unsigned)rows) {
            int x0 = cx * cell_w;
            int y0 = cy * cell_h;
            if (cursor_mode == CURSOR_BLOCK || cursor_mode == CURSOR_BLOCK_BLINK) {
                fill_rect(pix, w, h, x0, y0, cell_w, cell_h, cur);
                unsigned char c = (unsigned char)tw->cells[cy * tw->cols + cx];
                int baselineY = cy * cell_h + (int)font->max_top;
                tw_uefi_font_draw_char(font, pix, w, h, x0, baselineY, c, fg, cur);
            } else if (cursor_mode == CURSOR_BAR || cursor_mode == CURSOR_BAR_BLINK) {
                fill_rect(pix, w, h, x0, y0, 2, cell_h, cur);
            }
        }
    }
}

static int is_printable_ascii(unsigned char c) {
    return c >= 32 && c < 127;
}

static void update_title(Display *dpy, Window win, const TwBitmapFont *font, const char *filename) {
    char title[256];
    if (filename && filename[0])
        snprintf(title, sizeof(title), "linux-typewrite-x11 — %s — %s", filename, font ? font->name : "font");
    else
        snprintf(title, sizeof(title), "linux-typewrite-x11 — (no file) — %s", font ? font->name : "font");
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
    fprintf(stderr, "Usage: %s [-f FILE] [FILE]\n", argv0);
}

static void toggle_fullscreen(Display *dpy, int screen, Window win, Atom a_wm_state, Atom a_fullscreen) {
    XEvent e;
    memset(&e, 0, sizeof(e));
    e.xclient.type = ClientMessage;
    e.xclient.serial = 0;
    e.xclient.send_event = True;
    e.xclient.display = dpy;
    e.xclient.window = win;
    e.xclient.message_type = a_wm_state;
    e.xclient.format = 32;
    /* _NET_WM_STATE_TOGGLE = 2 */
    e.xclient.data.l[0] = 2;
    e.xclient.data.l[1] = (long)a_fullscreen;
    e.xclient.data.l[2] = 0;
    e.xclient.data.l[3] = 1;
    e.xclient.data.l[4] = 0;

    XSendEvent(dpy, RootWindow(dpy, screen), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &e);
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

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "XOpenDisplay failed\n");
        return 1;
    }
    screen = DefaultScreen(dpy);

    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, (unsigned)w, (unsigned)h, 0,
                              BlackPixel(dpy, screen), BlackPixel(dpy, screen));
    XStoreName(dpy, win, "linux-typewrite-x11");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(dpy, win);

    gc = XCreateGC(dpy, win, 0, NULL);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    a_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    a_wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

    TwCore tw;
    uint32_t *back = NULL;
    XImage *img = NULL;
    int font_idx = 2; /* Special Elite */
    const TwBitmapFont *font = tw_uefi_font_get(font_idx);
    CursorMode cursor_mode = CURSOR_BLOCK_BLINK;
    int dirty = 0;
    uint64_t last_autosave_ms = mono_ms();
    char filename[PATH_MAX];
    filename[0] = 0;

    /* CLI: -f FILE or positional FILE */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            XCloseDisplay(dpy);
            return 0;
        }
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            snprintf(filename, sizeof(filename), "%s", argv[i + 1]);
            i++;
            continue;
        }
        if (argv[i][0] != '-') {
            snprintf(filename, sizeof(filename), "%s", argv[i]);
            continue;
        }
    }

    int cols = w / ((int)font->max_width + 1);
    int rows = h / (int)font->line_box;
    if (cols < 10) cols = 10;
    if (rows < 5) rows = 5;
    if (tw_core_init(&tw, cols, rows) != 0) {
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

    update_title(dpy, win, font, filename);

    for (const char *p = "linux-typewrite-x11: ESC exit, F2 font, F3 cursor, Ctrl+S save"; *p; p++)
        tw_core_putc(&tw, *p);
    tw_core_newline(&tw);

    for (;;) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) {
                goto out;
            }

            if (ev.type == ConfigureNotify) {
                if (ev.xconfigure.width != w || ev.xconfigure.height != h) {
                    w = ev.xconfigure.width;
                    h = ev.xconfigure.height;
                }
            }

            if (ev.type == KeyPress) {
                KeySym ks = 0;
                char buf[32];
                int n = XLookupString(&ev.xkey, buf, (int)sizeof(buf), &ks, NULL);

                if (ks == XK_Escape)
                    goto out;
                if (ks == XK_F11) {
                    toggle_fullscreen(dpy, screen, win, a_wm_state, a_wm_state_fullscreen);
                }
                if (ks == XK_F2) {
                    font_idx = (font_idx + 1) % tw_uefi_font_count();
                    font = tw_uefi_font_get(font_idx);
                    update_title(dpy, win, font, filename);
                }
                if (ks == XK_F3) {
                    cursor_mode = (CursorMode)((cursor_mode + 1) % CURSOR_MODE_NUM);
                }
                if ((ev.xkey.state & ControlMask) && (ks == XK_s || ks == XK_S)) {
                    if (filename[0] == 0) {
                        snprintf(filename, sizeof(filename), "Typewriter.txt");
                    }
                    if (save_to_file(filename, &tw) == 0) {
                        dirty = 0;
                        last_autosave_ms = mono_ms();
                    }
                    update_title(dpy, win, font, filename);
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

        int need_recreate = 0;
        if (!back) need_recreate = 1;
        if (img && (img->width != w || img->height != h)) need_recreate = 1;

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

            cols = w / ((int)font->max_width + 1);
            rows = h / (int)font->line_box;
            if (cols < 10) cols = 10;
            if (rows < 5) rows = 5;
            if (tw_core_resize(&tw, cols, rows) != 0) {
                fprintf(stderr, "tw_core_resize failed\n");
                goto out;
            }
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

        render(back, w, h, &tw, font, cursor_mode, cursor_visible);

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

