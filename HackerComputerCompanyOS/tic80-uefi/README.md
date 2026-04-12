# TIC-80 UEFI scaffold

This directory builds **`TIC80.efi`**: a small **gnu-efi** application that proves the boot path (GOP + console). It is the landing zone for linking **TIC-80** static libraries built with **`-DTIC80_UEFI=ON`** from the [TIC-80](https://github.com/nesbox/TIC-80) tree.

## Build

Requires **`../gnu-efi`** (sibling of `typewrite_os` by default), same as `uefi-app/`.

```bash
cd tic80-uefi
make          # → TIC80.efi
```

Copy **`TIC80.efi`** to the ESP (e.g. next to `Typewriter.efi`) or into `uefi-app/fs/` for QEMU per `BUILD_SYSTEM.md`.

## TIC-80 static libraries

From your TIC-80 checkout (e.g. `../projects/TIC-80`):

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel -DTIC80_UEFI=ON
cmake --build . --parallel
```

That configures **no SDL**, **static** libs, and defines **`TIC80_UEFI`** for embedded-oriented code paths. The next step is to extend this Makefile’s `main.so` link line with the generated `lib/*.a` archives and a **C runtime** strategy (malloc, file I/O) — see **`../TIC80_UEFI_PORT.md`** and **`../TIC-80/src/system/uefi/README.md`**.

## Runtime

At startup the scaffold prints GOP geometry, draws a centered rectangle via **`Blt`**, then waits for a key.
