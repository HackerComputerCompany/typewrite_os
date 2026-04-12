/**
 * Sound engine implementation for Typewrite OS X11 client.
 * 
 * Uses SDL2 for audio playback.
 * Asset format: custom .assets file (TWAS)
 * Can use embedded sounds.assets or load from file.
 */

#include "tw_sound.h"
#include "sounds_assets.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAV_HEADER_SIZE 44

static struct {
    TwAssets assets;
    bool initialized;
    bool enabled;
    char *base_path;
    SDL_AudioDeviceID audio_device;
    bool audio_opened;
    Uint8 *play_ptr;      /* current position in decoded PCM */
    Uint8 *play_buf_orig; /* base pointer for free() when playback finishes */
    Uint32 play_len;      /* bytes remaining */
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

static const TwAssetsEntry *FindEntry(const char *name) {
    for (uint32_t i = 0; i < gSound.assets.entry_count; i++) {
        if (strncmp(gSound.assets.entries[i].name, name, TW_ASSETS_MAX_NAME) == 0) {
            return &gSound.assets.entries[i];
        }
    }
    return NULL;
}

static bool load_assets_from_memory(const uint8_t *data, uint32_t size) {
    if (size < 16) return false;

    uint32_t magic;
    uint16_t version;
    uint16_t entry_count;
    memcpy(&magic, data, 4);
    memcpy(&version, data + 4, 2);
    memcpy(&entry_count, data + 6, 2);

    if (magic != TW_ASSETS_MAGIC || version != TW_ASSETS_VERSION) {
        return false;
    }

    gSound.assets.entry_count = entry_count;
    gSound.assets.size = size;
    gSound.assets.data = malloc(size);
    if (!gSound.assets.data) return false;
    memcpy(gSound.assets.data, data, size);

    gSound.assets.entries = calloc(entry_count, sizeof(TwAssetsEntry));
    if (!gSound.assets.entries) {
        free(gSound.assets.data);
        gSound.assets.data = NULL;
        return false;
    }

    const uint8_t *idx = data + 16;
    for (uint32_t i = 0; i < entry_count; i++) {
        memcpy(gSound.assets.entries[i].name, idx, 16);
        memcpy(&gSound.assets.entries[i].offset, idx + 16, 4);
        memcpy(&gSound.assets.entries[i].size, idx + 20, 4);
        idx += 24;
    }

    return true;
}

bool TwSoundInit(const char *assets_path) {
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

    bool loaded = false;

    if (gSound.assets.data) {
        free(gSound.assets.data);
        gSound.assets.data = NULL;
    }
    if (gSound.assets.entries) {
        free(gSound.assets.entries);
        gSound.assets.entries = NULL;
    }

    if (assets_path) {
        char full_path[1024];
        if (gSound.base_path && assets_path[0] != '/') {
            snprintf(full_path, sizeof(full_path), "%s/%s", gSound.base_path, assets_path);
            assets_path = full_path;
        }

        FILE *f = fopen(assets_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            uint8_t *data = malloc(size);
            if (data && fread(data, 1, size, f) == (size_t)size) {
                loaded = load_assets_from_memory(data, size);
            }
            free(data);
            fclose(f);
        }
        if (!loaded) {
            fprintf(stderr, "TwSound: could not load %s, using embedded\n", assets_path);
        }
    }

    if (!loaded) {
        loaded = load_assets_from_memory(sounds_assets, sounds_assets_len);
        if (loaded) {
            printf("TwSound: using embedded sounds (%u entries)\n", gSound.assets.entry_count);
        } else {
            fprintf(stderr, "TwSound: failed to load embedded sounds\n");
        }
    }

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
    }

    return true;
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

    if (gSound.assets.data) {
        free(gSound.assets.data);
    }
    if (gSound.assets.entries) {
        free(gSound.assets.entries);
    }
    free(gSound.base_path);

    memset(&gSound, 0, sizeof(gSound));
}

bool TwPlaySound(TwSoundId id) {
    if (!gSound.enabled) {
        fprintf(stderr, "TwPlaySound: disabled\n");
        return false;
    }
    if (id == SOUND_NONE || id < 0 || id >= SOUND_COUNT) {
        fprintf(stderr, "TwPlaySound: bad id %d\n", id);
        return false;
    }

    if (!gSound.audio_opened || gSound.audio_device == 0) {
        fprintf(stderr, "TwPlaySound: no audio device (opened=%d device=%d)\n", 
                gSound.audio_opened, gSound.audio_device);
        return false;
    }

    if (!gSound.assets.data) {
        fprintf(stderr, "TwPlaySound: no assets.data\n");
        return false;
    }
    if (gSound.assets.entry_count == 0) {
        fprintf(stderr, "TwPlaySound: 0 entries\n");
        return false;
    }
    fprintf(stderr, "TwPlaySound: assets OK data=%p entries=%u\n", 
            (void*)gSound.assets.data, gSound.assets.entry_count);

    /* Sound ID to name lookup */
    static const char *const id_to_name[] = {
        [SOUND_TYPEWRITER_KEY] = "typewriter_key",
        [SOUND_TYPEWRITER_CARRIAGE] = "typewriter_carr",
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
        fprintf(stderr, "TwPlaySound: no name for id %d\n", id);
        return false;
    }
    const char *name = id_to_name[id];
    fprintf(stderr, "TwPlaySound: id=%d name=%s\n", id, name);

    const TwAssetsEntry *entry = FindEntry(name);
    if (!entry) {
        return false;
    }

    /* Raw PCM data from assets - just copy directly */
    const uint8_t *pcm_data = (const uint8_t *)gSound.assets.data + entry->offset;
    uint32_t pcm_size = entry->size;

    if (pcm_size == 0) {
        return false;
    }

    /* Copy PCM data to a buffer we own */
    Uint8 *buf = malloc(pcm_size);
    if (!buf) {
        return false;
    }
    memcpy(buf, pcm_data, pcm_size);

    SDL_LockAudioDevice(gSound.audio_device);
    if (gSound.play_buf_orig) {
        free(gSound.play_buf_orig);
        gSound.play_buf_orig = NULL;
        gSound.play_ptr = NULL;
        gSound.play_len = 0;
    }
    gSound.play_buf_orig = buf;
    gSound.play_ptr = buf;
    gSound.play_len = pcm_size;
    SDL_UnlockAudioDevice(gSound.audio_device);

    SDL_PauseAudioDevice(gSound.audio_device, 0);
    return true;
}

/* Map font index to sound - this must match the font order in tw_bitmapfont_uefi.c */
TwSoundId TwSoundForFont(int font_idx) {
    switch (font_idx) {
        case 0:  /* Virgil */
            return SOUND_VIRGIL_PENCIL;
        case 1:  /* Helvetica/Inter */
            return SOUND_UI_TAP;
        case 2:  /* Special Elite */
        case 3:  /* Courier Prime */
            return SOUND_TYPEWRITER_KEY;
        case 4:  /* VT323 */
        case 5:  /* Share Tech Mono */
            return SOUND_TERMINAL_BLIP;
        case 6:  /* IBM Plex Mono */
            return SOUND_IBM_KEYBOARD;
        case 7:  /* Press Start 2P */
            return SOUND_ARCADE_BLIP;
        default:
            return SOUND_SIMPLE_BLIP;
    }
}

TwSoundId TwSoundForFontCarriage(int font_idx) {
    switch (font_idx) {
        case 2:  /* Special Elite */
        case 3:  /* Courier Prime */
            return SOUND_TYPEWRITER_CARRIAGE;
        default:
            return SOUND_NONE;
    }
}

TwSoundId TwSoundForFontBell(int font_idx) {
    switch (font_idx) {
        case 2:  /* Special Elite */
        case 3:  /* Courier Prime */
            return SOUND_TYPEWRITER_BELL;
        default:
            return SOUND_NONE;
    }
}

bool TwPlaySoundForFont(int font_idx) {
    return TwPlaySound(TwSoundForFont(font_idx));
}
