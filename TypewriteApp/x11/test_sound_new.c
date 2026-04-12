#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "sounds_assets.h"

#define TW_ASSETS_MAGIC 0x54574153  /* "TWAS" */

typedef struct {
    char name[16];
    uint32_t offset;
    uint32_t size;
} AssetEntry;

static SDL_AudioDeviceID device;
static Uint8 *play_buf = NULL;
static Uint8 *play_ptr = NULL;
static Uint32 play_len = 0;

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    if (!play_ptr || play_len == 0) {
        memset(stream, 0, len);
        return;
    }
    int copy = len < (int)play_len ? len : (int)play_len;
    memcpy(stream, play_ptr, copy);
    if (copy < len) memset(stream + copy, 0, len - copy);
    play_ptr += copy;
    play_len -= copy;
    if (play_len == 0 && play_buf) {
        SDL_FreeWAV(play_buf);
        play_buf = NULL;
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("=== Sound Test with new assets ===\n");

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Parse assets header */
    uint32_t magic, version, count;
    memcpy(&magic, sounds_assets, 4);
    memcpy(&version, sounds_assets + 4, 2);
    memcpy(&count, sounds_assets + 6, 2);
    printf("Assets: magic=0x%x version=%u count=%u\n", magic, version, count);

    /* Find the entry we want */
    const char *wanted = "arcade_blip";
    AssetEntry *entries = (AssetEntry *)(sounds_assets + 16);
    uint32_t wav_offset = 0, wav_size = 0;
    for (int i = 0; i < count; i++) {
        if (strncmp(entries[i].name, wanted, 16) == 0) {
            wav_offset = entries[i].offset;
            wav_size = entries[i].size;
            printf("Found %s: offset=%u size=%u\n", wanted, wav_offset, wav_size);
            break;
        }
    }
    if (wav_offset == 0 || wav_size < 44) {
        printf("Failed to find entry\n");
        return 1;
    }

    /* Open audio */
    SDL_AudioSpec want = {0}, got = {0};
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    want.samples = 4096;
    want.callback = audio_callback;
    
    device = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (device == 0) {
        printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("Audio opened: freq=%d channels=%d format=%d\n", got.freq, got.channels, got.format);
    SDL_PauseAudioDevice(device, 0);

    /* Load WAV from assets */
    const uint8_t *wav_data = sounds_assets + wav_offset;
    SDL_RWops *rw = SDL_RWFromConstMem(wav_data, wav_size);
    if (!rw) {
        printf("SDL_RWFromConstMem failed\n");
        return 1;
    }
    
    SDL_AudioSpec spec;
    Uint8 *buf;
    Uint32 len;
    if (!SDL_LoadWAV_RW(rw, 1, &spec, &buf, &len)) {
        printf("SDL_LoadWAV_RW failed: %s\n", SDL_GetError());
        SDL_RWclose(rw);
        return 1;
    }
    SDL_RWclose(rw);
    printf("Loaded WAV: spec.freq=%d channels=%d format=%d len=%u\n", 
           spec.freq, spec.channels, spec.format, len);

    /* Play it */
    play_buf = buf;
    play_ptr = buf;
    play_len = len;
    SDL_PauseAudioDevice(device, 0);

    printf("Playing... (wait 1s)\n");
    SDL_Delay(1000);

    /* Cleanup */
    printf("Shutdown...\n");
    SDL_PauseAudioDevice(device, 1);
    SDL_Delay(100);
    if (play_buf) SDL_FreeWAV(play_buf);
    SDL_CloseAudioDevice(device);
    SDL_Quit();

    printf("SUCCESS\n");
    return 0;
}