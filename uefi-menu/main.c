/**
 * BootMenu.efi — text console boot chooser for the Typewrite USB / ESP layout.
 *
 * Expects on the same FAT volume (same directory convention as install-uefi-app.sh):
 *   \efi\boot\Typewriter.efi
 *   \efi\boot\UefiVi.efi
 *
 * Install as \efi\boot\bootx64.efi so firmware runs the menu first.
 */

#include <efi.h>
#include <efilib.h>
#include <eficon.h>

#ifndef MENU_PATH_TYPEWRITER
#define MENU_PATH_TYPEWRITER    L"\\efi\\boot\\Typewriter.efi"
#endif
#ifndef MENU_PATH_UEVIVI
#define MENU_PATH_UEVIVI        L"\\efi\\boot\\UefiVi.efi"
#endif

static EFI_STATUS RunChild(EFI_HANDLE parent, EFI_HANDLE vol, const CHAR16 *path) {
    EFI_DEVICE_PATH *dp;
    EFI_HANDLE child = NULL;
    EFI_STATUS st;

    Print(L"\r\n[BootMenu] LoadImage %s ...\r\n", path);
    dp = FileDevicePath(vol, (CHAR16 *)path);
    if (dp == NULL)
        return EFI_OUT_OF_RESOURCES;

    st = uefi_call_wrapper(BS->LoadImage, 6, FALSE, parent, dp, NULL, 0, &child);
    FreePool(dp);

    if (EFI_ERROR(st)) {
        Print(L"[BootMenu] LoadImage failed: %r\r\n", st);
        return st;
    }

    st = uefi_call_wrapper(BS->StartImage, 3, child, NULL, NULL);
    Print(L"[BootMenu] child exited: %r\r\n", st);
    /* Image is unloaded by firmware when the child returns from efi_main. */
    return st;
}

static VOID ShowMenu(VOID) {
    Print(L"\r\n");
    Print(L"  =============================================\r\n");
    Print(L"   Typewrite OS — boot menu\r\n");
    Print(L"  =============================================\r\n");
    Print(L"\r\n");
    Print(L"   [1]  Graphical typewriter  (Typewriter.efi)\r\n");
    Print(L"   [2]  Console editor       (UefiVi.efi)\r\n");
    Print(L"   [3]  Exit to Shell / firmware\r\n");
    Print(L"\r\n");
    Print(L"  Press 1, 2, or 3: ");
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_GUID g = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    EFI_STATUS st;

    InitializeLib(ImageHandle, SystemTable);

    st = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, &g, (VOID **)&li);
    if (EFI_ERROR(st) || li == NULL || li->DeviceHandle == NULL) {
        Print(L"[BootMenu] LoadedImage: %r\r\n", st);
        return (st != EFI_SUCCESS) ? st : EFI_NOT_READY;
    }

    if (ST->ConOut != NULL)
        uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, TRUE);

    uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);

    Print(L"\r\n[BootMenu] volume handle ready; 1=typewriter 2=vi 3=exit\r\n");

    for (;;) {
        EFI_INPUT_KEY key;

        ShowMenu();

        for (;;) {
            st = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
            if (!EFI_ERROR(st))
                break;
            uefi_call_wrapper(BS->Stall, 1, 15000);
        }

        if (key.UnicodeChar == L'1') {
            (void)RunChild(ImageHandle, li->DeviceHandle, MENU_PATH_TYPEWRITER);
            continue;
        }
        if (key.UnicodeChar == L'2') {
            (void)RunChild(ImageHandle, li->DeviceHandle, MENU_PATH_UEVIVI);
            continue;
        }
        if (key.UnicodeChar == L'3')
            break;
        if (key.UnicodeChar == L'q' || key.UnicodeChar == L'Q')
            break;

        Print(L"\r\n  Unknown choice — use 1, 2, or 3.\r\n");
    }

    Print(L"\r\n[BootMenu] exiting.\r\n");
    return EFI_SUCCESS;
}
