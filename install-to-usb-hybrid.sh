#!/bin/bash
# Typewrite OS USB Installer - Hybrid BIOS/UEFI
# Supports: Legacy BIOS, UEFI x86_64, UEFI IA32 (MacBooks)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDROOT_DIR="$SCRIPT_DIR/buildroot-2024.02"
IMAGES_DIR="$BUILDROOT_DIR/output/images"

KERNEL="$IMAGES_DIR/bzImage"
ROOTFS="$IMAGES_DIR/rootfs.ext2"

# Default partition sizes (MB)
EFI_SIZE=100       # EFI System Partition
BIOS_SIZE=1        # BIOS boot partition (for GPT)
ROOT_SIZE=80       # Boot/Root partition
DOCS_MIN=500       # Minimum docs partition

usage() {
    cat << EOF
Usage: $0 /dev/sdX [OPTIONS]

Install Typewrite OS to USB with hybrid BIOS/UEFI boot support.

OPTIONS:
    --hybrid         Install both BIOS and UEFI bootloaders (default)
    --bios-only      BIOS only (smaller EFI partition)
    --uefi-only      UEFI only (no extlinux)
    --no-docs        Skip documents partition
    --label LABEL    Set volume label for boot partition

TARGET HARDWARE:
    - Dell Latitude E6400 (Legacy BIOS)
    - MacBook Air 2010 (EFI/UEFI)
    - ThinkPad T60 (Legacy BIOS)
    - Most x86_64 PCs and laptops from 2006+

EXAMPLES:
    $0 /dev/sdb                 # Hybrid install (recommended)
    $0 /dev/sdb --hybrid        # Same as above
    $0 /dev/sdb --uefi-only     # UEFI only (for Macs)

EOF
}

# Parse arguments
HYBRID_MODE=true
UEFI_ONLY=false
NO_DOCS=false
BOOT_LABEL="TYPEWRITE"

while [[ $# -gt 0 ]]; do
    case $1 in
        --hybrid)
            HYBRID_MODE=true
            UEFI_ONLY=false
            shift
            ;;
        --bios-only)
            HYBRID_MODE=false
            UEFI_ONLY=false
            shift
            ;;
        --uefi-only)
            UEFI_ONLY=true
            HYBRID_MODE=false
            shift
            ;;
        --no-docs)
            NO_DOCS=true
            shift
            ;;
        --label)
            BOOT_LABEL="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            if [[ "$1" =~ ^/dev/sd[a-z]$ ]]; then
                DEVICE="$1"
            fi
            shift
            ;;
    esac
done

if [ -z "$DEVICE" ]; then
    echo "Error: No device specified"
    echo ""
    usage
    echo "To find your USB device, run: lsblk"
    exit 1
fi

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

# Check for bootloaders
HAS_EXTLINUX=false
if command -v extlinux &> /dev/null; then
    HAS_EXTLINUX=true
fi

HAS_GRUB=false
if command -v grub-mkimage &> /dev/null; then
    HAS_GRUB=true
fi

if [ "$HYBRID_MODE" = true ] && [ "$HAS_EXTLINUX" = false ] && [ "$HAS_GRUB" = false ]; then
    echo "Error: No bootloader found. Install extlinux or GRUB:"
    echo "  Ubuntu/Debian: sudo apt install extlinux syslinux-common grub-efi-amd64-bin"
    echo "  Fedora: sudo dnf install extlinux syslinux grub2-efi"
    exit 1
fi

# Find syslinux MBR
MBR_BIN=""
if [ "$HYBRID_MODE" = true ] || [ "$HAS_EXTLINUX" = true ]; then
    for path in /usr/lib/syslinux/mbr/mbr.bin /usr/share/syslinux/mbr.bin /usr/lib/syslinux/mbr.bin; do
        if [ -f "$path" ]; then
            MBR_BIN="$path"
            break
        fi
    done
fi

# Calculate sizes
ROOTFS_SIZE=$(stat -c%s "$ROOTFS")
ROOTFS_SIZE_MB=$((ROOTFS_SIZE / 1024 / 1024))
if [ "$ROOTFS_SIZE_MB" -lt "$ROOT_SIZE" ]; then
    ROOT_SIZE=$((ROOTFS_SIZE_MB + 5))
fi

echo "=========================================="
echo "Typewrite OS USB Installer"
echo "=========================================="
echo "Mode: $(if [ "$HYBRID_MODE" = true ]; then echo "Hybrid BIOS+UEFI"; elif [ "$UEFI_ONLY" = true ]; then echo "UEFI Only"; else echo "BIOS Only"; fi)"
echo ""
echo "Device: $DEVICE"
echo "Kernel: $KERNEL"
echo "RootFS: $ROOTFS (${ROOTFS_SIZE_MB}MB)"
echo ""
echo "Partitions:"
if [ "$HYBRID_MODE" = true ]; then
    echo "  Partition 1: BIOS Boot (1MB, unformatted)"
    echo "  Partition 2: EFI System (FAT32, 100MB)"
    echo "  Partition 3: Boot/Root (ext4, ${ROOT_SIZE}MB)"
elif [ "$UEFI_ONLY" = true ]; then
    echo "  Partition 1: EFI System (FAT32, 100MB)"
    echo "  Partition 2: Boot/Root (ext4, ${ROOT_SIZE}MB)"
else
    echo "  Partition 1: Boot/Root (ext4, ${ROOT_SIZE}MB)"
fi
if [ "$NO_DOCS" = false ]; then
    echo "  Partition $(( $([ "$HYBRID_MODE" = true ] && echo 4 || echo 3) )): Documents (FAT32)"
fi
echo ""
echo "WARNING: ALL DATA ON $DEVICE WILL BE ERASED!"
echo ""
read -p "Press Ctrl+C to cancel, or Enter to continue..." -r

# Unmount if mounted
echo "Unmounting any existing partitions..."
sudo umount "${DEVICE}"* 2>/dev/null || true
sleep 1

# Create partition table
echo "Creating partition table..."
if [ "$HYBRID_MODE" = true ]; then
    # GPT with BIOS boot partition for hybrid
    sudo parted -s "$DEVICE" mklabel gpt
    # BIOS boot partition (unformatted, for BIOS boot)
    sudo parted -s "$DEVICE" mkpart primary 2048s 4095s
    sudo parted -s "$DEVICE" set 1 bios_grub on
    # EFI System Partition - MUST have esp flag for Mac boot
    sudo parted -s "$DEVICE" mkpart primary fat32 4096s "${EFI_SIZE}MB"
    sudo parted -s "$DEVICE" set 2 esp on
    # Boot/Root partition
    BOOT_END=$((${EFI_SIZE} + ${ROOT_SIZE}))
    sudo parted -s "$DEVICE" mkpart primary ext4 "${EFI_SIZE}MB" "${BOOT_END}MB"
    # Documents partition (remaining space)
    if [ "$NO_DOCS" = false ]; then
        sudo parted -s "$DEVICE" mkpart primary fat32 "${BOOT_END}MB" 100%
    fi
    
    PART_EFI="${DEVICE}2"
    PART_BOOT="${DEVICE}3"
    if [ "$NO_DOCS" = false ]; then
        PART_DOCS="${DEVICE}4"
    fi
elif [ "$UEFI_ONLY" = true ]; then
    # GPT with EFI only
    sudo parted -s "$DEVICE" mklabel gpt
    sudo parted -s "$DEVICE" mkpart primary fat32 1MiB "$((1 + EFI_SIZE))MB"
    sudo parted -s "$DEVICE" set 1 esp on
    BOOT_START=$((1 + EFI_SIZE))
    sudo parted -s "$DEVICE" mkpart primary ext4 "${BOOT_START}MB" "$((BOOT_START + ROOT_SIZE))MB"
    if [ "$NO_DOCS" = false ]; then
        DOCS_START=$((BOOT_START + ROOT_SIZE))
        sudo parted -s "$DEVICE" mkpart primary fat32 "${DOCS_START}MB" 100%
    fi
    
    PART_EFI="${DEVICE}1"
    PART_BOOT="${DEVICE}2"
    if [ "$NO_DOCS" = false ]; then
        PART_DOCS="${DEVICE}3"
    fi
else
    # Legacy MBR
    sudo parted -s "$DEVICE" mklabel msdos
    sudo parted -s "$DEVICE" mkpart primary ext4 1MiB "${ROOT_SIZE}MB"
    sudo parted -s "$DEVICE" set 1 boot on
    if [ "$NO_DOCS" = false ]; then
        DOCS_START=$((1 + ROOT_SIZE))
        sudo parted -s "$DEVICE" mkpart primary fat32 "${DOCS_START}MB" 100%
        sudo parted -s "$DEVICE" set 2 lba on
    fi
    
    PART_BOOT="${DEVICE}1"
    if [ "$NO_DOCS" = false ]; then
        PART_DOCS="${DEVICE}2"
    fi
fi

# Wait for kernel to update partition table
sleep 2
sudo partprobe "$DEVICE" 2>/dev/null || true

# Format partitions
echo "Formatting partitions..."

if [ "$HYBRID_MODE" = true ] || [ "$UEFI_ONLY" = true ]; then
    echo "  EFI System Partition..."
    sudo mkfs.fat -F 32 -n "EFI" "$PART_EFI"
fi

echo "  Boot/Root partition..."
sudo mkfs.ext4 -F -L "$BOOT_LABEL" "$PART_BOOT"

if [ "$NO_DOCS" = false ]; then
    echo "  Documents partition..."
    sudo mkfs.vfat -F 32 -n "DOCUMENTS" "$PART_DOCS"
fi

# Install MBR for hybrid/BIOS mode
if [ "$HYBRID_MODE" = true ] && [ -n "$MBR_BIN" ]; then
    echo "Installing hybrid MBR..."
    sudo dd if="$MBR_BIN" of="$DEVICE" bs=440 count=1 status=none
fi

# Mount boot partition
MOUNT_DIR="/mnt/typewrite-install"
echo "Mounting boot partition..."
sudo mkdir -p "$MOUNT_DIR"
sudo mount "$PART_BOOT" "$MOUNT_DIR"

echo "Copying system files..."
sudo mkdir -p "$MOUNT_DIR/boot"

# Copy kernel
sudo cp "$KERNEL" "$MOUNT_DIR/boot/bzImage"

# Extract rootfs
echo "Extracting root filesystem..."
ROOTFS_MNT="/tmp/rootfs-extract-$$"
sudo mkdir -p "$ROOTFS_MNT"
sudo mount -o loop,ro "$ROOTFS" "$ROOTFS_MNT"
sudo cp -a "$ROOTFS_MNT/"* "$MOUNT_DIR/"
sudo umount "$ROOTFS_MNT"
sudo rmdir "$ROOTFS_MNT"

# Create directories
sudo mkdir -p "$MOUNT_DIR/root/Documents"
sudo mkdir -p "$MOUNT_DIR/boot/extlinux"

# Copy extlinux modules and install for BIOS/UEFI hybrid
if [ "$HYBRID_MODE" = true ] || [ "$HAS_EXTLINUX" = true ]; then
    echo "Installing extlinux (BIOS)..."
    for mod in ldlinux.c32 libcom32.c32 libutil.c32 menu.c32; do
        for path in "/usr/lib/syslinux/modules/bios/$mod" "/usr/share/syslinux/$mod" "/usr/lib/syslinux/$mod" "/usr/lib/EXTLINUX/$mod"; do
            if [ -f "$path" ]; then
                sudo cp "$path" "$MOUNT_DIR/boot/extlinux/" 2>/dev/null || true
                break
            fi
        done
    done
    
    sudo extlinux --install "$MOUNT_DIR/boot/extlinux"
fi

# Install GRUB for UEFI
if [ "$HYBRID_MODE" = true ] || [ "$UEFI_ONLY" = true ]; then
    echo "Installing GRUB (UEFI)..."
    
    # Create EFI directories
    sudo mkdir -p "$MOUNT_DIR/boot/EFI/BOOT"
    sudo mkdir -p "$MOUNT_DIR/boot/grub"
    
    # Create GRUB config
    sudo tee "$MOUNT_DIR/boot/grub/grub.cfg" > /dev/null << 'GRUBEOF'
set timeout=10
set default=0

menuentry "Typewrite OS (1280x800)" {
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=817
}

menuentry "Typewrite OS (1024x768)" {
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=791
}

menuentry "Typewrite OS (1440x900)" {
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=855
}

menuentry "Typewrite OS (1920x1080)" {
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=0x1B8
}

menuentry "Typewrite OS (Safe - 800x600)" {
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=771
}

menuentry "Troubleshooting (Text Mode)" {
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=text
}

menuentry "Troubleshooting (Shell)" {
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 shell=1
}

menuentry "Troubleshooting (Graphics Debug)" {
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 gfxdebug=1
}

menuentry "Troubleshooting (Safe Mode)" {
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=normal
}
GRUBEOF

    # Create standalone GRUB image (no runtime modules needed)
    if [ -d /usr/lib/grub/x86_64-efi ]; then
        echo "  Building GRUB x86_64-efi image..."
        sudo grub-mkimage \
            -O x86_64-efi \
            -o "$MOUNT_DIR/boot/EFI/BOOT/bootx64.efi" \
            -p /boot/grub \
            part_gpt part_msdos ext2 normal boot linux search search_fs_uuid \
            halt reboot cat echo ls test sleep true loadenv \
            2>/dev/null || true
    fi
    
    # Also create IA32 image for older Macs (MacBook Air 2010 uses 32-bit EFI)
    if [ -d /usr/lib/grub/i386-efi ]; then
        echo "  Building GRUB i386-efi image (for older Macs)..."
        sudo grub-mkimage \
            -O i386-efi \
            -o "$MOUNT_DIR/boot/EFI/BOOT/bootia32.efi" \
            -p /boot/grub \
            part_gpt part_msdos ext2 normal boot linux search search_fs_uuid \
            halt reboot cat echo ls test sleep true loadenv \
            2>/dev/null || true
    fi
    
    # Create robust EFI structure (some UEFIs are case-sensitive!)
    echo "  Creating robust EFI boot structure..."
    
    # Standard EFI boot files (uppercase - some UEFIs require this)
    # These go in root of EFI partition (sda2)
    sudo mkdir -p "$MOUNT_DIR/EFI/BOOT"
    if [ -f "$MOUNT_DIR/boot/EFI/BOOT/bootx64.efi" ]; then
        sudo cp "$MOUNT_DIR/boot/EFI/BOOT/bootx64.efi" "$MOUNT_DIR/EFI/BOOT/BOOTX64.EFI"
    fi
    if [ -f "$MOUNT_DIR/boot/EFI/BOOT/bootia32.efi" ]; then
        sudo cp "$MOUNT_DIR/boot/EFI/BOOT/bootia32.efi" "$MOUNT_DIR/EFI/BOOT/BOOTIA32.EFI"
    fi
    
    # Microsoft fallback (some UEFIs look for this)
    sudo mkdir -p "$MOUNT_DIR/EFI/Microsoft/Boot"
    if [ -f "$MOUNT_DIR/boot/EFI/BOOT/bootx64.efi" ]; then
        sudo cp "$MOUNT_DIR/boot/EFI/BOOT/bootx64.efi" "$MOUNT_DIR/EFI/Microsoft/Boot/bootmgfw.efi"
    fi
    
    # Create fallback grub.cfg in root of EFI partition
    sudo tee "$MOUNT_DIR/EFI/BOOT/grub.cfg" > /dev/null << 'EOF'
set timeout=5
set default=0

menuentry "Typewrite OS" {
    search --label --set=root TYPEWRITE
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=817
}

menuentry "Typewrite OS (Safe 800x600)" {
    search --label --set=root TYPEWRITE
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=771
}

menuentry "Typewrite OS (Text)" {
    search --label --set=root TYPEWRITE
    linux /boot/bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=text
}
EOF

    echo "  EFI boot files installed"
fi

# Create extlinux config (for BIOS boot)
if [ "$HYBRID_MODE" = true ] || [ "$HAS_EXTLINUX" = true ]; then
    echo "Creating boot configuration..."
    sudo tee "$MOUNT_DIR/boot/extlinux/extlinux.conf" > /dev/null << 'EOF'
DEFAULT typewrite_1280

MENU TITLE Typewrite OS Boot Menu
TIMEOUT 30
PROMPT 1

UI menu.c32

MENU BEGIN Typewrite OS

LABEL typewrite_1280
    MENU LABEL Typewrite OS (1280x800 - Laptop WXGA)
    LINUX /boot/bzImage
    APPEND root=/dev/sda3 rw console=tty0 rootdelay=5 vga=817

LABEL typewrite_normal
    MENU LABEL Typewrite OS (1024x768)
    LINUX /boot/bzImage
    APPEND root=/dev/sda3 rw console=tty0 rootdelay=5 vga=791

LABEL typewrite_1440
    MENU LABEL Typewrite OS (1440x900)
    LINUX /boot/bzImage
    APPEND root=/dev/sda3 rw console=tty0 rootdelay=5 vga=855

LABEL typewrite_1920
    MENU LABEL Typewrite OS (1920x1080 - Full HD)
    LINUX /boot/bzImage
    APPEND root=/dev/sda3 rw console=tty0 rootdelay=5 vga=0x1B8

LABEL typewrite_safe
    MENU LABEL Typewrite OS (Safe Mode - 800x600)
    LINUX /boot/bzImage
    APPEND root=/dev/sda3 rw console=tty0 rootdelay=5 vga=771

LABEL typewrite_vga
    MENU LABEL Typewrite OS (VGA Mode - 640x480)
    LINUX /boot/bzImage
    APPEND root=/dev/sda3 rw console=tty0 rootdelay=5 vga=785

MENU SEPARATOR

LABEL typewrite_text
    MENU LABEL Troubleshooting (Text Mode)
    LINUX /boot/bzImage
    APPEND root=/dev/sda3 rw console=tty0 rootdelay=5 vga=text

LABEL typewrite_nofb
    MENU LABEL Troubleshooting (No Framebuffer)
    LINUX /boot/bzImage
    APPEND root=/dev/sda3 rw console=tty0 rootdelay=5

LABEL typewrite_gfxdebug
    MENU LABEL Troubleshooting (Graphics Debug)
    LINUX /boot/bzImage
    APPEND root=/dev/sda3 rw console=tty0 rootdelay=5 gfxdebug=1

LABEL typewrite_shell
    MENU LABEL Troubleshooting (Shell)
    LINUX /boot/bzImage
    APPEND root=/dev/sda3 rw console=tty0 rootdelay=5 shell=1

MENU SEPARATOR
TEXT HELP
Select display mode. Try Safe Mode if graphics look wrong.
Use Graphics Debug to detect hardware issues.
ENDTEXT

MENU END
EOF
fi

# Add fstab entry for documents partition
if [ "$NO_DOCS" = false ]; then
    echo "Configuring documents mount..."
    if [ "$HYBRID_MODE" = true ]; then
        DOCS_DEV="/dev/sda4"
    elif [ "$UEFI_ONLY" = true ]; then
        DOCS_DEV="/dev/sda3"
    else
        DOCS_DEV="/dev/sda2"
    fi
    sudo tee -a "$MOUNT_DIR/etc/fstab" > /dev/null << EOF
$DOCS_DEV  /root/Documents  vfat  defaults,noatime  0  0
EOF
fi

# Unmount boot partition
echo "Unmounting boot partition..."
sudo umount "$MOUNT_DIR"

# Setup documents partition
if [ "$NO_DOCS" = false ]; then
    echo "Setting up documents partition..."
    sudo mkdir -p "$MOUNT_DIR"
    sudo mount "$PART_DOCS" "$MOUNT_DIR"
    
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
fi

# Install EFI bootloader to ESP
if [ "$HYBRID_MODE" = true ] || [ "$UEFI_ONLY" = true ]; then
    echo "Installing EFI bootloader..."
    sudo mkdir -p "$MOUNT_DIR"
    sudo mount "$PART_EFI" "$MOUNT_DIR"
    
    # CreateEFI directories
    sudo mkdir -p "$MOUNT_DIR/EFI/BOOT"
    sudo mkdir -p "$MOUNT_DIR/EFI/TYPEWRITE"
    
    # Copy GRUB to EFI locations
    if [ -f "$MOUNT_DIR/boot/EFI/BOOT/bootx64.efi" ]; then
        sudo cp "$MOUNT_DIR/boot/EFI/BOOT/bootx64.efi" "$MOUNT_DIR/EFI/BOOT/"
    fi
    if [ -f "$MOUNT_DIR/boot/EFI/BOOT/bootia32.efi" ]; then
        sudo cp "$MOUNT_DIR/boot/EFI/BOOT/bootia32.efi" "$MOUNT_DIR/EFI/BOOT/"
    fi
    
    # Create fallback config (if bootx64.efi can't be found, boot.efi might be tried)
    sudo tee "$MOUNT_DIR/EFI/BOOT/grub.cfg" > /dev/null << 'EOF'
set timeout=10
set default=0

menuentry "Typewrite OS (1280x800)" {
    linux /bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=817
}

menuentry "Typewrite OS (1024x768)" {
    linux /bzImage root=/dev/sda3 rw console=tty0 rootdelay=5 vga=791
}
EOF

    # Copy kernel to ESP root for direct EFI boot
    sudo cp "$KERNEL" "$MOUNT_DIR/bzImage"
    
    sudo umount "$MOUNT_DIR"
fi

sudo rmdir "$MOUNT_DIR" 2>/dev/null || true

echo ""
echo "=========================================="
echo "Installation complete!"
echo "=========================================="
echo ""
echo "Partition layout:"
if [ "$HYBRID_MODE" = true ]; then
    echo "  /dev/sda1 - BIOS Boot (reserved)"
    echo "  /dev/sda2 - EFI System Partition (FAT32)"
    echo "  /dev/sda3 - Boot/Root (ext4)"
    echo "  /dev/sda4 - Documents (FAT32)"
elif [ "$UEFI_ONLY" = true ]; then
    echo "  /dev/sda1 - EFI System Partition (FAT32)"
    echo "  /dev/sda2 - Boot/Root (ext4)"
    echo "  /dev/sda3 - Documents (FAT32)"
else
    echo "  /dev/sda1 - Boot/Root (ext4)"
    echo "  /dev/sda2 - Documents (FAT32)"
fi
echo ""
echo "Boot options:"
if [ "$HYBRID_MODE" = true ]; then
    echo "  BIOS: Boot from USB, select from extlinux menu"
    echo "  UEFI: Boot from USB, select 'EFI USB Device' or similar"
elif [ "$UEFI_ONLY" = true ]; then
    echo "  UEFI: Boot from USB, select 'EFI USB Device'"
else
    echo "  BIOS: Boot from USB, select from extlinux menu"
fi
echo ""
echo "For MacBook Air 2010:"
echo "  1. Hold Option/Alt while booting"
echo "  2. Select 'EFI Boot' or the USB drive icon"
echo ""
echo "For ThinkPad T60:"
echo "  1. Press F12 during boot"
echo "  2. Select USB from boot menu"
