#include "vs_native_fast_nmi.h"

#include <stddef.h>
#include <stdint.h>

enum {
    FRAME_COUNT = 0x0009,
    OAM_BUFFER = 0x0200,
    GAME_TIMER_STOP = 0x0747,
    TIMER_NMI_TICK = 0x077f,
    TIMERS_NMI = 0x0780,
    SRAND = 0x07a7,
    SPRITE_STROBE_OFFSET = 0x06e0,
    SPRITE_STROBE_AMOUNT = 0x06e1,
    OBJ_DATA_OFFSET_PLAYER = 0x06e4,
    OBJ_DATA_OFFSET_ACTOR_4 = 0x06e9,
    OBJ_DATA_OFFSET_PROJ = 0x06f1
};

#if defined(__GNUC__) || defined(__clang__)
#define NMI_FORCE_INLINE static inline __attribute__((always_inline))
#else
#define NMI_FORCE_INLINE static inline
#endif

NMI_FORCE_INLINE uint8_t *nmi_ram(SmbNeoNativeContext *ctx) {
    if (ctx == NULL)
        return NULL;
    if (ctx->ram == NULL) {
        ctx->unsupported = 1u;
        return NULL;
    }
    return ctx->ram;
}

NMI_FORCE_INLINE uint8_t nmi_nz_bits(uint8_t value) {
    return (uint8_t)(
        (value & NATIVE_N) | (value == 0u ? NATIVE_Z : 0u)
    );
}

void vs_native_fast_nmi_timers(SmbNeoNativeContext *ctx) {
    uint8_t *ram = nmi_ram(ctx);
    uint8_t value;

    if (ram == NULL)
        return;

    /* LDA/DEC/BNE either enters the timer bank or skips straight to the
     * frame counter. None of those instructions change C or V. */
    ctx->a = ram[GAME_TIMER_STOP];
    if (ctx->a != 0u) {
        ram[GAME_TIMER_STOP]--;
        if (ram[GAME_TIMER_STOP] != 0u)
            goto increment_frame;
    }

    ctx->x = 0x14u;
    value = --ram[TIMER_NMI_TICK];
    if ((value & NATIVE_N) != 0u) {
        ctx->a = 0x14u;
        ram[TIMER_NMI_TICK] = ctx->a;
        ctx->x = 0x23u;
    }

    for (;;) {
        unsigned address = TIMERS_NMI + ctx->x;

        ctx->a = ram[address];
        if (ctx->a != 0u)
            ram[address]--;
        if (ctx->x == 0u) {
            ctx->x = 0xffu;
            break;
        }
        ctx->x--;
    }

increment_frame:
    value = ++ram[FRAME_COUNT];
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) |
        nmi_nz_bits(value)
    );
}

void vs_native_fast_nmi_random(SmbNeoNativeContext *ctx) {
    uint8_t *ram = nmi_ram(ctx);
    uint8_t carry;
    unsigned byte;

    if (ram == NULL)
        return;

    /* The seven RORs need only their carry chain. The final DEY fixes N/Z
     * to clear/set, so all intermediate N/Z work is dead. */
    ctx->a = (uint8_t)(ram[SRAND] & 0x02u);
    ram[0x00u] = ctx->a;
    ctx->a = (uint8_t)((ram[SRAND + 1u] & 0x02u) ^ ram[0x00u]);
    carry = (uint8_t)(ctx->a != 0u);

    for (byte = 0u; byte < 7u; ++byte) {
        uint8_t value = ram[SRAND + byte];

        ram[SRAND + byte] = (uint8_t)((value >> 1) | (carry << 7));
        carry = (uint8_t)(value & 1u);
    }
    ctx->x = 7u;
    ctx->y = 0u;
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_C | NATIVE_N | NATIVE_Z)) |
        NATIVE_Z | (carry != 0u ? NATIVE_C : 0u)
    );
}

void vs_native_fast_nmi_hblank_delay(SmbNeoNativeContext *ctx) {
    if (ctx == NULL)
        return;

    /* LDY #$14 followed by DEY/BNE leaves only Y and N/Z observable. */
    ctx->y = 0u;
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) | NATIVE_Z
    );
}

void vs_native_fast_nmi_oam_shuffle(SmbNeoNativeContext *ctx) {
    uint8_t *ram = nmi_ram(ctx);
    uint8_t preserved_status;
    unsigned slot;
    unsigned sum = 0u;
    uint8_t lhs = 0u;

    if (ram == NULL)
        return;

    preserved_status = ctx->status;
    ram[0x00u] = 0x28u;

    /* Walk descending exactly because an out-of-domain strobe index can make
     * the amount byte alias an object offset changed by an earlier slot. */
    for (slot = 15u; slot-- != 0u;) {
        uint8_t value = ram[OBJ_DATA_OFFSET_PLAYER + slot];

        if (value >= 0x28u) {
            sum = (unsigned)value +
                ram[SPRITE_STROBE_AMOUNT + ram[SPRITE_STROBE_OFFSET]];
            value = (uint8_t)sum;
            if (sum > 255u) {
                sum = (unsigned)value + 0x28u;
                value = (uint8_t)sum;
            }
            ram[OBJ_DATA_OFFSET_PLAYER + slot] = value;
        }
    }

    ctx->x = (uint8_t)(ram[SPRITE_STROBE_OFFSET] + 1u);
    if (ctx->x == 3u)
        ctx->x = 0u;
    ram[SPRITE_STROBE_OFFSET] = ctx->x;

    ctx->x = 8u;
    ctx->y = 2u;
    for (;;) {
        unsigned target = OBJ_DATA_OFFSET_PROJ + ctx->x;

        ctx->a = ram[OBJ_DATA_OFFSET_ACTOR_4 + ctx->y];
        ram[target] = ctx->a;
        ctx->a = (uint8_t)((unsigned)ctx->a + 8u);
        ram[target + 1u] = ctx->a;
        lhs = ctx->a;
        sum = (unsigned)lhs + 8u;
        ctx->a = (uint8_t)sum;
        ram[target + 2u] = ctx->a;
        ctx->x = (uint8_t)(ctx->x - 3u);
        ctx->y--;
        if ((ctx->y & NATIVE_N) != 0u)
            break;
    }

    /* The last ADC supplies C/V; the final DEY supplies N/Z. */
    ctx->status = (uint8_t)(
        (preserved_status & (uint8_t)~(NATIVE_C | NATIVE_V |
                                       NATIVE_N | NATIVE_Z)) |
        NATIVE_N |
        (sum > 255u ? NATIVE_C : 0u) |
        ((~(lhs ^ 8u) & (lhs ^ ctx->a) & 0x80u) != 0u
            ? NATIVE_V : 0u)
    );
}

static void oam_init_from(SmbNeoNativeContext *ctx, uint8_t first) {
    uint8_t *ram = nmi_ram(ctx);
    unsigned offset;

    if (ram == NULL)
        return;

    ctx->a = 0xf8u;
    for (offset = first; offset < 0x100u; offset += 4u)
        ram[OAM_BUFFER + offset] = ctx->a;

    /* The final fourth INY wraps to zero for both source entry points. */
    ctx->y = 0u;
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) | NATIVE_Z
    );
}

void vs_native_fast_oam_init(SmbNeoNativeContext *ctx) {
    oam_init_from(ctx, 0u);
}

void vs_native_fast_oam_init_all(SmbNeoNativeContext *ctx) {
    oam_init_from(ctx, 4u);
}
