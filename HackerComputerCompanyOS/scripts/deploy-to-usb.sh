#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EFI_FILE="${SCRIPT_DIR}/uefi-app/fs/Typewriter.efi"

if [ ! -f "$EFI_FILE" ]; then
    echo "Error: Typewriter.efi not found at $EFI_FILE"
    echo "Build it first with: cd edk2 && source edksetup.sh && build -p HelloWorld.dsc -m apps/Typewriter/Typewriter.inf"
    exit 1
fi

echo "Looking for USB drives..."
echo ""

lsblk -o NAME,SIZE,TYPE,MOUNTPOINT | grep -E "disk|part"

echo ""
echo "Enter the device name (e.g., sdb, sdc - just the name, not /dev/):"
read -r DEVICE

if [ -z "$DEVICE" ]; then
    echo "Error: No device specified"
    exit 1
fi

DEVICE_PATH="/dev/${DEVICE}"
PART_PATH="${DEVICE_PATH}1"

if [ ! -b "$DEVICE_PATH" ]; then
    echo "Error: Device $DEVICE_PATH does not exist"
    exit 1
fi

echo ""
echo "WARNING: This will format $DEVICE_PATH and destroy all data!"
echo "Type 'yes' to confirm:"
read -r CONFIRM

if [ "$CONFIRM" != "yes" ]; then
    echo "Aborted."
    exit 1
fi

echo ""
echo "Formatting as FAT32..."
sudo mkfs.fat -F 32 "$PART_PATH"

MOUNT_POINT="/mnt/typewrite"
echo "Mounting to $MOUNT_POINT..."
sudo mkdir -p "$MOUNT_POINT"
sudo mount "$PART_PATH" "$MOUNT_POINT"

echo "Copying Typewriter.efi..."
sudo cp "$EFI_FILE" "$MOUNT_POINT/"

echo "Creating startup.nsh..."
echo "Typewriter.efi" | sudo tee "$MOUNT_POINT/startup.nsh" > /dev/null

echo "Creating EFI/BOOT/BOOTX64.EFI (for standard UEFI boot)..."
sudo mkdir -p "$MOUNT_POINT/EFI/BOOT"
sudo cp "$EFI_FILE" "$MOUNT_POINT/EFI/BOOT/BOOTX64.EFI"

echo "Unmounting..."
sudo umount "$MOUNT_POINT"
sudo rmdir "$MOUNT_POINT"

echo ""
echo "Done! Your USB drive is ready."
echo ""
echo "To boot:"
echo "1. Plug in the USB"
echo "2. Boot to UEFI/BIOS"
echo "3. Select 'UEFI: USB Device' or similar"
echo ""
