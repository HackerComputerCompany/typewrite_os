/**
 * Typewriter.c - UEFI Typewriter Application with Virgil/Helvetica Fonts
 * 
 * A native UEFI typewriter experience with:
 * - Bitmap font rendering (Virgil, Inter, + retro / typewriter faces)
 * - Typewriter-style input with visual feedback
 * - F8/F9 save/load per slot/page (TWSnPmm.TXT); five slots; F10 switch; F11/F12 pages;
 *   PgUp/PgDn; arrows move cursor; Ins line numbers; autoload TWS1Pxx then Typewriter.txt
 * - LCD-style clock HUD
 * - Multiple view modes
 * 
 * Built with gnu-efi
 */

#include <efi.h>
#include <efilib.h>
#include <eficon.h>
#include <efiprot.h>

#include <virgil.h>
#include <helvetica.h>
#include <special_elite.h>
#include <courier_prime.h>
#include <vt323.h>
#include <press_start_2p.h>
#include <ibm_plex_mono.h>
#include <share_tech_mono.h>

#define TYPEWRITER_DOC_FILENAME L"Typewriter.txt"
#define DOC_FILE_MAX_BYTES      (128 * 1024)
#define LCD_STATUS_ROW_H        34
#define LCD_STATUS_MARGIN_BOT   6
#define FILE_BANNER_HOLD_US     (4500000U)

static UINT32 StringPixelWidthChars(const CHAR16 *s);

typedef struct {
    const UINT32 *offsets;
    const UINT8 *widths;
    const UINT8 *advances;
    const UINT8 *bitmap_top;
    const UINT8 *bitmap;
    UINTN bitmap_size;
    UINT32 asc_min;
    UINT32 asc_max;
    UINT32 line_box;
    UINT32 max_top;
    UINT32 max_glyph_height;
} BitmapFont;

/* Order must match FONT_KIND for indices 0 .. FONT_SIMPLE-1 */
static const BitmapFont gBitmapFonts[] = {
    { virgil_offsets, virgil_widths, virgil_advances, virgil_bitmap_top, virgil_bitmap,
      sizeof(virgil_bitmap), VIRGIL_ASC_MIN, VIRGIL_ASC_MAX, VIRGIL_LINE_BOX, VIRGIL_MAX_TOP, VIRGIL_HEIGHT },
    { helvetica_offsets, helvetica_widths, helvetica_advances, helvetica_bitmap_top, helvetica_bitmap,
      sizeof(helvetica_bitmap), HELVETICA_ASC_MIN, HELVETICA_ASC_MAX, HELVETICA_LINE_BOX, HELVETICA_MAX_TOP, HELVETICA_HEIGHT },
    { special_elite_offsets, special_elite_widths, special_elite_advances, special_elite_bitmap_top, special_elite_bitmap,
      sizeof(special_elite_bitmap), SPECIAL_ELITE_ASC_MIN, SPECIAL_ELITE_ASC_MAX, SPECIAL_ELITE_LINE_BOX, SPECIAL_ELITE_MAX_TOP, SPECIAL_ELITE_HEIGHT },
    { courier_prime_offsets, courier_prime_widths, courier_prime_advances, courier_prime_bitmap_top, courier_prime_bitmap,
      sizeof(courier_prime_bitmap), COURIER_PRIME_ASC_MIN, COURIER_PRIME_ASC_MAX, COURIER_PRIME_LINE_BOX, COURIER_PRIME_MAX_TOP, COURIER_PRIME_HEIGHT },
    { vt323_offsets, vt323_widths, vt323_advances, vt323_bitmap_top, vt323_bitmap,
      sizeof(vt323_bitmap), VT323_ASC_MIN, VT323_ASC_MAX, VT323_LINE_BOX, VT323_MAX_TOP, VT323_HEIGHT },
    { press_start_2p_offsets, press_start_2p_widths, press_start_2p_advances, press_start_2p_bitmap_top, press_start_2p_bitmap,
      sizeof(press_start_2p_bitmap), PRESS_START_2P_ASC_MIN, PRESS_START_2P_ASC_MAX, PRESS_START_2P_LINE_BOX, PRESS_START_2P_MAX_TOP, PRESS_START_2P_HEIGHT },
    { ibm_plex_mono_offsets, ibm_plex_mono_widths, ibm_plex_mono_advances, ibm_plex_mono_bitmap_top, ibm_plex_mono_bitmap,
      sizeof(ibm_plex_mono_bitmap), IBM_PLEX_MONO_ASC_MIN, IBM_PLEX_MONO_ASC_MAX, IBM_PLEX_MONO_LINE_BOX, IBM_PLEX_MONO_MAX_TOP, IBM_PLEX_MONO_HEIGHT },
    { share_tech_mono_offsets, share_tech_mono_widths, share_tech_mono_advances, share_tech_mono_bitmap_top, share_tech_mono_bitmap,
      sizeof(share_tech_mono_bitmap), SHARE_TECH_MONO_ASC_MIN, SHARE_TECH_MONO_ASC_MAX, SHARE_TECH_MONO_LINE_BOX, SHARE_TECH_MONO_MAX_TOP, SHARE_TECH_MONO_HEIGHT },
};

// Simple 5x7 built-in font (ASCII 32-126)
#define SIMPLE_FONT_HEIGHT 8
#define SIMPLE_FONT_WIDTH 5
static const UINT8 simple_font_data[] = {
    // Space '!' etc - minimal bitmap for testing
    0x00, 0x00, 0x00, 0x00, 0x00,  // space
    0x00, 0x00, 0x2f, 0x00, 0x00,  // !
    0x00, 0x07, 0x00, 0x07, 0x00,  // "
    0x14, 0x7f, 0x14, 0x7f, 0x14,  // #
    0x24, 0x2a, 0x7f, 0x2a, 0x12,  // $
    0x62, 0x64, 0x08, 0x13, 0x23,  // %
    0x36, 0x49, 0x55, 0x22, 0x50,  // &
    0x00, 0x07, 0x00, 0x00, 0x00,  // '
    0x00, 0x1c, 0x22, 0x41, 0x00,  // (
    0x00, 0x41, 0x22, 0x1c, 0x00,  // )
    0x14, 0x08, 0x3e, 0x08, 0x14,  // *
    0x08, 0x08, 0x3e, 0x08, 0x08,  // +
    0x00, 0x50, 0x30, 0x00, 0x00,  // ,
    0x08, 0x08, 0x08, 0x08, 0x08,  // -
    0x00, 0x60, 0x60, 0x00, 0x00,  // .
    0x20, 0x10, 0x08, 0x04, 0x02,  // /
    0x3e, 0x51, 0x49, 0x45, 0x3e,  // 0
    0x00, 0x42, 0x7f, 0x40, 0x00,  // 1
    0x42, 0x61, 0x51, 0x49, 0x46,  // 2
    0x22, 0x41, 0x49, 0x49, 0x36,  // 3
    0x18, 0x14, 0x12, 0x7f, 0x10,  // 4
    0x27, 0x45, 0x45, 0x45, 0x39,  // 5
    0x3e, 0x49, 0x49, 0x49, 0x32,  // 6
    0x01, 0x01, 0x79, 0x07, 0x03,  // 7
    0x36, 0x49, 0x49, 0x49, 0x36,  // 8
    0x26, 0x49, 0x49, 0x49, 0x3e,  // 9
    0x00, 0x36, 0x36, 0x00, 0x00,  // :
    0x00, 0x56, 0x36, 0x00, 0x00,  // ;
    0x08, 0x14, 0x22, 0x41, 0x00,  // <
    0x14, 0x14, 0x14, 0x14, 0x14,  // =
    0x00, 0x41, 0x22, 0x14, 0x08,  // >
    0x02, 0x01, 0x51, 0x09, 0x06,  // ?
    0x3e, 0x41, 0x5d, 0x49, 0x46,  // @
    0x7e, 0x11, 0x11, 0x11, 0x7e,  // A
    0x7f, 0x49, 0x49, 0x49, 0x36,  // B
    0x3e, 0x41, 0x41, 0x41, 0x22,  // C
    0x7f, 0x41, 0x41, 0x41, 0x3e,  // D
    0x7f, 0x49, 0x49, 0x49, 0x41,  // E
    0x7f, 0x09, 0x09, 0x09, 0x01,  // F
    0x3e, 0x41, 0x49, 0x49, 0x3a,  // G
    0x7f, 0x08, 0x08, 0x08, 0x7f,  // H
    0x00, 0x41, 0x7f, 0x41, 0x00,  // I
    0x20, 0x40, 0x41, 0x3f, 0x01,  // J
    0x7f, 0x08, 0x14, 0x22, 0x41,  // K
    0x7f, 0x40, 0x40, 0x40, 0x40,  // L
    0x7f, 0x02, 0x0c, 0x02, 0x7f,  // M
    0x7f, 0x04, 0x08, 0x10, 0x7f,  // N
    0x3e, 0x41, 0x41, 0x41, 0x3e,  // O
    0x7f, 0x09, 0x09, 0x09, 0x06,  // P
    0x3e, 0x41, 0x51, 0x21, 0x5e,  // Q
    0x7f, 0x09, 0x19, 0x29, 0x46,  // R
    0x26, 0x49, 0x49, 0x49, 0x32,  // S
    0x01, 0x01, 0x7f, 0x01, 0x01,  // T
    0x3f, 0x40, 0x40, 0x40, 0x3f,  // U
    0x1f, 0x20, 0x40, 0x20, 0x1f,  // V
    0x3f, 0x40, 0x30, 0x40, 0x3f,  // W
    0x63, 0x14, 0x08, 0x14, 0x63,  // X
    0x07, 0x08, 0x70, 0x08, 0x07,  // Y
    0x61, 0x51, 0x49, 0x45, 0x43,  // Z
    0x00, 0x7f, 0x41, 0x41, 0x00,  // [
    0x02, 0x04, 0x08, 0x10, 0x20,  // backslash
    0x00, 0x41, 0x41, 0x7f, 0x00,  // ]
    0x04, 0x02, 0x01, 0x02, 0x04,  // ^
    0x40, 0x40, 0x40, 0x40, 0x40,  // _
    0x00, 0x01, 0x02, 0x04, 0x00,  // `
    0x20, 0x54, 0x54, 0x54, 0x78,  // a
    0x7f, 0x48, 0x48, 0x48, 0x30,  // b
    0x38, 0x44, 0x44, 0x44, 0x20,  // c
    0x30, 0x48, 0x48, 0x48, 0x7f,  // d
    0x44, 0x7c, 0x54, 0x54, 0x28,  // e
    0x08, 0x7e, 0x09, 0x01, 0x02,  // f
    0x0c, 0x52, 0x52, 0x52, 0x3e,  // g
    0x7f, 0x08, 0x08, 0x08, 0x70,  // h
    0x00, 0x00, 0x7f, 0x00, 0x00,  // i
    0x20, 0x00, 0x7f, 0x00, 0x00,  // j
    0x7f, 0x10, 0x28, 0x44, 0x00,  // k
    0x00, 0x00, 0x7f, 0x40, 0x00,  // l
    0x7c, 0x04, 0x18, 0x04, 0x78,  // m
    0x7c, 0x08, 0x04, 0x04, 0x78,  // n
    0x38, 0x44, 0x44, 0x44, 0x38,  // o
    0x7c, 0x14, 0x14, 0x14, 0x08,  // p
    0x08, 0x14, 0x14, 0x14, 0x7c,  // q
    0x7c, 0x08, 0x04, 0x04, 0x08,  // r
    0x48, 0x54, 0x54, 0x54, 0x20,  // s
    0x04, 0x3f, 0x44, 0x40, 0x20,  // t
    0x3c, 0x40, 0x40, 0x40, 0x7c,  // u
    0x1c, 0x20, 0x40, 0x20, 0x1c,  // v
    0x3c, 0x40, 0x30, 0x40, 0x3c,  // w
    0x44, 0x28, 0x10, 0x28, 0x44,  // x
    0x0c, 0x50, 0x50, 0x50, 0x3c,  // y
    0x44, 0x64, 0x54, 0x4c, 0x44,  // z
    0x08, 0x36, 0x41, 0x41, 0x00,  // {
    0x00, 0x00, 0x7f, 0x00, 0x00,  // |
    0x00, 0x41, 0x41, 0x36, 0x08,  // }
    0x08, 0x04, 0x08, 0x10, 0x08,  // ~
};

static const UINT32 simple_font_offsets[] = {
    0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75,
    80, 85, 90, 95, 100, 105, 110, 115, 120, 125, 130, 135, 140, 145,
    150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215,
    220, 225, 230, 235, 240, 245, 250, 255, 260, 265, 270, 275, 280, 285,
    290, 295, 300, 305, 310, 315, 320, 325, 330, 335, 340, 345, 350, 355,
    360, 365, 370, 375, 380, 385, 390, 395, 400, 405, 410, 415, 420, 425,
    430, 435, 440, 445, 450, 455, 460, 465, 470, 475, 480, 485, 490, 495,
    500, 505, 510, 515, 520, 525, 530, 535, 540, 545, 550, 555, 560, 565,
    570, 575, 580, 585, 590, 595
};

/*
 * One in-memory page: US Letter–style grid (~6.5" × ~9" printable at ~10 cpi × ~60 lines).
 * Up to MAX_RAM_PAGES per session; disk files TWS{slot}P{nn}.TXT (slot 1–5, page 01–99).
 */
#define PAGE_COLS 50
#define PAGE_ROWS 60
#define PAGE_CELLS (PAGE_COLS * PAGE_ROWS)
#define SAVE_SLOT_COUNT 5
#define MAX_RAM_PAGES 32

/* Logical ~96 DPI for 1" margins when GOP gives no physical size. */
#define FONT_ASSUMED_DPI 96
#define PAGE_MARGIN_INCH_NUM 1
#define PAGE_MARGIN_INCH_DEN 1

#define OFF_PAGE_COLOR RGB(6, 6, 8)

#define LEFT_MARGIN 20
#define RIGHT_MARGIN 24
#define TOP_MARGIN 30

/* UEFI firmware watchdog (often ~5 min) resets the platform if not fed; timeout 0 disables. */
static VOID KickFirmwareWatchdog(VOID) {
    if (BS != NULL && BS->SetWatchdogTimer != NULL)
        (void)uefi_call_wrapper(BS->SetWatchdogTimer, 4, (UINTN)0, (UINT64)0, (UINTN)0, NULL);
}

typedef struct {
    UINT32 Width;
    UINT32 Height;
    UINT32 Pitch;
    UINT8  *PixelData;
    UINT32 RedMask;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
} FRAMEBUFFER;

typedef struct {
    CHAR16 Grid[PAGE_ROWS][PAGE_COLS + 1];
    UINT32 CursorCol;
    UINT32 CursorRow;
    UINT32 ScrollYPx;
    BOOLEAN Modified;
} DOCUMENT;

typedef struct {
    CHAR16 Grid[PAGE_ROWS][PAGE_COLS + 1];
    UINT32 CursorCol;
    UINT32 CursorRow;
    UINT32 ScrollYPx;
} PAGE_SNAPSHOT;

static EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
static EFI_HANDLE BootImageHandle;
static CHAR16 FileOpBanner[96];
static UINT32 FileOpBannerRemainUs = 0;
static BOOLEAN HudNeedPaint = FALSE;

/* Session timer (µs since editor loop started); HUD shows HH:MM on 7-seg LCD. */
static UINT64 SessionElapsedUs = 0;
static UINT32 LastHudSessionMinute = (UINT32)-1;

typedef enum {
    FONT_VIRGIL = 0,
    FONT_HELVETICA,
    FONT_SPECIAL_ELITE,
    FONT_COURIER_PRIME,
    FONT_VT323,
    FONT_PRESS_START_2P,
    FONT_IBM_PLEX_MONO,
    FONT_SHARE_TECH_MONO,
    FONT_SIMPLE,
    FONT_NUM
} FONT_KIND;

static FONT_KIND CurrentFontKind = FONT_COURIER_PRIME;
static BOOLEAN ShowHelp = FALSE;
static BOOLEAN KeyDebugMode = FALSE;

#define KEY_DBG_LINES 8
#define KEY_DBG_COLS 72

#define RGB(r, g, b) ((r) | ((g) << 8) | ((b) << 16))

static const UINT32 COLORS[] = {
    RGB(30, 30, 30),     /* Dark */
    RGB(240, 240, 230),  /* Cream/Paper */
    RGB(0, 48, 96),      /* Blue */
    RGB(96, 48, 0),      /* Brown */
    RGB(0, 96, 48),      /* Green */
    RGB(96, 0, 48),      /* Maroon */
    RGB(48, 0, 96),      /* Purple */
    RGB(0, 96, 96),      /* Teal */
    RGB(96, 96, 0),      /* Olive */
    RGB(0, 48, 48),      /* Navy */
};

static UINT32 CurrentBgColor = 1;
/* Effective scale = FontScaleTwice / 2  (2 = 1.0×, 3 = 1.5×, …, 12 = 6.0×). */
static UINT32 FontScaleTwice = 2;

#define FONT_SZ_MUL(v) ((UINT32)(((UINTN)(v) * (UINTN)FontScaleTwice) / 2))
#define FONT_PIXEL_KERN() (FONT_SZ_MUL(1) ? FONT_SZ_MUL(1) : 1)

typedef enum {
    CURSOR_BAR = 0,
    CURSOR_BAR_BLINK,
    CURSOR_BLOCK,
    CURSOR_BLOCK_BLINK,
    CURSOR_HIDDEN,
    CURSOR_MODE_NUM
} CURSOR_MODE;

static UINT32 CursorMode = CURSOR_BLOCK_BLINK;
static BOOLEAN CursorBlinkPhase = TRUE;
static UINT64 CursorBlinkAccumUs = 0;

#define CURSOR_BLINK_PERIOD_US 500000

static BOOLEAN CursorWantsBlinkTimer(VOID) {
    return CursorMode == CURSOR_BAR_BLINK || CursorMode == CURSOR_BLOCK_BLINK;
}

static BOOLEAN CursorShouldDrawThisFrame(VOID) {
    if (CursorMode == CURSOR_HIDDEN)
        return FALSE;
    if (CursorWantsBlinkTimer())
        return CursorBlinkPhase;
    return TRUE;
}

static DOCUMENT Doc;
static BOOLEAN Running = TRUE;
static CHAR16 KeyDbgLines[KEY_DBG_LINES][KEY_DBG_COLS];

/* Repaint: full screen vs horizontal strips for changed document lines only. */
static BOOLEAN RepaintFull = TRUE;
static UINT32 DirtyLineTop = 1;
static UINT32 DirtyLineBottom = 0; /* invalid until MarkDirtyRange (Top <= Bottom). */
static UINT32 LastPaintedLineCount = 0;
/* Right edge (screen x) last cleared+painted per row — avoids over-clearing. */
static UINT32 LastLinePaintedRight[PAGE_ROWS];

static PAGE_SNAPSHOT RamPages[MAX_RAM_PAGES];
static UINT32 RamPageCount = 1;
static UINT32 ActiveRamPageIndex = 0;
static UINT32 SaveSlotIndex = 0;
static BOOLEAN ShowLineNumbers = FALSE;
static UINT32 G_lineNumGutterPx = 0;

/* Page layout (recomputed from GOP + font; document uses fixed column pitch). */
static UINT32 G_docViewportBot = 600;
static UINT32 G_pageMarginPx = 96;
static UINT32 G_pageColPitch = 8;
static UINT32 G_lineStep = 16;
static UINT32 G_pagePaperX = 0;
static UINT32 G_pagePaperY = 0;
static UINT32 G_pageTotalW = 0;
static UINT32 G_pageTotalH = 0;
static UINT32 G_pageContentX0 = 0;
static UINT32 G_pageContentY0 = 0;
/* Blinking cursor: repaint only the cursor band, not the whole line. */
static BOOLEAN CursorBlinkRedraw = FALSE;

#define CHAR_GAP 2
/* Word space advance multiplier (advance for ' ' is this × normal font advance). */
#define SPACE_ADVANCE_MULT 3

#define SCELL(_fb, _x, _y, _c) DrawRect((_fb), (_x), (_y), FONT_PIXEL_KERN(), FONT_PIXEL_KERN(), (_c))

static UINT32 GlyphAdvance(CHAR16 ch);
static UINT32 LineAdvance(VOID);

static VOID MarkFullRepaint(VOID) {
    RepaintFull = TRUE;
    SetMem(LastLinePaintedRight, sizeof(LastLinePaintedRight), 0);
}

static VOID MarkDirtyRange(UINT32 top, UINT32 bot) {
    if (RepaintFull)
        return;
    if (top > bot) {
        UINT32 t = top;
        top = bot;
        bot = t;
    }
    if (DirtyLineTop > DirtyLineBottom) {
        DirtyLineTop = top;
        DirtyLineBottom = bot;
    } else {
        if (top < DirtyLineTop)
            DirtyLineTop = top;
        if (bot > DirtyLineBottom)
            DirtyLineBottom = bot;
    }
}

static UINT32 GridRowLastUsedCol(const CHAR16 *row) {
    UINT32 c;
    UINT32 last = 0;

    for (c = 0; c < PAGE_COLS; c++) {
        if (row[c] != L' ' && row[c] != 0)
            last = c + 1;
    }
    return last;
}

static UINT32 ComputePageColPitch(VOID) {
    UINT32 m = GlyphAdvance(L'M');
    UINT32 w;
    CHAR16 ch;

    w = GlyphAdvance(L'W');
    if (w > m)
        m = w;
    w = GlyphAdvance(L'm');
    if (w > m)
        m = w;
    for (ch = L'0'; ch <= L'9'; ch++) {
        w = GlyphAdvance(ch);
        if (w > m)
            m = w;
    }
    w = GlyphAdvance(L'n');
    if (w > m)
        m = w;
    if (m < FONT_PIXEL_KERN())
        m = FONT_PIXEL_KERN();
    return m;
}

static VOID UpdatePageLayoutFromFb(const FRAMEBUFFER *fb) {
    UINT32 inch;
    UINT32 contentW;
    UINT32 contentH;

    G_docViewportBot = (fb->Height > LCD_STATUS_ROW_H + LCD_STATUS_MARGIN_BOT + 4)
                           ? fb->Height - LCD_STATUS_ROW_H - LCD_STATUS_MARGIN_BOT
                           : fb->Height;

    inch = (FONT_ASSUMED_DPI * PAGE_MARGIN_INCH_NUM) / PAGE_MARGIN_INCH_DEN;
    if (inch > fb->Width / 6)
        inch = fb->Width / 6;
    if (inch > G_docViewportBot / 6)
        inch = G_docViewportBot / 6;
    if (inch < 16)
        inch = 16;
    G_pageMarginPx = inch;

    G_pageColPitch = ComputePageColPitch();
    G_lineStep = LineAdvance();

    G_lineNumGutterPx = 0;
    if (ShowLineNumbers) {
        UINT32 gw = GlyphAdvance(L'0');

        if (gw < FONT_PIXEL_KERN())
            gw = FONT_PIXEL_KERN();
        G_lineNumGutterPx = gw * 3 + FONT_SZ_MUL(10);
        if (G_lineNumGutterPx < FONT_SZ_MUL(24))
            G_lineNumGutterPx = FONT_SZ_MUL(24);
    }

    contentW = PAGE_COLS * G_pageColPitch;
    contentH = PAGE_ROWS * G_lineStep;
    G_pageTotalW = 2 * G_pageMarginPx + contentW + G_lineNumGutterPx;
    G_pageTotalH = 2 * G_pageMarginPx + contentH;

    G_pagePaperX = (fb->Width > G_pageTotalW) ? (fb->Width - G_pageTotalW) / 2 : 0;
    G_pagePaperY = 0;

    G_pageContentX0 = G_pagePaperX + G_pageMarginPx + G_lineNumGutterPx;
    G_pageContentY0 = G_pagePaperY + G_pageMarginPx;
}

static INT32 RowTextScreenY(UINT32 rowIdx) {
    return (INT32)G_pageContentY0 + (INT32)(rowIdx * G_lineStep) - (INT32)Doc.ScrollYPx;
}

static UINT32 ContentXForCol(UINT32 col) {
    return G_pageContentX0 + col * G_pageColPitch;
}

static UINT32 GridRowPaintRightScreenX(const CHAR16 *row) {
    (void)row;
    return G_pageContentX0 + PAGE_COLS * G_pageColPitch;
}

static VOID EnsureCursorVisibleScroll(const FRAMEBUFFER *fb) {
    UINT32 cy = G_pageContentY0 + Doc.CursorRow * G_lineStep;
    UINT32 viewBot = G_docViewportBot;
    UINT32 maxScroll;

    if (G_pageTotalH <= viewBot) {
        Doc.ScrollYPx = 0;
        return;
    }
    maxScroll = G_pageTotalH - viewBot;
    if (cy < Doc.ScrollYPx)
        Doc.ScrollYPx = cy;
    if (cy + G_lineStep > Doc.ScrollYPx + viewBot)
        Doc.ScrollYPx = cy + G_lineStep - viewBot;
    if (Doc.ScrollYPx > maxScroll)
        Doc.ScrollYPx = maxScroll;
    (void)fb;
}

static VOID InitDocumentGridSpaces(VOID) {
    UINT32 r;
    UINT32 c;

    for (r = 0; r < PAGE_ROWS; r++) {
        for (c = 0; c < PAGE_COLS; c++)
            Doc.Grid[r][c] = L' ';
        Doc.Grid[r][PAGE_COLS] = 0;
    }
}

/* Pixel/word wrap removed; font changes only need a full layout pass. */
static VOID ReflowAllLines(VOID) {
}

/* GOP front buffer (hardware). When non-NULL, fb->PixelData points at a Pool back buffer. */
static UINT8 *FbFront = NULL;
static UINTN FbCopyBytes = 0;
/*
 * Apple UEFI: raw framebuffer writes often hang or show nothing — firmware expects
 * EfiBltBufferToVideo. QEMU/OVMF and typical PC firmware: direct FB is fine (Blt there
 * regressed to black screen in tests — GRAPHICS_DEBUG.md).
 */
static BOOLEAN GopUseBlt = FALSE;

static BOOLEAN IsAppleSystem(VOID) {
    CHAR16 *v = (ST != NULL) ? ST->FirmwareVendor : NULL;

    if (v == NULL)
        return FALSE;
    /* fw vendor is usually "Apple Inc." — gnu-efi has no StrStr */
    for (; *v; v++) {
        if (v[0] == L'A' && v[1] == L'p' && v[2] == L'p' && v[3] == L'l' && v[4] == L'e')
            return TRUE;
    }
    return FALSE;
}

/*
 * Prefer a modest listed mode on Macs to avoid huge native + broken linear FB combos.
 */
static UINT32 ApplePickGopMode(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
    UINT32 maxm = gop->Mode->MaxMode;
    UINT32 fallback = gop->Mode->Mode;
    UINT32 p, i;

    static const UINT32 pref[][2] = {
        { 1024, 768 },
        { 1280, 800 },
        { 1440, 900 },
        { 1280, 720 },
        { 1680, 1050 },
        { 800, 600 },
    };

    for (p = 0; p < sizeof(pref) / sizeof(pref[0]); p++) {
        UINT32 want_w = pref[p][0];
        UINT32 want_h = pref[p][1];

        for (i = 0; i < maxm; i++) {
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
            UINTN sz = 0;
            EFI_STATUS st = uefi_call_wrapper(gop->QueryMode, 4, gop, i, &sz, &info);

            if (EFI_ERROR(st) || info == NULL)
                continue;
            if (info->HorizontalResolution == want_w && info->VerticalResolution == want_h) {
                UINT32 chosen = i;

                FreePool(info);
                return chosen;
            }
            FreePool(info);
        }
    }
    return fallback;
}

static VOID FlipFramebuffer(FRAMEBUFFER *fb) {
    if (GopUseBlt && Gop && Gop->Blt) {
        UINTN delta = (UINTN)fb->Pitch;

        if (delta == 0)
            delta = (UINTN)fb->Width * 4;
        uefi_call_wrapper(Gop->Blt, 10, Gop,
                          (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)(VOID *)fb->PixelData,
                          EfiBltBufferToVideo,
                          0, 0,
                          0, 0,
                          fb->Width, fb->Height,
                          delta);
        /* Some Apple firmware watchdogs are tight during long GOP work. */
        KickFirmwareWatchdog();
        return;
    }
    if (FbCopyBytes == 0 || FbFront == NULL || fb->PixelData == FbFront)
        return;
    CopyMem(FbFront, fb->PixelData, FbCopyBytes);
}

EFI_STATUS DrawPixel(FRAMEBUFFER *fb, UINT32 x, UINT32 y, UINT32 color) {
    if (x >= fb->Width || y >= fb->Height) return EFI_SUCCESS;
    UINT8 *pixel = fb->PixelData + y * fb->Pitch + x * 4;
    
    /* Packed as RRGGBB in UINT32 (see RGB macro) */
    UINT8 r = (color >> 0) & 0xFF;
    UINT8 g = (color >> 8) & 0xFF;
    UINT8 b = (color >> 16) & 0xFF;
    
    if (fb->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        pixel[0] = r;
        pixel[1] = g;
        pixel[2] = b;
    } else {
        /* BGR and default path (PixelBlueGreenRedReserved8BitPerColor, BitMask, etc.) */
        pixel[0] = b;
        pixel[1] = g;
        pixel[2] = r;
    }
    pixel[3] = 0;
    return EFI_SUCCESS;
}

/*
 * Clamp before drawing. Without this, (y + h) or (x + w) UINT32 wrap can make
 * py < y + h true across a huge range and lock the machine for minutes — seen
 * on picky firmware when derived w/h are bogus. Same for underflowed widths.
 */
EFI_STATUS DrawRect(FRAMEBUFFER *fb, UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT32 color) {
    if (fb == NULL || fb->Width == 0 || fb->Height == 0)
        return EFI_SUCCESS;
    if (w == 0 || h == 0)
        return EFI_SUCCESS;
    if (x >= fb->Width || y >= fb->Height)
        return EFI_SUCCESS;
    if (w > fb->Width - x)
        w = fb->Width - x;
    if (h > fb->Height - y)
        h = fb->Height - y;
    if (w == 0 || h == 0)
        return EFI_SUCCESS;

    {
        UINT32 yEnd = y + h;
        UINT32 xEnd = x + w;

        for (UINT32 py = y; py < yEnd; py++) {
            for (UINT32 px = x; px < xEnd; px++)
                DrawPixel(fb, px, py, color);
        }
    }
    return EFI_SUCCESS;
}

EFI_STATUS DrawCharSimple(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 ch, UINT32 fgColor, UINT32 bgColor) {
    if (ch < 32 || ch > 126) return EFI_SUCCESS;
    
    UINT32 charIndex = ch - 32;
    UINT32 offset = simple_font_offsets[charIndex];
    
    /* simple_font_data: 5 bytes per glyph, one byte per COLUMN, bit 0 = top row */
    for (UINT32 row = 0; row < SIMPLE_FONT_HEIGHT; row++) {
        UINT32 dy = y + row * FONT_PIXEL_KERN();
        if (dy >= fb->Height)
            break;
        for (UINT32 col = 0; col < SIMPLE_FONT_WIDTH; col++) {
            UINT32 dx = x + col * FONT_PIXEL_KERN();
            if (dx >= fb->Width)
                continue;
            UINT8 colBits = simple_font_data[offset + col];
            if (colBits & (1u << row)) {
                SCELL(fb, dx, dy, fgColor);
            } else {
                SCELL(fb, dx, dy, bgColor);
            }
        }
    }
    return EFI_SUCCESS;
}

EFI_STATUS DrawCharBitmapFont(FRAMEBUFFER *fb, UINT32 x, UINT32 baselineY, CHAR16 ch, UINT32 fgColor,
                              UINT32 bgColor, const BitmapFont *F) {
    if (ch < F->asc_min || ch > F->asc_max)
        return EFI_SUCCESS;

    UINT32 charIndex = ch - F->asc_min;
    UINT32 offset = F->offsets[charIndex];
    UINT32 nextOffset = (charIndex < 94) ? F->offsets[charIndex + 1] : (UINT32)F->bitmap_size;
    UINT32 glyphBytes = nextOffset - offset;

    if (glyphBytes == 0 || offset + glyphBytes > F->bitmap_size)
        return EFI_SUCCESS;

    UINT32 gw = F->widths[charIndex];
    if (gw < 1)
        gw = 1;
    UINT32 rowB = (gw + 7) / 8;
    if (rowB < 1)
        rowB = 1;
    UINT32 gh = glyphBytes / rowB;
    if (gh < 1)
        gh = 1;
    if (gh > F->max_glyph_height)
        gh = F->max_glyph_height;
    UINT32 bmpTop = F->bitmap_top[charIndex];

    for (UINT32 py = 0; py < gh; py++) {
        INT32 dy = (INT32)baselineY - (INT32)FONT_SZ_MUL(bmpTop) + (INT32)FONT_SZ_MUL(py);
        if (dy < 0 || (UINT32)dy >= fb->Height)
            continue;
        UINT32 rowOff = offset + py * rowB;
        for (UINT32 byteIdx = 0; byteIdx < rowB && rowOff + byteIdx < F->bitmap_size; byteIdx++) {
            UINT8 byte = F->bitmap[rowOff + byteIdx];
            for (UINT32 bit = 0; bit < 8; bit++) {
                UINT32 px = byteIdx * 8 + bit;
                if (px >= gw)
                    break;
                UINT32 dx = x + px * FONT_PIXEL_KERN();
                if (dx >= fb->Width)
                    continue;
                if (byte & (1u << bit)) {
                    SCELL(fb, dx, (UINT32)dy, fgColor);
                } else {
                    SCELL(fb, dx, (UINT32)dy, bgColor);
                }
            }
        }
    }
    return EFI_SUCCESS;
}

EFI_STATUS DrawChar(FRAMEBUFFER *fb, UINT32 x, UINT32 yOrigin, CHAR16 ch, UINT32 fgColor, UINT32 bgColor) {
    if (CurrentFontKind == FONT_SIMPLE)
        return DrawCharSimple(fb, x, yOrigin, ch, fgColor, bgColor);
    if (CurrentFontKind < FONT_SIMPLE)
        return DrawCharBitmapFont(fb, x, yOrigin, ch, fgColor, bgColor, &gBitmapFonts[CurrentFontKind]);
    return EFI_SUCCESS;
}

static UINT32 GlyphAdvanceBitmap(CHAR16 ch, const BitmapFont *F) {
    UINT32 base;
    if (ch < F->asc_min || ch > F->asc_max)
        base = CHAR_GAP;
    else {
        UINT32 idx = ch - F->asc_min;
        UINT32 w = F->widths[idx];
        if (w < 1)
            w = 1;
        if (F->advances[idx] > 0)
            base = (UINT32)F->advances[idx];
        else
            base = w + CHAR_GAP;
    }
    if (ch == L' ')
        base *= SPACE_ADVANCE_MULT;
    return FONT_SZ_MUL(base);
}

static UINT32 GlyphAdvance(CHAR16 ch) {
    if (CurrentFontKind == FONT_SIMPLE) {
        UINT32 base;
        if (ch >= 32 && ch <= 126)
            base = SIMPLE_FONT_WIDTH + 1;
        else
            base = CHAR_GAP;
        if (ch == L' ')
            base *= SPACE_ADVANCE_MULT;
        return FONT_SZ_MUL(base);
    }
    if (CurrentFontKind < FONT_SIMPLE)
        return GlyphAdvanceBitmap(ch, &gBitmapFonts[CurrentFontKind]);
    return FONT_SZ_MUL(CHAR_GAP);
}

static UINT32 ActiveFontLineHeight(VOID) {
    if (CurrentFontKind == FONT_SIMPLE)
        return SIMPLE_FONT_HEIGHT;
    if (CurrentFontKind < FONT_SIMPLE)
        return gBitmapFonts[CurrentFontKind].line_box;
    return SIMPLE_FONT_HEIGHT;
}

static UINT32 GlyphBitmapHeightBitmap(CHAR16 ch, const BitmapFont *F) {
    if (ch < F->asc_min || ch > F->asc_max)
        return F->line_box;
    UINT32 charIndex = ch - F->asc_min;
    UINT32 offset = F->offsets[charIndex];
    UINT32 nextOffset = (charIndex < 94) ? F->offsets[charIndex + 1] : (UINT32)F->bitmap_size;
    UINT32 glyphBytes = nextOffset - offset;
    if (glyphBytes == 0 || offset + glyphBytes > F->bitmap_size)
        return F->line_box;
    UINT32 gw = F->widths[charIndex];
    if (gw < 1)
        gw = 1;
    UINT32 rowB = (gw + 7) / 8;
    if (rowB < 1)
        rowB = 1;
    UINT32 gh = glyphBytes / rowB;
    if (gh < 1)
        gh = 1;
    if (gh > F->max_glyph_height)
        gh = F->max_glyph_height;
    return gh;
}

static UINT32 GlyphBitmapHeight(CHAR16 ch) {
    if (CurrentFontKind == FONT_SIMPLE) {
        if (ch >= 32 && ch <= 126)
            return SIMPLE_FONT_HEIGHT;
        return ActiveFontLineHeight();
    }
    if (CurrentFontKind < FONT_SIMPLE)
        return GlyphBitmapHeightBitmap(ch, &gBitmapFonts[CurrentFontKind]);
    return ActiveFontLineHeight();
}

static UINT32 LineAdvance(VOID) {
    UINT32 box = FONT_SZ_MUL(ActiveFontLineHeight());
    UINT32 leading = box / 4 + FONT_SZ_MUL(4);

    return box + leading;
}

static UINT32 DocCursorPixelX(VOID) {
    return ContentXForCol(Doc.CursorCol);
}

EFI_STATUS DrawString(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 *str, UINT32 fgColor, UINT32 bgColor) {
    if (!str) return EFI_SUCCESS;
    UINT32 posX = x;
    UINT32 lineBox = FONT_SZ_MUL(ActiveFontLineHeight());
    while (*str) {
        CHAR16 c = *str;
        if (CurrentFontKind == FONT_SIMPLE) {
            UINT32 gh = GlyphBitmapHeight(c);
            UINT32 yDraw = y + lineBox - FONT_SZ_MUL(gh);
            DrawChar(fb, posX, yDraw, c, fgColor, bgColor);
        } else {
            UINT32 baselineY = y + FONT_SZ_MUL(gBitmapFonts[CurrentFontKind].max_top);
            DrawChar(fb, posX, baselineY, c, fgColor, bgColor);
        }
        posX += GlyphAdvance(c);
        str++;
    }
    return EFI_SUCCESS;
}

static VOID DrawCharOnDocLine(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 c, UINT32 fgColor, UINT32 bgColor) {
    UINT32 lineBox = FONT_SZ_MUL(ActiveFontLineHeight());

    if (CurrentFontKind == FONT_SIMPLE) {
        UINT32 gh = GlyphBitmapHeight(c);
        UINT32 yDraw = y + lineBox - FONT_SZ_MUL(gh);

        DrawChar(fb, x, yDraw, c, fgColor, bgColor);
    } else {
        UINT32 baselineY = y + FONT_SZ_MUL(gBitmapFonts[CurrentFontKind].max_top);

        DrawChar(fb, x, baselineY, c, fgColor, bgColor);
    }
}

static UINT32 CursorBarThickness(VOID) {
    UINT32 t = FONT_PIXEL_KERN();

    return t < 1 ? 1 : t;
}

static UINT32 CursorGlyphWidthOnLine(const VOID *unused) {
    UINT32 gw = G_pageColPitch;

    (void)unused;
    if (gw < FONT_PIXEL_KERN())
        gw = FONT_PIXEL_KERN();
    return gw;
}

/* Right x of cursor bar/block on this row (0 if cursor not on row or help hides doc). */
static UINT32 LineCursorRightEdgePx(const CHAR16 *ln, UINT32 rowIdx) {
    UINT32 cx;
    UINT32 cw;

    if (Doc.CursorRow != rowIdx || ShowHelp)
        return 0;
    cx = DocCursorPixelX();
    if (CursorMode == CURSOR_BAR || CursorMode == CURSOR_BAR_BLINK)
        cw = CursorBarThickness();
    else
        cw = CursorGlyphWidthOnLine(NULL);
    return cx + cw;
}

static UINT32 LinePaintRightX(const FRAMEBUFFER *fb, const CHAR16 *ln, UINT32 rowIdx) {
    UINT32 r = GridRowPaintRightScreenX(ln);
    UINT32 cr = LineCursorRightEdgePx(ln, rowIdx);

    if (cr > r)
        r = cr;
    r += FONT_SZ_MUL(2);
    if (r > fb->Width)
        r = fb->Width;
    (void)ln;
    return r;
}

static UINT32 LineNumberInk(VOID) {
    if (CurrentBgColor == 1)
        return RGB(96, 110, 138);
    return RGB(140, 150, 175);
}

static VOID DrawGridRowIfVisible(FRAMEBUFFER *fb, UINT32 r, UINT32 fg, UINT32 bg) {
    INT32 sy = RowTextScreenY(r);
    UINT32 yu;
    UINT32 px;
    UINT32 i;
    CHAR16 *ln = Doc.Grid[r];
    UINT32 lineBodyH = FONT_SZ_MUL(ActiveFontLineHeight());

    if (sy < 0 || sy >= (INT32)G_docViewportBot || sy + (INT32)lineBodyH <= 0)
        return;
    yu = (UINT32)sy;
    if (ShowLineNumbers && G_lineNumGutterPx > 0) {
        CHAR16 lnbuf[12];
        UINT32 lnx = G_pageContentX0 - G_lineNumGutterPx + FONT_SZ_MUL(4);

        if (lnx + 8 * FONT_PIXEL_KERN() > G_pageContentX0)
            lnx = G_pagePaperX + FONT_SZ_MUL(2);
        UnicodeSPrint(lnbuf, sizeof(lnbuf), L"%u", r + 1);
        DrawString(fb, lnx, yu, lnbuf, LineNumberInk(), bg);
    }
    px = G_pageContentX0;
    for (i = 0; i < PAGE_COLS; i++) {
        CHAR16 ch = ln[i] ? ln[i] : L' ';

        DrawCharOnDocLine(fb, px, yu, ch, fg, bg);
        px += G_pageColPitch;
    }
}

static VOID DrawClippedPaperFill(FRAMEBUFFER *fb, UINT32 paperBg, UINT32 clipBot) {
    INT32 pTop = (INT32)G_pagePaperY - (INT32)Doc.ScrollYPx;
    INT32 pBot = pTop + (INT32)G_pageTotalH;
    INT32 vb = (INT32)clipBot;
    UINT32 y0;
    UINT32 y1;
    UINT32 x0;
    UINT32 x1;

    if (pBot <= 0 || pTop >= vb)
        return;
    y0 = (pTop > 0) ? (UINT32)pTop : 0;
    y1 = (pBot < vb) ? (UINT32)pBot : (UINT32)vb;
    if (y1 <= y0)
        return;
    x0 = G_pagePaperX;
    if (x0 >= fb->Width)
        return;
    x1 = G_pagePaperX + G_pageTotalW;
    if (x1 > fb->Width)
        x1 = fb->Width;
    if (x1 <= x0)
        return;
    DrawRect(fb, x0, y0, x1 - x0, y1 - y0, paperBg);
}

static VOID RepaintDocLineCursorBand(FRAMEBUFFER *fb, UINT32 line, UINT32 bgColor, UINT32 fgColor) {
    CHAR16 *ln = Doc.Grid[line];
    INT32 sy = RowTextScreenY(line);
    UINT32 lineBodyH = FONT_SZ_MUL(ActiveFontLineHeight());
    UINT32 cx = DocCursorPixelX();
    UINT32 cw;
    UINT32 yu;

    if (sy < 0 || sy >= (INT32)G_docViewportBot || sy + (INT32)lineBodyH <= 0)
        return;

    if (CursorMode == CURSOR_BAR || CursorMode == CURSOR_BAR_BLINK)
        cw = CursorBarThickness();
    else
        cw = CursorGlyphWidthOnLine(NULL);

    {
        UINT32 bandL = cx;
        UINT32 bandR = cx + cw;
        UINT32 drawL = bandL;
        UINT32 drawR = bandR;
        UINT32 firstCol = (UINT32)-1;
        UINT32 lastCol = 0;
        UINT32 px = G_pageContentX0;
        UINT32 i;
        CHAR16 ch;

        for (i = 0; i < PAGE_COLS; i++) {
            UINT32 gL = px;
            UINT32 gR = px + G_pageColPitch;

            if (gR > bandL && gL < bandR) {
                if (firstCol == (UINT32)-1)
                    firstCol = i;
                lastCol = i;
                if (gL < drawL)
                    drawL = gL;
                if (gR > drawR)
                    drawR = gR;
            }
            px += G_pageColPitch;
        }
        if (drawL >= fb->Width)
            return;
        if (drawR > fb->Width)
            drawR = fb->Width;
        yu = (UINT32)sy;
        DrawRect(fb, drawL, yu, drawR - drawL, lineBodyH, bgColor);
        if (firstCol != (UINT32)-1) {
            px = ContentXForCol(firstCol);
            for (i = firstCol; i <= lastCol && i < PAGE_COLS; i++) {
                ch = ln[i] ? ln[i] : L' ';
                DrawCharOnDocLine(fb, px, yu, ch, fgColor, bgColor);
                px += G_pageColPitch;
            }
        }
    }

    if (CursorShouldDrawThisFrame()) {
        yu = (UINT32)sy;
        DrawRect(fb, cx, yu, cw, lineBodyH, fgColor);
    }
}

static VOID DrawHelpOverlay(FRAMEBUFFER *fb) {
    UINT32 hf = RGB(28, 28, 32);
    UINT32 hb = RGB(230, 224, 210);
    UINT32 hd = RGB(72, 72, 88);
    static const CHAR16 *const helpLines[] = {
        L"Typewrite OS - Help",
        L"F1   Toggle this help",
        L"F2   Cycle font (9): Virgil, Inter, Special Elite,",
        L"     Courier Prime, VT323, Press Start 2P,",
        L"     IBM Plex Mono, Share Tech Mono, Simple",
        L"F3   Increase font scale (half steps, 1.0×–6.0×)",
        L"F6   Decrease font scale",
        L"F4   Cycle background color",
        L"F5   Cycle cursor (default: blinking block)",
        L"F7   Toggle key debug (scan/unicode log)",
        L"F8   Save current page as TWS{slot}P{page}.TXT (UTF-8, boot volume)",
        L"F9   Load that file for current slot/page",
        L"F10  Next save slot (1–5); writes all pages, then loads new slot",
        L"F11  Save page & go to next (or new) page; PgDn same",
        L"F12  Save & previous page; PgUp same",
        L"↑↓←→ Move cursor (type only at cursor; arrows do not edit cells)",
        L"Ins  Toggle line numbers (gutter, muted color)",
        L"Boot: TWS1P01.TXT, P02… if present; else Typewriter.txt",
        L"ESC  Close help; quit app when help is hidden",
    };
    UINT32 n = sizeof(helpLines) / sizeof(helpLines[0]);
    UINT32 step = LineAdvance();
    UINT32 margin = FONT_SZ_MUL(6);
    UINT32 inner = FONT_SZ_MUL(5);
    UINT32 maxTw = 0;
    UINT32 i;

    for (i = 0; i < n; i++) {
        UINT32 w = StringPixelWidthChars(helpLines[i]);

        if (w > maxTw)
            maxTw = w;
    }
    {
        UINT32 bw = maxTw + inner * 2;

        if (bw < 120)
            bw = 120;
        if (bw > fb->Width - margin * 2 && fb->Width > margin * 2 + 120)
            bw = fb->Width - margin * 2;
        {
            UINT32 bh = n * step + inner * 2;

            if (bh > fb->Height - margin * 2 && fb->Height > margin * 2 + step * 2)
                bh = fb->Height - margin * 2;
            {
                UINT32 bx = (fb->Width > bw) ? (fb->Width - bw) / 2 : margin;
                UINT32 by = (fb->Height > bh) ? (fb->Height - bh) / 2 : margin;
                UINT32 lx = bx + inner;
                UINT32 ly = by + inner;

                DrawRect(fb, bx - 2, by - 2, bw + 4, bh + 4, hd);
                DrawRect(fb, bx, by, bw, bh, hb);
                for (i = 0; i < n; i++) {
                    DrawString(fb, lx, ly, (CHAR16 *)helpLines[i], hf, hb);
                    ly += step;
                }
            }
        }
    }
}

EFI_STATUS ClearScreen(FRAMEBUFFER *fb, UINT32 bgColor) {
    /* Full-screen fill: long on large GOP modes; feed watchdog for picky firmware. */
    for (UINT32 y = 0; y < fb->Height; y++) {
        if ((y & 63) == 0)
            KickFirmwareWatchdog();
        for (UINT32 x = 0; x < fb->Width; x++)
            DrawPixel(fb, x, y, bgColor);
    }
    return EFI_SUCCESS;
}

static UINT32 StringPixelWidthChars(const CHAR16 *s) {
    UINT32 w = 0;

    if (s == NULL)
        return 0;
    for (; *s; s++)
        w += GlyphAdvance(*s);
    return w;
}

/* Splash (~4 s) or any key */
#define SPLASH_TIMEOUT_US 4000000

static VOID DrawSplashScreen(FRAMEBUFFER *fb) {
    UINT32 bg = RGB(20, 22, 28);
    UINT32 fg = RGB(236, 232, 218);
    UINT32 accent = RGB(196, 160, 90);
    UINT32 muted = RGB(120, 116, 108);
    UINT32 rowStep = LineAdvance();
    UINT32 ruleW, ruleX, ruleY;

    ClearScreen(fb, bg);
    ruleY = (fb->Height / 2 > rowStep * 3) ? fb->Height / 2 - rowStep * 3 : fb->Height / 4;
    ruleW = fb->Width / 3;
    if (ruleW < 120)
        ruleW = fb->Width / 2;
    ruleX = (fb->Width > ruleW) ? (fb->Width - ruleW) / 2 : 0;
    if (ruleY + 2 < fb->Height)
        DrawRect(fb, ruleX, ruleY, ruleW, 2, accent);

    {
        CHAR16 *t = L"Typewrite OS";
        UINT32 tw = StringPixelWidthChars(t);
        UINT32 x = (fb->Width > tw) ? (fb->Width - tw) / 2 : LEFT_MARGIN;
        UINT32 y = fb->Height / 2 - rowStep * 2;

        DrawString(fb, x, y, t, fg, bg);
    }
    {
        CHAR16 *t = L"UEFI typewriter";
        UINT32 tw = StringPixelWidthChars(t);
        UINT32 x = (fb->Width > tw) ? (fb->Width - tw) / 2 : LEFT_MARGIN;
        UINT32 y = fb->Height / 2 - rowStep / 2;

        DrawString(fb, x, y, t, muted, bg);
    }
    {
        CHAR16 *t = L"Press any key to start";
        UINT32 tw = StringPixelWidthChars(t);
        UINT32 x = (fb->Width > tw) ? (fb->Width - tw) / 2 : LEFT_MARGIN;
        UINT32 y = fb->Height / 2 + rowStep;

        DrawString(fb, x, y, t, accent, bg);
    }
}

/*
 * OVMF/QEMU: ConIn->ReadKeyStroke sometimes returns EFI_SUCCESS with ScanCode/UnicodeChar
 * both zero forever; draining with while (!EFI_ERROR(...)) never terminates.
 */
#define SPLASH_KEY_DRAIN_MAX 256

static VOID SplashDrainConIn(VOID) {
    EFI_INPUT_KEY key;
    UINTN i;
    EFI_STATUS st;

    for (i = 0; i < SPLASH_KEY_DRAIN_MAX; i++) {
        st = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
        if (EFI_ERROR(st))
            return;
        if (key.ScanCode == SCAN_NULL && key.UnicodeChar == 0)
            return;
    }
}

static VOID RunSplashScreen(FRAMEBUFFER *fb) {
    EFI_INPUT_KEY key;
    UINT64 elapsed;

    DrawSplashScreen(fb);
    FlipFramebuffer(fb);
    KickFirmwareWatchdog();

    SplashDrainConIn();

    elapsed = 0;
    while (elapsed < (UINT64)SPLASH_TIMEOUT_US) {
        EFI_STATUS ks = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);

        if (!EFI_ERROR(ks)) {
            if (key.ScanCode != SCAN_NULL || key.UnicodeChar != 0)
                break;
        }
        KickFirmwareWatchdog();
        uefi_call_wrapper(BS->Stall, 1, 40000);
        elapsed += 40000;
    }

    SplashDrainConIn();
}

VOID InitDocument(VOID) {
    ZeroMem(&Doc, sizeof(Doc));
    InitDocumentGridSpaces();
    Doc.CursorCol = 0;
    Doc.CursorRow = 0;
    Doc.ScrollYPx = 0;
    Doc.Modified = TRUE;
    ZeroMem(KeyDbgLines, sizeof(KeyDbgLines));
    CursorBlinkRedraw = FALSE;
    MarkFullRepaint();
    DirtyLineTop = 1;
    DirtyLineBottom = 0;
    LastPaintedLineCount = 0;
}

/* 7-segment patterns (a..g, standard GFEDCBA-style bit order used below). */
#define LCD7_A 0x01
#define LCD7_B 0x02
#define LCD7_C 0x04
#define LCD7_D 0x08
#define LCD7_E 0x10
#define LCD7_F 0x20
#define LCD7_G 0x40

static const UINT8 LCD7_DIGITS[10] = {
    LCD7_A | LCD7_B | LCD7_C | LCD7_D | LCD7_E | LCD7_F,
    LCD7_B | LCD7_C,
    LCD7_A | LCD7_B | LCD7_G | LCD7_E | LCD7_D,
    LCD7_A | LCD7_B | LCD7_G | LCD7_C | LCD7_D,
    LCD7_F | LCD7_G | LCD7_B | LCD7_C,
    LCD7_A | LCD7_F | LCD7_G | LCD7_C | LCD7_D,
    LCD7_A | LCD7_F | LCD7_E | LCD7_D | LCD7_C | LCD7_G,
    LCD7_A | LCD7_B | LCD7_C,
    LCD7_A | LCD7_B | LCD7_C | LCD7_D | LCD7_E | LCD7_F | LCD7_G,
    LCD7_A | LCD7_F | LCD7_G | LCD7_C | LCD7_D,
};

static VOID LcdDraw7SegDigit(FRAMEBUFFER *fb, UINT32 x, UINT32 y, UINT32 cw, UINT32 ch, UINT32 digit,
                             UINT32 onC, UINT32 offC) {
    UINT32 th = ch / 9 + 2;

    UINT32 mw, mh;
    UINT8 segs;

    if (th > ch / 4)
        th = ch / 4;
    if (th < 2)
        th = 2;
    mw = cw - 2 * th;
    if (mw < 4)
        mw = 4;
    mh = (ch - 3 * th) / 2;
    if (mh < 3)
        mh = 3;

    if (digit == 10) {
        DrawRect(fb, x + th, y, mw, th, offC);
        DrawRect(fb, x + cw - th, y + th, th, mh, offC);
        DrawRect(fb, x + cw - th, y + th + mh + th, th, mh, offC);
        DrawRect(fb, x + th, y + ch - th, mw, th, offC);
        DrawRect(fb, x, y + th + mh + th, th, mh, offC);
        DrawRect(fb, x, y + th, th, mh, offC);
        DrawRect(fb, x + th, y + th + mh, mw, th, onC);
        return;
    }
    if (digit == 11) {
        DrawRect(fb, x + th, y, mw, th, offC);
        DrawRect(fb, x + cw - th, y + th, th, mh, offC);
        DrawRect(fb, x + cw - th, y + th + mh + th, th, mh, offC);
        DrawRect(fb, x + th, y + ch - th, mw, th, offC);
        DrawRect(fb, x, y + th + mh + th, th, mh, offC);
        DrawRect(fb, x, y + th, th, mh, offC);
        DrawRect(fb, x + th, y + th + mh, mw, th, offC);
        return;
    }

    segs = (digit < 10) ? LCD7_DIGITS[digit] : 0;

    DrawRect(fb, x + th, y, mw, th, (segs & LCD7_A) ? onC : offC);
    DrawRect(fb, x + cw - th, y + th, th, mh, (segs & LCD7_B) ? onC : offC);
    DrawRect(fb, x + cw - th, y + th + mh + th, th, mh, (segs & LCD7_C) ? onC : offC);
    DrawRect(fb, x + th, y + ch - th, mw, th, (segs & LCD7_D) ? onC : offC);
    DrawRect(fb, x, y + th + mh + th, th, mh, (segs & LCD7_E) ? onC : offC);
    DrawRect(fb, x, y + th, th, mh, (segs & LCD7_F) ? onC : offC);
    DrawRect(fb, x + th, y + th + mh, mw, th, (segs & LCD7_G) ? onC : offC);
}

static VOID LcdDrawColon(FRAMEBUFFER *fb, UINT32 x, UINT32 y, UINT32 ch, UINT32 onC) {
    UINT32 d = ch / 6;

    if (d < 2)
        d = 2;
    DrawRect(fb, x, y + ch / 3 - d / 2, d, d, onC);
    DrawRect(fb, x, y + 2 * ch / 3 - d / 2, d, d, onC);
}

/* Gray face, black “on” segments: centered 7-seg HH:MM session timer + file/slot banner (left). */
static VOID DrawCasioStatusHud(FRAMEBUFFER *fb, UINT32 hudY, UINT32 docBg) {
    UINT32 face = RGB(168, 172, 158);
    UINT32 frame = RGB(72, 74, 70);
    UINT32 segOff = RGB(130, 134, 122);
    UINT32 segOn = RGB(12, 14, 10);
    UINT32 cw = 15;
    UINT32 ch = 26;
    UINT32 gap = 3;
    UINT32 clockW = 4 * cw + gap * 2 + 6;
    UINT32 clockH = ch + 8;
    UINT32 clockX = (fb->Width > clockW) ? (fb->Width - clockW) / 2 : 4;
    UINT32 h1, h2, m1, m2;
    FONT_KIND saveFont;
    UINT32 saveFs;

    if (fb->Height < hudY + LCD_STATUS_ROW_H + 8)
        return;

    DrawRect(fb, clockX - 3, hudY, clockW + 6, clockH, docBg);

    DrawRect(fb, clockX - 3, hudY, clockW + 6, clockH, frame);
    DrawRect(fb, clockX - 1, hudY + 2, clockW + 2, clockH - 4, face);

    {
        UINT64 sec = SessionElapsedUs / 1000000ULL;
        UINT32 h = (UINT32)((sec / 3600ULL) % 100ULL);
        UINT32 m = (UINT32)((sec / 60ULL) % 60ULL);

        h1 = h / 10;
        h2 = h % 10;
        m1 = m / 10;
        m2 = m % 10;
    }

    {
        UINT32 cy = hudY + 5;
        UINT32 cx = clockX + 5;

        LcdDraw7SegDigit(fb, cx, cy, cw, ch, h1, segOn, segOff);
        cx += cw + gap;
        LcdDraw7SegDigit(fb, cx, cy, cw, ch, h2, segOn, segOff);
        cx += cw + gap;
        LcdDrawColon(fb, cx + 1, cy, ch, segOn);
        cx += 6;
        LcdDraw7SegDigit(fb, cx, cy, cw, ch, m1, segOn, segOff);
        cx += cw + gap;
        LcdDraw7SegDigit(fb, cx, cy, cw, ch, m2, segOn, segOff);
    }

    {
        UINT32 bx = 10;
        UINT32 by = hudY + 4;

        saveFont = CurrentFontKind;
        saveFs = FontScaleTwice;
        CurrentFontKind = FONT_SIMPLE;
        FontScaleTwice = 2;
        if (FileOpBannerRemainUs > 0 && FileOpBanner[0]) {
            DrawString(fb, bx, by, FileOpBanner, segOn, face);
        } else {
            CHAR16 sp[32];

            UnicodeSPrint(sp, sizeof(sp), L"S%u  P%u/%u", SaveSlotIndex + 1,
                          ActiveRamPageIndex + 1, RamPageCount);
            DrawString(fb, bx, by, sp, segOn, face);
        }
        CurrentFontKind = saveFont;
        FontScaleTwice = saveFs;
    }
}

static UINTN Utf8WriteChar(CHAR16 c, UINT8 *dst, UINTN left) {
    if (left < 1)
        return 0;
    if (c < 0x80) {
        dst[0] = (UINT8)c;
        return 1;
    }
    if (c < 0x800 && left >= 2) {
        dst[0] = (UINT8)(0xC0 | ((c >> 6) & 0x1F));
        dst[1] = (UINT8)(0x80 | (c & 0x3F));
        return 2;
    }
    if (left >= 3) {
        dst[0] = (UINT8)(0xE0 | ((c >> 12) & 0x0F));
        dst[1] = (UINT8)(0x80 | ((c >> 6) & 0x3F));
        dst[2] = (UINT8)(0x80 | (c & 0x3F));
        return 3;
    }
    return 0;
}

static VOID TypewriterLoadBytesIntoDoc(const UINT8 *data, UINTN len);

static VOID FileOpSetBannerOk(const CHAR16 *msg) {
    StrCpy(FileOpBanner, msg);
    FileOpBannerRemainUs = FILE_BANNER_HOLD_US;
    HudNeedPaint = TRUE;
}

static VOID FileOpSetBannerErr(const CHAR16 *msg, EFI_STATUS st) {
    UnicodeSPrint(FileOpBanner, sizeof(FileOpBanner), L"%s  (%lx)", msg, (UINTN)st);
    FileOpBannerRemainUs = FILE_BANNER_HOLD_US;
    HudNeedPaint = TRUE;
}

static VOID TypewriterFormatPagePath(CHAR16 *buf, UINTN bufBytes, UINT32 slot0, UINT32 page1based) {
    UnicodeSPrint(buf, bufBytes, L"TWS%uP%02u.TXT", slot0 + 1, page1based);
}

static VOID CopyDocToRamPage(UINT32 idx) {
    CopyMem(RamPages[idx].Grid, Doc.Grid, sizeof(Doc.Grid));
    RamPages[idx].CursorCol = Doc.CursorCol;
    RamPages[idx].CursorRow = Doc.CursorRow;
    RamPages[idx].ScrollYPx = Doc.ScrollYPx;
}

static VOID CopyRamPageToDoc(UINT32 idx) {
    CopyMem(Doc.Grid, RamPages[idx].Grid, sizeof(Doc.Grid));
    Doc.CursorCol = RamPages[idx].CursorCol;
    Doc.CursorRow = RamPages[idx].CursorRow;
    Doc.ScrollYPx = RamPages[idx].ScrollYPx;
    Doc.Modified = TRUE;
    MarkFullRepaint();
}

static EFI_STATUS TypewriterSaveGridToPath(EFI_HANDLE img, const CHAR16 *filename,
                                           const CHAR16 (*grid)[PAGE_COLS + 1]) {
    EFI_GUID liGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    EFI_FILE *root = NULL;
    EFI_FILE *f = NULL;
    EFI_STATUS st;
    UINTN cap = (UINTN)PAGE_ROWS * PAGE_COLS * 4 + PAGE_ROWS * 2 + 64;
    UINT8 *buf = NULL;
    UINTN used = 0;
    UINT32 L;
    CHAR16 *p;

    st = uefi_call_wrapper(BS->HandleProtocol, 3, img, &liGuid, (VOID **)&li);
    if (EFI_ERROR(st) || li == NULL || li->DeviceHandle == NULL)
        return (st != EFI_SUCCESS) ? st : EFI_NOT_READY;

    root = LibOpenRoot(li->DeviceHandle);
    if (root == NULL)
        return EFI_NO_MEDIA;

    if (!EFI_ERROR(uefi_call_wrapper(root->Open, 5, root, &f, (CHAR16 *)filename,
                                     EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, (UINT64)0))) {
        uefi_call_wrapper(f->Delete, 1, f);
        f = NULL;
    }

    st = uefi_call_wrapper(root->Open, 5, root, &f, (CHAR16 *)filename,
                           EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                           (UINT64)0);
    if (EFI_ERROR(st)) {
        uefi_call_wrapper(root->Close, 1, root);
        return st;
    }

    buf = AllocatePool(cap);
    if (buf == NULL) {
        uefi_call_wrapper(f->Close, 1, f);
        uefi_call_wrapper(root->Close, 1, root);
        return EFI_OUT_OF_RESOURCES;
    }

    {
        UINT32 lastRow = PAGE_ROWS;

        while (lastRow > 0 && GridRowLastUsedCol(grid[lastRow - 1]) == 0)
            lastRow--;

        for (L = 0; L < lastRow; L++) {
            UINT32 c;
            UINT32 last = GridRowLastUsedCol(grid[L]);

            p = (CHAR16 *)grid[L];
            for (c = 0; c < last && used + 4 < cap; c++) {
                UINTN n = Utf8WriteChar(p[c], buf + used, cap - used);

                if (n == 0)
                    break;
                used += n;
            }
            if (used + 1 < cap)
                buf[used++] = '\n';
        }
    }

    {
        UINTN wlen = used;

        st = uefi_call_wrapper(f->Write, 3, f, &wlen, buf);
        if (!EFI_ERROR(st) && wlen != used)
            st = EFI_DEVICE_ERROR;
    }
    uefi_call_wrapper(f->Flush, 1, f);
    uefi_call_wrapper(f->Close, 1, f);
    uefi_call_wrapper(root->Close, 1, root);
    FreePool(buf);
    return st;
}

static EFI_STATUS TypewriterSaveCurrentSlotPage(EFI_HANDLE img) {
    CHAR16 path[48];

    CopyDocToRamPage(ActiveRamPageIndex);
    TypewriterFormatPagePath(path, sizeof(path), SaveSlotIndex, ActiveRamPageIndex + 1);
    return TypewriterSaveGridToPath(img, path, RamPages[ActiveRamPageIndex].Grid);
}

static EFI_STATUS TypewriterLoadDocFromPath(EFI_HANDLE img, const CHAR16 *filename) {
    EFI_GUID liGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    EFI_FILE *root = NULL;
    EFI_FILE *f = NULL;
    EFI_STATUS st;
    UINT8 *buf = NULL;
    UINTN sz;

    st = uefi_call_wrapper(BS->HandleProtocol, 3, img, &liGuid, (VOID **)&li);
    if (EFI_ERROR(st) || li == NULL || li->DeviceHandle == NULL)
        return (st != EFI_SUCCESS) ? st : EFI_NOT_READY;

    root = LibOpenRoot(li->DeviceHandle);
    if (root == NULL)
        return EFI_NO_MEDIA;

    st = uefi_call_wrapper(root->Open, 5, root, &f, (CHAR16 *)filename, EFI_FILE_MODE_READ, (UINT64)0);
    if (EFI_ERROR(st)) {
        uefi_call_wrapper(root->Close, 1, root);
        return st;
    }

    buf = AllocatePool(DOC_FILE_MAX_BYTES + 1);
    if (buf == NULL) {
        uefi_call_wrapper(f->Close, 1, f);
        uefi_call_wrapper(root->Close, 1, root);
        return EFI_OUT_OF_RESOURCES;
    }

    sz = DOC_FILE_MAX_BYTES;
    st = uefi_call_wrapper(f->Read, 3, f, &sz, buf);
    if (!EFI_ERROR(st)) {
        buf[sz] = 0;
        TypewriterLoadBytesIntoDoc(buf, sz);
    }
    uefi_call_wrapper(f->Close, 1, f);
    uefi_call_wrapper(root->Close, 1, root);
    FreePool(buf);
    return st;
}

static EFI_STATUS TypewriterLoadCurrentSlotPage(EFI_HANDLE img) {
    CHAR16 path[48];

    TypewriterFormatPagePath(path, sizeof(path), SaveSlotIndex, ActiveRamPageIndex + 1);
    return TypewriterLoadDocFromPath(img, path);
}

static EFI_STATUS TypewriterFlushRamToSlot(EFI_HANDLE img, UINT32 slot0) {
    UINT32 i;
    CHAR16 path[48];
    EFI_STATUS st;

    for (i = 0; i < RamPageCount; i++) {
        TypewriterFormatPagePath(path, sizeof(path), slot0, i + 1);
        st = TypewriterSaveGridToPath(img, path, RamPages[i].Grid);
        if (EFI_ERROR(st))
            return st;
    }
    return EFI_SUCCESS;
}

static UINT32 TypewriterLoadSlotPagesFromDisk(EFI_HANDLE img, UINT32 slot0) {
    UINT32 n = 0;
    UINT32 p;
    CHAR16 path[48];
    EFI_STATUS st;

    for (p = 1; p <= MAX_RAM_PAGES; p++) {
        TypewriterFormatPagePath(path, sizeof(path), slot0, p);
        st = TypewriterLoadDocFromPath(img, path);
        if (EFI_ERROR(st))
            break;
        CopyDocToRamPage(n);
        n++;
    }
    return n;
}

static VOID TypewriterLoadBytesIntoDoc(const UINT8 *data, UINTN len) {
    UINT32 line = 0;
    UINT32 col = 0;
    UINTN i = 0;
    UINT32 cpad;

    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
        i = 3;

    ZeroMem(&Doc, sizeof(Doc));
    InitDocumentGridSpaces();
    Doc.Modified = TRUE;
    Doc.CursorCol = 0;
    Doc.CursorRow = 0;
    Doc.ScrollYPx = 0;
    ZeroMem(KeyDbgLines, sizeof(KeyDbgLines));

    while (i < len && line < PAGE_ROWS) {
        UINT8 b0 = data[i];
        CHAR16 c = 0;
        UINTN adv = 1;

        if (b0 < 0x80) {
            c = (CHAR16)b0;
        } else if ((b0 & 0xE0) == 0xC0 && i + 1 < len) {
            c = (CHAR16)(((b0 & 0x1F) << 6) | (data[i + 1] & 0x3F));
            adv = 2;
        } else if ((b0 & 0xF0) == 0xE0 && i + 2 < len) {
            c = (CHAR16)(((b0 & 0x0F) << 12) | ((data[i + 1] & 0x3F) << 6) |
                         (data[i + 2] & 0x3F));
            adv = 3;
        } else {
            i++;
            continue;
        }

        i += adv;

        if (c == L'\n' || c == L'\r') {
            if (c == L'\r' && i < len && data[i] == '\n')
                i++;
            for (cpad = col; cpad < PAGE_COLS; cpad++)
                Doc.Grid[line][cpad] = L' ';
            Doc.Grid[line][PAGE_COLS] = 0;
            line++;
            col = 0;
            if (line >= PAGE_ROWS)
                break;
            continue;
        }

        if (col < PAGE_COLS)
            Doc.Grid[line][col++] = c;
    }

    if (line < PAGE_ROWS) {
        for (cpad = col; cpad < PAGE_COLS; cpad++)
            Doc.Grid[line][cpad] = L' ';
        Doc.Grid[line][PAGE_COLS] = 0;
    }

    ReflowAllLines();
    {
        UINT32 ly = (line < PAGE_ROWS) ? line : PAGE_ROWS - 1;
        UINT32 lastCol = GridRowLastUsedCol(Doc.Grid[ly]);

        Doc.CursorRow = ly;
        Doc.CursorCol = (lastCol < PAGE_COLS) ? lastCol : PAGE_COLS - 1;
    }
    MarkFullRepaint();
}

/*
 * Boot: prefer TWS1P01.TXT, TWS1P02.TXT, … then legacy Typewriter.txt.
 * Missing files are silent; other errors show in the HUD.
 */
static VOID TypewriterAutoloadIfPresent(EFI_HANDLE img) {
    UINT32 n = 0;
    UINT32 p;
    CHAR16 path[48];
    EFI_STATUS st;

    for (p = 1; p <= MAX_RAM_PAGES; p++) {
        TypewriterFormatPagePath(path, sizeof(path), 0, p);
        st = TypewriterLoadDocFromPath(img, path);
        if (EFI_ERROR(st))
            break;
        CopyDocToRamPage(n);
        n++;
    }

    if (n > 0) {
        RamPageCount = n;
        ActiveRamPageIndex = 0;
        SaveSlotIndex = 0;
        CopyRamPageToDoc(0);
        FileOpSetBannerOk(L"Loaded slot 1 (pages)");
        Print(L"[Typewrite] startup: loaded %u page(s) from TWS1Pxx.TXT\r\n", n);
        return;
    }

    st = TypewriterLoadDocFromPath(img, TYPEWRITER_DOC_FILENAME);
    if (!EFI_ERROR(st)) {
        RamPageCount = 1;
        ActiveRamPageIndex = 0;
        SaveSlotIndex = 0;
        CopyDocToRamPage(0);
        FileOpSetBannerOk(L"Loaded Typewriter.txt");
        Print(L"[Typewrite] startup: loaded Typewriter.txt\r\n");
        return;
    }

    if (st != EFI_NOT_FOUND) {
        FileOpSetBannerErr(L"Startup load failed", st);
        Print(L"[Typewrite] startup load %r\r\n", st);
    }
    RamPageCount = 1;
    ActiveRamPageIndex = 0;
    SaveSlotIndex = 0;
    CopyDocToRamPage(0);
}

static VOID PageGoNext(EFI_HANDLE img) {
    UINT32 cur = ActiveRamPageIndex;
    CHAR16 path[48];
    EFI_STATUS st;

    if (cur + 1 >= MAX_RAM_PAGES) {
        FileOpSetBannerOk(L"Page limit reached (32)");
        return;
    }
    CopyDocToRamPage(cur);
    TypewriterFormatPagePath(path, sizeof(path), SaveSlotIndex, cur + 1);
    st = TypewriterSaveGridToPath(img, path, RamPages[cur].Grid);
    if (EFI_ERROR(st)) {
        FileOpSetBannerErr(L"Page save failed", st);
        return;
    }
    cur++;
    if (cur < RamPageCount) {
        ActiveRamPageIndex = cur;
        CopyRamPageToDoc(cur);
    } else {
        ActiveRamPageIndex = cur;
        RamPageCount = cur + 1;
        InitDocument();
        CopyDocToRamPage(cur);
    }
    FileOpSetBannerOk(L"Next page");
    Doc.Modified = TRUE;
    HudNeedPaint = TRUE;
}

static VOID PageGoPrev(EFI_HANDLE img) {
    UINT32 cur = ActiveRamPageIndex;
    CHAR16 path[48];
    EFI_STATUS st;

    CopyDocToRamPage(cur);
    TypewriterFormatPagePath(path, sizeof(path), SaveSlotIndex, cur + 1);
    st = TypewriterSaveGridToPath(img, path, RamPages[cur].Grid);
    if (EFI_ERROR(st))
        FileOpSetBannerErr(L"Page save failed", st);
    if (cur == 0) {
        FileOpSetBannerOk(L"First page");
        Doc.Modified = TRUE;
        HudNeedPaint = TRUE;
        return;
    }
    cur--;
    ActiveRamPageIndex = cur;
    CopyRamPageToDoc(cur);
    FileOpSetBannerOk(L"Previous page");
    Doc.Modified = TRUE;
    HudNeedPaint = TRUE;
}

static VOID SlotCycleNext(EFI_HANDLE img) {
    EFI_STATUS st;
    UINT32 oldSlot = SaveSlotIndex;
    UINT32 n;
    CHAR16 b[64];

    CopyDocToRamPage(ActiveRamPageIndex);
    st = TypewriterFlushRamToSlot(img, oldSlot);
    if (EFI_ERROR(st)) {
        FileOpSetBannerErr(L"Slot flush failed", st);
        return;
    }
    SaveSlotIndex = (SaveSlotIndex + 1) % SAVE_SLOT_COUNT;
    n = TypewriterLoadSlotPagesFromDisk(img, SaveSlotIndex);
    if (n == 0) {
        RamPageCount = 1;
        ActiveRamPageIndex = 0;
        InitDocument();
        CopyDocToRamPage(0);
    } else {
        RamPageCount = n;
        ActiveRamPageIndex = 0;
        CopyRamPageToDoc(0);
    }
    UnicodeSPrint(b, sizeof(b), L"Switched to slot %u", SaveSlotIndex + 1);
    FileOpSetBannerOk(b);
    Doc.Modified = TRUE;
    HudNeedPaint = TRUE;
}

static VOID KeyDbgPush(const EFI_INPUT_KEY *k) {
    UINTN i;
    for (i = 0; i < KEY_DBG_LINES - 1; i++)
        CopyMem(KeyDbgLines[i], KeyDbgLines[i + 1], sizeof(KeyDbgLines[0]));
    UnicodeSPrint(KeyDbgLines[KEY_DBG_LINES - 1], sizeof(KeyDbgLines[KEY_DBG_LINES - 1]),
                  L"SC=0x%04x  UC=0x%04x",
                  (UINTN)k->ScanCode, (UINTN)k->UnicodeChar);
}

static VOID DrawKeyDebugOverlay(FRAMEBUFFER *fb) {
    UINT32 bg = RGB(28, 32, 40);
    UINT32 fg = RGB(230, 210, 80);
    UINT32 hdr = RGB(255, 245, 200);
    UINT32 row = FONT_SZ_MUL(ActiveFontLineHeight()) + FONT_SZ_MUL(2);
    UINT32 panelH = row * (KEY_DBG_LINES + 2) + 40;
    UINT32 y0 = (panelH + 24 < fb->Height) ? fb->Height - panelH - 24 : TOP_MARGIN;
    DrawRect(fb, 4, y0 - 4, fb->Width - 8, panelH, bg);
    UINT32 ly = y0 + 8;
    DrawString(fb, 12, ly, L"[F7] Key debug — last keys (serial too)", hdr, bg);
    ly += row;
    for (UINT32 i = 0; i < KEY_DBG_LINES && ly + row < fb->Height - 8; i++) {
        DrawString(fb, 12, ly, KeyDbgLines[i], fg, bg);
        ly += row;
    }
}

EFI_STATUS RenderDocument(FRAMEBUFFER *fb) {
    UINT32 bgColor = COLORS[CurrentBgColor];
    UINT32 fgColor = (CurrentBgColor == 1) ? RGB(30, 30, 30) : RGB(240, 240, 230);
    UINT32 prevPainted = LastPaintedLineCount;
    UINT32 hudY = (fb->Height > LCD_STATUS_ROW_H + LCD_STATUS_MARGIN_BOT)
                      ? (fb->Height - LCD_STATUS_ROW_H - LCD_STATUS_MARGIN_BOT)
                      : 0;
    BOOLEAN clearedAllForHud = FALSE;
    UINT32 lineBodyH = FONT_SZ_MUL(ActiveFontLineHeight());

    UpdatePageLayoutFromFb(fb);
    EnsureCursorVisibleScroll(fb);
    KickFirmwareWatchdog();

    /*
     * docDirty: repaint document + cursor. Otherwise only overlays (e.g. F7 log
     * refresh without re-clearing the frame).
     */
    BOOLEAN docDirty =
        RepaintFull || ShowHelp || (DirtyLineTop <= DirtyLineBottom);
    BOOLEAN cursorBlinkOnly =
        CursorBlinkRedraw && !docDirty && !ShowHelp && CursorWantsBlinkTimer();

    if (docDirty) {
        clearedAllForHud = RepaintFull || ShowHelp;
        if (RepaintFull || ShowHelp) {
            DrawRect(fb, 0, 0, fb->Width, fb->Height, OFF_PAGE_COLOR);
            if (hudY < fb->Height)
                DrawRect(fb, 0, hudY, fb->Width, fb->Height - hudY, bgColor);
            DrawClippedPaperFill(fb, bgColor, G_docViewportBot);
            for (UINT32 line = 0; line < PAGE_ROWS; line++) {
                DrawGridRowIfVisible(fb, line, fgColor, bgColor);
                LastLinePaintedRight[line] = LinePaintRightX(fb, Doc.Grid[line], line);
                if ((line & 15) == 15)
                    KickFirmwareWatchdog();
            }
            RepaintFull = FALSE;
        } else {
            UINT32 top = DirtyLineTop;
            UINT32 bot = DirtyLineBottom;

            if (bot >= PAGE_ROWS)
                bot = PAGE_ROWS - 1;

            for (UINT32 line = top; line <= bot; line++) {
                INT32 sy = RowTextScreenY(line);
                UINT32 clearR;
                CHAR16 *ln = Doc.Grid[line];
                UINT32 yu;

                if (sy < 0 || sy >= (INT32)G_docViewportBot || sy + (INT32)lineBodyH <= 0)
                    continue;
                yu = (UINT32)sy;
                clearR = LinePaintRightX(fb, ln, line);
                if (line < PAGE_ROWS && LastLinePaintedRight[line] > clearR)
                    clearR = LastLinePaintedRight[line];
                if (clearR > G_pageContentX0)
                    DrawRect(fb, G_pageContentX0, yu, clearR - G_pageContentX0, lineBodyH, bgColor);
                DrawGridRowIfVisible(fb, line, fgColor, bgColor);
                if (line < PAGE_ROWS)
                    LastLinePaintedRight[line] = LinePaintRightX(fb, ln, line);
            }
            (void)prevPainted;
        }
        if (CursorShouldDrawThisFrame() && !ShowHelp) {
            INT32 csy = RowTextScreenY(Doc.CursorRow);

            if (csy >= 0 && csy < (INT32)G_docViewportBot && csy + (INT32)lineBodyH > 0) {
                UINT32 yu = (UINT32)csy;
                UINT32 cx = DocCursorPixelX();
                UINT32 cwBar = CursorBarThickness();
                UINT32 lineBox = lineBodyH;

                if (CursorMode == CURSOR_BAR || CursorMode == CURSOR_BAR_BLINK)
                    DrawRect(fb, cx, yu, cwBar, lineBox, fgColor);
                else if (CursorMode == CURSOR_BLOCK || CursorMode == CURSOR_BLOCK_BLINK)
                    DrawRect(fb, cx, yu, CursorGlyphWidthOnLine(NULL), lineBox, fgColor);
            }
        }
        DirtyLineTop = 1;
        DirtyLineBottom = 0;
        LastPaintedLineCount = PAGE_ROWS;
        CursorBlinkRedraw = FALSE;
    } else if (cursorBlinkOnly) {
        RepaintDocLineCursorBand(fb, Doc.CursorRow, bgColor, fgColor);
        CursorBlinkRedraw = FALSE;
    }

    if (KeyDebugMode && !ShowHelp)
        DrawKeyDebugOverlay(fb);

    if (ShowHelp)
        DrawHelpOverlay(fb);

    {
        BOOLEAN needHud = HudNeedPaint || clearedAllForHud;

        if (needHud) {
            if (!clearedAllForHud)
                DrawRect(fb, 0, hudY, fb->Width, fb->Height - hudY, bgColor);
            DrawCasioStatusHud(fb, hudY, bgColor);
            HudNeedPaint = FALSE;
        }
    }

    KickFirmwareWatchdog();
    FlipFramebuffer(fb);
    return EFI_SUCCESS;
}

EFI_STATUS HandleKey(EFI_INPUT_KEY *key) {
    if (!key)
        return EFI_SUCCESS;
    KickFirmwareWatchdog();

    /*
     * UEFI: SCAN_UP is 0x0001; SCAN_ESC is 0x0017 (see eficon.h). Treating 0x01
     * as ESC breaks the Up Arrow on spec-compliant firmware.
     */
    if (key->ScanCode == SCAN_ESC ||
        (key->ScanCode == SCAN_NULL && key->UnicodeChar == 0x001b)) {
        if (ShowHelp) {
            ShowHelp = FALSE;
            MarkFullRepaint();
            Doc.Modified = TRUE;
            return EFI_SUCCESS;
        }
        Running = FALSE;
        return EFI_SUCCESS;
    }

    if (key->ScanCode == SCAN_INSERT && key->UnicodeChar == 0) {
        ShowLineNumbers = !ShowLineNumbers;
        MarkFullRepaint();
        Doc.Modified = TRUE;
        HudNeedPaint = TRUE;
        return EFI_SUCCESS;
    }

    if (key->ScanCode >= SCAN_F1 && key->ScanCode <= SCAN_F12) {
        switch (key->ScanCode) {
            case SCAN_F1:  /* Help */
                ShowHelp = !ShowHelp;
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case SCAN_F2:  /* Cycle font */
                CurrentFontKind = (FONT_KIND)((CurrentFontKind + 1) % FONT_NUM);
                ReflowAllLines();
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case SCAN_F3:  /* Increase font / scale (half steps: 1.0 … 6.0×) */
                if (FontScaleTwice < 12)
                    FontScaleTwice++;
                ReflowAllLines();
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case SCAN_F4:  /* Cycle background */
                CurrentBgColor = (CurrentBgColor + 1) % 10;
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case SCAN_F5:  /* Cycle cursor style */
                CursorMode = (CursorMode + 1) % CURSOR_MODE_NUM;
                CursorBlinkAccumUs = 0;
                CursorBlinkPhase = TRUE;
                MarkDirtyRange(Doc.CursorRow, Doc.CursorRow);
                Doc.Modified = TRUE;
                break;
            case SCAN_F6:  /* Decrease font / scale */
                if (FontScaleTwice > 2)
                    FontScaleTwice--;
                ReflowAllLines();
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case SCAN_F7:  /* Toggle key debug overlay + serial log */
                KeyDebugMode = !KeyDebugMode;
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case SCAN_F8: {
                CHAR16 b[64];
                EFI_STATUS fst = TypewriterSaveCurrentSlotPage(BootImageHandle);

                if (!EFI_ERROR(fst)) {
                    UnicodeSPrint(b, sizeof(b), L"Saved TWS%uP%02u", SaveSlotIndex + 1,
                                  ActiveRamPageIndex + 1);
                    FileOpSetBannerOk(b);
                } else
                    FileOpSetBannerErr(L"Save failed", fst);
                Print(L"[Typewrite] save %r\n", fst);
                MarkFullRepaint();
                HudNeedPaint = TRUE;
                Doc.Modified = TRUE;
                break;
            }
            case SCAN_F9: {
                CHAR16 b[64];
                EFI_STATUS fst = TypewriterLoadCurrentSlotPage(BootImageHandle);

                if (!EFI_ERROR(fst)) {
                    CopyDocToRamPage(ActiveRamPageIndex);
                    UnicodeSPrint(b, sizeof(b), L"Loaded TWS%uP%02u", SaveSlotIndex + 1,
                                  ActiveRamPageIndex + 1);
                    FileOpSetBannerOk(b);
                } else
                    FileOpSetBannerErr(L"Load failed", fst);
                Print(L"[Typewrite] load %r\n", fst);
                Doc.Modified = TRUE;
                HudNeedPaint = TRUE;
                break;
            }
            case SCAN_F10:
                SlotCycleNext(BootImageHandle);
                HudNeedPaint = TRUE;
                break;
            case SCAN_F11:
                PageGoNext(BootImageHandle);
                MarkFullRepaint();
                HudNeedPaint = TRUE;
                break;
            case SCAN_F12:
                PageGoPrev(BootImageHandle);
                MarkFullRepaint();
                HudNeedPaint = TRUE;
                break;
        }
        KickFirmwareWatchdog();
        return EFI_SUCCESS;
    }

    if (key->UnicodeChar == 0) {
        UINT32 oldR = Doc.CursorRow;
        UINT32 oldC = Doc.CursorCol;

        if (key->ScanCode == SCAN_LEFT) {
            if (Doc.CursorCol > 0)
                Doc.CursorCol--;
            else if (Doc.CursorRow > 0) {
                Doc.CursorRow--;
                Doc.CursorCol = PAGE_COLS - 1;
            }
        } else if (key->ScanCode == SCAN_RIGHT) {
            if (Doc.CursorCol < PAGE_COLS - 1)
                Doc.CursorCol++;
            else if (Doc.CursorRow < PAGE_ROWS - 1) {
                Doc.CursorRow++;
                Doc.CursorCol = 0;
            }
        } else if (key->ScanCode == SCAN_UP) {
            if (Doc.CursorRow > 0)
                Doc.CursorRow--;
        } else if (key->ScanCode == SCAN_DOWN) {
            if (Doc.CursorRow < PAGE_ROWS - 1)
                Doc.CursorRow++;
        } else if (key->ScanCode == SCAN_PAGE_DOWN) {
            PageGoNext(BootImageHandle);
            MarkFullRepaint();
            KickFirmwareWatchdog();
            return EFI_SUCCESS;
        } else if (key->ScanCode == SCAN_PAGE_UP) {
            PageGoPrev(BootImageHandle);
            MarkFullRepaint();
            KickFirmwareWatchdog();
            return EFI_SUCCESS;
        }

        if (oldR != Doc.CursorRow || oldC != Doc.CursorCol) {
            MarkDirtyRange(oldR, Doc.CursorRow);
            Doc.Modified = TRUE;
            KickFirmwareWatchdog();
            return EFI_SUCCESS;
        }
    }

    if (key->UnicodeChar == 0x08) {  /* Backspace */
        if (Doc.CursorCol > 0) {
            Doc.CursorCol--;
            Doc.Grid[Doc.CursorRow][Doc.CursorCol] = L' ';
            Doc.Grid[Doc.CursorRow][PAGE_COLS] = 0;
            MarkDirtyRange(Doc.CursorRow, Doc.CursorRow);
            Doc.Modified = TRUE;
        } else if (Doc.CursorRow > 0) {
            Doc.CursorRow--;
            Doc.CursorCol = PAGE_COLS - 1;
            Doc.Grid[Doc.CursorRow][Doc.CursorCol] = L' ';
            Doc.Grid[Doc.CursorRow][PAGE_COLS] = 0;
            MarkDirtyRange(Doc.CursorRow, Doc.CursorRow + 1);
            Doc.Modified = TRUE;
        }
        return EFI_SUCCESS;
    }
    
    if (key->UnicodeChar == 0x09) {  /* Tab */
        UINT32 r0 = Doc.CursorRow;
        UINT32 r1 = Doc.CursorRow;
        INT32 k;

        for (k = 0; k < 4; k++) {
            Doc.Grid[Doc.CursorRow][Doc.CursorCol] = L' ';
            if (Doc.CursorCol < PAGE_COLS - 1)
                Doc.CursorCol++;
            else if (Doc.CursorRow < PAGE_ROWS - 1) {
                Doc.CursorRow++;
                Doc.CursorCol = 0;
                r1 = Doc.CursorRow;
            } else
                break;
        }
        Doc.Grid[Doc.CursorRow][PAGE_COLS] = 0;
        MarkDirtyRange(r0, r1);
        Doc.Modified = TRUE;
        return EFI_SUCCESS;
    }
    
    if (key->UnicodeChar == 0x0D || key->UnicodeChar == 0x0A) {  /* Enter */
        if (Doc.CursorRow < PAGE_ROWS - 1) {
            UINT32 oldCy = Doc.CursorRow;

            Doc.CursorRow++;
            Doc.CursorCol = 0;
            MarkDirtyRange(oldCy, Doc.CursorRow);
            Doc.Modified = TRUE;
        }
        return EFI_SUCCESS;
    }
    
    /* Printable BMP (not DEL); Latin/typical layouts need code points > 127 */
    if (key->UnicodeChar >= 32 && key->UnicodeChar != 0x7F) {
        UINT32 r0 = Doc.CursorRow;
        UINT32 r1 = Doc.CursorRow;

        Doc.Grid[Doc.CursorRow][Doc.CursorCol] = key->UnicodeChar;
        if (Doc.CursorCol < PAGE_COLS - 1)
            Doc.CursorCol++;
        else if (Doc.CursorRow < PAGE_ROWS - 1) {
            Doc.CursorRow++;
            Doc.CursorCol = 0;
            r1 = Doc.CursorRow;
        }
        Doc.Grid[r0][PAGE_COLS] = 0;
        if (r1 != r0)
            Doc.Grid[r1][PAGE_COLS] = 0;
        MarkDirtyRange(r0, r1);
        Doc.Modified = TRUE;
    }

    KickFirmwareWatchdog();
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    KickFirmwareWatchdog();
    BootImageHandle = ImageHandle;

    Print(L"\r\n========================================\r\n");
    Print(L"  Typewrite OS v1.0\r\n");
    Print(L"  UEFI Typewriter Application\r\n");
    Print(L"  F1 help  F8 save  F9 load  F10 slot  F11 next pg  F12 prev  Ins line#\r\n");
    Print(L"========================================\r\n\r\n");
    
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS status = LibLocateProtocol(&gopGuid, (VOID**)&Gop);
    if (EFI_ERROR(status)) {
        Print(L"Error: GOP not found (status=%r)\n", status);
        return status;
    }

    BOOLEAN apple_hw = IsAppleSystem();
    {
        UINT32 wantMode = apple_hw ? ApplePickGopMode(Gop) : Gop->Mode->Mode;

        /* gnu-efi: SetMode arity is 2 (per apps/bltgrid.c, not 1). */
        status = uefi_call_wrapper(Gop->SetMode, 2, Gop, wantMode);
        if (EFI_ERROR(status)) {
            Print(L"Warning: SetMode(%u) failed (status=%r)\n", wantMode, status);
            if (apple_hw && Gop->Mode->MaxMode > 0) {
                status = uefi_call_wrapper(Gop->SetMode, 2, Gop, 0);
                if (EFI_ERROR(status))
                    Print(L"Warning: SetMode(0) fallback failed (status=%r)\n", status);
            }
        }
        if (apple_hw)
            uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, FALSE);
    }

    FRAMEBUFFER fb;
    fb.Width = Gop->Mode->Info->HorizontalResolution;
    fb.Height = Gop->Mode->Info->VerticalResolution;
    /*
     * PixelsPerScanLine can be 0 on some firmware; Pitch must never be 0 or all
     * DrawPixel addressing breaks (black screen / single column).
     */
    {
        UINT32 ppl = Gop->Mode->Info->PixelsPerScanLine;
        if (ppl == 0 || ppl < fb.Width)
            ppl = fb.Width;
        fb.Pitch = ppl * 4;
    }
    GopUseBlt = FALSE;
    FbFront = (UINT8 *)(UINTN)Gop->Mode->FrameBufferBase;
    fb.PixelData = FbFront;
    /*
     * Non-Apple: direct GOP FB (Blt → black on some OVMF builds — GRAPHICS_DEBUG.md).
     * Apple: pool buffer + EfiBltBufferToVideo (linear FB writes are unreliable).
     */
    FbCopyBytes = 0;
    if (apple_hw && Gop->Blt) {
        UINTN nbytes = (UINTN)fb.Width * (UINTN)fb.Height * 4;
        UINT8 *pool = AllocatePool(nbytes);

        if (pool) {
            GopUseBlt = TRUE;
            fb.PixelData = pool;
            fb.Pitch = fb.Width * 4;
            /* Blt pixel buffer is EFI_GRAPHICS_OUTPUT_BLT_PIXEL layout (BGRx). */
            fb.PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
            fb.RedMask = 0xFF;
            FbFront = NULL;
        } else {
            Print(L"Warning: GOP Blt back-buffer alloc failed; using linear FB\n");
        }
    }

    if (!GopUseBlt) {
        fb.PixelFormat = Gop->Mode->Info->PixelFormat;
        if (fb.PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
            fb.RedMask = 0xFF;
        } else if (fb.PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
            fb.RedMask = 0xFF0000;
        } else if (fb.PixelFormat == PixelBitMask) {
            fb.RedMask = Gop->Mode->Info->PixelInformation.RedMask;
        } else {
            fb.RedMask = 0xFF;
        }
    }

    UpdatePageLayoutFromFb(&fb);
    RunSplashScreen(&fb);
    CursorBlinkPhase = TRUE;
    CursorBlinkAccumUs = 0;

    InitDocument();
    TypewriterAutoloadIfPresent(ImageHandle);
    SessionElapsedUs = 0;
    LastHudSessionMinute = (UINT32)-1;
    HudNeedPaint = TRUE;

    while (Running) {
        KickFirmwareWatchdog();
        if (Doc.Modified || HudNeedPaint) {
            RenderDocument(&fb);
            Doc.Modified = FALSE;
        }
        
        EFI_INPUT_KEY key;
        EFI_STATUS keyStatus = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
        
        if (!EFI_ERROR(keyStatus)) {
            KeyDbgPush(&key);
            {
                BOOLEAN dbgWas = KeyDebugMode;
                HandleKey(&key);
                if (KeyDebugMode)
                    Print(L"[KeyDbg] SC=0x%x  UC=0x%x\r\n",
                          key.ScanCode, key.UnicodeChar);
                if (KeyDebugMode || dbgWas)
                    Doc.Modified = TRUE;
            }
        }
        
        /* Idle longer when unchanged to avoid burning CPU; short delay after input */
        {
            UINT32 stallUs = EFI_ERROR(keyStatus) ? 50000 : 5000;
            UINT32 curMin;

            uefi_call_wrapper(BS->Stall, 1, stallUs);
            SessionElapsedUs += (UINT64)stallUs;
            curMin = (UINT32)(SessionElapsedUs / 60000000ULL);
            if (curMin != LastHudSessionMinute) {
                LastHudSessionMinute = curMin;
                HudNeedPaint = TRUE;
            }
            if (FileOpBannerRemainUs > 0) {
                if (stallUs >= FileOpBannerRemainUs) {
                    FileOpBannerRemainUs = 0;
                    HudNeedPaint = TRUE;
                } else {
                    FileOpBannerRemainUs -= stallUs;
                }
            }
            if (!ShowHelp && CursorWantsBlinkTimer()) {
                CursorBlinkAccumUs += stallUs;
                while (CursorBlinkAccumUs >= CURSOR_BLINK_PERIOD_US) {
                    CursorBlinkAccumUs -= CURSOR_BLINK_PERIOD_US;
                    CursorBlinkPhase = !CursorBlinkPhase;
                    CursorBlinkRedraw = TRUE;
                    Doc.Modified = TRUE;
                }
            }
        }
    }
    
    Print(L"Typewriter exited. Page rows: %u cols: %u\n", PAGE_ROWS, PAGE_COLS);
    
    return EFI_SUCCESS;
}
