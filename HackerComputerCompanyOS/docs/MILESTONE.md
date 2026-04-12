# Milestone — UEFI typewriter (beta)

**Date:** March 2026  

This milestone marks a **usable beta** of the native **UEFI** editor (`Typewriter.efi`): firmware boots to a graphical GOP typewriter with proportional fonts, stable rendering on QEMU and common hardware paths, and a workflow that keeps the binary and sources in sync with the repo.

## What works

- **GOP text editor** — `uefi-app/main.c`: **page grid** (up to **80×60** storage; **50–65** active columns with Letter margins), cursor, arrows, **PgUp/PgDn** pages, Enter / Tab / Backspace / printable BMP; **splash**, **session timer** HUD, **slot/page** files (`TWSnPmm.TXT`), autoload **`Typewriter.txt`**, **`Typewriter.settings`**.
- **F1 settings menu** — Font, scale (half-steps **1.0×–6.0×**), background, cursor (five modes), key-debug, line numbers, margins, chars/line, save/load, slot/page navigation, shutdown, **GOP resolution (try next)** with **10 s confirm** (Space/Enter keep + save `gop_mode`; ESC/timeout revert). See [`uefi-app/README.md`](uefi-app/README.md).
- **Fonts** — **Nine faces** from the menu: Virgil, Inter (sans), **Special Elite**, **Courier Prime**, **VT323**, **Press Start 2P**, **IBM Plex Mono**, **Share Tech Mono**, **simple**; `fonts/convert_font.py`, **`bitmap_top`** baselines. [`fonts/README.md`](fonts/README.md).
- **Spacing** — space character advance **3×** (`SPACE_ADVANCE_MULT`); column pitch from printable width.
- **Flicker** — **incremental** row bands where possible; full repaints for menu, resolution changes, and layout toggles.
- **Input** — **SCAN_ESC** for quit (not **Up**); optional key-debug from menu.
- **GOP** — **Apple**: pool + **`Blt`**; **else** direct FB. **`TwReinitFramebufferFromGopMode`** after **`SetMode`** (boot or resolution flow). Saved **`gop_mode`** ignored if out of range on this firmware.
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

- `FEATURES.md` still describes an older full Linux framebuffer product; the in-repo Linux client is **`linux-typewrite-x11`**. UEFI behavior is **grid / simplified** vs that document.
- High-DPI **PNG/TIFF export**, **multi-partition USB** layout for a separate data volume, and other installer-scale features remain **future work**.

---

*Treat this file as the snapshot summary for demos and handoffs; update it when the next milestone lands.*
