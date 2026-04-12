#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "sounds_assets.h"

#define WAV_HEADER_SIZE 44

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t entry_count;
} TwAssetsHeader;

typedef struct {
    char name[16];
    uint32_t offset;
    uint32_t size;
} TwAssetsEntry;

static struct {
    uint8_t *data;
    uint32_t size;
    TwAssetsEntry *entries;
    uint32_t entry_count;
    SDL_AudioDeviceID device;
    bool opened;
    uint8_t *sample_buf;
    uint8_t *sample_buf_original;
    uint32_t sample_len;
} gSound;

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    printf("callback: called len=%d sample_buf=%p sample_len=%u\n", 
           len, (void*)gSound.sample_buf, gSound.sample_len);
    if (!gSound.sample_buf || gSound.sample_len == 0) {
        printf("callback: nothing to play\n");
        return;
    }
    int to_copy = (len < (int)gSound.sample_len) ? len : (int)gSound.sample_len;
    memcpy(stream, gSound.sample_buf, to_copy);
    if (to_copy < len) {
        memset(stream + to_copy, 0, len - to_copy);
    }
    gSound.sample_buf += to_copy;
    gSound.sample_len -= to_copy;
    if (gSound.sample_len == 0 && gSound.sample_buf_original) {
        SDL_FreeWAV(gSound.sample_buf_original);
        gSound.sample_buf = NULL;
        gSound.sample_buf_original = NULL;
    }
}

static bool load_assets(const uint8_t *data, uint32_t size) {
    if (size < 16) return false;

    uint32_t magic;
    memcpy(&magic, data, 4);
    if (magic != 0x54574153) { /* "TWAS" */
        printf("Bad magic: 0x%x\n", magic);
        return false;
    }

    uint16_t version, entry_count;
    memcpy(&version, data + 4, 2);
    memcpy(&entry_count, data + 6, 2);
    printf("Version=%u entries=%u\n", version, entry_count);

    gSound.data = malloc(size);
    if (!gSound.data) return false;
    memcpy(gSound.data, data, size);
    gSound.size = size;

    gSound.entries = calloc(entry_count, sizeof(TwAssetsEntry));
    if (!gSound.entries) {
        free(gSound.data);
        gSound.data = NULL;
        return false;
    }
    gSound.entry_count = entry_count;

    const uint8_t *idx = data + 16;
    for (uint32_t i = 0; i < entry_count; i++) {
        memcpy(gSound.entries[i].name, idx, 16);
        memcpy(&gSound.entries[i].offset, idx + 16, 4);
        memcpy(&gSound.entries[i].size, idx + 20, 4);
        printf("  [%u] %s offset=%u size=%u\n", i, gSound.entries[i].name,
               gSound.entries[i].offset, gSound.entries[i].size);
        idx += 24;
    }
    return true;
}

static bool play_sound(const char *name) {
    printf("play_sound: looking for '%s'\n", name);
    
    if (gSound.sample_len > 0 && gSound.sample_buf) {
        printf("  Skipping - audio still playing\n");
        return false;
    }

    TwAssetsEntry *entry = NULL;
    for (uint32_t i = 0; i < gSound.entry_count; i++) {
        if (strncmp(gSound.entries[i].name, name, 16) == 0) {
            entry = &gSound.entries[i];
            break;
        }
    }
    if (!entry) {
        printf("  Not found\n");
        return false;
    }

    printf("  Found: offset=%u size=%u\n", entry->offset, entry->size);

    const uint8_t *wav_data = gSound.data + entry->offset;
    uint32_t wav_size = entry->size;

    SDL_RWops *rw = SDL_RWFromConstMem(wav_data, wav_size);
    if (!rw) {
        printf("  SDL_RWFromConstMem failed\n");
        return false;
    }

    SDL_AudioSpec spec;
    Uint8 *buf;
    Uint32 len;
    if (!SDL_LoadWAV_RW(rw, 0, &spec, &buf, &len)) {  /* freesrc=0 */
        printf("  SDL_LoadWAV_RW failed: %s\n", SDL_GetError());
        SDL_RWclose(rw);
        return false;
    }
    SDL_RWclose(rw);

    printf("  Loaded WAV: %u bytes\n", len);

    gSound.sample_buf = buf;
    gSound.sample_buf_original = buf;
    gSound.sample_len = len;

    SDL_PauseAudioDevice(gSound.device, 0);
    return true;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf("=== SDL Sound Test ===\n");
    fflush(stdout);

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    printf("Loading assets...\n");
    fflush(stdout);
    if (!load_assets(sounds_assets, sounds_assets_len)) {
        printf("Failed to load embedded assets\n");
        return 1;
    }

    printf("Opening audio device...\n");
    fflush(stdout);

    SDL_AudioSpec want = {0}, got = {0};
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    want.samples = 4096;
    want.callback = audio_callback;
    want.userdata = NULL;

    gSound.device = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (gSound.device == 0) {
        printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return 1;
    }
    gSound.opened = true;
    printf("Unpausing audio...\n");
    fflush(stdout);
    SDL_PauseAudioDevice(gSound.device, 0);
    printf("Audio device ready\n");
    fflush(stdout);

    printf("\nPlaying typewriter_key...\n");
    fflush(stdout);
    play_sound("typewriter_key");
    SDL_Delay(500);

    printf("Playing typewriter_key again (should skip)...\n");
    play_sound("typewriter_key");
    SDL_Delay(1000);

    printf("Done, shutting down...\n");
    fflush(stdout);
    
    printf("  Pausing audio...\n");
    fflush(stdout);
    SDL_PauseAudioDevice(gSound.device, 1);
    SDL_Delay(200);
    
    printf("  Closing device...\n");
    fflush(stdout);
    SDL_CloseAudioDevice(gSound.device);
    
    printf("  Freeing sample buffer...\n");
    fflush(stdout);
    if (gSound.sample_buf_original) {
        SDL_FreeWAV(gSound.sample_buf_original);
        gSound.sample_buf_original = NULL;
        gSound.sample_buf = NULL;
    }
    printf("  Freeing assets...\n");
    fflush(stdout);
    free(gSound.data);
    free(gSound.entries);
    printf("  Quitting SDL...\n");
    fflush(stdout);
    SDL_Quit();

    printf("SUCCESS\n");
    return 0;
}