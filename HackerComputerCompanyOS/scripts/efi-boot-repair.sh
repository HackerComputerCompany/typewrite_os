#!/bin/sh
# EFI Boot Repair Script for Typewrite OS
# Run this on a system where the USB doesn't boot in UEFI mode

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 /dev/sdX"
    echo ""
    echo "This script repairs/reinstalls the EFI bootloader"
    echo "for systems that don't recognize the USB boot."
    echo ""
    exit 1
fi

DEVICE="$1"
MOUNT_DIR="/mnt/efi-repair"

echo "=========================================="
echo "Typewrite OS - EFI Boot Repair"
echo "=========================================="
echo ""

# Check if device exists
if [ ! -b "$DEVICE" ]; then
    echo "Error: $DEVICE not found"
    exit 1
fi

# Find EFI partition (usually partition 2 on hybrid setup)
EFI_PART=""
for i in 1 2 3 4; do
    if blkid "${DEVICE}${i}" 2>/dev/null | grep -q "ESP"; then
        EFI_PART="${DEVICE}${i}"
        break
    fi
done

if [ -z "$EFI_PART" ]; then
    # Try partition 2 by default (hybrid layout)
    if blkid "${DEVICE}2" 2>/dev/null | grep -q "vfat"; then
        EFI_PART="${DEVICE}2"
    fi
fi

if [ -z "$EFI_PART" ]; then
    echo "Error: Could not find EFI partition"
    echo "Partitions found:"
    for i in 1 2 3 4; do
        if [ -b "${DEVICE}${i}" ]; then
            echo "  ${DEVICE}${i}: $(blkid -s TYPE -o value "${DEVICE}${i}" 2>/dev/null || echo 'unknown')"
        fi
    done
    exit 1
fi

echo "Found EFI partition: $EFI_PART"

# Mount it
echo "Mounting EFI partition..."
sudo mkdir -p "$MOUNT_DIR"
sudo mount "$EFI_PART" "$MOUNT_DIR"

# Check what's there
echo ""
echo "Current EFI contents:"
ls -la "$MOUNT_DIR/" 2>/dev/null || true
ls -la "$MOUNT_DIR/EFI/" 2>/dev/null || true
ls -la "$MOUNT_DIR/EFI/BOOT/" 2>/dev/null 2>/dev/null || true

# Create standard EFI boot structure
echo ""
echo "Setting up standard EFI boot..."

# Create directories
sudo mkdir -p "$MOUNT_DIR/EFI/BOOT"
sudo mkdir -p "$MOUNT_DIR/EFI/ Boot"
sudo mkdir -p "$MOUNT_DIR/EFI/TYPEWRITE"

# Check for kernel
if [ -f "$MOUNT_DIR/bzImage" ]; then
    echo "Found kernel: $MOUNT_DIR/bzImage"
else
    echo "Warning: No kernel found in EFI partition root"
fi

# Try to copy GRUB images
if [ -d /usr/lib/grub/x86_64-efi ]; then
    echo "Building GRUB x86_64.efi..."
    sudo grub-mkimage \
        -O x86_64-efi \
        -o "$MOUNT_DIR/EFI/BOOT/BOOTX64.EFI" \
        -p /EFI/BOOT \
        part_gpt part_msdos ext2 fat iso9660 normal boot chain linux \
        search search_fs_uuid search_fs_file search_label search \
        echo test ls sleep reboot halt png jpeg tga \
        font cat chain boot configfile help \
        2>/dev/null || echo "Failed to build GRUB x86_64"
fi

if [ -d /usr/lib/grub/i386-efi ]; then
    echo "Building GRUB IA32.efi..."
    sudo grub-mkimage \
        -O i386-efi \
        -o "$MOUNT_DIR/EFI/BOOT/BOOTIA32.EFI" \
        -p /EFI/BOOT \
        part_gpt part_msdos ext2 fat normal boot chain linux \
        search echo test ls sleep reboot halt \
        2>/dev/null || echo "Failed to build GRUB IA32"
fi

# Create fallback GRUB config
sudo tee "$MOUNT_DIR/EFI/BOOT/grub.cfg" > /dev/null << 'EOF'
set timeout=5
set default=0

menuentry "Typewrite OS" {
    search --label --set=root TYPEWRITE
    if [ -f /boot/bzImage ]; then
        linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=817
    else
        linux /bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=817
    fi
}

menuentry "Typewrite OS (Safe 800x600)" {
    search --label --set=root TYPEWRITE
    if [ -f /boot/bzImage ]; then
        linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=771
    else
        linux /bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=771
    fi
}

menuentry "Typewrite OS (Text Mode)" {
    search --label --set=root TYPEWRITE
    if [ -f /boot/bzImage ]; then
        linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=text
    else
        linux /bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=text
    fi
}

menuentry "Typewrite OS (Shell)" {
    search --label --set=root TYPEWRITE
    if [ -f /boot/bzImage ]; then
        linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 shell=1
    else
        linux /bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 shell=1
    fi
}
EOF

# Also create Microsoft-style boot entry (some firmwares look for this)
if [ -d /usr/lib/grub/x86_64-efi ]; then
    sudo mkdir -p "$MOUNT_DIR/EFI/Microsoft/Boot"
    sudo cp "$MOUNT_DIR/EFI/BOOT/BOOTX64.EFI" "$MOUNT_DIR/EFI/Microsoft/Boot/bootmgfw.efi" 2>/dev/null || true
fi

# Unmount
sudo umount "$MOUNT_DIR"
sudo rmdir "$MOUNT_DIR" 2>/dev/null || true

echo ""
echo "=========================================="
echo "EFI Boot Repair Complete!"
echo "=========================================="
echo ""
echo "What to try:"
echo ""
echo "1. In BIOS/UEFI settings:"
echo "   - Disable Secure Boot"
echo "   - Enable 'USB Boot' or 'External Boot'"
echo "   - Set boot order to USB first"
echo ""
echo "2. Boot menu key (try during startup):"
echo "   - F12, F10, F8, F9, F2, or Esc"
echo ""
echo "3. For Intel NUC/Jasper Lake specifically:"
echo "   - Press F2 for BIOS setup"
echo "   - Go to Boot tab"
echo "   - Enable 'USB'"
echo "   - Set 'Boot Order' to USB first"
echo ""
echo "4. If still not working, try:"
echo "   - Legacy boot mode (CSM) instead of UEFI"
echo "   - Or use the USB on another computer to verify it works"
