# Typewrite OS Sound Assets

Sound assets organized by font aesthetic.

## Directory Structure

```
sounds/
├── arcade/           # Press Start 2P - 8-bit arcade
├── c64_atari/        # C64/Atari 800 style retro
├── typewriter/       # Special Elite, Courier Prime
├── ibm/              # IBM Plex Mono - corporate
├── terminal/          # VT323, Share Tech Mono - CRT/DOS
├── ui/               # Inter (Helvetica) - soft UI feedback
├── hand_drawn/       # Virgil - organic/pencil
├── simple/           # Simple 5x8 - minimal blips
├── soundgen.py       # Sound generator script
└── README.md         # This file
```

## Generated Sounds (18 total)

### Typewriter (Special Elite, Courier Prime)
- `typewriter_key.wav` - Mechanical key strike/clack
- `typewriter_carriage.wav` - Carriage return whir
- `typewriter_bell.wav` - Bell ding

### Arcade (Press Start 2P)
- `arcade_blip.wav` - Basic 8-bit blip
- `arcade_coin.wav` - Coin pickup (two-tone)
- `arcade_menu.wav` - Menu selection
- `arcade_boot.wav` - Boot sequence

### CRT/Terminal (VT323, Share Tech Mono)
- `crt_power_on.wav` - CRT warmup
- `crt_buzz.wav` - Phosphor buzz
- `terminal_blip.wav` - Terminal blip

### IBM (IBM Plex Mono)
- `ibm_keyboard.wav` - Keyboard click
- `ibm_disk.wav` - Disk drive spin
- `ibm_post.wav` - POST beep

### UI (Inter/Helvetica)
- `ui_tap.wav` - Soft tap
- `ui_chime.wav` - Notification chime

### Hand-drawn (Virgil)
- `virgil_pencil.wav` - Pencil scratch
- `virgil_paper.wav` - Paper rustle

### Simple
- `simple_blip.wav` - Minimal blip

---

## Sound Generator

Use `soundgen.py` to generate or modify sounds locally.

### Quick Start

```bash
cd sounds
python3 soundgen.py                    # Show available sounds
python3 soundgen.py .                  # Generate all to current directory
python3 soundgen.py arcade/            # Generate all to arcade/ folder
```

### Generate Individual Sounds

```bash
python3 soundgen.py --sound typewriter_key
python3 soundgen.py --sound arcade_coin --freq 880  # Custom freq
```

### Modify Sounds Programmatically

Edit the generator functions in `soundgen.py`:

| Parameter | Description |
|-----------|-------------|
| `duration` | Sound length in seconds |
| `freq` | Base frequency (Hz) |
| `attack/decay/sustain/release` | ADSR envelope (seconds) |
| `volume` | 0.0-1.0 amplitude |

---

## External Sources (for higher quality)

If you want professional recordings:

### Arcade / C64 / Atari
- **Double Trouble Audio - C64 Percussion & FX** (3 GB, 23,132 samples)
  - http://www.doubletroubleaudio.com/freebies/
- **sfxr** - Retro 8-bit sound generator
  - https://github.com/grimfang4/sfxr
- **Freesound - C64 tag** (265 sounds)
  - https://freesound.org/browse/tags/c64/

### Typewriter
- **Pixabay** (163 free typewriter sounds)
  - https://pixabay.com/sound-effects/search/typewriter/
- **Mixkit** (24 free sounds)
  - https://mixkit.co/free-sound-effects/typewriter/
- **BigSoundBank - Joseph Sardin**
  - https://www.bigsoundbank.com (search "typewriter")

### IBM / Corporate
- **Freesound - IBM tag**
  - https://freesound.org/browse/tags/IBM/
- **Freesfx - Computers**
  - https://freesfx.co.uk/Category/Computers/189

### CRT / Terminal
- **Pixabay - CRT sounds**
  - https://pixabay.com/sound-effects/search/crt/

---

## Sound Mapping by Font

| Font | Category | Sounds to Use |
|------|----------|----------------|
| Special Elite | typewriter | key, carriage, bell |
| Courier Prime | typewriter | key (cleaner), carriage, bell |
| VT323 | terminal | boot, buzz, blip |
| Share Tech Mono | terminal | boot, buzz, blip (sci-fi variant) |
| Press Start 2P | arcade | coin, menu, boot, blip |
| IBM Plex Mono | ibm | keyboard, disk, post |
| Inter/Helvetica | ui | tap, chime |
| Virgil | hand_drawn | pencil, paper |
| Simple | simple | blip |

---

## Adding New Sounds

1. Add generator function to `soundgen.py`:
```python
def generate_my_sound(duration=0.1, freq=440):
    """Description."""
    n_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, n_samples)
    wave = np.sin(2 * math.pi * freq * t) * np.exp(-t * 10)
    return wave.astype(np.float32)
```

2. Add to SOUNDS dict:
```python
SOUNDS = {
    ...
    "my_sound": generate_my_sound,
}
```

3. Regenerate:
```bash
python3 soundgen.py .
```

---

## License

Generated sounds are CC0 (public domain). External sources have their own licenses - check each.
