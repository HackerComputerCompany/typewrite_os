#!/bin/bash
# Typewrite OS - BIOS/Legacy Boot USB Installer
# For systems that won't boot via UEFI
# Uses MBR + FAT32 + SYSLINUX - most compatible option

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDROOT_DIR="$SCRIPT_DIR/buildroot-2024.02"
IMAGES_DIR="$BUILDROOT_DIR/output/images"

KERNEL="$IMAGES_DIR/bzImage"
ROOTFS="$IMAGES_DIR/rootfs.ext2"

if [ $# -lt 1 ]; then
    echo "Usage: $0 /dev/sdX"
    echo ""
    echo "Creates a BIOS-compatible USB drive using MBR + FAT32"
    echo "This is the most compatible option for cheap laptops"
    echo "that won't boot via UEFI."
    echo ""
    echo "WARNING: ALL DATA ON DEVICE WILL BE ERASED!"
    echo ""
    exit 1
fi

DEVICE="$1"

echo "=========================================="
echo "Typewrite OS - BIOS Boot Installer"
echo "=========================================="
echo ""
echo "This installer creates a BIOS/Legacy boot USB"
echo "Use this if UEFI boot fails on your system."
echo ""

# Check for required files
if [ ! -f "$KERNEL" ]; then
    echo "Error: Kernel not found. Build first:"
    echo "  cd buildroot-2024.02 && make"
    exit 1
fi

if [ ! -f "$ROOTFS" ]; then
    echo "Error: Rootfs not found. Build first:"
    echo "  cd buildroot-2024.02 && make"
    exit 1
fi

# Check for extlinux
if ! command -v extlinux &> /dev/null; then
    echo "Error: extlinux not found. Install:"
    echo "  sudo apt install extlinux syslinux-common"
    exit 1
fi

echo "WARNING: This will ERASE all data on $DEVICE"
echo ""
read -p "Press Enter to continue, Ctrl+C to cancel..."

# Unmount any mounted partitions
echo "Unmounting partitions..."
sudo umount "${DEVICE}"* 2>/dev/null || true
sleep 1

# Delete all existing partitions
echo "Deleting all existing partitions..."
sudo wipefs --all "$DEVICE" 2>/dev/null || true
sleep 1
sudo partprobe "$DEVICE" 2>/dev/null || true

# Create MBR partition table with single FAT32 partition
echo "Creating partition table (MBR)..."
sudo parted -s "$DEVICE" mklabel msdos
sudo parted -s "$DEVICE" mkpart primary fat32 1MiB 100%
sudo parted -s "$DEVICE" set 1 boot on

sleep 2
sudo partprobe "$DEVICE" 2>/dev/null || true
sudo udevadm settle 2>/dev/null || true

# Format as FAT32
echo "Formatting as FAT32..."
sudo mkfs.fat -F 32 -n "TYPEWRITE" "${DEVICE}1"

# Install MBR
echo "Installing boot sector..."
MBR_BIN=""
for path in /usr/lib/syslinux/mbr/mbr.bin /usr/share/syslinux/mbr.bin; do
    if [ -f "$path" ]; then
        MBR_BIN="$path"
        break
    fi
done

if [ -z "$MBR_BIN" ]; then
    echo "Error: MBR not found"
    exit 1
fi

sudo dd if="$MBR_BIN" of="$DEVICE" bs=440 count=1 status=none

# Mount
MOUNT_DIR="/mnt/typewrite-usb"
echo "Mounting..."
sudo mkdir -p "$MOUNT_DIR"
sudo mount "${DEVICE}1" "$MOUNT_DIR"

# Create directories
sudo mkdir -p "$MOUNT_DIR/boot/extlinux"

# Copy kernel
echo "Copying kernel..."
sudo cp "$KERNEL" "$MOUNT_DIR/boot/bzImage"

# Extract rootfs
echo "Extracting system files..."
ROOTFS_MNT="/tmp/rootfs-extract"
sudo mkdir -p "$ROOTFS_MNT"
sudo mount -o loop,ro "$ROOTFS" "$ROOTFS_MNT"
sudo cp -a "$ROOTFS_MNT/"* "$MOUNT_DIR/"
sudo umount "$ROOTFS_MNT"
sudo rmdir "$ROOTFS_MNT"

# Copy syslinux modules
echo "Installing SYSLINUX..."
for mod in ldlinux.c32 libcom32.c32 libutil.c32 menu.c32 vesamenu.c32; do
    for path in "/usr/lib/syslinux/modules/bios/$mod" "/usr/share/syslinux/$mod" "/usr/lib/syslinux/$mod"; do
        if [ -f "$path" ]; then
            sudo cp "$path" "$MOUNT_DIR/boot/extlinux/"
            break
        fi
    done
done

# Create syslinux config
echo "Creating boot menu..."
sudo tee "$MOUNT_DIR/boot/extlinux/extlinux.conf" > /dev/null << 'EOF'
DEFAULT typewrite

MENU TITLE Typewrite OS
TIMEOUT 30
PROMPT 1

UI menu.c32

MENU BEGIN Display Options

LABEL typewrite_1280
    MENU LABEL Display (1280x800)
    LINUX /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0 rootdelay=5 vga=817

LABEL typewrite_1024
    MENU LABEL Display (1024x768)
    LINUX /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0 rootdelay=5 vga=791

LABEL typewrite_1440
    MENU LABEL Display (1440x900)
    LINUX /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0 rootdelay=5 vga=855

LABEL typewrite_1920
    MENU LABEL Display (1920x1080)
    LINUX /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0 rootdelay=5 vga=0x1B8

LABEL typewrite_safe
    MENU LABEL Display (800x600 - Safe)
    LINUX /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0 rootdelay=5 vga=771

MENU END

MENU BEGIN Troubleshooting

LABEL typewrite_text
    MENU LABEL Text Mode Only
    LINUX /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0 rootdelay=5 vga=text

LABEL typewrite_nofb
    MENU LABEL No Framebuffer
    LINUX /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0 rootdelay=5

LABEL typewrite_gfxdebug
    MENU LABEL Graphics Debug
    LINUX /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0 rootdelay=5 gfxdebug=1

LABEL typewrite_shell
    MENU LABEL Shell (Login)
    LINUX /boot/bzImage
    APPEND root=/dev/sda1 rw console=tty0 rootdelay=5 shell=1

MENU END
EOF

# Install extlinux
echo "Installing boot loader..."
sudo extlinux --install "$MOUNT_DIR/boot/extlinux"

# Unmount
sudo umount "$MOUNT_DIR"
sudo rmdir "$MOUNT_DIR" 2>/dev/null || true

echo ""
echo "=========================================="
echo "BIOS Boot USB Ready!"
echo "=========================================="
echo ""
echo "Boot instructions:"
echo "1. Insert USB drive"
echo "2. Enter BIOS/UEFI (F2, Del, or Esc)"
echo "3. Look for 'Boot' tab"
echo "4. Find 'CSM' or 'Legacy Boot' or 'USB' option"
echo "5. Enable 'Legacy Boot' or 'CSM Support'"
echo "6. Set USB as first boot device"
echo "7. Save and exit"
echo ""
echo "If no Legacy option:"
echo "- Try pressing F12 or F8 for boot menu"
echo "- Some laptops boot USB automatically in Legacy mode"
