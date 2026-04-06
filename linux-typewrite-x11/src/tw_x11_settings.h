#pragma once

#include <stddef.h>

/*
 * JSON UI settings for x11typewrite. Defaults are applied first; a config file
 * overwrites only keys that appear. Unknown keys are ignored.
 */
typedef struct TwX11AppSettings {
    int font_index;
    int background;
    int cursor_mode;
    int gutter_mode;
    int page_margins;
    int cols_margined;
    int typewriter_view;
    int word_wrap;
    int status_pulse;
    int insert_mode;
    int start_fullscreen;
    int window_width;
    int window_height;
} TwX11AppSettings;

void tw_x11_settings_defaults(TwX11AppSettings *s);

/* Merge keys from JSON file into *s. Missing file is OK (returns 0). */
int tw_x11_settings_load(const char *path, TwX11AppSettings *s);

int tw_x11_settings_save(const char *path, const TwX11AppSettings *s);

/* $HOME/.typewriter-settings.json (dotfile in home). */
void tw_x11_settings_default_path(char *out, size_t out_sz);
