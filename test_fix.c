#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

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
    printf("=== Test: load BEFORE unpause ===\n");

    SDL_Init(SDL_INIT_AUDIO);
    
    SDL_AudioSpec want = {0}, got = {0};
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    want.samples = 4096;
    want.callback = audio_callback;
    
    device = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    printf("Audio opened\n");

    /* Load FIRST, before unpausing */
    printf("Loading WAV...\n");
    SDL_RWops *rw = SDL_RWFromFile("sounds/all/arcade_blip.wav", "rb");
    if (!rw) { printf("File open failed\n"); return 1; }
    
    SDL_AudioSpec spec;
    Uint8 *buf;
    Uint32 len;
    if (!SDL_LoadWAV_RW(rw, 1, &spec, &buf, &len)) {
        printf("Load failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_RWclose(rw);
    printf("Loaded: %u bytes\n", len);

    /* Set up buffer BEFORE unpausing */
    play_buf = buf;
    play_ptr = buf;
    play_len = len;
    
    /* NOW unpause */
    printf("Unpausing...\n");
    SDL_PauseAudioDevice(device, 0);
    
    printf("Playing...\n");
    SDL_Delay(1500);

    printf("Done\n");
    SDL_PauseAudioDevice(device, 1);
    SDL_Delay(100);
    if (play_buf) SDL_FreeWAV(play_buf);
    SDL_CloseAudioDevice(device);
    SDL_Quit();
    printf("SUCCESS\n");
    return 0;
}