#pragma once

#include <stddef.h>

typedef struct {
    int cols;
    int rows;
    int cx;
    int cy;
    char *cells; /* rows*cols */
} TwCore;

int tw_core_init(TwCore *tw, int cols, int rows);
void tw_core_destroy(TwCore *tw);

void tw_core_clear(TwCore *tw);
void tw_core_putc(TwCore *tw, char c);
void tw_core_backspace(TwCore *tw);
void tw_core_newline(TwCore *tw);

static inline const char *tw_core_cells(const TwCore *tw) { return tw->cells; }

