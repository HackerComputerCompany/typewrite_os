# Installing Typewrite OS

This repository ships **two runnable paths**: a **native UEFI** app (`Typewriter.efi`) and a **Linux X11** desktop client (`x11typewrite`, also packaged as a `.deb`). The former **minimal Linux + Buildroot** image and its USB installers were **removed** from the tree (2026) to shrink the repo; bring your own Buildroot if you need that again.

## UEFI: bootable USB (recommended for hardware)

Prerequisites: **`gnu-efi`** built or available (see [`uefi-app/Makefile`](uefi-app/Makefile) — default **`EFIDIR=../gnu-efi`** next to this repo).

From the **repo root**:

```bash
./write-typewriter-to-usb.sh /dev/sdX    # builds EFI apps, then lays out GPT + FAT ESP
# or, if Typewriter.efi is already built:
sudo ./install-uefi-app.sh /dev/sdX
```

Pick the block device with **`lsblk`**. These scripts **erase** the target disk.

Details: [`uefi-app/README.md`](uefi-app/README.md), [`BUILD_SYSTEM.md`](BUILD_SYSTEM.md).

## UEFI: QEMU (no USB)

```bash
./start-qemu.sh --help
```

Default flow builds `uefi-app`, syncs `Typewriter.efi` into `uefi-app/fs/`, and launches QEMU + OVMF.

## Linux desktop: X11 app

Build on the host:

```bash
make -C linux-typewrite-x11
./linux-typewrite-x11/x11typewrite
```

**Debian/Ubuntu package** (from repo root): see [`linux-typewrite-x11/README.md`](linux-typewrite-x11/README.md) — `build-deb.sh` / `dpkg-buildpackage`, then `sudo apt install ./x11typewrite_*_amd64.deb`.

## Secure Boot

Firmware may refuse unsigned loaders. Disable Secure Boot or enroll your own keys if the stick does not boot.

## Serial debug (QEMU)

```bash
./start-qemu.sh --serial
socat - UNIX-CONNECT:/tmp/typewrite-serial.sock
```
