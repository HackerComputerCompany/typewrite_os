#!/bin/bash
# Run latest Typewriter.efi in QEMU + OVMF. See ./start-qemu.sh --help and BUILD_SYSTEM.md.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
UEFI_DIR="${SCRIPT_DIR}/uefi-app"
EFI_BUILT="${UEFI_DIR}/Typewriter.efi"
EFI_IN_FS="${UEFI_DIR}/fs/Typewriter.efi"
STARTUP_NSH="${UEFI_DIR}/fs/startup.nsh"
FAT_IMG="${UEFI_DIR}/twfs.img"
FAT_IMG_MB="${FAT_IMG_MB:-64}"
OVMF_VARS_LOCAL="${SCRIPT_DIR}/ovmf_vars.fd"
SERIAL_LOG="${UEFI_DIR}/serial.log"

DO_BUILD=1
FRESH_VARS=0
FRESH_FS=0
SERIAL_STDIO=0

usage() {
    cat <<'EOF'
NAME
    start-qemu.sh — build Typewrite UEFI apps and run them under QEMU + OVMF.

SYNOPSIS
    ./start-qemu.sh [OPTION]...

DESCRIPTION
    Runs from the repository root. By default:
      1. make -C uefi-app, uefi-vi, uefi-menu   (compile only for uefi-app unless you use its ship target)
      2. Stage uefi-app/fs/EFI/BOOT/bootx64.efi (BootMenu), Typewriter.efi, UefiVi.efi,
         TIC80.efi when built (+ root Typewriter.efi copy, startup.nsh → efi\\boot\\bootx64.efi)
      3. Build/update writable FAT image (uefi-app/twfs.img)
      4. Launches qemu-system-x86_64 with OVMF pflash + FAT + serial log

    Firmware: read-only code from the distro (or OVMF_CODE); writable NVRAM
    in ./ovmf_vars.fd (created from OVMF_VARS.fd template when missing).

OPTIONS
    --no-build       Skip make; need existing Typewriter.efi, UefiVi.efi, BootMenu.efi
                     (TIC80.efi optional — copied if tic80-uefi/TIC80.efi exists).
    --fresh-vars     Delete ./ovmf_vars.fd and recreate from the template (reset
                     NVRAM / boot entries).
    --fresh-fs       Recreate uefi-app/twfs.img from scratch (wipes saved pages/settings).
    --serial-stdio   Send guest COM1 to this terminal (firmware + Print output).
                     Does not write uefi-app/serial.log (use shell redirection if needed).
    --sdl            Use SDL instead of GTK (often works when the GTK window hangs
                     or stays black during “display init”).
    -h, --help       Print this help and exit (exit status 0).

ENVIRONMENT
    OVMF_CODE       Absolute path to OVMF_CODE.fd. If unset, the script tries:
                    /usr/share/OVMF/OVMF_CODE.fd
                    /usr/share/qemu/OVMF_CODE.fd
                    /usr/share/ovmf/OVMF.fd
    QEMU_DISPLAY    Passed to qemu -display. Default is gtk with OpenGL off
                    (gtk,gl=off) to avoid common host hangs/black screens.
                    Examples: sdl, none, gtk, gtk,gl=on
    QEMU_GTK_GL     Set to 1 to use OpenGL with GTK (default is off).
    QEMU_MACHINE    Full -machine string (default: q35,accel=…). Set empty to use
                    QEMU’s default PC: QEMU_MACHINE= ./start-qemu.sh
    QEMU_ACCEL      Accelerator when using the default machine: kvm:tcg (KVM if
                    /dev/kvm is usable), tcg, etc. Overrides auto-detection.
    KEEP_NVRAM      Set to 1 to prevent auto-reinitializing ./ovmf_vars.fd when
                    its size doesn't match the system OVMF_VARS.fd template.

FILES (paths relative to repo root)
    uefi-app/Typewriter.efi   Graphical app (make output).
    uefi-vi/UefiVi.efi       Console editor.
    uefi-menu/BootMenu.efi   Text boot menu (includes TIC-80 when TIC80.efi is staged).
    tic80-uefi/TIC80.efi     Optional; built if ../TIC-80/build-uefi-smoke/lib/libtic80core.a exists.
    uefi-app/fs/            Staging for twfs.img (EFI/BOOT/*.efi + startup.nsh + tests).
    uefi-app/twfs.img          Writable FAT image used by QEMU (real disk, persistent).
    ovmf_vars.fd              Writable UEFI variable store (git may track or ignore).
    uefi-app/serial.log       Serial port output from the guest.

REQUIREMENTS
    Debian/Ubuntu:  sudo apt install qemu-system-x86 ovmf build-essential
    gnu-efi path:  set in uefi-app/Makefile (EFIDIR) for the UEFI build.

EXAMPLES
    ./start-qemu.sh
    ./start-qemu.sh --sdl
    ./start-qemu.sh --serial-stdio
    ./start-qemu.sh --no-build --fresh-vars
    ./start-qemu.sh --fresh-fs
    QEMU_DISPLAY=none ./start-qemu.sh
    OVMF_CODE=/usr/share/OVMF/OVMF_CODE.fd ./start-qemu.sh

SEE ALSO
    BUILD_SYSTEM.md (QEMU details), uefi-app/README.md, AGENTS.md
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-build) DO_BUILD=0; shift ;;
        --fresh-vars) FRESH_VARS=1; shift ;;
        --fresh-fs) FRESH_FS=1; shift ;;
        --serial-stdio) SERIAL_STDIO=1; shift ;;
        --sdl) QEMU_DISPLAY=sdl; shift ;;
        -h|--help) usage; exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

resolve_ovmf_code() {
    if [[ -n "${OVMF_CODE:-}" && -f "${OVMF_CODE}" ]]; then
        echo "$OVMF_CODE"
        return 0
    fi
    local candidates=(
        /usr/share/OVMF/OVMF_CODE.fd
        /usr/share/qemu/OVMF_CODE.fd
        /usr/share/ovmf/OVMF.fd
    )
    local p
    for p in "${candidates[@]}"; do
        if [[ -f "$p" ]]; then
            echo "$p"
            return 0
        fi
    done
    return 1
}

init_ovmf_vars() {
    local code_path="$1"
    if [[ "$FRESH_VARS" -eq 1 ]]; then
        rm -f "$OVMF_VARS_LOCAL"
    fi
    local template=""
    for template in /usr/share/OVMF/OVMF_VARS.fd /usr/share/qemu/OVMF_VARS.fd; do
        if [[ -f "$template" ]]; then
            # If we already have a vars file but it's a different size than the
            # distro template, it's often from a different OVMF layout (e.g. a
            # combined OVMF.fd copy). That mismatch can crash OVMF very early.
            if [[ -f "$OVMF_VARS_LOCAL" && "${KEEP_NVRAM:-0}" != "1" ]]; then
                local cur_sz tmpl_sz
                cur_sz="$(stat -c '%s' "$OVMF_VARS_LOCAL" 2>/dev/null || echo 0)"
                tmpl_sz="$(stat -c '%s' "$template" 2>/dev/null || echo 0)"
                if [[ "$cur_sz" != "$tmpl_sz" ]]; then
                    echo "NVRAM size mismatch ($cur_sz != $tmpl_sz). Re-initializing from $template" >&2
                    rm -f "$OVMF_VARS_LOCAL"
                fi
            fi
            if [[ ! -f "$OVMF_VARS_LOCAL" ]]; then
                echo "Initializing NVRAM from $template"
                cp "$template" "$OVMF_VARS_LOCAL"
            fi
            return 0
        fi
    done
    if [[ -f "$OVMF_VARS_LOCAL" ]]; then
        return 0
    fi
    echo "Initializing NVRAM from $code_path (combined OVMF image)"
    cp "$code_path" "$OVMF_VARS_LOCAL"
}

have_cmd() { command -v "$1" >/dev/null 2>&1; }

sync_fat_image_mtools() {
    # Requires mtools (mcopy, mmd). No root needed.
    local img="$1"
    local src="$2"

    # Recreate filesystem each time: simplest + avoids weird metadata deltas.
    dd if=/dev/zero of="$img" bs=1M count="$FAT_IMG_MB" status=none
    mkfs.fat -F 32 -n TYPEWRITE "$img" >/dev/null

    # Create basic dirs and copy everything under staging root.
    mmd -i "$img" ::/EFI >/dev/null 2>&1 || true
    # Recursive copy, preserve directory structure.
    mcopy -i "$img" -s -n "$src"/* ::/ >/dev/null 2>&1 || true
}

sync_fat_image_loopmount() {
    # Fallback when mtools is unavailable. Needs mount permissions (root/sudo).
    local img="$1"
    local src="$2"
    local mnt

    mnt="$(mktemp -d)"
    trap 'sudo -n umount "$mnt" >/dev/null 2>&1 || true; rmdir "$mnt" >/dev/null 2>&1 || true' RETURN

    dd if=/dev/zero of="$img" bs=1M count="$FAT_IMG_MB" status=none
    mkfs.fat -F 32 -n TYPEWRITE "$img" >/dev/null

    if ! sudo -n true 2>/dev/null; then
        echo "Error: need passwordless sudo for loop-mount, or install mtools." >&2
        echo "Try: sudo apt install mtools" >&2
        exit 1
    fi

    sudo -n mount -o loop,uid="$(id -u)",gid="$(id -g)" "$img" "$mnt" >/dev/null
    cp -a "$src"/. "$mnt"/
    sync
    sudo -n umount "$mnt" >/dev/null
    rmdir "$mnt" >/dev/null 2>&1 || true
    trap - RETURN
}

ensure_fat_image() {
    local src="${UEFI_DIR}/fs"
    local tmp=""

    if [[ "$FRESH_FS" -eq 1 ]]; then
        rm -f "$FAT_IMG"
    fi

    if [[ ! -d "$src" ]]; then
        echo "Error: missing staging folder $src" >&2
        exit 1
    fi

    # FAT is case-insensitive. If staging has both EFI/BOOT and EFI/boot, copy will fail.
    # Prefer EFI/BOOT and drop EFI/boot in the image.
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp" >/dev/null 2>&1 || true' RETURN
    cp -a "$src"/. "$tmp"/
    if [[ -d "$tmp/EFI/BOOT" && -d "$tmp/EFI/boot" ]]; then
        rm -rf "$tmp/EFI/boot"
    fi

    # Always (re)sync to make the image match staging contents.
    if have_cmd mcopy && have_cmd mmd; then
        sync_fat_image_mtools "$FAT_IMG" "$tmp"
        return 0
    fi

    if have_cmd sudo; then
        echo "Note: mtools not found; using sudo loop-mount to build FAT image." >&2
        sync_fat_image_loopmount "$FAT_IMG" "$tmp"
        return 0
    fi

    echo "Error: need either mtools (mcopy/mmd) or sudo for loop-mount image sync." >&2
    echo "Install mtools: sudo apt install mtools" >&2
    exit 1
}

if [[ ! -d "$UEFI_DIR" ]]; then
    echo "Error: missing $UEFI_DIR" >&2
    exit 1
fi

TIC80_CORE="${SCRIPT_DIR}/../TIC-80/build-uefi-smoke/lib/libtic80core.a"
TIC80_BUILT="${SCRIPT_DIR}/tic80-uefi/TIC80.efi"

if [[ "$DO_BUILD" -eq 1 ]]; then
    echo "Building uefi-app, uefi-vi, uefi-menu..."
    make -C "$UEFI_DIR" all
    make -C "${SCRIPT_DIR}/uefi-vi" all
    make -C "${SCRIPT_DIR}/uefi-menu" all
    if [[ -f "$TIC80_CORE" ]]; then
        echo "Building tic80-uefi (TIC-80 core found)..."
        make -C "${SCRIPT_DIR}/tic80-uefi" all
    else
        echo "Note: skip tic80-uefi — no TIC80 core at $TIC80_CORE (menu item 3 needs TIC80.efi)." >&2
    fi
else
    echo "Skipping build (--no-build)."
fi

if [[ ! -f "$EFI_BUILT" ]]; then
    echo "Error: built EFI not found at $EFI_BUILT (run without --no-build or run make -C uefi-app)" >&2
    exit 1
fi

MENU_BUILT="${SCRIPT_DIR}/uefi-menu/BootMenu.efi"
VI_BUILT="${SCRIPT_DIR}/uefi-vi/UefiVi.efi"
if [[ ! -f "$MENU_BUILT" || ! -f "$VI_BUILT" ]]; then
    echo "Error: need $MENU_BUILT and $VI_BUILT (run without --no-build or: make -C uefi-menu all && make -C uefi-vi all)" >&2
    exit 1
fi

mkdir -p "${UEFI_DIR}/fs/EFI/BOOT"
cp -f "$MENU_BUILT" "${UEFI_DIR}/fs/EFI/BOOT/bootx64.efi"
cp -f "$EFI_BUILT" "${UEFI_DIR}/fs/EFI/BOOT/Typewriter.efi"
cp -f "$VI_BUILT" "${UEFI_DIR}/fs/EFI/BOOT/UefiVi.efi"
if [[ -f "$TIC80_BUILT" ]]; then
    cp -f "$TIC80_BUILT" "${UEFI_DIR}/fs/EFI/BOOT/TIC80.efi"
else
    rm -f "${UEFI_DIR}/fs/EFI/BOOT/TIC80.efi"
fi
mkdir -p "${UEFI_DIR}/fs"
cp -f "$EFI_BUILT" "$EFI_IN_FS"
printf '%s\r\n' 'efi\boot\bootx64.efi' >"$STARTUP_NSH"
if [[ -f "${UEFI_DIR}/fs/EFI/BOOT/TIC80.efi" ]]; then
    echo "Synced FAT staging: BootMenu, Typewriter.efi, UefiVi.efi, TIC80.efi; startup.nsh → menu"
else
    echo "Synced FAT staging: BootMenu, Typewriter.efi, UefiVi.efi (no TIC80.efi — build tic80-uefi for menu item 3); startup.nsh → menu"
fi

ensure_fat_image

if ! OVMF_CODE_PATH="$(resolve_ovmf_code)"; then
    echo "Error: OVMF firmware not found. Install ovmf (Debian/Ubuntu) or set OVMF_CODE." >&2
    echo "Tried: /usr/share/OVMF/OVMF_CODE.fd, /usr/share/qemu/OVMF_CODE.fd, /usr/share/ovmf/OVMF.fd" >&2
    exit 1
fi

init_ovmf_vars "$OVMF_CODE_PATH"

# Default -machine uses KVM only when /dev/kvm is readable+writable (avoids qemu
# "Could not access KVM kernel module: Permission denied" when user not in kvm group).
resolve_default_accel() {
    if [[ -n "${QEMU_ACCEL:-}" ]]; then
        printf '%s' "$QEMU_ACCEL"
        return
    fi
    if [[ -r /dev/kvm && -w /dev/kvm ]]; then
        printf '%s' 'kvm:tcg'
    else
        printf '%s' 'tcg'
    fi
}

QEMU_DISPLAY="${QEMU_DISPLAY:-gtk}"
# GTK + OpenGL often black-screens or stalls on some hosts (Wayland/Mesa); default gl=off.
if [[ "$QEMU_DISPLAY" == "gtk" && "${QEMU_GTK_GL:-0}" != "1" ]]; then
    QEMU_DISPLAY="gtk,gl=off"
fi

MACHINE_ARGS=()
if [[ ! -v QEMU_MACHINE ]]; then
    _accel="$(resolve_default_accel)"
    MACHINE_ARGS=(-machine "q35,accel=$_accel")
    if [[ "$_accel" == "tcg" && -z "${QEMU_ACCEL:-}" ]]; then
        echo "Note: KVM not available (/dev/kvm not usable by this user). Using TCG (slower)." >&2
        echo "      Enable KVM: sudo usermod -aG kvm \"$USER\"  then log out and back in." >&2
    fi
elif [[ -n "$QEMU_MACHINE" ]]; then
    MACHINE_ARGS=(-machine "$QEMU_MACHINE")
fi

SERIAL_ARGS=(-serial file:"$SERIAL_LOG")
if [[ "$SERIAL_STDIO" -eq 1 ]]; then
    SERIAL_ARGS=(-serial stdio)
fi

echo "Starting QEMU with UEFI..."
echo "  OVMF code:  $OVMF_CODE_PATH"
echo "  OVMF vars:  $OVMF_VARS_LOCAL"
echo "  FAT image:  $FAT_IMG"
if [[ "$SERIAL_STDIO" -eq 1 ]]; then
    echo "  Serial:     stdio (this terminal — firmware and Print output)"
else
    echo "  Serial log: $SERIAL_LOG"
fi
echo "  Display:    $QEMU_DISPLAY"
if [[ "$QEMU_DISPLAY" == gtk* ]]; then
    echo "  Grab:       Ctrl+Alt+G / Ctrl+Alt to release mouse (GTK)"
fi
if [[ -n "${MACHINE_ARGS[*]}" ]]; then
    echo "  Machine:    ${MACHINE_ARGS[1]}"
else
    echo "  Machine:    (QEMU default — set QEMU_MACHINE=q35,accel=kvm:tcg or tcg if unsure)"
fi
echo ""
if [[ "$QEMU_DISPLAY" != "none" && "$QEMU_DISPLAY" != curses ]]; then
    echo "Tip: If the window stays blank or seems stuck on “display” init, try:" >&2
    echo "     ./start-qemu.sh --sdl     or     ./start-qemu.sh --serial-stdio" >&2
    echo "     (OVMF can take a few seconds before the first frame; that is normal.)" >&2
    echo "" >&2
fi

exec qemu-system-x86_64 \
    "${MACHINE_ARGS[@]}" \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE_PATH" \
    -drive if=pflash,format=raw,file="$OVMF_VARS_LOCAL" \
    -drive if=ide,format=raw,file="$FAT_IMG" \
    -m 256M \
    -net none \
    -display "$QEMU_DISPLAY" \
    "${SERIAL_ARGS[@]}"
