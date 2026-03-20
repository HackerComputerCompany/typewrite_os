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
    echo "WARNING: This will erase ALL data on the specified device!"
    echo ""
    echo "Creates two partitions:"
    echo "  1. Boot/Root (ext4) - System files"
    echo "  2. Documents (FAT32) - Portable documents readable on any PC"
    echo ""
    echo "To find your USB device, run: lsblk"
    exit 1
fi

DEVICE=$1
BOOT_PART="${DEVICE}1"
DOCS_PART="${DEVICE}2"

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

# Check for extlinux
if ! command -v extlinux &> /dev/null; then
    echo "Error: extlinux not found. Install it first:"
    echo "  Ubuntu/Debian: sudo apt install extlinux syslinux-common"
    echo "  Fedora: sudo dnf install syslinux-extlinux"
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
    echo "Install: sudo apt install syslinux-common"
    exit 1
fi

# Calculate size
ROOTFS_SIZE=$(stat -c%s "$ROOTFS")
ROOTFS_SIZE_MB=$((ROOTFS_SIZE / 1024 / 1024))
ROOTFS_END_SECTOR=$((ROOTFS_SIZE_MB * 1024 * 1024 / 512 + 2048))  # First partition end

echo "=========================================="
echo "Typewrite OS USB Installer"
echo "=========================================="
echo ""
echo "Device: $DEVICE"
echo "Kernel: $KERNEL"
echo "RootFS: $ROOTFS (${ROOTFS_SIZE_MB}MB)"
echo ""
echo "Partitions:"
echo "  ${BOOT_PART} (ext4)  - Boot/Root filesystem"
echo "  ${DOCS_PART} (FAT32) - Documents (portable)"
echo ""
echo "WARNING: ALL DATA ON $DEVICE WILL BE ERASED!"
echo ""
read -p "Press Ctrl+C to cancel, or Enter to continue..." -r

# Unmount if mounted
echo "Unmounting any existing partitions..."
sudo umount "$BOOT_PART" 2>/dev/null || true
sudo umount "$DOCS_PART" 2>/dev/null || true
sudo umount "$DEVICE"* 2>/dev/null || true

# Create partition table with two partitions
echo "Creating partition table..."
sudo fdisk "$DEVICE" << FDISK
o
n
p
1

+${ROOTFS_SIZE_MB}M
n
p
2


t
2
b
a
1
w
FDISK

# Wait for partitions to be recognized
sleep 2

# Format boot partition as ext4
echo "Formatting boot partition as ext4..."
sudo mkfs.ext4 -F -L "TYPOWRITE" "$BOOT_PART"

# Format documents partition as FAT32
echo "Formatting documents partition as FAT32..."
sudo mkfs.vfat -F 32 -n "DOCUMENTS" "$DOCS_PART"

# Install MBR
echo "Installing bootloader MBR..."
sudo dd if="$MBR_BIN" of="$DEVICE" bs=440 count=1 status=none

# Mount and copy system files
MOUNT_DIR="/mnt/typewrite-install"
echo "Mounting boot partition..."
sudo mkdir -p "$MOUNT_DIR"
sudo mount "$BOOT_PART" "$MOUNT_DIR"

echo "Copying files..."
sudo mkdir -p "$MOUNT_DIR/boot/extlinux"

# Copy kernel
sudo cp "$KERNEL" "$MOUNT_DIR/boot/bzImage"

# Copy and extract rootfs
echo "Extracting root filesystem..."
sudo tar -xf "$ROOTFS" -C "$MOUNT_DIR" 2>/dev/null || \
    (echo "Using direct copy instead..."; sudo cp "$ROOTFS" "$MOUNT_DIR/boot/rootfs.ext2")

# Copy extlinux modules
for mod in ldlinux.c32 libcom32.c32 libutil.c32 menu.c32; do
    for path in "/usr/lib/syslinux/modules/bios/$mod" "/usr/share/syslinux/$mod" "/usr/lib/syslinux/$mod" "/usr/lib/EXTLINUX/$mod"; do
        if [ -f "$path" ]; then
            sudo cp "$path" "$MOUNT_DIR/boot/extlinux/" 2>/dev/null || true
            break
        fi
    done
done

# Create extlinux config
echo "Creating boot configuration..."
sudo tee "$MOUNT_DIR/boot/extlinux/extlinux.conf" > /dev/null << 'EOF'
DEFAULT typewrite
PROMPT 0
TIMEOUT 10

UI menu.c32

MENU TITLE Typewrite OS

LABEL typewrite
    MENU LABEL Typewrite OS
    LINUX /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0
EOF

# Create mount point for documents partition
echo "Configuring documents mount..."
sudo mkdir -p "$MOUNT_DIR/root/Documents"

# Add fstab entry for documents partition
sudo tee -a "$MOUNT_DIR/etc/fstab" > /dev/null << 'EOF'
/dev/sda2  /root/Documents  vfat  defaults,noatime  0  0
EOF

# Create a symlink so documents appear in /root
sudo mkdir -p "$MOUNT_DIR/root"
if [ ! -L "$MOUNT_DIR/root/Documents" ]; then
    # The mount point is created, documents partition will be mounted there
    true
fi

# Install extlinux
echo "Installing extlinux bootloader..."
sudo extlinux --install "$MOUNT_DIR/boot/extlinux"

# Unmount
echo "Unmounting..."
sudo umount "$MOUNT_DIR"
sudo rmdir "$MOUNT_DIR" 2>/dev/null || true

# Mount documents partition and create README
echo "Setting up documents partition..."
sudo mkdir -p "$MOUNT_DIR"
sudo mount "$DOCS_PART" "$MOUNT_DIR"

sudo tee "$MOUNT_DIR/README.txt" > /dev/null << 'EOF'
Typewrite OS Documents
======================

This FAT32 partition contains your documents.

Documents are saved here when you press Ctrl+S in Typewrite OS.

To access from another computer:
- Insert this USB drive
- Open the "DOCUMENTS" volume
- Documents are plain text (.md files)
- Ink colors are stored in companion .ink files

The partition is formatted as FAT32 for maximum compatibility
with Windows, macOS, and Linux systems.
EOF

sudo umount "$MOUNT_DIR"
sudo rmdir "$MOUNT_DIR" 2>/dev/null || true

echo ""
echo "=========================================="
echo "Installation complete!"
echo "=========================================="
echo ""
echo "Partitions created:"
echo "  ${BOOT_PART} - ext4 - Boot/Root (${ROOTFS_SIZE_MB}MB)"
echo "  ${DOCS_PART} - FAT32 - Documents (remaining space)"
echo ""
echo "To boot from USB:"
echo "1. Insert USB drive into target computer"
echo "2. Boot and enter BIOS/UEFI setup (F2, F12, or Del)"
echo "3. Set USB as first boot device"
echo "4. Save and exit"
echo ""
echo "Documents will be saved to the FAT32 partition and"
echo "can be read on any computer with USB support."