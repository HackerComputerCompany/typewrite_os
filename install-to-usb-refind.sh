#!/bin/bash
# USB Installer for Typewrite OS - MacBook Air 2010 Compatible
# Uses lowercase /efi/boot/ structure which is what Mac EFI expects

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDROOT_DIR="$SCRIPT_DIR/buildroot-2024.02"
IMAGES_DIR="$BUILDROOT_DIR/output/images"

KERNEL="$IMAGES_DIR/bzImage"
ROOTFS="$IMAGES_DIR/rootfs.ext2"

if [ $# -lt 1 ]; then
    echo "Usage: $0 /dev/sdX"
    echo ""
    echo "WARNING: ALL DATA WILL BE ERASED!"
    exit 1
fi

DEVICE="$1"

echo "=========================================="
echo "Typewrite OS - MacBook Air 2010 USB Installer"
echo "=========================================="
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

echo "WARNING: ALL DATA ON $DEVICE WILL BE ERASED!"
echo ""
read -p "Press Enter to continue, Ctrl+C to cancel..."

# Unmount
sudo umount "${DEVICE}"* 2>/dev/null || true
sleep 1

# Delete all existing partitions
echo "Deleting existing partitions..."
sudo wipefs --all "$DEVICE" 2>/dev/null || true
sleep 1
sudo partprobe "$DEVICE" 2>/dev/null || true
sudo udevadm settle 2>/dev/null || true

# Partition layout
EFI_SIZE=100
ROOT_SIZE=80

# Create GPT partition table
echo "Creating GPT partitions..."
sudo parted -s "$DEVICE" mklabel gpt
sudo parted -s "$DEVICE" mkpart primary fat32 1MiB "${EFI_SIZE}MB"
sudo parted -s "$DEVICE" set 1 esp on
sudo parted -s "$DEVICE" mkpart primary ext4 "${EFI_SIZE}MB" "$((EFI_SIZE + ROOT_SIZE))MB"
sudo parted -s "$DEVICE" mkpart primary fat32 "$((EFI_SIZE + ROOT_SIZE))MB" 100%

sleep 2
sudo partprobe "$DEVICE" 2>/dev/null || true
sudo udevadm settle 2>/dev/null || true

PART_EFI="${DEVICE}1"
PART_BOOT="${DEVICE}2"
PART_DOCS="${DEVICE}3"

# Format
echo "Formatting partitions..."
sudo mkfs.fat -F 32 -n "EFI" "$PART_EFI"
sudo mkfs.ext4 -F -L "TYPEWRITE" "$PART_BOOT"
sudo mkfs.vfat -F 32 -n "DOCUMENTS" "$PART_DOCS"

# Create protective MBR (not hybrid) - Mac should still find it
# Some Macs need the partition to be bootable in MBR
echo "Creating protective MBR..."
sudo sfdisk --part-type "$DEVICE" 1 0xEE >/dev/null 2>&1 || true

# Mount EFI partition
MOUNT_DIR="/mnt/typewrite-efi"
sudo mkdir -p "$MOUNT_DIR"
sudo mount "$PART_EFI" "$MOUNT_DIR"

# Create the Mac-standard directory structure
# CRITICAL: Must be lowercase /efi/boot/ for Mac EFI
echo "Creating EFI directory structure..."
sudo mkdir -p "$MOUNT_DIR/efi/boot"
sudo mkdir -p "$MOUNT_DIR/efi/boot/icons"

# Download rEFInd for Mac bootloader
REFIND_VERSION="0.14.2"
REFIND_URL="https://sourceforge.net/projects/refind/files/${REFIND_VERSION}/refind-bin-gnuefi-${REFIND_VERSION}.zip/download"

echo "Downloading rEFInd..."
REFIND_TMP="/tmp/refind-${REFIND_VERSION}"
rm -rf "$REFIND_TMP" "$REFIND_TMP.zip"
mkdir -p "$REFIND_TMP"

if command -v curl &> /dev/null; then
    curl -sL "$REFIND_URL" -o "$REFIND_TMP.zip"
elif command -v wget &> /dev/null; then
    wget -q "$REFIND_URL" -O "$REFIND_TMP.zip"
fi

if [ ! -f "$REFIND_TMP.zip" ]; then
    echo "Error: Failed to download rEFInd"
    exit 1
fi

cd "$REFIND_TMP"
unzip -o "$REFIND_TMP.zip" 2>/dev/null || {
    echo "Error: Failed to extract rEFInd"
    exit 1
}
cd - > /dev/null

REFIND_EXTRACT=$(ls -d "$REFIND_TMP"/refind-* 2>/dev/null | head -1)
if [ -z "$REFIND_EXTRACT" ]; then
    echo "Error: rEFInd extraction failed"
    exit 1
fi

echo "Installing rEFInd..."

# Install 32-bit boot.efi for MacBook Air 2010 (CRITICAL)
if [ -f "$REFIND_EXTRACT/refind_ia32.efi" ]; then
    echo "  Installing 32-bit boot.efi (for MacBook Air 2010)..."
    sudo cp "$REFIND_EXTRACT/refind_ia32.efi" "$MOUNT_DIR/efi/boot/boot.efi"
fi

# Also install 64-bit as fallback
if [ -f "$REFIND_EXTRACT/refind_x64.efi" ]; then
    echo "  Installing 64-bit bootx64.efi..."
    sudo cp "$REFIND_EXTRACT/refind_x64.efi" "$MOUNT_DIR/efi/boot/bootx64.efi"
fi

# Copy icons
if [ -d "$REFIND_EXTRACT/icons" ]; then
    sudo cp -r "$REFIND_EXTRACT/icons/"* "$MOUNT_DIR/efi/boot/icons/" 2>/dev/null || true
fi

# Copy drivers for filesystem support
if [ -d "$REFIND_EXTRACT/drivers_ia32" ]; then
    sudo mkdir -p "$MOUNT_DIR/efi/boot/drivers_ia32"
    sudo cp "$REFIND_EXTRACT/drivers_ia32/"* "$MOUNT_DIR/efi/boot/drivers_ia32/" 2>/dev/null || true
fi
if [ -d "$REFIND_EXTRACT/drivers_x64" ]; then
    sudo mkdir -p "$MOUNT_DIR/efi/boot/drivers_x64"
    sudo cp "$REFIND_EXTRACT/drivers_x64/"* "$MOUNT_DIR/efi/boot/drivers_x64/" 2>/dev/null || true
fi

# Create rEFInd config for direct kernel boot
echo "Creating rEFInd configuration..."
sudo tee "$MOUNT_DIR/efi/boot/refind.conf" > /dev/null << 'EOF'
timeout 20
default 0
scanfor Manual

menuentry "Typewrite OS (1280x800)" {
    icon /efi/boot/icons/os_ubuntu.icns
    loader /bzImage
    options "root=/dev/sda2 rw console=tty0 vga=817"
}

menuentry "Typewrite OS (1024x768)" {
    icon /efi/boot/icons/os_ubuntu.icns
    loader /bzImage
    options "root=/dev/sda2 rw console=tty0 vga=791"
}

menuentry "Typewrite OS (Safe 800x600)" {
    icon /efi/boot/icons/os_ubuntu.icns
    loader /bzImage
    options "root=/dev/sda2 rw console=tty0 vga=771"
}

menuentry "Typewrite OS (Text Mode)" {
    icon /efi/boot/icons/tar.icns
    loader /bzImage
    options "root=/dev/sda2 rw console=tty0 vga=text"
}
EOF

# Copy kernel to EFI partition root
echo "Copying kernel..."
sudo cp "$KERNEL" "$MOUNT_DIR/bzImage"

# Verify structure
echo ""
echo "EFI partition contents:"
ls -la "$MOUNT_DIR/"
ls -la "$MOUNT_DIR/efi/boot/"

sudo umount "$MOUNT_DIR"

# Mount boot partition
sudo mount "$PART_BOOT" "$MOUNT_DIR"

# Copy kernel
sudo mkdir -p "$MOUNT_DIR/boot"
sudo cp "$KERNEL" "$MOUNT_DIR/boot/bzImage"

# Extract rootfs
echo "Extracting root filesystem..."
ROOTFS_MNT="/tmp/rootfs-$$"
sudo mkdir -p "$ROOTFS_MNT"
sudo mount -o loop,ro "$ROOTFS" "$ROOTFS_MNT"
sudo cp -a "$ROOTFS_MNT/"* "$MOUNT_DIR/"
sudo umount "$ROOTFS_MNT"
sudo rmdir "$ROOTFS_MNT"

# Install extlinux for BIOS boot
sudo mkdir -p "$MOUNT_DIR/boot/extlinux"
for mod in ldlinux.c32 libcom32.c32 libutil.c32 menu.c32; do
    for path in "/usr/lib/syslinux/modules/bios/$mod" "/usr/share/syslinux/$mod"; do
        [ -f "$path" ] && sudo cp "$path" "$MOUNT_DIR/boot/extlinux/"
    done
done
sudo extlinux --install "$MOUNT_DIR/boot/extlinux" 2>/dev/null || true

# Create extlinux config
sudo tee "$MOUNT_DIR/boot/extlinux/extlinux.conf" > /dev/null << 'EOF'
DEFAULT typewrite

MENU TITLE Typewrite OS
TIMEOUT 30

UI menu.c32

LABEL typewrite
    LINUX /boot/bzImage
    APPEND root=/dev/sda2 rw console=tty0 vga=817
EOF

# Add fstab
sudo mkdir -p "$MOUNT_DIR/root/Documents"
sudo tee -a "$MOUNT_DIR/etc/fstab" > /dev/null << EOF
/dev/sda3  /root/Documents  vfat  defaults,noatime  0  0
EOF

sudo umount "$MOUNT_DIR"
sudo rmdir "$MOUNT_DIR" 2>/dev/null || true

# Cleanup
rm -rf "$REFIND_TMP" "$REFIND_TMP.zip"

echo ""
echo "=========================================="
echo "MacBook Air 2010 USB Ready!"
echo "=========================================="
echo ""
echo "Partition layout:"
echo "  sda1 - EFI System (FAT32, lowercase efi/boot/)"
echo "  sda2 - Boot/Root (ext4)"
echo "  sda3 - Documents (FAT32)"
echo ""
echo "Boot instructions:"
echo "1. Insert USB, hold Option (Alt) at startup"
echo "2. Select 'EFI Boot' or USB icon"
echo "3. rEFInd should appear with boot options"
echo ""
echo "If still not showing:"
echo "- Reset NVRAM: Cmd+Opt+P+R at startup chime"
echo "- Try different USB port"
echo "- External boot must be enabled in System Preferences"
