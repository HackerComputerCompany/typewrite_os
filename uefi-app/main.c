/**
 * Typewriter.c - UEFI Typewriter Application with Virgil/Helvetica Fonts
 * 
 * A native UEFI typewriter experience with:
 * - Bitmap font rendering (Virgil, Inter, + retro / typewriter faces)
 * - Typewriter-style input with visual feedback
 * - File save/load from EFI variables
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

#define MAX_LINES 100
#define MAX_CHARS_PER_LINE 120
#define LEFT_MARGIN 20
#define RIGHT_MARGIN 24
#define TOP_MARGIN 30

typedef struct {
    UINT32 Width;
    UINT32 Height;
    UINT32 Pitch;
    UINT8  *PixelData;
    UINT32 RedMask;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
} FRAMEBUFFER;

typedef struct {
    CHAR16 Text[MAX_LINES][MAX_CHARS_PER_LINE];
    UINT32 LineCount;
    UINT32 CursorX;
    UINT32 CursorY;
    BOOLEAN Modified;
} DOCUMENT;

static EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;

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

static FONT_KIND CurrentFontKind = FONT_VIRGIL;
static BOOLEAN ShowHelp = FALSE;
static BOOLEAN KeyDebugMode = FALSE;

#define KEY_DBG_LINES 8
#define KEY_DBG_COLS 72

#define HELP_LINE_GAP 24

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
static UINT32 FontSize = 1;

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
static UINT32 WrapPixelWidth = 800;
static CHAR16 KeyDbgLines[KEY_DBG_LINES][KEY_DBG_COLS];

/* Repaint: full screen vs horizontal strips for changed document lines only. */
static BOOLEAN RepaintFull = TRUE;
static UINT32 DirtyLineTop = 1;
static UINT32 DirtyLineBottom = 0; /* invalid until MarkDirtyRange (Top <= Bottom). */
static UINT32 LastPaintedLineCount = 0;

#define CHAR_GAP 2
/* Word space advance multiplier (advance for ' ' is this × normal font advance). */
#define SPACE_ADVANCE_MULT 3

#define SCELL(_fb, _x, _y, _c) DrawRect((_fb), (_x), (_y), FontSize, FontSize, (_c))

static UINT32 GlyphAdvance(CHAR16 ch);

static VOID MarkFullRepaint(VOID) {
    RepaintFull = TRUE;
}

/*
 * Text wraps when LinePixelWidth exceeds this. Must track GOP width minus margins;
 * the old guard (width > margin + 80) left the default 800px wrap on many panels, so
 * lines never broke. Use saturating margin math (no UINT32 underflow).
 */
static VOID UpdateWrapPixelWidthFromFb(const FRAMEBUFFER *fb) {
    UINT32 margin = LEFT_MARGIN + RIGHT_MARGIN;
    UINT32 w;

    if (fb->Width > margin)
        w = fb->Width - margin;
    else if (fb->Width > 16)
        w = fb->Width - 8;
    else
        w = 48;
    if (w < 48)
        w = 48;
    WrapPixelWidth = w;
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

static UINT32 LineLen(const CHAR16 *s) {
    UINT32 n = 0;
    while (n < MAX_CHARS_PER_LINE && s[n])
        n++;
    return n;
}

static UINT32 LinePixelWidth(const CHAR16 *line) {
    UINT32 w = 0;
    for (UINT32 i = 0; i < MAX_CHARS_PER_LINE && line[i]; i++)
        w += GlyphAdvance(line[i]);
    return w;
}

static VOID PrependToLine(CHAR16 *line, const CHAR16 *prefix) {
    UINT32 pl = LineLen(prefix);
    UINT32 ol = LineLen(line);
    if (pl + ol >= MAX_CHARS_PER_LINE - 1)
        return;
    CHAR16 buf[MAX_CHARS_PER_LINE];
    CopyMem(buf, (VOID *)(UINTN)prefix, pl * sizeof(CHAR16));
    CopyMem(buf + pl, line, (ol + 1) * sizeof(CHAR16));
    CopyMem(line, buf, (pl + ol + 1) * sizeof(CHAR16));
}

static VOID ApplyWordWrap(UINT32 L) {
    if (L >= MAX_LINES - 1 || WrapPixelWidth < 48)
        return;
    CHAR16 *line = Doc.Text[L];
    UINT32 guard = 0;

    while (LinePixelWidth(line) > WrapPixelWidth && guard++ < MAX_CHARS_PER_LINE * 2) {
        UINT32 len = LineLen(line);
        if (len == 0)
            return;

        UINT32 i = 0;
        UINT32 w = 0;
        UINT32 lastSpace = (UINT32)-1;
        UINT32 overflowIdx = len;

        for (i = 0; line[i] && i < MAX_CHARS_PER_LINE; i++) {
            UINT32 adv = GlyphAdvance(line[i]);
            if (line[i] == L' ')
                lastSpace = i;
            if (w + adv > WrapPixelWidth) {
                overflowIdx = i;
                break;
            }
            w += adv;
        }

        if (overflowIdx >= len)
            break;

        UINT32 breakStart;
        if (lastSpace != (UINT32)-1 && lastSpace < overflowIdx && lastSpace > 0) {
            breakStart = lastSpace + 1;
            while (line[breakStart] == L' ')
                breakStart++;
        } else {
            w = 0;
            breakStart = 0;
            for (i = 0; line[i] && i < MAX_CHARS_PER_LINE; i++) {
                UINT32 adv = GlyphAdvance(line[i]);
                if (w + adv > WrapPixelWidth)
                    break;
                w += adv;
                breakStart = i + 1;
            }
            if (breakStart == 0 && line[0])
                breakStart = 1;
        }

        if (breakStart >= len)
            break;

        UINT32 oldCx = Doc.CursorX;
        UINT32 oldCy = Doc.CursorY;

        CHAR16 tail[MAX_CHARS_PER_LINE];
        UINT32 t = 0;
        for (i = breakStart; line[i] && t < MAX_CHARS_PER_LINE - 1; i++)
            tail[t++] = line[i];
        tail[t] = 0;
        line[breakStart] = 0;

        while (LineLen(line) > 0) {
            UINT32 u = LineLen(line);
            if (u > 0 && line[u - 1] == L' ')
                line[u - 1] = 0;
            else
                break;
        }

        if (Doc.Text[L + 1][0] == 0) {
            StrCpy(Doc.Text[L + 1], tail);
        } else {
            PrependToLine(Doc.Text[L + 1], tail);
        }

        if (Doc.LineCount < L + 2)
            Doc.LineCount = L + 2;

        if (oldCy == L && oldCx >= breakStart) {
            Doc.CursorY = L + 1;
            Doc.CursorX = oldCx - breakStart;
        }

        ApplyWordWrap(L + 1);
    }
}

static VOID ReflowAllLines(VOID) {
    for (UINT32 iter = 0; iter < 500; iter++) {
        UINT32 L;
        for (L = 0; L < Doc.LineCount && L < MAX_LINES - 1; L++) {
            if (LinePixelWidth(Doc.Text[L]) > WrapPixelWidth)
                break;
        }
        if (L >= Doc.LineCount || L >= MAX_LINES - 1)
            return;
        ApplyWordWrap(L);
    }
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

EFI_STATUS DrawRect(FRAMEBUFFER *fb, UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT32 color) {
    for (UINT32 py = y; py < y + h && py < fb->Height; py++) {
        for (UINT32 px = x; px < x + w && px < fb->Width; px++) {
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
        UINT32 dy = y + row * FontSize;
        if (dy >= fb->Height)
            break;
        for (UINT32 col = 0; col < SIMPLE_FONT_WIDTH; col++) {
            UINT32 dx = x + col * FontSize;
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
        INT32 dy = (INT32)baselineY - (INT32)(bmpTop * FontSize) + (INT32)(py * FontSize);
        if (dy < 0 || (UINT32)dy >= fb->Height)
            continue;
        UINT32 rowOff = offset + py * rowB;
        for (UINT32 byteIdx = 0; byteIdx < rowB && rowOff + byteIdx < F->bitmap_size; byteIdx++) {
            UINT8 byte = F->bitmap[rowOff + byteIdx];
            for (UINT32 bit = 0; bit < 8; bit++) {
                UINT32 px = byteIdx * 8 + bit;
                if (px >= gw)
                    break;
                UINT32 dx = x + px * FontSize;
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
    return base * FontSize;
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
        return base * FontSize;
    }
    if (CurrentFontKind < FONT_SIMPLE)
        return GlyphAdvanceBitmap(ch, &gBitmapFonts[CurrentFontKind]);
    return CHAR_GAP * FontSize;
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
    UINT32 box = ActiveFontLineHeight() * FontSize;
    UINT32 leading = box / 4 + FontSize * 4;
    return box + leading;
}

static UINT32 CursorPixelX(CHAR16 *line, UINT32 col) {
    UINT32 px = LEFT_MARGIN;
    for (UINT32 i = 0; i < col && i < MAX_CHARS_PER_LINE; i++) {
        CHAR16 c = line[i];
        if (!c)
            break;
        px += GlyphAdvance(c);
    }
    return px;
}

EFI_STATUS DrawString(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 *str, UINT32 fgColor, UINT32 bgColor) {
    if (!str) return EFI_SUCCESS;
    UINT32 posX = x;
    UINT32 lineBox = ActiveFontLineHeight() * FontSize;
    while (*str) {
        CHAR16 c = *str;
        if (CurrentFontKind == FONT_SIMPLE) {
            UINT32 gh = GlyphBitmapHeight(c);
            UINT32 yDraw = y + lineBox - gh * FontSize;
            DrawChar(fb, posX, yDraw, c, fgColor, bgColor);
        } else {
            UINT32 baselineY = y + gBitmapFonts[CurrentFontKind].max_top * FontSize;
            DrawChar(fb, posX, baselineY, c, fgColor, bgColor);
        }
        posX += GlyphAdvance(c);
        str++;
    }
    return EFI_SUCCESS;
}

EFI_STATUS ClearScreen(FRAMEBUFFER *fb, UINT32 bgColor) {
    // Use direct framebuffer for full screen clear
    for (UINT32 y = 0; y < fb->Height; y++) {
        for (UINT32 x = 0; x < fb->Width; x++) {
            DrawPixel(fb, x, y, bgColor);
        }
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

static VOID RunSplashScreen(FRAMEBUFFER *fb) {
    EFI_INPUT_KEY key;
    UINT64 elapsed;

    DrawSplashScreen(fb);
    FlipFramebuffer(fb);

    while (!EFI_ERROR(uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key)))
        ;

    elapsed = 0;
    while (elapsed < (UINT64)SPLASH_TIMEOUT_US) {
        EFI_STATUS ks = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);

        if (!EFI_ERROR(ks))
            break;
        uefi_call_wrapper(BS->Stall, 1, 40000);
        elapsed += 40000;
    }

    while (!EFI_ERROR(uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key)))
        ;
}

VOID InitDocument(VOID) {
    ZeroMem(&Doc, sizeof(Doc));
    Doc.LineCount = 1;
    Doc.CursorX = 0;
    Doc.CursorY = 0;
    Doc.Text[0][0] = 0;
    Doc.Modified = TRUE;
    ZeroMem(KeyDbgLines, sizeof(KeyDbgLines));
    MarkFullRepaint();
    DirtyLineTop = 1;
    DirtyLineBottom = 0;
    LastPaintedLineCount = 0;
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
    UINT32 row = ActiveFontLineHeight() * FontSize + FontSize * 2;
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
    UpdateWrapPixelWidthFromFb(fb);

    UINT32 bgColor = COLORS[CurrentBgColor];
    UINT32 fgColor = (CurrentBgColor == 1) ? RGB(30, 30, 30) : RGB(240, 240, 230);
    UINT32 lineStep = LineAdvance();
    UINT32 prevPainted = LastPaintedLineCount;

    /*
     * docDirty: repaint document + cursor. Otherwise only overlays (e.g. F7 log
     * refresh without re-clearing the frame).
     */
    BOOLEAN docDirty =
        RepaintFull || ShowHelp || (DirtyLineTop <= DirtyLineBottom);

    if (docDirty) {
        if (RepaintFull || ShowHelp) {
            ClearScreen(fb, bgColor);
            for (UINT32 line = 0; line < Doc.LineCount; line++) {
                UINT32 y = TOP_MARGIN + line * lineStep;
                DrawString(fb, LEFT_MARGIN, y, Doc.Text[line], fgColor, bgColor);
            }
            RepaintFull = FALSE;
        } else {
            UINT32 top = DirtyLineTop;
            UINT32 bot = DirtyLineBottom;
            if (bot >= Doc.LineCount)
                bot = (Doc.LineCount > 0) ? Doc.LineCount - 1 : 0;
            UINT32 stripeW = fb->Width;
            /* Only clear the typographic line body (cap height box), not the full
             * LineAdvance() band — rewriting the inter-line leading every keystroke
             * made visible tearing/flicker on some GOP scanouts. */
            UINT32 lineBodyH = ActiveFontLineHeight() * FontSize;
            for (UINT32 line = top; line <= bot; line++) {
                UINT32 y = TOP_MARGIN + line * lineStep;
                if (y >= fb->Height)
                    break;
                DrawRect(fb, 0, y, stripeW, lineBodyH, bgColor);
                if (line < Doc.LineCount)
                    DrawString(fb, LEFT_MARGIN, y, Doc.Text[line], fgColor, bgColor);
            }
            if (prevPainted > Doc.LineCount) {
                for (UINT32 line = Doc.LineCount; line < prevPainted; line++) {
                    UINT32 y = TOP_MARGIN + line * lineStep;
                    if (y >= fb->Height)
                        break;
                    DrawRect(fb, 0, y, stripeW, lineBodyH, bgColor);
                }
            }
        }
        if (CursorShouldDrawThisFrame() && !ShowHelp) {
            UINT32 y = TOP_MARGIN + Doc.CursorY * lineStep;
            UINT32 lineBox = ActiveFontLineHeight() * FontSize;
            CHAR16 *ln = Doc.Text[Doc.CursorY];
            UINT32 cx = CursorPixelX(ln, Doc.CursorX);
            /* Thin bar like "|": one tall-pixel wide, height = line body (not line+leading). */
            UINT32 cwBar = FontSize;
            if (cwBar < 1)
                cwBar = 1;

            if (CursorMode == CURSOR_BAR || CursorMode == CURSOR_BAR_BLINK) {
                DrawRect(fb, cx, y, cwBar, lineBox, fgColor);
            } else if (CursorMode == CURSOR_BLOCK || CursorMode == CURSOR_BLOCK_BLINK) {
                UINT32 gw;
                if (Doc.CursorX < MAX_CHARS_PER_LINE && ln[Doc.CursorX] != 0)
                    gw = GlyphAdvance(ln[Doc.CursorX]);
                else
                    gw = GlyphAdvance(L' ');
                if (gw < (UINT32)FontSize)
                    gw = FontSize;
                DrawRect(fb, cx, y, gw, lineBox, fgColor);
            }
        }
        DirtyLineTop = 1;
        DirtyLineBottom = 0;
        LastPaintedLineCount = Doc.LineCount;
    }

    if (KeyDebugMode && !ShowHelp)
        DrawKeyDebugOverlay(fb);

    if (ShowHelp) {
        UINT32 hf = RGB(28, 28, 32);
        UINT32 hb = RGB(230, 224, 210);
        UINT32 hd = RGB(72, 72, 88);
        UINT32 bx = 48;
        UINT32 by = 64;
        UINT32 bw = fb->Width - 96;
        if (bw > 760)
            bw = 760;
        UINT32 bh = 480;
        if (by + bh + 48 > fb->Height)
            bh = fb->Height - by - 64;
        DrawRect(fb, bx - 2, by - 2, bw + 4, bh + 4, hd);
        DrawRect(fb, bx, by, bw, bh, hb);
        UINT32 lx = bx + 20;
        UINT32 ly = by + 16;
        DrawString(fb, lx, ly, L"Typewrite OS - Help", hf, hb);
        ly += HELP_LINE_GAP + 10;
        DrawString(fb, lx, ly, L"F1   Toggle this help", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"F2   Cycle font (9): Virgil, Inter, Special Elite,", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"     Courier Prime, VT323, Press Start 2P,", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"     IBM Plex Mono, Share Tech Mono, Simple", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"F3   Increase font size (scale)", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"F6   Decrease font size (scale)", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"F4   Cycle background color", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly,
                  L"F5   Cycle cursor (default: blinking block)",
                  hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"F7   Toggle key debug (scan/unicode log)", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"ESC  Close help; quit app when help is hidden", hf, hb);
    }
    
    FlipFramebuffer(fb);
    return EFI_SUCCESS;
}

EFI_STATUS HandleKey(EFI_INPUT_KEY *key) {
    if (!key) return EFI_SUCCESS;
    
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
    
    if (key->ScanCode >= 0x0B && key->ScanCode <= 0x12) {  /* F1-F8 */
        switch (key->ScanCode) {
            case 0x0B:  /* F1 - Help */
                ShowHelp = !ShowHelp;
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case 0x0C:  /* F2 - Cycle font */
                CurrentFontKind = (FONT_KIND)((CurrentFontKind + 1) % FONT_NUM);
                ReflowAllLines();
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case 0x0D:  /* F3 - Increase font / scale */
                if (FontSize < 6)
                    FontSize++;
                ReflowAllLines();
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case 0x0E:  /* F4 - Cycle background */
                CurrentBgColor = (CurrentBgColor + 1) % 10;
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case 0x0F:  /* F5 - Cycle cursor style */
                CursorMode = (CursorMode + 1) % CURSOR_MODE_NUM;
                CursorBlinkAccumUs = 0;
                CursorBlinkPhase = TRUE;
                MarkDirtyRange(Doc.CursorY, Doc.CursorY);
                Doc.Modified = TRUE;
                break;
            case 0x10:  /* F6 - Decrease font / scale */
                if (FontSize > 1)
                    FontSize--;
                ReflowAllLines();
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
            case SCAN_F7:  /* F7 - Toggle key debug overlay + serial log */
                KeyDebugMode = !KeyDebugMode;
                MarkFullRepaint();
                Doc.Modified = TRUE;
                break;
        }
        return EFI_SUCCESS;
    }
    
    if (key->UnicodeChar == 0x08) {  /* Backspace */
        if (Doc.CursorX > 0) {
            Doc.CursorX--;
            Doc.Text[Doc.CursorY][Doc.CursorX] = 0;
            MarkDirtyRange(Doc.CursorY, Doc.CursorY);
            Doc.Modified = TRUE;
        }
        return EFI_SUCCESS;
    }
    
    if (key->UnicodeChar == 0x09) {  /* Tab */
        UINT32 ly = Doc.CursorY;
        for (INT32 i = 0; i < 4; i++) {
            if (Doc.CursorX < MAX_CHARS_PER_LINE - 1) {
                Doc.Text[Doc.CursorY][Doc.CursorX++] = ' ';
            }
        }
        Doc.Text[Doc.CursorY][Doc.CursorX] = 0;
        ApplyWordWrap(ly);
        MarkDirtyRange(ly, Doc.LineCount - 1);
        Doc.Modified = TRUE;
        return EFI_SUCCESS;
    }
    
    if (key->UnicodeChar == 0x0D || key->UnicodeChar == 0x0A) {  /* Enter */
        if (Doc.CursorY < MAX_LINES - 1) {
            UINT32 oldCy = Doc.CursorY;
            UINT32 oldLC = Doc.LineCount;
            Doc.CursorY++;
            Doc.CursorX = 0;
            Doc.LineCount = Doc.CursorY + 1;
            Doc.Text[Doc.CursorY][0] = 0;
            {
                UINT32 newLC = Doc.LineCount;
                UINT32 mx = oldLC > newLC ? oldLC : newLC;
                MarkDirtyRange(oldCy, mx - 1);
            }
            Doc.Modified = TRUE;
        }
        return EFI_SUCCESS;
    }
    
    /* Printable BMP (not DEL); Latin/typical layouts need code points > 127 */
    if (key->UnicodeChar >= 32 && key->UnicodeChar != 0x7F) {
        if (Doc.CursorX < MAX_CHARS_PER_LINE - 1) {
            UINT32 ly = Doc.CursorY;
            Doc.Text[Doc.CursorY][Doc.CursorX++] = key->UnicodeChar;
            Doc.Text[Doc.CursorY][Doc.CursorX] = 0;
            ApplyWordWrap(ly);
            MarkDirtyRange(ly, Doc.LineCount - 1);
            Doc.Modified = TRUE;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n========================================\r\n");
    Print(L"  Typewrite OS v1.0\r\n");
    Print(L"  UEFI Typewriter Application\r\n");
    Print(L"  F1 help  F2 font  F3/F6 scale  F5 cursor  F7 key debug\r\n");
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

    UpdateWrapPixelWidthFromFb(&fb);
    RunSplashScreen(&fb);
    CursorBlinkPhase = TRUE;
    CursorBlinkAccumUs = 0;

    InitDocument();

    while (Running) {
        if (Doc.Modified) {
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
            uefi_call_wrapper(BS->Stall, 1, stallUs);
            if (!ShowHelp && CursorWantsBlinkTimer()) {
                CursorBlinkAccumUs += stallUs;
                while (CursorBlinkAccumUs >= CURSOR_BLINK_PERIOD_US) {
                    CursorBlinkAccumUs -= CURSOR_BLINK_PERIOD_US;
                    CursorBlinkPhase = !CursorBlinkPhase;
                    MarkDirtyRange(Doc.CursorY, Doc.CursorY);
                    Doc.Modified = TRUE;
                }
            }
        }
    }
    
    Print(L"Typewriter exited. Lines: %d\n", Doc.LineCount);
    
    return EFI_SUCCESS;
}
