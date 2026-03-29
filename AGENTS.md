# AGENTS.md — Context for humans and coding agents

Use this file when you (or an assistant) return to the repo after time away. It summarizes **what this project is**, **what is buildable today**, and **where to read more**.

## Purpose

**Typewrite OS** is a distraction-free, typewriter-style writing environment (Freewrite-inspired): minimal UI, typewriter semantics, document persistence.

## Two implementation tracks (important)

| Track | Role | Buildable in this repo? |
|--------|------|-------------------------|
| **Native UEFI** | Single `Typewriter.efi` — boots from firmware, no Linux | **Yes** — primary active work under `uefi-app/` |
| **Linux + Buildroot** | Minimal distro + framebuffer typewrite app (FreeType, etc.) | **Partially** — Buildroot tree and package recipes exist; the **`typewrite/` application source tree is not present** here. `typewrite.mk` uses **`TYPEWRITE_SITE = $(TOPDIR)/../typewrite`** (repo root); restore sources there or change `TYPEWRITE_SITE` before expecting Buildroot to build the app. |

Documentation elsewhere may still describe only the Linux path (`PROJECT_STATUS.md` history, `FEATURES.md`). **Authoritative “what works now”** for firmware: **`MILESTONE.md`**, `uefi-app/README.md`, **`fonts/README.md`** (faces + licenses), `BUILD_SYSTEM.md`, `GRAPHICS_DEBUG.md`.

## Quick start (UEFI app)

```bash
cd uefi-app
make    # default: build + ship (commit/push if uefi-app/ changed); make all = compile only
```

Requirements:

- **gnu-efi**: `uefi-app/Makefile` defaults `EFIDIR` to a **sibling** directory `../gnu-efi` (next to the repo). Override with `export EFIDIR=/path` or `make EFIDIR=/path all` if your clone lives elsewhere.
- **gcc/ld/objutils** for `x86_64`, as documented in `BUILD_SYSTEM.md`.

QEMU + OVMF: from repo root run **`./start-qemu.sh`** — it **builds** `uefi-app`, **syncs** `Typewriter.efi` → `uefi-app/fs/`, then launches QEMU. Use `./start-qemu.sh --help`; full options in [`BUILD_SYSTEM.md`](BUILD_SYSTEM.md#testing).

## Critical build detail (do not regress)

Invalid PE output used to make the firmware report **“Unsupported format”**. The fix is **linker flags + `objcopy --target efi-app-x86_64`** (not `-O efi-app-x86_64`) and keeping **`.reloc`**. Full analysis: **`BUILD_SYSTEM.md`**.

## Open problems (UEFI / graphics)

- **`GRAPHICS_DEBUG.md`**: Bitmap font decode is fixed (proportional stride, baseline via `bitmap_top`, etc.). **Direct GOP framebuffer** drawing (off-screen pool + `CopyMem` reverted — black screen on some OVMF builds). **GOP pitch** must not be 0 (`PixelsPerScanLine` clamp). Typing uses **incremental** stripe repaint to limit flicker; future **`Blt()`** possible.

## Key paths

| Path | Contents |
|------|-----------|
| `uefi-app/main.c` | Main UEFI application |
| `uefi-app/Makefile` | EFI build; default **ship** (git); `make all` for compile only |
| `fonts/README.md` | **Nine UI fonts** (F2): sources, licenses, `convert_font.py` workflow |
| `fonts/*.h`, `fonts/*.ttf` | Bitmap headers + upstream TTFs (Virgil, Inter, six OFL/Apache faces, etc.) |
| `uefi-app/fs/` | QEMU FAT contents (e.g. copied `Typewriter.efi`) |
| `buildroot-2024.02/` | Vendored Buildroot; custom `package/typewrite/`, boards |
| `start-qemu.sh` | QEMU launcher |
| `write-typewriter-to-usb.sh`, `install-uefi-app.sh`, `install-to-usb*.sh` | USB / ESP deployment helpers |
| `ARCHITECTURE.md` | USB layout, UEFI component view |
| `FEATURES.md` | Product behavior (mostly Linux app design) |

## Doc index

- **`MILESTONE.md`** — Beta milestone summary (UEFI editor capabilities, March 2026)
- **`fonts/README.md`** — Font inventory, attribution, regenerating `*.h` from TTF
- **`BUILD_SYSTEM.md`** — PE32+ / objcopy / linker, QEMU invocation
- **`GRAPHICS_DEBUG.md`** — GOP / framebuffer test status
- **`uefi-app/README.md`** — UEFI experiment overview and build flow
- **`PROJECT_STATUS.md`** — High-level status (Linux + UEFI), updated for current repo state
- **`FEATURES.md`** — Keyboard shortcuts and typewriter rules (Linux-oriented but useful product spec)
- **`ARCHITECTURE.md`** — System and disk layout
- **`INSTALL.md`**, **`REQUIREMENTS.md`** — Setup and dependencies

## Conventions for agents

- Prefer **small, task-focused diffs**; do not “clean up” unrelated code.
- After changing the EFI build, verify **`Typewriter.efi`** is still valid PE32+ (see `BUILD_SYSTEM.md`).
- If adding Linux app sources back, put them at **`typewrite/`** next to **`buildroot-2024.02/`** (default **`TYPEWRITE_SITE`**) or update **`typewrite.mk`** and this file.

Last reviewed: **2026-03-29** (nine-font selection + `fonts/README.md`).
