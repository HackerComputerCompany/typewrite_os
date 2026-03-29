#!/usr/bin/env bash
# Build Typewriter.efi (optional) and write it to a USB stick as a bootable UEFI app.
# Wraps install-uefi-app.sh (GPT + FAT32 + bootx64.efi).
#
# Build needs a built gnu-efi tree; uefi-app/Makefile defaults EFIDIR to ../gnu-efi
# (sibling of this repo). Override: export EFIDIR=/path/to/gnu-efi
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR"

usage() {
    cat <<EOF
Usage: $(basename "$0") [options] /dev/sdX | /dev/nvme0n1

  Builds the UEFI app (unless --no-build), syncs uefi-app/fs/Typewriter.efi,
  then runs install-uefi-app.sh to wipe the target disk, create GPT + FAT32 ESP,
  and install Typewriter.efi as efi/boot/bootx64.efi.

Options:
  --no-build   Skip "make -C uefi-app all" (use existing uefi-app/Typewriter.efi)
  --yes, -y    Pass through: skip "Press Enter" confirmation (destructive!)
  -h, --help   This help

Examples:
  $(basename "$0") /dev/sdb
  $(basename "$0") --yes /dev/nvme0n1
  $(basename "$0") --no-build /dev/sdc

WARNING: The target disk is fully erased. Use lsblk(8) to pick the correct device.

Build: clone/build gnu-efi, or set EFIDIR if it is not beside this repo (see uefi-app/Makefile).
EOF
}

NO_BUILD=0
YES_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-build) NO_BUILD=1; shift ;;
        --yes|-y)   YES_ARGS+=(--yes); shift ;;
        -h|--help)  usage; exit 0 ;;
        -*)         echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
        *)          break ;;
    esac
done

if [[ $# -lt 1 ]]; then
    usage >&2
    exit 1
fi

DEVICE="$1"

if [[ "$NO_BUILD" -eq 0 ]]; then
    echo "Building uefi-app (make all)..."
    make -C "$ROOT/uefi-app" all
fi

if [[ ! -f "$ROOT/uefi-app/Typewriter.efi" ]]; then
    echo "Error: missing $ROOT/uefi-app/Typewriter.efi" >&2
    exit 1
fi

echo "Syncing Typewriter.efi -> uefi-app/fs/"
cp -f "$ROOT/uefi-app/Typewriter.efi" "$ROOT/uefi-app/fs/Typewriter.efi"

exec "$ROOT/install-uefi-app.sh" "${YES_ARGS[@]}" "$DEVICE"
