# Milestone — UEFI typewriter (beta)

**Date:** March 2026  

This milestone marks a **usable beta** of the native **UEFI** editor (`Typewriter.efi`): firmware boots to a graphical GOP typewriter with proportional fonts, stable rendering on QEMU and common hardware paths, and a workflow that keeps the binary and sources in sync with the repo.

## What works

- **GOP text editor** — `uefi-app/main.c`: document buffer, cursor, Enter / Tab / Backspace / printable ASCII, **word wrap** at left/right margins.
- **Fonts** — **Nine faces** via **F2**: Virgil, Inter (sans), **Special Elite** (typewriter), **Courier Prime** (typewriter mono), **VT323** (terminal), **Press Start 2P** (8-bit), **IBM Plex Mono**, **Share Tech Mono**, plus built-in **simple**; all proportional/universal bitmaps from `fonts/convert_font.py` with **baseline-aligned** `bitmap_top` for descenders. Sources and licenses: [`fonts/README.md`](fonts/README.md).
- **Scale** — **F3 / F6** scale glyph “fat pixels” and advances (1–6); line step follows **line box + leading**.
- **Spacing** — word space width is **3×** the normal per-font advance; adjustable via `SPACE_ADVANCE_MULT` in `main.c`.
- **Flicker** — **incremental** repaint (dirty horizontal stripes) for normal typing; full clears for help, palette/font/scale changes, and first paint.
- **Input** — **SCAN_ESC** (`0x17`) for **ESC** (not confused with **Up**); **F7** key-debug overlay + serial `Print` for scan/Unicode diagnosis.
- **Cursor** — **F5** cycles five modes: bar solid, bar blink (~0.5 s), block solid, block blink, hidden.
- **Build & ship** — From `uefi-app/`, default **`make`** runs **ship**: build, sync `fs/Typewriter.efi`, **commit + push** when `uefi-app/` (and tracked root files) change. **`make all`** is compile-only (**`start-qemu.sh`** uses this).
- **Fonts in repo** — Canonical headers live under **`fonts/`** (`virgil.h`, `helvetica.h`); regenerate with `fonts/convert_font.py` after TTF changes.

## Quick verify

```bash
cd uefi-app && make all    # or: make  for build + ship if you changed sources
cd .. && ./start-qemu.sh
```

## References

| Doc | Role |
|-----|------|
| [`uefi-app/README.md`](uefi-app/README.md) | Keys, architecture diagrams, build notes |
| [`fonts/README.md`](fonts/README.md) | Nine bundled fonts, licenses, `convert_font.py` |
| [`BUILD_SYSTEM.md`](BUILD_SYSTEM.md) | PE32+, objcopy, QEMU |
| [`GRAPHICS_DEBUG.md`](GRAPHICS_DEBUG.md) | GOP pitch / framebuffer |
| [`AGENTS.md`](AGENTS.md) | Repo map and agent conventions |

## Not in scope for this milestone

- Linux **Buildroot** app sources are not bundled; `FEATURES.md` still skews Linux-product. UEFI behavior is **insert/overstrike-limited** (Backspace deletes on current line; not full Linux typewriter rules).
- Save/load to firmware variables or files, arrow-key cursor movement, and rich typewriter rules remain **future work**.

---

*Treat this file as the snapshot summary for demos and handoffs; update it when the next milestone lands.*
