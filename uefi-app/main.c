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
#define LINE_HEIGHT 12
#define LEFT_MARGIN 20
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
static BOOLEAN UseVirgilFont = TRUE;

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

EFI_STATUS DrawPixel(FRAMEBUFFER *fb, UINT32 x, UINT32 y, UINT32 color) {
    if (x >= fb->Width || y >= fb->Height) return EFI_SUCCESS;
    UINT8 *pixel = fb->PixelData + y * fb->Pitch + x * 4;
    
    // Extract RGB from our color format (RRGGBB)
    UINT8 r = (color >> 0) & 0xFF;
    UINT8 g = (color >> 8) & 0xFF;
    UINT8 b = (color >> 16) & 0xFF;
    
    // Pixel format 1 = BlueGreenRedReserved8BitPerColor (BGR)
    pixel[0] = b;
    pixel[1] = g;
    pixel[2] = r;
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
    for (UINT32 row = 0; row < 8; row++) {
        for (UINT32 col = 0; col < 5; col++) {
            UINT8 colBits = simple_font_data[offset + col];
            if (colBits & (1u << row)) {
                DrawPixel(fb, x + col, y + row, fgColor);
            } else {
                DrawPixel(fb, x + col, y + row, bgColor);
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
    
    if (glyphBytes == 0 || offset >= sizeof(virgil_bitmap)) return EFI_SUCCESS;
    
    /* Row-major bitmap: each row is VIRGIL_ROW_BYTES bytes (see virgil.h) */
    for (UINT32 py = 0; py < VIRGIL_HEIGHT && (y + py) < fb->Height; py++) {
        UINT32 rowOff = offset + py * VIRGIL_ROW_BYTES;
        for (UINT32 byteIdx = 0; byteIdx < VIRGIL_ROW_BYTES && (rowOff + byteIdx) < sizeof(virgil_bitmap); byteIdx++) {
            UINT8 byte = virgil_bitmap[rowOff + byteIdx];
            for (UINT32 bit = 0; bit < 8; bit++) {
                UINT32 px = byteIdx * 8 + bit;
                if (px >= VIRGIL_MAX_WIDTH) break;
                if (byte & (1u << bit)) {
                    DrawPixel(fb, x + px, y + py, fgColor);
                } else {
                    DrawPixel(fb, x + px, y + py, bgColor);
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
    
    if (glyphBytes == 0 || offset >= sizeof(helvetica_bitmap)) return EFI_SUCCESS;
    
    for (UINT32 py = 0; py < HELVETICA_HEIGHT && (y + py) < fb->Height; py++) {
        UINT32 rowOff = offset + py * HELVETICA_ROW_BYTES;
        for (UINT32 byteIdx = 0; byteIdx < HELVETICA_ROW_BYTES && (rowOff + byteIdx) < sizeof(helvetica_bitmap); byteIdx++) {
            UINT8 byte = helvetica_bitmap[rowOff + byteIdx];
            for (UINT32 bit = 0; bit < 8; bit++) {
                UINT32 px = byteIdx * 8 + bit;
                if (px >= HELVETICA_MAX_WIDTH) break;
                if (byte & (1u << bit)) {
                    DrawPixel(fb, x + px, y + py, fgColor);
                } else {
                    DrawPixel(fb, x + px, y + py, bgColor);
                }
            }
        }
    }
    return EFI_SUCCESS;
}

EFI_STATUS DrawChar(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 ch, UINT32 fgColor, UINT32 bgColor) {
    if (UseVirgilFont) {
        return DrawCharVirgil(fb, x, y, ch, fgColor, bgColor);
    }
    return DrawCharHelvetica(fb, x, y, ch, fgColor, bgColor);
}

/* Horizontal advance per glyph column (fixed cell; bitmaps are <= max width) */
static UINT32 CharCellWidth(VOID) {
    if (UseVirgilFont) {
        return VIRGIL_MAX_WIDTH + 2;
    }
    return HELVETICA_MAX_WIDTH + 2;
}

static UINT32 LineAdvance(VOID) {
    UINT32 fh = (UseVirgilFont ? VIRGIL_HEIGHT : HELVETICA_HEIGHT) * FontSize;
    UINT32 lh = LINE_HEIGHT * FontSize;
    return (fh > lh) ? fh : lh;
}

EFI_STATUS DrawString(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 *str, UINT32 fgColor, UINT32 bgColor) {
    if (!str) return EFI_SUCCESS;
    UINT32 posX = x;
    UINT32 step = CharCellWidth();
    while (*str) {
        DrawChar(fb, posX, y, *str, fgColor, bgColor);
        posX += step;
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
    Doc.Modified = FALSE;
}

EFI_STATUS RenderDocument(FRAMEBUFFER *fb) {
    ClearScreen(fb, COLORS[CurrentBgColor]);
    
    UINT32 fgColor = (CurrentBgColor == 1) ? RGB(30, 30, 30) : RGB(240, 240, 230);
    
    UINT32 lineStep = LineAdvance();
    for (UINT32 line = 0; line < Doc.LineCount; line++) {
        UINT32 y = TOP_MARGIN + line * lineStep;
        DrawString(fb, LEFT_MARGIN, y, Doc.Text[line], fgColor, COLORS[CurrentBgColor]);
    }
    
    if (ShowCursor) {
        UINT32 cursorY = TOP_MARGIN + Doc.CursorY * lineStep;
        UINT32 cursorX = LEFT_MARGIN + Doc.CursorX * CharCellWidth() * FontSize;
        DrawRect(fb, cursorX, cursorY, 2, lineStep, fgColor);
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS HandleKey(EFI_INPUT_KEY *key) {
    if (!key) return EFI_SUCCESS;
    
    if (key->ScanCode == 0x01) {  /* ESC */
        Running = FALSE;
        return EFI_SUCCESS;
    }
    
    if (key->ScanCode >= 0x0B && key->ScanCode <= 0x12) {  /* F1-F8 */
        switch (key->ScanCode) {
            case 0x0B:  /* F1 - Help */
                break;
            case 0x0C:  /* F2 - Decrease font */
                if (FontSize > 1) FontSize--;
                break;
            case 0x0D:  /* F3 - Increase font */
                if (FontSize < 3) FontSize++;
                break;
            case 0x0E:  /* F4 - Cycle background */
                CurrentBgColor = (CurrentBgColor + 1) % 10;
                break;
            case 0x0F:  /* F5 - Toggle cursor */
                ShowCursor = !ShowCursor;
                break;
            case 0x10:  /* F6 - Switch font */
                UseVirgilFont = !UseVirgilFont;
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
    Print(L"  Virgil & Helvetica Fonts\r\n");
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
    fb.Pitch = Gop->Mode->Info->PixelsPerScanLine * 4;
    fb.PixelData = (UINT8*)(UINTN)Gop->Mode->FrameBufferBase;
    
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
        RenderDocument(&fb);
        
        EFI_INPUT_KEY key;
        EFI_STATUS keyStatus = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
        
        if (!EFI_ERROR(keyStatus)) {
            HandleKey(&key);
        }
        
        uefi_call_wrapper(BS->Stall, 1, 10000);
    }
    
    Print(L"Typewriter exited. Lines: %d\n", Doc.LineCount);
    
    return EFI_SUCCESS;
}
