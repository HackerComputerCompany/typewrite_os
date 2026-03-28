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

#define MAX_LINES 100
#define MAX_CHARS_PER_LINE 120
#define LINE_HEIGHT 32
#define LEFT_MARGIN 60
#define TOP_MARGIN 80

typedef struct {
    UINT32 Width;
    UINT32 Height;
    UINT32 Pitch;
    UINT8  *PixelData;
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
static UINT32 CurrentFgColor = 0;
static UINT32 FontSize = 1;
static BOOLEAN ShowCursor = TRUE;
static DOCUMENT Doc;
static BOOLEAN Running = TRUE;

EFI_STATUS DrawPixel(FRAMEBUFFER *fb, UINT32 x, UINT32 y, UINT32 color) {
    if (x >= fb->Width || y >= fb->Height) return EFI_SUCCESS;
    UINT8 *pixel = fb->PixelData + y * fb->Pitch + x * 4;
    *(UINT32*)pixel = color | 0xFF000000;
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

EFI_STATUS DrawCharVirgil(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 ch, UINT32 fgColor, UINT32 bgColor) {
    if (ch < VIRGIL_ASC_MIN || ch > VIRGIL_ASC_MAX) return EFI_SUCCESS;
    
    UINT32 charIndex = ch - VIRGIL_ASC_MIN;
    UINT32 offset = virgil_offsets[charIndex];
    UINT32 nextOffset = (charIndex < 94) ? virgil_offsets[charIndex + 1] : sizeof(virgil_bitmap);
    UINT32 glyphBytes = nextOffset - offset;
    
    if (glyphBytes == 0 || offset >= sizeof(virgil_bitmap)) return EFI_SUCCESS;
    
    for (UINT32 py = 0; py < VIRGIL_HEIGHT && (y + py) < fb->Height; py++) {
        for (UINT32 byteIdx = 0; byteIdx < VIRGIL_ROW_BYTES && (offset + byteIdx) < sizeof(virgil_bitmap); byteIdx++) {
            UINT8 byte = virgil_bitmap[offset + byteIdx];
            for (UINT32 bit = 0; bit < 8; bit++) {
                UINT32 px = byteIdx * 8 + bit;
                if (px >= VIRGIL_MAX_WIDTH) break;
                if (byte & (1 << bit)) {
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
        for (UINT32 byteIdx = 0; byteIdx < HELVETICA_ROW_BYTES && (offset + byteIdx) < sizeof(helvetica_bitmap); byteIdx++) {
            UINT8 byte = helvetica_bitmap[offset + byteIdx];
            for (UINT32 bit = 0; bit < 8; bit++) {
                UINT32 px = byteIdx * 8 + bit;
                if (px >= HELVETICA_MAX_WIDTH) break;
                if (byte & (1 << bit)) {
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
    } else {
        return DrawCharHelvetica(fb, x, y, ch, fgColor, bgColor);
    }
}

EFI_STATUS DrawString(FRAMEBUFFER *fb, UINT32 x, UINT32 y, CHAR16 *str, UINT32 fgColor, UINT32 bgColor) {
    if (!str) return EFI_SUCCESS;
    UINT32 posX = x;
    while (*str) {
        DrawChar(fb, posX, y, *str, fgColor, bgColor);
        posX += UseVirgilFont ? (VIRGIL_MAX_WIDTH + 1) : (HELVETICA_MAX_WIDTH + 1);
        str++;
    }
    return EFI_SUCCESS;
}

EFI_STATUS ClearScreen(FRAMEBUFFER *fb, UINT32 bgColor) {
    DrawRect(fb, 0, 0, fb->Width, fb->Height, bgColor);
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
    
    for (UINT32 line = 0; line < Doc.LineCount; line++) {
        UINT32 y = TOP_MARGIN + line * LINE_HEIGHT * FontSize;
        DrawString(fb, LEFT_MARGIN, y, Doc.Text[line], fgColor, COLORS[CurrentBgColor]);
    }
    
    if (ShowCursor) {
        UINT32 cursorY = TOP_MARGIN + Doc.CursorY * LINE_HEIGHT * FontSize;
        UINT32 cursorX = LEFT_MARGIN + Doc.CursorX * (UseVirgilFont ? (VIRGIL_MAX_WIDTH + 1) : (HELVETICA_MAX_WIDTH + 1)) * FontSize;
        DrawRect(fb, cursorX, cursorY, 2, LINE_HEIGHT * FontSize, fgColor);
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
    
    FRAMEBUFFER fb;
    fb.Width = Gop->Mode->Info->HorizontalResolution;
    fb.Height = Gop->Mode->Info->VerticalResolution;
    fb.Pitch = Gop->Mode->Info->PixelsPerScanLine * 4;
    fb.PixelData = (UINT8*)(UINTN)Gop->Mode->FrameBufferBase;
    
    Print(L"Resolution: %dx%d\n", fb.Width, fb.Height);
    
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
