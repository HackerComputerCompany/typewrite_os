#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDROOT_DIR="$SCRIPT_DIR/buildroot-2024.02"
IMAGES_DIR="$BUILDROOT_DIR/output/images"

KERNEL="$IMAGES_DIR/bzImage"
ROOTFS="$IMAGES_DIR/rootfs.ext2"
INITRD="$IMAGES_DIR/rootfs.cpio.gz"

SERIAL_SOCK="/tmp/typewrite-serial.sock"
SERIAL=0
KVM=1

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --serial    Run with serial console output"
    echo "  --no-kvm    Disable KVM acceleration"
    echo "  --help      Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0              # Normal run with KVM acceleration"
    echo "  $0 --serial     # Run with serial console only"
    echo "  $0 --no-kvm     # Run without KVM (slower)"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --serial)
            SERIAL=1
            shift
            ;;
        --no-kvm)
            KVM=0
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

if [ "$KVM" -eq 1 ]; then
    KVM_OPTS="-enable-kvm -cpu host"
else
    KVM_OPTS=""
fi

if [ "$SERIAL" -eq 1 ]; then
    exec qemu-system-x86_64 \
        -kernel "$KERNEL" \
        -drive "file=$ROOTFS,if=virtio,format=raw" \
        -m 512 \
        -nographic \
        -append "console=tty0 root=/dev/vda rw" \
        $KVM_OPTS \
        -netdev user,id=net0 -device virtio-net-pci,netdev=net0
else
    rm -f "$SERIAL_SOCK"
    echo "Starting Typewrite OS..."
    echo ""
    echo "Serial console available at: $SERIAL_SOCK"
    echo "Connect from another terminal with: socat - UNIX-CONNECT:$SERIAL_SOCK"
    echo "Or: minicom -D \"unix#$SERIAL_SOCK\""
    echo ""
    exec env SDL_VIDEO_WINDOW_POS=0,0 SDL_VIDEO_WINDOW_SIZE=1920x1080 qemu-system-x86_64 \
        -kernel "$KERNEL" \
        -drive "file=$ROOTFS,if=virtio,format=raw" \
        -m 512 \
        -vga std \
        -display sdl \
        -append "console=tty0 root=/dev/vda rw" \
        -serial "unix:$SERIAL_SOCK,server,nowait" \
        $KVM_OPTS \
        -netdev user,id=net0 -device virtio-net-pci,netdev=net0
fi
