# Typewrite OS — Project status

## Goal

A focused typewriter experience (Freewrite-inspired): minimal chrome, typewriter semantics, persistence. The same product idea is pursued on **two stacks**; only one is fully present in this checkout.

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

## Parallel track: Linux + Buildroot (legacy / on hold in this tree)

Originally: minimal **x86_64** image with framebuffer typewrite app (FreeType, double buffering, markdown + strikethrough, F-key features). Artifacts still in repo:

- Vendored [`buildroot-2024.02/`](buildroot-2024.02/)
- Package recipe [`buildroot-2024.02/package/typewrite/`](buildroot-2024.02/package/typewrite/)
- Board overlays under `buildroot-2024.02/board/typewrite/`

### Gap in this repository

The Buildroot recipe uses **`TYPEWRITE_SITE = $(TOPDIR)/../typewrite`** (the **`typewrite/`** directory at the **repo root**, next to **`buildroot-2024.02/`**). That **`typewrite/` directory is not present** in this workspace. To resume the Linux build:

1. Restore the `typewrite` sources under the repo root (or change `TYPEWRITE_SITE` in `typewrite.mk`), then
2. Run a normal Buildroot build with the intended defconfig.

Historical detail (FreeType app, unit tests, host `make`) is preserved in older notes below for reference.

---

## Historical: Linux app milestones (when `typewrite/` existed)

The following applied when the standalone Linux application lived at `typewrite/`:

- Unified typewrite app with FreeType2, double buffering, raw terminal mode, markdown save/load with `~~strikethrough~~`, document unit tests.
- Buildroot package and QEMU board configs wired for that tree.

Host test commands (only valid if you restore `typewrite/`):

```bash
cd typewrite
gcc -Wall -O2 -I/usr/include/freetype2 tests/test_document.c -lfreetype -lm -o tests/test_document
./tests/test_document
```

```bash
cd typewrite
make clean && make
sudo ./typewrite
```

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
