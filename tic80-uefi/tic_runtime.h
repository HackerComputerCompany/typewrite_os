#pragma once

#include <efi.h>
#include <efiprot.h>

void tic80_uefi_run_loop(EFI_HANDLE image, EFI_GRAPHICS_OUTPUT_PROTOCOL *gop);
