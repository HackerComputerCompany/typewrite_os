#pragma once

#include <stdint.h>

typedef struct {
    int fd;
    int width;
    int height;
    int stride_bytes;
    int bpp;
    uint8_t *map;
    uint32_t map_len;
} TwFb;

int tw_fbdev_open(TwFb *fb, const char *path);
void tw_fbdev_close(TwFb *fb);

void tw_fbdev_fill(TwFb *fb, uint32_t rgb); /* 0x00RRGGBB */
void tw_fbdev_put_pixel(TwFb *fb, int x, int y, uint32_t rgb);

