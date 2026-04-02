/**
 * TIC-80 UEFI scaffold — valid PE32+ app to exercise GOP + console before
 * linking libtic80*.a from a TIC-80 build (-DTIC80_UEFI=ON).
 */
#include <efi.h>
#include <efilib.h>
#include <efiprot.h>

#include "tic_runtime.h"

void tic80_uefi_probe_create_destroy(void);

static EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;

static VOID WaitForAnyKey(VOID) {
    EFI_INPUT_KEY key;
    EFI_EVENT ev = ST->ConIn->WaitForKey;
    UINTN idx;

    uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ev, &idx);
    uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS st;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    UINT32 mode;

    (VOID)ImageHandle;
    InitializeLib(ImageHandle, SystemTable);

    Print(L"\r\n");
    Print(L"========================================\r\n");
    Print(L"  TIC-80 UEFI scaffold (Typewrite OS)\r\n");
    Print(L"  Runtime loop + GOP Blt + tic80_sound.\r\n");
    Print(L"========================================\r\n\r\n");

    st = LibLocateProtocol(&gopGuid, (VOID **)&Gop);
    if (EFI_ERROR(st) || Gop == NULL || Gop->Mode == NULL) {
        Print(L"GOP not available (%r). Console-only.\r\n", st);
        Print(L"TIC-80 core probe...\r\n");
        tic80_uefi_probe_create_destroy();
        Print(L"Probe done.\r\nPress any key...\r\n");
        WaitForAnyKey();
        return st;
    }

    mode = Gop->Mode->Mode;
    info = Gop->Mode->Info;
    Print(L"GOP mode=%u  %ux%u  PixelsPerScanLine=%u  PixelFormat=%u\r\n",
          mode,
          info->HorizontalResolution,
          info->VerticalResolution,
          info->PixelsPerScanLine,
          (UINT32)info->PixelFormat);

    tic80_uefi_run_loop(ImageHandle, Gop);

    Print(L"\r\nPress any key to exit...\r\n");
    WaitForAnyKey();
    return EFI_SUCCESS;
}
