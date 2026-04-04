# Patching / iterative builds

The **vendored Buildroot tree** and **`rootfs.ext2`**-based workflows described in older notes are **no longer in this repository**.

## UEFI application

- Edit sources under **`uefi-app/`** (and shared fonts under **`fonts/`** as needed).
- Rebuild: **`make -C uefi-app`** (or **`make all`** for compile-only without the default ship step — see **`uefi-app/README.md`**).
- QEMU: **`./start-qemu.sh`** picks up the new **`Typewriter.efi`** in **`uefi-app/fs/`**.

## Linux X11 application

- Core document logic: **`linux-typewrite/src/`** (`tw_core`, `tw_doc`).
- X11 front end: **`linux-typewrite-x11/`** — **`make -C linux-typewrite-x11`** after changes.

## Debian package

After code changes, bump **`debian/changelog`** if you ship a new **`x11typewrite`** version, then rebuild with **`dpkg-buildpackage -us -uc -b`** (see **`linux-typewrite-x11/README.md`**).
