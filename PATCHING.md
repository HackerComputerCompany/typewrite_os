# Patching rootfs.ext2 Manually

This document describes how to patch the rootfs.ext2 image without rebuilding the entire Buildroot system.

## Quick Reference

```bash
# Mount the rootfs
sudo mkdir -p /tmp/rootfs
sudo mount -o loop buildroot-2024.02/output/images/rootfs.ext2 /tmp/rootfs

# Copy updated binary
sudo cp buildroot-2024.02/output/target/usr/bin/typewrite /tmp/rootfs/usr/bin/typewrite

# Unmount
sudo umount /tmp/rootfs
```

## Detailed Steps

### 1. Build the typewrite binary

After making code changes to `typewrite/src/main.c`:

```bash
cd /path/to/typewrite_os
make -C buildroot-2024.02 typewrite-rebuild
```

This compiles the source and installs to `buildroot-2024.02/output/target/usr/bin/typewrite`

### 2. Mount the rootfs image

```bash
sudo mkdir -p /tmp/rootfs
sudo mount -o loop buildroot-2024.02/output/images/rootfs.ext2 /tmp/rootfs
```

The `rootfs.ext2` file is at `buildroot-2024.02/output/images/rootfs.ext2`

### 3. Copy files to the mounted filesystem

```bash
# Copy binary
sudo cp buildroot-2024.02/output/target/usr/bin/typewrite /tmp/rootfs/usr/bin/typewrite

# Or copy any other files (fonts, configs, etc)
# sudo cp some_file /tmp/rootfs/path/to/destination
```

### 4. Unmount the image

```bash
sudo umount /tmp/rootfs
```

The `rootfs.ext2` file is now updated and ready to use with QEMU.

## What Gets Built vs What Needs Patching

### After code changes
- `typewrite/src/main.c` → rebuild with `make typewrite-rebuild` → patch `usr/bin/typewrite`

### After config changes
- Buildroot configs (`.config`) → full rebuild required
- Kernel configs → full rebuild required
- Package selections → `make` to rebuild rootfs

## Full Rootfs Rebuild (if needed)

If you need to completely rebuild the rootfs:

```bash
cd buildroot-2024.02
make
```

This rebuilds everything from scratch but takes much longer (10-30+ minutes depending on changes).

## Checking Installed Files

To see what's in the rootfs:

```bash
sudo mount -o loop buildroot-2024.02/output/images/rootfs.ext2 /tmp/rootfs
ls -la /tmp/rootfs/usr/bin/
ls -la /tmp/rootfs/usr/share/fonts/
sudo umount /tmp/rootfs
```

## Debug Log Location

When running with debug logging enabled, the log file is written to:
- `/tmp/typewrite_debug.log` inside the guest filesystem

To retrieve it after shutdown:

```bash
sudo mount -o loop buildroot-2024.02/output/images/rootfs.ext2 /tmp/rootfs
cat /tmp/rootfs/tmp/typewrite_debug.log
sudo umount /tmp/rootfs
```

Or use QEMU's serial output by adding `-serial stdio` to see messages in real-time.