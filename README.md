# Typewrite OS

A minimalist **typewriter-style** writing environment (Freewrite-inspired), explored as:

1. **Native UEFI firmware app** — `Typewriter.efi` (gnu-efi), **actively developed** under [`uefi-app/`](uefi-app/).
2. **Linux + Buildroot** — minimal distro packaging and board support under [`buildroot-2024.02/`](buildroot-2024.02/); the standalone **`typewrite/` source tree is not checked in here** (see [`AGENTS.md`](AGENTS.md)).

## Where to look first

- **[`AGENTS.md`](AGENTS.md)** — Return-to-context summary for you and coding agents (tracks, build commands, open issues).
- **[`BUILD_SYSTEM.md`](BUILD_SYSTEM.md)** — Valid PE32+ EFI build (`objcopy`, linker flags), QEMU.
- **[`GRAPHICS_DEBUG.md`](GRAPHICS_DEBUG.md)** — GOP / framebuffer status on QEMU vs real hardware.
- **[`uefi-app/README.md`](uefi-app/README.md)** — UEFI app overview and diagrams.

## Build the UEFI app

```bash
cd uefi-app
make
```

Output: **`Typewriter.efi`**. Adjust `EFIDIR` in `uefi-app/Makefile` if gnu-efi lives elsewhere on your machine.

## Run in QEMU

From the **repo root** (install `qemu-system-x86` and `ovmf` on Debian/Ubuntu):

```bash
./start-qemu.sh
```

This runs **`make -C uefi-app`**, copies **`Typewriter.efi`** into **`uefi-app/fs/`** (QEMU’s FAT folder), then starts QEMU with OVMF (default **`q35`** + **`gtk,gl=off`**). Options: **`./start-qemu.sh --help`**. If the QEMU window looks stuck or black, try **`./start-qemu.sh --sdl`** or **`./start-qemu.sh --serial-stdio`**. Full troubleshooting (display hang, black window): [`BUILD_SYSTEM.md`](BUILD_SYSTEM.md) → *Testing* → *QEMU window hangs…*.

## License / upstream

Buildroot is upstream software in `buildroot-2024.02/`. Project-specific licensing, if any, is not summarized here; check individual files or future `LICENSE` as applicable.
