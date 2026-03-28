# Makefile for Typewrite UEFI App
# Using locally-built gnu-efi from source

ARCH            = x86_64
OBJS            = main.o
TARGET          = typewrite.efi

# Use locally-built gnu-efi (from source)
EFIDIR          = /ironwolf4TB/data01/projects/typewrite_os/gnu-efi-src
EFIINC          = $(EFIDIR)/inc
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
LIB             = $(EFIDIR)/$(ARCH)/lib
EFILIB          = $(EFIDIR)/gnuefi

EFI_CRT_OBJS    = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFILIB)/elf_$(ARCH)_efi.lds

# Compiler flags
CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
                  -fshort-wchar -mno-red-zone -Wall -DEFI_FUNCTION_WRAPPER

# Linker flags
LDFLAGS         = -nostdlib -znocombreloc -T $(EFI_LDS) -shared -Bsymbolic

# Additional libs
LIBS            = -L$(EFIDIR)/x86_64/gnuefi -L$(EFIDIR)/x86_64/lib -lgnuefi -lefi

.PHONY: all clean run debug

all: $(TARGET)

# Build ELF shared object, then convert to PE using local gnu-efi tools
$(TARGET): main.so
	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc -O efi-app-x86_64 $< $@

main.so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) *.so $(TARGET)

# Run in QEMU with EFI shell
run: $(TARGET)
	@mkdir -p fs
	@cp $(TARGET) fs/
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
		-m 256M \
		-drive format=raw,file=fat:rw:fs \
		-net none \
		-nographic \
		-display curses

# Debug with GDB
debug: $(TARGET)
	@mkdir -p fs
	@cp $(TARGET) fs/
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
		-m 256M \
		-drive format=raw,file=fat:rw:fs \
		-net none \
		-nographic \
		-s -S
