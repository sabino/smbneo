#include "vs_native_fast_video.h"
#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
#define SMBNEO_FAST_INLINE static inline __attribute__((always_inline))
#else
#define SMBNEO_FAST_INLINE static inline
#endif

SMBNEO_FAST_INLINE void io_write(SmbNeoNativeContext *ctx, uint16_t address,
                                 uint8_t value) {
    ctx->write8(ctx->opaque, address, value);
}

bool vs_native_fast_ppu_displist_command(
    SmbNeoNativeContext *ctx, const VsNativeFastVideoHooks *hooks) {
    uint8_t *ram;
    uint16_t source;
    uint16_t destination;
    uint16_t increment;
    uint8_t command;
    uint8_t count;
    bool repeat;
    bool bulk_done = false;

    /* Preflight every rejection before making the first observable change. */
    if (ctx == NULL || ctx->ram == NULL || ctx->write8 == NULL ||
        ctx->y != 0u || ctx->suspended != 0u || ctx->unsupported != 0u)
        return false;
    ram = ctx->ram;
    source = (uint16_t)(ram[0] | ((uint16_t)ram[1] << 8));
    if (source < 0x0300u || source > 0x03fcu ||
        ctx->a != ram[source] || ctx->a == 0u)
        return false;

    command = ram[source + 2u];
    count = (uint8_t)(command & 0x3fu);
    repeat = (command & 0x40u) != 0u;
    if (count == 0u ||
        (uint16_t)(source + 3u + (repeat ? 0u : count - 1u)) > 0x03ffu)
        return false;

    destination = (uint16_t)(((uint16_t)(ctx->a & 0x3fu) << 8) |
                             ram[source + 1u]);
    increment = (command & 0x80u) != 0u ? 32u : 1u;

    /* These are the same ordered, externally visible writes as the source
     * routine.  Its instruction-temporary flags are deliberately collapsed;
     * the exact routine-exit flags and registers are restored below. */
    io_write(ctx, 0x2006u, ctx->a);
    io_write(ctx, 0x2006u, ram[source + 1u]);
    ctx->a = (uint8_t)((ram[0x0778u] & 0xfbu) |
        ((command & 0x80u) != 0u ? 4u : 0u));
    io_write(ctx, 0x2000u, ctx->a);
    ram[0x0778u] = ctx->a;

    if (hooks != NULL && hooks->ppu_data_run != NULL) {
        bulk_done = hooks->ppu_data_run(hooks->opaque, destination,
                                       ram + source + 3u, count,
                                       increment, repeat);
    }

    if (bulk_done) {
        /* The callback has committed the complete ordered data-port run. */
    } else {
        for (unsigned i = 0; i < count; ++i)
            io_write(ctx, 0x2007u, ram[source + 3u +
                                       (repeat ? 0u : i)]);
    }

    ctx->x = 0u;
    ctx->y = repeat ? 3u : (uint8_t)(count + 2u);
    source = (uint16_t)(source + ctx->y + 1u);
    ram[0] = (uint8_t)source;
    ram[1] = (uint8_t)(source >> 8);

    io_write(ctx, 0x2006u, 0x3fu);
    io_write(ctx, 0x2006u, 0u);
    io_write(ctx, 0x2006u, 0u);
    io_write(ctx, 0x2006u, 0u);
    ctx->a = 0u;
    /* The final pointer-high addition cannot carry or overflow on page $03;
     * the final load of zero defines N/Z.  All other status bits survive. */
    ctx->status = (uint8_t)((ctx->status &
        (uint8_t)~(NATIVE_C | NATIVE_V | NATIVE_N | NATIVE_Z)) | NATIVE_Z);
    return true;
}
