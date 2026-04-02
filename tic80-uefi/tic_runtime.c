/*
 * TIC-80 runtime: load cart (boot volume cart.tic or builtin), 60 Hz tick + sound, GOP Blt.
 * UEFI has no standard audio sink; tic80_sound() still runs so timing and buffers stay valid.
 */
#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <eficon.h>

#include <tic80.h>

#include "tic_runtime.h"

#define INPUT_HOLD_FRAMES 12U
#define MAX_CART_BYTES    (4U * 1024U * 1024U)

static const CHAR16 CartFileName[] = L"cart.tic";

#include "builtin_cart.inc"

static volatile BOOLEAN g_exit_request;

static void on_tic_exit(void) {
    g_exit_request = TRUE;
}

static void on_tic_error(const char *msg) {
    if (msg)
        Print(L"TIC-80: %a\r\n", msg);
}

static UINT64 rdtsc(void) {
    UINT32 lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((UINT64)hi << 32) | (UINT64)lo;
}

static UINT64 tsc_hz_calibrate(void) {
    UINT64 a = rdtsc();
    uefi_call_wrapper(BS->Stall, 1, BS, 100 * 1000);
    UINT64 b = rdtsc();
    if (b > a)
        return (b - a) * 10;
    return 2500000000ULL;
}

static UINT64 g_tsc_hz;
static UINT64 g_next_frame_tsc;

static UINT64 uefi_counter(void *unused) {
    (void)unused;
    return rdtsc();
}

static UINT64 uefi_freq(void *unused) {
    (void)unused;
    return g_tsc_hz;
}

static void wait_next_frame(void) {
    UINT64 now;

    for (;;) {
        now = rdtsc();
        if ((INT64)(now - g_next_frame_tsc) >= 0)
            break;
    }
    g_next_frame_tsc += g_tsc_hz / (UINT64)TIC80_FRAMERATE;
    now = rdtsc();
    if ((INT64)(now - g_next_frame_tsc) > (INT64)(g_tsc_hz / (UINT64)TIC80_FRAMERATE))
        g_next_frame_tsc = now;
}

typedef struct {
    UINTN up, down, left, right, a, b, x, y;
} HoldState;

static void hold_decay(HoldState *h) {
    if (h->up)
        h->up--;
    if (h->down)
        h->down--;
    if (h->left)
        h->left--;
    if (h->right)
        h->right--;
    if (h->a)
        h->a--;
    if (h->b)
        h->b--;
    if (h->x)
        h->x--;
    if (h->y)
        h->y--;
}

static void pump_input(HoldState *h) {
    EFI_INPUT_KEY key;

    while (!EFI_ERROR(uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key))) {
        if (key.ScanCode == SCAN_ESC) {
            g_exit_request = TRUE;
            continue;
        }
        if (key.ScanCode == SCAN_UP)
            h->up = INPUT_HOLD_FRAMES;
        if (key.ScanCode == SCAN_DOWN)
            h->down = INPUT_HOLD_FRAMES;
        if (key.ScanCode == SCAN_LEFT)
            h->left = INPUT_HOLD_FRAMES;
        if (key.ScanCode == SCAN_RIGHT)
            h->right = INPUT_HOLD_FRAMES;
        /* Match SDL player: Z X A S -> gamepad a b x y */
        if (key.UnicodeChar == L'z' || key.UnicodeChar == L'Z')
            h->a = INPUT_HOLD_FRAMES;
        if (key.UnicodeChar == L'x' || key.UnicodeChar == L'X')
            h->b = INPUT_HOLD_FRAMES;
        if (key.UnicodeChar == L'a' || key.UnicodeChar == L'A')
            h->x = INPUT_HOLD_FRAMES;
        if (key.UnicodeChar == L's' || key.UnicodeChar == L'S')
            h->y = INPUT_HOLD_FRAMES;
    }
}

static void holds_to_tic_input(const HoldState *h, tic80_input *in) {
    in->gamepads.data = 0;
    if (h->up)
        in->gamepads.first.up = 1;
    if (h->down)
        in->gamepads.first.down = 1;
    if (h->left)
        in->gamepads.first.left = 1;
    if (h->right)
        in->gamepads.first.right = 1;
    if (h->a)
        in->gamepads.first.a = 1;
    if (h->b)
        in->gamepads.first.b = 1;
    if (h->x)
        in->gamepads.first.x = 1;
    if (h->y)
        in->gamepads.first.y = 1;
}

static EFI_STATUS load_cart_from_boot_volume(EFI_HANDLE image, VOID **out_buf, UINTN *out_size) {
    EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    EFI_FILE *root = NULL;
    EFI_FILE *f = NULL;
    EFI_STATUS st;
    EFI_FILE_INFO *info = NULL;
    VOID *buf = NULL;
    UINTN read_size;

    *out_buf = NULL;
    *out_size = 0;

    st = uefi_call_wrapper(BS->HandleProtocol, 3, image, &li_guid, (VOID **)&li);
    if (EFI_ERROR(st) || li == NULL || li->DeviceHandle == NULL)
        return EFI_NOT_FOUND;

    root = LibOpenRoot(li->DeviceHandle);
    if (root == NULL)
        return EFI_NOT_FOUND;

    st = uefi_call_wrapper(root->Open, 5, root, &f, (CHAR16 *)CartFileName, EFI_FILE_MODE_READ, (UINT64)0);
    if (EFI_ERROR(st)) {
        uefi_call_wrapper(root->Close, 1, root);
        return st;
    }

    info = LibFileInfo(f);
    if (info == NULL) {
        uefi_call_wrapper(f->Close, 1, f);
        uefi_call_wrapper(root->Close, 1, root);
        return EFI_DEVICE_ERROR;
    }

    if (info->FileSize == 0 || info->FileSize > MAX_CART_BYTES) {
        FreePool(info);
        uefi_call_wrapper(f->Close, 1, f);
        uefi_call_wrapper(root->Close, 1, root);
        return EFI_BAD_BUFFER_SIZE;
    }

    buf = AllocatePool((UINTN)info->FileSize);
    if (buf == NULL) {
        FreePool(info);
        uefi_call_wrapper(f->Close, 1, f);
        uefi_call_wrapper(root->Close, 1, root);
        return EFI_OUT_OF_RESOURCES;
    }

    read_size = (UINTN)info->FileSize;
    FreePool(info);

    st = uefi_call_wrapper(f->Read, 3, f, &read_size, buf);
    uefi_call_wrapper(f->Close, 1, f);
    uefi_call_wrapper(root->Close, 1, root);

    if (EFI_ERROR(st) || read_size == 0) {
        FreePool(buf);
        return EFI_LOAD_ERROR;
    }

    *out_buf = buf;
    *out_size = read_size;
    return EFI_SUCCESS;
}

static void scale_blit_buffer(const UINT32 *src, EFI_GRAPHICS_OUTPUT_BLT_PIXEL *dst, UINTN scale,
                              UINTN sw, UINTN sh, UINTN dst_stride_px) {
    UINTN dy = 0;
    UINTN y;

    for (y = 0; y < sh; y++) {
        UINTN sy;

        for (sy = 0; sy < scale; sy++) {
            UINTN dx = 0;
            UINTN x;

            for (x = 0; x < sw; x++) {
                UINT32 pix = src[y * sw + x];
                UINTN sx2;

                for (sx2 = 0; sx2 < scale; sx2++) {
                    EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION *u =
                        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION *)&dst[dy * dst_stride_px + dx++];

                    u->Raw = pix;
                }
            }
            dy++;
        }
    }
}

void tic80_uefi_run_loop(EFI_HANDLE image, EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
    tic80 *tic = NULL;
    VOID *cart_buf = NULL;
    UINTN cart_size = 0;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *scaled = NULL;
    HoldState holds = {0};
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    UINTN gop_w, gop_h, scale, dst_w, dst_h, ox, oy, dst_stride;
    EFI_STATUS st;

    g_exit_request = FALSE;
    uefi_call_wrapper(BS->SetWatchdogTimer, 4, BS, 0, 0, 0, NULL);

    g_tsc_hz = tsc_hz_calibrate();
    g_next_frame_tsc = rdtsc();

    if (!EFI_ERROR(load_cart_from_boot_volume(image, &cart_buf, &cart_size))) {
        Print(L"Loaded cart.tic (%u bytes).\r\n", (UINT32)cart_size);
    } else {
        cart_buf = (VOID *)(UINTN)tic80_builtin_cart;
        cart_size = (UINTN)tic80_builtin_cart_len;
        Print(L"No cart.tic on boot volume; using builtin demo.\r\n");
    }

    tic = tic80_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_BGRA8888);
    if (tic == NULL) {
        Print(L"tic80_create failed.\r\n");
        if (cart_buf != (VOID *)(UINTN)tic80_builtin_cart)
            FreePool(cart_buf);
        return;
    }

    tic->callback.exit = on_tic_exit;
    tic->callback.error = on_tic_error;

    tic80_load(tic, cart_buf, (s32)cart_size);
    if (cart_buf != (VOID *)(UINTN)tic80_builtin_cart)
        FreePool(cart_buf);

    info = gop->Mode->Info;
    gop_w = info->HorizontalResolution;
    gop_h = info->VerticalResolution;

    scale = 1;
    while (((scale + 1U) * (UINTN)TIC80_FULLWIDTH <= gop_w) &&
           ((scale + 1U) * (UINTN)TIC80_FULLHEIGHT <= gop_h))
        scale++;

    dst_w = (UINTN)TIC80_FULLWIDTH * scale;
    dst_h = (UINTN)TIC80_FULLHEIGHT * scale;
    dst_stride = dst_w;
    ox = gop_w > dst_w ? (gop_w - dst_w) / 2 : 0;
    oy = gop_h > dst_h ? (gop_h - dst_h) / 2 : 0;

    scaled = AllocatePool(dst_w * dst_h * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    if (scaled == NULL) {
        Print(L"AllocatePool scaled framebuffer failed.\r\n");
        tic80_delete(tic);
        return;
    }

    Print(L"TIC-80 loop: %ux%u @ scale %u  ESC=exit\r\n", (UINT32)dst_w, (UINT32)dst_h, (UINT32)scale);

    uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);

    while (!g_exit_request) {
        tic80_input input;

        hold_decay(&holds);
        pump_input(&holds);
        holds_to_tic_input(&holds, &input);

        tic80_tick(tic, input, uefi_counter, uefi_freq);
        tic80_sound(tic);

        scale_blit_buffer(tic->screen, scaled, scale, (UINTN)TIC80_FULLWIDTH, (UINTN)TIC80_FULLHEIGHT, dst_stride);

        st = uefi_call_wrapper(gop->Blt, 10, gop, scaled, EfiBltBufferToVideo, 0, 0, ox, oy, dst_w, dst_h,
                               0);
        if (EFI_ERROR(st)) {
            Print(L"GOP Blt failed: %r\r\n", st);
            break;
        }

        wait_next_frame();
    }

    FreePool(scaled);
    tic80_delete(tic);
    Print(L"TIC-80 stopped.\r\n");
}
