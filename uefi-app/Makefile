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
EFILIB          = $(EFIDIR)/$(ARCH)/gnuefi

EFI_CRT_OBJS   = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFIDIR)/gnuefi/elf_$(ARCH)_efi.lds

CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
                  -fshort-wchar -mno-red-zone -Wall -DEFI_FUNCTION_WRAPPER \
                  -I../fonts

# Use same linker flags as gnu-efi - critical for valid PE output
LDFLAGS         = -nostdlib --warn-common --no-undefined --fatal-warnings \
                  --build-id=sha1 -z nocombreloc -shared -Bsymbolic \
                  -L$(LIB) -L$(EFILIB) -T $(EFI_LDS)

# Link with .a files directly
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
