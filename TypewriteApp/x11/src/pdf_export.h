#pragma once

#include "tw_doc.h"
#include "tw_bitmapfont_uefi.h"

/* Mirrors main_x11 LineNumMode values. */
typedef enum {
    TW_PDF_LINE_OFF = 0,
    TW_PDF_LINE_ASC,
    TW_PDF_LINE_DESC,
} TwPdfLineNum;

typedef struct {
    int page_margins; /* Letter-style margins like the app */
    TwPdfLineNum line_num_mode;
    int cols_margined; /* used when page_margins (50–65), same as F7 */
    int bg_idx;        /* 0..9 background scheme */
} TwPdfOpts;

/*
 * Write a multi-page PDF using Cairo. Each document page is rasterized with the
 * same bitmap fonts and layout rules as on screen (no typewriter transform, no
 * cursor). Returns 0 on success, -1 on failure.
 */
int tw_export_pdf(const TwDoc *doc, const char *pdf_path, const TwBitmapFont *font, const TwPdfOpts *opts);
