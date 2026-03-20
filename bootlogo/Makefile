CC = gcc
CFLAGS = -O2
LDFLAGS = -lfreetype -lm

all: bootlogo

bootlogo: src/main.c
	$(CC) $(CFLAGS) src/main.c -o bootlogo $(LDFLAGS)

clean:
	rm -f bootlogo

install: bootlogo
	install -D -m 755 bootlogo $(DESTDIR)/usr/bin/bootlogo

.PHONY: all clean install