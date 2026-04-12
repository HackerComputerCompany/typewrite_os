# Typewrite OS - Running on Existing Linux Systems

This file explains how to compile and run Typewrite on a standard Linux system (tested on Ubuntu 24.04).

## Prerequisites

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y libfreetype6-dev fonts-dejavu-core gcc make
```

### Fedora/RHEL
```bash
sudo dnf install -y freetype-devel dejavu-serif-fonts gcc make
```

### Arch Linux
```bash
sudo pacman -S freetype2 fontconfig dejavu-gui gcc make
```

## Compiling

```bash
cd typewrite

# Compile with pkg-config (recommended)
gcc -Wall -O2 $(pkg-config --cflags freetype2) src/main.c $(pkg-config --libs freetype2) -lm -o typewrite

# Or manually specify paths (if pkg-config unavailable)
gcc -Wall -O2 -I/usr/include/freetype2 src/main.c -lfreetype -lm -o typewrite
```

## Running

Typewrite requires a framebuffer device. On a real system, this means running from a Linux console (not X11/Wayland).

### Option 1: Direct Framebuffer (Best for Testing)
```bash
# Switch to a virtual console: Ctrl+Alt+F1-F6
# Login, then run:

cd typewrite
sudo ./typewrite
```

### Option 2: Framebuffer via DRM (Modern Systems)
```bash
# Some systems use DRM instead of legacy fb0
# Try this if /dev/fb0 doesn't work:

sudo modprobe uvesafb
./typewrite
```

### Option 3: QEMU with Framebuffer
```bash
# Install QEMU
sudo apt install -y qemu-system-x86

# Run in QEMU with framebuffer
qemu-system-x86_64 \
  -kernel bzImage \
  -initrd rootfs.cpio.gz \
  -append "console=tty0" \
  -vga std \
  -display sdl
```

## Expected Output

On successful startup, you should see:
```
=== Typewrite ===
Framebuffer: 1024x768
Font: /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf at 24pt (anti-aliased)
Loaded: /root/document.md
Ready! F1:ink F2/F3:scale F4:dark | Ctrl+S:save Ctrl+Q:quit
```

## Troubleshooting

### "Cannot open /dev/fb0: Permission denied"
```bash
# Add user to video group
sudo usermod -a -G video $USER
# Log out and back in
```

### "No display available!"
- You're likely running in X11/Wayland. Switch to a virtual console (Ctrl+Alt+F1-F6).

### "Cannot load font"
```bash
# Verify fonts are installed
ls /usr/share/fonts/truetype/dejavu/

# If missing, install
sudo apt install -y fonts-dejavu-core
```

### "FreeType init failed"
```bash
# Verify freetype is installed
dpkg -l | grep freetype
```

## Keyboard Controls

| Key | Action |
|-----|--------|
| Type | Insert character |
| Backspace | Strikethrough character |
| Enter | New line |
| Arrow Keys | Move cursor |
| F1 | Cycle ink color |
| F2 | Decrease text size |
| F3 | Increase text size |
| F4 | Toggle dark mode |
| Ctrl+S | Save document |
| Ctrl+Q | Quit |

## Document Format

Documents are saved as Markdown with strikethrough:
```
Hello ~~deleted~~ world
```

## For Embedded/SoC Development

For production on custom hardware, see **`REQUIREMENTS.md`** and track-specific READMEs (UEFI vs Linux). A vendored **Buildroot** tree is **not** included in this repository anymore (2026).
