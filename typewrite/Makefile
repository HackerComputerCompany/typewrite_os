CC ?= gcc
CFLAGS ?= -Wall -O2 $(shell pkg-config --cflags freetype2)
LDFLAGS ?= $(shell pkg-config --libs freetype2) -lm

TYPEWRITE_SRCS := src/main.c

all:
	$(CC) $(CFLAGS) $(TYPEWRITE_SRCS) $(LDFLAGS) -o typewrite

install:
	install -D -m 755 typewrite $(TARGET_DIR)/usr/bin/typewrite

clean:
	rm -f typewrite
