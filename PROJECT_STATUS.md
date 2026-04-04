# Typewrite OS — Project status

## Goal

A focused typewriter experience (Freewrite-inspired): minimal chrome, typewriter semantics, persistence. The product is pursued on **UEFI** and **Linux X11** in this checkout.

## Current focus (2026): Native UEFI

**Primary active work:** [`uefi-app/`](uefi-app/) builds **`Typewriter.efi`** with gnu-efi.

### Done (UEFI)

- Valid **PE32+** EFI binary (fixed linker + `objcopy --target efi-app-x86_64`; see [`BUILD_SYSTEM.md`](BUILD_SYSTEM.md)).
- QEMU + OVMF path via [`start-qemu.sh`](start-qemu.sh) and FAT layout under `uefi-app/fs/`.
- GOP framebuffer bring-up; large fill tests work on QEMU and some real hardware.
- Embedded / linked font data (`virgil.h`, `helvetica.h`) and simple bitmap font in `main.c`.

### In progress / open (UEFI)

- Fine-grained drawing and glyph rendering on **real hardware** (flicker, wrong shapes); see [`GRAPHICS_DEBUG.md`](GRAPHICS_DEBUG.md). Likely next steps: cache/barriers, pitch verification, GOP `Blt()`.
- Full typewriter feature parity with the Linux app spec ([`FEATURES.md`](FEATURES.md)) — incremental.

### How to build / run (UEFI)

```bash
cd uefi-app
make
# Optional: ./start-qemu.sh from repo root
```

**Note:** `uefi-app/Makefile` defaults `EFIDIR` to **`../gnu-efi`** (sibling of this repo). Override with `export EFIDIR=...` if needed.

---

## Linux X11 track (in tree)

[`linux-typewrite-x11/`](linux-typewrite-x11/) builds **`x11typewrite`** on the host (Xlib + Cairo, shared **`TwDoc`** with UEFI). A **Debian/Ubuntu** binary package is defined under [`debian/`](debian/); see [`linux-typewrite-x11/README.md`](linux-typewrite-x11/README.md).

The old **Buildroot**-based minimal Linux image, **`typewrite/`** framebuffer app package, and **`install-to-usb*.sh`** installers for that image were **removed** from this repository (2026).

---

## Historical: Linux framebuffer + Buildroot (removed)

Earlier iterations used a vendored Buildroot tree, a **`typewrite/`** FreeType framebuffer application, and USB scripts that installed **`bzImage`** + **`rootfs.ext2`**. That stack is no longer maintained here; use **`FEATURES.md`** and git history if you need design reference.

---

## Documentation map

| Document | Purpose |
|----------|---------|
| [`AGENTS.md`](AGENTS.md) | **Start here** for agents / return visits |
| [`BUILD_SYSTEM.md`](BUILD_SYSTEM.md) | EFI toolchain, PE fix, QEMU |
| [`GRAPHICS_DEBUG.md`](GRAPHICS_DEBUG.md) | GOP / framebuffer debugging |
| [`uefi-app/README.md`](uefi-app/README.md) | UEFI experiment details |
| [`FEATURES.md`](FEATURES.md) | Product behavior (Linux-oriented spec) |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | USB / UEFI layout |

## Key bug-fix history (Linux app era)

1. F-keys — `cfmakeraw()` and multiple escape sequence formats  
2. One character per line — raw mode  
3. Cursor width — 3px underline instead of full block  
4. Font scaling — `get_char_width()` / cursor alignment  
5. Backspace + strikethrough — cursor position after mark  

Last updated: **2026-03-29**.
