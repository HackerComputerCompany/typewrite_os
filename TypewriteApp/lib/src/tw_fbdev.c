#include "tw_fbdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static uint32_t pack_xrgb8888(uint32_t rgb) {
    return 0xff000000u | (rgb & 0x00ffffffu);
}

int tw_fbdev_open(TwFb *fb, const char *path) {
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;

    if (!fb || !path)
        return -1;
    memset(fb, 0, sizeof(*fb));
    fb->fd = open(path, O_RDWR);
    if (fb->fd < 0)
        return -1;
    if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &v) != 0)
        goto fail;
    if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &f) != 0)
        goto fail;

    fb->width = (int)v.xres;
    fb->height = (int)v.yres;
    fb->bpp = (int)v.bits_per_pixel;
    fb->stride_bytes = (int)f.line_length;
    fb->map_len = (uint32_t)f.smem_len;

    if (fb->bpp != 32) {
        fprintf(stderr, "fbdev: unsupported bpp=%d (need 32)\n", fb->bpp);
        goto fail;
    }

    fb->map = (uint8_t *)mmap(NULL, fb->map_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
    if (fb->map == MAP_FAILED) {
        fb->map = NULL;
        goto fail;
    }

    return 0;

fail:
    tw_fbdev_close(fb);
    return -1;
}

void tw_fbdev_close(TwFb *fb) {
    if (!fb)
        return;
    if (fb->map) {
        munmap(fb->map, fb->map_len);
        fb->map = NULL;
    }
    if (fb->fd > 0) {
        close(fb->fd);
        fb->fd = -1;
    }
}

void tw_fbdev_fill(TwFb *fb, uint32_t rgb) {
    if (!fb || !fb->map)
        return;
    uint32_t xrgb = pack_xrgb8888(rgb);
    for (int y = 0; y < fb->height; y++) {
        uint32_t *row = (uint32_t *)(fb->map + (size_t)y * (size_t)fb->stride_bytes);
        for (int x = 0; x < fb->width; x++)
            row[x] = xrgb;
    }
}

void tw_fbdev_put_pixel(TwFb *fb, int x, int y, uint32_t rgb) {
    if (!fb || !fb->map)
        return;
    if ((unsigned)x >= (unsigned)fb->width || (unsigned)y >= (unsigned)fb->height)
        return;
    uint32_t *row = (uint32_t *)(fb->map + (size_t)y * (size_t)fb->stride_bytes);
    row[x] = pack_xrgb8888(rgb);
}

