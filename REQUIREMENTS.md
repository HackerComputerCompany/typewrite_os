# Typewrite OS - Requirements Document

## Project Overview
A minimalist Linux distribution for x86/ARM systems that simulates a typewriter experience, inspired by the Freewrite interface. Designed as a focused, distraction-free writing environment.

**Philosophy**: This is a writing *experience*, not just an app. No distractions, no notifications, just you and your thoughts.

## Build System
- **Framework**: Buildroot 2024.02
- **Target Architecture**: x86_64 (ARM support planned for SoC)
- **Kernel**: Linux 6.1.44
- **Filesystem**: ext2 (128MB image)

## Hardware Requirements
- Standard x86_64 PC (UEFI/BIOS)
- **Framebuffer** support (VESA BIOS Extensions for x86, DRM/KMS for SoC)
- Minimum 512MB RAM
- VirtIO block device support for QEMU

## Hardware (Planned - SoC/Laptop)
- Custom ARM/ARM64 SoC
- **Mesa + DRM/KMS** graphics stack
- Mali/PowerVR/Vivante GPU (open-source drivers preferred)
- No X11 or Wayland - direct DRM/KMS access

## Build Artifacts
- `bzImage` - Linux kernel
- `rootfs.ext2` - Root filesystem
- `start-qemu.sh` - QEMU launcher script

## Application: Typewrite

### Core Features

#### Display Stack (Priority Order)
1. **Primary**: Mesa + DRM/KMS + GBM - Direct GPU access, best performance
2. **Fallback**: Linux framebuffer (`/dev/fb0`) - Legacy support, universal compatibility
3. **Graphics**: OpenGL ES 2.0 via EGL for GPU-accelerated rendering

#### Text Rendering
- **FreeType2** for TrueType font rendering
- Anti-aliased glyph rendering with alpha blending
- DejaVu Sans Mono as default font
- Configurable font scale (1x-5x)

#### Text Editing
- Monospace font rendering
- Word wrap (automatic at line end)
- Line-based document structure (up to 500 lines)
- Character limit per line (200 chars)

#### Ink Colors
- Black ink (default): `#000000`
- Green ink: `#1E8C1E`
- Red ink: `#B41E1E`
- Blue ink: `#1E3CB4`
- Toggle with F5 key
- Per-character ink (each character retains its color)

#### Dark Mode
- Invert all colors (white ↔ black, light gray ↔ dark gray)
- Toggle with F4 key
- Page background inverts
- Ink colors adjust appropriately

#### Text Behavior
- Strikethrough on backspace (characters marked but NOT deleted)
- Strikethrough persists in saved document
- F2/F3 keys to adjust zoom (accessibility - 1-5x)
- Arrow keys for cursor navigation

#### Visual Elements
- White page on gray background
- Red margin lines
- Status bar at bottom
- Underline cursor indicator
- Help overlay (press F1)

### Keyboard Controls
| Key | Action |
|-----|--------|
| F1 | Toggle help overlay |
| F2 | Zoom out (smaller text) |
| F3 | Zoom in (larger text) |
| F4 | Toggle dark mode |
| F5 | Cycle ink color (Black → Green → Red → Blue) |
| Arrow Keys | Move cursor |
| Backspace | Mark character for strikethrough |
| Enter | New line + word wrap |
| Ctrl+S | Save document to `/root/document.md` |
| Ctrl+Q | Quit application |

## Operating System Features

### Boot Process
1. Linux kernel loads with framebuffer console
2. BusyBox init starts
3. Typewrite application auto-launches on tty1

### Services
- syslogd - System logging (optional)
- getty on serial console (ttyS0)

### Serial Console
- Available on ttyS0 for debugging
- Baud rate: 115200
- Login prompt for system access

## QEMU Testing Environment

### Serial Mode
```bash
./start-qemu.sh --serial
```
- Full serial console output
- Login prompt for debugging
- No display

### Graphics Mode (Default)
```bash
./start-qemu.sh
```
- SDL2/framebuffer display window
- Typewrite app auto-starts

## Directory Structure
```
typewrite_os/
├── buildroot-2024.02/          # Buildroot source
│   ├── .config                 # Build configuration
│   ├── board/qemu/x86_64/     # Board-specific configs
│   │   ├── linux.config       # Kernel config
│   │   └── post-build.sh      # Rootfs customization
│   └── package/typewrite/     # Typewrite package
│       ├── Config.in
│       └── typewrite.mk
├── typewrite/                  # Typewrite source
│   ├── Makefile
│   └── src/
│       ├── main.c             # Unified app (FB + DRM/KMS)
│       ├── framebuffer.c      # Legacy FB version
│       └── sdl_main.c         # SDL2 version (deprecated)
├── start-qemu.sh              # QEMU launcher
├── mount_rootfs.sh            # Helper to mount rootfs.ext2
└── REQUIREMENTS.md           # This file
```

## TODO / Future Enhancements

### High Priority
- [ ] Test FreeType rendering in QEMU
- [ ] Add DRM/KMS + GBM backend (for real hardware)
- [ ] Add EGL-based rendering pipeline
- [ ] Sound effects (typewriter sounds)
- [ ] Paper texture/feel to page

### Medium Priority
- [ ] Undo/redo for strikethrough
- [ ] Page numbers
- [ ] Different font choices
- [ ] Configurable margins

### Low Priority
- [ ] Network document sync
- [ ] SSH access
- [ ] Web interface
- [ ] Touch screen support
- [ ] Real hardware SoC testing

## Known Issues
- F-key detection may vary by terminal
- QEMU may require `-vga std` for proper framebuffer

## Testing Checklist
- [ ] Boot to typewrite app in QEMU
- [ ] Type text and see anti-aliased rendering
- [ ] Test all ink colors (F1)
- [ ] Test dark mode (F4)
- [ ] Test scaling (F2/F3)
- [ ] Test backspace creates strikethrough
- [ ] Test word wrap on long lines
- [ ] Test save (Ctrl+S) creates valid .md file
- [ ] Test reload preserves strikethrough
- [ ] Test quit (Ctrl+Q)
- [ ] Test arrow key navigation
- [ ] Test serial console login

## Architecture Notes

### Why No X11/Wayland?
- **Complexity**: X11 and Wayland add significant complexity and attack surface
- **Latency**: Direct DRM/KMS access has lower input-to-display latency
- **Embedded**: Embedded systems often don't need full compositor
- **Philosophy**: Minimal, focused, no distractions

### DRM/KMS Stack
```
App (OpenGL ES / Software) → Mesa (libEGL, libGLESv2) → DRM/KMS (kernel) → GPU
                                                          ↓
                                                    GBM (buffer management)
```

### Framebuffer Fallback
For systems without GPU or DRM support:
```
App → Linux fbdev → /dev/fb0 → Framebuffer console
```

## Version History
- v0.3 (2024-03-18): Unified app with FreeType + DRM/GBM + framebuffer fallback
- v0.2 (2024-03-18): SDL2 version with anti-aliased fonts
- v0.1 (2024-03-18): Initial framebuffer implementation

## License
MIT License
