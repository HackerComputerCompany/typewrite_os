#include "tw_core.h"
#include "tw_fbdev.h"
#include "tw_font8x8.h"
#include "tw_termios.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop;

static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

static void draw_glyph(TwFb *fb, int x0, int y0, unsigned char c, uint32_t fg, uint32_t bg) {
    const uint8_t *rows = tw_font8x8_glyph(c);
    for (int y = 0; y < 8; y++) {
        uint8_t m = rows[y];
        for (int x = 0; x < 8; x++) {
            uint32_t col = (m & (0x80u >> x)) ? fg : bg;
            tw_fbdev_put_pixel(fb, x0 + x, y0 + y, col);
        }
    }
}

static void render(TwFb *fb, const TwCore *tw) {
    const uint32_t bg = 0x00101010u;
    const uint32_t fg = 0x00e8e0d0u;
    const uint32_t cur = 0x0088c0ffu;

    const int cell_w = 8;
    const int cell_h = 8;
    const int max_cols = fb->width / cell_w;
    const int max_rows = fb->height / cell_h;
    const int cols = (tw->cols < max_cols) ? tw->cols : max_cols;
    const int rows = (tw->rows < max_rows) ? tw->rows : max_rows;

    tw_fbdev_fill(fb, bg);

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            unsigned char c = (unsigned char)tw->cells[y * tw->cols + x];
            uint32_t cell_bg = bg;
            if (x == tw->cx && y == tw->cy)
                cell_bg = cur;
            draw_glyph(fb, x * cell_w, y * cell_h, c, fg, cell_bg);
        }
    }
}

int main(int argc, char **argv) {
    const char *fb_path = "/dev/fb0";
    TwFb fb;
    TwCore tw;

    if (argc > 1)
        fb_path = argv[1];

    if (tw_fbdev_open(&fb, fb_path) != 0) {
        fprintf(stderr, "Failed to open %s (need fbdev + 32bpp). Try: sudo %s\n", fb_path, argv[0]);
        return 1;
    }

    /* Fit a simple text grid. */
    int cols = fb.width / 8;
    int rows = fb.height / 8;
    if (cols < 10) cols = 10;
    if (rows < 5) rows = 5;
    if (tw_core_init(&tw, cols, rows) != 0) {
        fprintf(stderr, "tw_core_init failed\n");
        tw_fbdev_close(&fb);
        return 1;
    }

    (void)signal(SIGINT, on_sigint);
    (void)signal(SIGTERM, on_sigint);

    if (tw_termios_raw_enter() != 0) {
        fprintf(stderr, "warning: failed to enter raw terminal mode; input may not work\n");
    }

    tw_core_clear(&tw);
    const char *banner = "linux-typewrite: Ctrl+C to exit";
    for (const char *p = banner; *p; p++)
        tw_core_putc(&tw, *p);
    tw_core_newline(&tw);

    while (!g_stop) {
        unsigned char ch;
        ssize_t n = read(STDIN_FILENO, &ch, 1);
        if (n == 1) {
            if (ch == '\r' || ch == '\n')
                tw_core_newline(&tw);
            else if (ch == 0x7f || ch == '\b')
                tw_core_backspace(&tw);
            else
                tw_core_putc(&tw, (char)ch);
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            break;
        }

        render(&fb, &tw);
        usleep(16 * 1000);
    }

    tw_termios_raw_leave();
    tw_core_destroy(&tw);
    tw_fbdev_close(&fb);
    return 0;
}

