# BootMenu.efi

Text-console **boot menu** that loads either **`Typewriter.efi`** (GOP) or **`UefiVi.efi`** (console) from the same ESP using `LoadImage` / `StartImage`.

## Layout (same as `install-uefi-app.sh`)

All on the boot FAT volume:

| Path | Role |
|------|------|
| `efi/boot/bootx64.efi` | This menu (firmware default entry) |
| `efi/boot/Typewriter.efi` | Graphical app |
| `efi/boot/UefiVi.efi` | Console editor |

Paths use **backslashes** in the device path (`\efi\boot\...`); FAT is case-insensitive.

## Build

```bash
cd uefi-menu
make
```

Requires sibling **`../gnu-efi`** (or `EFIDIR=...`).

## Override paths (compile-time)

```bash
make CFLAGS='-DMENU_PATH_TYPEWRITER=L"\\foo\\bar.efi" -DMENU_PATH_UEVIVI=L"\\foo\\vi.efi" ...'
```

(Keep full gnu-efi `CFLAGS` from the Makefile — easiest to edit the `#define`s in `main.c`.)

## Behaviour

After a child exits, the menu **redisplays** so you can switch apps. Option **3** returns to the UEFI Shell (or ends the boot app) so firmware can continue.

## See also

- [`../uefi-app/`](../uefi-app/) — Typewriter
- [`../uefi-vi/`](../uefi-vi/) — UefiVi
- [`../install-uefi-app.sh`](../install-uefi-app.sh) — USB layout
