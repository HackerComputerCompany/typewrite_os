# Typewrite OS — Requirements

## Project overview

**Typewrite OS** is a distraction-free, typewriter-style writing environment (Freewrite-inspired): minimal chrome, typewriter semantics, and plain-text persistence.

This repository ships **multiple implementation tracks**. Treat **`AGENTS.md`**, **`MILESTONE.md`**, and track-specific READMEs as the live index; this document summarizes **requirements and behavior** that exist **in tree today**.

| Track | Location | Role |
|--------|-----------|------|
| **UEFI firmware app** | `uefi-app/` | Primary active target: `Typewriter.efi`, GOP display, bitmap fonts, editor. |
| **Linux X11 bring-up** | `linux-typewrite-x11/` | Host-side editor using shared core + same font headers; PDF export, toasts. |
| **Shared text core** | `linux-typewrite/src/` | `TwCore` grid, **`TwDoc`** multi-page document, save/load. |
| **Linux framebuffer app** | `linux-typewrite/` (non-X11) | Optional fbdev path; shares `tw_core` / `tw_doc`. |
| **Buildroot image** | `buildroot-2024.02/` | Minimal distro recipes; **application sources** for the original `typewrite` package may live outside this repo (see `AGENTS.md`). |

---

## UEFI application (`uefi-app/`)

### Platform

- x86_64 UEFI; GOP (graphics) for display.
- Toolchain: gnu-efi, PE32+ output per **`BUILD_SYSTEM.md`** (linker flags + `objcopy --target efi-app-x86_64`).

### Product requirements (summary)

- Bitmap fonts from `fonts/*.h` (multiple faces; selection in app).
- Multi-page or single-grid editing, persistence to FAT/ESP (see **`uefi-app/README.md`**, **`MILESTONE.md`**).
- Settings file (`Typewriter.settings`) for mode/font preferences where implemented.
- F-key menus, line numbers, margins, and UEFI-specific behavior documented in **`FEATURES.md`** (product-oriented; some items are Linux-oriented—cross-check **`MILESTONE.md`** for firmware).

### Graphics constraints

- GOP pitch and framebuffer path per **`GRAPHICS_DEBUG.md`** (direct FB vs `Blt`, mode confirm/revert, etc.).

---

## Shared editor core (`linux-typewrite/src/`)

### `TwCore`

- Fixed `cols` × `rows` grid of spaces; cursor `cx`, `cy`.
- Insert vs typeover affects `twdoc_putc` / backspace behavior (`TwDoc.insert_mode`).

### `TwDoc` (multi-page)

- Stack of pages (`TwCore` per page); grow on newline/wrap past last row.
- **Word wrap** (`word_wrap`, default on): when a typed line reaches the right edge, if there is a **break space** and the **next grid row is empty**, the fragment after that space moves to the next row; otherwise behavior matches **hard** wrap. Toggle from the X11 app (**F10**). Not persisted in the `.txt` file.
- **Save format**: plain text, **newline per row** (trailing spaces on a row stripped in output). Pages separated by ASCII **form feed** (`\f`, Ctrl+L).
- **Trailing trim (required)**: On save, **trailing whitespace-only rows** at the bottom of each page and **trailing wholly empty pages** are **not** written. A blank page **between** non-blank pages is preserved by emitting one newline per grid row on that page before the next `\f`, so page boundaries remain stable.
- **Reflow**: `twdoc_resize_reflow` flattens with the **same** rules as save so window resize does not create spurious pages from unused grid padding.
- **Load**: interprets `\f` as page break; `\r` ignored; `\t` → four spaces; printable ASCII only for typed characters.

---

## Linux X11 application (`linux-typewrite-x11/`)

### Dependencies

- **Xlib** (`libx11-dev`).
- **Cairo** (`libcairo2-dev`) for **Ctrl+P** PDF export (`pkg-config cairo`).

### Display and rendering

- 32-bit pixmap; UEFI bitmap fonts via gnu-efi include path (see `Makefile`).
- **F4**: background schemes (aligned with UEFI palette set).
- **F5**: gutter off / ascending line index / rows-remaining.
- **F6**: Letter-style margins vs full-width (up to 80 columns).
- **F7**: characters per line 50–65 when margins on (UEFI parity).
- **F8**: typewriter view (bottom-anchored grid + red rule on active row).
- **F11** / `--fullscreen**: EWMH fullscreen (WM may steal F11).

### Document and persistence

- Uses **`TwDoc`** / **`tw_core`**; reflow on resize.
- **Ctrl+S**: save; default filename `Typewriter.txt` when unset.
- **Autosave**: every **5 minutes** when a filename is set and the buffer is dirty.
- **Ctrl+P**: raster PDF (Cairo), one A4 page per document page; layout options mirror margins/gutter/background; no cursor or typewriter transform in PDF.
- **Ctrl+Q** / **Ctrl+X**: save and exit.

### Toasts

- **Action toasts** (save, PDF, F-keys, Insert, etc.): footer band, left of **Page N of M**; reveal speed follows **learned typing pace**; then fade toward paper color.
- **Status toast** (**F9** cycles interval: 1 / 5 / 10 / 15 / 30 minutes, 1 hour): **top margin** band; fields **`wpm | session words | doc words | HH:MM`** (session = 5-keystroke units typed this run; doc = whitespace word count over all pages).

### Keyboard (X11)

| Key | Action |
|-----|--------|
| F1 | Help overlay |
| F2 | Cycle font |
| F3 | Cycle cursor mode |
| F4 | Cycle background |
| F5 | Cycle gutter mode |
| F6 | Toggle margins |
| F7 | Cycle chars/line (margins on) |
| F8 | Toggle typewriter view |
| F9 | Cycle status toast interval |
| F10 | Toggle word wrap (soft break at last space when next row empty) |
| F11 | Toggle fullscreen (EWMH) |
| Tab | Four spaces |
| Insert | Toggle insert / typeover (default **typeover** in `tw_doc`) |
| Arrows, Home/End, PgUp/PgDn | Move cursor / page |
| Ctrl+S | Save |
| Ctrl+P | Export PDF |
| Ctrl+Q / Ctrl+X | Save and quit |
| Enter | Newline (multi-page per `TwDoc`) |

Full detail: **`linux-typewrite-x11/README.md`**.

---

## Buildroot / distribution image (optional)

When the full image is built:

- **Framework**: Buildroot 2024.02, x86_64, Linux kernel and ext2 rootfs as configured in-tree.
- **QEMU**: **`./start-qemu.sh`** builds/syncs the UEFI app by default; see **`BUILD_SYSTEM.md`** for KVM, OVMF, and display options.
- **Typewrite Linux package**: expects application sources at **`typewrite/`** next to Buildroot or **`TYPEWRITE_SITE`** override—see **`AGENTS.md`**.

---

## Historical / design reference (not all vendored)

**`FEATURES.md`** describes a richer Linux framebuffer product (FreeType, `.md` + `.ink` per-character color/bold, zoom, resolution cycling, strikethrough backspace, etc.). That stack is **design and history**; it is **not** what `linux-typewrite-x11` implements today. Use **`FEATURES.md`** for product ideas; use this file and track READMEs for **current** requirements.

---

## Version history (repository)

- **2026-04**: `tw_doc` trailing blank trim on save/flatten; `linux-typewrite-x11` PDF, toasts, status pulse, 5-minute autosave, default typeover—see **`BUILD_SYSTEM.md`** changelog.
- Earlier entries in this file referred to a **v0.1–v0.4** framebuffer app timeline; sources may live outside this repo.

## License

MIT License (see repository root if present).
