#!/bin/bash
# Start QEMU with UEFI to test Typewriter

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EFI_APP="${SCRIPT_DIR}/uefi-app/fs/Typewriter.efi"
OVMF_CODE="/usr/share/ovmf/OVMF.fd"
OVMF_VARS="${SCRIPT_DIR}/ovmf_vars.fd"

if [ ! -f "$EFI_APP" ]; then
    echo "Error: EFI app not found at $EFI_APP"
    exit 1
fi

if [ ! -f "$OVMF_CODE" ]; then
    echo "Error: OVMF not found at $OVMF_CODE"
    exit 1
fi

if [ ! -f "$OVMF_VARS" ]; then
    cp "$OVMF_CODE" "$OVMF_VARS"
fi

echo "Starting QEMU with UEFI..."
echo "EFI App: $EFI_APP"
echo "Resolution: Use Ctrl+Alt+G to grab, Ctrl+Alt to release"

qemu-system-x86_64 \
    -bios "$OVMF_CODE" \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$OVMF_VARS" \
    -drive format=raw,file=fat:rw:"${SCRIPT_DIR}/uefi-app/fs" \
    -m 256M \
    -net none \
    -display gtk \
    -serial file:"${SCRIPT_DIR}/uefi-app/serial.log"
