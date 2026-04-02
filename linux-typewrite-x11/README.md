## linux-typewrite-x11

Simple X11/Xlib version of the typewrite bring-up app.

It **reuses** the shared core and tiny font from `../linux-typewrite/src/`.

### Build

Requires Xlib headers (e.g. Debian/Ubuntu: `sudo apt install libx11-dev`).

```bash
make -C linux-typewrite-x11
```

### Run

```bash
./linux-typewrite-x11/x11typewrite
```

Or choose a file:

```bash
./linux-typewrite-x11/x11typewrite -f MyDraft.txt
```

Keys:
- **Esc**: exit
- **F11**: toggle fullscreen
- **F2**: cycle bundled fonts (same set as UEFI app)
- **F3**: cycle cursor modes (bar, blink bar, block, blink block, hidden)
- **Ctrl+S**: save (first save sets `Typewriter.txt` in current directory)
- **Backspace**: delete
- **Enter**: newline
- Printable ASCII: inserts into the buffer

