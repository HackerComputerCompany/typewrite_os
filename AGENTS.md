# AGENTS.md — Context for humans and coding agents

Use this file when you (or an assistant) return to the repo after time away. It summarizes **what this project is**, **what is buildable today**, and **where to read more**.

## Purpose

**Typewrite OS** is a distraction-free, typewriter-style writing environment (Freewrite-inspired): minimal UI, typewriter semantics, document persistence.

## Implementation tracks (important)

| Track | Role | Buildable in this repo? |
|--------|------|-------------------------|
| **Native UEFI** | `Typewriter.efi` — boots from firmware, no Linux | **Yes** — primary work under `uefi-app/` |
| **Linux X11** | Host editor: shared `TwDoc` core, Cairo PDF, same bitmap fonts | **Yes** — `linux-typewrite-x11/`; **`.deb`** via `debian/` / `linux-typewrite-x11/build-deb.sh` |

The **vendored Buildroot** tree and **`install-to-usb*.sh`** scripts that produced a full Linux USB image were **removed** (2026) to keep the repository small. **`FEATURES.md`** still describes an older framebuffer product; cross-check **`MILESTONE.md`**, **`REQUIREMENTS.md`**, and **`linux-typewrite-x11/README.md`** for what is implemented today.

**Authoritative “what works now”** for firmware: **`MILESTONE.md`**, `uefi-app/README.md`, **`fonts/README.md`**, `BUILD_SYSTEM.md`, `GRAPHICS_DEBUG.md`.

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

- **`GRAPHICS_DEBUG.md`**: Bitmap font decode is fixed (proportional stride, baseline via `bitmap_top`, etc.). **Direct GOP framebuffer** drawing (off-screen pool + `CopyMem` reverted — black screen on some OVMF builds). **GOP pitch** must not be 0 (`PixelsPerScanLine` clamp). **Apple** uses pool + **`Blt`**; **non-Apple** direct FB. **F1 menu → Resolution** cycles modes with **try / 10s confirm / auto-revert**; **`gop_mode`** in **`Typewriter.settings`** when kept. Typing uses **incremental** stripe repaint; future **`Blt()`** for non-Apple if needed.

## Key paths

| Path | Contents |
|------|-----------|
| `uefi-app/main.c` | Main UEFI application |
| `uefi-app/Makefile` | EFI build; default **ship** (git); `make all` for compile only |
| `uefi-vi/` | **`UefiVi.efi`** — text-console vi-like editor (no GOP); `make all` only |
| `uefi-menu/` | **`BootMenu.efi`** — text menu: Typewriter, UefiVi, TIC80 (`LoadImage`); exit on 4 |
| `tic80-uefi/` | **`TIC80.efi`** — TIC-80 UEFI scaffold (`make`); full port links TIC-80 static libs built with `-DTIC80_UEFI=ON` |
| `fonts/README.md` | **Nine UI fonts** (F2): sources, licenses, `convert_font.py` workflow |
| `fonts/*.h`, `fonts/*.ttf` | Bitmap headers + upstream TTFs (Virgil, Inter, six OFL/Apache faces, etc.) |
| `uefi-app/fs/` | QEMU FAT contents (e.g. copied `Typewriter.efi`) |
| `linux-typewrite-x11/` | X11 + Cairo client; **`build-deb.sh`** for Debian package |
| `debian/` | Source packaging for **`x11typewrite`** `.deb` |
| `start-qemu.sh` | QEMU launcher |
| `write-typewriter-to-usb.sh`, `install-uefi-app.sh` | UEFI USB / ESP deployment |
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
- **`TIC80_UEFI_PORT.md`** — Approach + status for porting [TIC-80](https://github.com/nesbox/TIC-80) to UEFI (`tic80-uefi/`, `-DTIC80_UEFI=ON`)
- **`THREAD_CONTEXT.md`** — Handoff when pausing TIC-80 UEFI; **next**: Linux framebuffer typewriter, then X11 (see file)

## Conventions for agents

- Prefer **small, task-focused diffs**; do not “clean up” unrelated code.
- After changing the EFI build, verify **`Typewriter.efi`** is still valid PE32+ (see `BUILD_SYSTEM.md`).

Last reviewed: **2026-04-04** (Buildroot tree removed; Linux path = X11 + `.deb`).
