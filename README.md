# Typewrite OS

A minimalist **typewriter-style** writing environment (Freewrite-inspired), explored as:

1. **Native UEFI firmware app** — `Typewriter.efi` (gnu-efi), **actively developed** under [`uefi-app/`](uefi-app/).
2. **Linux + Buildroot** — minimal distro packaging and board support under [`buildroot-2024.02/`](buildroot-2024.02/); the standalone **`typewrite/` source tree is not checked in here** (see [`AGENTS.md`](AGENTS.md)).

## Where to look first

- **[`MILESTONE.md`](MILESTONE.md)** — **Current milestone:** beta-ready UEFI typewriter (what works, how to run, doc index).
- **[`fonts/README.md`](fonts/README.md)** — Typefaces bundled for the UEFI app (**F2** cycles nine), licenses, how to regenerate bitmap headers.
- **[`AGENTS.md`](AGENTS.md)** — Return-to-context summary for you and coding agents (tracks, build commands, open issues).
- **[`BUILD_SYSTEM.md`](BUILD_SYSTEM.md)** — Valid PE32+ EFI build (`objcopy`, linker flags), QEMU.
- **[`GRAPHICS_DEBUG.md`](GRAPHICS_DEBUG.md)** — GOP / framebuffer status on QEMU vs real hardware.
- **[`uefi-app/README.md`](uefi-app/README.md)** — UEFI app overview and diagrams.

## Build the UEFI app

```bash
cd uefi-app
make
```

Output: **`Typewriter.efi`**; default **`make`** also syncs **`fs/Typewriter.efi`** and **commits + pushes** when the tree changes (see [`uefi-app/README.md`](uefi-app/README.md)). Use **`make all`** for compile only. **`EFIDIR`** defaults to **`../gnu-efi`** (sibling of this repo); override with `export EFIDIR=...` or `make EFIDIR=... all` if needed.

## Run in QEMU

From the **repo root** (install `qemu-system-x86` and `ovmf` on Debian/Ubuntu):

```bash
./start-qemu.sh
```

This runs **`make -C uefi-app all`** (compile only), copies **`Typewriter.efi`** into **`uefi-app/fs/`** (QEMU’s FAT folder), then starts QEMU with OVMF (default **`q35`**, **KVM** if **`/dev/kvm`** is usable, else **TCG**; display default **`gtk,gl=off`**). Options: **`./start-qemu.sh --help`**. If the window looks stuck or black, try **`./start-qemu.sh --sdl`** or **`./start-qemu.sh --serial-stdio`**. To enable KVM (faster than TCG): add yourself to the **`kvm`** group — see [`BUILD_SYSTEM.md`](BUILD_SYSTEM.md) (*Testing*: KVM vs TCG and `usermod`). More troubleshooting in the same doc.

## License / upstream

Buildroot is upstream software in `buildroot-2024.02/`. Project-specific licensing, if any, is not summarized here; check individual files or future `LICENSE` as applicable.
