#!/usr/bin/env bash
# USB Installer for Typewrite OS - UEFI App Only
# Pure UEFI application - no Linux kernel required
# Simplest approach: copy .efi to EFI partition

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UEFI_APP="$SCRIPT_DIR/uefi-app/fs/Typewriter.efi"

usage() {
    cat <<EOF
Usage: $(basename "$0") [options] /dev/sdX | /dev/nvme0n1 | /dev/mmcblk0

  Wipes the target disk, creates GPT + FAT32 ESP, installs Typewriter.efi as
  efi/boot/bootx64.efi (and Typewriter.efi).

Options:
  --yes, -y    Skip confirmation prompt (destructive)
  -h, --help   This help

Build / refresh the .efi first, e.g.:
  make -C uefi-app all && cp -f uefi-app/Typewriter.efi uefi-app/fs/Typewriter.efi
Or use: ./write-typewriter-to-usb.sh /dev/sdX
EOF
}

YES=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --yes|-y) YES=1; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
        *) break ;;
    esac
done

if [[ ! -f "$UEFI_APP" ]]; then
    echo "Error: Typewriter.efi not found at $UEFI_APP"
    echo "Build the app first with:"
    echo "  make -C uefi-app all"
    echo "  cp -f uefi-app/Typewriter.efi uefi-app/fs/Typewriter.efi"
    echo "Or run: ./write-typewriter-to-usb.sh /dev/sdX"
    exit 1
fi

if [[ $# -lt 1 ]]; then
    usage >&2
    exit 1
fi

DEVICE="$1"

BLK_TYPE=$(lsblk -ndo TYPE "$DEVICE" 2>/dev/null || true)
if [[ "$BLK_TYPE" != "disk" ]]; then
    echo "Error: $DEVICE must be a whole disk (lsblk type: ${BLK_TYPE:-unknown})."
    echo "Use e.g. /dev/sdb or /dev/nvme0n1, not a partition like /dev/sdb1."
    exit 1
fi

# First partition name: /dev/sdb -> sdb1; /dev/nvme0n1 -> nvme0n1p1; /dev/mmcblk0 -> mmcblk0p1
first_efi_partition() {
    local d="$1"
    local b
    b="$(basename "$d")"
    if [[ "$b" =~ ^(nvme[0-9]+n[0-9]+|mmcblk[0-9]+)$ ]]; then
        echo "${d}p1"
    else
        echo "${d}1"
    fi
}
PART="$(first_efi_partition "$DEVICE")"

echo "=========================================="
echo "Typewrite OS - UEFI App USB Installer"
echo "=========================================="
echo ""
echo "Source: $UEFI_APP"
file "$UEFI_APP"
echo ""
echo "Target disk: $DEVICE  (ESP will be $PART)"
echo ""

echo "WARNING: ALL DATA ON $DEVICE WILL BE ERASED!"
echo ""
if [[ "$YES" -eq 0 ]]; then
    read -r -p "Press Enter to continue, Ctrl+C to cancel..."
fi

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

# Optional rEFInd-style menu (no icon path — avoids missing file on stick)
sudo tee "$MOUNT_DIR/efi/boot/refind.conf" > /dev/null << 'EOF'
timeout 20
default 0
scanfor Manual,External

menuentry "Typewrite OS" {
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
