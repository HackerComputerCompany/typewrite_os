# Sound Integration Plan for Typewrite OS

## Overview

This document outlines how to integrate sound effects with font selection in the UEFI application.

---

## Current Architecture

### Font System
- **9 fonts** indexed by `FONT_KIND` enum (main.c:274):
  ```
  0: FONT_VIRGIL       (hand-drawn)
  1: FONT_HELVETICA    (UI/sans)
  2: FONT_SPECIAL_ELITE (typewriter)
  3: FONT_COURIER_PRIME (typewriter)
  4: FONT_VT323        (DOS terminal)
  5: FONT_PRESS_START_2P (8-bit arcade)
  6: FONT_IBM_PLEX_MONO (IBM corporate)
  7: FONT_SHARE_TECH_MONO (sci-fi terminal)
  8: FONT_SIMPLE       (built-in 5x8)
  ```

### Key Input Flow
```
ReadKeyStroke() → HandleKey() → Process character/control
                                  ↓
                            Sound trigger point
```

---

## Sound-to-Font Mapping

| FONT_KIND | Category | Sound Files | Trigger Events |
|-----------|----------|-------------|----------------|
| FONT_SPECIAL_ELITE | typewriter | typewriter_key, carriage, bell | key press, enter, line end |
| FONT_COURIER_PRIME | typewriter | typewriter_key, carriage, bell | key press, enter, line end |
| FONT_VT323 | terminal | terminal_blip, crt_buzz | key press, power |
| FONT_SHARE_TECH_MONO | terminal | terminal_blip, crt_buzz | key press, power |
| FONT_PRESS_START_2P | arcade | arcade_key, coin, boot | key press, menu |
| FONT_IBM_PLEX_MONO | ibm | ibm_keyboard, ibm_disk, post | key press, save |
| FONT_HELVETICA | ui | ui_tap, chime | key press |
| FONT_VIRGIL | hand_drawn | virgil_pencil, paper | key press |
| FONT_SIMPLE | simple | simple_blip | key press |

---

## Implementation Phases

### Phase 1: Sound Infrastructure (Minimal)

**Goal**: Add ability to play sounds without changing app behavior.

1. **Add minimal PC speaker / beep support**
   - UEFI has no audio APIs - need to use GOP graphics for visual feedback OR
   - Check if `BootAudio.h` (UEFI 2.5+ spec) is available
   - Fallback: Visual flash/haptic on keypress

2. **If using Linux app for sound**
   - Keep UEFI app silent, use X11 client (linux-typewrite-x11/)
   - Communicate via shared file / settings for current font

### Phase 2: Sound Engine (if UEFI audio available)

**Goal**: Core sound playback in UEFI.

1. **Audio protocol detection**
   - Check for `EFI_AUDIO_PROTOCOL` (UEFI 2.5+)
   - Check for `EFI_BOOT_SOUND_PROTOCOL`

2. **Sound asset storage**
   - Embed WAV as C arrays in `.rodata` (compact)
   - Or load from filesystem (`fs/sounds/`)

3. **Sound playback function**
   ```c
   typedef struct {
       const UINT8 *data;
       UINT32 size;
       UINT32 sample_rate;
   } SoundData;
   
   EFI_STATUS PlaySound(CONST SoundData *sound);
   ```

### Phase 3: Event Integration

**Goal**: Trigger sounds at appropriate times.

1. **Key press sounds** (HandleKey, line ~3032)
   - After successful character insert
   - Use current font to select sound

2. **Control sounds**
   - Enter key → carriage return (typewriter fonts)
   - Bell at line end (typewriter fonts)
   - Menu navigation (arcade fonts)

3. **State sounds**
   - Save complete → disk whir (IBM)
   - Boot → CRT power on (terminal fonts)

---

## File Structure

```
sounds/
├── soundgen.py              # Sound generator
├── sounds.h                 # Embedded sound data (generated)
├── sounds.c
├── sounds.h                 # Header with sound definitions
└── sound_config.h           # Font-to-sound mapping
```

---

## Implementation Steps

### Step 1: Embed Sounds (if storage allows)

```bash
# Generate sound C arrays
python3 sounds/soundgen.py --embed
```

### Step 2: Add Sound API

```c
// sound.h
#ifndef SOUND_H
#define SOUND_H

#include <efi.h>

typedef enum {
    SOUND_TYPEWRITER_KEY,
    SOUND_TYPEWRITER_CARRIAGE,
    SOUND_TYPEWRITER_BELL,
    SOUND_ARCADE_BLIP,
    SOUND_ARCADE_COIN,
    SOUND_IBM_KEYBOARD,
    SOUND_IBM_DISK,
    SOUND_UI_TAP,
    SOUND_VIRGIL_PENCIL,
    SOUND_SIMPLE_BLIP,
    // ... etc
    SOUND_COUNT
} SoundId;

EFI_STATUS SoundInit(VOID);
EFI_STATUS PlaySound(SoundId id);

#endif
```

### Step 3: Map Font to Sound

```c
// Get sound for current font's key press
static SoundId GetKeySoundForFont(FONT_KIND font) {
    switch (font) {
        case FONT_SPECIAL_ELITE:
        case FONT_COURIER_PRIME:
            return SOUND_TYPEWRITER_KEY;
        case FONT_PRESS_START_2P:
            return SOUND_ARCADE_BLIP;
        case FONT_IBM_PLEX_MONO:
            return SOUND_IBM_KEYBOARD;
        case FONT_HELVETICA:
            return SOUND_UI_TAP;
        case FONT_VIRGIL:
            return SOUND_VIRGIL_PENCIL;
        case FONT_VT323:
        case FONT_SHARE_TECH_MONO:
            return SOUND_TERMINAL_BLIP;
        default:
            return SOUND_SIMPLE_BLIP;
    }
}
```

### Step 4: Hook into HandleKey

```c
// In HandleKey(), after character processing
if (Doc.Modified) {  // Key was accepted
    PlaySound(GetKeySoundForFont(CurrentFontKind));
}
```

---

## Alternative: X11 Client Sound

Since UEFI audio is limited, consider:

1. **UEFI app**: Visual-only feedback (flash cursor, subtle screen shake)
2. **Linux X11 client**: Full audio support via SDL2/OpenAL
3. **Communication**: Font selection stored in `Typewriter.settings`

This is simpler and more robust. The UEFI app focuses on text editing, the X11 client provides the full audiovisual experience.

---

## Testing

1. Generate sounds with various parameters
2. Listen and iterate on `soundgen.py`
3. Test with different fonts
4. Measure CPU/memory impact

---

## Open Questions

1. **UEFI audio support**: Most firmware lacks audio - should we rely on X11 client?
2. **Storage**: Embedding 18 WAVs (~500KB) is acceptable for UEFI app?
3. **Timing**: Should sounds be async or block until complete?
4. **Volume**: User-controllable or fixed?

---

## Next Steps

1. Decide on audio architecture (UEFI native vs X11 client)
2. If X11 client: add SDL2 sound to `linux-typewrite-x11/`
3. If UEFI native: embed sounds and add `PlaySound()` call
4. Iterate on sound design with `soundgen.py`
