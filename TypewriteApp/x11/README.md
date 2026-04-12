## linux-typewrite-x11

Simple X11/Xlib version of the typewrite bring-up app.

It **reuses** the shared core and UEFI bitmap fonts from `../linux-typewrite/src/` and `fonts/*.h` (via gnu-efi include path in the `Makefile`).

### Build

Requires Xlib and Cairo (e.g. Debian/Ubuntu: `sudo apt install libx11-dev libcairo2-dev`). The `Makefile` uses `pkg-config cairo` for compile and link flags (typically **dynamic** `-lcairo`; static linking is possible if you supply a full static Cairo stack).

```bash
make -C linux-typewrite-x11
```

`make all` produces **`x11typewrite`** (64-bit on typical x86_64 hosts) and a same-content copy **`x11typewrite-x86_64`** for packaging. Optional **32-bit x86** ELF (when multilib and `:i386` dev packages are installed):

```bash
make -C linux-typewrite-x11 i686   # writes x11typewrite-i686; phony target name avoids a make circularity
```

### Debian / Ubuntu `.deb` package

Packaging lives in the repo’s `debian/` directory (native source name `x11typewrite`). Build dependencies:

```bash
sudo apt install build-essential debhelper libcairo2-dev libx11-dev pkg-config gnu-efi
```

From the **repository root** (parent of `linux-typewrite-x11/`):

```bash
./linux-typewrite-x11/build-deb.sh
# or: dpkg-buildpackage -us -uc -b
```

`dpkg-buildpackage` writes `x11typewrite_<version>_amd64.deb` in the **parent directory of the repo** (one level above the clone). Install locally:

```bash
sudo apt install /path/to/x11typewrite_0.1.0-1_amd64.deb
```

The package installs `x11typewrite` to `/usr/bin` and a `.desktop` entry for your app menu.

### Portable `.tar.gz` (bundled libraries)

**Output directory:** `linux-typewrite-x11/dist/` (listed in `.gitignore`; rebuild locally before shipping).

**What gets packed:** On an **x86_64** build host, the script installs **`bin/x11typewrite-x86_64`** and runs `ldd` on it, copying resolved shared objects into **`lib/x86_64/`** (Cairo, X11, FreeType, etc.). **glibc**, **libpthread**, **libm**, and the **dynamic linker** are **not** copied so the tarball stays compatible with typical desktop Linux distros.

If **`x11typewrite-i686`** exists (see **`make i686`** above), the same archive also contains **`bin/x11typewrite-i686`** and **`lib/i686/`**. The tarball basename then includes **`x86_64+i686`**; otherwise it ends with **`x86_64`**. On **aarch64** hosts, the layout uses **`x11typewrite-aarch64`** and **`lib/aarch64/`**.

**Launcher:** `run-x11typewrite.sh` chooses the binary and `LD_LIBRARY_PATH` from `uname -m` (`x86_64`, `i686` / `i386`, `aarch64`).

**32-bit build deps (Debian/Ubuntu example):** multilib toolchain and i386 dev packages, e.g. `gcc-multilib`, `libc6-dev-i386`, `libx11-dev:i386`, `libcairo2-dev:i386`. If `pkg-config` for 32-bit Cairo is not on the default path, set **`PKG_CONFIG_I686_LIBDIR`** (see `Makefile`).

```bash
make -C linux-typewrite-x11 portable
# same as: make all; make i686 (ignored if deps missing); ./make-portable-tarball.sh --no-build

tar xf linux-typewrite-x11/dist/x11typewrite-portable-0.1.0-x86_64.tar.gz
# or: ...-x86_64+i686.tar.gz when the i686 binary was built

cd x11typewrite-portable-0.1.0-x86_64   # directory name matches the archive
./run-x11typewrite.sh
```

You still need a working **X11** display and a **glibc** roughly as new as the build machine (e.g. Ubuntu 22.04+ when built on 22.04).

### Run

```bash
./linux-typewrite-x11/x11typewrite
```

Choose a file:

```bash
./linux-typewrite-x11/x11typewrite -f MyDraft.txt
```

Start in fullscreen (EWMH `_NET_WM_STATE_FULLSCREEN`; applied after the window maps):

```bash
./linux-typewrite-x11/x11typewrite --fullscreen
```

### Keys

- **Esc**: closes the **F1** menu when it is open (does not quit the app)
- **Ctrl+Q** or **Ctrl+X**: **save** (default file `Typewriter.txt` if none set) and **exit**
- **F1**: **Help menu** (overlay). **Up/Down** (and **Home/End**, **PgUp/PgDn**) move a **highlight** over lines; **Enter** runs the **highlighted** shortcut (same as the listed key) and then closes the menu; on purely informational lines it just closes. While the menu is open, editing and **F2–F10** / **F11** are disabled so shortcuts do not fire behind the overlay; **Ctrl+S** / **Ctrl+Q** / **Ctrl+X** still work (**Ctrl+P** is disabled like other edit shortcuts).
- **F2**: cycle bundled fonts (same set as the UEFI app)
- **F3**: cycle cursor modes (bar, blink bar, block, blink block, hidden)
- **F4**: cycle background color schemes (UEFI set)
- **F5**: cycle **gutter labels** (three steps): **off** → **1…n** top-to-bottom (line index on the page / “how far down the page you are”) → **n…1** top-to-bottom (**rows remaining** from that line through the end of the buffer, inclusive) → off again
- **F6**: toggle **page margins** — on: Letter-style inset “paper” on a dark surround; off: up to **80** columns (UEFI full-width mode), centered in the window
- **F7**: cycle **characters per line** **50–65** when margins are on (default **58**, matching UEFI `cols_margined`)
- **F8**: **Typewriter view** — **on by default**; toggle bottom-anchored typing (new lines from the **bottom**, text moves **up**) and the **red rule** on the active row.
- **F9**: cycle **status toast** interval: **1 min** (default) → **5** → **10** → **15** → **30** → **1 hr** → …
- **F10**: toggle **word wrap** (default **on**). When a line fills, text after the **last space** moves to the **next row** if that row is empty; otherwise the editor keeps a **hard** break at the column edge (same as wrap off). Long words with no space still hard-wrap.
- **F11**: toggle fullscreen via EWMH — **often captured by the window manager** before this client sees it; use **`--fullscreen`** or your WM’s own fullscreen binding if F11 does nothing
- **Ctrl+S**: save (first save defaults to `Typewriter.txt` in the current directory); with a filename set, **autosave** runs after about **10 seconds** with no document edits while the buffer is dirty
- **Ctrl+P**: export **PDF** via **Cairo** (same basename as the current save path with a `.pdf` extension, or `Typewriter.pdf` if no file is set yet). Raster layout matches the document (margins, background, gutter numbers, `Page N of M`); no typewriter transform or cursor. Not available while the F1 help overlay has focus (same as other edit shortcuts except **Ctrl+S** / **Ctrl+Q** / **Ctrl+X**).
- **Arrow keys** (incl. keypad): move the cursor; at page/line edges, **Up/Down** and **Left/Right** continue on the **previous/next** page or row as you would expect
- **Home** / **End**: start of line / after last non-space on the line
- **PgUp** / **PgDn**: previous / next page
- **Insert**: toggle **insert** vs **typeover** (default **typeover**: characters replace in place; **insert** shifts the rest of the line right). **Backspace** in insert mode pulls the rest of the line left; **Delete** removes under the cursor (shift in insert mode)
- **Backspace**: delete backward
- **Tab**: insert **four spaces** (no raw tab character in the buffer)
- **Enter**: newline in the document; with the **F1** menu open, **activates the highlighted row** (or closes on info-only rows)
- Printable ASCII: inserts or overwrites per the current mode

Autoloads `Typewriter.txt` if present when no `-f`/positional file is given; dirty buffers autosave after **~10 seconds idle** (no typing/editing) when a filename is set.

### Toasts

Short messages use the same muted ink as the gutter stamp. They **type on** at a speed derived from **your recent typing pace** (smoothed gaps between characters you insert—printable keys, **Tab**, **Enter**); until enough data exists, a moderate default pace is used. After the full line appears, the text **fades** toward the paper color (fading-ink effect).

**Action toasts** (save, autosave, **Ctrl+P**, **F2–F10** / **F11**, **Insert**, etc.) sit in the **footer band**, to the **left** of **Page N of M**.

**Status toast** (interval **F9**) appears in the **top margin band**—the strip **between the top edge of the paper and the first line of text** (Letter margins on), or along the top of the paper when margins are off.

**Status toast** interval: **1 min** (default), **5**, **10**, **15**, **30 minutes**, or **1 hour**. Each pulse shows:

`42 wpm | session 12 words | doc 100 words | 14:05`

- **wpm** — from your smoothed inter-key timing (standard **chars/5** notion; **0** until measurable).
- **session words** — typing-units this run **÷ 5** (printable **+1**, **Tab +4**, **Enter +1**); does not decrease when you delete.
- **doc words** — whitespace-separated tokens across **all pages** (each grid row ends a word).
- **time** — local **24h `HH:MM`**, last field so it stays easy to ignore.

Fields are separated by **`|`** (pipe with spaces).

### Multi-page documents

The buffer is a stack of **pages**, each the same size as the on-screen grid (`cols` × `rows`). When the cursor is on the **last row** and you press **Enter**, or when a line **wraps** past the last row, the editor moves to the **next page** instead of scrolling the single grid (which used to break rendering at the bottom).

Saved files use an ASCII **form feed** (`\f`, **Ctrl+L**) between pages. Older single-page `Typewriter.txt` files load as before (one page).

Each page shows a footer stamp **`Page N of M`** (muted ink): bottom margin when Letter margins are on, otherwise top-right of the paper area.

### PDF export (**Ctrl+P**)

Implemented in `src/pdf_export.c`: each document page is rasterized with the same bitmap font drawing as the editor, then scaled to fit an **A4** PDF page through **Cairo** (`cairo_pdf_surface` + `cairo_image_surface` / `cairo_set_source_surface`). This is **image-based** PDF (not vector outlines), so file size grows with page count and resolution is fixed to the on-screen cell grid.

A possible follow-up is **vector** PDF (glyph paths) for smaller files and smooth zooming.

