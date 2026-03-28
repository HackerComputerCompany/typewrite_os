# Makefile for Typewriter UEFI App
# Using locally-built modern gnu-efi

ARCH            = x86_64
OBJS            = main.o
TARGET          = Typewriter.efi

# Use locally-built modern gnu-efi
EFIDIR          = /ironwolf4TB/data01/projects/gnu-efi
EFIINC          = $(EFIDIR)/inc
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
LIB             = $(EFIDIR)/$(ARCH)/lib
EFILIB          = $(EFIDIR)/gnuefi

EFI_CRT_OBJS   = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFILIB)/elf_$(ARCH)_efi.lds

CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
                  -fshort-wchar -mno-red-zone -Wall -DEFI_FUNCTION_WRAPPER \
                  -I../fonts

LDFLAGS         = -nostdlib -znocombreloc -T $(EFI_LDS) -shared -Bsymbolic

LIBS            = $(EFIDIR)/x86_64/gnuefi/libgnuefi.a $(LIB)/libefi.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.so
	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc -O efi-app-x86_64 $< $@

main.so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) *.so $(TARGET)
