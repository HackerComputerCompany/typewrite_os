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

Keys:
- **Esc**: exit
- **Backspace**: delete
- **Enter**: newline
- Printable ASCII: inserts into the buffer

