#!/bin/bash
# Minimal setup for running QEMU with UEFI
# Run this on the host where you want to test the EFI app

set -e

echo "=== Installing QEMU and OVMF ==="
sudo apt-get update
sudo apt-get install -y ovmf qemu-system-x86

echo "=== Setup complete! ==="
echo "Run ./start-qemu.sh to test"
