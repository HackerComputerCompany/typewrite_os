#!/bin/bash
# USB Installer for Typewrite OS - Single Partition EFI
# ALL files on one FAT32 partition - simplest approach for Mac
# Kernel + Rootfs on same partition as bootloader

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDROOT_DIR="$SCRIPT_DIR/buildroot-2024.02"
IMAGES_DIR="$BUILDROOT_DIR/output/images"
TOOLS_DIR="$SCRIPT_DIR/bootloader/tools"
REFIND_LOCAL="$SCRIPT_DIR/bootloader/refind/refind-bin-0.14.2"

# Use existing x86_64 kernel (32-bit i686 build needs host dependencies)
KERNEL="$IMAGES_DIR/bzImage"
ROOTFS="$IMAGES_DIR/rootfs.ext2"

if [ ! -f "$KERNEL" ]; then
    echo "Error: No kernel found. Build with:"
    echo "  cd buildroot-2024.02 && make"
    exit 1
fi

echo "Using kernel: $KERNEL"
echo "Kernel info:"
file "$KERNEL"

REFIND_LOCAL="$SCRIPT_DIR/bootloader/refind/refind-bin-0.14.2"

if [ $# -lt 1 ]; then
    echo "Usage: $0 /dev/sdX"
    echo ""
    echo "Creates a USB with ALL files on one FAT32 partition"
    echo "Simplest approach - everything self-contained"
    echo ""
    echo "WARNING: ALL DATA WILL BE ERASED!"
    exit 1
fi

DEVICE="$1"

echo "=========================================="
echo "Typewrite OS - Single Partition USB"
echo "=========================================="
echo ""

# Check for required files
if [ ! -f "$KERNEL" ]; then
    echo "Error: Kernel not found. Build first:"
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

# Single partition layout - everything on FAT32
echo "Creating single FAT32 partition..."
sudo parted -s "$DEVICE" mklabel gpt
sudo parted -s "$DEVICE" mkpart primary fat32 1MiB 100%
sudo parted -s "$DEVICE" set 1 esp on

sleep 2
sudo partprobe "$DEVICE" 2>/dev/null || true
sudo udevadm settle 2>/dev/null || true

PART="${DEVICE}1"

# Format as FAT32
echo "Formatting..."
sudo mkfs.fat -F 32 -n "TYPEWRITE" "$PART"

# Mount
MOUNT_DIR="/mnt/typewrite-single"
sudo mkdir -p "$MOUNT_DIR"
sudo mount "$PART" "$MOUNT_DIR"

# Create EFI directory structure
echo "Creating directory structure..."
sudo mkdir -p "$MOUNT_DIR/efi/boot"
sudo mkdir -p "$MOUNT_DIR/efi/boot/icons"
sudo mkdir -p "$MOUNT_DIR/boot"

# Install rEFInd IA32 for MacBook Air 2010
if [ -f "$REFIND_LOCAL/refind/refind_ia32.efi" ]; then
    echo "Installing rEFInd IA32..."
    sudo cp "$REFIND_LOCAL/refind/refind_ia32.efi" "$MOUNT_DIR/efi/boot/bootia32.efi"
    sudo cp "$REFIND_LOCAL/refind/refind_ia32.efi" "$MOUNT_DIR/efi/boot/boot.efi"
fi

if [ -f "$REFIND_LOCAL/refind/refind_x64.efi" ]; then
    echo "Installing rEFInd x64..."
    sudo cp "$REFIND_LOCAL/refind/refind_x64.efi" "$MOUNT_DIR/efi/boot/bootx64.efi"
fi

# Copy icons
if [ -d "$REFIND_LOCAL/refind/icons" ]; then
    sudo cp -r "$REFIND_LOCAL/refind/icons/"* "$MOUNT_DIR/efi/boot/icons/" 2>/dev/null || true
fi

# Install diagnostic tools
echo "Installing diagnostic tools..."
if [ -f "$TOOLS_DIR/shellx64.efi" ]; then
    sudo cp "$TOOLS_DIR/shellx64.efi" "$MOUNT_DIR/efi/boot/shell.efi"
    sudo cp "$TOOLS_DIR/shellx64.efi" "$MOUNT_DIR/shell.efi"
    echo "  Added EFI Shell (includes edit command)"
fi

# Create startup script for shell
if [ -f "$MOUNT_DIR/startup.nsh" ]; then
    echo "  Added startup.nsh"
fi

# Build GRUB IA32 that can load 64-bit kernel
echo "Building GRUB IA32 chainloader..."
GRUB_MKIMAGE=""
for bin in "/usr/bin/grub-mkimage" "$BUILDROOT_DIR/output/host/bin/grub-mkimage"; do
    if [ -f "$bin" ]; then
        GRUB_MKIMAGE="$bin"
        break
    fi
done

GRUB_MODULES=""
for dir in "/usr/lib/grub/i386-efi" "$BUILDROOT_DIR/output/host/lib/grub/i386-efi"; do
    if [ -d "$dir" ] && [ -f "$dir/linux.mod" ]; then
        GRUB_MODULES="$dir"
        break
    fi
done

if [ -n "$GRUB_MKIMAGE" ] && [ -n "$GRUB_MODULES" ]; then
    # Create GRUB config - kernel on same partition at /boot/bzImage
    sudo tee "$MOUNT_DIR/efi/boot/grub.cfg" > /dev/null << 'EOF'
set timeout=10
set default=0

menuentry "Typewrite OS (1280x800)" {
    linux /boot/bzImage root=/dev/sda1 rw console=tty0 vga=817
}

menuentry "Typewrite OS (1024x768)" {
    linux /boot/bzImage root=/dev/sda1 rw console=tty0 vga=791
}

menuentry "Typewrite OS (800x600)" {
    linux /boot/bzImage root=/dev/sda1 rw console=tty0 vga=771
}

menuentry "Typewrite OS (Text)" {
    linux /boot/bzImage root=/dev/sda1 rw console=tty0 vga=text
}
EOF

    # Build standalone GRUB IA32 with embedded config
    "$GRUB_MKIMAGE" -O i386-efi \
        -p /efi/boot \
        -c "$MOUNT_DIR/efi/boot/grub.cfg" \
        -o "$MOUNT_DIR/efi/boot/grubia32.efi" \
        part_gpt fat normal linux boot cat echo ls \
        2>&1 && echo "GRUB IA32 built!" || echo "GRUB IA32 build failed"
else
    echo "Warning: Could not find GRUB tools"
fi

# Copy kernel to /boot on the same partition
echo "Copying kernel to /boot..."
sudo cp "$KERNEL" "$MOUNT_DIR/boot/bzImage"

# Copy rootfs and extract
# Note: FAT32 doesn't support symlinks, so we use --copy-links to dereference them
echo "Extracting rootfs to /..."
ROOTFS_MNT="/tmp/rootfs-$$"
sudo mkdir -p "$ROOTFS_MNT"
sudo mount -o loop,ro "$ROOTFS" "$ROOTFS_MNT"
sudo cp -aL --no-preserve=ownership "$ROOTFS_MNT/"* "$MOUNT_DIR/" 2>/dev/null || \
sudo cp -a --no-preserve=links "$ROOTFS_MNT/"* "$MOUNT_DIR/" 2>/dev/null || \
sudo rsync -a --copy-links "$ROOTFS_MNT/" "$MOUNT_DIR/" 2>/dev/null || \
find "$ROOTFS_MNT" -mindepth 1 -exec sudo cp -a --no-preserve=links {} "$MOUNT_DIR/" \; 2>/dev/null
sudo umount "$ROOTFS_MNT"
sudo rmdir "$ROOTFS_MNT"

# Update fstab for single partition
echo "Updating fstab..."
sudo mkdir -p "$MOUNT_DIR/root/Documents"
sudo tee "$MOUNT_DIR/etc/fstab" > /dev/null << EOF
/dev/sda1  /           ext4  defaults,noatime  0  0
/dev/sda1  /root/Documents  vfat  defaults,noatime  0  0
EOF

# Create rEFInd config with diagnostic options
echo "Creating rEFInd config..."
sudo tee "$MOUNT_DIR/efi/boot/refind.conf" > /dev/null << 'EOF'
timeout 20
default 0
scanfor Manual,External
scan_delay 1

# === DIAGNOSTIC TOOLS ===
menuentry ">>> EFI Shell (Debug)" {
    icon /efi/boot/icons/tool_shell.icns
    loader /efi/boot/shell.efi
}

menuentry ">>> Text Editor (via Shell)" {
    icon /efi/boot/icons/tool_part.icns
    loader /efi/boot/shell.efi
    options "-nomap"
}

menuentry ">>> GRUB (Manual Boot)" {
    icon /efi/boot/icons/os_ubuntu.icns
    loader /efi/boot/grubia32.efi
}

# === TYPEWRITE OS ===
menuentry "Typewrite OS (1280x800)" {
    icon /efi/boot/icons/os_ubuntu.icns
    loader /boot/bzImage
    options "root=/dev/sda1 rw console=tty0 vga=817"
}

menuentry "Typewrite OS (1024x768)" {
    icon /efi/boot/icons/os_ubuntu.icns
    loader /boot/bzImage
    options "root=/dev/sda1 rw console=tty0 vga=791"
}

menuentry "Typewrite OS (800x600)" {
    icon /efi/boot/icons/os_ubuntu.icns
    loader /boot/bzImage
    options "root=/dev/sda1 rw console=tty0 vga=771"
}

menuentry "Typewrite OS (Text Only)" {
    icon /efi/boot/icons/tar.icns
    loader /boot/bzImage
    options "root=/dev/sda1 rw console=tty0 vga=text"
}

# === TROUBLESHOOTING ===
menuentry "Troubleshoot: Verbose Boot" {
    icon /efi/boot/icons/tar.icns
    loader /boot/bzImage
    options "root=/dev/sda1 rw console=tty0 vga=817 debug"
}

menuentry "Troubleshoot: Shell Instead of App" {
    icon /efi/boot/icons/tool_shell.icns
    loader /boot/bzImage
    options "root=/dev/sda1 rw console=tty0 vga=817 shell=1"
}

menuentry "Troubleshoot: No Framebuffer" {
    icon /efi/boot/icons/tar.icns
    loader /boot/bzImage
    options "root=/dev/sda1 rw console=tty0 vga=text nofb"
}
EOF

# Create a startup script for the shell that opens editor
sudo tee "$MOUNT_DIR/startup.nsh" > /dev/null << 'EOF'
# Typewrite OS EFI Shell Startup
# Type 'edit filename' to edit text files
# Use 'ls' to list files, 'cd' to change directories
# Type 'exit' to return to rEFInd

echo ""
echo "==========================================="
echo "Typewrite OS - EFI Shell"
echo "==========================================="
echo ""
echo "Available commands:"
echo "  edit <file>   - Edit a text file"
echo "  ls            - List files"
echo "  cd <dir>      - Change directory"
echo "  cat <file>    - Display file contents"
echo "  mkdir <dir>   - Create directory"
echo "  rm <file>     - Delete file"
echo "  exit          - Return to boot menu"
echo ""
echo "Files are on: FS0: (the EFI partition)"
echo ""
EOF

# Verify structure
echo ""
echo "=== Partition contents ==="
ls -la "$MOUNT_DIR/"
echo ""
echo "/boot:"
ls -la "$MOUNT_DIR/boot/"
echo ""
echo "/efi/boot:"
ls -la "$MOUNT_DIR/efi/boot/"

sudo umount "$MOUNT_DIR"
sudo rmdir "$MOUNT_DIR" 2>/dev/null || true

echo ""
echo "=========================================="
echo "Single Partition USB Ready!"
echo "=========================================="
echo ""
echo "Partition layout:"
echo "  /dev/sda1 - Everything [FAT32]"
echo "    /boot/bzImage - Kernel"
echo "    /extracted/ - Root filesystem"
echo "    /efi/boot/ - Bootloader"
echo ""
echo "NOTE: Single partition means simpler boot but no separate docs partition."
echo "Your documents will be stored in /root/Documents on the FAT32 partition."
