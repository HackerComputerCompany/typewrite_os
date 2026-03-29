# Typewrite OS — UEFI application

Native **UEFI** build of Typewrite: boots without Linux, talks to **GOP**, **ConIn**, and firmware services via gnu-efi.

> **Build troubleshooting (PE32+, objcopy):** see [`../BUILD_SYSTEM.md`](../BUILD_SYSTEM.md).  
> **Framebuffer / hardware drawing issues:** see [`../GRAPHICS_DEBUG.md`](../GRAPHICS_DEBUG.md).  
> **Repo-wide context:** see [`../AGENTS.md`](../AGENTS.md).

## Architecture

```mermaid
graph TB
    subgraph "Traditional Linux Boot"
        A[BIOS/UEFI] --> B[GRUB Bootloader]
        B --> C[Linux Kernel]
        C --> D[InitRAMFS]
        D --> E[BusyBox/Userland]
        E --> F[Typewrite App]
    end

    subgraph "UEFI App Approach"
        G[BIOS/UEFI] --> H[UEFI Shell]
        H --> I[Typewriter.efi]
    end

    subgraph "Typewrite UEFI App"
        I --> J[EFI System Table]
        I --> K[EFI Boot Services]
        I --> L[EFI Runtime Services]
        J --> M[Console I/O]
        J --> N[Graphics Output]
        J --> O[File System]
        K --> P[Memory Management]
        K --> Q[Event/Timer]
    end
```

## Build flow

```mermaid
flowchart LR
    A[main.c] --> B[gcc -fpic]
    B --> C[main.o]
    C --> D[ld -shared]
    D --> E[main.so]
    E --> F[objcopy]
    F --> G[Typewriter.efi]
    
    H[gnu-efi libs] --> D
    I[crt0-efi-x86_64.o] --> D
```

## Build

```bash
cd uefi-app
make          # default: build, sync fs/Typewriter.efi, commit+push if uefi-app/ changed
make all      # compile only (no git); used by ../start-qemu.sh
make ship     # same as bare make
make clean    # remove objects and .efi (does not run git)
```

Optional: `make ship MSG="Short commit subject"` (default subject is `uefi-app: build Typewriter.efi`).

**gnu-efi location:** `Makefile` sets `EFIDIR` to **`$(repo)/../gnu-efi`** by default. Override with `export EFIDIR=...` or `make EFIDIR=...` if your tree is elsewhere.

For QEMU, run [`../start-qemu.sh`](../start-qemu.sh) from the repo root (it runs **`make -C uefi-app all`**, then copies **`Typewriter.efi`** into **`fs/`**). For USB/ESP, run [`../write-typewriter-to-usb.sh`](../write-typewriter-to-usb.sh) `/dev/sdX` (build + sync + installer), or call [`../install-uefi-app.sh`](../install-uefi-app.sh) directly if `uefi-app/fs/Typewriter.efi` is already up to date.

## Current status

### Working

- **Valid PE32+** UEFI application (firmware loads it; prior “Unsupported format” came from bad `objcopy`/link — fixed per `BUILD_SYSTEM.md`).
- **QEMU + OVMF** with FAT payload under `fs/` (e.g. `startup.nsh`).
- **GOP**: mode set, framebuffer base/pitch; large region fills verified on QEMU and some real hardware.
- **Bitmap fonts** from `fonts/convert_font.py`; **`Doc.Modified`** drives redraw (incremental row updates where possible; see [`../GRAPHICS_DEBUG.md`](../GRAPHICS_DEBUG.md)).
- **US Letter–style page**: fixed **50×60** character grid (**3000** cells), **~1"** margins at **96 logical DPI** (clamped on small displays), **black** outside the paper, centered horizontally. Column pitch is derived from the font so line length is consistent. **Vertical scroll** (`Doc.ScrollYPx`) keeps the caret in view when the scaled page is taller than the area above the status HUD.
- **Font scale**: **half-step** zoom (**1.0× … 6.0×**, stored as `FontScaleTwice` = 2…12).

### Open

- Retest **raw framebuffer** behavior on picky hardware; consider GOP **`Blt()`** if needed.
- Feature growth toward the behavior described in [`../FEATURES.md`](../FEATURES.md) (save/load, typewriter rules) — implemented incrementally in `main.c`.

## Keys (in the graphical editor)

| Key | Action |
|-----|--------|
| **F1** | Toggle on-screen **help** |
| **F2** | **Cycle font** (9): Virgil, Inter (sans), Special Elite, Courier Prime, VT323, Press Start 2P, IBM Plex Mono, Share Tech Mono, simple — see [`../fonts/README.md`](../fonts/README.md) |
| **F3** / **F6** | Increase / decrease **font scale** in **half steps** (effective **1.0× … 6.0×**). At end of a row, further typing wraps to the next row of the grid. |
| **F4** | Cycle background color |
| **F5** | **Cycle cursor:** bar (solid) → bar (blink ~0.5s) → block (solid) → block (blink) → hidden |
| **F7** | Toggle **key debug**: last 8 keys as `SC=` scan code / `UC=` Unicode on-screen and on the firmware **serial** console (`[KeyDbg]`) |
| **F8** | **Save** the document as **`Typewriter.txt`** (UTF-8, LF line endings) on the **boot volume** (same FAT volume as the `.efi`). Status appears in the bottom HUD and on serial (`[Typewrite] save`). |
| **F9** | **Load** **`Typewriter.txt`** from the boot volume (replaces the in-memory buffer). |
| **ESC** | Close help if open; otherwise **exit** (uses UEFI **SCAN_ESC** `0x0017`, not Up Arrow `0x0001`) |

### Status HUD (clock)

A **centered** **HH:MM** readout uses a fixed **7-segment** style (not the document font), gray LCD look. Time comes from UEFI **`RT->GetTime`** when the firmware provides it; otherwise **88:88**-style placeholders. The clock is **refetched about every 45 seconds**; the HUD repaints on that tick, on **full** document clears, or when a **save/load** banner appears or expires. File status uses the built-in simple font on the **left** of the status row.

### Page memory model (`main.c`)

- **`Doc.Grid[PAGE_ROWS][PAGE_COLS+1]`** — one page, space-padded rows, NUL after column 50.
- **`TYPEWRITER_PAGE`** matches that grid shape for a future **multi-page** buffer (only one page is loaded in RAM today).
- **Save** trims **trailing blank lines**; **load** fills the grid and places the caret after the last character.

## Source layout

| File | Role |
|------|------|
| `Makefile` | `TARGET = Typewriter.efi`, gnu-efi link + objcopy |
| `main.c` | Application entry, GOP, fonts, input loop |
| `main_minimal.c` | Alternate minimal sketch (not the default `make` target) |
| `../fonts/virgil.h`, `../fonts/helvetica.h` | Font bitmap data (built with `fonts/convert_font.py`) |
| `fs/` | FAT contents for QEMU (copy `Typewriter.efi`, `startup.nsh`, etc.) |

## Technical notes

### objcopy (summary)

Include `.text`, `.sdata`, `.data`, `.dynamic`, `.rodata`, `.rel`, `.rela`, **`.reloc`**, and use:

`--target efi-app-x86_64`

(not `-O efi-app-x86_64` — see `BUILD_SYSTEM.md`).

### UEFI subsystem IDs

- 10 = EFI_APPLICATION  
- 11 = EFI_BOOT_SERVICE_DRIVER  
- 12 = EFI_RUNTIME_DRIVER  

## Resources

- [Rod Smith's EFI Programming Guide](http://www.rodsbooks.com/efi-programming/)
- [OSDev Wiki — GNU-EFI](https://wiki.osdev.org/GNU-EFI)
- [GNU-EFI (GitHub)](https://github.com/pbatard/gnu-efi)
