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
RESOLUTION="1280x1024"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --serial        Run with serial console output"
    echo "  --no-kvm        Disable KVM acceleration"
    echo "  --res WxH       Set native resolution (default: 1280x1024)"
    echo "  --help          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                              # Normal run at 1280x1024"
    echo "  $0 --res 2560x1600              # Run at 2560x1600"
    echo "  $0 --serial                     # Run with serial console only"
    echo "  $0 --no-kvm                     # Run without KVM (slower)"
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
        --res)
            RESOLUTION="$2"
            shift 2
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
        -append "console=ttyS0 root=/dev/vda rw" \
        $KVM_OPTS \
        -netdev user,id=net0 -device virtio-net-pci,netdev=net0
else
    rm -f "$SERIAL_SOCK"
    echo "Starting Typewrite OS..."
    echo "Resolution: $RESOLUTION"
    echo ""
    echo "Serial console available at: $SERIAL_SOCK"
    echo "Connect from another terminal with: socat - UNIX-CONNECT:$SERIAL_SOCK"
    echo "Or: minicom -D \"unix#$SERIAL_SOCK\""
    echo ""
exec env SDL_VIDEO_WINDOW_POS=0,0 SDL_VIDEO_WINDOW_SIZE=$RESOLUTION qemu-system-x86_64 \
    -enable-kvm \
    -m 512 \
    -kernel buildroot-2024.02/output/images/bzImage \
    -drive file=buildroot-2024.02/output/images/rootfs.ext2,format=raw,if=virtio \
    -append "console=tty0 console=ttyS0 root=/dev/vda rw video=$RESOLUTION" \
        -serial "unix:$SERIAL_SOCK,server,nowait" \
        $KVM_OPTS \
        -netdev user,id=net0 -device virtio-net-pci,netdev=net0
fi