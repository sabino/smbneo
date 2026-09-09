#include "smbneo_native.h"

static uint8_t memory_image[65536];

static uint8_t read8(void *opaque, uint16_t address) {
    return ((uint8_t *)opaque)[address];
}

static void write8(void *opaque, uint16_t address, uint8_t value) {
    ((uint8_t *)opaque)[address] = value;
}

/* ABI/boot smoke only: semantic game behavior is not claimed yet. */
int main(void) {
    SmbNeoNativeContext context = {
        .opaque = memory_image,
        .read8 = read8,
        .write8 = write8,
        .ram = memory_image,
        .extra_ram = memory_image + 0x6000,
        .prg = memory_image + 0x8000,
    };
    smbneo_native_boot(&context);
    if (context.calls == 0u)
        return 1;
    context.suspended = 0;
    context.unsupported = 0;
    smbneo_native_frame(&context);
    return context.calls > 1u ? 0 : 2;
}
