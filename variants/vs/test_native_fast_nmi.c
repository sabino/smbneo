#include "vs_native_fast_nmi.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    RAM_SIZE = 0x0800,
    EXTRA_RAM_SIZE = 0x0800,
    EXHAUSTIVE_CASES = 256 * 256,
    OAM_CASES = 4096
};

typedef void (*NmiFunction)(SmbNeoNativeContext *ctx);

static uint64_t cases_run;

static uint32_t mix32(uint32_t value) {
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return value;
}

static uint8_t pattern_byte(uint32_t seed, unsigned index) {
    return (uint8_t)mix32(seed + (uint32_t)index * UINT32_C(0x9e3779b9));
}

static uint8_t reference_nz(SmbNeoNativeContext *ctx, uint8_t value) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) |
        (value & NATIVE_N) |
        (value == 0u ? NATIVE_Z : 0u)
    );
    return value;
}

static void reference_cmp(
    SmbNeoNativeContext *ctx,
    uint8_t lhs,
    uint8_t rhs
) {
    uint8_t result = (uint8_t)(lhs - rhs);
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_C | NATIVE_N | NATIVE_Z)) |
        (lhs >= rhs ? NATIVE_C : 0u) |
        (result & NATIVE_N) |
        (result == 0u ? NATIVE_Z : 0u)
    );
}

static void reference_adc(SmbNeoNativeContext *ctx, uint8_t value) {
    uint8_t lhs = ctx->a;
    unsigned sum = (unsigned)lhs + value +
        ((ctx->status & NATIVE_C) != 0u ? 1u : 0u);
    uint8_t result = (uint8_t)sum;

    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_C | NATIVE_V)) |
        (sum > 255u ? NATIVE_C : 0u) |
        ((~(lhs ^ value) & (lhs ^ result) & 0x80u) != 0u
            ? NATIVE_V : 0u)
    );
    ctx->a = reference_nz(ctx, result);
}

static uint8_t reference_ror(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    uint8_t carry_in = (ctx->status & NATIVE_C) != 0u ? 0x80u : 0u;
    uint8_t result;

    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 1u) != 0u ? NATIVE_C : 0u)
    );
    result = (uint8_t)((value >> 1) | carry_in);
    return reference_nz(ctx, result);
}

/* Instruction-shaped oracle for $817e through the INC at $81a1. */
static void reference_nmi_timers(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;

    ctx->a = reference_nz(ctx, ram[0x0747u]);
    if ((ctx->status & NATIVE_Z) == 0u) {
        ram[0x0747u] = reference_nz(
            ctx, (uint8_t)(ram[0x0747u] - 1u)
        );
        if ((ctx->status & NATIVE_Z) == 0u)
            goto increment_frame;
    }

    ctx->x = reference_nz(ctx, 0x14u);
    ram[0x077fu] = reference_nz(ctx, (uint8_t)(ram[0x077fu] - 1u));
    if ((ctx->status & NATIVE_N) != 0u) {
        ctx->a = reference_nz(ctx, 0x14u);
        ram[0x077fu] = ctx->a;
        ctx->x = reference_nz(ctx, 0x23u);
    }

    for (;;) {
        unsigned address = 0x0780u + ctx->x;

        ctx->a = reference_nz(ctx, ram[address]);
        if ((ctx->status & NATIVE_Z) == 0u)
            ram[address] = reference_nz(ctx, (uint8_t)(ram[address] - 1u));
        ctx->x = reference_nz(ctx, (uint8_t)(ctx->x - 1u));
        if ((ctx->status & NATIVE_N) != 0u)
            break;
    }

increment_frame:
    ram[0x0009u] = reference_nz(ctx, (uint8_t)(ram[0x0009u] + 1u));
}

/* Instruction-shaped oracle for $81a3 through the BNE at $81be. */
static void reference_nmi_random(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;

    ctx->x = reference_nz(ctx, 0u);
    ctx->y = reference_nz(ctx, 7u);
    ctx->a = reference_nz(ctx, ram[0x07a7u]);
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 2u));
    ram[0x00u] = ctx->a;
    ctx->a = reference_nz(ctx, ram[0x07a8u]);
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 2u));
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a ^ ram[0x00u]));
    ctx->status &= (uint8_t)~NATIVE_C;
    if ((ctx->status & NATIVE_Z) == 0u)
        ctx->status |= NATIVE_C;

    for (;;) {
        unsigned address = 0x07a7u + ctx->x;

        ram[address] = reference_ror(ctx, ram[address]);
        ctx->x = reference_nz(ctx, (uint8_t)(ctx->x + 1u));
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y - 1u));
        if ((ctx->status & NATIVE_Z) != 0u)
            break;
    }
}

/* Literal LDY/DEY/BNE oracle for the delay at $81d9. */
static void reference_nmi_hblank_delay(SmbNeoNativeContext *ctx) {
    ctx->y = reference_nz(ctx, 0x14u);
    do {
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y - 1u));
    } while ((ctx->status & NATIVE_Z) == 0u);
}

/* Instruction-shaped oracle for the complete $8212 callable routine. */
static void reference_nmi_oam_shuffle(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;

    ctx->y = reference_nz(ctx, ram[0x074eu]);
    ctx->a = reference_nz(ctx, 0x28u);
    ram[0x00u] = ctx->a;
    ctx->x = reference_nz(ctx, 0x0eu);
    for (;;) {
        unsigned slot = ctx->x;

        ctx->a = reference_nz(ctx, ram[0x06e4u + slot]);
        reference_cmp(ctx, ctx->a, ram[0x00u]);
        if ((ctx->status & NATIVE_C) != 0u) {
            ctx->y = reference_nz(ctx, ram[0x06e0u]);
            ctx->status &= (uint8_t)~NATIVE_C;
            reference_adc(ctx, ram[0x06e1u + ctx->y]);
            if ((ctx->status & NATIVE_C) != 0u) {
                ctx->status &= (uint8_t)~NATIVE_C;
                reference_adc(ctx, ram[0x00u]);
            }
            ram[0x06e4u + slot] = ctx->a;
        }
        ctx->x = reference_nz(ctx, (uint8_t)(ctx->x - 1u));
        if ((ctx->status & NATIVE_N) != 0u)
            break;
    }

    ctx->x = reference_nz(ctx, ram[0x06e0u]);
    ctx->x = reference_nz(ctx, (uint8_t)(ctx->x + 1u));
    reference_cmp(ctx, ctx->x, 3u);
    if ((ctx->status & NATIVE_Z) != 0u)
        ctx->x = reference_nz(ctx, 0u);
    ram[0x06e0u] = ctx->x;
    ctx->x = reference_nz(ctx, 8u);
    ctx->y = reference_nz(ctx, 2u);
    for (;;) {
        unsigned source = 0x06e9u + ctx->y;
        unsigned target = 0x06f1u + ctx->x;

        ctx->a = reference_nz(ctx, ram[source]);
        ram[target] = ctx->a;
        ctx->status &= (uint8_t)~NATIVE_C;
        reference_adc(ctx, 8u);
        ram[target + 1u] = ctx->a;
        ctx->status &= (uint8_t)~NATIVE_C;
        reference_adc(ctx, 8u);
        ram[target + 2u] = ctx->a;
        ctx->x = reference_nz(ctx, (uint8_t)(ctx->x - 1u));
        ctx->x = reference_nz(ctx, (uint8_t)(ctx->x - 1u));
        ctx->x = reference_nz(ctx, (uint8_t)(ctx->x - 1u));
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y - 1u));
        if ((ctx->status & NATIVE_N) != 0u)
            break;
    }
}

static void reference_oam_init_from(
    SmbNeoNativeContext *ctx,
    uint8_t first
) {
    uint8_t *ram = ctx->ram;

    ctx->y = reference_nz(ctx, first);
    ctx->a = reference_nz(ctx, 0xf8u);
    do {
        ram[0x0200u + ctx->y] = ctx->a;
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y + 1u));
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y + 1u));
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y + 1u));
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y + 1u));
    } while ((ctx->status & NATIVE_Z) == 0u);
}

static void reference_oam_init(SmbNeoNativeContext *ctx) {
    reference_oam_init_from(ctx, 0u);
}

static void reference_oam_init_all(SmbNeoNativeContext *ctx) {
    reference_oam_init_from(ctx, 4u);
}

static void prepare_context(
    SmbNeoNativeContext *ctx,
    uint8_t *ram,
    uint8_t *extra_ram,
    uint32_t seed
) {
    memset(ctx, 0, sizeof(*ctx));
    memset(ram, pattern_byte(seed, 0u), RAM_SIZE);
    memset(extra_ram, pattern_byte(seed, 1u), EXTRA_RAM_SIZE);
    ctx->a = pattern_byte(seed, 2u);
    ctx->x = pattern_byte(seed, 3u);
    ctx->y = pattern_byte(seed, 4u);
    ctx->status = pattern_byte(seed, 5u);
    ctx->ram = ram;
    ctx->extra_ram = extra_ram;
    ctx->opaque = extra_ram;
    ctx->suspended = pattern_byte(seed, 6u);
    ctx->unsupported = pattern_byte(seed, 7u);
    ctx->calls = mix32(seed ^ UINT32_C(0xa5a55a5a));
    ctx->last_symbol = (uint16_t)mix32(seed ^ UINT32_C(0x31415926));
}

static int compare_case(
    const char *suite,
    uint32_t ordinal,
    NmiFunction reference_function,
    NmiFunction fast_function,
    SmbNeoNativeContext *reference,
    SmbNeoNativeContext *actual,
    uint8_t *reference_ram,
    uint8_t *actual_ram,
    uint8_t *reference_extra,
    uint8_t *actual_extra
) {
    reference_function(reference);
    fast_function(actual);
    ++cases_run;

#define CHECK_SCALAR(field) \
    do { if (reference->field != actual->field) { \
        fprintf(stderr, "%s case=%" PRIu32 " " #field \
                " expected=%" PRIu32 " actual=%" PRIu32 "\n", \
                suite, ordinal, (uint32_t)reference->field, \
                (uint32_t)actual->field); \
        return 1; \
    } } while (0)
    CHECK_SCALAR(a);
    CHECK_SCALAR(x);
    CHECK_SCALAR(y);
    CHECK_SCALAR(status);
    CHECK_SCALAR(suspended);
    CHECK_SCALAR(unsupported);
    CHECK_SCALAR(calls);
    CHECK_SCALAR(last_symbol);
#undef CHECK_SCALAR

    if (memcmp(reference_ram, actual_ram, RAM_SIZE) != 0) {
        unsigned address;
        for (address = 0u; address < RAM_SIZE; ++address) {
            if (reference_ram[address] != actual_ram[address]) {
                fprintf(stderr, "%s case=%" PRIu32 " RAM[%04x] "
                        "expected=%02x actual=%02x\n", suite, ordinal,
                        address, reference_ram[address], actual_ram[address]);
                break;
            }
        }
        return 1;
    }
    if (memcmp(reference_extra, actual_extra, EXTRA_RAM_SIZE) != 0) {
        fprintf(stderr, "%s case=%" PRIu32 " modified extra RAM\n",
                suite, ordinal);
        return 1;
    }
    if (reference->ram != reference_ram || actual->ram != actual_ram ||
            reference->extra_ram != reference_extra ||
            actual->extra_ram != actual_extra ||
            reference->opaque != reference_extra ||
            actual->opaque != actual_extra) {
        fprintf(stderr, "%s case=%" PRIu32 " modified context binding\n",
                suite, ordinal);
        return 1;
    }
    return 0;
}

static int run_case(
    const char *suite,
    uint32_t ordinal,
    NmiFunction reference_function,
    NmiFunction fast_function,
    void (*specialize)(SmbNeoNativeContext *, uint32_t)
) {
    uint8_t reference_ram[RAM_SIZE];
    uint8_t actual_ram[RAM_SIZE];
    uint8_t reference_extra[EXTRA_RAM_SIZE];
    uint8_t actual_extra[EXTRA_RAM_SIZE];
    SmbNeoNativeContext reference;
    SmbNeoNativeContext actual;
    uint32_t seed = mix32(ordinal ^ UINT32_C(0xd1b54a35));

    prepare_context(&reference, reference_ram, reference_extra, seed);
    if (specialize != NULL)
        specialize(&reference, ordinal);
    actual = reference;
    memcpy(actual_ram, reference_ram, sizeof(actual_ram));
    memcpy(actual_extra, reference_extra, sizeof(actual_extra));
    actual.ram = actual_ram;
    actual.extra_ram = actual_extra;
    actual.opaque = actual_extra;
    return compare_case(
        suite, ordinal, reference_function, fast_function,
        &reference, &actual, reference_ram, actual_ram,
        reference_extra, actual_extra
    );
}

static void specialize_timers(SmbNeoNativeContext *ctx, uint32_t ordinal) {
    unsigned timer;

    ctx->ram[0x0747u] = (uint8_t)ordinal;
    ctx->ram[0x077fu] = (uint8_t)(ordinal >> 8);
    ctx->ram[0x0009u] = pattern_byte(ordinal, 0x09u);
    for (timer = 0u; timer <= 0x23u; ++timer)
        ctx->ram[0x0780u + timer] = pattern_byte(ordinal, timer + 0x80u);
}

static void specialize_random(SmbNeoNativeContext *ctx, uint32_t ordinal) {
    unsigned byte;

    for (byte = 0u; byte < 7u; ++byte)
        ctx->ram[0x07a7u + byte] = pattern_byte(ordinal, byte + 0xa7u);
    ctx->ram[0x07a7u] = (uint8_t)ordinal;
    ctx->ram[0x07a8u] = (uint8_t)(ordinal >> 8);
    ctx->ram[0x00u] = pattern_byte(ordinal, 0x100u);
}

static void specialize_hblank(SmbNeoNativeContext *ctx, uint32_t ordinal) {
    ctx->status = (uint8_t)ordinal;
    ctx->y = (uint8_t)(ordinal >> 8);
}

static void specialize_shuffle(SmbNeoNativeContext *ctx, uint32_t ordinal) {
    unsigned address;
    uint8_t slot = (uint8_t)(ordinal % 15u);

    for (address = 0x06e0u; address <= 0x07e0u; ++address)
        ctx->ram[address] = pattern_byte(ordinal, address);
    ctx->ram[0x06e0u] = (uint8_t)ordinal;
    ctx->ram[0x06e4u + slot] = (uint8_t)(ordinal >> 8);
}

static void specialize_oam(SmbNeoNativeContext *ctx, uint32_t ordinal) {
    unsigned address;

    for (address = 0x0200u; address < 0x0300u; ++address)
        ctx->ram[address] = pattern_byte(ordinal, address);
}

static int run_suite(
    const char *name,
    uint32_t count,
    NmiFunction reference_function,
    NmiFunction fast_function,
    void (*specialize)(SmbNeoNativeContext *, uint32_t)
) {
    uint32_t ordinal;

    for (ordinal = 0u; ordinal < count; ++ordinal) {
        if (run_case(
                name, ordinal, reference_function, fast_function, specialize
            ) != 0)
            return 1;
    }
    return 0;
}

static int check_missing_ram(void) {
    static const NmiFunction functions[] = {
        vs_native_fast_nmi_timers,
        vs_native_fast_nmi_random,
        vs_native_fast_nmi_oam_shuffle,
        vs_native_fast_oam_init,
        vs_native_fast_oam_init_all
    };
    unsigned index;

    for (index = 0u; index < sizeof(functions) / sizeof(functions[0]); ++index) {
        SmbNeoNativeContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        functions[index](&ctx);
        if (ctx.unsupported != 1u) {
            fprintf(stderr, "missing RAM was not rejected for function %u\n",
                    index);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    if (run_suite(
            "timers", EXHAUSTIVE_CASES,
            reference_nmi_timers, vs_native_fast_nmi_timers,
            specialize_timers
        ) != 0 ||
        run_suite(
            "random", EXHAUSTIVE_CASES,
            reference_nmi_random, vs_native_fast_nmi_random,
            specialize_random
        ) != 0 ||
        run_suite(
            "hblank", EXHAUSTIVE_CASES,
            reference_nmi_hblank_delay, vs_native_fast_nmi_hblank_delay,
            specialize_hblank
        ) != 0 ||
        run_suite(
            "shuffle", EXHAUSTIVE_CASES,
            reference_nmi_oam_shuffle, vs_native_fast_nmi_oam_shuffle,
            specialize_shuffle
        ) != 0 ||
        run_suite(
            "oam-init", OAM_CASES,
            reference_oam_init, vs_native_fast_oam_init,
            specialize_oam
        ) != 0 ||
        run_suite(
            "oam-init-all", OAM_CASES,
            reference_oam_init_all, vs_native_fast_oam_init_all,
            specialize_oam
        ) != 0 ||
        check_missing_ram() != 0)
        return 1;

    printf("VS native NMI fast paths: %" PRIu64
           " exact source-semantic cases and 5 missing-RAM guards passed\n",
           cases_run);
    return 0;
}
