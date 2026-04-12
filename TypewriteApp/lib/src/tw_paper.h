#pragma once

/*
 * Nominal physical page for layout (UI maps margins to pixels from window DPI, ~96).
 *
 * Document model (see tw_doc.h): a TwDoc is a sequence of Pages; each page is a
 * TwCore grid of Lines × column cells. Cells hold printable characters (letters,
 * digits, punctuation, space). “Words” are derived in the UI (whitespace-separated
 * tokens; each grid row boundary also ends a word for stats).
 *
 * Margins are the unprintable border inside the paper edge: left, right, top, bottom.
 * The X11 renderer keeps symmetric pixel margins today (one value → all four sides);
 * this struct is the stable place to grow per-side margin config (e.g. JSON, 1/100").
 */
#define TW_PAPER_WIDTH_NUM 17
#define TW_PAPER_WIDTH_DEN 2 /* 8.5" = 17/2 */
#define TW_PAPER_HEIGHT_IN 11

typedef struct {
    int top_hundredth_in;    /* default 100 ≈ 1" at 100 dpi nominal */
    int bottom_hundredth_in;
    int left_hundredth_in;
    int right_hundredth_in;
} TwPaperMargins;

static inline void tw_paper_margins_default_letter_1in(TwPaperMargins *m) {
    if (!m)
        return;
    m->top_hundredth_in = 100;
    m->bottom_hundredth_in = 100;
    m->left_hundredth_in = 100;
    m->right_hundredth_in = 100;
}
