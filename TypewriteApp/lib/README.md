## linux-typewrite

Minimal Linux implementation track for the Typewrite app.

### Current state

- `fbtypewrite`: writes directly to `/dev/fb0` (fbdev) and reads keys from stdin.
- Rendering uses a tiny built-in 8x8 bitmap font (ASCII).

### Build

```bash
make -C linux-typewrite
```

### Run (framebuffer console)

You typically want to run from a VT (not inside a terminal emulator) and you may need permissions for `/dev/fb0`.

```bash
sudo ./linux-typewrite/fbtypewrite
```

Keys:
- **Ctrl+C**: exit
- **Enter**: newline
- Printable ASCII: inserts into the buffer

