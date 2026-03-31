# Boot-time text menu: launches Typewriter.efi or UefiVi.efi from the ESP (same toolchain as uefi-vi/).

ARCH            = x86_64
OBJS            = main.o
TARGET          = BootMenu.efi

REPO_ROOT       := $(abspath $(CURDIR)/..)
EFIDIR          ?= $(REPO_ROOT)/../gnu-efi
EFIINC          = $(EFIDIR)/inc
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
LIB             = $(EFIDIR)/$(ARCH)/lib
EFILIB          = $(EFIDIR)/$(ARCH)/gnuefi

EFI_CRT_OBJS   = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFIDIR)/gnuefi/elf_$(ARCH)_efi.lds

CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
                  -fshort-wchar -mno-red-zone -Wall -DEFI_FUNCTION_WRAPPER

LDFLAGS         = -nostdlib --warn-common --no-undefined --fatal-warnings \
                  --build-id=sha1 -z nocombreloc -shared -Bsymbolic \
                  -L$(LIB) -L$(EFILIB) -T $(EFI_LDS)

LIBS            = $(EFILIB)/libgnuefi.a $(LIB)/libefi.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .rodata -j .rel -j .rela -j .reloc --target efi-app-$(ARCH) $< $@

main.so: $(OBJS) $(EFI_CRT_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(EFI_CRT_OBJS) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) *.so $(TARGET)
