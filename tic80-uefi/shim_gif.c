/* Stubs when tic80core is built without src/ext/gif.c (UEFI) but cart.c still references GIF. */
#include <stddef.h>

typedef unsigned char u8;
typedef int s32;

typedef struct {
    u8 r, g, b;
} gif_color;

typedef struct {
    u8 *buffer;
    gif_color *palette;
    s32 width;
    s32 height;
    s32 colors;
} gif_image;

gif_image *gif_read_data(const void *buffer, int size) {
    (void)buffer;
    (void)size;
    return NULL;
}

void gif_close(gif_image *image) {
    (void)image;
}
