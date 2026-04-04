# Typewrite OS - Features Documentation

## Overview

A minimalist Linux distribution for x86_64 that simulates a typewriter experience. Boots directly into a writing environment with no distractions.

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| F1 | Toggle help overlay |
| F2 | Toggle zoom (1x/2x) |
| F4 | Toggle dark/light mode |
| F5 | Cycle ink color (Black, Green, Red, Blue) |
| F6 | Cycle resolution (640x480, 800x600, 1024x768, 1280x1024, 1920x1080) |
| F7 | New document (auto-numbered: document.md, document1.md, etc.) |
| F8 | Cycle tab width (2, 4, 6, 8 spaces) |
| F9 | Toggle bold text |
| F11 | Open document browser |
| Arrow Keys | Move cursor |
| Enter | Carriage return (move to next line) |
| Backspace/Delete | Move cursor left (no deletion) |
| Tab | Insert spaces to next tab stop |
| Ctrl+S | Save document |
| Ctrl+Q | Quit to shell |

## Typewriter Behavior

### Overwrite Mode
- Typing at any position overwrites existing characters
- No insertion or shifting of text
- Like a real typewriter, you type over what's there

### Carriage Return (Enter)
- Advances paper to next line
- Does NOT insert a new line between existing content
- Cursor moves to beginning of next line

### Cursor Movement
- Arrow keys move cursor freely
- Can position cursor anywhere, even beyond existing text
- Cursor stays centered on screen (paper scrolls, cursor remains)

### No Deletion
- Backspace/Delete only moves cursor left
- No character deletion - what's typed stays typed
- Matches authentic typewriter experience

## Document Storage

- Saved as plain text files in `/root/Documents/`
- Auto-numbered filenames: `document.md`, `document1.md`, `document2.md`, etc.
- F7 creates new document, saves current first if dirty
- Ctrl+S explicitly saves current document
- Companion `.ink` files store color/bold metadata per character

## Display Features

### Zoom
- F2 toggles between 1x and 2x zoom
- Margins scale with zoom
- At 2x zoom, status bar shows two rows of information

### Ink Colors
- F5 cycles through: Black, Green, Red, Blue
- Each character retains its ink color when saved

### Bold Text
- F9 toggles bold mode
- Uses DejaVu Sans Mono Bold font variant
- Bold setting preserved per character

### Dark Mode
- F4 toggles between light and dark themes
- Inverts colors for night writing

### Resolution
- F6 cycles through preset resolutions
- Shows toast notification with new resolution
- Falls back if resolution not supported by hardware

### Fonts
- DejaVu Sans Mono (FreeType2)
- Anti-aliased rendering
- Monospace for typewriter feel
- Bold variant for bold text

## Status Bar

Bottom of screen shows:
- Left side: F-key quick reference (varies by zoom level)
- Right side: Word count

At 2x zoom, two rows display:
- Row 1: Help | Zoom | Dark | Ink:[color][B]
- Row 2: Res | New | Bold | Open | Save

## File Browser

Press F11 to open the document browser:
- Lists all `.md` files in `/root/Documents/`
- Up/Down arrows to select
- Enter to open selected document
- Escape to cancel

## File Format

Documents saved as plain text:
- `.md` file: One line per line on screen
- `.ink` file: Companion metadata (format: `ink_index,bold_flag` per character)

Example `.ink` file line:
```
0,0 0,0 1,1 0,0
```
This means: black normal, black normal, green bold, black normal

## Architecture

- Linux kernel (6.1.x)
- Direct framebuffer access (`/dev/fb0`)
- VT graphics mode (no console interference)
- FreeType2 for font rendering
- No X11, Wayland, or display server

## Building (this repository today)

- **UEFI:** `cd uefi-app && make` — see **`BUILD_SYSTEM.md`**.
- **Linux X11:** `make -C linux-typewrite-x11` — see **`linux-typewrite-x11/README.md`**; **`.deb`** via **`debian/`** / **`build-deb.sh`**.

The **Buildroot**-based minimal Linux image that matched much of this document is **no longer vendored** here (2026).

## Running in QEMU (UEFI)

```bash
./start-qemu.sh              # UEFI + OVMF, default resolution
./start-qemu.sh --res 1920x1080  # Custom resolution
./start-qemu.sh --no-kvm     # Without KVM (slower)
./start-qemu.sh --serial     # Serial console debug
```

## Version History

- **v0.5**: Document browser (F11), FAT32 documents partition, word count, shell exit (Ctrl+Q)
- **v0.4**: Bold text, ink persistence, tab support
- **v0.3**: FreeType rendering, framebuffer graphics
- **v0.2**: SDL2 prototype
- **v0.1**: Initial framebuffer implementation