#include "tw_x11_settings.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tw_x11_settings_defaults(TwX11AppSettings *s) {
    if (!s)
        return;
    memset(s, 0, sizeof(*s));
    s->font_index = 2;
    s->background = 1;
    s->cursor_mode = 3;
    s->gutter_mode = 0;
    s->page_margins = 1;
    s->cols_margined = 58;
    s->typewriter_view = 1;
    s->word_wrap = 1;
    s->status_pulse = 0;
    s->insert_mode = 0;
    s->start_fullscreen = 0;
    s->window_width = 960;
    s->window_height = 540;
}

void tw_x11_settings_default_path(char *out, size_t out_sz) {
    const char *home = getenv("HOME");
    if (!home || !home[0])
        home = ".";
    snprintf(out, out_sz, "%s/.typewriter-settings.json", home);
}

static void skip_ws(const char **p) {
    while (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t')
        (*p)++;
}

static int parse_string(const char **p, char *out, size_t olen) {
    if (**p != '"')
        return -1;
    (*p)++;
    size_t i = 0;
    while (**p && **p != '"') {
        if (i + 1 < olen)
            out[i++] = **p;
        (*p)++;
    }
    if (**p != '"')
        return -1;
    (*p)++;
    out[i] = 0;
    return 0;
}

static int parse_int(const char **p, int *out) {
    char *end = NULL;
    long v = strtol(*p, &end, 10);
    if (end == *p)
        return -1;
    *p = end;
    *out = (int)v;
    return 0;
}

static int parse_bool(const char **p, int *out) {
    if (strncmp(*p, "true", 4) == 0) {
        *p += 4;
        *out = 1;
        return 0;
    }
    if (strncmp(*p, "false", 5) == 0) {
        *p += 5;
        *out = 0;
        return 0;
    }
    return -1;
}

static void apply_key(const char *key, const char **vp, TwX11AppSettings *s) {
    const char *p = *vp;
    int iv;
    int bv;

    if (strcmp(key, "font_index") == 0) {
        if (parse_int(&p, &iv) == 0)
            s->font_index = iv;
    } else if (strcmp(key, "background") == 0) {
        if (parse_int(&p, &iv) == 0)
            s->background = iv;
    } else if (strcmp(key, "cursor_mode") == 0) {
        if (parse_int(&p, &iv) == 0)
            s->cursor_mode = iv;
    } else if (strcmp(key, "gutter_mode") == 0) {
        if (parse_int(&p, &iv) == 0)
            s->gutter_mode = iv;
    } else if (strcmp(key, "page_margins") == 0) {
        if (parse_bool(&p, &bv) == 0)
            s->page_margins = bv;
    } else if (strcmp(key, "cols_margined") == 0) {
        if (parse_int(&p, &iv) == 0)
            s->cols_margined = iv;
    } else if (strcmp(key, "typewriter_view") == 0) {
        if (parse_bool(&p, &bv) == 0)
            s->typewriter_view = bv;
    } else if (strcmp(key, "word_wrap") == 0) {
        if (parse_bool(&p, &bv) == 0)
            s->word_wrap = bv;
    } else if (strcmp(key, "status_pulse") == 0) {
        if (parse_int(&p, &iv) == 0)
            s->status_pulse = iv;
    } else if (strcmp(key, "insert_mode") == 0) {
        if (parse_bool(&p, &bv) == 0)
            s->insert_mode = bv;
    } else if (strcmp(key, "start_fullscreen") == 0) {
        if (parse_bool(&p, &bv) == 0)
            s->start_fullscreen = bv;
    } else if (strcmp(key, "window_width") == 0) {
        if (parse_int(&p, &iv) == 0)
            s->window_width = iv;
    } else if (strcmp(key, "window_height") == 0) {
        if (parse_int(&p, &iv) == 0)
            s->window_height = iv;
    } else {
        /* skip unknown value */
        skip_ws(&p);
        if (*p == '"') {
            char discard[128];
            (void)parse_string(&p, discard, sizeof(discard));
        } else if (strncmp(p, "true", 4) == 0 || strncmp(p, "false", 5) == 0) {
            int x;
            (void)parse_bool(&p, &x);
        } else {
            (void)parse_int(&p, &iv);
        }
    }
    *vp = p;
}

static int parse_object(const char **pp, TwX11AppSettings *s) {
    const char *p = *pp;
    skip_ws(&p);
    if (*p != '{')
        return -1;
    p++;
    for (;;) {
        skip_ws(&p);
        if (*p == '}')
            break;
        char key[64];
        if (parse_string(&p, key, sizeof(key)) != 0)
            return -1;
        skip_ws(&p);
        if (*p != ':')
            return -1;
        p++;
        skip_ws(&p);
        apply_key(key, &p, s);
        skip_ws(&p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}')
            break;
        return -1;
    }
    if (*p != '}')
        return -1;
    p++;
    skip_ws(&p);
    if (*p != 0)
        return -1;
    *pp = p;
    return 0;
}

int tw_x11_settings_load(const char *path, TwX11AppSettings *s) {
    if (!path || !path[0] || !s)
        return -1;
    TwX11AppSettings scratch;
    tw_x11_settings_defaults(&scratch);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long sz = ftell(fp);
    if (sz < 0 || sz > 65536) {
        fclose(fp);
        return -1;
    }
    rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    buf[sz] = 0;

    const char *p = buf;
    skip_ws(&p);
    int st = parse_object(&p, &scratch);
    free(buf);
    if (st == 0)
        *s = scratch;
    return st;
}

int tw_x11_settings_save(const char *path, const TwX11AppSettings *s) {
    if (!path || !path[0] || !s)
        return -1;
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return -1;
    fprintf(fp,
            "{\n"
            "  \"font_index\": %d,\n"
            "  \"background\": %d,\n"
            "  \"cursor_mode\": %d,\n"
            "  \"gutter_mode\": %d,\n"
            "  \"page_margins\": %s,\n"
            "  \"cols_margined\": %d,\n"
            "  \"typewriter_view\": %s,\n"
            "  \"word_wrap\": %s,\n"
            "  \"status_pulse\": %d,\n"
            "  \"insert_mode\": %s,\n"
            "  \"start_fullscreen\": %s,\n"
            "  \"window_width\": %d,\n"
            "  \"window_height\": %d\n"
            "}\n",
            s->font_index, s->background, s->cursor_mode, s->gutter_mode, s->page_margins ? "true" : "false",
            s->cols_margined, s->typewriter_view ? "true" : "false", s->word_wrap ? "true" : "false",
            s->status_pulse, s->insert_mode ? "true" : "false", s->start_fullscreen ? "true" : "false",
            s->window_width, s->window_height);
    fclose(fp);
    return 0;
}
