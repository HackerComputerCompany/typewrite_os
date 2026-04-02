#include "tw_core.h"
#include "tw_font8x8.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { CELL_W = 8, CELL_H = 8 };

static uint32_t pack_xrgb8888(uint32_t rgb) {
    return 0xff000000u | (rgb & 0x00ffffffu);
}

static void put_px(uint32_t *pix, int w, int h, int x, int y, uint32_t xrgb) {
    if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h)
        return;
    pix[y * w + x] = xrgb;
}

static void draw_glyph(uint32_t *pix, int w, int h, int x0, int y0, unsigned char c, uint32_t fg, uint32_t bg) {
    const uint8_t *rows = tw_font8x8_glyph(c);
    for (int y = 0; y < 8; y++) {
        uint8_t m = rows[y];
        for (int x = 0; x < 8; x++) {
            uint32_t col = (m & (0x80u >> x)) ? fg : bg;
            put_px(pix, w, h, x0 + x, y0 + y, col);
        }
    }
}

static void render(uint32_t *pix, int w, int h, const TwCore *tw) {
    const uint32_t bg = pack_xrgb8888(0x101010u);
    const uint32_t fg = pack_xrgb8888(0xe8e0d0u);
    const uint32_t cur = pack_xrgb8888(0x88c0ffu);

    const int max_cols = w / CELL_W;
    const int max_rows = h / CELL_H;
    const int cols = (tw->cols < max_cols) ? tw->cols : max_cols;
    const int rows = (tw->rows < max_rows) ? tw->rows : max_rows;

    for (int i = 0; i < w * h; i++)
        pix[i] = bg;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            unsigned char c = (unsigned char)tw->cells[y * tw->cols + x];
            uint32_t cell_bg = (x == tw->cx && y == tw->cy) ? cur : bg;
            draw_glyph(pix, w, h, x * CELL_W, y * CELL_H, c, fg, cell_bg);
        }
    }
}

static int is_printable_ascii(unsigned char c) {
    return c >= 32 && c < 127;
}

int main(void) {
    Display *dpy = NULL;
    int screen = 0;
    Window win;
    GC gc;
    Atom wm_delete;

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

    TwCore tw;
    uint32_t *back = NULL;
    XImage *img = NULL;

    int cols = w / CELL_W;
    int rows = h / CELL_H;
    if (cols < 10) cols = 10;
    if (rows < 5) rows = 5;
    if (tw_core_init(&tw, cols, rows) != 0) {
        fprintf(stderr, "tw_core_init failed\n");
        XCloseDisplay(dpy);
        return 1;
    }

    for (const char *p = "linux-typewrite-x11: ESC to exit"; *p; p++)
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
                if (ks == XK_BackSpace) {
                    tw_core_backspace(&tw);
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    tw_core_newline(&tw);
                } else if (n > 0 && is_printable_ascii((unsigned char)buf[0])) {
                    tw_core_putc(&tw, buf[0]);
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

            cols = w / CELL_W;
            rows = h / CELL_H;
            if (cols < 10) cols = 10;
            if (rows < 5) rows = 5;
            /* Re-init core to match new grid size (simple bring-up choice). */
            tw_core_destroy(&tw);
            if (tw_core_init(&tw, cols, rows) != 0) {
                fprintf(stderr, "tw_core_init resize failed\n");
                goto out;
            }
        }

        render(back, w, h, &tw);

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

