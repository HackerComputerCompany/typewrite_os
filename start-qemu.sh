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

echo "Starting Typewrite OS..."
echo ""

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
    exec qemu-system-x86_64 \
        -kernel "$KERNEL" \
        -drive "file=$ROOTFS,if=virtio,format=raw" \
        -m 512 \
        -vga std \
        -display sdl \
        -append "console=tty0 root=/dev/vda rw" \
        -serial "file:$SCRIPT_DIR/serial.log" \
        $KVM_OPTS \
        -netdev user,id=net0 -device virtio-net-pci,netdev=net0
fi
