# Makefile for Typewriter UEFI App
# Using locally-built modern gnu-efi
#
# Default goal is `ship`: build, refresh fs/Typewriter.efi, commit+push uefi-app/
# when there are staged changes. Use `make all` for compile only (no git).

ARCH            = x86_64
OBJS            = main.o
TARGET          = Typewriter.efi

REPO_ROOT       := $(abspath $(CURDIR)/..)

# gnu-efi tree (clone + `make` in gnu-efi). Default: sibling dir next to this repo,
# e.g. .../projects/typewrite_os/../gnu-efi. Override: `EFIDIR=/path make all` or export EFIDIR.
EFIDIR          ?= $(REPO_ROOT)/../../gnu-efi
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

.PHONY: all clean ship

.DEFAULT_GOAL   := ship

# MSG="your message" optional; default commit subject below
ship: all
	@mkdir -p fs
	@cp -f $(TARGET) fs/$(TARGET)
	@cd $(REPO_ROOT) && \
	git add -A -- uefi-app/ && \
	git add -u -- start-qemu.sh README.md BUILD_SYSTEM.md 2>/dev/null || true && \
	if git diff --staged --quiet; then \
		echo "uefi-app: nothing to commit (already synced)."; \
	else \
		git commit -m "$(if $(MSG),$(MSG),uefi-app: build $(TARGET))" && \
		git push; \
	fi

all: $(TARGET)

$(TARGET): main.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .rodata -j .rel -j .rela -j .reloc --target efi-app-$(ARCH) $< $@

main.so: $(OBJS) $(EFI_CRT_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(EFI_CRT_OBJS) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) *.so $(TARGET)
