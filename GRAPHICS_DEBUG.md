# Graphics Debugging Notes

> **Repo context:** [`AGENTS.md`](AGENTS.md)

## Current Status (2026-03-29)

### Font rendering — root cause fixed in code

The **“H-like” glyphs and wrong shapes** were largely from **incorrect bitmap indexing**, not from GOP/pixels failing:

1. **Virgil / Helvetica** (`DrawCharVirgil`, `DrawCharHelvetica`): glyphs are **row-major** (each row = `VIRGIL_ROW_BYTES` / `HELVETICA_ROW_BYTES`). The loop reused `offset` for every row, so **only the first row of each glyph was drawn 28 times** — repeated horizontal strips that often read as vertical bars (e.g. like an “H”).
2. **Simple 5×8 font** (`DrawCharSimple`): data is **5 bytes per character = one byte per column** (bits = rows). The old code indexed `offset + row`, which scrambled columns/rows.

**Fix:** advance bitmap pointer by `py * ROW_BYTES` per row; decode simple font as column-major with `(1u << row)` per column byte.

The **square** tests used tight loops writing many pixels and did not use the font decode path, so they could “work” while text looked broken.

### What Works
- ✅ Build system produces valid PE32+ EFI app
- ✅ QEMU runs the app successfully
- ✅ Framebuffer address detected (0x80000000)
- ✅ Pitch detected (4096 for 1024x768)
- ✅ GOP PixelFormat = 1 (BlueGreenRedReserved8BitPerColor = BGR)
- ✅ Half-screen red/blue test works in QEMU and on real hardware

### What Doesn't Work
- ❌ Single pixel drawing doesn't appear on real MacBook hardware
- ❌ Font characters appear as "H-like" shapes with flickering
- ❌ Full screen fills work but individual pixels don't render correctly

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
