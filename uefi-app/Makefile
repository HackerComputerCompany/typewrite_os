# Makefile for Typewrite UEFI App
# Using system gnu-efi

ARCH            = x86_64
OBJS            = main.o
TARGET          = Typewriter.efi

# System gnu-efi
EFIINC          = /usr/include/efi
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
LIB             = /usr/lib
EFILIB          = /usr/lib

EFI_CRT_OBJS   = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFILIB)/elf_$(ARCH)_efi.lds

CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
                  -fshort-wchar -mno-red-zone -Wall -DEFI_FUNCTION_WRAPPER \
                  -I../fonts

LDFLAGS         = -nostdlib -znocombreloc -T $(EFI_LDS) -shared -Bsymbolic

LIBS            = /usr/lib/libgnuefi.a /usr/lib/libefi.a

# Font headers
FONTS           = ../fonts/virgil.h ../fonts/helvetica.h

.PHONY: all clean run

all: $(TARGET)

$(TARGET): main.so
	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc -O efi-app-x86_64 $< $@

main.so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c $(FONTS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) *.so $(TARGET)

run: $(TARGET)
	@cp $(TARGET) fs/
	@echo "Run: qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -m 256M -drive format=raw,file=fat:rw:fs -net none -nographic"
