# Typewrite OS - Features Documentation

## Overview

A minimalist Linux distribution for x86 that simulates a typewriter experience. Boots directly into a writing environment with no distractions.

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| F1 | Toggle help overlay |
| F2 | Zoom out (smaller text) |
| F3 | Zoom in (larger text) |
| F4 | Toggle dark/light mode |
| F5 | Cycle ink color (Black, Green, Red, Blue) |
| F6 | Cycle resolution (640x480, 800x600, 1024x768, native) |
| F7 | New document (auto-numbered: document1.md, document2.md, etc.) |
| F8-F12 | Unassigned (shows toast notification) |
| Arrow Keys | Move cursor |
| Enter | Carriage return (move to next line) |
| Backspace/Delete | Move cursor left (no deletion) |
| Ctrl+S | Save document |
| Ctrl+Q | Quit |

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

- Saved as plain text files in `/root/`
- Auto-numbered filenames: `document.md`, `document1.md`, `document2.md`, etc.
- F7 creates new document, saves current first
- Ctrl+S explicitly saves current document

## Display Features

### Zoom
- F2/F3 control zoom level (1x-5x)
- Margins scale with zoom
- Cursor stays centered on screen

### Ink Colors
- F5 cycles through: Black, Green, Red, Blue
- Each character retains its ink color

### Dark Mode
- F4 toggles between light and dark themes
- Inverts colors for night writing

### Resolution
- F6 cycles through: 640x480, 800x600, 1024x768, native
- Dynamically changes framebuffer resolution

### Fonts
- DejaVu Sans Mono (FreeType2)
- Anti-aliased rendering
- Monospace for typewriter feel

## Status Bar

Bottom of screen shows:
- Current resolution (or "native")
- Zoom level
- Mode (Light/Dark)
- Current ink color
- F-key quick reference

## File Format

Documents saved as plain text:
- One line per line on screen
- No markdown or special formatting
- Ink colors stored in separate metadata (not yet implemented)

## Architecture

- Linux kernel (6.1.x)
- Direct framebuffer access (`/dev/fb0`)
- VT graphics mode (no console interference)
- FreeType2 for font rendering
- No X11, Wayland, or display server

## Building

```bash
cd buildroot-2024.02
make
```

## Running in QEMU

```bash
./start-qemu.sh           # Normal run with KVM
./start-qemu.sh --no-kvm  # Run without KVM
./start-qemu.sh --serial  # Serial console only
```

## Patching Rootfs

See `PATCHING.md` for manual rootfs patching instructions.