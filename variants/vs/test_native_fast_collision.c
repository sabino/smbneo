#include "vs_native_fast_collision.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    RAM_SIZE = 0x0800,
    PRG_SIZE = 0x8000,
    COL_PROC_CASES = 13 * 7 * 256,
    COL_BUFFER_CASES = 22 * 28 * 256,
    POS_X_CASES = 256 * 256,
    POS_PROC_CASES = 32768,
    POS_ACTOR_CASES = 32768
};

typedef bool (*FastFunction)(SmbNeoNativeContext *ctx);

static uint8_t actual_ram[RAM_SIZE];
static uint8_t reference_ram[RAM_SIZE];
static uint8_t test_prg[PRG_SIZE];
static uint64_t cases_run;

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
    unsigned sum = (unsigned)ctx->a + value +
        ((ctx->status & NATIVE_C) != 0u ? 1u : 0u);
    uint8_t result = (uint8_t)sum;
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_C | NATIVE_V)) |
        (sum > 255u ? NATIVE_C : 0u) |
        ((~(ctx->a ^ value) & (ctx->a ^ result) & 0x80u) != 0u
            ? NATIVE_V : 0u)
    );
    ctx->a = reference_nz(ctx, result);
}

static void reference_sbc(SmbNeoNativeContext *ctx, uint8_t value) {
    reference_adc(ctx, (uint8_t)~value);
}

static void reference_asl(SmbNeoNativeContext *ctx) {
    uint8_t value = ctx->a;
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 0x80u) != 0u ? NATIVE_C : 0u)
    );
    ctx->a = reference_nz(ctx, (uint8_t)(value << 1));
}

static void reference_lsr(SmbNeoNativeContext *ctx) {
    uint8_t value = ctx->a;
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 1u) != 0u ? NATIVE_C : 0u)
    );
    ctx->a = reference_nz(ctx, (uint8_t)(value >> 1));
}

static void reference_ror(SmbNeoNativeContext *ctx) {
    uint8_t value = ctx->a;
    uint8_t carry = (ctx->status & NATIVE_C) != 0u ? 0x80u : 0u;
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 1u) != 0u ? NATIVE_C : 0u)
    );
    ctx->a = reference_nz(ctx, (uint8_t)((value >> 1) | carry));
}

static uint8_t reference_read(
    const SmbNeoNativeContext *ctx,
    uint16_t address
) {
    if (address < 0x2000u)
        return ctx->ram[address & 0x07ffu];
    if (address >= 0x8000u)
        return ctx->prg[address - 0x8000u];
    abort();
}

static void reference_write(
    SmbNeoNativeContext *ctx,
    uint16_t address,
    uint8_t value
) {
    if (address >= 0x2000u)
        abort();
    ctx->ram[address & 0x07ffu] = value;
}

static uint16_t reference_zpword(
    const SmbNeoNativeContext *ctx,
    uint8_t address
) {
    return (uint16_t)(reference_read(ctx, address) |
        ((uint16_t)reference_read(ctx, (uint8_t)(address + 1u)) << 8));
}

/* Literal direct-C instruction order for $E1F2-$E233. */
static void reference_col_box_proc(SmbNeoNativeContext *ctx) {
    uint8_t saved_value;

    reference_write(ctx, 0x00u, ctx->x);
    ctx->a = reference_nz(ctx,
        reference_read(ctx, (uint16_t)(0x03b8u + ctx->y)));
    reference_write(ctx, 0x02u, ctx->a);
    ctx->a = reference_nz(ctx,
        reference_read(ctx, (uint16_t)(0x03adu + ctx->y)));
    reference_write(ctx, 0x01u, ctx->a);
    ctx->a = reference_nz(ctx, ctx->x);
    reference_asl(ctx);
    reference_asl(ctx);
    saved_value = ctx->a;
    ctx->y = reference_nz(ctx, ctx->a);
    ctx->a = reference_nz(ctx,
        reference_read(ctx, (uint16_t)(0x0499u + ctx->x)));
    reference_asl(ctx);
    reference_asl(ctx);
    ctx->x = reference_nz(ctx, ctx->a);
    ctx->a = reference_nz(ctx, reference_read(ctx, 0x01u));
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx,
        reference_read(ctx, (uint16_t)(0xe153u + ctx->x)));
    reference_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
    ctx->a = reference_nz(ctx, reference_read(ctx, 0x01u));
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx,
        reference_read(ctx, (uint16_t)(0xe155u + ctx->x)));
    reference_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
    ctx->x = reference_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = reference_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = reference_nz(ctx, reference_read(ctx, 0x02u));
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx,
        reference_read(ctx, (uint16_t)(0xe153u + ctx->x)));
    reference_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
    ctx->a = reference_nz(ctx, reference_read(ctx, 0x02u));
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx,
        reference_read(ctx, (uint16_t)(0xe155u + ctx->x)));
    reference_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
    ctx->a = reference_nz(ctx, saved_value);
    ctx->y = reference_nz(ctx, ctx->a);
    ctx->x = reference_nz(ctx, reference_read(ctx, 0x00u));
}

/* Literal direct-C instruction order for the $AB14 dependency. */
static void reference_scenery_get_buffer_ptr(SmbNeoNativeContext *ctx) {
    uint8_t saved_value = ctx->a;

    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_lsr(ctx);
    ctx->y = reference_nz(ctx, ctx->a);
    ctx->a = reference_nz(ctx,
        reference_read(ctx, (uint16_t)(0xab12u + ctx->y)));
    reference_write(ctx, 0x07u, ctx->a);
    ctx->a = reference_nz(ctx, saved_value);
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx,
        reference_read(ctx, (uint16_t)(0xab10u + ctx->y)));
    reference_write(ctx, 0x06u, ctx->a);
}

/* Literal direct-C instruction order for $E348-$E389. */
static void reference_col_box_buffer(SmbNeoNativeContext *ctx) {
    uint8_t saved_value = ctx->a;

    reference_write(ctx, 0x04u, ctx->y);
    ctx->a = reference_nz(ctx,
        reference_read(ctx, (uint16_t)(0xe306u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx,
        reference_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    reference_write(ctx, 0x05u, ctx->a);
    ctx->a = reference_nz(ctx,
        reference_read(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x)));
    reference_adc(ctx, 0u);
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 1u));
    reference_lsr(ctx);
    ctx->a = reference_nz(ctx,
        (uint8_t)(ctx->a | reference_read(ctx, 0x05u)));
    reference_ror(ctx);
    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_scenery_get_buffer_ptr(ctx);
    ctx->y = reference_nz(ctx, reference_read(ctx, 0x04u));
    ctx->a = reference_nz(ctx,
        reference_read(ctx, (uint16_t)(uint8_t)(0xceu + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx,
        reference_read(ctx, (uint16_t)(0xe322u + ctx->y)));
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    ctx->status |= NATIVE_C;
    reference_sbc(ctx, 0x20u);
    reference_write(ctx, 0x02u, ctx->a);
    ctx->y = reference_nz(ctx, ctx->a);
    ctx->a = reference_nz(ctx, reference_read(ctx,
        (uint16_t)(reference_zpword(ctx, 0x06u) + ctx->y)));
    reference_write(ctx, 0x03u, ctx->a);
    ctx->y = reference_nz(ctx, reference_read(ctx, 0x04u));
    ctx->a = reference_nz(ctx, saved_value);
    if (ctx->a == 0u) {
        ctx->a = reference_nz(ctx,
            reference_read(ctx, (uint16_t)(uint8_t)(0xceu + ctx->x)));
    } else {
        ctx->a = reference_nz(ctx,
            reference_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    }
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    reference_write(ctx, 0x04u, ctx->a);
    ctx->a = reference_nz(ctx, reference_read(ctx, 0x03u));
}

/* Literal helper at $F1D2-$F1E6. */
static void reference_pos_bits_diff(SmbNeoNativeContext *ctx) {
    reference_write(ctx, 0x05u, ctx->a);
    ctx->a = reference_nz(ctx, reference_read(ctx, 0x07u));
    reference_cmp(ctx, ctx->a, reference_read(ctx, 0x06u));
    if ((ctx->status & NATIVE_C) != 0u)
        return;
    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_lsr(ctx);
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    reference_cmp(ctx, ctx->y, 0x01u);
    if ((ctx->status & NATIVE_C) == 0u)
        reference_adc(ctx, reference_read(ctx, 0x05u));
    ctx->x = reference_nz(ctx, ctx->a);
}

/* Literal direct-C instruction order for $F15B-$F18F. */
static void reference_pos_bits_get_do_x(SmbNeoNativeContext *ctx) {
    reference_write(ctx, 0x04u, ctx->x);
    ctx->y = reference_nz(ctx, 0x01u);
    for (;;) {
        ctx->a = reference_nz(ctx,
            reference_read(ctx, (uint16_t)(0x071cu + ctx->y)));
        ctx->status |= NATIVE_C;
        reference_sbc(ctx,
            reference_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
        reference_write(ctx, 0x07u, ctx->a);
        ctx->a = reference_nz(ctx,
            reference_read(ctx, (uint16_t)(0x071au + ctx->y)));
        reference_sbc(ctx,
            reference_read(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x)));
        ctx->x = reference_nz(ctx,
            reference_read(ctx, (uint16_t)(0xf158u + ctx->y)));
        reference_cmp(ctx, ctx->a, 0u);
        if ((ctx->status & NATIVE_N) == 0u) {
            ctx->x = reference_nz(ctx,
                reference_read(ctx, (uint16_t)(0xf159u + ctx->y)));
            reference_cmp(ctx, ctx->a, 1u);
            if ((ctx->status & NATIVE_N) != 0u) {
                ctx->a = reference_nz(ctx, 0x38u);
                reference_write(ctx, 0x06u, ctx->a);
                ctx->a = reference_nz(ctx, 0x08u);
                reference_pos_bits_diff(ctx);
            }
        }
        ctx->a = reference_nz(ctx,
            reference_read(ctx, (uint16_t)(0xf148u + ctx->x)));
        ctx->x = reference_nz(ctx, reference_read(ctx, 0x04u));
        reference_cmp(ctx, ctx->a, 0u);
        if ((ctx->status & NATIVE_Z) == 0u)
            return;
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y - 1u));
        if ((ctx->status & NATIVE_N) != 0u)
            return;
    }
}

/* The $F19E-$F1D1 tail owned by pos_bits_proc. */
static void reference_pos_bits_get_do_y(SmbNeoNativeContext *ctx) {
    reference_write(ctx, 0x04u, ctx->x);
    ctx->y = reference_nz(ctx, 0x01u);
    for (;;) {
        ctx->a = reference_nz(ctx,
            reference_read(ctx, (uint16_t)(0xf19cu + ctx->y)));
        ctx->status |= NATIVE_C;
        reference_sbc(ctx,
            reference_read(ctx, (uint16_t)(uint8_t)(0xceu + ctx->x)));
        reference_write(ctx, 0x07u, ctx->a);
        ctx->a = reference_nz(ctx, 0x01u);
        reference_sbc(ctx,
            reference_read(ctx, (uint16_t)(uint8_t)(0xb5u + ctx->x)));
        ctx->x = reference_nz(ctx,
            reference_read(ctx, (uint16_t)(0xf199u + ctx->y)));
        reference_cmp(ctx, ctx->a, 0u);
        if ((ctx->status & NATIVE_N) == 0u) {
            ctx->x = reference_nz(ctx,
                reference_read(ctx, (uint16_t)(0xf19au + ctx->y)));
            reference_cmp(ctx, ctx->a, 1u);
            if ((ctx->status & NATIVE_N) != 0u) {
                ctx->a = reference_nz(ctx, 0x20u);
                reference_write(ctx, 0x06u, ctx->a);
                ctx->a = reference_nz(ctx, 0x04u);
                reference_pos_bits_diff(ctx);
            }
        }
        ctx->a = reference_nz(ctx,
            reference_read(ctx, (uint16_t)(0xf190u + ctx->x)));
        ctx->x = reference_nz(ctx, reference_read(ctx, 0x04u));
        reference_cmp(ctx, ctx->a, 0u);
        if ((ctx->status & NATIVE_Z) == 0u)
            return;
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y - 1u));
        if ((ctx->status & NATIVE_N) != 0u)
            return;
    }
}

static void reference_pos_bits_proc(SmbNeoNativeContext *ctx) {
    reference_pos_bits_get_do_x(ctx);
    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_write(ctx, 0x00u, ctx->a);
    reference_pos_bits_get_do_y(ctx);
}

static void reference_pos_bits_get_actor(SmbNeoNativeContext *ctx) {
    uint8_t saved_value;

    ctx->a = reference_nz(ctx, 0x01u);
    ctx->y = reference_nz(ctx, 0x01u);
    reference_write(ctx, 0x00u, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx, reference_read(ctx, 0x00u));
    ctx->x = reference_nz(ctx, ctx->a);
    ctx->a = reference_nz(ctx, ctx->y);
    saved_value = ctx->a;
    reference_pos_bits_proc(ctx);
    reference_asl(ctx);
    reference_asl(ctx);
    reference_asl(ctx);
    reference_asl(ctx);
    ctx->a = reference_nz(ctx,
        (uint8_t)(ctx->a | reference_read(ctx, 0x00u)));
    reference_write(ctx, 0x00u, ctx->a);
    ctx->a = reference_nz(ctx, saved_value);
    ctx->y = reference_nz(ctx, ctx->a);
    ctx->a = reference_nz(ctx, reference_read(ctx, 0x00u));
    reference_write(ctx, (uint16_t)(0x03d0u + ctx->y), ctx->a);
    ctx->x = reference_nz(ctx, reference_read(ctx, 0x08u));
}

static uint8_t pattern_byte(uint32_t seed, unsigned address) {
    uint32_t value = seed ^ ((uint32_t)address * UINT32_C(0x45d9f3b));
    value ^= value >> 16;
    value *= UINT32_C(0x27d4eb2d);
    value ^= value >> 15;
    return (uint8_t)value;
}

static void fill_ram(uint32_t seed) {
    for (unsigned address = 0; address < RAM_SIZE; ++address)
        actual_ram[address] = pattern_byte(seed, address);
    memcpy(reference_ram, actual_ram, sizeof(actual_ram));
}

static void init_prg(void) {
    for (unsigned address = 0; address < PRG_SIZE; ++address)
        test_prg[address] = pattern_byte(UINT32_C(0x5381a9cd), address);

    /* Construct the two directional bit masks as geometry, not ROM data. */
    for (unsigned index = 0; index < 8u; ++index) {
        test_prg[0xf148u - 0x8000u + index] =
            (uint8_t)(0xffu >> (index + 1u));
        test_prg[0xf150u - 0x8000u + index] =
            (uint8_t)(0xffu << (7u - index));
    }
    test_prg[0xf158u - 0x8000u] = 7u;
    test_prg[0xf159u - 0x8000u] = 15u;
    test_prg[0xf15au - 0x8000u] = 7u;
    for (unsigned index = 0; index <= 4u; ++index) {
        test_prg[0xf190u - 0x8000u + index] = index == 0u ? 0u :
            (uint8_t)((0x0fu << (4u - index)) & 0x0fu);
    }
    for (unsigned index = 5u; index <= 8u; ++index)
        test_prg[0xf190u - 0x8000u + index] =
            (uint8_t)(0x0fu >> (index - 4u));
    test_prg[0xf199u - 0x8000u] = 4u;
    test_prg[0xf19au - 0x8000u] = 0u;
    test_prg[0xf19bu - 0x8000u] = 4u;
    test_prg[0xf19cu - 0x8000u] = 0xffu;
    test_prg[0xf19du - 0x8000u] = 0u;

    /* Two verified scenery-buffer columns: $0500 and $05D0. */
    test_prg[0xab10u - 0x8000u] = 0x00u;
    test_prg[0xab11u - 0x8000u] = 0xd0u;
    test_prg[0xab12u - 0x8000u] = 0x05u;
    test_prg[0xab13u - 0x8000u] = 0x05u;
}

static SmbNeoNativeContext make_context(
    uint8_t *ram,
    uint8_t a,
    uint8_t x,
    uint8_t y,
    uint8_t status
) {
    SmbNeoNativeContext ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.a = a;
    ctx.x = x;
    ctx.y = y;
    ctx.status = status;
    ctx.ram = ram;
    ctx.prg = test_prg;
    return ctx;
}

static void fail_context(
    const char *name,
    uint64_t case_index,
    const SmbNeoNativeContext *actual,
    const SmbNeoNativeContext *reference
) {
    fprintf(stderr,
        "%s case %" PRIu64 " register mismatch:\n"
        "  actual    A=%02x X=%02x Y=%02x P=%02x susp=%u unsup=%u\n"
        "  reference A=%02x X=%02x Y=%02x P=%02x susp=%u unsup=%u\n",
        name, case_index,
        actual->a, actual->x, actual->y, actual->status,
        actual->suspended, actual->unsupported,
        reference->a, reference->x, reference->y, reference->status,
        reference->suspended, reference->unsupported);
    exit(1);
}

static void check_equal(
    const char *name,
    uint64_t case_index,
    const SmbNeoNativeContext *actual,
    const SmbNeoNativeContext *reference
) {
    if (actual->a != reference->a || actual->x != reference->x ||
        actual->y != reference->y || actual->status != reference->status ||
        actual->suspended != reference->suspended ||
        actual->unsupported != reference->unsupported ||
        actual->calls != reference->calls ||
        actual->last_symbol != reference->last_symbol)
        fail_context(name, case_index, actual, reference);
    if (memcmp(actual_ram, reference_ram, RAM_SIZE) != 0) {
        for (unsigned address = 0; address < RAM_SIZE; ++address) {
            if (actual_ram[address] != reference_ram[address]) {
                fprintf(stderr,
                    "%s case %" PRIu64 " RAM mismatch at $%04x: "
                    "actual=%02x reference=%02x\n",
                    name, case_index, address, actual_ram[address],
                    reference_ram[address]);
                exit(1);
            }
        }
    }
    ++cases_run;
}

static void test_col_box_proc(void) {
    uint64_t case_index = 0u;

    for (unsigned x = 0; x <= 12u; ++x) {
        for (unsigned y = 0; y <= 6u; ++y) {
            for (unsigned status = 0; status <= 255u; ++status) {
                uint32_t seed = UINT32_C(0x310eca71) ^
                    (uint32_t)(x << 24) ^ (uint32_t)(y << 16) ^ status;
                SmbNeoNativeContext actual;
                SmbNeoNativeContext reference;

                fill_ram(seed);
                actual = make_context(actual_ram, pattern_byte(seed, 3u),
                                      (uint8_t)x, (uint8_t)y,
                                      (uint8_t)status);
                reference = make_context(reference_ram, actual.a, actual.x,
                                         actual.y, actual.status);
                if (!vs_native_fast_col_box_proc(&actual)) {
                    fprintf(stderr, "col_box_proc rejected valid case\n");
                    exit(1);
                }
                reference_col_box_proc(&reference);
                check_equal("col_box_proc", case_index++, &actual, &reference);
            }
        }
    }
}

static void test_col_box_buffer(void) {
    uint64_t case_index = 0u;

    for (unsigned x = 0; x <= 21u; ++x) {
        for (unsigned y = 0; y <= 27u; ++y) {
            for (unsigned a = 0; a <= 255u; ++a) {
                uint32_t seed = UINT32_C(0x9a37f20b) ^
                    (uint32_t)(x << 24) ^ (uint32_t)(y << 16) ^ a;
                uint8_t status = pattern_byte(seed, 0x3f1u);
                SmbNeoNativeContext actual;
                SmbNeoNativeContext reference;

                fill_ram(seed);
                actual = make_context(actual_ram, (uint8_t)a, (uint8_t)x,
                                      (uint8_t)y, status);
                reference = make_context(reference_ram, actual.a, actual.x,
                                         actual.y, actual.status);
                if (!vs_native_fast_col_box_buffer(&actual)) {
                    fprintf(stderr, "col_box_buffer rejected valid case\n");
                    exit(1);
                }
                reference_col_box_buffer(&reference);
                check_equal("col_box_buffer", case_index++,
                            &actual, &reference);
            }
        }
    }
}

static void test_pos_bits_get_do_x(void) {
    for (uint32_t packed = 0; packed < POS_X_CASES; ++packed) {
        uint8_t edge = (uint8_t)(packed >> 8);
        uint8_t object = (uint8_t)packed;
        uint8_t x = (uint8_t)((packed * UINT32_C(13) + 7u) % 22u);
        uint32_t seed = UINT32_C(0x5832d609) ^
            packed * UINT32_C(0x9e3779b9);
        SmbNeoNativeContext actual;
        SmbNeoNativeContext reference;

        fill_ram(seed);
        actual_ram[0x071du] = edge;
        actual_ram[(uint8_t)(0x86u + x)] = object;
        actual_ram[0x071bu] = (uint8_t)(packed >> 3);
        actual_ram[(uint8_t)(0x6du + x)] = (uint8_t)(packed >> 11);
        actual_ram[0x071cu] = (uint8_t)(edge ^ 0xa5u);
        memcpy(reference_ram, actual_ram, RAM_SIZE);

        actual = make_context(actual_ram, pattern_byte(seed, 9u), x,
                              pattern_byte(seed, 10u),
                              pattern_byte(seed, 11u));
        reference = make_context(reference_ram, actual.a, actual.x,
                                 actual.y, actual.status);
        if (!vs_native_fast_pos_bits_get_do_x(&actual)) {
            fprintf(stderr, "pos_bits_get_do_x rejected valid case\n");
            exit(1);
        }
        reference_pos_bits_get_do_x(&reference);
        check_equal("pos_bits_get_do_x", packed, &actual, &reference);
    }
}

static void test_pos_bits_proc(void) {
    for (uint32_t index = 0; index < POS_PROC_CASES; ++index) {
        uint32_t seed = UINT32_C(0x06e8feb8) ^
            index * UINT32_C(0x85ebca6b);
        uint8_t x = (uint8_t)(index % 22u);
        SmbNeoNativeContext actual;
        SmbNeoNativeContext reference;

        fill_ram(seed);
        actual = make_context(actual_ram, pattern_byte(seed, 15u), x,
                              pattern_byte(seed, 16u),
                              pattern_byte(seed, 17u));
        reference = make_context(reference_ram, actual.a, actual.x,
                                 actual.y, actual.status);
        if (!vs_native_fast_pos_bits_proc(&actual)) {
            fprintf(stderr, "pos_bits_proc rejected valid case\n");
            exit(1);
        }
        reference_pos_bits_proc(&reference);
        check_equal("pos_bits_proc", index, &actual, &reference);
    }
}

static void test_pos_bits_get_actor(void) {
    for (uint32_t index = 0; index < POS_ACTOR_CASES; ++index) {
        uint32_t seed = UINT32_C(0xd8b71345) ^
            index * UINT32_C(0xc2b2ae35);
        uint8_t x = (uint8_t)(index % 21u);
        SmbNeoNativeContext actual;
        SmbNeoNativeContext reference;

        fill_ram(seed);
        actual = make_context(actual_ram, pattern_byte(seed, 21u), x,
                              pattern_byte(seed, 22u),
                              pattern_byte(seed, 23u));
        reference = make_context(reference_ram, actual.a, actual.x,
                                 actual.y, actual.status);
        if (!vs_native_fast_pos_bits_get_actor(&actual)) {
            fprintf(stderr, "pos_bits_get_actor rejected valid case\n");
            exit(1);
        }
        reference_pos_bits_get_actor(&reference);
        check_equal("pos_bits_get_actor", index, &actual, &reference);
    }
}

static void expect_rejected(
    const char *name,
    FastFunction function,
    SmbNeoNativeContext *ctx
) {
    SmbNeoNativeContext before = *ctx;
    uint8_t before_ram[RAM_SIZE];

    memcpy(before_ram, actual_ram, RAM_SIZE);
    if (function(ctx)) {
        fprintf(stderr, "%s accepted invalid input\n", name);
        exit(1);
    }
    if (memcmp(ctx, &before, sizeof(before)) != 0 ||
        memcmp(actual_ram, before_ram, RAM_SIZE) != 0) {
        fprintf(stderr, "%s mutated state before rejecting input\n", name);
        exit(1);
    }
    ++cases_run;
}

static void test_transactional_fallbacks(void) {
    static const struct {
        const char *name;
        FastFunction function;
        uint8_t invalid_x;
        uint8_t invalid_y;
    } routines[] = {
        {"col_box_proc", vs_native_fast_col_box_proc, 13u, 0u},
        {"col_box_proc_y", vs_native_fast_col_box_proc, 0u, 7u},
        {"col_box_buffer", vs_native_fast_col_box_buffer, 22u, 0u},
        {"col_box_buffer_y", vs_native_fast_col_box_buffer, 0u, 28u},
        {"pos_bits_get_do_x", vs_native_fast_pos_bits_get_do_x, 22u, 0u},
        {"pos_bits_proc", vs_native_fast_pos_bits_proc, 22u, 0u},
        {"pos_bits_get_actor", vs_native_fast_pos_bits_get_actor, 21u, 0u}
    };

    if (vs_native_fast_col_box_proc(NULL) ||
        vs_native_fast_col_box_buffer(NULL) ||
        vs_native_fast_pos_bits_get_do_x(NULL) ||
        vs_native_fast_pos_bits_proc(NULL) ||
        vs_native_fast_pos_bits_get_actor(NULL)) {
        fprintf(stderr, "NULL context accepted\n");
        exit(1);
    }
    cases_run += 5u;

    for (unsigned index = 0; index < sizeof(routines) / sizeof(routines[0]);
         ++index) {
        SmbNeoNativeContext ctx;

        fill_ram(UINT32_C(0xa5c11307) ^ index);
        ctx = make_context(actual_ram, 0x55u, routines[index].invalid_x,
                           routines[index].invalid_y, 0xa5u);
        expect_rejected(routines[index].name, routines[index].function, &ctx);

        ctx = make_context(actual_ram, 0x55u, 0u, 0u, 0xa5u);
        ctx.suspended = 1u;
        expect_rejected("suspended", routines[index].function, &ctx);

        ctx = make_context(actual_ram, 0x55u, 0u, 0u, 0xa5u);
        ctx.unsupported = 1u;
        expect_rejected("unsupported", routines[index].function, &ctx);

        ctx = make_context(actual_ram, 0x55u, 0u, 0u, 0xa5u);
        ctx.prg = NULL;
        expect_rejected("missing_prg", routines[index].function, &ctx);

        ctx = make_context(actual_ram, 0x55u, 0u, 0u, 0xa5u);
        ctx.ram = NULL;
        expect_rejected("missing_ram", routines[index].function, &ctx);
    }

    {
        SmbNeoNativeContext ctx;
        uint8_t saved_high_0 = test_prg[0xab12u - 0x8000u];
        uint8_t saved_high_1 = test_prg[0xab13u - 0x8000u];

        fill_ram(UINT32_C(0x219ea035));
        test_prg[0xab12u - 0x8000u] = 0x20u;
        test_prg[0xab13u - 0x8000u] = 0x20u;
        ctx = make_context(actual_ram, 0u, 0u, 0u, 0x24u);
        expect_rejected("col_box_buffer_pointer",
                        vs_native_fast_col_box_buffer, &ctx);
        test_prg[0xab12u - 0x8000u] = saved_high_0;
        test_prg[0xab13u - 0x8000u] = saved_high_1;
    }
}

int main(void) {
    init_prg();
    test_col_box_proc();
    test_col_box_buffer();
    test_pos_bits_get_do_x();
    test_pos_bits_proc();
    test_pos_bits_get_actor();
    test_transactional_fallbacks();
    printf("native collision/position exact tests passed: %" PRIu64
           " cases\n", cases_run);
    return 0;
}
