#!/bin/bash
# Build x11typewrite and pack it with bundled shared libraries into a .tar.gz for
# redistribution. Host libc / dynamic linker stay system-provided (glibc ABI).
#
# On x86_64, also builds x11typewrite-i686 when multilib deps exist, and ships both
# binaries under bin/ with separate lib/x86_64 and lib/i686 trees.
#
# Usage: from repo root OR linux-typewrite-x11/:
#   ./linux-typewrite-x11/make-portable-tarball.sh [--no-build]
#
# Output: linux-typewrite-x11/dist/x11typewrite-portable-<ver>-<tag>.tar.gz
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$SCRIPT_DIR/x11typewrite"
BIN_X64="$SCRIPT_DIR/x11typewrite-x86_64"
BIN_I686="$SCRIPT_DIR/x11typewrite-i686"
DIST="$SCRIPT_DIR/dist"
DO_BUILD=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-build) DO_BUILD=0; shift ;;
        -h|--help)
            echo "Usage: $0 [--no-build]"
            echo "  Builds x11typewrite (unless --no-build), optional i686, bundles .so deps, writes dist/*.tar.gz"
            exit 0
            ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# Version: first line of debian/changelog, e.g. x11typewrite (0.1.0-1)
VERSION="0.1.0"
if [[ -f "$ROOT/debian/changelog" ]]; then
    v=$(grep -m1 '^x11typewrite ' "$ROOT/debian/changelog" | sed -n 's/.*(\([^)]*\)).*/\1/p')
    [[ -n "$v" ]] && VERSION="${v%-*}"
fi

HOST_ARCH=$(uname -m)
[[ "$HOST_ARCH" == "x86_64" || "$HOST_ARCH" == "aarch64" ]] || {
    echo "Unsupported machine: $HOST_ARCH (only x86_64 and aarch64 tested)" >&2
    exit 1
}

if [[ "$DO_BUILD" -eq 1 ]]; then
    make -C "$SCRIPT_DIR" clean all
    if [[ "$HOST_ARCH" == "x86_64" ]]; then
        make -C "$SCRIPT_DIR" i686 || true
    fi
fi

if [[ ! -x "$BIN" ]]; then
    echo "No executable at $BIN (build failed or use without --no-build)" >&2
    exit 1
fi

# Basenames we never bundle (must match the end-user's glibc / loader). Same
# idea as AppImage excludelist — avoids subtle breakage across distros.
declare -a EXCLUDE=(
    "ld-linux-x86-64.so.2"
    "ld-linux.so.2"
    "ld-linux-aarch64.so.1"
    "libc.so.6"
    "libm.so.6"
    "libmvec.so.1"
    "libpthread.so.0"
    "libdl.so.0"
    "libdl.so.2"
    "librt.so.1"
    "libresolv.so.2"
    "libutil.so.1"
    "libanl.so.1"
    "libBrokenLocale.so.1"
    "libnss_compat.so.2"
    "libnss_dns.so.2"
    "libnss_files.so.2"
    "libnss_hesiod.so.2"
    "libthread_db.so.1"
)

is_excluded() {
    local b="$1"
    for e in "${EXCLUDE[@]}"; do
        [[ "$b" == "$e" ]] && return 0
    done
    return 1
}

# Copy resolved DSOs for one ELF into $PKG/$lib_subdir (e.g. lib/x86_64).
bundle_libs_for() {
    local elf="$1"
    local pkg_lib="$2"
    mkdir -p "$pkg_lib"

    declare -A COPIED_REALPATH=()
    while IFS= read -r line; do
        [[ "$line" == *"=>"* ]] || continue
        path=${line#*=> }
        path=${path%% (*}
        path=$(echo "$path" | tr -d '[:space:]')
        [[ "$path" == /* ]] || continue
        [[ -f "$path" || -L "$path" ]] || continue
        real=$(readlink -f "$path" 2>/dev/null || true)
        [[ -n "$real" && -f "$real" ]] || continue
        base=$(basename "$real")
        is_excluded "$base" && continue
        [[ -n ${COPIED_REALPATH[$real]+x} ]] && continue
        COPIED_REALPATH[$real]=1
        install -D -m755 "$real" "$pkg_lib/$base"
    done < <(ldd "$elf" 2>/dev/null)

    while IFS= read -r line; do
        [[ "$line" == *"=>"* ]] || continue
        left=${line%%=>*}
        soname=$(echo "$left" | tr -d '[:space:]')
        path=${line#*=> }
        path=${path%% (*}
        path=$(echo "$path" | tr -d '[:space:]')
        [[ "$path" == /* ]] || continue
        real=$(readlink -f "$path" 2>/dev/null || true)
        [[ -n "$real" ]] || continue
        base=$(basename "$real")
        is_excluded "$base" && continue
        [[ -f "$pkg_lib/$base" ]] || continue
        if [[ "$soname" != "$base" && ! -e "$pkg_lib/$soname" ]]; then
            ln -sf "$base" "$pkg_lib/$soname"
        fi
    done < <(ldd "$elf" 2>/dev/null)
}

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

PKG_TAG="$HOST_ARCH"
[[ "$HOST_ARCH" == "x86_64" && -x "$BIN_I686" ]] && PKG_TAG="x86_64+i686"

NAME="x11typewrite-portable-${VERSION}-${PKG_TAG}"
PKG="$STAGE/$NAME"
mkdir -p "$PKG/bin"

HAVE_I686=0
if [[ "$HOST_ARCH" == "x86_64" ]]; then
    X64_SRC="$BIN"
    [[ -x "$BIN_X64" ]] && X64_SRC="$BIN_X64"
    install -D -m755 "$X64_SRC" "$PKG/bin/x11typewrite-x86_64"
    bundle_libs_for "$X64_SRC" "$PKG/lib/x86_64"
    if [[ -x "$BIN_I686" ]]; then
        install -D -m755 "$BIN_I686" "$PKG/bin/x11typewrite-i686"
        bundle_libs_for "$BIN_I686" "$PKG/lib/i686"
        HAVE_I686=1
    fi
elif [[ "$HOST_ARCH" == "aarch64" ]]; then
    install -D -m755 "$BIN" "$PKG/bin/x11typewrite-aarch64"
    bundle_libs_for "$BIN" "$PKG/lib/aarch64"
fi

cat > "$PKG/run-x11typewrite.sh" << 'LAUNCH'
#!/bin/sh
# Portable launcher: pick arch-matched binary and bundled libs.
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
M=$(uname -m)
case "$M" in
    x86_64|amd64)
        B="$DIR/bin/x11typewrite-x86_64"
        L="$DIR/lib/x86_64"
        ;;
    i686|i386|x86)
        B="$DIR/bin/x11typewrite-i686"
        L="$DIR/lib/i686"
        ;;
    aarch64)
        B="$DIR/bin/x11typewrite-aarch64"
        L="$DIR/lib/aarch64"
        ;;
    *)
        echo "x11typewrite portable: unsupported machine: $M" >&2
        exit 1
        ;;
esac
if [ ! -x "$B" ]; then
    echo "x11typewrite portable: missing binary for this CPU: $B" >&2
    exit 1
fi
if [ ! -d "$L" ]; then
    echo "x11typewrite portable: missing library dir: $L" >&2
    exit 1
fi
export LD_LIBRARY_PATH="$L${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$B" "$@"
LAUNCH
chmod 755 "$PKG/run-x11typewrite.sh"

if [[ "$HOST_ARCH" == "x86_64" ]]; then
    I686_LINE="32-bit: bin/x11typewrite-i686 + lib/i686/"
    if [[ "$HAVE_I686" -eq 0 ]]; then
        I686_LINE="32-bit: not included (build failed — install i386 dev libs, see Makefile)"
    fi
    cat > "$PKG/README-PORTABLE.txt" << EOF
x11typewrite portable bundle
Version: ${VERSION}
Host build: x86_64 (includes 64-bit; 32-bit if built)

Run (no install):
  tar xf ${NAME}.tar.gz
  cd ${NAME}
  ./run-x11typewrite.sh

Binaries (same directory):
  bin/x11typewrite-x86_64   — 64-bit x86-64
  bin/x11typewrite-i686    — 32-bit x86 (optional)
${I686_LINE}

Requires: Linux with glibc compatible with the build host. X11 display.
Not bundled: C library, dynamic linker, pthread, math library.

Source: https://github.com/HackerComputerCompany/typewrite_os
EOF
else
    cat > "$PKG/README-PORTABLE.txt" << EOF
x11typewrite portable bundle
Version: ${VERSION}
Architecture: aarch64

Run (no install):
  tar xf ${NAME}.tar.gz
  cd ${NAME}
  ./run-x11typewrite.sh

Requires: 64-bit ARM Linux with glibc compatible with the build host.
Not bundled: C library, dynamic linker, pthread, math library.

Source: https://github.com/HackerComputerCompany/typewrite_os
EOF
fi

mkdir -p "$DIST"
OUT_TGZ="$DIST/${NAME}.tar.gz"
tar -C "$STAGE" -czf "$OUT_TGZ" "$NAME"
echo "Wrote $OUT_TGZ ($(ls -lh "$OUT_TGZ" | awk '{print $5}'))"
