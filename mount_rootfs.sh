#!/bin/bash
MOUNT_DIR="/mnt/rootfs"
IMG_FILE="buildroot-2024.02/output/images/rootfs.ext2"

if [ "$1" = "umount" ] || [ "$1" = "u" ]; then
    echo "Unmounting..."
    sudo umount "$MOUNT_DIR" 2>/dev/null && rmdir "$MOUNT_DIR" 2>/dev/null
    echo "Done"
else
    echo "Mounting rootfs..."
    sudo mkdir -p "$MOUNT_DIR"
    sudo mount -o loop "$IMG_FILE" "$MOUNT_DIR"
    echo "Mounted at $MOUNT_DIR"
    echo ""
    echo "Commands:"
    echo "  sudo vim $MOUNT_DIR/etc/inittab    # Edit inittab"
    echo "  sudo cp new_binary $MOUNT_DIR/usr/bin/   # Copy files"
    echo "  ./mount_rootfs.sh umount           # Unmount when done"
fi
