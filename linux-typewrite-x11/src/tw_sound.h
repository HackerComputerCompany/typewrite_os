/**
 * Sound engine for Typewrite OS X11 client.
 * 
 * Uses custom .assets file format:
 * - Header (16 bytes): magic(4) + version(2) + entry_count(2) + reserved(8)
 * - Index entries (24 bytes each): name(16) + offset(4) + size(4)
 * - Data section: raw sound data (WAV)
 */

#ifndef TW_SOUND_H
#define TW_SOUND_H

#include <stdint.h>
#include <stdbool.h>

#define TW_ASSETS_MAGIC 0x54574153  /* "TWAS" in little-endian */
#define TW_ASSETS_VERSION 1
#define TW_ASSETS_MAX_NAME 16
#define TW_ASSETS_MAX_ENTRIES 64

typedef struct {
    char name[TW_ASSETS_MAX_NAME];
    uint32_t offset;
    uint32_t size;
} TwAssetsEntry;

typedef struct {
    char *data;
    uint32_t size;
    uint32_t entry_count;
    TwAssetsEntry *entries;
} TwAssets;

typedef enum {
    SOUND_NONE = 0,
    SOUND_TYPEWRITER_KEY,
    SOUND_TYPEWRITER_CARRIAGE,
    SOUND_TYPEWRITER_BELL,
    SOUND_ARCADE_BLIP,
    SOUND_ARCADE_COIN,
    SOUND_ARCADE_MENU,
    SOUND_ARCADE_BOOT,
    SOUND_TERMINAL_BLIP,
    SOUND_CRT_POWER_ON,
    SOUND_IBM_KEYBOARD,
    SOUND_IBM_DISK,
    SOUND_IBM_POST,
    SOUND_UI_TAP,
    SOUND_UI_CHIME,
    SOUND_VIRGIL_PENCIL,
    SOUND_VIRGIL_PAPER,
    SOUND_SIMPLE_BLIP,
    SOUND_COUNT
} TwSoundId;

/* Map font index (from tw_uefi_font_get) to key press sound */
TwSoundId TwSoundForFont(int font_idx);
TwSoundId TwSoundForFontCarriage(int font_idx);
TwSoundId TwSoundForFontBell(int font_idx);

bool TwSoundInit(const char *assets_path);
void TwSoundShutdown(void);
void TwSoundSetBasePath(const char *path);

bool TwPlaySound(TwSoundId id);
bool TwPlaySoundForFont(int font_idx);  /* Play key sound for font */

#endif /* TW_SOUND_H */
