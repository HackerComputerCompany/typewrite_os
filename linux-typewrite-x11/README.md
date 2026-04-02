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
- **F5**: toggle **line numbers** in the left gutter (muted ink, UEFI-style)
- **F6**: toggle **page margins** — on: Letter-style inset “paper” on a dark surround; off: up to **80** columns (UEFI full-width mode), centered in the window
- **F7**: cycle **characters per line** **50–65** when margins are on (default **58**, matching UEFI `cols_margined`)
- **F11**: toggle fullscreen via EWMH — **often captured by the window manager** before this client sees it; use **`--fullscreen`** or your WM’s own fullscreen binding if F11 does nothing
- **Ctrl+S**: save (first save defaults to `Typewriter.txt` in the current directory)
- **Backspace**: delete
- **Enter**: newline
- Printable ASCII: inserts into the buffer

Autoloads `Typewriter.txt` if present when no `-f`/positional file is given; dirty buffers autosave every 30 seconds when a filename is set.
