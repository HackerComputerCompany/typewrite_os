#pragma once

#include "tw_core.h"

/*
 * Multi-page document: each page is a TwCore grid of d->cols × d->rows.
 * New page when the cursor leaves the last row (newline or wrap) instead of
 * scrolling the grid (which breaks typewriter-style rendering).
 *
 * Save/load: pages are separated by ASCII form feed '\f' in the file.
 */

typedef struct {
    TwCore *pages;
    int n_pages;
    int cap_pages;
    int cur_page;
    int cols;
    int rows;
    int insert_mode; /* non-zero: insert (shift line right); 0: typeover */
} TwDoc;

int twdoc_init(TwDoc *d, int cols, int rows);
void twdoc_destroy(TwDoc *d);

/* Resize every page and reflow text; preserves reading order. */
int twdoc_resize_reflow(TwDoc *d, int cols, int rows);

TwCore *twdoc_cur(TwDoc *d);
const TwCore *twdoc_cur_const(const TwDoc *d);

int twdoc_cur_page(const TwDoc *d);
int twdoc_num_pages(const TwDoc *d);

void twdoc_putc(TwDoc *d, char c);
void twdoc_newline(TwDoc *d);
void twdoc_backspace(TwDoc *d);
void twdoc_delete_forward(TwDoc *d);

void twdoc_clear(TwDoc *d);

int twdoc_save(const char *path, const TwDoc *d);
int twdoc_load(const char *path, TwDoc *d);
