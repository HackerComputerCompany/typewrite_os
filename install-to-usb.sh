#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDROOT_DIR="$SCRIPT_DIR/buildroot-2024.02"
IMAGES_DIR="$BUILDROOT_DIR/output/images"

KERNEL="$IMAGES_DIR/bzImage"
ROOTFS="$IMAGES_DIR/rootfs.ext2"

if [ $# -lt 1 ]; then
    echo "Usage: $0 /dev/sdX"
    echo ""
    echo "Example:"
    echo "  $0 /dev/sdb"
    echo ""
    echo "WARNING: This will erase all data on the specified device!"
    echo ""
    echo "To find your USB device, run: lsblk"
    exit 1
fi

DEVICE=$1
PARTITION="${DEVICE}1"

# Verify device exists
if [ ! -b "$DEVICE" ]; then
    echo "Error: Device $DEVICE not found"
    exit 1
fi

# Check for required files
if [ ! -f "$KERNEL" ]; then
    echo "Error: Kernel not found at $KERNEL"
    echo "Please build first: cd buildroot-2024.02 && make"
    exit 1
fi

if [ ! -f "$ROOTFS" ]; then
    echo "Error: Root filesystem not found at $ROOTFS"
    echo "Please build first: cd buildroot-2024.02 && make"
    exit 1
fi

# Check for syslinux
if ! command -v syslinux &> /dev/null; then
    echo "Error: syslinux not found. Install it first:"
    echo "  Ubuntu/Debian: sudo apt install syslinux syslinux-common"
    echo "  Fedora: sudo dnf install syslinux"
    exit 1
fi

# Find syslinux MBR
MBR_BIN=""
for path in /usr/lib/syslinux/mbr/mbr.bin /usr/share/syslinux/mbr.bin /usr/lib/syslinux/mbr.bin; do
    if [ -f "$path" ]; then
        MBR_BIN="$path"
        break
    fi
done

if [ -z "$MBR_BIN" ]; then
    echo "Error: syslinux MBR not found"
    exit 1
fi

echo "=========================================="
echo "Typewrite OS USB Installer"
echo "=========================================="
echo ""
echo "Device: $DEVICE"
echo "Kernel: $KERNEL"
echo "RootFS: $ROOTFS"
echo ""
echo "WARNING: ALL DATA ON $DEVICE WILL BE ERASED!"
echo ""
read -p "Press Ctrl+C to cancel, or Enter to continue..." -r

# Unmount if mounted
echo "Unmounting any existing partitions..."
sudo umount "$PARTITION" 2>/dev/null || true
sudo umount "$DEVICE" 2>/dev/null || true

# Create partition table
echo "Creating partition table..."
sudo fdisk "$DEVICE" << 'FDISK'
o
n
p
1


a
w
FDISK

# Wait for partition to be recognized
sleep 2

# Format partition
echo "Formatting partition..."
sudo mkfs.ext4 -F "$PARTITION"

# Install MBR
echo "Installing bootloader MBR..."
sudo dd if="$MBR_BIN" of="$DEVICE" bs=440 count=1 status=none

# Mount and copy files
MOUNT_DIR="/mnt/typewrite-install"
echo "Mounting partition..."
sudo mkdir -p "$MOUNT_DIR"
sudo mount "$PARTITION" "$MOUNT_DIR"

echo "Copying kernel..."
sudo mkdir -p "$MOUNT_DIR/boot"
sudo cp "$KERNEL" "$MOUNT_DIR/boot/bzImage"

echo "Copying root filesystem..."
sudo cp "$ROOTFS" "$MOUNT_DIR/boot/rootfs.ext2"

echo "Installing SYSLINUX..."
sudo mkdir -p "$MOUNT_DIR/boot/syslinux"

# Copy syslinux modules
SYSLINUX_MODULES="ldlinux.c32 libutil.c32 menu.c32"
for mod in $SYSLINUX_MODULES; do
    for path in "/usr/lib/syslinux/modules/bios/$mod" "/usr/share/syslinux/$mod" "/usr/lib/syslinux/$mod"; do
        if [ -f "$path" ]; then
            sudo cp "$path" "$MOUNT_DIR/boot/syslinux/"
            break
        fi
    done
done

# Create syslinux config
echo "Creating boot configuration..."
sudo tee "$MOUNT_DIR/boot/syslinux/syslinux.cfg" > /dev/null << 'EOF'
DEFAULT typewrite
PROMPT 0
TIMEOUT 10
UI menu.c32

MENU TITLE Typewrite OS

LABEL typewrite
    MENU LABEL Typewrite OS
    KERNEL /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0
EOF

# Install syslinux to partition
echo "Installing SYSLINUX to partition..."
sudo syslinux --install "$PARTITION"

# Unmount
echo "Unmounting..."
sudo umount "$MOUNT_DIR"
sudo rmdir "$MOUNT_DIR" 2>/dev/null || true

echo ""
echo "=========================================="
echo "Installation complete!"
echo "=========================================="
echo ""
echo "To boot from USB:"
echo "1. Insert USB drive into target computer"
echo "2. Boot and enter BIOS/UEFI setup (usually F2, F12, or Del)"
echo "3. Set USB as first boot device"
echo "4. Save and exit"
echo ""
echo "Serial console (optional):"
echo "  Connect to serial port at 115200 baud"
echo "  Or use: socat - UNIX-CONNECT:/tmp/typewrite-serial.sock"