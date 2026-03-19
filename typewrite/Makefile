CC ?= gcc
CFLAGS ?= -Wall -O2
LDFLAGS ?= -lfreetype -lm

TYPEWRITE_SRCS := src/main.c

all:
	$(CC) $(CFLAGS) $(TYPEWRITE_SRCS) $(LDFLAGS) -o typewrite

install:
	install -D -m 755 typewrite $(TARGET_DIR)/usr/bin/typewrite

clean:
	rm -f typewrite
