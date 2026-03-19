#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_LINES 500
#define MAX_LINE_LEN 200

typedef struct {
    char text[MAX_LINE_LEN];
    int char_count;
    bool strikethrough[MAX_LINE_LEN];
} Line;

typedef struct {
    Line lines[MAX_LINES];
    int total_lines;
    int cursor_line;
    int cursor_col;
    int top_line;
    SDL_Color ink_color;
    SDL_Color ink_colors[3];
    int ink_idx;
    int scale;
    int margin_left;
    int margin_right;
    int margin_top;
    int ruler_height;
    bool inverted;
    SDL_Color bg_color;
    SDL_Color fg_color;
    SDL_Color page_color;
    SDL_Color gray_color;
    SDL_Color red_color;
} Document;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static TTF_Font *font = NULL;
static TTF_Font *small_font = NULL;
static Document doc;

void init_colors() {
    doc.ink_colors[0] = (SDL_Color){0, 0, 0, 255};      // Black
    doc.ink_colors[1] = (SDL_Color){180, 30, 30, 255};   // Red
    doc.ink_colors[2] = (SDL_Color){30, 60, 180, 255};   // Blue
    doc.ink_idx = 0;
    doc.ink_color = doc.ink_colors[0];
}

void update_theme_colors() {
    if (doc.inverted) {
        doc.bg_color = (SDL_Color){30, 30, 30, 255};
        doc.fg_color = (SDL_Color){220, 220, 220, 255};
        doc.page_color = (SDL_Color){50, 50, 50, 255};
        doc.gray_color = (SDL_Color){80, 80, 80, 255};
        doc.red_color = (SDL_Color){255, 100, 100, 255};
    } else {
        doc.bg_color = (SDL_Color){220, 220, 220, 255};
        doc.fg_color = (SDL_Color){20, 20, 20, 255};
        doc.page_color = (SDL_Color){250, 250, 250, 255};
        doc.gray_color = (SDL_Color){200, 200, 200, 255};
        doc.red_color = (SDL_Color){200, 50, 50, 255};
    }
}

int get_font_size() {
    return 16 * doc.scale;
}

int get_char_width() {
    return 8 * doc.scale;
}

int get_char_height() {
    return 16 * doc.scale;
}

int get_chars_per_line() {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    return (w - doc.margin_left - doc.margin_right - 20) / get_char_width();
}

int get_lines_per_page() {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    return (h - doc.margin_top - doc.ruler_height - 40 - 30) / get_char_height();
}

int get_page_start_x() {
    return doc.margin_left + 10;
}

int get_page_start_y() {
    return doc.margin_top + doc.ruler_height + 10;
}

void init_document() {
    memset(&doc, 0, sizeof(doc));
    doc.total_lines = 1;
    doc.scale = 2;
    doc.margin_left = 60;
    doc.margin_right = 40;
    doc.margin_top = 30;
    doc.ruler_height = 25;
    init_colors();
    update_theme_colors();
}

int init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return -1;
    }
    
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF init failed: %s\n", TTF_GetError());
        return -1;
    }
    
    window = SDL_CreateWindow("Typewrite OS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_RESIZABLE);
    
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return -1;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) {
            fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
            return -1;
        }
    }
    
    font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", get_font_size());
    small_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 10);
    
    if (!font) {
        fprintf(stderr, "Font loading failed, trying fallback\n");
        font = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf", get_font_size());
        if (!font) {
            font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeMono.ttf", get_font_size());
            if (!font) {
                fprintf(stderr, "No fonts found\n");
                return -1;
            }
        }
        if (!small_font) {
            small_font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeMono.ttf", 10);
        }
    }
    
    if (!small_font) {
        small_font = font;
    }
    
    return 0;
}

void close_sdl() {
    if (font && font != small_font) TTF_CloseFont(font);
    if (small_font) TTF_CloseFont(small_font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

void draw_text(int x, int y, const char *text, SDL_Color color) {
    if (!font || !text) return;
    
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;
    
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) return;
    
    SDL_Rect dest = {x, y, 0, 0};
    SDL_QueryTexture(texture, NULL, NULL, &dest.w, &dest.h);
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
}

void draw_ruler() {
    int start_x = get_page_start_x();
    int end_x = start_x + get_chars_per_line() * get_char_width();
    int y = doc.margin_top;
    int height = doc.ruler_height;
    
    SDL_SetRenderDrawColor(renderer, doc.bg_color.r, doc.bg_color.g, doc.bg_color.b, 255);
    SDL_Rect ruler_bg = {start_x, y, end_x - start_x, height};
    SDL_RenderFillRect(renderer, &ruler_bg);
    
    SDL_SetRenderDrawColor(renderer, doc.fg_color.r, doc.fg_color.g, doc.fg_color.b, 255);
    SDL_RenderDrawLine(renderer, start_x, y + height - 1, end_x, y + height - 1);
    
    char buf[16];
    for (int i = 0; i <= get_chars_per_line(); i += 5) {
        int x = start_x + i * get_char_width();
        int tick_h = (i % 10 == 0) ? 10 : (i % 5 == 0) ? 6 : 3;
        SDL_RenderDrawLine(renderer, x, y + height - tick_h, x, y + height);
        
        if (i % 10 == 0) {
            snprintf(buf, sizeof(buf), "%d", i);
            SDL_Surface *s = TTF_RenderText_Blended(small_font, buf, doc.fg_color);
            if (s) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_Rect r = {x, y + 2, s->w, s->h};
                SDL_RenderCopy(renderer, t, NULL, &r);
                SDL_FreeSurface(s);
                SDL_DestroyTexture(t);
            }
        }
    }
}

void draw_margins() {
    SDL_SetRenderDrawColor(renderer, doc.red_color.r, doc.red_color.g, doc.red_color.b, 255);
    SDL_RenderDrawLine(renderer, doc.margin_left, doc.margin_top, doc.margin_left, 600);
    SDL_RenderDrawLine(renderer, 800 - doc.margin_right, doc.margin_top, 800 - doc.margin_right, 600);
}

void draw_page() {
    int start_x = get_page_start_x();
    int start_y = get_page_start_y();
    int w = 800 - start_x - doc.margin_right - 10;
    int h = 600 - start_y - 30;
    
    SDL_SetRenderDrawColor(renderer, doc.gray_color.r, doc.gray_color.g, doc.gray_color.b, 255);
    SDL_Rect page_shadow = {start_x + 5, start_y + 5, w, h};
    SDL_RenderFillRect(renderer, &page_shadow);
    
    SDL_SetRenderDrawColor(renderer, doc.page_color.r, doc.page_color.g, doc.page_color.b, 255);
    SDL_Rect page = {start_x, start_y, w, h};
    SDL_RenderFillRect(renderer, &page);
}

void draw_text_content() {
    int start_x = get_page_start_x();
    int start_y = get_page_start_y();
    int cw = get_char_width();
    int ch = get_char_height();
    
    for (int line = doc.top_line; line < doc.total_lines && line - doc.top_line < get_lines_per_page(); line++) {
        for (int j = 0; j < doc.lines[line].char_count; j++) {
            char buf[2] = {doc.lines[line].text[j], 0};
            int x = start_x + j * cw;
            int y = start_y + (line - doc.top_line) * ch;
            
            SDL_Color ink = doc.ink_color;
            if (doc.inverted) {
                ink.r = 255 - ink.r;
                ink.g = 255 - ink.g;
                ink.b = 255 - ink.b;
            }
            
            SDL_Surface *s = TTF_RenderText_Blended(font, buf, ink);
            if (s) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_Rect r = {x, y, s->w, s->h};
                SDL_RenderCopy(renderer, t, NULL, &r);
                SDL_FreeSurface(s);
                SDL_DestroyTexture(t);
            }
            
            if (doc.lines[line].strikethrough[j]) {
                SDL_SetRenderDrawColor(renderer, ink.r, ink.g, ink.b, 255);
                SDL_RenderDrawLine(renderer, x, y + ch/2, x + cw, y + ch/2);
            }
        }
    }
}

void draw_cursor() {
    int x = get_page_start_x() + doc.cursor_col * get_char_width();
    int y = get_page_start_y() + (doc.cursor_line - doc.top_line) * get_char_height();
    int w = get_char_width();
    int h = get_char_height();
    
    SDL_Color cursor_color = doc.inverted ? (SDL_Color){220, 220, 220, 255} : (SDL_Color){0, 0, 0, 255};
    SDL_SetRenderDrawColor(renderer, cursor_color.r, cursor_color.g, cursor_color.b, 255);
    SDL_Rect cursor = {x, y, w, h};
    SDL_RenderFillRect(renderer, &cursor);
}

void draw_status_bar() {
    int y = 600 - 25;
    
    SDL_SetRenderDrawColor(renderer, doc.bg_color.r, doc.bg_color.g, doc.bg_color.b, 255);
    SDL_Rect bar = {0, y, 800, 25};
    SDL_RenderFillRect(renderer, &bar);
    
    SDL_SetRenderDrawColor(renderer, doc.fg_color.r, doc.fg_color.g, doc.fg_color.b, 255);
    SDL_RenderDrawLine(renderer, 0, y, 800, y);
    
    const char *ink_names[] = {"Black", "Red", "Blue"};
    const char *mode_names[] = {"Light", "Dark"};
    
    int chars = 0;
    for (int i = 0; i < doc.total_lines; i++) {
        chars += doc.lines[i].char_count;
    }
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Scale:%dx | %s | Ink:%s | Lines:%d | F1:color F2:smaller F3:larger F4:mode | Ctrl+S:save",
             doc.scale, mode_names[doc.inverted], ink_names[doc.ink_idx], doc.total_lines);
    
    draw_text(10, y + 5, buf, doc.fg_color);
}

void render() {
    SDL_SetRenderDrawColor(renderer, doc.bg_color.r, doc.bg_color.g, doc.bg_color.b, 255);
    SDL_RenderClear(renderer);
    
    draw_ruler();
    draw_margins();
    draw_page();
    draw_text_content();
    draw_cursor();
    draw_status_bar();
    
    SDL_RenderPresent(renderer);
}

void cycle_ink_color() {
    doc.ink_idx = (doc.ink_idx + 1) % 3;
    doc.ink_color = doc.ink_colors[doc.ink_idx];
}

void insert_char(char c) {
    if (doc.cursor_line >= MAX_LINES) return;
    Line *line = &doc.lines[doc.cursor_line];
    if (line->char_count >= MAX_LINE_LEN - 1) return;
    
    for (int i = line->char_count; i > doc.cursor_col; i--) {
        line->text[i] = line->text[i - 1];
        line->strikethrough[i] = line->strikethrough[i - 1];
    }
    line->text[doc.cursor_col] = c;
    line->strikethrough[doc.cursor_col] = false;
    line->char_count++;
    doc.cursor_col++;
}

void handle_backspace() {
    Line *line = &doc.lines[doc.cursor_line];
    
    if (doc.cursor_col > 0) {
        doc.cursor_col--;
        if (line->strikethrough[doc.cursor_col]) {
            for (int i = doc.cursor_col; i < line->char_count - 1; i++) {
                line->text[i] = line->text[i + 1];
                line->strikethrough[i] = line->strikethrough[i + 1];
            }
            line->char_count--;
        } else {
            line->strikethrough[doc.cursor_col] = true;
        }
    } else if (doc.cursor_line > 0) {
        doc.cursor_line--;
        doc.cursor_col = doc.lines[doc.cursor_line].char_count;
        handle_backspace();
    }
}

void handle_enter() {
    if (doc.cursor_line >= MAX_LINES - 1) return;
    
    Line *current = &doc.lines[doc.cursor_line];
    Line *next = &doc.lines[doc.cursor_line + 1];
    
    for (int i = doc.cursor_col; i < current->char_count; i++) {
        next->text[i - doc.cursor_col] = current->text[i];
        next->strikethrough[i - doc.cursor_col] = current->strikethrough[i];
    }
    next->char_count = current->char_count - doc.cursor_col;
    current->char_count = doc.cursor_col;
    
    for (int i = doc.cursor_col; i < MAX_LINE_LEN; i++) {
        current->text[i] = 0;
        current->strikethrough[i] = false;
    }
    
    doc.cursor_line++;
    doc.cursor_col = 0;
    if (doc.cursor_line >= doc.total_lines) {
        doc.total_lines = doc.cursor_line + 1;
    }
}

void word_wrap() {
    int cpl = get_chars_per_line();
    
    for (int line = 0; line < doc.total_lines; line++) {
        while (doc.lines[line].char_count > cpl) {
            int break_point = cpl;
            while (break_point > 0 && doc.lines[line].text[break_point] != ' ') {
                break_point--;
            }
            
            if (break_point == 0) break_point = cpl;
            
            int next_line = line + 1;
            if (next_line >= MAX_LINES) break;
            
            if (next_line >= doc.total_lines) {
                doc.total_lines = next_line + 1;
            }
            
            for (int i = break_point + 1; i < doc.lines[line].char_count; i++) {
                int dest = doc.lines[next_line].char_count++;
                if (dest < MAX_LINE_LEN - 1) {
                    doc.lines[next_line].text[dest] = doc.lines[line].text[i];
                    doc.lines[next_line].strikethrough[dest] = doc.lines[line].strikethrough[i];
                }
            }
            
            doc.lines[line].char_count = break_point;
            if (doc.lines[line].text[break_point] == ' ') {
                doc.lines[line].char_count = break_point;
            }
        }
    }
}

void move_cursor(int dx, int dy) {
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
    
    while (doc.top_line > doc.cursor_line) doc.top_line--;
    while (doc.top_line + get_lines_per_page() <= doc.cursor_line) doc.top_line++;
    
    if (doc.cursor_line >= MAX_LINES) doc.cursor_line = MAX_LINES - 1;
    if (doc.total_lines < doc.cursor_line + 1) {
        doc.total_lines = doc.cursor_line + 1;
    }
}

void save_document() {
    FILE *f = fopen("/root/document.txt", "w");
    if (f) {
        for (int i = 0; i < doc.total_lines; i++) {
            for (int j = 0; j < doc.lines[i].char_count; j++) {
                if (!doc.lines[i].strikethrough[j]) {
                    fputc(doc.lines[i].text[j], f);
                }
            }
            if (i < doc.total_lines - 1) fputc('\n', f);
        }
        fclose(f);
    }
}

int main() {
    if (init_sdl() == -1) {
        fprintf(stderr, "Failed to initialize SDL\n");
        return 1;
    }
    
    init_document();
    
    SDL_Event event;
    bool running = true;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_q:
                        if (event.key.keysym.mod & KMOD_CTRL) {
                            running = false;
                        }
                        break;
                    case SDLK_s:
                        if (event.key.keysym.mod & KMOD_CTRL) {
                            save_document();
                        }
                        break;
                    case SDLK_F1:
                        cycle_ink_color();
                        break;
                    case SDLK_F2:
                        if (doc.scale > 1) {
                            doc.scale--;
                            TTF_CloseFont(font);
                            font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", get_font_size());
                            if (!font) font = small_font;
                        }
                        break;
                    case SDLK_F3:
                        if (doc.scale < 5) {
                            doc.scale++;
                            TTF_CloseFont(font);
                            font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", get_font_size());
                            if (!font) font = small_font;
                        }
                        break;
                    case SDLK_F4:
                        doc.inverted = !doc.inverted;
                        update_theme_colors();
                        break;
                    case SDLK_BACKSPACE:
                    case SDLK_DELETE:
                        handle_backspace();
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        handle_enter();
                        word_wrap();
                        break;
                    case SDLK_LEFT:
                        move_cursor(-1, 0);
                        break;
                    case SDLK_RIGHT:
                        move_cursor(1, 0);
                        break;
                    case SDLK_UP:
                        move_cursor(0, -1);
                        break;
                    case SDLK_DOWN:
                        move_cursor(0, 1);
                        break;
                    default:
                        if (event.key.keysym.sym >= SDLK_SPACE && 
                            event.key.keysym.sym <= SDLK_z &&
                            !(event.key.keysym.mod & (KMOD_CTRL | KMOD_ALT))) {
                            char c = (char)event.key.keysym.sym;
                            if (c >= 32 && c <= 126) {
                                insert_char(c);
                                word_wrap();
                            }
                        }
                        break;
                }
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    SDL_SetWindowSize(window, event.window.data1, event.window.data2);
                }
            }
        }
        
        render();
        SDL_Delay(16);
    }
    
    close_sdl();
    return 0;
}
