#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDROOT_DIR="$SCRIPT_DIR/buildroot-2024.02"
IMAGES_DIR="$BUILDROOT_DIR/output/images"

KERNEL="$IMAGES_DIR/bzImage"
ROOTFS="$IMAGES_DIR/rootfs.ext2"
INITRD="$IMAGES_DIR/rootfs.cpio.gz"

SERIAL=0
KVM=0

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --serial    Run with serial console output"
    echo "  --kvm       Enable KVM acceleration (requires hardware VT)"
    echo "  --help      Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0              # Normal run with SDL/framebuffer display"
    echo "  $0 --serial     # Run with serial console only"
    echo "  $0 --kvm        # Run with KVM acceleration"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --serial)
            SERIAL=1
            shift
            ;;
        --kvm)
            KVM=1
            shift
            ;;
        --help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

if [ ! -f "$KERNEL" ]; then
    echo "Error: Kernel not found at $KERNEL"
    echo "Please run: cd $BUILDROOT_DIR && make"
    exit 1
fi

if [ ! -f "$ROOTFS" ]; then
    echo "Error: Root filesystem not found at $ROOTFS"
    echo "Please run: cd $BUILDROOT_DIR && make"
    exit 1
fi

QEMU_CMD="qemu-system-x86_64"

QEMU_CMD="$QEMU_CMD -kernel $KERNEL"
QEMU_CMD="$QEMU_CMD -hda $ROOTFS"
QEMU_CMD="$QEMU_CMD -m 512"

QEMU_CMD="$QEMU_CMD -vga std"
QEMU_CMD="$QEMU_CMD -display sdl"

QEMU_CMD="$QEMU_CMD -append \"console=tty0 root=/dev/sda rw\""

if [ "$SERIAL" -eq 1 ]; then
    QEMU_CMD="$QEMU_CMD -serial stdio"
    QEMU_CMD="${QEMU_CMD//-display sdl/}"
else
    QEMU_CMD="$QEMU_CMD -serial file:$SCRIPT_DIR/serial.log"
fi

if [ "$KVM" -eq 1 ]; then
    if grep -q "vmx\|svm" /proc/cpuinfo 2>/dev/null; then
        QEMU_CMD="$QEMU_CMD -enable-kvm -cpu host"
    else
        echo "Warning: KVM not available, running without acceleration"
    fi
fi

QEMU_CMD="$QEMU_CMD -netdev user,id=net0 -device virtio-net-pci,netdev=net0"

echo "Starting Typewrite OS..."
echo "Command: $QEMU_CMD"
echo ""

exec $QEMU_CMD
