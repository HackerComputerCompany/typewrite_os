/* Pull in libtic80core + Lua from the TIC-80 UEFI static build; not executed on success path. */
#include <tic80.h>

void tic80_uefi_probe_create_destroy(void) {
    tic80 *t = tic80_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_BGRA8888);
    if (t)
        tic80_delete(t);
}
