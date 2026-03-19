#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

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
    bool inverted;
} Document;

static Document doc;

void init_doc(void) {
    memset(&doc, 0, sizeof(doc));
    doc.total_lines = 1;
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

void handle_backspace(void) {
    Line *line = &doc.lines[doc.cursor_line];
    
    if (doc.cursor_col > 0) {
        int pos = doc.cursor_col - 1;
        if (line->strikethrough[pos]) {
            for (int i = pos; i < line->char_count - 1; i++) {
                line->text[i] = line->text[i + 1];
                line->strikethrough[i] = line->strikethrough[i + 1];
            }
            line->char_count--;
        } else {
            line->strikethrough[pos] = true;
            doc.cursor_col = pos + 1;
        }
    } else if (doc.cursor_line > 0) {
        doc.cursor_line--;
        doc.cursor_col = doc.lines[doc.cursor_line].char_count;
        handle_backspace();
    }
}

void handle_enter(void) {
    if (doc.cursor_line >= MAX_LINES - 1) return;
    
    Line *current = &doc.lines[doc.cursor_line];
    Line *next = &doc.lines[doc.cursor_line + 1];
    
    for (int i = doc.cursor_col; i < current->char_count; i++) {
        next->text[i - doc.cursor_col] = current->text[i];
        next->strikethrough[i - doc.cursor_col] = current->strikethrough[i];
    }
    next->char_count = current->char_count - doc.cursor_col;
    current->char_count = doc.cursor_col;
    
    memset(&current->text[current->char_count], 0, MAX_LINE_LEN - current->char_count);
    memset(&current->strikethrough[current->char_count], 0, MAX_LINE_LEN - current->char_count);
    
    doc.cursor_line++;
    doc.cursor_col = 0;
    if (doc.cursor_line >= doc.total_lines) doc.total_lines = doc.cursor_line + 1;
}

void word_wrap(int chars_per_line) {
    if (chars_per_line <= 0) return;
    
    for (int line = 0; line < doc.total_lines; line++) {
        while (doc.lines[line].char_count > chars_per_line) {
            int bp = chars_per_line;
            while (bp > 0 && doc.lines[line].text[bp] != ' ') bp--;
            if (bp == 0) bp = chars_per_line;
            
            int next_line = line + 1;
            if (next_line >= MAX_LINES) break;
            if (next_line >= doc.total_lines) doc.total_lines = next_line + 1;
            
            for (int i = bp + 1; i < doc.lines[line].char_count; i++) {
                int dest = doc.lines[next_line].char_count++;
                if (dest < MAX_LINE_LEN - 1) {
                    doc.lines[next_line].text[dest] = doc.lines[line].text[i];
                    doc.lines[next_line].strikethrough[dest] = doc.lines[line].strikethrough[i];
                }
            }
            
            doc.lines[line].char_count = bp;
            if (doc.lines[line].text[bp] == ' ') doc.lines[line].char_count = bp;
        }
    }
}

void save_to_string(char *buf, size_t bufsize) {
    size_t pos = 0;
    buf[0] = '\0';
    
    for (int i = 0; i < doc.total_lines && pos < bufsize - 1; i++) {
        for (int j = 0; j < doc.lines[i].char_count && pos < bufsize - 1; j++) {
            if (j > 0 && !doc.lines[i].strikethrough[j - 1] && doc.lines[i].strikethrough[j]) {
                if (pos + 2 < bufsize) { strcat(buf, "~~"); pos += 2; }
            }
            if (pos < bufsize - 1) {
                buf[pos++] = doc.lines[i].text[j];
                buf[pos] = '\0';
            }
            if (doc.lines[i].strikethrough[j] && (j == doc.lines[i].char_count - 1 || !doc.lines[i].strikethrough[j + 1])) {
                if (pos + 2 < bufsize) { strcat(buf, "~~"); pos += 2; }
            }
        }
        if (i < doc.total_lines - 1 && pos < bufsize - 1) {
            buf[pos++] = '\n';
            buf[pos] = '\0';
        }
    }
}

void load_from_string(const char *str) {
    memset(&doc, 0, sizeof(doc));
    doc.total_lines = 1;
    doc.cursor_line = 0;
    doc.cursor_col = 0;
    
    int line = 0;
    int col = 0;
    int i = 0;
    bool in_strike = false;
    
    while (str[i] && line < MAX_LINES) {
        if (str[i] == '\n') {
            line++;
            col = 0;
            if (line >= doc.total_lines) doc.total_lines = line + 1;
            i++;
            continue;
        }
        
        if (i < (int)strlen(str) - 1 && str[i] == '~' && str[i + 1] == '~') {
            in_strike = !in_strike;
            i += 2;
            continue;
        }
        
        if (col < MAX_LINE_LEN - 1) {
            doc.lines[line].text[col] = str[i];
            doc.lines[line].strikethrough[col] = in_strike;
            col++;
        }
        i++;
    }
    
    for (int l = 0; l <= line; l++) {
        doc.lines[l].char_count = strlen(doc.lines[l].text);
    }
    doc.total_lines = line > 0 ? line + 1 : 1;
}

void test_insert_char(void) {
    printf("Test: insert_char... ");
    init_doc();
    insert_char('H');
    insert_char('i');
    assert(doc.lines[0].char_count == 2);
    assert(doc.lines[0].text[0] == 'H');
    assert(doc.lines[0].text[1] == 'i');
    assert(doc.cursor_col == 2);
    printf("PASS\n");
}

void test_backspace_strikethrough(void) {
    printf("Test: backspace creates strikethrough... ");
    init_doc();
    insert_char('H');
    insert_char('i');
    handle_backspace();
    assert(doc.lines[0].text[0] == 'H');
    assert(doc.lines[0].text[1] == 'i');
    assert(doc.lines[0].strikethrough[1] == true);
    assert(doc.lines[0].char_count == 2);
    printf("PASS\n");
}

void test_backspace_delete_strikethrough(void) {
    printf("Test: backspace on strikethrough deletes... ");
    init_doc();
    insert_char('A');
    insert_char('B');
    handle_backspace();
    assert(doc.lines[0].strikethrough[1] == true);
    handle_backspace();
    assert(doc.lines[0].char_count == 1);
    assert(doc.lines[0].text[0] == 'A');
    printf("PASS\n");
}

void test_enter(void) {
    printf("Test: enter creates new line... ");
    init_doc();
    insert_char('H');
    insert_char('i');
    handle_enter();
    assert(doc.cursor_line == 1);
    assert(doc.cursor_col == 0);
    assert(doc.total_lines == 2);
    printf("PASS\n");
}

void test_word_wrap(void) {
    printf("Test: word wrap... ");
    init_doc();
    insert_char('T');
    insert_char('h');
    insert_char('i');
    insert_char('s');
    insert_char(' ');
    insert_char('i');
    insert_char('s');
    insert_char(' ');
    insert_char('a');
    insert_char(' ');
    insert_char('t');
    insert_char('e');
    insert_char('s');
    insert_char('t');
    
    word_wrap(10);
    
    assert(doc.total_lines >= 2);
    assert(doc.lines[0].char_count <= 10);
    printf("PASS (lines: %d)\n", doc.total_lines);
}

void test_save_strikethrough(void) {
    printf("Test: save strikethrough as ~~... ");
    init_doc();
    insert_char('H');
    insert_char('i');
    handle_backspace();
    
    char buf[1024];
    save_to_string(buf, sizeof(buf));
    
    assert(strstr(buf, "~~") != NULL);
    printf("PASS (output: %s)\n", buf);
}

void test_load_strikethrough(void) {
    printf("Test: load strikethrough from ~~... ");
    load_from_string("H~~i~~");
    
    assert(doc.lines[0].text[0] == 'H');
    assert(doc.lines[0].strikethrough[0] == false);
    assert(doc.lines[0].text[1] == 'i');
    assert(doc.lines[0].strikethrough[1] == true);
    printf("PASS\n");
}

void test_dark_mode_toggle(void) {
    printf("Test: dark mode toggle... ");
    init_doc();
    assert(doc.inverted == false);
    doc.inverted = true;
    assert(doc.inverted == true);
    doc.inverted = !doc.inverted;
    assert(doc.inverted == false);
    printf("PASS\n");
}

int main(void) {
    printf("=== Typewrite Unit Tests ===\n\n");
    
    test_insert_char();
    test_backspace_strikethrough();
    test_backspace_delete_strikethrough();
    test_enter();
    test_word_wrap();
    test_save_strikethrough();
    test_load_strikethrough();
    test_dark_mode_toggle();
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
