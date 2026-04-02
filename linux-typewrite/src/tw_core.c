#include "tw_core.h"

#include <stdlib.h>
#include <string.h>

int tw_core_init(TwCore *tw, int cols, int rows) {
    if (!tw || cols <= 0 || rows <= 0)
        return -1;
    memset(tw, 0, sizeof(*tw));
    tw->cols = cols;
    tw->rows = rows;
    tw->cells = (char *)malloc((size_t)cols * (size_t)rows);
    if (!tw->cells)
        return -1;
    tw_core_clear(tw);
    return 0;
}

void tw_core_destroy(TwCore *tw) {
    if (!tw)
        return;
    free(tw->cells);
    memset(tw, 0, sizeof(*tw));
}

void tw_core_clear(TwCore *tw) {
    if (!tw || !tw->cells)
        return;
    memset(tw->cells, ' ', (size_t)tw->cols * (size_t)tw->rows);
    tw->cx = 0;
    tw->cy = 0;
}

static void scroll_up(TwCore *tw) {
    size_t row_sz = (size_t)tw->cols;
    memmove(tw->cells, tw->cells + row_sz, row_sz * (size_t)(tw->rows - 1));
    memset(tw->cells + row_sz * (size_t)(tw->rows - 1), ' ', row_sz);
    if (tw->cy > 0)
        tw->cy--;
}

void tw_core_newline(TwCore *tw) {
    if (!tw || !tw->cells)
        return;
    tw->cx = 0;
    tw->cy++;
    if (tw->cy >= tw->rows) {
        tw->cy = tw->rows - 1;
        scroll_up(tw);
    }
}

void tw_core_putc(TwCore *tw, char c) {
    if (!tw || !tw->cells)
        return;
    if (c == '\n') {
        tw_core_newline(tw);
        return;
    }
    if (c == '\r') {
        tw->cx = 0;
        return;
    }
    if (c == '\b' || c == 127) {
        tw_core_backspace(tw);
        return;
    }
    if ((unsigned char)c < 32)
        return;

    tw->cells[tw->cy * tw->cols + tw->cx] = c;
    tw->cx++;
    if (tw->cx >= tw->cols) {
        tw->cx = 0;
        tw->cy++;
        if (tw->cy >= tw->rows) {
            tw->cy = tw->rows - 1;
            scroll_up(tw);
        }
    }
}

void tw_core_backspace(TwCore *tw) {
    if (!tw || !tw->cells)
        return;
    if (tw->cx > 0) {
        tw->cx--;
    } else if (tw->cy > 0) {
        tw->cy--;
        tw->cx = tw->cols - 1;
    } else {
        return;
    }
    tw->cells[tw->cy * tw->cols + tw->cx] = ' ';
}

