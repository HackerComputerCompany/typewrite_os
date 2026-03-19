#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define MAX_LINES 500
#define MAX_LINE_LEN 200
#define DEFAULT_FONT_SIZE 14
#define STATUS_BAR_HEIGHT 30
#define MAX_RESOLUTIONS 4
#define PAGE_GAP 10

static const int resolutions[MAX_RESOLUTIONS][2] = {
    {640, 480},
    {800, 600},
    {1024, 768},
    {0, 0}
};

typedef struct {
    uint8_t r, g, b, a;
} Color;

typedef struct {
    char text[MAX_LINE_LEN];
    int char_count;
    uint8_t ink_idx[MAX_LINE_LEN];
    uint8_t bold[MAX_LINE_LEN];
    bool page_break;  // true if page break after this line
} Line;

typedef struct {
    Line lines[MAX_LINES];
    int total_lines;
    int cursor_line;
    int cursor_col;
    int top_line;
    int ink_idx;
    bool bold;
    int zoom;
    int res_idx;
    int tab_width;
    bool inverted;
    bool dirty;
} Document;

typedef struct {
    int width;
    int height;
    uint32_t stride;
    void *fb_map;
    void *backbuffer;
    size_t fb_size;
    int fb_fd;
    int margin_x;
    int margin_y;
} Display;

typedef struct {
    FT_Library ft;
    FT_Face face;
    FT_Face face_bold;
    FT_GlyphSlot slot;
    int size;
    int height;
    int char_width;
} FontState;

static Display display;
static FontState font;
static Document doc;
static int tty_fd = -1;
static int original_kb_mode = KD_TEXT;
static FILE *debug_log = NULL;
static int show_help = 0;
static char toast_msg[128] = "";
static int toast_frames = 0;

static void show_toast(const char *msg) {
    strncpy(toast_msg, msg, sizeof(toast_msg) - 1);
    toast_frames = 1;
}

static Color COLOR_BLACK = {0, 0, 0, 255};
static Color COLOR_WHITE = {255, 255, 255, 255};
static Color COLOR_RED = {180, 30, 30, 255};
static Color COLOR_GREEN = {30, 140, 30, 255};
static Color COLOR_BLUE = {30, 60, 180, 255};
static Color COLOR_GRAY = {200, 200, 200, 255};
static Color COLOR_PAGE = {250, 250, 250, 255};
static Color COLOR_BG = {220, 220, 220, 255};
static Color ink_colors[4];
static const char *ink_names[] = {"Black", "Green", "Red", "Blue"};

static void init_ink_colors(void) {
    ink_colors[0] = COLOR_BLACK;
    ink_colors[1] = COLOR_GREEN;
    ink_colors[2] = COLOR_RED;
    ink_colors[3] = COLOR_BLUE;
}

static void restore_console(void) {
    if (tty_fd >= 0) {
        ioctl(tty_fd, KDSETMODE, original_kb_mode);
        close(tty_fd);
        tty_fd = -1;
    }
}

static void signal_handler(int sig) {
    (void)sig;
    restore_console();
    _exit(1);
}

static int set_graphics_mode(void) {
    tty_fd = open("/dev/tty0", O_RDWR);
    if (tty_fd < 0) {
        tty_fd = open("/dev/console", O_RDWR);
    }
    if (tty_fd < 0) {
        return -1;
    }
    
    if (ioctl(tty_fd, KDGETMODE, &original_kb_mode) < 0) {
        original_kb_mode = KD_TEXT;
    }
    
    if (ioctl(tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
        close(tty_fd);
        tty_fd = -1;
        return -1;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);
    
    return 0;
}

static Color invert_color(Color c) {
    return (Color){255 - c.r, 255 - c.g, 255 - c.b, c.a};
}

static Color get_color(Color normal) {
    return doc.inverted ? invert_color(normal) : normal;
}

static int get_char_width(void) {
    return font.char_width;
}

static int get_char_height(void) {
    return font.height;
}

static int get_page_content_width(void) {
    return display.width - display.margin_x * 2;
}

static int get_page_content_height(void) {
    return display.height - display.margin_y * 2 - STATUS_BAR_HEIGHT;
}

static int get_chars_per_line(void) {
    int cw = get_char_width();
    return cw > 0 ? get_page_content_width() / cw : 80;
}

static int get_lines_per_page(void) {
    int ch = get_char_height();
    return ch > 0 ? get_page_content_height() / ch : 50;
}

static int get_page_start_x(void) {
    return display.margin_x;
}

static int get_page_start_y(void) {
    return display.margin_y;
}

static void blit_pixel(int x, int y, Color c) {
    if (x < 0 || x >= display.width || y < 0 || y >= display.height) return;
    uint32_t offset = (y * display.stride) + (x * 4);
    uint32_t *ptr = (uint32_t *)((uint8_t *)display.backbuffer + offset);
    *ptr = (c.r << 16) | (c.g << 8) | c.b | (c.a << 24);
}

static void blit_rect(int x, int y, int w, int h, Color c) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            blit_pixel(x + dx, y + dy, c);
        }
    }
}

static void blit_line_h(int x1, int y, int x2, Color c) {
    if (y < 0 || y >= display.height) return;
    if (x1 > x2) { int tmp = x1; x1 = x2; x2 = tmp; }
    if (x2 < 0 || x1 >= display.width) return;
    x1 = x1 < 0 ? 0 : x1;
    x2 = x2 >= display.width ? display.width - 1 : x2;
    for (int x = x1; x <= x2; x++) blit_pixel(x, y, c);
}

static void blit_line_v(int x, int y1, int y2, Color c) {
    if (x < 0 || x >= display.width) return;
    if (y1 > y2) { int tmp = y1; y1 = y2; y2 = tmp; }
    if (y2 < 0 || y1 >= display.height) return;
    y1 = y1 < 0 ? 0 : y1;
    y2 = y2 >= display.height ? display.height - 1 : y2;
    for (int y = y1; y <= y2; y++) blit_pixel(x, y, c);
}

static void blit_clear(Color c) {
    blit_rect(0, 0, display.width, display.height, c);
}

static void blit_glyph(int x, int y, FT_Bitmap *bitmap, Color fg) {
    int px, py, a;
    unsigned char *s = bitmap->buffer;
    Color bg = get_color(COLOR_WHITE);
    
    for (py = 0; py < bitmap->rows; py++) {
        for (px = 0; px < bitmap->width; px++) {
            a = s[px];
            int sx = x + px;
            int sy = y + py;
            if (sx >= 0 && sx < display.width && sy >= 0 && sy < display.height) {
                float alpha = a / 255.0f;
                Color out = {
                    (uint8_t)(fg.r * alpha + bg.r * (1 - alpha)),
                    (uint8_t)(fg.g * alpha + bg.g * (1 - alpha)),
                    (uint8_t)(fg.b * alpha + bg.b * (1 - alpha)),
                    255
                };
                blit_pixel(sx, sy, out);
            }
        }
        s += bitmap->pitch;
    }
}

static void blit_text(int x, int y, const char *text, Color c, bool use_bold) {
    FT_Face face = (use_bold && font.face_bold) ? font.face_bold : font.face;
    FT_GlyphSlot slot = face->glyph;
    const char *p = text;
    int pen_x = x;
    int cell_height = get_char_height();
    int cell_width = get_char_width();
    int baseline_y = y + cell_height;
    
    while (*p) {
        FT_Load_Glyph(face, FT_Get_Char_Index(face, (unsigned char)*p), FT_LOAD_RENDER);
        
        int glyph_height = slot->bitmap.rows;
        int glyph_width = slot->bitmap.width;
        
        int sx = pen_x + (cell_width - glyph_width) / 2;
        int sy = baseline_y - cell_height + (cell_height - glyph_height) / 2;
        
        if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
            blit_glyph(sx, sy, &slot->bitmap, c);
        }
        
        pen_x += cell_width;
        p++;
    }
}

static void draw_page_background(void) {
    int sx = get_page_start_x();
    int sy = get_page_start_y();
    int w = get_page_content_width();
    int h = get_page_content_height();
    
    blit_rect(sx, sy, w, h, get_color(COLOR_GRAY));
    blit_rect(sx + 4, sy + 4, w - 8, h - 8, get_color(COLOR_PAGE));
}

static void draw_margins(void) {
    Color mc = get_color(COLOR_RED);
    int top_y = get_page_start_y();
    int bottom_y = display.height - STATUS_BAR_HEIGHT;
    blit_line_v(display.margin_x - 2, top_y, bottom_y, mc);
    blit_line_v(display.width - display.margin_x + 1, top_y, bottom_y, mc);
}

static void draw_char(int line_idx, int char_idx) {
    if (line_idx < 0 || line_idx >= doc.total_lines) return;
    if (char_idx < 0 || char_idx >= doc.lines[line_idx].char_count) return;
    
    int cw = get_char_width();
    int ch = get_char_height();
    int px = get_page_start_x() + 4 + char_idx * cw;
    int py = get_page_start_y() + 4 + (line_idx - doc.top_line) * ch;
    
    if (py + ch < get_page_start_y() || py >= display.height - STATUS_BAR_HEIGHT) return;
    
    char buf[2] = {doc.lines[line_idx].text[char_idx], 0};
    uint8_t idx = doc.lines[line_idx].ink_idx[char_idx];
    bool is_bold = doc.lines[line_idx].bold[char_idx];
    Color c = get_color(ink_colors[idx]);
    
    blit_text(px, py, buf, c, is_bold);
}

static void draw_cursor(void) {
    int x = get_page_start_x() + 4 + doc.cursor_col * get_char_width();
    int y = get_page_start_y() + 4 + (doc.cursor_line - doc.top_line) * get_char_height();
    int cw = get_char_width();
    int ch = get_char_height();
    
    Color c = get_color(COLOR_BLACK);
    blit_rect(x, y + ch - 3, cw, 3, c);
}

static void draw_help_overlay(void);
static void draw_toast(void);

static void draw_status_bar(void) {
    int y = display.height - STATUS_BAR_HEIGHT;
    
    blit_rect(0, y, display.width, STATUS_BAR_HEIGHT, get_color(COLOR_WHITE));
    blit_line_h(0, y, display.width, get_color(COLOR_BLACK));
    
    char buf[256];
    const char *mode = doc.inverted ? "Dark" : "Light";
    char res_str[32];
    if (doc.res_idx < MAX_RESOLUTIONS - 1) {
        snprintf(res_str, sizeof(res_str), "%dx%d", resolutions[doc.res_idx][0], resolutions[doc.res_idx][1]);
    } else {
        snprintf(res_str, sizeof(res_str), "native");
    }
    const char *bold_str = doc.bold ? "B" : "";
    snprintf(buf, sizeof(buf), "%s | Zoom:%dx | %s | Ink:%s%s | Tab:%d | F1:help",
             res_str, doc.zoom, mode, ink_names[doc.ink_idx], bold_str, doc.tab_width);
    
    blit_text(10, y + 8, buf, get_color(COLOR_BLACK), false);
}

static void render_to_backbuffer(void) {
    blit_clear(get_color(COLOR_BG));
    
    int ch = get_char_height();
    int lpp = get_lines_per_page();
    
    // Calculate which line should be at center of screen
    int cursor_screen_line = lpp / 2;
    
    // Calculate Y offset accumulated from page gaps above top_line
    int page_y_offset = 0;
    int line_y = get_page_start_y();
    
    // Draw pages starting from top_line
    int i = 0;
    while (i < doc.total_lines) {
        // Check if we're starting a new page (explicit break or natural page end)
        bool is_page_start = (i == 0) || doc.lines[i-1].page_break;
        int page_start = i;
        
        // Find end of this page
        int page_end = page_start + lpp;
        for (int j = page_start; j < page_end && j < doc.total_lines; j++) {
            if (doc.lines[j].page_break && j > page_start) {
                page_end = j + 1;
                break;
            }
        }
        if (page_end > doc.total_lines) page_end = doc.total_lines;
        
        // Draw page background
        int page_height = (page_end - page_start) * ch + 8;
        
        // Check if this page should be visible
        if (i + lpp > doc.top_line && i < doc.top_line + lpp + 5) {
            // Draw page background
            int px = get_page_start_x();
            int py = line_y;
            int pw = get_page_content_width();
            int ph = page_height;
            
            blit_rect(px, py, pw, ph, get_color(COLOR_GRAY));
            blit_rect(px + 4, py + 4, pw - 8, ph - 8, get_color(COLOR_PAGE));
            
            // Draw text for this page
            int screen_line = i - doc.top_line;
            if (screen_line < 0) screen_line = 0;
            
            for (int j = page_start; j < page_end; j++) {
                int draw_line = j - doc.top_line;
                if (draw_line >= 0 && draw_line < lpp) {
                    for (int k = 0; k < doc.lines[j].char_count; k++) {
                        draw_char(j, k);
                    }
                }
            }
        }
        
        i = page_end;
        line_y += page_height + PAGE_GAP;
    }
    
    // Ensure top_line keeps cursor visible
    doc.top_line = doc.cursor_line - cursor_screen_line;
    if (doc.top_line < 0) doc.top_line = 0;
    
    draw_margins();
    draw_cursor();
    draw_status_bar();
    
    if (show_help) {
        draw_help_overlay();
    }
    
    if (toast_frames > 0 && toast_msg[0] != '\0') {
        draw_toast();
        toast_frames--;
    }
}

static void flip_buffers(void) {
    memcpy(display.fb_map, display.backbuffer, display.fb_size);
}

static void render(void) {
    if (debug_log) {
        fprintf(debug_log, "Render: line=%d col=%d total_lines=%d top_line=%d\n", 
                doc.cursor_line, doc.cursor_col, doc.total_lines, doc.top_line);
        fflush(debug_log);
    }
    render_to_backbuffer();
    flip_buffers();
}

static int init_framebuffer(void) {
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    display.fb_fd = open("/dev/fb0", O_RDWR);
    if (display.fb_fd < 0) {
        fprintf(stderr, "Cannot open /dev/fb0: %s\n", strerror(errno));
        return -1;
    }
    
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
    
    display.backbuffer = malloc(display.fb_size);
    if (!display.backbuffer) {
        munmap(display.fb_map, display.fb_size);
        close(display.fb_fd);
        return -1;
    }
    
    display.margin_x = 80;
    display.margin_y = 50;
    
    memset(display.fb_map, 0, display.fb_size);
    memset(display.backbuffer, 0, display.fb_size);
    
    printf("Framebuffer: %dx%d stride=%d\n", display.width, display.height, display.stride);
    return 0;
}

static void close_framebuffer(void) {
    if (display.backbuffer) free(display.backbuffer);
    if (display.fb_map) munmap(display.fb_map, display.fb_size);
    if (display.fb_fd >= 0) close(display.fb_fd);
}

static int init_font(const char *path, const char *path_bold) {
    if (FT_Init_FreeType(&font.ft)) return -1;
    if (FT_New_Face(font.ft, path, 0, &font.face)) {
        FT_Done_FreeType(font.ft);
        return -1;
    }
    
    font.face_bold = NULL;
    if (path_bold && FT_New_Face(font.ft, path_bold, 0, &font.face_bold)) {
        font.face_bold = NULL;
    }
    
    int size = DEFAULT_FONT_SIZE * doc.zoom;
    if (FT_Set_Char_Size(font.face, 0, size * 64, 72, 72)) {
        FT_Done_Face(font.face);
        if (font.face_bold) FT_Done_Face(font.face_bold);
        FT_Done_FreeType(font.ft);
        return -1;
    }
    
    if (font.face_bold) {
        FT_Set_Char_Size(font.face_bold, 0, size * 64, 72, 72);
    }
    
    font.slot = font.face->glyph;
    font.height = font.face->size->metrics.height >> 6;
    
    FT_Load_Glyph(font.face, FT_Get_Char_Index(font.face, 'M'), FT_LOAD_RENDER);
    font.char_width = font.slot->advance.x >> 6;
    
    if (debug_log) {
        fprintf(debug_log, "Font loaded: %s size=%d height=%d char_width=%d bold=%s\n", 
                path, size, font.height, font.char_width, font.face_bold ? "yes" : "no");
        fflush(debug_log);
    }
    
    printf("Font: %s at %dpt h=%d cw=%d bold=%s\n", path, size, font.height, font.char_width, 
           font.face_bold ? "yes" : "no");
    return 0;
}

static void update_font_size(void) {
    int size = DEFAULT_FONT_SIZE * doc.zoom;
    FT_Set_Char_Size(font.face, 0, size * 64, 72, 72);
    if (font.face_bold) FT_Set_Char_Size(font.face_bold, 0, size * 64, 72, 72);
    font.height = font.face->size->metrics.height >> 6;
    FT_Load_Glyph(font.face, FT_Get_Char_Index(font.face, 'M'), FT_LOAD_RENDER);
    font.char_width = font.slot->advance.x >> 6;
    printf("Zoom %d: size=%d h=%d cw=%d chars=%d lines=%d\n",
           doc.zoom, size, font.height, font.char_width,
           get_chars_per_line(), get_lines_per_page());
}

static void close_font(void) {
    if (font.face) FT_Done_Face(font.face);
    if (font.face_bold) FT_Done_Face(font.face_bold);
    if (font.ft) FT_Done_FreeType(font.ft);
}

static void init_document(void) {
    memset(&doc, 0, sizeof(doc));
    doc.total_lines = 1;
    doc.ink_idx = 0;
    doc.zoom = 2;
    doc.res_idx = MAX_RESOLUTIONS - 1;
    doc.tab_width = 4;
    doc.dirty = false;
}

static void cycle_tab_width(void) {
    int widths[] = {2, 4, 6, 8};
    int current = 0;
    for (int i = 0; i < 4; i++) {
        if (doc.tab_width == widths[i]) {
            current = i;
            break;
        }
    }
    doc.tab_width = widths[(current + 1) % 4];
    char msg[32];
    snprintf(msg, sizeof(msg), "Tab: %d spaces", doc.tab_width);
    show_toast(msg);
}

static int set_resolution(int w, int h) {
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    if (ioctl(display.fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        printf("Failed to get vinfo\n");
        return -1;
    }
    
    vinfo.xres = w;
    vinfo.yres = h;
    vinfo.xres_virtual = w;
    vinfo.yres_virtual = h;
    vinfo.bits_per_pixel = 32;
    
    if (ioctl(display.fb_fd, FBIOPUT_VSCREENINFO, &vinfo) < 0) {
        printf("Failed to set resolution %dx%d\n", w, h);
        return -1;
    }
    
    if (ioctl(display.fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        return -1;
    }
    
    if (display.backbuffer) free(display.backbuffer);
    if (display.fb_map) munmap(display.fb_map, display.fb_size);
    
    display.width = vinfo.xres;
    display.height = vinfo.yres;
    display.stride = finfo.line_length;
    display.fb_size = finfo.smem_len;
    display.margin_x = 80;
    display.margin_y = 50;
    
    display.fb_map = mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, display.fb_fd, 0);
    if (display.fb_map == MAP_FAILED) {
        return -1;
    }
    
    display.backbuffer = malloc(display.fb_size);
    if (!display.backbuffer) {
        munmap(display.fb_map, display.fb_size);
        return -1;
    }
    
    memset(display.fb_map, 0, display.fb_size);
    memset(display.backbuffer, 0, display.fb_size);
    
    printf("Resolution: %dx%d\n", display.width, display.height);
    return 0;
}

static void cycle_resolution(void) {
    doc.res_idx = (doc.res_idx + 1) % MAX_RESOLUTIONS;
    int w = resolutions[doc.res_idx][0];
    int h = resolutions[doc.res_idx][1];
    
    if (w == 0 || h == 0) {
        doc.res_idx = 0;
        w = resolutions[0][0];
        h = resolutions[0][1];
    }
    
    if (set_resolution(w, h) == 0) {
        doc.dirty = true;
    }
}

static void cycle_ink_color(void) {
    doc.ink_idx = (doc.ink_idx + 1) % 4;
    doc.dirty = true;
    printf("Ink: %s\n", ink_names[doc.ink_idx]);
}

static void toggle_bold(void) {
    doc.bold = !doc.bold;
    show_toast(doc.bold ? "Bold: ON" : "Bold: OFF");
}

static void draw_help_overlay(void) {
    int cw = get_char_width();
    int ch = get_char_height();
    int max_text_width = 30 * cw;
    int w = max_text_width + 60;
    int h = 15 * ch + 40;
    int x = (display.width - w) / 2;
    int y = (display.height - h) / 2;
    
    blit_rect(x, y, w, h, get_color(COLOR_WHITE));
    blit_rect(x + 2, y + 2, w - 4, h - 4, get_color(COLOR_BLACK));
    blit_rect(x + 4, y + 4, w - 8, h - 8, get_color(COLOR_PAGE));
    
    int ty = y + 12;
    int tx = x + 20;
    blit_text(tx, ty, "Typewrite Help", get_color(COLOR_BLACK), false); ty += ch + 5;
    blit_text(tx, ty, "F1: Show/hide this help", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "F2/F3: Zoom out/in (1x/2x)", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "F4: Toggle dark mode", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "F5: Next ink color", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "F6: Change resolution", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "F7: New document", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "F8: Tab width (2/4/6/8)", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "F9: Toggle bold", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "Arrow keys: Move cursor", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "Enter: Carriage return", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "Backspace: Strikethrough", get_color(COLOR_BLACK), false); ty += ch;
    blit_text(tx, ty, "Ctrl+S: Save | Ctrl+Q: Quit", get_color(COLOR_BLACK), false); ty += ch;
}

static void draw_toast(void) {
    if (toast_frames <= 0 || toast_msg[0] == '\0') return;
    
    int len = strlen(toast_msg);
    int tw = len * get_char_width();
    int th = get_char_height() + 16;
    int x = (display.width - tw) / 2 - 20;
    int y = display.height / 2;
    
    if (x < 10) x = 10;
    
    blit_rect(x, y, tw + 40, th, get_color(COLOR_WHITE));
    blit_rect(x + 2, y + 2, tw + 36, th - 4, get_color(COLOR_BLACK));
    blit_rect(x + 4, y + 4, tw + 32, th - 8, get_color(COLOR_PAGE));
    blit_text(x + 20, y + 10, toast_msg, get_color(COLOR_BLACK), false);
}

static void handle_tab(void) {
    int next_tab = ((doc.cursor_col / doc.tab_width) + 1) * doc.tab_width;
    int spaces = next_tab - doc.cursor_col;
    
    Line *line = &doc.lines[doc.cursor_line];
    for (int i = 0; i < spaces && doc.cursor_col < MAX_LINE_LEN - 1; i++) {
        line->text[doc.cursor_col] = ' ';
        line->ink_idx[doc.cursor_col] = doc.ink_idx;
        line->bold[doc.cursor_col] = doc.bold ? 1 : 0;
        if (doc.cursor_col >= line->char_count) {
            line->char_count = doc.cursor_col + 1;
        }
        doc.cursor_col++;
    }
    doc.dirty = true;
}

static void insert_char(char c) {
    if (doc.cursor_line >= MAX_LINES) return;
    Line *line = &doc.lines[doc.cursor_line];
    
    if (doc.cursor_col >= MAX_LINE_LEN - 1) return;
    
    line->text[doc.cursor_col] = c;
    line->ink_idx[doc.cursor_col] = doc.ink_idx;
    line->bold[doc.cursor_col] = doc.bold ? 1 : 0;
    
    if (doc.cursor_col >= line->char_count) {
        line->char_count = doc.cursor_col + 1;
    }
    
    doc.cursor_col++;
    doc.dirty = true;
}

static void handle_backspace(void) {
    if (doc.cursor_col > 0) {
        doc.cursor_col--;
        doc.dirty = true;
    } else if (doc.cursor_line > 0) {
        doc.cursor_line--;
        Line *prev = &doc.lines[doc.cursor_line];
        doc.cursor_col = prev->char_count > 0 ? prev->char_count : 0;
    }
}

static void handle_enter(void) {
    if (doc.cursor_line >= MAX_LINES - 1) return;
    
    doc.cursor_line++;
    doc.cursor_col = 0;
    
    if (doc.cursor_line >= doc.total_lines) {
        doc.total_lines = doc.cursor_line + 1;
        memset(&doc.lines[doc.cursor_line], 0, sizeof(Line));
    }
    doc.dirty = true;
}

static void move_cursor(int dx, int dy) {
    doc.cursor_col += dx;
    doc.cursor_line += dy;
    
    if (doc.cursor_col < 0 && doc.cursor_line > 0) {
        doc.cursor_line--;
        doc.cursor_col = doc.lines[doc.cursor_line].char_count;
    }
    if (doc.cursor_col < 0) doc.cursor_col = 0;
    if (doc.cursor_col > doc.lines[doc.cursor_line].char_count) {
        doc.cursor_col = doc.lines[doc.cursor_line].char_count;
    }
    if (doc.cursor_line < 0) doc.cursor_line = 0;
    if (doc.cursor_line >= doc.total_lines) {
        doc.cursor_line = doc.total_lines - 1;
        if (doc.cursor_line < 0) doc.cursor_line = 0;
    }
    
    int lpp = get_lines_per_page();
    while (doc.top_line > doc.cursor_line) doc.top_line--;
    while (doc.top_line + lpp <= doc.cursor_line) doc.top_line++;
    
    if (doc.cursor_line >= MAX_LINES) doc.cursor_line = MAX_LINES - 1;
    if (doc.total_lines < doc.cursor_line + 1) doc.total_lines = doc.cursor_line + 1;
}

static char current_filename[256] = "/root/document.md";

static char ink_filename[256];

static void save_document(void) {
    FILE *f = fopen(current_filename, "w");
    if (!f) return;
    
    for (int i = 0; i < doc.total_lines; i++) {
        for (int j = 0; j < doc.lines[i].char_count; j++) {
            fputc(doc.lines[i].text[j], f);
        }
        if (i < doc.total_lines - 1) fputc('\n', f);
    }
    fclose(f);
    
    snprintf(ink_filename, sizeof(ink_filename), "%s", current_filename);
    char *dot = strrchr(ink_filename, '.');
    if (dot) *dot = '\0';
    strcat(ink_filename, ".ink");
    
    FILE *inkf = fopen(ink_filename, "w");
    if (inkf) {
        for (int i = 0; i < doc.total_lines; i++) {
            for (int j = 0; j < doc.lines[i].char_count; j++) {
                fprintf(inkf, "%d,%d", doc.lines[i].ink_idx[j], doc.lines[i].bold[j]);
                if (j < doc.lines[i].char_count - 1) fputc(' ', inkf);
            }
            fputc('\n', inkf);
        }
        fclose(inkf);
    }
    
    sync();
    doc.dirty = false;
    show_toast("Saved");
}

static void new_document(void) {
    if (doc.dirty) save_document();
    
    memset(&doc, 0, sizeof(doc));
    doc.total_lines = 1;
    doc.ink_idx = 0;
    doc.zoom = 2;
    doc.res_idx = MAX_RESOLUTIONS - 1;
    doc.dirty = false;
    
    int num = 1;
    char *dot = strrchr(current_filename, '.');
    if (dot) {
        *dot = '\0';
        char *base = strrchr(current_filename, '/');
        if (base) base++; else base = current_filename;
        if (strncmp(base, "document", 8) == 0) {
            num = atoi(base + 8);
            if (num < 1) num = 1;
            num++;
        }
        *dot = '.';
    }
    
    snprintf(current_filename, sizeof(current_filename), "/root/document%d.md", num);
    show_toast("New document");
}

static void load_document(void) {
    FILE *f = fopen(current_filename, "r");
    if (!f) return;
    
    char buf[MAX_LINE_LEN * 4];
    int line = 0;
    
    while (fgets(buf, sizeof(buf), f) && line < MAX_LINES) {
        int len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[--len] = 0;
        
        int col = 0;
        for (int i = 0; i < len && col < MAX_LINE_LEN - 1; i++) {
            doc.lines[line].text[col] = buf[i];
            doc.lines[line].ink_idx[col] = 0;
            col++;
        }
        doc.lines[line].char_count = col;
        line++;
    }
    
    doc.total_lines = line > 0 ? line : 1;
    doc.cursor_line = 0;
    doc.cursor_col = 0;
    doc.top_line = 0;
    
    fclose(f);
    
    snprintf(ink_filename, sizeof(ink_filename), "%s", current_filename);
    char *dot = strrchr(ink_filename, '.');
    if (dot) *dot = '\0';
    strcat(ink_filename, ".ink");
    
    FILE *inkf = fopen(ink_filename, "r");
    if (inkf) {
        int ink_line = 0;
        while (fgets(buf, sizeof(buf), inkf) && ink_line < doc.total_lines) {
            char *p = buf;
            int col = 0;
            while (*p && *p != '\n' && col < doc.lines[ink_line].char_count) {
                int ink = 0, bold = 0;
                if (sscanf(p, "%d,%d", &ink, &bold) >= 1) {
                    doc.lines[ink_line].ink_idx[col] = ink & 3;
                    doc.lines[ink_line].bold[col] = bold ? 1 : 0;
                    col++;
                }
                while (*p && *p != ' ' && *p != '\n') p++;
                if (*p == ' ') p++;
            }
            ink_line++;
        }
        fclose(inkf);
    }
    
    printf("Loaded: %s\n", current_filename);
}

static const char *font_paths[] = {
    "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeMono.ttf"
};

static const char *font_paths_bold[] = {
    "/usr/share/fonts/dejavu/DejaVuSansMono-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Bold.ttf",
    "/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf"
};

int main(int argc, char *argv[]) {
    printf("=== Typewrite ===\n");
    
    debug_log = fopen("/tmp/typewrite_debug.log", "w");
    if (debug_log) {
        fprintf(debug_log, "=== Typewrite Debug Log ===\n");
        fflush(debug_log);
    }
    
    init_ink_colors();
    init_document();
    
    if (init_framebuffer() != 0) {
        fprintf(stderr, "No display!\n");
        return 1;
    }
    
    if (set_graphics_mode() != 0) {
        fprintf(stderr, "Warning: Could not set graphics mode\n");
    }
    
    int font_loaded = 0;
    for (size_t i = 0; i < sizeof(font_paths) / sizeof(font_paths[0]); i++) {
        if (init_font(font_paths[i], font_paths_bold[i]) == 0) {
            font_loaded = 1;
            break;
        }
    }
    if (!font_loaded) {
        fprintf(stderr, "No fonts!\n");
        close_framebuffer();
        return 1;
    }
    
    update_font_size();
    load_document();
    
    struct termios old_tty;
    tcgetattr(STDIN_FILENO, &old_tty);
    struct termios new_tty = old_tty;
    cfmakeraw(&new_tty);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tty);
    
    render();
    printf("Ready! F1:help F2:zoom- F3:zoom+ F4:dark F5:ink | Ctrl+S:save Ctrl+Q:quit\n");
    
    unsigned char buf[64];
    int running = 1;
    
    while (running) {
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) continue;
        
        if (debug_log) {
            fprintf(debug_log, "Read %zd bytes: ", n);
            for (ssize_t j = 0; j < n && j < 10; j++) fprintf(debug_log, "%02x ", buf[j]);
            fprintf(debug_log, "\n");
            fflush(debug_log);
        }
        
        int i = 0;
        while (i < n) {
            unsigned char c = buf[i];
            int do_render = 1;
            i++;
            
            if (c == 27 && i < n) {
                unsigned char next = buf[i];
                if (next == '[') {
                    i++;
                    if (i < n) {
                        unsigned char code = buf[i];
                        i++;
                        if (code == '[') {
                            if (i < n) {
                                unsigned char fcode = buf[i];
                                i++;
                                switch (fcode) {
                                    case 'A': show_help = !show_help; break;           // F1
                                    case 'B': if (doc.zoom > 1) { doc.zoom--; update_font_size(); } break;  // F2
                                    case 'C': if (doc.zoom < 2) { doc.zoom++; update_font_size(); } break;  // F3
                                    case 'D': doc.inverted = !doc.inverted; break;     // F4
                                    case 'E': cycle_ink_color(); break;                // F5
                                    case 'F': cycle_resolution(); break;               // F6
                                    case 'G': new_document(); break;                   // F7
                                    default: show_toast("F key not assigned"); break;
                                }
                            }
                        } else {
                            switch (code) {
                                case 'A': move_cursor(0, -1); break;
                                case 'B': move_cursor(0, 1); break;
                                case 'C': move_cursor(1, 0); break;
                                case 'D': move_cursor(-1, 0); break;
                                case '1':
                                    if (i < n && buf[i] == '~') { i++; show_help = !show_help; }
                                    else if (i < n && buf[i] == '7' && i+1 < n && buf[i+1] == '~') { i += 2; cycle_resolution(); }
                                    else if (i < n && buf[i] == '8' && i+1 < n && buf[i+1] == '~') { i += 2; new_document(); }
                                    else if (i < n && buf[i] == '9' && i+1 < n && buf[i+1] == '~') { i += 2; cycle_tab_width(); }
                                    break;
                                case '2':
                                    if (i < n && buf[i] == '~') { i++; if (doc.zoom > 1) { doc.zoom--; update_font_size(); } }
                                    else if (i < n && buf[i] == '0' && i+1 < n && buf[i+1] == '~') { i += 2; toggle_bold(); }
                                    else if (i < n && buf[i] == '1' && i+1 < n && buf[i+1] == '~') { i += 2; show_toast("F10 not assigned"); }
                                    break;
                                case '3':
                                    if (i < n && buf[i] == '~') { i++; if (doc.zoom < 2) { doc.zoom++; update_font_size(); } }
                                    else if (i < n && buf[i] == '3' && i+1 < n && buf[i+1] == '~') { i += 2; show_toast("F11 not assigned"); }
                                    else if (i < n && buf[i] == '4' && i+1 < n && buf[i+1] == '~') { i += 2; show_toast("F12 not assigned"); }
                                    break;
                                case '4':
                                    if (i < n && buf[i] == '~') { i++; doc.inverted = !doc.inverted; }
                                    break;
                                case '5':
                                    if (i < n && buf[i] == '~') { i++; cycle_ink_color(); }
                                    break;
                                case '6':
                                    if (i < n && buf[i] == '~') { i++; show_toast("F6: Use F6 for resolution"); }
                                    break;
                            }
                        }
                    }
                } else if (next == 'O') {
                    i++;
                    if (i < n) {
                        unsigned char code = buf[i];
                        i++;
                        switch (code) {
                            case 'P': show_help = !show_help; break;
                            case 'Q': if (doc.zoom > 1) { doc.zoom--; update_font_size(); } break;
                            case 'R': if (doc.zoom < 2) { doc.zoom++; update_font_size(); } break;
                            case 'S': doc.inverted = !doc.inverted; break;
                            case 'T': cycle_ink_color(); break;
                            case 'U': cycle_resolution(); break;
                            case 'V': new_document(); break;
                            case 'W': cycle_tab_width(); break;
                            case 'X': toggle_bold(); break;
                            default: show_toast("F key not assigned"); break;
                        }
                    }
                }
            } else if (c == 17) {
                running = 0;
                do_render = 0;
            } else if (c == 19) {
                show_toast("Saving...");
                render();
                save_document();
                do_render = 1;
            } else if (c == 127 || c == 8) {
                handle_backspace();
            } else if (c == '\n' || c == '\r') {
                handle_enter();
            } else if (c == '\t') {
                handle_tab();
            } else if (c >= 32 && c <= 126) {
                if (debug_log) { fprintf(debug_log, "Inserting char '%c' at line %d col %d\n", c, doc.cursor_line, doc.cursor_col); fflush(debug_log); }
                insert_char(c);
            } else {
                do_render = 0;
            }
            
            if (do_render && running) render();
        }
    }
    
    if (doc.dirty) save_document();
    
    if (debug_log) {
        fprintf(debug_log, "Exiting\n");
        fclose(debug_log);
    }
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_tty);
    restore_console();
    close_font();
    close_framebuffer();
    
    printf("Goodbye!\n");
    return 0;
}
