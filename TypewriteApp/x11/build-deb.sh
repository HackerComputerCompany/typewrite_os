#!/bin/sh
# Build x11typewrite_*.deb (install with: sudo apt install ./x11typewrite_*_amd64.deb).
# Requires: build-essential debhelper libcairo2-dev libx11-dev pkg-config gnu-efi
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
cd "$ROOT"
dpkg-buildpackage -us -uc -b "$@"
echo "Built: $(ls -1 "$ROOT"/../x11typewrite_*_amd64.deb 2>/dev/null | tail -1)"
