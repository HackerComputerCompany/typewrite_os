/**
* HelloWorld.c
*
* Copyright (c) 2024, Brad Anderson
* SPDX-License-Identifier: BSD-3-Clause
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>

INTN
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
  UINTN Index;

  Print(L"========================================\r\n");
  Print(L"  Typewrite OS - EDK II Test\r\n");
  Print(L"========================================\r\n\r\n");
  Print(L"Hello from EDK II UEFI Application!\r\n\r\n");

  for (Index = 0; Index < Argc; Index++) {
    Print(L"Argv[%d]: %s\r\n", Index, Argv[Index]);
  }

  Print(L"\r\nPress any key to exit...\r\n");
  
  // Wait for key press
  WaitForSingleEvent (gST->ConIn->WaitForKey, 0);
  InputChar ();

  Print(L"\r\nExiting...\r\n");

  return 0;
}
