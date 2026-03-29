# Graphics Debugging Notes

> **Repo context:** [`AGENTS.md`](AGENTS.md)

## Current Status (2026-03-30)

### Summary (fonts + flashing)

- **Bitmap fonts** (Virgil / Helvetica) match **`fonts/convert_font.py`** layout: per-glyph width, row stride \((w+7)/8\), variable height; proportional advance and cursor position. Landed in commit **`cd386303`**.
- **Idle flicker** reduced by redrawing only when **`Doc.Modified`**.
- **Keystroke flash** reduced by drawing into a **pool back-buffer** and **`CopyMem`** once to the GOP **`FrameBufferBase`** per frame (`FlipFramebuffer` in **`uefi-app/main.c`**). If **`AllocatePool`** fails, the app draws directly and may still flash.

### Font rendering — root cause fixed in code

The **“H-like” glyphs and wrong shapes** were largely from **incorrect bitmap indexing**, not from GOP/pixels failing:

1. **Virgil / Helvetica** (`DrawCharVirgil`, `DrawCharHelvetica`): glyphs are **row-major** (each row = `VIRGIL_ROW_BYTES` / `HELVETICA_ROW_BYTES`). The loop reused `offset` for every row, so **only the first row of each glyph was drawn 28 times** — repeated horizontal strips that often read as vertical bars (e.g. like an “H”).
2. **Simple 5×8 font** (`DrawCharSimple`): data is **5 bytes per character = one byte per column** (bits = rows). The old code indexed `offset + row`, which scrambled columns/rows.

**Fix:** advance bitmap pointer by `py * ROW_BYTES` per row; decode simple font as column-major with `(1u << row)` per column byte.

### Proportional glyph stride (2026-03-29)

`fonts/convert_font.py` packs each glyph with **`rowBytes = (glyphWidth + 7) / 8`** per scanline and a **variable number of rows** per character. The headers expose **`virgil_widths[]` / `helvetica_widths[]`**, but the renderer mistakenly used **`VIRGIL_ROW_BYTES`** and **`VIRGIL_HEIGHT`** for **every** glyph. Narrow characters (e.g. `i`) used the wrong row stride; taller glyphs (e.g. `T`) **read past the bitmap into the next glyph**, so the **top looked fine** and the **bottom was garbage**.

**Fix:** for each glyph use `rowB = (gw + 7) / 8`, `gh = glyphBytes / rowB`, clip pixels with `px < gw`, advance with `width + gap` (or `advances[]` when non-zero).

### Flicker

The main loop was calling **`RenderDocument` ~100×/s** (full-screen clear each time). **Fix:** redraw only when **`Doc.Modified`** is set.

### Full-frame flash on each keystroke

Even with **`Doc.Modified`**, **`RenderDocument`** still does **`ClearScreen`** then redraws everything into the **visible GOP framebuffer**. The hardware can show that **cleared** frame briefly → perceived **flash** on every key.

**Fix:** **compositing back-buffer** — allocate a same-size **`EfiBootServicesData`** pool, set **`fb.PixelData`** to it for all **`DrawPixel`** paths, then **`CopyMem`** once into **`FrameBufferBase`** at end of **`RenderDocument`** (`FlipFramebuffer`). The user sees a single update per frame. If pool allocation fails, the app falls back to drawing directly (may flicker). See **`uefi-app/main.c`** (`FbFront`, `FlipFramebuffer`).

The **square** tests used tight loops writing many pixels and did not use the font decode path, so they could “work” while text looked broken.

### What Works
- ✅ Build system produces valid PE32+ EFI app
- ✅ QEMU runs the app successfully
- ✅ Framebuffer address detected (0x80000000)
- ✅ Pitch detected (4096 for 1024x768)
- ✅ GOP PixelFormat = 1 (BlueGreenRedReserved8BitPerColor = BGR)
- ✅ Half-screen red/blue test works in QEMU and on real hardware

### Outstanding (real hardware / GOP edge cases)

Earlier MacBook tests: tiny draws and some GOP paths behaved badly; **bitmap glyph decode bugs above are fixed** in software. Remaining issues may include cache coherency, **`Blt()`** vs raw FB, or firmware-specific GOP — Needs retesting after back-buffer change.

## Test Results

### Test: Half-screen red/blue
- **QEMU**: ✅ Works - left half red, right half blue
- **Real hardware**: ✅ Works - colors appear correct

### Test: Single red pixel at center
- **QEMU**: Not tested yet
- **Real hardware**: ❌ Did not see the pixel

### Test: Block "A" (40x40 pixels)
- **Real hardware**: Looked like "H" - vertical bars visible but middle bar missing or displaced

## Observations

1. The framebuffer write seems to work (large fills work)
2. Small draws may be getting overwritten or not visible
3. The "H" shape suggests:
   - Two vertical bars are being drawn (left and right)
   - Horizontal bar is either missing or drawn in wrong position
4. Flickering suggests the screen may be getting redrawn

## Possible Causes

1. **Framebuffer sync**: The GOP may need explicit sync/flush
2. **Pitch mismatch**: Actual pitch might differ from reported
3. **Memory caching**: May need to use write-combining or cache flush
4. **Display mode**: The MacBook display might use different resolution than GOP reports

## Next Steps to Try

1. Test with explicit memory barrier/flush
2. Try writing larger blocks instead of single pixels
3. Check if screen is being cleared by something else
4. Try using GOP's Blt() function instead of direct framebuffer
5. Check actual framebuffer dimensions vs reported

## Code Locations

- Main app: `uefi-app/main.c`
- Font data: `simple_font_data[]` in `main.c`; `virgil.h` / `helvetica.h`
- `DrawPixel`, `DrawCharVirgil`, `DrawCharHelvetica`, `DrawCharSimple`: `uefi-app/main.c`
- Framebuffer init: `efi_main` after GOP `SetMode`

## App entry (2026-03-29)

The temporary **blue screen + nested squares + early `return`** in `efi_main` was removed so the **typewriter loop** runs again. Reintroduce a short GFX self-test only behind a compile-time flag if needed.
