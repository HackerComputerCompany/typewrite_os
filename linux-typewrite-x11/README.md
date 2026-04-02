## linux-typewrite-x11

Simple X11/Xlib version of the typewrite bring-up app.

It **reuses** the shared core and UEFI bitmap fonts from `../linux-typewrite/src/` and `fonts/*.h` (via gnu-efi include path in the `Makefile`).

### Build

Requires Xlib headers (e.g. Debian/Ubuntu: `sudo apt install libx11-dev`).

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

- **Esc**: exit
- **F1**: help (overlay)
- **F2**: cycle bundled fonts (same set as the UEFI app)
- **F3**: cycle cursor modes (bar, blink bar, block, blink block, hidden)
- **F4**: cycle background color schemes (UEFI set)
- **F5**: cycle **gutter labels** (three steps): **off** → **1…n** top-to-bottom (line index on the page / “how far down the page you are”) → **n…1** top-to-bottom (**rows remaining** from that line through the end of the buffer, inclusive) → off again
- **F6**: toggle **page margins** — on: Letter-style inset “paper” on a dark surround; off: up to **80** columns (UEFI full-width mode), centered in the window
- **F7**: cycle **characters per line** **50–65** when margins are on (default **58**, matching UEFI `cols_margined`)
- **F8**: **Typewriter view** — toggle a bottom-anchored viewport: new text grows from the **bottom** of the page and earlier lines **scroll up** (instead of the usual top-down full-page editor layout). While on, a **thin red horizontal rule** marks the **active typing row** (classic typewriter cue).
- **F11**: toggle fullscreen via EWMH — **often captured by the window manager** before this client sees it; use **`--fullscreen`** or your WM’s own fullscreen binding if F11 does nothing
- **Ctrl+S**: save (first save defaults to `Typewriter.txt` in the current directory)
- **Backspace**: delete
- **Tab**: insert **four spaces** (no raw tab character in the buffer)
- **Enter**: newline
- Printable ASCII: inserts into the buffer

Autoloads `Typewriter.txt` if present when no `-f`/positional file is given; dirty buffers autosave every 30 seconds when a filename is set.

### Multi-page documents

The buffer is a stack of **pages**, each the same size as the on-screen grid (`cols` × `rows`). When the cursor is on the **last row** and you press **Enter**, or when a line **wraps** past the last row, the editor moves to the **next page** instead of scrolling the single grid (which used to break rendering at the bottom).

Saved files use an ASCII **form feed** (`\f`, **Ctrl+L**) between pages. Older single-page `Typewriter.txt` files load as before (one page).

Each page shows a footer stamp **`Page N of M`** (muted ink): bottom margin when Letter margins are on, otherwise top-right of the paper area.
