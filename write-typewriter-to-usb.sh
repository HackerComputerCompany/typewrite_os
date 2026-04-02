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

  Builds Typewriter, UefiVi, and BootMenu (unless --no-build), optionally tic80-uefi
  when TIC-80 UEFI libs exist, syncs uefi-app/fs/Typewriter.efi, then runs
  install-uefi-app.sh (menu + Typewriter.efi + UefiVi.efi + TIC80.efi if built).

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

TIC80_CORE="$ROOT/../TIC-80/build-uefi-smoke/lib/libtic80core.a"

if [[ "$NO_BUILD" -eq 0 ]]; then
    echo "Building uefi-app, uefi-vi, uefi-menu..."
    make -C "$ROOT/uefi-app" all
    make -C "$ROOT/uefi-vi" all
    make -C "$ROOT/uefi-menu" all
    if [[ -f "$TIC80_CORE" ]]; then
        echo "Building tic80-uefi (TIC-80 core found)..."
        make -C "$ROOT/tic80-uefi" all
    else
        echo "Note: skip tic80-uefi — no $TIC80_CORE (optional)." >&2
    fi
fi

if [[ ! -f "$ROOT/uefi-app/Typewriter.efi" ]]; then
    echo "Error: missing $ROOT/uefi-app/Typewriter.efi" >&2
    exit 1
fi
if [[ ! -f "$ROOT/uefi-vi/UefiVi.efi" ]]; then
    echo "Error: missing $ROOT/uefi-vi/UefiVi.efi" >&2
    exit 1
fi
if [[ ! -f "$ROOT/uefi-menu/BootMenu.efi" ]]; then
    echo "Error: missing $ROOT/uefi-menu/BootMenu.efi" >&2
    exit 1
fi

echo "Syncing Typewriter.efi -> uefi-app/fs/"
cp -f "$ROOT/uefi-app/Typewriter.efi" "$ROOT/uefi-app/fs/Typewriter.efi"

exec "$ROOT/install-uefi-app.sh" "${YES_ARGS[@]}" "$DEVICE"
