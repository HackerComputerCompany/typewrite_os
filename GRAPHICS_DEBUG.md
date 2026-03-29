# Graphics Debugging Notes

> **Repo context:** [`AGENTS.md`](AGENTS.md)

## Current Status (2026-03-30)

### Summary (fonts + flashing)

- **Bitmap fonts** (Virgil / Helvetica) match **`fonts/convert_font.py`** layout: per-glyph width, row stride \((w+7)/8\), variable height; proportional advance and cursor position. Landed in commit **`cd386303`**.
- **Idle flicker** reduced by redrawing only when **`Doc.Modified`**.
- **Keystroke flash:** pool + **`CopyMem`** to **`FrameBufferBase`** was tried but caused a **black screen** on some OVMF/QEMU paths; **current** code draws **directly** to the GOP framebuffer (`FlipFramebuffer` is a no-op). Mitigation: **`Doc.Modified`** coalescing. **Pitch** is clamped so **`PixelsPerScanLine` is never 0** (that also caused black/wrong output).

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

Even with **`Doc.Modified`**, **`RenderDocument`** still does **`ClearScreen`** then redraws into the framebuffer — a brief **flash** is possible. **Off-screen pool + `CopyMem`** was reverted after black-screen regressions; a future fix may use **GOP `Blt`** with **`EfiBltBufferToVideo`**.

The **square** tests used tight loops writing many pixels and did not use the font decode path, so they could “work” while text looked broken.

### What Works
- ✅ Build system produces valid PE32+ EFI app
- ✅ QEMU runs the app successfully
- ✅ Framebuffer address detected (0x80000000)
- ✅ Pitch detected (4096 for 1024x768)
- ✅ GOP PixelFormat = 1 (BlueGreenRedReserved8BitPerColor = BGR)
- ✅ Half-screen red/blue test works in QEMU and on real hardware

### Outstanding (real hardware / GOP edge cases)

**2010-era MacBook (Pro / Air):** boot then **lockup** traced to **graphics**: Apple’s UEFI often expects updates via **`Gop->Blt` (`EfiBltBufferToVideo`)**; **raw framebuffer writes** can hang or never scan out. **Fix in `main.c`:** if `FirmwareVendor` contains `Apple`, pick a **listed mode** (prefers 1024×768, then 1280×800, …), allocate an **off-screen BGR buffer**, draw there, **`Blt` full frame** on each flush. **Non-Apple (QEMU, typical PC):** unchanged **direct framebuffer** path — earlier tests showed full-screen **`Blt`** could go **black on OVMF**.

Also corrected **`SetMode` `uefi_call_wrapper` arity** to **2** (matches gnu-efi `apps/bltgrid.c`); arity **1** was wrong and may confuse some firmware.

Earlier MacBook tests: tiny draws and some GOP paths behaved badly; **bitmap glyph decode bugs above are fixed** in software.

### Long-run stability (unexpected reboot after typing)

Many PCs enable a **firmware watchdog** during boot that **resets** the machine if the EFI image never clears it. **`main.c`** calls **`BootServices->SetWatchdogTimer(0, …)`** (timeout **0** = off per UEFI spec) after init and **once per main-loop iteration** so long sessions do not trip a **~5 minute** timer.

**Word wrap** used to **`ApplyWordWrap` → `ApplyWordWrap(L+1)` recursion**; a tall document could recurse **per visual line** and **overflow the small UEFI stack** (undefined behavior / reset). Wrapping is now **iterative** (`SplitOverflowingLineOnce` + bounded rounds).

Document buffers are fixed (**`MAX_LINES` × `MAX_CHARS_PER_LINE`**); there is no heap growth for the editor body (only the optional Apple GOP Blt buffer at startup).

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
