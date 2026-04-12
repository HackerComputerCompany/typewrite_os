# AGENTS.md — Context for humans and coding agents

Use this file when you (or an assistant) return to the repo after time away. It summarizes **what this project is**, **what is buildable today**, and **where to read more**.

## Purpose

**Typewrite OS** is a distraction-free, typewriter-style writing environment (Freewrite-inspired): minimal UI, typewriter semantics, document persistence.

## Two Projects (Important)

This repo contains **two separate projects** that share the `fonts/` directory:

| Project | Location | What it builds |
|---------|----------|----------------|
| **TypewriteApp** | `TypewriteApp/x11/` | Linux X11 app with SDL2 + Cairo |
| **HackerComputerCompanyOS** | `HackerComputerCompanyOS/app/` | UEFI firmware (Typewriter.efi) |

### TypewriteApp (X11)

```bash
cd TypewriteApp/x11
make    # builds x11typewrite (x86_64)
```

Requires: `libx11-dev`, `libcairo2-dev`, `gnu-efi` (sibling to repo: `../../../gnu-efi`).

Output: `x11typewrite` + copy `x11typewrite-x86_64`.

Portable tarball:
```bash
make portable   # → TypewriteApp/x11/dist/
```

### HackerComputerCompanyOS (UEFI)

```bash
cd HackerComputerCompanyOS/app
make    # builds Typewriter.efi
```

Requires: **gnu-efi** sibling to repo (`../gnu-efi`).

QEMU test:
```bash
./start-qemu.sh    # builds app, syncs to fs/, launches QEMU
```

## Shared Assets

- `TypewriteApp/fonts/` — 9 bitmap fonts (Virgil, Inter, Special Elite, etc.)
- `TypewriteApp/sounds/` — WAV sound effects

## Key Paths

| Path | Contents |
|------|-----------|
| `TypewriteApp/x11/src/main_x11.c` | X11 application |
| `TypewriteApp/x11/Makefile` | X11 build config |
| `TypewriteApp/lib/src/tw_core.c` | Shared document core |
| `HackerComputerCompanyOS/app/main.c` | UEFI application |
| `HackerComputerCompanyOS/app/Makefile` | EFI build config |

## Docs

- `TypewriteApp/x11/README.md` — X11 build and usage
- `HackerComputerCompanyOS/app/README.md` — UEFI build
- `TypewriteApp/fonts/README.md` — Font sources and licenses
- `BUILD_SYSTEM.md` — EFI build details (PE32+, objcopy)
- `GRAPHICS_DEBUG.md` — GOP/framebuffer notes

## Previous Folder Structure (Pre-2026-04)

The old structure had all code in root-level folders (`uefi-app/`, `linux-typewrite-x11/`, etc.). The split to `TypewriteApp/` and `HackerComputerCompanyOS/` was done on branch `dev-divide`.

## Conventions

- Prefer **small, task-focused diffs**
- After EFI changes, verify PE32+ validity (see `BUILD_SYSTEM.md`)