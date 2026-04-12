# Thread context — pause (March 2026)

This file captures **where the TIC-80 UEFI work stopped** and what to do **next** on a **new thread** (Linux framebuffer typewriter, then X11).

## Completed on this thread (TIC-80 × UEFI)

- **`tic80-uefi/`** links **`TIC80.efi`** against TIC-80 static libs built with **`-DTIC80_UEFI=ON`** (sibling **`../TIC-80/build-uefi-smoke`** by default). Shims avoid glibc **libm** (IFUNC), duplicate **memcpy** with gnu-efi, and stub GIF/FFT paths as documented in **`TIC80_UEFI_PORT.md`**.
- **Runtime**: `tic_runtime.c` — **`tic80_tick` / `tic80_sound`**, RDTSC frame pacing, **`cart.tic`** on boot volume or **builtin** cart, GOP **`Blt`** with integer scale, ConIn input (arrows + Z/X/A/S, ESC).
- **Boot menu**: **`uefi-menu/BootMenu.efi`** — **`[3]`** launches **`\\efi\\boot\\TIC80.efi`**, **`[4]`** exits (was exit on 3 before). **`start-qemu.sh`** builds **`tic80-uefi`** when **`../TIC-80/build-uefi-smoke/lib/libtic80core.a`** exists and stages **`TIC80.efi`**. **`install-uefi-app.sh`** / **`write-typewriter-to-usb.sh`** optionally install **`TIC80.efi`** when present; **`refind.conf`** includes a TIC-80 entry.

## Paused

No further TIC-80 UEFI changes are required for this pause. Reopen **`TIC80_UEFI_PORT.md`**, **`tic80-uefi/README.md`**, and **`AGENTS.md`** for paths and build commands.

## Next goals (new thread — Linux typewriter)

User direction:

1. **Simple framebuffer Linux version** of the **typewrite** application (minimal dependency: e.g. `/dev/fb0` or KMS dumb buffer, bitmap fonts aligned with existing **`fonts/*.h`** where practical).
2. **X11 (or Xlib/XCB) version** — same product behavior, windowed desktop.

### Repo facts for implementation

- **`linux-typewrite/`** (fbdev) and **`linux-typewrite-x11/`** (Xlib + Cairo) share **`linux-typewrite/src/`** (`TwDoc`, `tw_core`). A **Debian** binary package is under **`debian/`**. The old **Buildroot** tree and **`typewrite/`** framebuffer package were **removed** from this repository (2026).
- **Authoritative product behavior** for typing UX: **`FEATURES.md`** (design/history), **`REQUIREMENTS.md`**, **`uefi-app/main.c`**, **`MILESTONE.md`**.
- **Fonts**: **`fonts/README.md`**, generated **`fonts/*.h`**, **`convert_font.py`**.

### Suggested order

1. Define a **small shared core** (buffer, cursor, typewriter rules) used by two front-ends.
2. **Framebuffer backend**: open fb, mmap, 32-bpp draw, input via **`/dev/input/event*`** (evdev) or stdin for first bring-up.
3. **X11 backend**: same core, **`XCreateWindow`**, expose events, optional MIT-SHM for speed.

---

*Written for handoff when switching from UEFI/TIC-80 work to Linux UI tracks.*
