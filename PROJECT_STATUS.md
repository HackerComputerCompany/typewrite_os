# Typewrite OS - Project Summary

## Goal
Create a minimalist Linux distro for x86 systems that simulates a typewriter experience (inspired by Freewrite). The app is a focused writing environment with no distractions, supporting strikethrough on backspace, ink colors, dark mode, and document persistence.

## Build System
- **Buildroot 2024.02** as the build system
- **Target:** x86_64 with framebuffer/VESA for QEMU testing
- **Graphics:** DRM/KMS + Mesa for real hardware (no X11/Wayland)
- **Documents:** Saved as Markdown with `~~strikethrough~~` syntax

## Key Features
- No ruler at top - margins for 8.5x11 paper layout
- Double buffering to prevent flicker
- F-keys for ink color cycling and text scaling
- Terminal raw mode with `cfmakeraw()`
- Multiple F-key escape sequence formats handled
- Anti-aliased TrueType fonts via FreeType2

## Current Status: IN PROGRESS

### Completed
- ✅ Created unified typewrite app with FreeType2 integration (`typewrite/src/main.c`)
- ✅ Removed ruler, kept margins
- ✅ Implemented double buffering (backbuffer → memcpy to fb)
- ✅ Fixed cursor drawing (small 3px underline instead of full block)
- ✅ Fixed terminal raw mode with `cfmakeraw()`
- ✅ Added multiple F-key escape sequence formats
- ✅ Implemented Markdown save/load with `~~strikethrough~~`
- ✅ Wrote unit tests for document logic (8 tests passing)
- ✅ Created `start-qemu.sh` launcher script
- ✅ Created `RUNNING_ON_HOST.md` for Ubuntu testing
- ✅ Enabled FreeType and DejaVu fonts in Buildroot config
- ✅ Created Buildroot package (`buildroot-2024.02/package/typewrite/`)
- ✅ Created QEMU x86_64 board configs
- ✅ Git initialized with essential files staged (16 files)

### In Progress
- 🔄 Buildroot build compiling (~1800+ files done, kernel modules phase)

### Remaining
- ⏳ Wait for Buildroot build completion
- ⏳ Test app in QEMU
- ⏳ Commit to git

## Git Tracked Files (16 total)
```
.gitignore
REQUIREMENTS.md
RUNNING_ON_HOST.md
buildroot-2024.02/.config
buildroot-2024.02/board/qemu/x86_64/linux.config
buildroot-2024.02/board/qemu/x86_64/post-build.sh
buildroot-2024.02/board/qemu/x86_64/readme.txt
buildroot-2024.02/package/typewrite/Config.in
buildroot-2024.02/package/typewrite/typewrite.mk
start-qemu.sh
typewrite/Makefile
typewrite/src/framebuffer.c
typewrite/src/main.c
typewrite/src/sdl_main.c
typewrite/src/simple.c
typewrite/tests/test_document.c
```

## Build Progress
- Started: Mar 19 06:47
- Log: `/tmp/build5.log`
- Phase: Linux kernel compilation (modules)

## How to Run Tests
```bash
cd /ironwolf4TB/data01/projects/typewrite_os/typewrite
gcc -Wall -O2 -I/usr/include/freetype2 tests/test_document.c -lfreetype -lm -o tests/test_document
./tests/test_document
```

## How to Test on Ubuntu (Host)
```bash
cd /ironwolf4TB/data01/projects/typewrite_os/typewrite
make clean && make
sudo ./typewrite
```

## Key Bug Fixes History
1. **F-keys not working** - Fixed with `cfmakeraw()` and multiple sequence formats
2. **Typing only 1 char per line** - Terminal wasn't in proper raw mode (fixed)
3. **Cursor stretches full width** - Changed from block cursor to 3px underline
4. **Font scaling bug** - `get_char_width()` returns scaled value, cursor was doubling width
5. **Backspace cursor position** - Was advancing cursor after marking strikethrough (fixed)
