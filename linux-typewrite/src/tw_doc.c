#include "tw_doc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int row_last_nonempty(const TwCore *tw, int row) {
    int c = tw->cols - 1;
    while (c >= 0 && tw->cells[(size_t)row * (size_t)tw->cols + (size_t)c] == ' ')
        c--;
    return c;
}

/* Last row index on page with any non-space character, or -1 if page is all blank. */
static int page_last_nonempty_row(const TwCore *tw) {
    if (!tw)
        return -1;
    for (int r = tw->rows - 1; r >= 0; r--) {
        if (row_last_nonempty(tw, r) >= 0)
            return r;
    }
    return -1;
}

/* Highest page index that has at least one non-space character, or -1 if document empty. */
static void twdoc_find_last_nonempty_page(const TwDoc *d, int *out_last_p) {
    *out_last_p = -1;
    if (!d || !d->pages)
        return;
    for (int p = d->n_pages - 1; p >= 0; p--) {
        if (page_last_nonempty_row(&d->pages[p]) >= 0) {
            *out_last_p = p;
            return;
        }
    }
}

static int twdoc_grow(TwDoc *d, int need_pages) {
    if (need_pages <= 0)
        return -1;
    if (need_pages <= d->n_pages)
        return 0;

    if (!d->pages) {
        int cap = need_pages < 4 ? 4 : need_pages;
        d->pages = (TwCore *)calloc((size_t)cap, sizeof(TwCore));
        if (!d->pages)
            return -1;
        d->cap_pages = cap;
        for (int i = 0; i < need_pages; i++) {
            if (tw_core_init(&d->pages[i], d->cols, d->rows) != 0)
                return -1;
        }
        d->n_pages = need_pages;
        return 0;
    }

    if (need_pages > d->cap_pages) {
        int ncap = d->cap_pages;
        while (ncap < need_pages)
            ncap *= 2;
        TwCore *np = (TwCore *)realloc(d->pages, (size_t)ncap * sizeof(TwCore));
        if (!np)
            return -1;
        memset(np + d->n_pages, 0, (size_t)(ncap - d->n_pages) * sizeof(TwCore));
        d->pages = np;
        d->cap_pages = ncap;
    }
    while (d->n_pages < need_pages) {
        if (tw_core_init(&d->pages[d->n_pages], d->cols, d->rows) != 0)
            return -1;
        d->n_pages++;
    }
    return 0;
}

static void twdoc_shrink_pages(TwDoc *d, int new_count) {
    if (new_count < 0)
        new_count = 0;
    while (d->n_pages > new_count) {
        d->n_pages--;
        tw_core_destroy(&d->pages[d->n_pages]);
    }
    if (new_count == 0 && d->pages) {
        free(d->pages);
        d->pages = NULL;
        d->cap_pages = 0;
    }
    if (d->n_pages > 0 && d->cur_page >= d->n_pages)
        d->cur_page = d->n_pages - 1;
    if (d->n_pages == 0)
        d->cur_page = 0;
}

int twdoc_init(TwDoc *d, int cols, int rows) {
    if (!d || cols <= 0 || rows <= 0)
        return -1;
    memset(d, 0, sizeof(*d));
    d->cols = cols;
    d->rows = rows;
    d->cap_pages = 0;
    d->insert_mode = 0; /* default typeover; Insert toggles insert (shift) */
    d->word_wrap = 1;
    return twdoc_grow(d, 1);
}

void twdoc_destroy(TwDoc *d) {
    if (!d)
        return;
    for (int i = 0; i < d->n_pages; i++)
        tw_core_destroy(&d->pages[i]);
    free(d->pages);
    memset(d, 0, sizeof(*d));
}

TwCore *twdoc_cur(TwDoc *d) {
    if (!d || d->n_pages <= 0 || d->cur_page < 0 || d->cur_page >= d->n_pages)
        return NULL;
    return &d->pages[d->cur_page];
}

const TwCore *twdoc_cur_const(const TwDoc *d) {
    if (!d || d->n_pages <= 0 || d->cur_page < 0 || d->cur_page >= d->n_pages)
        return NULL;
    return &d->pages[d->cur_page];
}

int twdoc_cur_page(const TwDoc *d) {
    return d ? d->cur_page : 0;
}

int twdoc_num_pages(const TwDoc *d) {
    return d ? d->n_pages : 0;
}

void twdoc_clear(TwDoc *d) {
    if (!d)
        return;
    twdoc_shrink_pages(d, 1);
    if (d->n_pages < 1 && twdoc_grow(d, 1) != 0)
        return;
    d->cur_page = 0;
    tw_core_clear(&d->pages[0]);
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} LineBuf;

static int lb_push_line(LineBuf *lb, const char *src, int nchars) {
    while (lb->len + (size_t)nchars + 2 > lb->cap) {
        size_t nc = lb->cap ? lb->cap * 2 : 256;
        char *p = (char *)realloc(lb->data, nc);
        if (!p)
            return -1;
        lb->data = p;
        lb->cap = nc;
    }
    memcpy(lb->data + lb->len, src, (size_t)nchars);
    lb->len += (size_t)nchars;
    lb->data[lb->len++] = '\n';
    return 0;
}

static void lb_free(LineBuf *lb) {
    free(lb->data);
    lb->data = NULL;
    lb->len = lb->cap = 0;
}

/*
 * Flatten document to lines (trailing spaces stripped per row).
 * Omits trailing blank rows on each page and trailing wholly blank pages, so reflow
 * matches save format and does not grow spurious pages from grid padding.
 * A blank intermediate page is emitted as `rows` empty lines to preserve page breaks.
 */
static int twdoc_flatten(const TwDoc *d, LineBuf *out) {
    memset(out, 0, sizeof(*out));
    int last_p;
    twdoc_find_last_nonempty_page(d, &last_p);
    if (last_p < 0)
        return 0;

    for (int p = 0; p <= last_p; p++) {
        const TwCore *tw = &d->pages[p];
        int lr = page_last_nonempty_row(tw);
        if (lr < 0) {
            for (int y = 0; y < tw->rows; y++) {
                if (lb_push_line(out, tw->cells + (size_t)y * (size_t)tw->cols, 0) != 0)
                    return -1;
            }
            continue;
        }
        for (int y = 0; y <= lr; y++) {
            int end = row_last_nonempty(tw, y) + 1;
            if (end < 0)
                end = 0;
            const char *row = tw->cells + (size_t)y * (size_t)tw->cols;
            if (lb_push_line(out, row, end) != 0)
                return -1;
        }
    }
    return 0;
}

static int refill_from_lines(TwDoc *d, LineBuf *lb, int cols, int rows) {
    int n_lines = 0;
    if (lb->len > 0) {
        for (size_t i = 0; i < lb->len; i++) {
            if (lb->data[i] == '\n')
                n_lines++;
        }
    }

    int old_cp = d->cur_page;
    TwCore *old_tw = twdoc_cur(d);
    int old_cx = old_tw ? old_tw->cx : 0;
    int old_cy = old_tw ? old_tw->cy : 0;
    int glob_line = old_cp * d->rows + old_cy;

    twdoc_shrink_pages(d, 0);
    d->cols = cols;
    d->rows = rows;

    int npg = n_lines > 0 ? (n_lines + rows - 1) / rows : 1;
    if (npg < 1)
        npg = 1;
    if (twdoc_grow(d, npg) != 0)
        return -1;

    size_t pos = 0;
    for (int p = 0; p < npg; p++) {
        TwCore *tw = &d->pages[p];
        tw_core_clear(tw);
        for (int r = 0; r < rows; r++) {
            if (pos > lb->len)
                break;
            if (pos == lb->len)
                break;
            char *nl = memchr(lb->data + pos, '\n', lb->len - pos);
            if (!nl)
                break;
            size_t linelen = (size_t)(nl - (lb->data + pos));
            int copy = (int)linelen < cols ? (int)linelen : cols;
            if (copy > 0)
                memcpy(tw->cells + (size_t)r * (size_t)cols, lb->data + pos, (size_t)copy);
            pos = (size_t)(nl - lb->data) + 1;
        }
    }

    if (glob_line < 0)
        glob_line = 0;
    int max_line = npg * rows - 1;
    if (glob_line > max_line)
        glob_line = max_line;
    int out_cp = glob_line / rows;
    int out_cy = glob_line % rows;
    int out_cx = old_cx;
    if (out_cx >= cols)
        out_cx = cols - 1;
    if (out_cx < 0)
        out_cx = 0;

    TwCore *tw = &d->pages[out_cp];
    tw->cx = out_cx;
    tw->cy = out_cy;
    d->cur_page = out_cp;
    return 0;
}

static int twdoc_row_all_spaces(const TwCore *tw, int row_idx) {
    if (!tw || row_idx < 0 || row_idx >= tw->rows)
        return 1;
    const char *r = tw->cells + (size_t)row_idx * (size_t)tw->cols;
    for (int i = 0; i < tw->cols; i++) {
        if (r[i] != ' ')
            return 0;
    }
    return 1;
}

/*
 * After filling the current row (cx == cols), move the last word to the next row
 * if there is a break space and the target row is empty. Returns 1 if handled.
 */
static int twdoc_try_soft_wrap(TwDoc *d) {
    char buf[512];
    TwCore *tw = twdoc_cur(d);
    if (!tw)
        return 0;
    int cy = tw->cy;
    int cols = tw->cols;
    char *row = tw->cells + (size_t)cy * (size_t)cols;

    int last_ns = -1;
    for (int i = cols - 1; i >= 0; i--) {
        if (row[i] != ' ') {
            last_ns = i;
            break;
        }
    }
    if (last_ns < 0)
        return 0;

    int sp = -1;
    for (int i = last_ns - 1; i >= 0; i--) {
        if (row[i] == ' ') {
            sp = i;
            break;
        }
    }
    if (sp < 0)
        return 0;

    int tail_len = last_ns - sp;
    if (tail_len <= 0 || tail_len > cols)
        return 0;

    if (cy < tw->rows - 1) {
        if (!twdoc_row_all_spaces(tw, cy + 1))
            return 0;
    }

    if (tail_len > (int)sizeof(buf))
        return 0;
    memcpy(buf, row + sp + 1, (size_t)tail_len);
    memset(row + sp + 1, ' ', (size_t)(cols - 1 - sp));

    twdoc_newline(d);
    tw = twdoc_cur(d);
    if (!tw)
        return 0;
    char *nrow = tw->cells + (size_t)tw->cy * (size_t)tw->cols;
    memset(nrow, ' ', (size_t)cols);
    memcpy(nrow, buf, (size_t)tail_len);
    tw->cx = tail_len;
    return 1;
}

int twdoc_resize_reflow(TwDoc *d, int cols, int rows) {
    if (!d || cols <= 0 || rows <= 0)
        return -1;
    if (d->cols == cols && d->rows == rows)
        return 0;

    LineBuf lb;
    if (twdoc_flatten(d, &lb) != 0)
        return -1;

    int st = refill_from_lines(d, &lb, cols, rows);
    lb_free(&lb);
    return st;
}

void twdoc_newline(TwDoc *d) {
    TwCore *tw = twdoc_cur(d);
    if (!tw)
        return;
    tw->cx = 0;
    if (tw->cy < tw->rows - 1) {
        tw->cy++;
        return;
    }
    /* Last row -> next page */
    d->cur_page++;
    if (twdoc_grow(d, d->cur_page + 1) != 0) {
        d->cur_page = d->n_pages - 1;
        return;
    }
    tw = twdoc_cur(d);
    tw->cx = 0;
    tw->cy = 0;
}

void twdoc_putc(TwDoc *d, char c) {
    TwCore *tw = twdoc_cur(d);
    if (!tw)
        return;
    if (c == '\n') {
        twdoc_newline(d);
        return;
    }
    if (c == '\r') {
        tw->cx = 0;
        return;
    }
    if ((unsigned char)c < 32)
        return;

    char *row = tw->cells + (size_t)tw->cy * (size_t)tw->cols;
    if (d->insert_mode && tw->cx < tw->cols - 1)
        memmove(row + tw->cx + 1, row + tw->cx, (size_t)(tw->cols - tw->cx - 1));
    row[tw->cx] = c;
    tw->cx++;
    if (tw->cx >= tw->cols) {
        if (d->word_wrap && twdoc_try_soft_wrap(d))
            return;
        tw->cx = 0;
        if (tw->cy < tw->rows - 1) {
            tw->cy++;
        } else {
            d->cur_page++;
            if (twdoc_grow(d, d->cur_page + 1) != 0) {
                d->cur_page = d->n_pages - 1;
                tw = twdoc_cur(d);
                tw->cx = tw->cols - 1;
                return;
            }
            tw = twdoc_cur(d);
            tw->cy = 0;
            tw->cx = 0;
        }
    }
}

void twdoc_backspace(TwDoc *d) {
    TwCore *tw = twdoc_cur(d);
    if (!tw)
        return;
    if (tw->cx > 0) {
        tw->cx--;
        char *row = tw->cells + (size_t)tw->cy * (size_t)tw->cols;
        if (d->insert_mode && tw->cx < tw->cols - 1)
            memmove(row + tw->cx, row + tw->cx + 1, (size_t)(tw->cols - tw->cx - 1));
        row[tw->cols - 1] = ' ';
        return;
    }
    if (tw->cy > 0) {
        tw->cy--;
        tw->cx = tw->cols - 1;
        tw->cells[(size_t)tw->cy * (size_t)tw->cols + (size_t)tw->cx] = ' ';
        return;
    }
    if (d->cur_page == 0)
        return;
    d->cur_page--;
    tw = twdoc_cur(d);
    for (int r = tw->rows - 1; r >= 0; r--) {
        int le = row_last_nonempty(tw, r);
        if (le >= 0) {
            tw->cy = r;
            tw->cx = le;
            tw->cells[(size_t)tw->cy * (size_t)tw->cols + (size_t)tw->cx] = ' ';
            return;
        }
    }
    tw->cx = 0;
    tw->cy = 0;
}

void twdoc_delete_forward(TwDoc *d) {
    TwCore *tw = twdoc_cur(d);
    if (!tw)
        return;
    char *row = tw->cells + (size_t)tw->cy * (size_t)tw->cols;
    if (d->insert_mode && tw->cx < tw->cols - 1) {
        memmove(row + tw->cx, row + tw->cx + 1, (size_t)(tw->cols - tw->cx - 1));
        row[tw->cols - 1] = ' ';
    } else {
        if (tw->cx < tw->cols)
            row[tw->cx] = ' ';
    }
}

int twdoc_save(const char *path, const TwDoc *d) {
    if (!d)
        return -1;
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return -1;

    int last_p;
    twdoc_find_last_nonempty_page(d, &last_p);
    if (last_p < 0) {
        fputc('\n', fp);
        fclose(fp);
        return 0;
    }

    for (int p = 0; p <= last_p; p++) {
        const TwCore *tw = &d->pages[p];
        if (p > 0)
            fputc('\f', fp);

        int lr = page_last_nonempty_row(tw);
        if (lr < 0) {
            /* Blank page before later content: preserve page break with empty lines. */
            for (int y = 0; y < tw->rows; y++)
                fputc('\n', fp);
            continue;
        }

        for (int y = 0; y <= lr; y++) {
            int end = tw->cols;
            while (end > 0 && tw->cells[(size_t)y * (size_t)tw->cols + (size_t)(end - 1)] == ' ')
                end--;
            if (end == 0) {
                fputc('\n', fp);
                continue;
            }
            fwrite(tw->cells + (size_t)y * (size_t)tw->cols, 1, (size_t)end, fp);
            fputc('\n', fp);
        }
    }
    fclose(fp);
    return 0;
}

int twdoc_load(const char *path, TwDoc *d) {
    if (!d || d->n_pages < 1)
        return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;

    twdoc_clear(d);
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\r')
            continue;
        if (ch == '\f') {
            d->cur_page++;
            if (twdoc_grow(d, d->cur_page + 1) != 0) {
                fclose(fp);
                return -1;
            }
            tw_core_clear(twdoc_cur(d));
            continue;
        }
        if (ch == '\n') {
            twdoc_newline(d);
            continue;
        }
        if (ch == '\t') {
            for (int i = 0; i < 4; i++)
                twdoc_putc(d, ' ');
            continue;
        }
        if ((unsigned char)ch >= 32 && (unsigned char)ch < 127)
            twdoc_putc(d, (char)ch);
    }
    fclose(fp);
    d->cur_page = 0;
    if (twdoc_cur(d)) {
        twdoc_cur(d)->cx = 0;
        twdoc_cur(d)->cy = 0;
    }
    return 0;
}
