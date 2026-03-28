// Minimal UEFI app - no library dependencies
#include <efi.h>
#include <efilib.h>

EFI_STATUS
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    // Simple console output using EFI boot services
    EFI_STATUS Status;
    CHAR16 *msg = L"Hello UEFI World!\r\n";
    
    // Use ConOut directly - no library initialization
    Status = SystemTable->ConOut->OutputString(SystemTable->ConOut, msg);
    
    // Wait a bit
    SystemTable->BootServices->Stall(3000000); // 3 seconds
    
    return EFI_SUCCESS;
}
