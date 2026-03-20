# Installing Typewrite OS on a USB Thumb Drive

This guide explains how to create a bootable USB thumb drive with Typewrite OS supporting both legacy BIOS and UEFI boot.

## Quick Start

```bash
# Build the images first
cd buildroot-2024.02
make

# Install to USB drive (hybrid BIOS+UEFI)
sudo ./install-to-usb-hybrid.sh /dev/sdX

# Or use the legacy BIOS-only installer
sudo ./install-to-usb.sh /dev/sdX
```

## Install Scripts

### install-to-usb-hybrid.sh (Recommended)

Creates a USB drive with **hybrid BIOS/UEFI boot support**:
- GPT partition table
- BIOS boot partition (for legacy BIOS)
- EFI System Partition (FAT32) with GRUB
- Boot/Root partition (ext4)
- Documents partition (FAT32)

**Options:**
```bash
./install-to-usb-hybrid.sh /dev/sdX           # Hybrid (default)
./install-to-usb-hybrid.sh /dev/sdX --hybrid   # Same as default
./install-to-usb-hybrid.sh /dev/sdX --bios-only   # BIOS only
./install-to-usb-hybrid.sh /dev/sdX --uefi-only   # UEFI only
./install-to-usb-hybrid.sh /dev/sdX --no-docs     # Skip documents partition
```

### install-to-usb.sh (Legacy)

Original BIOS-only installer using MBR partition table.

## Hardware Compatibility

### Tested Hardware

| Machine | Year | Boot Mode | Notes |
|---------|------|-----------|-------|
| Dell Latitude E6400 | 2009 | Legacy BIOS | Works with vga=817 (1280x800) |
| MacBook Air 2010 | 2010 | EFI (IA32) | Use `--hybrid`; hold Option at boot |
| ThinkPad T60 | 2006 | Legacy BIOS | May need `vga=771` (800x600) safe mode |

### Boot Methods by Machine

#### MacBook Air 2010
1. Insert USB drive
2. **Hold Option (Alt) key** while powering on
3. Select "EFI Boot" or the USB drive icon
4. GRUB menu should appear

If no boot option appears:
- The Mac may have disabled external boot - check System Preferences > Startup Disk
- Try holding Command+Option+P+R to reset NVRAM

#### ThinkPad T60
1. Insert USB drive
2. Press **F12** during boot to enter boot menu
3. Select USB from list
4. Use extlinux menu to select resolution

If no USB option in F12 menu:
- Enable USB boot in BIOS (F1 to enter Setup)

#### Dell Latitude E6400
1. Press **F12** during boot for boot menu
2. Select "USB Storage" or similar
3. Use extlinux menu - try 1280x800 first

### Graphics/Display Issues

If display looks wrong (stretched, glitchy, wrong colors):

1. **Try different VESA modes:**
   - 800x600 (Safe Mode): `vga=771`
   - 1024x768 (Normal): `vga=791`
   - 1280x800 (WXGA): `vga=817`
   - 1440x900: `vga=855`

2. **Try troubleshooting modes:**
   - Text Mode: `vga=text`
   - No Framebuffer: Boot → Troubleshooting → No Framebuffer

3. **Native resolution:**
   Most laptop LCDs have fixed native resolutions. Common ones:
   - 1024x768 - older laptops
   - 1280x800 - 13-14" laptops
   - 1440x900 - 14-15" laptops  
   - 1920x1080 - modern displays

### VESA Mode Reference

| Resolution | VESA Mode | Linux Number | Common Use |
|------------|-----------|-------------|------------|
| 640x480 | 0x311 | 785 | VGA fallback |
| 800x600 | 0x303 | 771 | Safe mode |
| 1024x768 | 0x317 | 791 | Standard |
| 1280x800 | 0x331 | 817 | WXGA laptop |
| 1440x900 | 0x357 | 855 | WXGA+ laptop |
| 1920x1080 | 0x1B8 | 440 | Full HD |

Formula: `Linux Number = VESA Mode + 512` (or `+ 0x200` in hex)

## Partition Layout

### Hybrid (GPT)

```
/dev/sda1  - BIOS Boot (1MB, unformatted, bios_grub flag)
/dev/sda2  - EFI System Partition (100MB, FAT32)
/dev/sda3  - Boot/Root (ext4)
/dev/sda4  - Documents (FAT32)
```

### BIOS Only (MBR)

```
/dev/sda1  - Boot/Root (ext4)
/dev/sda2  - Documents (FAT32)
```

## Kernel Boot Parameters

Common parameters for troubleshooting:

```
root=/dev/sda3         # Root partition (sda3 for hybrid)
console=tty0           # Output to primary display
rootdelay=5            # Wait for USB to initialize
vga=XXX               # VESA mode number
video=XXX             # Alternative video mode (KMS)
nomodeset              # Disable kernel mode setting
shell=1               # Drop to shell instead of app
nofs=1                # Skip framebuffer, text mode
```

## Troubleshooting

### "No bootable device"

**BIOS:**
- Verify partition is bootable
- Reinstall MBR: `sudo dd if=/usr/lib/syslinux/mbr/mbr.bin of=/dev/sdX bs=440 count=1`
- Check USB is first in boot order

**UEFI:**
- Verify EFI System Partition is marked as ESP (boot flag)
- Check Secure Boot settings (may need to disable)
- Try `--uefi-only` mode

### Kernel Panic: "VFS: Unable to mount root fs"

- Verify `root=` parameter matches partition
- Try `rootdelay=10` for slow USB drives
- Check if SATA/AHCI drivers are needed

### Display Issues

- Try safe mode (800x600)
- Try text mode to verify system boots
- Check if LCD native resolution is supported
- Some displays need specific VESA timings

### Serial Console Debugging

```bash
# Boot with serial
./start-qemu.sh --serial

# Connect
socat - UNIX-CONNECT:/tmp/typewrite-serial.sock
```

## Building the Kernel for Your Hardware

If you need to add hardware support:

1. Edit kernel config:
```bash
nano buildroot-2024.02/board/qemu/x86_64/linux.config
```

2. Add drivers:
```bash
# Intel graphics
CONFIG_DRM_I915=y

# NVIDIA (if needed)
CONFIG_DRM_NOUVEAU=m

# AMD (if needed)
CONFIG_DRM_RADEON=m
CONFIG_DRM_AMDGPU=m

# More USB controllers
CONFIG_USB_XHCI_HCD=y
CONFIG_USB_EHCI_HCD=y
CONFIG_USB_OHCI_HCD=y
```

3. Rebuild:
```bash
cd buildroot-2024.02
make linux-rebuild
make
```

## Notes

- The `rootfs.ext2` is ~60MB. Increase if needed in `.config`
- Documents partition is FAT32 for cross-platform compatibility
- Press **Ctrl+Q** to exit to shell
- Press **F1** in typewriter app for help
- Unsaved documents auto-save on exit

## Security Notes

- Secure Boot may block unsigned bootloaders
- Disable Secure Boot in BIOS/UEFI settings to boot Typewrite OS
- The GRUB images are self-signed but not Microsoft-signed

## File Locations

After installation:
- `/boot/bzImage` - Linux kernel
- `/boot/extlinux/extlinux.conf` - BIOS boot menu
- `/boot/grub/grub.cfg` - UEFI GRUB config
- `/EFI/BOOT/bootx64.efi` - UEFI bootloader (64-bit)
- `/EFI/BOOT/bootia32.efi` - UEFI bootloader (32-bit, for older Macs)
- `/root/Documents/` - Your documents (on FAT32 partition)