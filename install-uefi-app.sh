#!/bin/bash
# USB Installer for Typewrite OS - UEFI App Only
# Pure UEFI application - no Linux kernel required
# Simplest approach: just copy .efi to EFI partition

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UEFI_APP="$SCRIPT_DIR/uefi-app/fs/Typewriter.efi"

if [ ! -f "$UEFI_APP" ]; then
    echo "Error: Typewriter.efi not found at $UEFI_APP"
    echo "Build the app first with:"
    echo "  cd uefi-app && make"
    exit 1
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 /dev/sdX"
    echo ""
    echo "Creates a USB with Typewrite OS as a native UEFI application"
    echo "No Linux kernel - runs directly as EFI app"
    echo ""
    echo "WARNING: ALL DATA WILL BE ERASED!"
    exit 1
fi

DEVICE="$1"

echo "=========================================="
echo "Typewrite OS - UEFI App USB Installer"
echo "=========================================="
echo ""
echo "Source: $UEFI_APP"
file "$UEFI_APP"
echo ""

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

# Single EFI partition
echo "Creating EFI partition..."
sudo parted -s "$DEVICE" mklabel gpt
sudo parted -s "$DEVICE" mkpart primary fat32 1MiB 100%
sudo parted -s "$DEVICE" set 1 esp on

sleep 2
sudo partprobe "$DEVICE" 2>/dev/null || true
sudo udevadm settle 2>/dev/null || true

PART="${DEVICE}1"

# Format
echo "Formatting as FAT32..."
sudo mkfs.fat -F 32 -n "TYPEWRITE" "$PART"

# Mount
MOUNT_DIR="/mnt/typewrite-efi"
sudo mkdir -p "$MOUNT_DIR"
sudo mount "$PART" "$MOUNT_DIR"

# Create EFI directory
echo "Installing Typewrite OS..."
sudo mkdir -p "$MOUNT_DIR/efi/boot"

# Copy the UEFI application
sudo cp "$UEFI_APP" "$MOUNT_DIR/efi/boot/Typewriter.efi"

# Also copy as bootx64.efi for direct boot
sudo cp "$UEFI_APP" "$MOUNT_DIR/efi/boot/bootx64.efi"

# Create startup script
sudo tee "$MOUNT_DIR/startup.nsh" > /dev/null << 'EOF'
\efi\boot\Typewriter.efi
EOF

# Create a simple config for systems that support EFIs
sudo tee "$MOUNT_DIR/efi/boot/refind.conf" > /dev/null << 'EOF'
timeout 20
default 0
scanfor Manual,External

menuentry "Typewrite OS" {
    icon /efi/boot/icons/os_ubuntu.icns
    loader /efi/boot/Typewriter.efi
}
EOF

# Show what was created
echo ""
echo "=== EFI partition contents ==="
ls -la "$MOUNT_DIR/"
echo ""
echo "=== EFI/boot contents ==="
ls -la "$MOUNT_DIR/efi/boot/"

sudo umount "$MOUNT_DIR"
sudo rmdir "$MOUNT_DIR" 2>/dev/null || true

echo ""
echo "=========================================="
echo "USB Ready!"
echo "=========================================="
echo ""
echo "Boot instructions:"
echo ""
echo "MacBook Air:"
echo "  1. Hold Option (Alt) at startup"
echo "  2. Select 'EFI Boot' or USB drive"
echo ""
echo "PC (UEFI):"
echo "  1. Enter BIOS/UEFI setup (Del/F2)"
echo "  2. Disable Secure Boot if needed"
echo "  3. Select USB as boot device"
echo ""
echo "To boot directly from USB without rEFInd:"
echo "  - The app is at /efi/boot/bootx64.efi"
echo "  - Some UEFI firmwares will auto-boot EFI apps"
