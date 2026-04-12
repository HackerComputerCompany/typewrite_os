# TIC-80 UEFI — gnu-efi PE32+ + TIC-80 static libs (see README.md).
# Requires TIC-80 built with: cmake .. -DTIC80_UEFI=ON && cmake --build .

ARCH            = x86_64
REPO_ROOT       := $(abspath $(CURDIR)/..)
EFIDIR          ?= $(REPO_ROOT)/../gnu-efi
TIC80_ROOT      ?= $(REPO_ROOT)/../TIC-80
TIC80_BUILD     ?= $(TIC80_ROOT)/build-uefi-smoke

EFIINC          = $(EFIDIR)/inc
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
EFIINCS         += -I$(TIC80_ROOT)/include -I$(TIC80_BUILD)
LIB             = $(EFIDIR)/$(ARCH)/lib
EFILIB          = $(EFIDIR)/$(ARCH)/gnuefi

EFI_CRT_OBJS    = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFIDIR)/gnuefi/elf_$(ARCH)_efi.lds

TIC_LIBS        = $(TIC80_BUILD)/lib/libtic80core.a \
                  $(TIC80_BUILD)/lib/liblua.a \
                  $(TIC80_BUILD)/lib/libluaapi.a \
                  $(TIC80_BUILD)/lib/libpng.a \
                  $(TIC80_BUILD)/lib/libzlib.a \
                  $(TIC80_BUILD)/lib/libblipbuf.a \
                  $(TIC80_BUILD)/lib/libzip.a

LIBGCC          := $(shell $(CC) -print-file-name=libgcc.a)

OBJS            = main.o tic_probe.o tic_runtime.o shim.o shim_more.o shim_math.o shim_gif.o setjmp_x86_64.o
TARGET          = TIC80.efi

CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
                  -fshort-wchar -mno-red-zone -Wall -DEFI_FUNCTION_WRAPPER \
                  -U_FORTIFY_SOURCE

LDFLAGS         = -nostdlib --warn-common --no-undefined --fatal-warnings \
                  --build-id=sha1 -z nocombreloc -shared -Bsymbolic \
                  -L$(LIB) -L$(EFILIB) -T $(EFI_LDS)

LIBS            = $(EFILIB)/libgnuefi.a $(LIB)/libefi.a

.PHONY: all clean check-tic80-build

all: check-tic80-build $(TARGET)

check-tic80-build:
	@test -f $(TIC80_BUILD)/lib/libtic80core.a || (echo "Missing $(TIC80_BUILD)/lib/libtic80core.a — build TIC-80 with -DTIC80_UEFI=ON" >&2; exit 1)

$(TARGET): main.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .rodata -j .rel -j .rela -j .reloc --target efi-app-$(ARCH) $< $@

main.so: $(OBJS) $(EFI_CRT_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(EFI_CRT_OBJS) $(OBJS) \
		--start-group $(TIC_LIBS) $(LIBGCC) $(LIBS) --end-group

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) -c -o $@ $<

clean:
	rm -f $(OBJS) *.so $(TARGET)
