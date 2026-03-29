/**
 * Typewriter.c - UEFI Typewriter Application with Virgil/Helvetica Fonts
 * 
 * A native UEFI typewriter experience with:
 * - Bitmap font rendering (Virgil + Helvetica)
 * - Typewriter-style input with visual feedback
 * - File save/load from EFI variables
 * - Multiple view modes
 * 
 * Built with gnu-efi
 */

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>

#include "virgil.h"
#include "helvetica.h"

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
    FONT_SIMPLE,
    FONT_NUM
} FONT_KIND;

static FONT_KIND CurrentFontKind = FONT_VIRGIL;
static BOOLEAN ShowHelp = FALSE;

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
static BOOLEAN ShowCursor = TRUE;
static DOCUMENT Doc;
static BOOLEAN Running = TRUE;
static UINT32 WrapPixelWidth = 800;

#define CHAR_GAP 2
/* Word space advance multiplier (advance for ' ' is this × normal font advance). */
#define SPACE_ADVANCE_MULT 3

#define SCELL(_fb, _x, _y, _c) DrawRect((_fb), (_x), (_y), FontSize, FontSize, (_c))

static UINT32 GlyphAdvance(CHAR16 ch);

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
    if (L >= MAX_LINES - 1 || WrapPixelWidth < 80)
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

static VOID FlipFramebuffer(FRAMEBUFFER *fb) {
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

EFI_STATUS DrawCharVirgil(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 ch, UINT32 fgColor, UINT32 bgColor) {
    if (ch < VIRGIL_ASC_MIN || ch > VIRGIL_ASC_MAX) return EFI_SUCCESS;
    
    UINT32 charIndex = ch - VIRGIL_ASC_MIN;
    UINT32 offset = virgil_offsets[charIndex];
    UINT32 nextOffset = (charIndex < 94) ? virgil_offsets[charIndex + 1] : sizeof(virgil_bitmap);
    UINT32 glyphBytes = nextOffset - offset;
    
    if (glyphBytes == 0 || offset + glyphBytes > sizeof(virgil_bitmap)) return EFI_SUCCESS;

    /*
     * Per-glyph layout (see fonts/convert_font.py): each row has
     * (glyphWidth+7)/8 bytes, not VIRGIL_ROW_BYTES. Fixed stride was reading
     * past the glyph into the next character — garbled bottoms (e.g. \"T\").
     */
    UINT32 gw = virgil_widths[charIndex];
    if (gw < 1)
        gw = 1;
    UINT32 rowB = (gw + 7) / 8;
    if (rowB < 1)
        rowB = 1;
    UINT32 gh = glyphBytes / rowB;
    if (gh < 1)
        gh = 1;
    if (gh > VIRGIL_HEIGHT)
        gh = VIRGIL_HEIGHT;

    for (UINT32 py = 0; py < gh; py++) {
        UINT32 dy = y + py * FontSize;
        if (dy >= fb->Height)
            break;
        UINT32 rowOff = offset + py * rowB;
        for (UINT32 byteIdx = 0; byteIdx < rowB && rowOff + byteIdx < sizeof(virgil_bitmap); byteIdx++) {
            UINT8 byte = virgil_bitmap[rowOff + byteIdx];
            for (UINT32 bit = 0; bit < 8; bit++) {
                UINT32 px = byteIdx * 8 + bit;
                if (px >= gw)
                    break;
                UINT32 dx = x + px * FontSize;
                if (dx >= fb->Width)
                    continue;
                if (byte & (1u << bit)) {
                    SCELL(fb, dx, dy, fgColor);
                } else {
                    SCELL(fb, dx, dy, bgColor);
                }
            }
        }
    }
    return EFI_SUCCESS;
}

EFI_STATUS DrawCharHelvetica(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 ch, UINT32 fgColor, UINT32 bgColor) {
    if (ch < HELVETICA_ASC_MIN || ch > HELVETICA_ASC_MAX) return EFI_SUCCESS;
    
    UINT32 charIndex = ch - HELVETICA_ASC_MIN;
    UINT32 offset = helvetica_offsets[charIndex];
    UINT32 nextOffset = (charIndex < 94) ? helvetica_offsets[charIndex + 1] : sizeof(helvetica_bitmap);
    UINT32 glyphBytes = nextOffset - offset;
    
    if (glyphBytes == 0 || offset + glyphBytes > sizeof(helvetica_bitmap)) return EFI_SUCCESS;

    UINT32 gw = helvetica_widths[charIndex];
    if (gw < 1)
        gw = 1;
    UINT32 rowB = (gw + 7) / 8;
    if (rowB < 1)
        rowB = 1;
    UINT32 gh = glyphBytes / rowB;
    if (gh < 1)
        gh = 1;
    if (gh > HELVETICA_HEIGHT)
        gh = HELVETICA_HEIGHT;

    for (UINT32 py = 0; py < gh; py++) {
        UINT32 dy = y + py * FontSize;
        if (dy >= fb->Height)
            break;
        UINT32 rowOff = offset + py * rowB;
        for (UINT32 byteIdx = 0; byteIdx < rowB && rowOff + byteIdx < sizeof(helvetica_bitmap); byteIdx++) {
            UINT8 byte = helvetica_bitmap[rowOff + byteIdx];
            for (UINT32 bit = 0; bit < 8; bit++) {
                UINT32 px = byteIdx * 8 + bit;
                if (px >= gw)
                    break;
                UINT32 dx = x + px * FontSize;
                if (dx >= fb->Width)
                    continue;
                if (byte & (1u << bit)) {
                    SCELL(fb, dx, dy, fgColor);
                } else {
                    SCELL(fb, dx, dy, bgColor);
                }
            }
        }
    }
    return EFI_SUCCESS;
}

EFI_STATUS DrawChar(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 ch, UINT32 fgColor, UINT32 bgColor) {
    switch (CurrentFontKind) {
    case FONT_VIRGIL:
        return DrawCharVirgil(fb, x, y, ch, fgColor, bgColor);
    case FONT_HELVETICA:
        return DrawCharHelvetica(fb, x, y, ch, fgColor, bgColor);
    case FONT_SIMPLE:
        return DrawCharSimple(fb, x, y, ch, fgColor, bgColor);
    default:
        return EFI_SUCCESS;
    }
}

static UINT32 GlyphAdvance(CHAR16 ch) {
    UINT32 base;
    switch (CurrentFontKind) {
    case FONT_SIMPLE:
        if (ch >= 32 && ch <= 126)
            base = SIMPLE_FONT_WIDTH + 1;
        else
            base = CHAR_GAP;
        break;
    case FONT_VIRGIL:
        if (ch < VIRGIL_ASC_MIN || ch > VIRGIL_ASC_MAX)
            base = CHAR_GAP;
        else {
            UINT32 idx = ch - VIRGIL_ASC_MIN;
            UINT32 w = virgil_widths[idx];
            if (w < 1)
                w = 1;
            if (virgil_advances[idx] > 0)
                base = (UINT32)virgil_advances[idx];
            else
                base = w + CHAR_GAP;
        }
        break;
    case FONT_HELVETICA:
    default:
        if (ch < HELVETICA_ASC_MIN || ch > HELVETICA_ASC_MAX)
            base = CHAR_GAP;
        else {
            UINT32 idx2 = ch - HELVETICA_ASC_MIN;
            UINT32 w2 = helvetica_widths[idx2];
            if (w2 < 1)
                w2 = 1;
            if (helvetica_advances[idx2] > 0)
                base = (UINT32)helvetica_advances[idx2];
            else
                base = w2 + CHAR_GAP;
        }
        break;
    }
    if (ch == L' ')
        base *= SPACE_ADVANCE_MULT;
    return base * FontSize;
}

static UINT32 ActiveFontLineHeight(VOID) {
    switch (CurrentFontKind) {
    case FONT_SIMPLE:
        return SIMPLE_FONT_HEIGHT;
    case FONT_VIRGIL:
        return VIRGIL_HEIGHT;
    case FONT_HELVETICA:
    default:
        return HELVETICA_HEIGHT;
    }
}

static UINT32 GlyphBitmapHeight(CHAR16 ch) {
    switch (CurrentFontKind) {
    case FONT_SIMPLE:
        if (ch >= 32 && ch <= 126)
            return SIMPLE_FONT_HEIGHT;
        return ActiveFontLineHeight();
    case FONT_VIRGIL:
        if (ch < VIRGIL_ASC_MIN || ch > VIRGIL_ASC_MAX)
            return ActiveFontLineHeight();
        {
            UINT32 charIndex = ch - VIRGIL_ASC_MIN;
            UINT32 offset = virgil_offsets[charIndex];
            UINT32 nextOffset = (charIndex < 94) ? virgil_offsets[charIndex + 1] : sizeof(virgil_bitmap);
            UINT32 glyphBytes = nextOffset - offset;
            if (glyphBytes == 0 || offset + glyphBytes > sizeof(virgil_bitmap))
                return ActiveFontLineHeight();
            UINT32 gw = virgil_widths[charIndex];
            if (gw < 1)
                gw = 1;
            UINT32 rowB = (gw + 7) / 8;
            if (rowB < 1)
                rowB = 1;
            UINT32 gh = glyphBytes / rowB;
            if (gh < 1)
                gh = 1;
            if (gh > VIRGIL_HEIGHT)
                gh = VIRGIL_HEIGHT;
            return gh;
        }
    case FONT_HELVETICA:
    default:
        if (ch < HELVETICA_ASC_MIN || ch > HELVETICA_ASC_MAX)
            return ActiveFontLineHeight();
        {
            UINT32 charIndex = ch - HELVETICA_ASC_MIN;
            UINT32 offset = helvetica_offsets[charIndex];
            UINT32 nextOffset = (charIndex < 94) ? helvetica_offsets[charIndex + 1] : sizeof(helvetica_bitmap);
            UINT32 glyphBytes = nextOffset - offset;
            if (glyphBytes == 0 || offset + glyphBytes > sizeof(helvetica_bitmap))
                return ActiveFontLineHeight();
            UINT32 gw = helvetica_widths[charIndex];
            if (gw < 1)
                gw = 1;
            UINT32 rowB = (gw + 7) / 8;
            if (rowB < 1)
                rowB = 1;
            UINT32 gh = glyphBytes / rowB;
            if (gh < 1)
                gh = 1;
            if (gh > HELVETICA_HEIGHT)
                gh = HELVETICA_HEIGHT;
            return gh;
        }
    }
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
        UINT32 gh = GlyphBitmapHeight(c);
        UINT32 yDraw = y + lineBox - gh * FontSize;
        DrawChar(fb, posX, yDraw, c, fgColor, bgColor);
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

VOID InitDocument(VOID) {
    ZeroMem(&Doc, sizeof(Doc));
    Doc.LineCount = 1;
    Doc.CursorX = 0;
    Doc.CursorY = 0;
    Doc.Text[0][0] = 0;
    Doc.Modified = TRUE;
}

EFI_STATUS RenderDocument(FRAMEBUFFER *fb) {
    if (fb->Width > LEFT_MARGIN + RIGHT_MARGIN + 80)
        WrapPixelWidth = fb->Width - LEFT_MARGIN - RIGHT_MARGIN;
    ClearScreen(fb, COLORS[CurrentBgColor]);
    
    UINT32 fgColor = (CurrentBgColor == 1) ? RGB(30, 30, 30) : RGB(240, 240, 230);
    
    UINT32 lineStep = LineAdvance();
    for (UINT32 line = 0; line < Doc.LineCount; line++) {
        UINT32 y = TOP_MARGIN + line * lineStep;
        DrawString(fb, LEFT_MARGIN, y, Doc.Text[line], fgColor, COLORS[CurrentBgColor]);
    }
    
    if (ShowCursor) {
        UINT32 cursorY = TOP_MARGIN + Doc.CursorY * lineStep;
        UINT32 cursorX = CursorPixelX(Doc.Text[Doc.CursorY], Doc.CursorX);
        UINT32 cw = FontSize * 2;
        if (cw < 2)
            cw = 2;
        DrawRect(fb, cursorX, cursorY, cw, lineStep, fgColor);
    }
    
    if (ShowHelp) {
        UINT32 hf = RGB(28, 28, 32);
        UINT32 hb = RGB(230, 224, 210);
        UINT32 hd = RGB(72, 72, 88);
        UINT32 bx = 48;
        UINT32 by = 64;
        UINT32 bw = fb->Width - 96;
        if (bw > 760)
            bw = 760;
        UINT32 bh = 400;
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
        DrawString(fb, lx, ly, L"F2   Cycle font: Virgil -> Helvetica -> Simple", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"F3   Increase font size (scale)", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"F6   Decrease font size (scale)", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"F4   Cycle background color", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"F5   Toggle cursor", hf, hb);
        ly += HELP_LINE_GAP;
        DrawString(fb, lx, ly, L"ESC  Close help; quit app when help is hidden", hf, hb);
    }
    
    FlipFramebuffer(fb);
    return EFI_SUCCESS;
}

EFI_STATUS HandleKey(EFI_INPUT_KEY *key) {
    if (!key) return EFI_SUCCESS;
    
    if (key->ScanCode == 0x01) {  /* ESC */
        if (ShowHelp) {
            ShowHelp = FALSE;
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
                Doc.Modified = TRUE;
                break;
            case 0x0C:  /* F2 - Cycle font */
                CurrentFontKind = (FONT_KIND)((CurrentFontKind + 1) % FONT_NUM);
                ReflowAllLines();
                Doc.Modified = TRUE;
                break;
            case 0x0D:  /* F3 - Increase font / scale */
                if (FontSize < 6)
                    FontSize++;
                ReflowAllLines();
                Doc.Modified = TRUE;
                break;
            case 0x0E:  /* F4 - Cycle background */
                CurrentBgColor = (CurrentBgColor + 1) % 10;
                Doc.Modified = TRUE;
                break;
            case 0x0F:  /* F5 - Toggle cursor */
                ShowCursor = !ShowCursor;
                Doc.Modified = TRUE;
                break;
            case 0x10:  /* F6 - Decrease font / scale */
                if (FontSize > 1)
                    FontSize--;
                ReflowAllLines();
                Doc.Modified = TRUE;
                break;
        }
        return EFI_SUCCESS;
    }
    
    if (key->UnicodeChar == 0x08) {  /* Backspace */
        if (Doc.CursorX > 0) {
            Doc.CursorX--;
            Doc.Text[Doc.CursorY][Doc.CursorX] = 0;
            Doc.Modified = TRUE;
        }
        return EFI_SUCCESS;
    }
    
    if (key->UnicodeChar == 0x09) {  /* Tab */
        for (INT32 i = 0; i < 4; i++) {
            if (Doc.CursorX < MAX_CHARS_PER_LINE - 1) {
                Doc.Text[Doc.CursorY][Doc.CursorX++] = ' ';
            }
        }
        Doc.Text[Doc.CursorY][Doc.CursorX] = 0;
        ApplyWordWrap(Doc.CursorY);
        Doc.Modified = TRUE;
        return EFI_SUCCESS;
    }
    
    if (key->UnicodeChar == 0x0D || key->UnicodeChar == 0x0A) {  /* Enter */
        if (Doc.CursorY < MAX_LINES - 1) {
            Doc.CursorY++;
            Doc.CursorX = 0;
            Doc.LineCount = Doc.CursorY + 1;
            Doc.Text[Doc.CursorY][0] = 0;
            Doc.Modified = TRUE;
        }
        return EFI_SUCCESS;
    }
    
    if (key->UnicodeChar >= 32 && key->UnicodeChar < 127) {
        if (Doc.CursorX < MAX_CHARS_PER_LINE - 1) {
            Doc.Text[Doc.CursorY][Doc.CursorX++] = key->UnicodeChar;
            Doc.Text[Doc.CursorY][Doc.CursorX] = 0;
            ApplyWordWrap(Doc.CursorY);
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
    Print(L"  F1 help  F2 font  F3/F6 scale\r\n");
    Print(L"========================================\r\n\r\n");
    
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS status = LibLocateProtocol(&gopGuid, (VOID**)&Gop);
    if (EFI_ERROR(status)) {
        Print(L"Error: GOP not found (status=%r)\n", status);
        return status;
    }
    
    // Set the video mode to ensure it's initialized
    status = uefi_call_wrapper(Gop->SetMode, 1, Gop, Gop->Mode->Mode);
    if (EFI_ERROR(status)) {
        Print(L"Warning: SetMode failed (status=%r)\n", status);
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
    FbFront = (UINT8*)(UINTN)Gop->Mode->FrameBufferBase;
    fb.PixelData = FbFront;
    /*
     * Off-screen pool + CopyMem worked on some hosts but leaves a black display on
     * other OVMF/QEMU paths (scanout vs CPU writes). Draw directly to GOP FB; use
     * Doc.Modified coalescing to limit full clears. Revisit with GOP Blt if needed.
     */
    FbCopyBytes = 0;
    
    // Detect pixel format from GOP
    fb.PixelFormat = Gop->Mode->Info->PixelFormat;
    if (fb.PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        fb.RedMask = 0xFF;  // BGR format
    } else if (fb.PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        fb.RedMask = 0xFF0000;  // RGB format
    } else if (fb.PixelFormat == PixelBitMask) {
        fb.RedMask = Gop->Mode->Info->PixelInformation.RedMask;
    } else {
        // Default to BGR
        fb.RedMask = 0xFF;
    }
    
    InitDocument();
    
    while (Running) {
        if (Doc.Modified) {
            RenderDocument(&fb);
            Doc.Modified = FALSE;
        }
        
        EFI_INPUT_KEY key;
        EFI_STATUS keyStatus = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
        
        if (!EFI_ERROR(keyStatus)) {
            HandleKey(&key);
        }
        
        /* Idle longer when unchanged to avoid burning CPU; short delay after input */
        uefi_call_wrapper(BS->Stall, 1, EFI_ERROR(keyStatus) ? 50000 : 5000);
    }
    
    Print(L"Typewriter exited. Lines: %d\n", Doc.LineCount);
    
    return EFI_SUCCESS;
}
