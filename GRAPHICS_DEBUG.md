# Graphics Debugging Notes

## Current Status (2026-03-28)

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
- Font data: Embedded in main.c (simple_font_data[])
- DrawPixel function: Lines ~70-85 in main.c
- Framebuffer init: Lines ~285-315 in main.c
