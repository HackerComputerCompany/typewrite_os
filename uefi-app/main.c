#include <efi.h>
#include <efilib.h>

EFI_STATUS
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n========================================\r\n");
    Print(L"  Typewrite OS - UEFI App Test\r\n");
    Print(L"========================================\r\n\r\n");
    Print(L"Hello from UEFI!\r\n\r\n");
    Print(L"App running successfully!\r\n\r\n");
    
    // Wait for key press
    Print(L"Press any key to exit...\r\n");
    
    EFI_INPUT_KEY key;
    while (TRUE) {
        EFI_STATUS status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
        if (!EFI_ERROR(status)) {
            break;
        }
        uefi_call_wrapper(BS->Stall, 1, 10000);
    }

    Print(L"\r\nExiting Typewrite UEFI App...\r\n");

    return EFI_SUCCESS;
}
