CC ?= cc

CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -Wshadow -Wundef -Wstrict-prototypes -Wmissing-prototypes
CFLAGS += -I./src -I../linux-typewrite/src

LDFLAGS ?=
LDLIBS += -lX11

BIN_DIR := .
BIN := $(BIN_DIR)/x11typewrite

SRC_SHARED := \
	../linux-typewrite/src/tw_core.c \
	../linux-typewrite/src/tw_font8x8.c

SRC := src/main_x11.c

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC) $(SRC_SHARED)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(BIN)

