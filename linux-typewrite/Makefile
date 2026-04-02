CC ?= cc

CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -Wshadow -Wundef -Wstrict-prototypes -Wmissing-prototypes
CFLAGS += -I./src

LDFLAGS ?=

BIN_DIR := .

FB_BIN := $(BIN_DIR)/fbtypewrite

SRC_COMMON := src/tw_core.c src/tw_font8x8.c src/tw_termios.c
SRC_FB := src/tw_fbdev.c src/main_fb.c

.PHONY: all clean

all: $(FB_BIN)

$(FB_BIN): $(SRC_COMMON) $(SRC_FB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(FB_BIN)

