#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

static SDL_AudioDeviceID device;
static Uint8 *play_buf = NULL;
static Uint32 play_len = 0;
static Uint32 play_pos = 0;

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    if (!play_buf || play_pos >= play_len) { 
        memset(stream, 0, len); 
        return; 
    }
    int copy = len;
    if (play_pos + copy > play_len) copy = play_len - play_pos;
    memcpy(stream, play_buf + play_pos, copy);
    if (copy < len) memset(stream + copy, 0, len - copy);
    play_pos += copy;
}

static int load_wav_raw(const char *path, Uint8 **out_buf, Uint32 *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    Uint8 *data = malloc(size);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, size, f) != (size_t)size) { free(data); fclose(f); return -1; }
    fclose(f);
    
    /* Find data chunk */
    int data_offset = 12; /* After RIFF header */
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
        if (chunk_size % 2) data_offset++; /* pad */
    }
    free(data);
    return -1;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("=== Test: manual WAV parse ===\n");

    SDL_Init(SDL_INIT_AUDIO);
    
    SDL_AudioSpec want = {0}, got = {0};
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    want.samples = 4096;
    want.callback = audio_callback;
    
    device = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    printf("Audio opened\n");

    /* Load raw PCM ourselves */
    printf("Loading WAV...\n");
    if (load_wav_raw("sounds/all/arcade_blip.wav", &play_buf, &play_len) < 0) {
        printf("Load failed\n");
        return 1;
    }
    printf("Loaded: %u bytes raw PCM\n", play_len);

    play_pos = 0;
    printf("Unpausing...\n");
    SDL_PauseAudioDevice(device, 0);
    
    printf("Playing...\n");
    SDL_Delay(1500);

    printf("Done\n");
    SDL_PauseAudioDevice(device, 1);
    SDL_Delay(100);
    free(play_buf);
    SDL_CloseAudioDevice(device);
    SDL_Quit();
    printf("SUCCESS\n");
    return 0;
}