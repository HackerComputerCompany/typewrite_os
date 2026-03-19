#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>

#define MAX_LINES 100
#define LINE_LEN 160
#define COLS 80

char screen[MAX_LINES][LINE_LEN];
int cursor_line = 0;
int cursor_col = 0;
int total_lines = 1;

void clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void refresh_screen() {
    write(STDOUT_FILENO, "\x1b[H", 3);
    for (int i = 0; i < total_lines && i < 24; i++) {
        write(STDOUT_FILENO, screen[i], strlen(screen[i]));
        write(STDOUT_FILENO, "\r\n", 2);
    }
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cursor_line + 1, cursor_col + 1);
    write(STDOUT_FILENO, buf, len);
}

void scroll_up() {
    for (int i = 0; i < MAX_LINES - 1; i++) {
        memcpy(screen[i], screen[i + 1], LINE_LEN);
    }
    memset(screen[MAX_LINES - 1], 0, LINE_LEN);
    if (cursor_line > 0) cursor_line--;
}

void type_char(char c) {
    if (c == '\n' || c == '\r') {
        cursor_line++;
        cursor_col = 0;
        if (cursor_line >= MAX_LINES) scroll_up();
        if (cursor_line >= total_lines) total_lines = cursor_line + 1;
    } else if (c >= 32 && c <= 126) {
        if (cursor_col < COLS - 1) {
            screen[cursor_line][cursor_col++] = c;
            screen[cursor_line][cursor_col] = 0;
        }
    }
    refresh_screen();
}

void backspace() {
    if (cursor_col > 0) {
        cursor_col--;
        screen[cursor_line][cursor_col] = 0;
    } else if (cursor_line > 0) {
        cursor_line--;
        cursor_col = strlen(screen[cursor_line]);
    }
    refresh_screen();
}

void save_document() {
    FILE *f = fopen("/root/document.txt", "w");
    if (f) {
        for (int i = 0; i < total_lines; i++) {
            fprintf(f, "%s\n", screen[i]);
        }
        fclose(f);
        write(STDOUT_FILENO, "\r\n[Saved to /root/document.txt]\r\n", 30);
    }
}

void draw_border() {
    memset(screen, 0, sizeof(screen));
    snprintf(screen[0], LINE_LEN, "+%-78s+", "");
    for (int i = 1; i < 23; i++) {
        snprintf(screen[i], LINE_LEN, "|%-78s|", "");
    }
    snprintf(screen[23], LINE_LEN, "+%-78s+", "");
    total_lines = 24;
    cursor_line = 1;
    cursor_col = 1;
    refresh_screen();
}

int main() {
    struct termios old_tty, new_tty;
    
    tcgetattr(STDIN_FILENO, &old_tty);
    new_tty = old_tty;
    new_tty.c_lflag &= ~(ICANON | ECHO);
    new_tty.c_cc[VMIN] = 0;
    new_tty.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_tty);
    
    write(STDOUT_FILENO, "\x1b[?25l", 6);
    draw_border();
    
    char buf[128];
    snprintf(buf, sizeof(buf), " Typewrite OS - Characters: 0 | Ctrl+S save | Ctrl+Q quit ");
    strncpy(screen[23], buf, 78);
    
    fd_set readfds;
    int running = 1;
    
    while (running) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval tv = {0, 50000};
        
        if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0) {
                if (c == 19) {
                    save_document();
                } else if (c == 17 || c == 3) {
                    running = 0;
                } else if (c == 127 || c == 8) {
                    backspace();
                } else if (c >= 32) {
                    type_char(c);
                }
            }
        }
    }
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_tty);
    write(STDOUT_FILENO, "\x1b[?25h\x1b[2J\x1b[H", 12);
    
    return 0;
}
