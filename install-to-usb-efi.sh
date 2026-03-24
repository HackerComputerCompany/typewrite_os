#!/bin/bash
# USB Installer for Typewrite OS - EFI Only
# For systems that only support EFI boot (MacBooks, modern UEFI-only PCs)
# Uses GPT with EFI System Partition only - no BIOS/Legacy support

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDROOT_DIR="$SCRIPT_DIR/buildroot-2024.02"
IMAGES_DIR="$BUILDROOT_DIR/output/images"

KERNEL="$IMAGES_DIR/bzImage"
ROOTFS="$IMAGES_DIR/rootfs.ext2"

# Local rEFInd (bundled with project - no download needed)
REFIND_LOCAL="$SCRIPT_DIR/bootloader/refind/refind-bin-0.14.2"

if [ $# -lt 1 ]; then
    echo "Usage: $0 /dev/sdX"
    echo ""
    echo "Creates a USB drive with EFI-only boot (no BIOS/Legacy support)"
    echo "Best for: MacBooks, modern UEFI-only PCs"
    echo ""
    echo "WARNING: ALL DATA WILL BE ERASED!"
    exit 1
fi

DEVICE="$1"

echo "=========================================="
echo "Typewrite OS - EFI Only USB Installer"
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
echo "This USB will ONLY boot on EFI/UEFI systems (no BIOS/Legacy support)"
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

# Create GPT partition table (EFI only - no BIOS partition needed)
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

# Verify partition labels
echo "Verifying partitions:"
sudo fatlabel "$PART_EFI" 2>/dev/null || echo "EFI partition created"
sudo e2label "$PART_BOOT" 2>/dev/null || echo "Boot partition created"

# Mount EFI partition
MOUNT_DIR="/mnt/typewrite-efi"
sudo mkdir -p "$MOUNT_DIR"
sudo mount "$PART_EFI" "$MOUNT_DIR"

# Create Mac-compatible directory structure (lowercase /efi/boot/)
echo "Creating EFI directory structure..."
sudo mkdir -p "$MOUNT_DIR/efi/boot/icons"

# Use local rEFInd
if [ -d "$REFIND_LOCAL" ]; then
    echo "Installing rEFInd from local..."
    REFIND_EXTRACT="$REFIND_LOCAL"
    
    # Install 32-bit boot.efi for MacBook Air 2010 and older Macs
    if [ -f "$REFIND_EXTRACT/refind/refind_ia32.efi" ]; then
        echo "  Installing 32-bit boot.efi..."
        sudo cp "$REFIND_EXTRACT/refind/refind_ia32.efi" "$MOUNT_DIR/efi/boot/boot.efi"
    fi
    
    # Also install 64-bit as fallback
    if [ -f "$REFIND_EXTRACT/refind/refind_x64.efi" ]; then
        echo "  Installing 64-bit bootx64.efi..."
        sudo cp "$REFIND_EXTRACT/refind/refind_x64.efi" "$MOUNT_DIR/efi/boot/bootx64.efi"
    fi
    
    # Install both as BOOTX64.EFI and BOOTIA32.EFI (uppercase for some firmwares)
    if [ -f "$REFIND_EXTRACT/refind/refind_x64.efi" ]; then
        sudo cp "$REFIND_EXTRACT/refind/refind_x64.efi" "$MOUNT_DIR/efi/boot/BOOTX64.EFI"
    fi
    if [ -f "$REFIND_EXTRACT/refind/refind_ia32.efi" ]; then
        sudo cp "$REFIND_EXTRACT/refind/refind_ia32.efi" "$MOUNT_DIR/efi/boot/BOOTIA32.EFI"
    fi
    
    # Copy icons
    if [ -d "$REFIND_EXTRACT/refind/icons" ]; then
        sudo cp -r "$REFIND_EXTRACT/refind/icons/"* "$MOUNT_DIR/efi/boot/icons/" 2>/dev/null || true
    fi
    
    # Copy drivers
    if [ -d "$REFIND_EXTRACT/refind/drivers_ia32" ]; then
        sudo mkdir -p "$MOUNT_DIR/efi/boot/drivers_ia32"
        sudo cp "$REFIND_EXTRACT/refind/drivers_ia32/"* "$MOUNT_DIR/efi/boot/drivers_ia32/" 2>/dev/null || true
    fi
    if [ -d "$REFIND_EXTRACT/refind/drivers_x64" ]; then
        sudo mkdir -p "$MOUNT_DIR/efi/boot/drivers_x64"
        sudo cp "$REFIND_EXTRACT/refind/drivers_x64/"* "$MOUNT_DIR/efi/boot/drivers_x64/" 2>/dev/null || true
    fi
else
    echo "Warning: rEFInd not found at $REFIND_LOCAL"
    echo "Creating minimal EFI structure without rEFInd..."
fi

# Create rEFInd config for direct kernel boot
# NOTE: For MacBook Air 2010, we need the kernel on the EFI partition
# and use the full volume-relative path
echo "Creating boot configuration..."
sudo tee "$MOUNT_DIR/efi/boot/refind.conf" > /dev/null << 'EOF'
timeout 20
default 0
scanfor Manual,External
scan_delay 1

# Explicitly set the volume to scan
also_scan_dirs 

menuentry "Typewrite OS (1280x800)" {
    icon /efi/boot/icons/os_ubuntu.icns
    volume "EFI"
    loader /efi/boot/vmlinuz.efi
    options "root=/dev/sda2 rw console=tty0 vga=817"
}

menuentry "Typewrite OS (1024x768)" {
    icon /efi/boot/icons/os_ubuntu.icns
    volume "EFI"
    loader /efi/boot/vmlinuz.efi
    options "root=/dev/sda2 rw console=tty0 vga=791"
}

menuentry "Typewrite OS (Safe 800x600)" {
    icon /efi/boot/icons/os_ubuntu.icns
    volume "EFI"
    loader /efi/boot/vmlinuz.efi
    options "root=/dev/sda2 rw console=tty0 vga=771"
}

menuentry "Typewrite OS (Text Mode)" {
    icon /efi/boot/icons/tar.icns
    volume "EFI"
    loader /efi/boot/vmlinuz.efi
    options "root=/dev/sda2 rw console=tty0 vga=text"
}
EOF

# Copy kernel to EFI partition root
# Use both bzImage and vmlinuz.efi names for compatibility
echo "Copying kernel..."
sudo cp "$KERNEL" "$MOUNT_DIR/bzImage"
sudo cp "$KERNEL" "$MOUNT_DIR/vmlinuz.efi"

# Verify structure
echo ""
echo "EFI partition contents:"
ls -la "$MOUNT_DIR/"

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

# Add fstab
sudo mkdir -p "$MOUNT_DIR/root/Documents"
sudo tee -a "$MOUNT_DIR/etc/fstab" > /dev/null << EOF
/dev/sda3  /root/Documents  vfat  defaults,noatime  0  0
EOF

sudo umount "$MOUNT_DIR"
sudo rmdir "$MOUNT_DIR" 2>/dev/null || true

echo ""
echo "=========================================="
echo "EFI-Only USB Ready!"
echo "=========================================="
echo ""
echo "Partition layout:"
echo "  sda1 - EFI System (FAT32, /efi/boot/)"
echo "  sda2 - Boot/Root (ext4)"
echo "  sda3 - Documents (FAT32)"
echo ""
echo "Boot instructions:"
echo ""
echo "MacBook:"
echo "  1. Hold Option (Alt) at startup"
echo "  2. Select 'EFI Boot' or USB icon"
echo "  3. rEFInd should appear"
echo ""
echo "PC with UEFI:"
echo "  1. Enter BIOS/UEFI setup (Del/F2)"
echo "  2. Disable Secure Boot if needed"
echo "  3. Select USB as boot device"
echo ""
echo "NOTE: This USB will NOT work on BIOS-only systems!"
