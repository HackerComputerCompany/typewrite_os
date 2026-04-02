# UefiVi — console vi-like editor for UEFI

Small **EFI application** that edits text files on the **boot volume** using the firmware **text console** only (no GOP / framebuffer). Complements **`uefi-app/Typewriter.efi`**.

## Build

```bash
cd uefi-vi
make          # needs sibling ../gnu-efi (or EFIDIR=…)
```

Output: **`UefiVi.efi`** (valid PE32+ per `BUILD_SYSTEM.md`).

## Usage

Copy **`UefiVi.efi`** to the ESP (e.g. next to `Typewriter.efi`). From **UEFI Shell**:

```text
fs0:
UefiVi.efi
```

Opens **`EDIT.TXT`** on that volume (create empty if missing).

With a path (**load options** after the program name):

```text
UefiVi.efi Typewriter.txt
```

The first argument is the file name (same FAT volume as the `.efi`).

## Keys (minimal vi subset)

| Mode | Key | Action |
|------|-----|--------|
| Normal | `h` `j` `k` `l` | Cursor left / down / up / right |
| Normal | `i` | Insert before cursor |
| Normal | `a` | Insert after cursor |
| Normal | `x` | Delete character under cursor |
| Normal | `:` | Command line (`w` write, `q` quit, `wq` both, `q!` discard) |
| Normal | `o` or **F2** | Open **file browser** on the boot volume |
| Normal | `:e` or `:browse` | Same as browser (after typing `:`) |
| Insert | printable | Insert at cursor (line capped) |
| Insert | Backspace | Delete before cursor |
| Insert | Enter | New line below |
| Any | **ESC** | Back to Normal (in browser: parent directory, or exit browser at volume root) |

**File browser:** `j` / `k` or arrow keys move; **Enter** opens a directory or loads a file (`-` or **ESC** in a subdirectory goes to parent). If the buffer is **dirty**, loading another file from the browser is blocked until you `:w` or `:q!`.

Status line shows mode and dirty flag.

## Limits

- Max **128** lines × **240** characters per line (UTF-16 in memory).
- File read/write **UTF-8**; astral / invalid sequences may be skipped.
- No search, undo, or visual line wrap; terminal size from `ConOut->QueryMode`.

## See also

- [`../BUILD_SYSTEM.md`](../BUILD_SYSTEM.md) — PE / objcopy / EFIDIR
- [`../uefi-app/`](../uefi-app/) — graphical typewriter
