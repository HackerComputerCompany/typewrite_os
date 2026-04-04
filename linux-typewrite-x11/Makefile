CC ?= cc

CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -Wshadow -Wundef -Wstrict-prototypes -Wmissing-prototypes
CFLAGS += -I./src -I../linux-typewrite/src
## Font headers include <efi.h>; we only want typedefs/macros, not warnings.
CFLAGS += -Wno-strict-prototypes -Wno-undef
# Default: vendored gnu-efi next to repo root. For Debian/Ubuntu: make USE_SYSTEM_GNU_EFI=1 (needs gnu-efi package).
GNUEFI ?= ../../gnu-efi
ifeq ($(USE_SYSTEM_GNU_EFI),1)
EFI_INC := /usr/include/efi
else
EFI_INC := $(GNUEFI)/inc
endif
CFLAGS += -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(EFI_INC)/protocol

LDFLAGS ?=
LDLIBS += -lX11

PKG_CONFIG ?= pkg-config
CAIRO_CFLAGS := $(shell $(PKG_CONFIG) --cflags cairo 2>/dev/null)
CAIRO_LIBS := $(shell $(PKG_CONFIG) --libs cairo 2>/dev/null)
CFLAGS += $(CAIRO_CFLAGS)
LDLIBS += $(CAIRO_LIBS) -lm

BIN_DIR := .
BIN := $(BIN_DIR)/x11typewrite

SRC_SHARED := \
	../linux-typewrite/src/tw_core.c \
	../linux-typewrite/src/tw_doc.c

SRC := src/main_x11.c src/tw_bitmapfont_uefi.c src/pdf_export.c

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC) $(SRC_SHARED)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(BIN)

