#!/bin/bash
# Run latest Typewriter.efi in QEMU + OVMF. See ./start-qemu.sh --help and BUILD_SYSTEM.md.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
UEFI_DIR="${SCRIPT_DIR}/uefi-app"
EFI_BUILT="${UEFI_DIR}/Typewriter.efi"
EFI_IN_FS="${UEFI_DIR}/fs/Typewriter.efi"
STARTUP_NSH="${UEFI_DIR}/fs/startup.nsh"
OVMF_VARS_LOCAL="${SCRIPT_DIR}/ovmf_vars.fd"
SERIAL_LOG="${UEFI_DIR}/serial.log"

DO_BUILD=1
FRESH_VARS=0
SERIAL_STDIO=0

usage() {
    cat <<'EOF'
NAME
    start-qemu.sh — build Typewriter.efi and run it under QEMU with OVMF (UEFI).

SYNOPSIS
    ./start-qemu.sh [OPTION]...

DESCRIPTION
    Runs from the repository root. By default:
      1. make -C uefi-app          (produces uefi-app/Typewriter.efi)
      2. cp Typewriter.efi         → uefi-app/fs/ (QEMU synthetic FAT drive)
      3. Ensures uefi-app/fs/startup.nsh runs Typewriter.efi in the UEFI Shell
      4. Launches qemu-system-x86_64 with OVMF pflash + FAT + serial log

    Firmware: read-only code from the distro (or OVMF_CODE); writable NVRAM
    in ./ovmf_vars.fd (created from OVMF_VARS.fd template when missing).

OPTIONS
    --no-build       Skip make; use existing uefi-app/Typewriter.efi (must exist).
    --fresh-vars     Delete ./ovmf_vars.fd and recreate from the template (reset
                     NVRAM / boot entries).
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

FILES (paths relative to repo root)
    uefi-app/Typewriter.efi   Built binary (make output).
    uefi-app/fs/              FAT contents for QEMU (Typewriter.efi + startup.nsh).
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
    if [[ -f "$OVMF_VARS_LOCAL" ]]; then
        return 0
    fi
    local template=""
    for template in /usr/share/OVMF/OVMF_VARS.fd /usr/share/qemu/OVMF_VARS.fd; do
        if [[ -f "$template" ]]; then
            echo "Initializing NVRAM from $template"
            cp "$template" "$OVMF_VARS_LOCAL"
            return 0
        fi
    done
    echo "Initializing NVRAM from $code_path (combined OVMF image)"
    cp "$code_path" "$OVMF_VARS_LOCAL"
}

if [[ ! -d "$UEFI_DIR" ]]; then
    echo "Error: missing $UEFI_DIR" >&2
    exit 1
fi

if [[ "$DO_BUILD" -eq 1 ]]; then
    echo "Building UEFI app (make -C uefi-app)..."
    make -C "$UEFI_DIR"
else
    echo "Skipping build (--no-build)."
fi

if [[ ! -f "$EFI_BUILT" ]]; then
    echo "Error: built EFI not found at $EFI_BUILT (run without --no-build or run make -C uefi-app)" >&2
    exit 1
fi

mkdir -p "${UEFI_DIR}/fs"
cp -f "$EFI_BUILT" "$EFI_IN_FS"
echo "Synced $(basename "$EFI_BUILT") -> $EFI_IN_FS"

if [[ ! -f "$STARTUP_NSH" ]]; then
    echo "Typewriter.efi" >"$STARTUP_NSH"
    echo "Created $STARTUP_NSH (auto-runs Typewriter.efi in UEFI Shell)"
fi

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
echo "  FAT folder: ${UEFI_DIR}/fs"
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
    -drive format=raw,file=fat:rw:"${UEFI_DIR}/fs" \
    -m 256M \
    -net none \
    -display "$QEMU_DISPLAY" \
    "${SERIAL_ARGS[@]}"
