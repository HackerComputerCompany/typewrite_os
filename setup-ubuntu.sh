#!/usr/bin/env bash
# Setup script for Ubuntu 22.04 to build Typewrite OS UEFI application
# Run this from anywhere; uses this repo as the working directory.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Installing build dependencies ==="
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    gcc \
    g++ \
    make \
    git \
    uuid-dev \
    iasl \
    nasm \
    python3 \
    acpica-tools \
    ovmf \
    qemu-system-x86

echo "=== Cloning EDK II ==="
if [ ! -d "edk2" ]; then
    git clone https://github.com/tianocore/edk2.git
    cd edk2
    git submodule update --init
else
    cd edk2
fi

echo "=== Building BaseTools ==="
make -C BaseTools

echo "=== Setting up build environment ==="
source ./edksetup.sh

echo "=== Building HelloWorld ==="
build -p HelloWorld.dsc -a X64 -b DEBUG -t GCC

echo "=== Copying EFI to test location ==="
mkdir -p ../uefi-app/fs
cp Build/MdeModule/DEBUG_GCC/X64/apps/HelloWorld/HelloWorld/OUTPUT/HelloWorld.efi ../uefi-app/fs/

echo "=== Setup complete! ==="
echo "Run ./start-qemu.sh to test"
