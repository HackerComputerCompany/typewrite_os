/**
 * UefiVi — UEFI text-console vi-like file editor (no GOP).
 *
 * Boot volume file I/O via LoadedImage->DeviceHandle + LibOpenRoot, same idea as uefi-app/.
 */

#include <efi.h>
#include <efilib.h>
#include <eficon.h>
#include <efiprot.h>

#define MAX_LINES   128
#define MAX_COL     240
#define FILE_CAP    (256 * 1024)
#define COLON_MAX   32
#define MAX_BROWSER_ENTRIES 96
#define MAX_BROWSER_NAME    72
#define BROWSER_READ_CHUNK  4096

typedef enum {
    MODE_NORMAL = 0,
    MODE_INSERT,
    MODE_COLON,
    MODE_BROWSER
} EDITOR_MODE;

static CHAR16 Lines[MAX_LINES][MAX_COL + 1];
static UINT32 NumLines = 1;
static UINT32 CursorRow;
static UINT32 CursorCol;
static UINT32 ScrollRow;
static EDITOR_MODE Mode = MODE_NORMAL;
static BOOLEAN Dirty = FALSE;
static BOOLEAN Running = TRUE;

static CHAR16 FilePath[128];

static CHAR16 BrowserCwd[120];
static CHAR16 BrowserNames[MAX_BROWSER_ENTRIES][MAX_BROWSER_NAME];
static BOOLEAN BrowserIsDir[MAX_BROWSER_ENTRIES];
static UINT32 BrowserCount;
static UINT32 BrowserSel;
static UINT32 BrowserScroll;
static CHAR16 ColonBuf[COLON_MAX];
static UINT32 ColonLen;

static UINT32 TermCols = 80;
static UINT32 TermRows = 25;
static EFI_HANDLE GImage;

static EFI_STATUS LoadFile(EFI_HANDLE img);

static VOID line_clear(UINT32 row) {
    UINT32 c;

    for (c = 0; c < MAX_COL; c++)
        Lines[row][c] = L' ';
    Lines[row][MAX_COL] = 0;
}

static VOID EditorInitBuffer(VOID) {
    UINT32 r;

    NumLines = 1;
    CursorRow = 0;
    CursorCol = 0;
    ScrollRow = 0;
    Dirty = FALSE;
    for (r = 0; r < MAX_LINES; r++)
        line_clear(r);
}

static UINT32 LineLastNonSpace(UINT32 row) {
    INT32 c;

    for (c = MAX_COL - 1; c >= 0; c--) {
        if (Lines[row][c] != L' ')
            return (UINT32)c;
    }
    return (UINT32)-1; /* empty */
}

static VOID TrimTrailingSpace(UINT32 row) {
    UINT32 last = LineLastNonSpace(row);
    UINT32 c;

    if (last == (UINT32)-1) {
        for (c = 0; c < MAX_COL; c++)
            Lines[row][c] = L' ';
        Lines[row][MAX_COL] = 0;
        return;
    }
    for (c = last + 1; c < MAX_COL; c++)
        Lines[row][c] = L' ';
    Lines[row][MAX_COL] = 0;
}

static VOID RefreshTermSize(VOID) {
    UINTN c = 0, r = 0;
    UINT32 m;

    if (ST == NULL || ST->ConOut == NULL)
        return;
    m = ST->ConOut->Mode->Mode;
    if (!EFI_ERROR(ST->ConOut->QueryMode(ST->ConOut, m, &c, &r))) {
        if (c > 0 && c < 256)
            TermCols = (UINT32)c;
        if (r > 0 && r < 256)
            TermRows = (UINT32)r;
    }
    if (TermCols < 40)
        TermCols = 80;
    if (TermRows < 10)
        TermRows = 25;
}

static VOID EnsureScroll(VOID) {
    UINT32 viewH = (TermRows > 3) ? TermRows - 2 : TermRows - 1;

    if (viewH < 1)
        viewH = 1;
    if (CursorRow < ScrollRow)
        ScrollRow = CursorRow;
    if (CursorRow >= ScrollRow + viewH)
        ScrollRow = CursorRow - viewH + 1;
}

static VOID DrawScreen(VOID) {
    UINT32 viewH = (TermRows > 3) ? TermRows - 2 : TermRows - 1;
    UINT32 cw = (TermCols > 1) ? TermCols - 1 : 80;
    UINT32 r, i;
    CHAR16 rowbuf[260];
    CHAR16 sts[160];

    RefreshTermSize();
    EnsureScroll();
    if (viewH < 1)
        viewH = 1;

    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);

    for (i = 0; i < viewH; i++) {
        r = ScrollRow + i;
        if (r < NumLines) {
            UINT32 c;
            UINT32 copy = cw;

            if (copy > MAX_COL)
                copy = MAX_COL;
            for (c = 0; c < copy; c++)
                rowbuf[c] = Lines[r][c];
            rowbuf[copy] = 0;
        } else {
            rowbuf[0] = L'~';
            rowbuf[1] = 0;
        }
        Print(L"%s\r\n", rowbuf);
    }

    if (Mode == MODE_COLON) {
        UnicodeSPrint(sts, sizeof(sts), L":%s", ColonBuf);
        Print(L"%s\r\n", sts);
    } else {
        UnicodeSPrint(sts, sizeof(sts),
                      L"-- %s -- %s %s  [hjkl i a x o F2 :e :  ESC]",
                      (Mode == MODE_INSERT) ? L"INSERT" : L"NORMAL",
                      Dirty ? L"[+]" : L"",
                      FilePath);
        Print(L"%s\r\n", sts);
    }
}

static BOOLEAN TokenLooksLikeEfiBinary(const CHAR16 *t) {
    UINTN n = StrLen(t);

    if (n < 4)
        return FALSE;
    return (t[n - 4] == L'.' &&
            (t[n - 3] == L'e' || t[n - 3] == L'E') &&
            (t[n - 2] == L'f' || t[n - 2] == L'F') &&
            (t[n - 1] == L'i' || t[n - 1] == L'I'));
}

static VOID ParseLoadOptionsFilename(EFI_HANDLE img, CHAR16 *out, UINTN outChars) {
    EFI_GUID g = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    CHAR16 *s;
    UINTN nch, i, j;

    StrCpy(out, L"EDIT.TXT");
    if (EFI_ERROR(uefi_call_wrapper(BS->HandleProtocol, 3, img, &g, (VOID **)&li)) || li == NULL)
        return;
    if (li->LoadOptions == NULL || li->LoadOptionsSize < sizeof(CHAR16))
        return;

    s = (CHAR16 *)li->LoadOptions;
    nch = li->LoadOptionsSize / sizeof(CHAR16);

    i = 0;
    while (i < nch && (s[i] == L' ' || s[i] == L'\t'))
        i++;

    j = 0;
    while (i < nch && s[i] && s[i] != L' ' && s[i] != L'\t' && s[i] != L'\r' && s[i] != L'\n' &&
           j + 1 < outChars)
        out[j++] = s[i++];
    out[j] = 0;

    if (j == 0 || TokenLooksLikeEfiBinary(out))
        StrCpy(out, L"EDIT.TXT");
}

static VOID BrowserGoParent(VOID) {
    INTN i;
    UINTN n = StrLen(BrowserCwd);

    if (n == 0)
        return;
    for (i = (INTN)n - 1; i >= 0; i--) {
        if (BrowserCwd[i] == L'\\' || BrowserCwd[i] == L'/') {
            BrowserCwd[i] = 0;
            return;
        }
    }
    BrowserCwd[0] = 0;
}

static BOOLEAN BrowserEntryOrderOk(UINT32 i, UINT32 j) {
    /* true if entry i should appear before j (dirs first, then name). */
    if (BrowserIsDir[i] && !BrowserIsDir[j])
        return TRUE;
    if (!BrowserIsDir[i] && BrowserIsDir[j])
        return FALSE;
    return StrCmp(BrowserNames[i], BrowserNames[j]) <= 0;
}

static VOID BrowserSwapEntries(UINT32 i, UINT32 j) {
    CHAR16 tn[MAX_BROWSER_NAME];
    BOOLEAN tb;

    CopyMem(tn, BrowserNames[i], sizeof(tn));
    tb = BrowserIsDir[i];
    CopyMem(BrowserNames[i], BrowserNames[j], sizeof(BrowserNames[i]));
    BrowserIsDir[i] = BrowserIsDir[j];
    CopyMem(BrowserNames[j], tn, sizeof(BrowserNames[j]));
    BrowserIsDir[j] = tb;
}

static VOID BrowserSortEntries(VOID) {
    UINT32 i, j, k;

    for (i = 0; i < BrowserCount; i++) {
        j = i;
        for (k = i + 1; k < BrowserCount; k++) {
            if (!BrowserEntryOrderOk(j, k))
                j = k;
        }
        if (j != i)
            BrowserSwapEntries(i, j);
    }
}

static VOID BrowserEnsureScroll(UINT32 viewH) {
    if (viewH < 1)
        viewH = 1;
    if (BrowserSel < BrowserScroll)
        BrowserScroll = BrowserSel;
    if (BrowserCount > 0 && BrowserSel >= BrowserScroll + viewH)
        BrowserScroll = BrowserSel - viewH + 1;
}

static EFI_STATUS BrowserRefresh(VOID) {
    EFI_GUID g = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    EFI_FILE *root = NULL;
    EFI_FILE *dir = NULL;
    EFI_STATUS st;
    UINT8 *InfoBuf = NULL;
    UINTN ReadSize;

    BrowserCount = 0;
    st = uefi_call_wrapper(BS->HandleProtocol, 3, GImage, &g, (VOID **)&li);
    if (EFI_ERROR(st) || li == NULL || li->DeviceHandle == NULL)
        return EFI_NOT_READY;

    root = LibOpenRoot(li->DeviceHandle);
    if (root == NULL)
        return EFI_NO_MEDIA;

    if (BrowserCwd[0] == 0) {
        dir = root;
    } else {
        st = uefi_call_wrapper(root->Open, 5, root, &dir, BrowserCwd, EFI_FILE_MODE_READ, (UINT64)0);
        uefi_call_wrapper(root->Close, 1, root);
        root = NULL;
        if (EFI_ERROR(st))
            return st;
    }

    st = uefi_call_wrapper(dir->SetPosition, 2, dir, 0);
    if (EFI_ERROR(st)) {
        uefi_call_wrapper(dir->Close, 1, dir);
        return st;
    }

    InfoBuf = AllocatePool(BROWSER_READ_CHUNK);
    if (InfoBuf == NULL) {
        uefi_call_wrapper(dir->Close, 1, dir);
        return EFI_OUT_OF_RESOURCES;
    }

    for (;;) {
        ReadSize = BROWSER_READ_CHUNK;
        st = uefi_call_wrapper(dir->Read, 3, dir, &ReadSize, InfoBuf);
        if (EFI_ERROR(st))
            break;
        if (ReadSize == 0)
            break;

        if (ReadSize >= SIZE_OF_EFI_FILE_INFO + sizeof(CHAR16)) {
            EFI_FILE_INFO *info = (EFI_FILE_INFO *)InfoBuf;
            UINTN nl;

            if (StrCmp(info->FileName, L".") == 0 || StrCmp(info->FileName, L"..") == 0)
                continue;
            if (BrowserCount >= MAX_BROWSER_ENTRIES)
                break;

            nl = StrLen(info->FileName);
            if (nl >= MAX_BROWSER_NAME)
                nl = MAX_BROWSER_NAME - 1;
            CopyMem(BrowserNames[BrowserCount], info->FileName, nl * sizeof(CHAR16));
            BrowserNames[BrowserCount][nl] = 0;
            BrowserIsDir[BrowserCount] = (info->Attribute & EFI_FILE_DIRECTORY) != 0;
            BrowserCount++;
        }
    }

    FreePool(InfoBuf);
    uefi_call_wrapper(dir->Close, 1, dir);

    BrowserSortEntries();
    if (BrowserCount > 0 && BrowserSel >= BrowserCount)
        BrowserSel = BrowserCount - 1;
    if (BrowserCount == 0)
        BrowserSel = 0;
    BrowserScroll = 0;
    RefreshTermSize();
    BrowserEnsureScroll((TermRows > 4) ? TermRows - 4 : TermRows - 2);
    return EFI_SUCCESS;
}

static VOID BrowserEnterSubdir(const CHAR16 *name) {
    if (BrowserCwd[0] == 0)
        UnicodeSPrint(BrowserCwd, sizeof(BrowserCwd), L"%s", name);
    else
        UnicodeSPrint(BrowserCwd, sizeof(BrowserCwd), L"%s\\%s", BrowserCwd, name);
    (void)BrowserRefresh();
}

static VOID EditorEnterBrowser(VOID) {
    BrowserCwd[0] = 0;
    BrowserSel = 0;
    if (EFI_ERROR(BrowserRefresh())) {
        Print(L"[UefiVi] cannot read directory\r\n");
        uefi_call_wrapper(BS->Stall, 1, 1000000);
        return;
    }
    Mode = MODE_BROWSER;
}

static VOID BrowserOpenSelected(VOID) {
    EFI_STATUS st;
    CHAR16 full[160];
    const CHAR16 *name;

    if (BrowserCount == 0 || BrowserSel >= BrowserCount)
        return;

    name = BrowserNames[BrowserSel];
    if (BrowserIsDir[BrowserSel]) {
        BrowserEnterSubdir(name);
        return;
    }

    if (Dirty) {
        Print(L"\r\n[UefiVi] buffer modified — :w or discard with :q! before opening another file\r\n");
        uefi_call_wrapper(BS->Stall, 1, 2000000);
        return;
    }

    if (BrowserCwd[0] == 0)
        StrCpy(full, name);
    else
        UnicodeSPrint(full, sizeof(full), L"%s\\%s", BrowserCwd, name);

    if (StrLen(full) >= sizeof(FilePath) / sizeof(CHAR16)) {
        Print(L"\r\n[UefiVi] path too long\r\n");
        uefi_call_wrapper(BS->Stall, 1, 1500000);
        return;
    }
    StrCpy(FilePath, full);

    st = LoadFile(GImage);
    if (EFI_ERROR(st)) {
        Print(L"\r\n[UefiVi] open failed: %r\r\n", st);
        uefi_call_wrapper(BS->Stall, 1, 1500000);
        return;
    }

    Mode = MODE_NORMAL;
    ScrollRow = 0;
}

static VOID DrawBrowser(VOID) {
    UINT32 viewH = (TermRows > 4) ? TermRows - 4 : TermRows - 2;
    UINT32 cw = (TermCols > 2) ? TermCols - 2 : 78;
    UINT32 i;
    CHAR16 line[180];
    CHAR16 prefix[24];

    RefreshTermSize();
    if (viewH < 1)
        viewH = 1;
    BrowserEnsureScroll(viewH);

    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    Print(L"  UefiVi — file browser   j/k arrows  Enter=open  ESC=parent/exit  -=parent\r\n");
    if (BrowserCwd[0])
        Print(L"  \\%s\r\n\r\n", BrowserCwd);
    else
        Print(L"  \\(volume root)\r\n\r\n");

    for (i = 0; i < viewH && BrowserScroll + i < BrowserCount; i++) {
        UINT32 idx = BrowserScroll + i;
        CHAR16 mark = (idx == BrowserSel) ? L'>' : L' ';
        UINTN maxCopy;
        UINTN plen;

        if (BrowserIsDir[idx])
            StrCpy(prefix, L"[DIR] ");
        else
            StrCpy(prefix, L"      ");

        UnicodeSPrint(line, sizeof(line), L"%c %s%s", mark, prefix, BrowserNames[idx]);
        plen = StrLen(line);
        maxCopy = plen;
        if (maxCopy > cw)
            maxCopy = cw;
        line[maxCopy] = 0;
        Print(L"  %s\r\n", line);
    }

    if (BrowserCount == 0)
        Print(L"  (empty directory)\r\n");

    Print(L"\r\n  %u items\r\n", BrowserCount);
}

static UINTN Utf8EncodeOne(CHAR16 ch, UINT8 *dst, UINTN cap) {
    if (ch < 0x80 && cap >= 1) {
        dst[0] = (UINT8)ch;
        return 1;
    }
    if (ch < 0x800 && cap >= 2) {
        dst[0] = (UINT8)(0xC0 | ((ch >> 6) & 0x1F));
        dst[1] = (UINT8)(0x80 | (ch & 0x3F));
        return 2;
    }
    if (cap >= 3) {
        dst[0] = (UINT8)(0xE0 | ((ch >> 12) & 0x0F));
        dst[1] = (UINT8)(0x80 | ((ch >> 6) & 0x3F));
        dst[2] = (UINT8)(0x80 | (ch & 0x3F));
        return 3;
    }
    return 0;
}

static EFI_STATUS SaveFile(EFI_HANDLE img) {
    EFI_GUID g = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    EFI_FILE *root = NULL;
    EFI_FILE *f = NULL;
    UINT8 *buf;
    UINTN used = 0;
    UINTN wlen;
    UINT32 r;
    EFI_STATUS st;
    UINT32 lastDataLine = 0;
    UINT32 rr;

    for (rr = 0; rr < NumLines; rr++) {
        if (LineLastNonSpace(rr) != (UINT32)-1)
            lastDataLine = rr;
    }

    st = uefi_call_wrapper(BS->HandleProtocol, 3, img, &g, (VOID **)&li);
    if (EFI_ERROR(st) || li == NULL || li->DeviceHandle == NULL)
        return EFI_NOT_READY;

    root = LibOpenRoot(li->DeviceHandle);
    if (root == NULL)
        return EFI_NO_MEDIA;

    buf = AllocatePool(FILE_CAP);
    if (buf == NULL) {
        uefi_call_wrapper(root->Close, 1, root);
        return EFI_OUT_OF_RESOURCES;
    }

    for (r = 0; r <= lastDataLine && used + 4 < FILE_CAP; r++) {
        UINT32 c, last = LineLastNonSpace(r);

        if (last == (UINT32)-1) {
            if (used < FILE_CAP)
                buf[used++] = (UINT8)'\n';
            continue;
        }
        for (c = 0; c <= last && used + 4 < FILE_CAP; c++) {
            UINTN n = Utf8EncodeOne(Lines[r][c], buf + used, FILE_CAP - used);

            if (n == 0)
                break;
            used += n;
        }
        if (used < FILE_CAP)
            buf[used++] = (UINT8)'\n';
    }

    if (!EFI_ERROR(uefi_call_wrapper(root->Open, 5, root, &f, FilePath,
                                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, (UINT64)0))) {
        uefi_call_wrapper(f->Delete, 1, f);
        f = NULL;
    }

    st = uefi_call_wrapper(root->Open, 5, root, &f, FilePath,
                           EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                           (UINT64)0);
    if (EFI_ERROR(st)) {
        FreePool(buf);
        uefi_call_wrapper(root->Close, 1, root);
        return st;
    }

    wlen = used;
    st = uefi_call_wrapper(f->Write, 3, f, &wlen, buf);
    uefi_call_wrapper(f->Flush, 1, f);
    uefi_call_wrapper(f->Close, 1, f);
    uefi_call_wrapper(root->Close, 1, root);
    FreePool(buf);

    if (!EFI_ERROR(st))
        Dirty = FALSE;
    return st;
}

static VOID LoadBytesIntoBuffer(const UINT8 *data, UINTN len) {
    UINTN i = 0;
    UINT32 row = 0, col = 0;

    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
        i = 3;

    EditorInitBuffer();

    while (i < len && row < MAX_LINES) {
        UINT8 b0 = data[i];
        CHAR16 ch = 0;
        UINTN adv = 1;

        if (b0 < 0x80) {
            ch = (CHAR16)b0;
        } else if ((b0 & 0xE0) == 0xC0 && i + 1 < len) {
            ch = (CHAR16)(((b0 & 0x1F) << 6) | (data[i + 1] & 0x3F));
            adv = 2;
        } else if ((b0 & 0xF0) == 0xE0 && i + 2 < len) {
            ch = (CHAR16)(((b0 & 0x0F) << 12) | ((data[i + 1] & 0x3F) << 6) |
                          (data[i + 2] & 0x3F));
            adv = 3;
        } else {
            i++;
            continue;
        }

        i += adv;

        if (ch == L'\r')
            continue;
        if (ch == L'\n') {
            TrimTrailingSpace(row);
            row++;
            col = 0;
            if (row >= MAX_LINES)
                break;
            continue;
        }

        if (col < MAX_COL) {
            Lines[row][col++] = ch;
            if (col >= MAX_COL) {
                TrimTrailingSpace(row);
                row++;
                col = 0;
                if (row >= MAX_LINES)
                    break;
            }
        }
    }

    TrimTrailingSpace(row);
    NumLines = row + 1;
    if (NumLines < 1)
        NumLines = 1;
    CursorRow = 0;
    CursorCol = 0;
    Dirty = FALSE;
}

static EFI_STATUS LoadFile(EFI_HANDLE img) {
    EFI_GUID g = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    EFI_FILE *root = NULL;
    EFI_FILE *f = NULL;
    UINT8 *buf = NULL;
    UINTN sz;
    EFI_STATUS st;

    st = uefi_call_wrapper(BS->HandleProtocol, 3, img, &g, (VOID **)&li);
    if (EFI_ERROR(st) || li == NULL || li->DeviceHandle == NULL)
        return EFI_NOT_READY;

    root = LibOpenRoot(li->DeviceHandle);
    if (root == NULL)
        return EFI_NO_MEDIA;

    st = uefi_call_wrapper(root->Open, 5, root, &f, FilePath, EFI_FILE_MODE_READ, (UINT64)0);
    if (EFI_ERROR(st)) {
        uefi_call_wrapper(root->Close, 1, root);
        return st;
    }

    buf = AllocatePool(FILE_CAP + 1);
    if (buf == NULL) {
        uefi_call_wrapper(f->Close, 1, f);
        uefi_call_wrapper(root->Close, 1, root);
        return EFI_OUT_OF_RESOURCES;
    }

    sz = FILE_CAP;
    st = uefi_call_wrapper(f->Read, 3, f, &sz, buf);
    uefi_call_wrapper(f->Close, 1, f);
    uefi_call_wrapper(root->Close, 1, root);

    if (EFI_ERROR(st)) {
        FreePool(buf);
        return st;
    }

    buf[sz] = 0;
    LoadBytesIntoBuffer(buf, sz);
    FreePool(buf);
    return EFI_SUCCESS;
}

static VOID InsertChar(CHAR16 ch) {
    UINT32 c;

    if (CursorCol >= MAX_COL)
        return;
    for (c = MAX_COL - 1; c > CursorCol; c--)
        Lines[CursorRow][c] = Lines[CursorRow][c - 1];
    Lines[CursorRow][CursorCol] = ch;
    Lines[CursorRow][MAX_COL] = 0;
    if (CursorCol < MAX_COL - 1)
        CursorCol++;
    Dirty = TRUE;
}

static VOID DeleteUnderCursor(VOID) {
    UINT32 c;

    if (CursorCol >= MAX_COL)
        return;
    for (c = CursorCol; c < MAX_COL - 1; c++)
        Lines[CursorRow][c] = Lines[CursorRow][c + 1];
    Lines[CursorRow][MAX_COL - 1] = L' ';
    Lines[CursorRow][MAX_COL] = 0;
    Dirty = TRUE;
}

static VOID Backspace(VOID) {
    UINT32 r, c;

    if (CursorCol > 0) {
        CursorCol--;
        DeleteUnderCursor();
        Dirty = TRUE;
        return;
    }
    if (CursorRow > 0) {
        UINT32 prevlen = LineLastNonSpace(CursorRow - 1);
        UINT32 curlen = LineLastNonSpace(CursorRow);

        if (prevlen == (UINT32)-1)
            prevlen = 0;
        else
            prevlen++;

        if (curlen != (UINT32)-1) {
            for (c = 0; c <= curlen && prevlen + c < MAX_COL; c++)
                Lines[CursorRow - 1][prevlen + c] = Lines[CursorRow][c];
        }
        for (r = CursorRow; r < NumLines - 1; r++) {
            CopyMem(Lines[r], Lines[r + 1], sizeof(Lines[0]));
            TrimTrailingSpace(r);
        }
        line_clear(NumLines - 1);
        NumLines--;
        CursorRow--;
        CursorCol = prevlen <= MAX_COL ? prevlen : MAX_COL - 1;
        if (NumLines < 1) {
            NumLines = 1;
            EditorInitBuffer();
        }
        Dirty = TRUE;
    }
}

static VOID InsertNewline(VOID) {
    UINT32 r;

    if (NumLines >= MAX_LINES)
        return;
    for (r = NumLines; r > CursorRow + 1; r--)
        CopyMem(Lines[r], Lines[r - 1], sizeof(Lines[0]));
    TrimTrailingSpace(CursorRow);
    NumLines++;
    line_clear(CursorRow + 1);
    CursorRow++;
    CursorCol = 0;
    Dirty = TRUE;
}

static VOID HandleColonFinish(VOID) {
    EFI_STATUS st;

    ColonBuf[ColonLen] = 0;

    if (StrCmp(ColonBuf, L"wq") == 0 || StrCmp(ColonBuf, L"x") == 0) {
        st = SaveFile(GImage);
        Print(L"\r\n[UefiVi] write %r\r\n", st);
        if (!EFI_ERROR(st))
            Running = FALSE;
    } else if (StrCmp(ColonBuf, L"w") == 0 || StrCmp(ColonBuf, L"write") == 0) {
        st = SaveFile(GImage);
        Print(L"\r\n[UefiVi] write %r\r\n", st);
    } else if (StrCmp(ColonBuf, L"q!") == 0) {
        Running = FALSE;
    } else if (StrCmp(ColonBuf, L"q") == 0 || StrCmp(ColonBuf, L"quit") == 0) {
        if (Dirty)
            Print(L"\r\n[UefiVi] dirty buffer; use :wq or :q!\r\n");
        else
            Running = FALSE;
    } else if (StrCmp(ColonBuf, L"e") == 0 || StrCmp(ColonBuf, L"browse") == 0) {
        ColonLen = 0;
        EditorEnterBrowser();
        return;
    } else if (ColonLen > 0)
        Print(L"\r\n[UefiVi] unknown command\r\n");

    ColonLen = 0;
    Mode = MODE_NORMAL;
}

static VOID HandleKey(EFI_INPUT_KEY *key) {
    if (key == NULL)
        return;

    if (key->ScanCode == SCAN_ESC ||
        (key->ScanCode == SCAN_NULL && key->UnicodeChar == 0x001b)) {
        if (Mode == MODE_INSERT)
            Mode = MODE_NORMAL;
        else if (Mode == MODE_COLON) {
            ColonLen = 0;
            Mode = MODE_NORMAL;
        } else if (Mode == MODE_BROWSER) {
            if (BrowserCwd[0] == 0)
                Mode = MODE_NORMAL;
            else {
                BrowserGoParent();
                BrowserSel = 0;
                (VOID)BrowserRefresh();
            }
        }
        return;
    }

    if (Mode == MODE_COLON) {
        if (key->UnicodeChar == 0x0d || key->UnicodeChar == 0x0a) {
            HandleColonFinish();
            return;
        }
        if (key->UnicodeChar == 0x08 || key->ScanCode == SCAN_DELETE) {
            if (ColonLen > 0)
                ColonLen--;
            return;
        }
        if (key->UnicodeChar >= 32 && key->UnicodeChar != 0x7f && ColonLen + 1 < COLON_MAX)
            ColonBuf[ColonLen++] = key->UnicodeChar;
        return;
    }

    if (Mode == MODE_BROWSER) {
        UINT32 bv;

        RefreshTermSize();
        bv = (TermRows > 4) ? TermRows - 4 : TermRows - 2;

        if (key->UnicodeChar == L'-' || key->UnicodeChar == L'_') {
            BrowserGoParent();
            BrowserSel = 0;
            (VOID)BrowserRefresh();
            return;
        }
        if (key->UnicodeChar == L'j' || key->ScanCode == SCAN_DOWN) {
            if (BrowserSel + 1 < BrowserCount)
                BrowserSel++;
            BrowserEnsureScroll(bv);
            return;
        }
        if (key->UnicodeChar == L'k' || key->ScanCode == SCAN_UP) {
            if (BrowserSel > 0)
                BrowserSel--;
            BrowserEnsureScroll(bv);
            return;
        }
        if (key->UnicodeChar == 0x0d || key->UnicodeChar == 0x0a) {
            BrowserOpenSelected();
            return;
        }
        return;
    }

    if (Mode == MODE_INSERT) {
        if (key->UnicodeChar == 0x0d || key->UnicodeChar == 0x0a) {
            InsertNewline();
            return;
        }
        if (key->UnicodeChar == 0x08) {
            Backspace();
            return;
        }
        if (key->UnicodeChar >= 32 && key->UnicodeChar != 0x7f)
            InsertChar(key->UnicodeChar);
        return;
    }

    /* MODE_NORMAL */
    if (key->UnicodeChar == L':') {
        Mode = MODE_COLON;
        ColonLen = 0;
        return;
    }
    if (key->UnicodeChar == L'i') {
        Mode = MODE_INSERT;
        return;
    }
    if (key->UnicodeChar == L'a') {
        Mode = MODE_INSERT;
        if (CursorCol < MAX_COL - 1)
            CursorCol++;
        return;
    }
    if (key->UnicodeChar == L'o' || key->ScanCode == SCAN_F2) {
        EditorEnterBrowser();
        return;
    }
    if (key->UnicodeChar == L'h' || key->ScanCode == SCAN_LEFT) {
        if (CursorCol > 0)
            CursorCol--;
        return;
    }
    if (key->UnicodeChar == L'l' || key->ScanCode == SCAN_RIGHT) {
        if (CursorCol < MAX_COL - 1)
            CursorCol++;
        return;
    }
    if (key->UnicodeChar == L'k' || key->ScanCode == SCAN_UP) {
        if (CursorRow > 0)
            CursorRow--;
        return;
    }
    if (key->UnicodeChar == L'j' || key->ScanCode == SCAN_DOWN) {
        if (CursorRow + 1 < NumLines)
            CursorRow++;
        return;
    }
    if (key->UnicodeChar == L'x')
        DeleteUnderCursor();
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS st;

    InitializeLib(ImageHandle, SystemTable);
    GImage = ImageHandle;

    if (ST->ConOut != NULL)
        uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, TRUE);

    ParseLoadOptionsFilename(ImageHandle, FilePath, sizeof(FilePath) / sizeof(CHAR16));
    Print(L"\r\n[UefiVi] editing \"%s\" on boot volume\r\n", FilePath);

    st = LoadFile(ImageHandle);
    if (st == EFI_NOT_FOUND) {
        EditorInitBuffer();
        Print(L"[UefiVi] new file (not found)\r\n");
    } else if (EFI_ERROR(st)) {
        Print(L"[UefiVi] load %r — starting empty\r\n", st);
        EditorInitBuffer();
    }

    uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);

    while (Running) {
        EFI_INPUT_KEY key;
        EFI_STATUS ks;

        if (Mode == MODE_BROWSER)
            DrawBrowser();
        else
            DrawScreen();
        for (;;) {
            ks = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
            if (!EFI_ERROR(ks)) {
                HandleKey(&key);
                break;
            }
            uefi_call_wrapper(BS->Stall, 1, 15000);
        }
    }

    Print(L"[UefiVi] exited\r\n");
    return EFI_SUCCESS;
}
