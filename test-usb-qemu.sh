#!/bin/bash
# Typewrite OS - USB Boot Test Script
# Test your USB drive in QEMU before trying on real hardware

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDROOT_DIR="$SCRIPT_DIR/buildroot-2024.02"
IMAGES_DIR="$BUILDROOT_DIR/output/images"

usage() {
    cat << EOF
Usage: $0 [OPTIONS] [DEVICE_OR_IMAGE]

Test Typewrite OS USB boot in QEMU.

OPTIONS:
    --bios               Boot in BIOS/Legacy mode (default)
    --uefi               Boot in UEFI mode (requires OVMF)
    --kvm                Enable KVM acceleration (default)
    --no-kvm             Disable KVM (slower but more compatible)
    --ram SIZE           RAM size (default: 512M)
    --cpu CPU            CPU model (default: host)
    --snapshot           Don't write to USB (use with /dev/sdX)
    --verbose            Show QEMU command
    --serial             Output to serial console
    --res WxH            Resolution for built-in kernel (default: 1024x768)
    --help               Show this help

EXAMPLES:
    # Test USB image file - boots from USB with menu
    $0 usb-drive.img

    # Test real USB drive (read-only via snapshot)
    $0 /dev/sdb --snapshot

    # Test with UEFI - boots from USB with GRUB menu
    $0 usb-drive.img --uefi

    # Test without KVM (for testing kernel/init issues)
    $0 usb-drive.img --no-kvm

    # Test built-in rootfs at specific resolution
    $0 --res 1280x800

EOF
}

# Defaults
BOOT_MODE="bios"
KVM=1
RAM="512M"
CPU="host"
SNAPSHOT=""
VERBOSE=0
SERIAL=0
DEVICE=""
UEFI_OVMF=""
RESOLUTION="1024x768"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --bios)
            BOOT_MODE="bios"
            shift
            ;;
        --uefi)
            BOOT_MODE="uefi"
            ;;
        --kvm)
            KVM=1
            shift
            ;;
        --no-kvm)
            KVM=0
            shift
            ;;
        --ram)
            RAM="$2"
            shift 2
            ;;
        --cpu)
            CPU="$2"
            shift 2
            ;;
        --snapshot)
            SNAPSHOT="-snapshot"
            shift
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        --serial)
            SERIAL=1
            shift
            ;;
        --res)
            RESOLUTION="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            DEVICE="$1"
            shift
            ;;
    esac
done

# Check for OVMF if UEFI mode
if [ "$BOOT_MODE" = "uefi" ]; then
    # Try common OVMF locations
    for path in \
        "/usr/share/ovmf/OVMF.fd" \
        "/usr/share/edk2/ovmf/OVMF.fd" \
        "/usr/share/qemu/ovmf-x86_64-code.bin" \
        "/usr/share/OVMF/OVMF.fd" \
        "/usr/share/edk2-ovmf/OVMF.fd"
    do
        if [ -f "$path" ]; then
            UEFI_OVMF="$path"
            break
        fi
    done

    if [ -z "$UEFI_OVMF" ]; then
        echo "Error: OVMF not found!"
        echo "Install it with:"
        echo "  Ubuntu/Debian: sudo apt install ovmf"
        echo "  Fedora: sudo dnf install edk2-ovmf"
        echo ""
        echo "Falling back to BIOS mode..."
        BOOT_MODE="bios"
    fi
fi

# Build QEMU command
QEMU_CMD="qemu-system-x86_64"

# CPU
QEMU_CMD="$QEMU_CMD -cpu $CPU"

# RAM
QEMU_CMD="$QEMU_CMD -m $RAM"

# KVM
if [ "$KVM" = "1" ]; then
    QEMU_CMD="$QEMU_CMD -enable-kvm"
fi

# Machine type
if [ "$BOOT_MODE" = "uefi" ]; then
    QEMU_CMD="$QEMU_CMD -machine q35"
    QEMU_CMD="$QEMU_CMD -drive if=pflash,format=raw,readonly=on,file=$UEFI_OVMF"
else
    QEMU_CMD="$QEMU_CMD -machine pc"
fi

# Boot device
if [ -n "$DEVICE" ]; then
    if [ -b "$DEVICE" ]; then
        # Physical device
        echo "Testing physical USB device: $DEVICE"
        
        # Add USB controller
        QEMU_CMD="$QEMU_CMD -device qemu-xhci,id=xhci"
        
        # USB storage device
        QEMU_CMD="$QEMU_CMD -drive if=none,id=usbstick,file=$DEVICE,format=raw $SNAPSHOT"
        QEMU_CMD="$QEMU_CMD -device usb-storage,drive=usbstick"
        
    elif [ -f "$DEVICE" ]; then
        # Image file
        echo "Testing USB image: $DEVICE"
        
        if [ "$BOOT_MODE" = "uefi" ]; then
            # UEFI needs SATA or USB
            QEMU_CMD="$QEMU_CMD -device qemu-xhci,id=xhci"
            QEMU_CMD="$QEMU_CMD -drive if=none,id=usbstick,file=$DEVICE,format=raw"
            QEMU_CMD="$QEMU_CMD -device usb-storage,drive=usbstick"
        else
            # BIOS can boot USB directly
            QEMU_CMD="$QEMU_CMD -drive file=$DEVICE,format=raw,if=usb $SNAPSHOT"
        fi
    else
        echo "Error: $DEVICE not found"
        exit 1
    fi
else
    # No device - use built-in rootfs for testing
    echo "No device specified, using built-in rootfs for testing..."
    
    ROOTFS="$IMAGES_DIR/rootfs.ext2"
    KERNEL="$IMAGES_DIR/bzImage"
    
    if [ ! -f "$KERNEL" ]; then
        echo "Error: Kernel not found at $KERNEL"
        echo "Build first: cd buildroot-2024.02 && make"
        exit 1
    fi
    
    QEMU_CMD="$QEMU_CMD -kernel $KERNEL"
    
    if [ "$BOOT_MODE" = "uefi" ]; then
        QEMU_CMD="$QEMU_CMD -drive file=$ROOTFS,format=raw,if=virtio"
        QEMU_CMD="$QEMU_CMD -append \"console=tty0 console=ttyS0 root=/dev/vda rw video=$RESOLUTION\""
    else
        QEMU_CMD="$QEMU_CMD -drive file=$ROOTFS,format=raw,if=ide"
        QEMU_CMD="$QEMU_CMD -append \"console=tty0 root=/dev/sda rw video=$RESOLUTION\""
    fi
    
    if [ "$KVM" = "1" ]; then
        QEMU_CMD="$QEMU_CMD -enable-kvm"
    else
        QEMU_CMD="$QEMU_CMD -no-kvm"
    fi
fi

# Display output
if [ "$SERIAL" = "1" ]; then
    QEMU_CMD="$QEMU_CMD -nographic"
else
    # Set SDL window size for the resolution
    export SDL_VIDEO_WINDOW_POS=0,0
    export SDL_VIDEO_WINDOW_SIZE=$RESOLUTION
    QEMU_CMD="$QEMU_CMD -display gtk"
fi

# Serial for debugging
QEMU_CMD="$QEMU_CMD -serial file:/tmp/typewrite-serial.log"

# USB 2.0 controller (EHCI)
QEMU_CMD="$QEMU_CMD -device qemu-xhci,id=xhci"

echo ""
echo "========================================"
echo "Typewrite OS - USB Boot Test"
echo "========================================"
echo ""
echo "Boot mode: $BOOT_MODE"
echo "KVM: $([ "$KVM" = "1" ] && echo enabled || echo disabled)"
echo "RAM: $RAM"
echo "Resolution: $RESOLUTION"
echo ""
if [ -n "$DEVICE" ]; then
    echo "Device: $DEVICE"
    echo ""
    echo "Booting from USB - use boot menu to select resolution:"
    echo "  - Select 'Display Options' → choose resolution"
    echo "  - Or select 'Troubleshooting' for debug options"
else
    echo "Booting built-in kernel"
fi
echo ""

# Show command if verbose
if [ "$VERBOSE" = "1" ]; then
    echo "QEMU Command:"
    echo "$QEMU_CMD"
    echo ""
fi

echo "Starting QEMU..."
echo "Press Ctrl+Alt+G to release mouse"
echo "Press Ctrl+C to exit"
echo ""
echo "To view serial output:"
echo "  tail -f /tmp/typewrite-serial.log"
echo ""

# Run QEMU
eval "$QEMU_CMD"
