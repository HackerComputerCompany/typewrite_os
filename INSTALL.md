# Installing Typewrite OS on a USB Thumb Drive

This guide explains how to create a bootable USB thumb drive with Typewrite OS.

## Prerequisites

- USB thumb drive (1GB or larger)
- Linux system with root/sudo access
- Built Typewrite OS images (see BUILDING.md)

## Quick Start

```bash
# Build the images first
cd buildroot-2024.02
make

# Install to USB drive (replace /dev/sdX with your device)
sudo ./install-to-usb.sh /dev/sdX
```

## Manual Installation

### 1. Identify Your USB Drive

```bash
lsblk
# or
fdisk -l
```

Note the device path (e.g., `/dev/sdb`, `/dev/sdc`). **Make sure you have the correct device** - all data on it will be erased.

### 2. Create Partitions

```bash
# Replace /dev/sdX with your USB device
sudo fdisk /dev/sdX
```

In fdisk:
1. Press `o` to create a new DOS partition table
2. Press `n` to create a new partition
3. Press `p` for primary
4. Press `1` for partition number
5. Press `Enter` for default first sector
6. Press `Enter` for default last sector (use full drive)
7. Press `a` to make it bootable
8. Press `w` to write and exit

### 3. Format the Partition

```bash
sudo mkfs.ext4 /dev/sdX1
# or for FAT32 (smaller footprint):
sudo mkfs.vfat -F 32 /dev/sdX1
```

### 4. Install the Bootloader (SYSLINUX)

```bash
# Install SYSLINUX MBR
sudo dd if=/usr/lib/syslinux/mbr/mbr.bin of=/dev/sdX bs=440 count=1

# Or for newer systems:
sudo dd if=/usr/lib/syslinux/mbr/gptmbr.bin of=/dev/sdX bs=440 count=1

# Mount the partition
sudo mount /dev/sdX1 /mnt/usb

# Install SYSLINUX files
sudo mkdir -p /mnt/usb/boot/syslinux
sudo cp /usr/lib/syslinux/modules/bios/ldlinux.c32 /mnt/usb/boot/syslinux/
sudo extlinux --install /mnt/usb/boot/syslinux
```

### 5. Copy the Kernel and Root Filesystem

```bash
# Copy kernel
sudo cp buildroot-2024.02/output/images/bzImage /mnt/usb/boot/

# Copy root filesystem (ext2 image)
sudo cp buildroot-2024.02/output/images/rootfs.ext2 /mnt/usb/boot/
```

### 6. Create SYSLINUX Configuration

```bash
sudo cat > /mnt/usb/boot/syslinux/syslinux.cfg << 'EOF'
DEFAULT typewrite
PROMPT 0
TIMEOUT 10

LABEL typewrite
    KERNEL /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0 video=1024x768
EOF
```

### 7. Unmount

```bash
sudo umount /mnt/usb
```

### 8. Make Bootable

```bash
sudo syslinux /dev/sdX1
```

## Alternative: Direct Disk Image

### Create a Disk Image First

```bash
# Create a 128MB image
dd if=/dev/zero of=typewrite.img bs=1M count=128

# Partition it
fdisk typewrite.img
# (same partition steps as above)

# Set up loop device
sudo losetup -fP typewrite.img
sudo losetup -a  # note the loop device, e.g., /dev/loop0

# Format
sudo mkfs.ext4 /dev/loop0p1

# Mount and install
sudo mount /dev/loop0p1 /mnt/usb
# ... follow steps 4-7 above

# Detach
sudo losetup -d /dev/loop0
```

### Write Image to USB

```bash
sudo dd if=typewrite.img of=/dev/sdX bs=4M status=progress && sync
```

## Booting

1. Insert the USB drive
2. Boot your computer and enter BIOS/UEFI setup (usually F2, F12, or Del)
3. Set USB as first boot device
4. Save and exit
5. Typewrite OS should boot directly into the typewriter application

## Troubleshooting

### "No bootable device" error

- Verify the partition is marked bootable (`fdisk -l /dev/sdX`)
- Reinstall the MBR bootloader
- Try a different USB drive (some have compatibility issues)

### Kernel panics

- Check the root device parameter matches your USB partition
- Try different video modes: `video=800x600` or `video=640x480`

### Framebuffer issues

- Add `nomodeset` to kernel parameters for basic VESA mode
- Some systems need `vga=792` for 1024x768

### Serial console debugging

Connect a serial cable or use QEMU:
```bash
# Boot with serial console
./start-qemu.sh --serial

# Connect from another terminal
socat - UNIX-CONNECT:/tmp/typewrite-serial.sock
```

## Creating an Installation Script

Create `install-to-usb.sh`:

```bash
#!/bin/bash
set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 /dev/sdX"
    echo "Example: $0 /dev/sdb"
    exit 1
fi

DEVICE=$1
PARTITION="${DEVICE}1"

echo "WARNING: This will erase all data on $DEVICE"
echo "Press Ctrl+C to cancel, Enter to continue..."
read

# Unmount if mounted
sudo umount "$PARTITION" 2>/dev/null || true
sudo umount "$DEVICE" 2>/dev/null || true

# Create partition table
sudo fdisk "$DEVICE" << 'FDISK'
o
n
p
1


a
w
FDISK

# Format
sudo mkfs.ext4 "$PARTITION"

# Mount
sudo mkdir -p /mnt/typewrite-install
sudo mount "$PARTITION" /mnt/typewrite-install

# Install bootloader
sudo dd if=/usr/lib/syslinux/mbr/mbr.bin of="$DEVICE" bs=440 count=1

# Copy files
sudo mkdir -p /mnt/typewrite-install/boot/syslinux
sudo cp buildroot-2024.02/output/images/bzImage /mnt/typewrite-install/boot/
sudo cp buildroot-2024.02/output/images/rootfs.ext2 /mnt/typewrite-install/boot/
sudo cp /usr/lib/syslinux/modules/bios/ldlinux.c32 /mnt/typewrite-install/boot/syslinux/

# Create config
sudo tee /mnt/typewrite-install/boot/syslinux/syslinux.cfg > /dev/null << 'EOF'
DEFAULT typewrite
PROMPT 0
TIMEOUT 10

LABEL typewrite
    KERNEL /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0
EOF

# Install SYSLINUX
sudo syslinux --install "$PARTITION"

# Unmount
sudo umount /mnt/typewrite-install

echo "Installation complete!"
echo "You can now boot from $DEVICE"
```

Make it executable:
```bash
chmod +x install-to-usb.sh
```

## UEFI Systems

For UEFI boot (most modern computers), use GRUB instead:

```bash
# Format as FAT32 (required for UEFI)
sudo mkfs.vfat -F 32 /dev/sdX1

# Mount
sudo mount /dev/sdX1 /mnt/usb

# Create EFI directory structure
sudo mkdir -p /mnt/usb/EFI/BOOT

# Copy kernel
sudo cp buildroot-2024.02/output/images/bzImage /mnt/usb/

# Create GRUB config
sudo mkdir -p /mnt/usb/boot/grub
sudo tee /mnt/usb/boot/grub/grub.cfg > /dev/null << 'EOF'
set timeout=0
set default=0

menuentry "Typewrite OS" {
    linux /bzImage root=/dev/sda1 rw console=tty0
}
EOF

# Install GRUB (from host system with grub-efi)
sudo grub-install --target=x86_64-efi --efi-directory=/mnt/usb --boot-directory=/mnt/usb/boot --removable

sudo umount /mnt/usb
```

## Notes

- The `rootfs.ext2` file can be quite large. For smaller installations, consider using a compressed initramfs.
- For persistence across reboots, the root filesystem on the USB partition must be writable.
- Test with QEMU before writing to USB: `./start-qemu.sh`
- Documents are stored in `/root/Documents/` on the FAT32 partition (portable to other systems)
- Resolution changes (F6) may not work on all hardware; the app will show a toast if the resolution isn't supported
- The signal handler gracefully saves any unsaved documents on SIGINT/SIGTERM
- Press Ctrl+Q to exit to shell (documents will auto-save if dirty)
- Ink colors and bold formatting are stored in companion `.ink` files (format: `ink_idx,bold_flag` per character)

## Hardware Considerations

- **Display**: Works with standard Linux framebuffer (`/dev/fb0`)
- **Keyboard**: Requires direct keyboard access (raw mode)
- **Video modes**: Not all resolutions may be supported by your hardware's LCD panel
- **Persistence**: Documents on the FAT32 partition are readable on Windows/macOS/Linux
- **Boot**: Uses EXTLINUX for BIOS boot (not UEFI); for UEFI systems, see the UEFI section above