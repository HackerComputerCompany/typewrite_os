# Typewrite OS - Requirements Document

## Project Overview
A minimalist Linux distribution for x86_64 systems that simulates a typewriter experience. Designed as a focused, distraction-free writing environment.

**Philosophy**: This is a writing *experience*, not just an app. No distractions, no notifications, just you and your thoughts.

## Build System
- **Framework**: Buildroot 2024.02
- **Target Architecture**: x86_64
- **Kernel**: Linux 6.1.44
- **Filesystem**: ext2 (128MB image)

## Hardware Requirements
- Standard x86_64 PC (UEFI/BIOS)
- **Framebuffer** support (VESA BIOS Extensions)
- Minimum 512MB RAM
- VirtIO block device support for QEMU

## Build Artifacts
- `bzImage` - Linux kernel
- `rootfs.ext2` - Root filesystem
- `start-qemu.sh` - QEMU launcher script

## Application: Typewrite

### Core Features

#### Display Stack
- **Linux framebuffer** (`/dev/fb0`) - Direct framebuffer access
- Resolution cycling (F6): 640x480 → 800x600 → 1024x768 → native
- 32-bit ARGB pixel format

#### Text Rendering
- **FreeType2** for TrueType font rendering
- Anti-aliased glyph rendering with alpha blending
- DejaVu Sans Mono (regular + bold variants)
- Zoom levels: 1x and 2x

#### Document Structure
- Line-based document (up to 500 lines)
- 200 characters per line maximum
- Documents saved as plain text (`.md` files)
- Ink colors and bold preserved in companion `.ink` file

#### Ink Colors
- Black ink (default): `#000000`
- Green ink: `#1E8C1E`
- Red ink: `#B41E1E`
- Blue ink: `#1E3CB4`
- Toggle with F5 key
- Per-character ink (each character retains its color)

#### Bold Text
- Toggle bold mode with F9
- Uses DejaVu Sans Mono Bold font
- Per-character bold flag saved to `.ink` file

#### Tab Support
- F8 cycles tab width: 2, 4, 6, 8 spaces
- Tab key inserts spaces to next tab stop

#### Dark Mode
- Invert all colors (white ↔ black, light gray ↔ dark gray)
- Toggle with F4 key

#### Text Behavior
- **Overwrite mode**: New characters replace existing ones
- **Carriage return**: Enter advances paper, doesn't insert lines
- Backspace marks strikethrough (characters visible but crossed out)
- Cursor stays centered on screen (paper scrolls)
- Arrow keys for cursor navigation

#### Visual Elements
- White page on gray background
- Red margin lines
- Status bar: resolution | zoom | mode | ink+bold | tab | help
- Centered help overlay (F1)
- Toast notifications for actions

### Keyboard Controls
| Key | Action |
|-----|--------|
| F1 | Toggle help overlay |
| F2 | Zoom out (to 1x) |
| F3 | Zoom in (to 2x) |
| F4 | Toggle dark mode |
| F5 | Cycle ink color |
| F6 | Cycle resolution |
| F7 | New document |
| F8 | Cycle tab width (2/4/6/8) |
| F9 | Toggle bold mode |
| Arrow Keys | Move cursor |
| Tab | Insert spaces to next tab stop |
| Enter | Carriage return (new line) |
| Backspace | Strikethrough character |
| Ctrl+S | Save document |
| Ctrl+Q | Quit application |

### File Format
Documents saved as two files:
- `document.md` - Plain text content
- `document.ink` - Color/bold overlay (format: `ink,bold` pairs per character)

## Operating System Features

### Boot Process
1. Linux kernel loads with framebuffer console
2. BusyBox init starts
3. Typewrite application auto-launches on tty1

### Serial Console
- Unix socket at `/tmp/typewrite-serial.sock`
- Connect with: `socat - UNIX-CONNECT:/tmp/typewrite-serial.sock`

## QEMU Testing Environment

### Default Mode (Graphics)
```bash
./start-qemu.sh
```
- SDL2 display at 1024x768
- Typewrite app auto-starts
- Serial console via Unix socket

### Serial Mode
```bash
./start-qemu.sh --serial
```
- Full serial console output
- No display

## Directory Structure
```
typewrite_os/
├── buildroot-2024.02/          # Buildroot source
│   ├── .config                 # Build configuration
│   └── package/typewrite/      # Typewrite package
│       ├── Config.in
│       └── typewrite.mk
├── typewrite/                  # Typewrite source
│   ├── Makefile
│   └── src/main.c              # Main application
├── start-qemu.sh              # QEMU launcher
├── REQUIREMENTS.md            # This file
├── FEATURES.md                # Feature documentation
└── PATCHING.md                # How to patch rootfs
```

## Development Notes

### Key Implementation Details
1. **FreeType font width**: Must divide advance by 64 to convert from 26.6 fixed-point
2. **F-key escape sequences**: F1-F5 use `ESC[[A-E]`, F6-F12 use `ESC[17~` through `ESC[24~`
3. **VT graphics mode**: `ioctl(tty_fd, KDSETMODE, KD_GRAPHICS)` disables console
4. **Framebuffer resolution**: Changed via `FBIOPUT_VSCREENINFO`
5. **Character alignment**: Center each glyph in fixed-size cell
6. **Zoom**: Limited to 1x and 2x for clean rendering

### Quick Build & Patch
```bash
cd buildroot-2024.02 && make typewrite-rebuild
sudo mount -o loop output/images/rootfs.ext2 /tmp/rootfs
sudo cp output/target/usr/bin/typewrite /tmp/rootfs/usr/bin/typewrite
sudo umount /tmp/rootfs
```

## Version History
- **v0.4** (2024-03-19): Added bold support, ink persistence, page breaks
- **v0.3** (2024-03-18): FreeType rendering, framebuffer graphics
- **v0.2** (2024-03-18): SDL2 version
- **v0.1** (2024-03-18): Initial framebuffer implementation

## License
MIT License