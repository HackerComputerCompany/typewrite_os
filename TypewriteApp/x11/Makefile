CC ?= cc

OPT_CFLAGS ?= -O2 -g
WARN_CFLAGS := -Wall -Wextra -Wshadow -Wundef -Wstrict-prototypes -Wmissing-prototypes
LOCAL_INC := -I./src -I../lib/src -I../fonts
## Font headers include <efi.h>; we only want typedefs/macros, not warnings.
NOISE_CFLAGS := -Wno-strict-prototypes -Wno-undef
# Default: vendored gnu-efi next to repo root. For Debian/Ubuntu: make USE_SYSTEM_GNU_EFI=1 (needs gnu-efi package).
GNUEFI ?= ../../../gnu-efi
ifeq ($(USE_SYSTEM_GNU_EFI),1)
EFI_INC := /usr/include/efi
else
EFI_INC := $(GNUEFI)/inc
endif
EFI_CPPFLAGS := -I$(EFI_INC)
EFI_CPPFLAGS_X64 := $(EFI_CPPFLAGS) -I$(EFI_INC)/x86_64
EFI_CPPFLAGS_I686 := $(EFI_CPPFLAGS) -I$(EFI_INC)/ia32

PKG_CONFIG ?= pkg-config
CAIRO_CFLAGS := $(shell $(PKG_CONFIG) --cflags cairo 2>/dev/null)
CAIRO_LIBS := $(shell $(PKG_CONFIG) --libs cairo 2>/dev/null)
# 32-bit deps on multilib Debian/Ubuntu (libc6-dev-i386, libx11-dev:i386, libcairo2-dev:i386, …)
PKG_CONFIG_I686_LIBDIR ?= /usr/lib/i386-linux-gnu/pkgconfig
CAIRO_CFLAGS_I686 := $(shell PKG_CONFIG_LIBDIR=$(PKG_CONFIG_I686_LIBDIR) PKG_CONFIG_PATH=$(PKG_CONFIG_I686_LIBDIR) $(PKG_CONFIG) --cflags cairo 2>/dev/null)
CAIRO_LIBS_I686 := $(shell PKG_CONFIG_LIBDIR=$(PKG_CONFIG_I686_LIBDIR) PKG_CONFIG_PATH=$(PKG_CONFIG_I686_LIBDIR) $(PKG_CONFIG) --libs cairo 2>/dev/null)

CFLAGS_X64 := $(OPT_CFLAGS) $(WARN_CFLAGS) $(LOCAL_INC) $(NOISE_CFLAGS) $(EFI_CPPFLAGS_X64) $(CAIRO_CFLAGS)
CFLAGS_I686 := $(OPT_CFLAGS) $(WARN_CFLAGS) -m32 $(LOCAL_INC) $(NOISE_CFLAGS) $(EFI_CPPFLAGS_I686) $(CAIRO_CFLAGS_I686)

LDFLAGS ?=
LDFLAGS_I686 := $(LDFLAGS) -m32

BIN_DIR := .
BIN := $(BIN_DIR)/x11typewrite
BIN_X64 := $(BIN_DIR)/x11typewrite-x86_64
BIN_I686 := $(BIN_DIR)/x11typewrite-i686

SRC_SHARED := \
	../lib/src/tw_core.c \
	../lib/src/tw_doc.c

SRC := src/main_x11.c src/tw_bitmapfont_uefi.c src/pdf_export.c src/tw_x11_settings.c src/tw_sound.c

# Phony must not equal BIN_I686 filename or make sees a circular dependency.
.PHONY: all clean portable i686

all: $(BIN)

# Default binary name stays x11typewrite (x86_64 on typical desktop); also build x86_64-suffixed copy for dist.
$(BIN): $(SRC) $(SRC_SHARED)
	$(CC) $(CFLAGS_X64) -o $@ $^ $(LDFLAGS) $(CAIRO_LIBS) -lX11 -lm -lSDL2
	cp -f $@ $(BIN_X64)

# Optional: requires 32-bit dev libraries (gcc -m32 / multilib). May fail on minimal systems — portable script tolerates absence.
$(BIN_I686): $(SRC) $(SRC_SHARED)
	@if [ -z "$(CAIRO_CFLAGS_I686)" ] || [ -z "$(CAIRO_LIBS_I686)" ]; then \
		echo "Skipping i686: no 32-bit Cairo pkg-config (install libcairo2-dev:i386 and set PKG_CONFIG_I686_LIBDIR if needed)" >&2; \
		exit 1; \
	fi
	$(CC) $(CFLAGS_I686) -o $@ $^ $(LDFLAGS_I686) $(CAIRO_LIBS_I686) -lX11 -lm -lSDL2

i686: $(BIN_I686)

# Build amd64, try i686, then pack (script skips i686 if binary missing).
portable: $(BIN)
	-$(MAKE) i686 || true
	./make-portable-tarball.sh --no-build

clean:
	rm -f $(BIN) $(BIN_X64) $(BIN_I686)
