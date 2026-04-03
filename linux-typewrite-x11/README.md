## linux-typewrite-x11

Simple X11/Xlib version of the typewrite bring-up app.

It **reuses** the shared core and UEFI bitmap fonts from `../linux-typewrite/src/` and `fonts/*.h` (via gnu-efi include path in the `Makefile`).

### Build

Requires Xlib and Cairo (e.g. Debian/Ubuntu: `sudo apt install libx11-dev libcairo2-dev`). The `Makefile` uses `pkg-config cairo` for compile and link flags (typically **dynamic** `-lcairo`; static linking is possible if you supply a full static Cairo stack).

```bash
make -C linux-typewrite-x11
```

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
- **F1**: **Help menu** (overlay). **Up/Down** (and **Home/End**, **PgUp/PgDn**) move a **highlight** over lines; **Enter** closes the menu. While the menu is open, editing and **F2–F9** / **F11** are disabled so shortcuts do not fire behind the overlay; **Ctrl+S** / **Ctrl+Q** / **Ctrl+X** still work (**Ctrl+P** is disabled like other edit shortcuts).
- **F2**: cycle bundled fonts (same set as the UEFI app)
- **F3**: cycle cursor modes (bar, blink bar, block, blink block, hidden)
- **F4**: cycle background color schemes (UEFI set)
- **F5**: cycle **gutter labels** (three steps): **off** → **1…n** top-to-bottom (line index on the page / “how far down the page you are”) → **n…1** top-to-bottom (**rows remaining** from that line through the end of the buffer, inclusive) → off again
- **F6**: toggle **page margins** — on: Letter-style inset “paper” on a dark surround; off: up to **80** columns (UEFI full-width mode), centered in the window
- **F7**: cycle **characters per line** **50–65** when margins are on (default **58**, matching UEFI `cols_margined`)
- **F8**: **Typewriter view** — **on by default**; toggle bottom-anchored typing (new lines from the **bottom**, text moves **up**) and the **red rule** on the active row.
- **F9**: cycle **status toast** interval: **1 min** (default) → **5** → **10** → **15** → **30** → **1 hr** → …
- **F11**: toggle fullscreen via EWMH — **often captured by the window manager** before this client sees it; use **`--fullscreen`** or your WM’s own fullscreen binding if F11 does nothing
- **Ctrl+S**: save (first save defaults to `Typewriter.txt` in the current directory); with a filename set, **autosave** runs every **5 minutes** while the buffer is dirty
- **Ctrl+P**: export **PDF** via **Cairo** (same basename as the current save path with a `.pdf` extension, or `Typewriter.pdf` if no file is set yet). Raster layout matches the document (margins, background, gutter numbers, `Page N of M`); no typewriter transform or cursor. Not available while the F1 help overlay has focus (same as other edit shortcuts except **Ctrl+S** / **Ctrl+Q** / **Ctrl+X**).
- **Arrow keys** (incl. keypad): move the cursor; at page/line edges, **Up/Down** and **Left/Right** continue on the **previous/next** page or row as you would expect
- **Home** / **End**: start of line / after last non-space on the line
- **PgUp** / **PgDn**: previous / next page
- **Insert**: toggle **insert** vs **typeover** (default **typeover**: characters replace in place; **insert** shifts the rest of the line right). **Backspace** in insert mode pulls the rest of the line left; **Delete** removes under the cursor (shift in insert mode)
- **Backspace**: delete backward
- **Tab**: insert **four spaces** (no raw tab character in the buffer)
- **Enter**: newline in the document; **closes the F1 menu** when it is open
- Printable ASCII: inserts or overwrites per the current mode

Autoloads `Typewriter.txt` if present when no `-f`/positional file is given; dirty buffers autosave every **5 minutes** when a filename is set.

### Toasts

Short messages use the same muted ink as the gutter stamp. They **type on** at a speed derived from **your recent typing pace** (smoothed gaps between characters you insert—printable keys, **Tab**, **Enter**); until enough data exists, a moderate default pace is used. After the full line appears, the text **fades** toward the paper color (fading-ink effect).

**Action toasts** (save, autosave, **Ctrl+P**, **F2–F9** / **F11**, **Insert**, etc.) sit in the **footer band**, to the **left** of **Page N of M**.

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

