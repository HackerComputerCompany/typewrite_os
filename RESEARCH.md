# Typewrite OS - Research Notes

## Project Overview

**Goal**: Build Typewrite OS as a native UEFI application with graphical menus, typewriter input, and desk accessories running on real hardware (MacBook Air) via USB boot.

## Architecture

- **Framework**: EDK II (UEFI Development Kit)
- **Testing**: QEMU with OVMF
- **Target**: Native UEFI boot from USB on MacBook Air

## Key Technical Decisions

### Color Scheme
- Amber/Green/White text colors on dark backgrounds
- Background colors: Green (#1A4D26), Black (#000000), Blue (#003060)

### Resolution Selection
- Uses GOP (Graphics Output Protocol) to query available modes
- Graphical menu for resolution selection (not text-based)

### Keyboard Input
- F1: Toggle debug output (shows scan codes)
- F2: Increase font size
- F3: Decrease font size  
- F4: Cycle background colors (Green → Black → Blue)

### Font System
- 10 font sizes: widths {6,8,10,12,14,16,20,24,30,40}, heights {8,10,14,16,20,24,30,36,46,60}
- 512-entry font array (only 0-255 populated with CP437 glyphs)
- Currently uses pixel replication for scaling (creates "grid" effect at large sizes)

### Text Storage
- `CHAR16 lineBuffer[MAX_LINES][MAX_LINE_LEN]` (500x200)
- `UINT32 lineCharCount[MAX_LINES]`

### Typewriter Behavior
- Middle-line scrolling: cursor stays centered, page scrolls up
- Blinking block cursor: White when visible, background color when hidden
- Word wrap: looks backwards for spaces when line overflows

## F-Key Scan Codes
| Key | Scan Code |
|-----|-----------|
| F1  | 0x0B      |
| F2  | 0x0C      |
| F3  | 0x0D      |
| F4  | 0x0E      |

## UEFI Memory
- No practical limit for font data
- 1MB for full Unicode is feasible

## Applications

### Typewriter
- Main text input application
- Typewriter-style scrolling
- Word wrap support
- Font size adjustment (F2/F3)
- Background color cycling (F4)
- Debug mode (F1)

### Graphics Demo
- Demonstrates UEFI graphics primitives
- Various visual effects using GOP
- 6 rotating demo modes:
  1. **Gradient** - Diagonal color gradient
  2. **Sine Waves** - Animated sine wave patterns
  3. **Polar** - Polar coordinate color mapping
  4. **Checkerboard** - Animated checkerboard
  5. **Color Bars** - Cycling color bars
  6. **Radial** - Radial wave patterns
- Cycles every 5 seconds (300 frames)
- Shows current demo name in top-left corner

### Mandelbrot (Existing, not currently exposed)
- Fractal renderer
- Zoom sequence animation

## Build & Deployment

### Build
```bash
cd edk2
source edksetup.sh
build
```

### Deploy to USB
```bash
./deploy-to-usb.sh
```

### Test with QEMU
```bash
./start-qemu.sh
```

## Open Questions / Future Work

1. **Font rendering**: Current pixel replication creates grid effect at large sizes. Consider pre-rendered fonts at each size or vector-based approach.

2. **Extended characters**: Font array 256-511 are empty. Need to populate for extended ASCII or Unicode support.

3. **Real hardware testing**: Need to test on MacBook Air to verify compatibility.

4. **Calculator app**: Was removed due to implementation issues.

## File Structure

```
typewrite_os/
├── edk2/apps/Typewriter/
│   ├── Typewriter.c              # Main UEFI application
│   └── Typewriter.inf            # Module definition
├── edk2/HelloWorld.dsc           # Build configuration
├── uefi-app/fs/
│   └── Typewriter.efi            # Built output
├── start-qemu.sh                 # QEMU launcher
├── deploy-to-usb.sh              # USB deployment
└── RESEARCH.md                   # This file
```
