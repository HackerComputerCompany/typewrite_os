#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <ft2build.h>
#include FT_FREETYPE_H

typedef struct {
    uint8_t r, g, b, a;
} Color;

typedef struct {
    int width;
    int height;
    uint32_t stride;
    void *fb_map;
    size_t fb_size;
    int fb_fd;
} Display;

static Display display;
static FT_Library ft_library = NULL;
static FT_Face ft_face = NULL;

static void draw_pixel(int x, int y, Color c) {
    if (x < 0 || x >= display.width || y < 0 || y >= display.height) return;
    uint32_t offset = (y * display.stride) + (x * 4);
    uint32_t *ptr = (uint32_t *)((uint8_t *)display.fb_map + offset);
    *ptr = (c.r << 16) | (c.g << 8) | c.b | (c.a << 24);
}

static void draw_rect(int x, int y, int w, int h, Color c) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            draw_pixel(x + dx, y + dy, c);
        }
    }
}

static void draw_text(FT_Face face, int x, int y, const char *text, Color c, int size) {
    FT_Set_Char_Size(face, 0, size * 64, 72, 72);
    int pen_x = x;
    int pen_y = y;
    
    for (const char *p = text; *p; p++) {
        if (FT_Load_Glyph(face, FT_Get_Char_Index(face, (unsigned char)*p), FT_LOAD_RENDER)) continue;
        
        FT_Bitmap *bmp = &face->glyph->bitmap;
        int top = face->glyph->bitmap_top;
        int left = face->glyph->bitmap_left;
        
        for (int py = 0; py < bmp->rows; py++) {
            for (int px = 0; px < bmp->width; px++) {
                int alpha = bmp->buffer[py * bmp->pitch + px];
                if (alpha > 0) {
                    float a = alpha / 255.0f;
                    int sx = pen_x + left + px;
                    int sy = pen_y - top + py;
                    Color out = {
                        (uint8_t)(c.r * a + 200 * (1 - a)),
                        (uint8_t)(c.g * a + 200 * (1 - a)),
                        (uint8_t)(c.b * a + 200 * (1 - a)),
                        255
                    };
                    draw_pixel(sx, sy, out);
                }
            }
        }
        pen_x += (face->glyph->advance.x >> 6);
    }
}

static int init_framebuffer(void) {
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    display.fb_fd = open("/dev/fb0", O_RDWR);
    if (display.fb_fd < 0) return -1;
    
    if (ioctl(display.fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        close(display.fb_fd);
        return -1;
    }
    
    if (ioctl(display.fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        close(display.fb_fd);
        return -1;
    }
    
    display.width = vinfo.xres;
    display.height = vinfo.yres;
    display.stride = finfo.line_length;
    display.fb_size = finfo.smem_len;
    
    display.fb_map = mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, display.fb_fd, 0);
    if (display.fb_map == MAP_FAILED) {
        close(display.fb_fd);
        return -1;
    }
    
    return 0;
}

static int init_font(const char *path) {
    if (FT_Init_FreeType(&ft_library)) return -1;
    if (FT_New_Face(ft_library, path, 0, &ft_face)) {
        FT_Done_FreeType(ft_library);
        return -1;
    }
    return 0;
}

static Color make_color(uint8_t r, uint8_t g, uint8_t b) {
    return (Color){r, g, b, 255};
}

static void draw_keyboard_row(int x, int y, int key_w, int key_h, int gap, int num_keys, Color key_color) {
    for (int i = 0; i < num_keys; i++) {
        Color c = key_color;
        draw_rect(x + i * (key_w + gap), y, key_w, key_h, c);
        draw_rect(x + i * (key_w + gap), y, key_w - 1, 1, make_color(255, 255, 255));
        draw_rect(x + i * (key_w + gap), y, 1, key_h - 1, make_color(255, 255, 255));
        draw_rect(x + i * (key_w + gap) + key_w - 1, y + 1, 1, key_h - 1, make_color(80, 60, 40));
        draw_rect(x + i * (key_w + gap) + 1, y + key_h - 1, key_w - 1, 1, make_color(80, 60, 40));
    }
}

static void draw_typewriter(int center_x, int center_y) {
    int key_w = 28;
    int key_h = 28;
    int gap = 3;
    
    Color key_colors[] = {
        make_color(40, 35, 30),
        make_color(60, 50, 40),
        make_color(50, 45, 35),
    };
    
    int row_y = center_y + 60;
    draw_keyboard_row(center_x - 125, row_y, key_w, key_h, gap, 10, key_colors[0]);
    
    row_y += key_h + gap + 5;
    draw_keyboard_row(center_x - 112, row_y, key_w, key_h, gap, 9, key_colors[1]);
    
    row_y += key_h + gap + 5;
    draw_keyboard_row(center_x - 100, row_y, key_w, key_h, gap, 7, key_colors[2]);
    
    int space_w = 140;
    int space_x = center_x - space_w / 2;
    row_y += key_h + gap + 5;
    draw_rect(space_x, row_y, space_w, key_h, make_color(180, 160, 140));
    draw_rect(space_x, row_y, space_w - 1, 1, make_color(255, 255, 255));
    draw_rect(space_x, row_y, 1, key_h - 1, make_color(255, 255, 255));
    draw_rect(space_x + space_w - 1, row_y + 1, 1, key_h - 1, make_color(80, 60, 40));
    draw_rect(space_x + 1, row_y + key_h - 1, space_w - 1, 1, make_color(80, 60, 40));
    
    int paper_w = 200;
    int paper_h = 80;
    int paper_x = center_x - paper_w / 2;
    int paper_y = center_y - 120;
    
    draw_rect(paper_x - 5, paper_y - 5, paper_w + 10, paper_h + 10, make_color(80, 70, 60));
    draw_rect(paper_x, paper_y, paper_w, paper_h, make_color(250, 248, 240));
}

static void draw_gradient_bar(int x, int y, int w, int h, float progress) {
    Color colors[] = {
        make_color(139, 90, 43),
        make_color(205, 133, 63),
        make_color(255, 215, 0),
        make_color(50, 205, 50),
        make_color(30, 144, 255),
        make_color(138, 43, 226),
        make_color(255, 69, 0),
    };
    int num_colors = sizeof(colors) / sizeof(colors[0]);
    int segment_w = (w * progress) / num_colors;
    if (segment_w < 1) segment_w = 1;
    
    for (int i = 0; i < num_colors && i * segment_w < w * progress; i++) {
        int sw = segment_w;
        if (i == num_colors - 1 || (i + 1) * segment_w > w * progress) {
            sw = (int)(w * progress) - i * segment_w;
        }
        if (sw > 0) {
            draw_rect(x + i * segment_w, y, sw, h, colors[i]);
        }
    }
}

static void draw_progress_bar(int x, int y, int w, int h, float progress) {
    draw_rect(x, y, w, h, make_color(60, 50, 40));
    draw_rect(x + 2, y + 2, w - 4, h - 4, make_color(40, 35, 30));
    if (progress > 0.001f) {
        draw_gradient_bar(x + 2, y + 2, w - 4, h - 4, progress);
    }
}

int main(void) {
    if (init_framebuffer() != 0) {
        fprintf(stderr, "bootlogo: Cannot init framebuffer\n");
        return 1;
    }
    
    draw_rect(0, 0, display.width, display.height, make_color(25, 20, 15));
    
    const char *font_paths[] = {
        "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
        NULL
    };
    
    int font_loaded = 0;
    for (int i = 0; font_paths[i]; i++) {
        if (init_font(font_paths[i]) == 0) {
            font_loaded = 1;
            break;
        }
    }
    
    int center_x = display.width / 2;
    int center_y = display.height / 2;
    
    if (font_loaded) {
        Color title_color = make_color(255, 248, 220);
        int title_y = center_y - 180;
        
        FT_Set_Char_Size(ft_face, 0, 72 * 64, 72, 72);
        int title_width = 0;
        for (const char *p = "TYPEWRITE"; *p; p++) {
            if (FT_Load_Glyph(ft_face, FT_Get_Char_Index(ft_face, (unsigned char)*p), FT_LOAD_RENDER)) continue;
            title_width += (ft_face->glyph->advance.x >> 6);
        }
        draw_text(ft_face, center_x - title_width / 2, title_y, "TYPEWRITE", title_color, 72);
        
        Color sub_color = make_color(180, 170, 150);
        draw_text(ft_face, center_x - 40, title_y + 75, "OS", sub_color, 48);
    }
    
    draw_typewriter(center_x, center_y);
    
    int bar_w = 300;
    int bar_h = 12;
    int bar_x = center_x - bar_w / 2;
    int bar_y = center_y + 200;
    
    for (int i = 0; i <= 20; i++) {
        draw_progress_bar(bar_x, bar_y, bar_w, bar_h, i / 20.0f);
        
        int label_y = bar_y + bar_h + 8;
        if (font_loaded) {
            char label[32];
            snprintf(label, sizeof(label), "Loading... %d%%", i * 5);
            Color label_color = make_color(180, 170, 150);
            draw_text(ft_face, center_x - 60, label_y, label, label_color, 16);
        }
        
        usleep(40000);
    }
    
    usleep(500000);
    
    if (ft_face) FT_Done_Face(ft_face);
    if (ft_library) FT_Done_FreeType(ft_library);
    if (display.fb_map) munmap(display.fb_map, display.fb_size);
    if (display.fb_fd >= 0) close(display.fb_fd);
    
    return 0;
}