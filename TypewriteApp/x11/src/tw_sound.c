/**
 * Sound engine implementation for Typewrite OS X11 client.
 * 
 * Uses SDL2 for audio playback.
 * Loads WAV files from disk at runtime (game-like approach).
 */

#include "tw_sound.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    bool initialized;
    bool enabled;
    char *base_path;
    SDL_AudioDeviceID audio_device;
    bool audio_opened;
    Uint8 *play_ptr;
    Uint8 *play_buf_orig;
    Uint32 play_len;
    SDL_AudioSpec playback_spec;
} gSound;

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    if (!gSound.play_ptr || gSound.play_len == 0) {
        memset(stream, 0, (size_t)len);
        return;
    }
    int to_copy = (len < (int)gSound.play_len) ? len : (int)gSound.play_len;
    memcpy(stream, gSound.play_ptr, (size_t)to_copy);
    if (to_copy < len)
        memset(stream + to_copy, 0, (size_t)(len - to_copy));
    gSound.play_ptr += to_copy;
    gSound.play_len -= (Uint32)to_copy;
    if (gSound.play_len == 0 && gSound.play_buf_orig) {
        free(gSound.play_buf_orig);
        gSound.play_ptr = NULL;
        gSound.play_buf_orig = NULL;
    }
}

static char *dirname_copy(const char *path) {
    if (!path) return NULL;
    char *d = strdup(path);
    if (!d) return NULL;
    char *slash = strrchr(d, '/');
    if (slash) *slash = '\0';
    else { free(d); d = strdup("."); }
    return d;
}

void TwSoundSetBasePath(const char *path) {
    free(gSound.base_path);
    gSound.base_path = path ? dirname_copy(path) : NULL;
}

static int load_wav_raw(const char *path, Uint8 **out_buf, Uint32 *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    Uint8 *data = malloc(size);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, size, f) != (size_t)size) { free(data); fclose(f); return -1; }
    fclose(f);
    
    int data_offset = 12;
    while (data_offset < size - 8) {
        char id[4] = {data[data_offset], data[data_offset+1], data[data_offset+2], data[data_offset+3]};
        unsigned int chunk_size = *(unsigned int*)(data + data_offset + 4);
        if (memcmp(id, "data", 4) == 0) {
            *out_buf = malloc(chunk_size);
            if (!*out_buf) { free(data); return -1; }
            memcpy(*out_buf, data + data_offset + 8, chunk_size);
            *out_len = chunk_size;
            free(data);
            return 0;
        }
        data_offset += 8 + chunk_size;
        if (chunk_size % 2) data_offset++;
    }
    free(data);
    return -1;
}

void TwSoundShutdown(void) {
    if (!gSound.initialized) {
        return;
    }

    gSound.enabled = false;
    if (gSound.audio_opened && gSound.audio_device != 0) {
        SDL_LockAudioDevice(gSound.audio_device);
        if (gSound.play_buf_orig) {
            free(gSound.play_buf_orig);
            gSound.play_buf_orig = NULL;
        }
        gSound.play_ptr = NULL;
        gSound.play_len = 0;
        SDL_UnlockAudioDevice(gSound.audio_device);
        SDL_CloseAudioDevice(gSound.audio_device);
    }

    SDL_Quit();
    free(gSound.base_path);

    memset(&gSound, 0, sizeof(gSound));
}

bool TwSoundInit(const char *assets_path) {
    (void)assets_path;
    
    if (gSound.initialized) {
        return true;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "TwSound: SDL_Init failed: %s\n", SDL_GetError());
        gSound.enabled = false;
        return false;
    }

    gSound.enabled = true;
    gSound.initialized = true;

    SDL_AudioSpec want = {0}, obtained = {0};
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    want.samples = 4096;
    want.callback = audio_callback;
    want.userdata = NULL;
    
    gSound.audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &obtained, 0);
    if (gSound.audio_device == 0) {
        fprintf(stderr, "TwSound: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        gSound.enabled = false;
    } else {
        gSound.audio_opened = true;
        gSound.playback_spec = obtained;
        printf("TwSound: initialized (freq=%d, format=%d, channels=%d)\n",
               obtained.freq, obtained.format, obtained.channels);
    }

    return true;
}

bool TwPlaySound(TwSoundId id) {
    if (!gSound.enabled) {
        return false;
    }
    if (id == SOUND_NONE || id < 0 || id >= SOUND_COUNT) {
        return false;
    }

    if (!gSound.audio_opened || gSound.audio_device == 0) {
        fprintf(stderr, "TwPlaySound: no audio device\n");
        return false;
    }

    static const char *const id_to_name[] = {
        [SOUND_TYPEWRITER_KEY] = "typewriter_key",
        [SOUND_TYPEWRITER_CARRIAGE] = "typewriter_carriage",
        [SOUND_TYPEWRITER_BELL] = "typewriter_bell",
        [SOUND_ARCADE_BLIP] = "arcade_blip",
        [SOUND_ARCADE_COIN] = "arcade_coin",
        [SOUND_ARCADE_MENU] = "arcade_menu",
        [SOUND_ARCADE_BOOT] = "arcade_boot",
        [SOUND_TERMINAL_BLIP] = "terminal_blip",
        [SOUND_CRT_POWER_ON] = "crt_power_on",
        [SOUND_IBM_KEYBOARD] = "ibm_keyboard",
        [SOUND_IBM_DISK] = "ibm_disk",
        [SOUND_IBM_POST] = "ibm_post",
        [SOUND_UI_TAP] = "ui_tap",
        [SOUND_UI_CHIME] = "ui_chime",
        [SOUND_VIRGIL_PENCIL] = "virgil_pencil",
        [SOUND_VIRGIL_PAPER] = "virgil_paper",
        [SOUND_SIMPLE_BLIP] = "simple_blip",
    };
    if (id < 0 || id >= SOUND_COUNT || id_to_name[id] == NULL) {
        return false;
    }
    const char *name = id_to_name[id];

    char path[1024];
    const char *search_dirs[] = {
        "../sounds/all",
        "sounds/all",
        "sounds",
        ".",
        NULL
    };
    
    bool found = false;
    for (int i = 0; search_dirs[i] != NULL; i++) {
        if (gSound.base_path) {
            snprintf(path, sizeof(path), "%s/%s/%s.wav", gSound.base_path, search_dirs[i], name);
        } else {
            snprintf(path, sizeof(path), "%s/%s.wav", search_dirs[i], name);
        }
        
        Uint8 *buf = NULL;
        Uint32 len = 0;
        if (load_wav_raw(path, &buf, &len) == 0) {
            found = true;
            
            SDL_LockAudioDevice(gSound.audio_device);
            if (gSound.play_buf_orig) {
                free(gSound.play_buf_orig);
                gSound.play_buf_orig = NULL;
                gSound.play_ptr = NULL;
                gSound.play_len = 0;
            }
            gSound.play_buf_orig = buf;
            gSound.play_ptr = buf;
            gSound.play_len = len;
            SDL_UnlockAudioDevice(gSound.audio_device);

            SDL_PauseAudioDevice(gSound.audio_device, 0);
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "TwPlaySound: could not load sound '%s'\n", name);
    }
    
    return found;
}

TwSoundId TwSoundForFont(int font_idx) {
    switch (font_idx) {
        case 0:
            return SOUND_VIRGIL_PENCIL;
        case 1:
            return SOUND_UI_TAP;
        case 2:
        case 3:
            return SOUND_TYPEWRITER_KEY;
        case 4:
        case 5:
            return SOUND_TERMINAL_BLIP;
        case 6:
            return SOUND_IBM_KEYBOARD;
        case 7:
            return SOUND_ARCADE_BLIP;
        default:
            return SOUND_SIMPLE_BLIP;
    }
}

TwSoundId TwSoundForFontCarriage(int font_idx) {
    switch (font_idx) {
        case 2:
        case 3:
            return SOUND_TYPEWRITER_CARRIAGE;
        default:
            return SOUND_NONE;
    }
}

TwSoundId TwSoundForFontBell(int font_idx) {
    switch (font_idx) {
        case 2:
        case 3:
            return SOUND_TYPEWRITER_BELL;
        default:
            return SOUND_NONE;
    }
}

bool TwPlaySoundForFont(int font_idx) {
    return TwPlaySound(TwSoundForFont(font_idx));
}
