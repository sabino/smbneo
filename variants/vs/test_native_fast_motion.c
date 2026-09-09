#include "vs_native_fast_motion.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    RAM_SIZE = 0x0800,
    EXTRA_RAM_SIZE = 0x0800,
    MOTION_X_DO_CASES = 256 * 256,
    MOTION_GRAVITY_CASES = 32768,
    WRAPPER_CASES = 2048
};

typedef void (*MotionFunction)(SmbNeoNativeContext *ctx);

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

static void reference_rol(SmbNeoNativeContext *ctx) {
    uint8_t value = ctx->a;
    uint8_t carry_in = (ctx->status & NATIVE_C) != 0u ? 1u : 0u;
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 0x80u) != 0u ? NATIVE_C : 0u)
    );
    ctx->a = reference_nz(ctx, (uint8_t)((value << 1) | carry_in));
}

static void reference_ror(SmbNeoNativeContext *ctx) {
    uint8_t value = ctx->a;
    uint8_t carry_in = (ctx->status & NATIVE_C) != 0u ? 0x80u : 0u;
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 1u) != 0u ? NATIVE_C : 0u)
    );
    ctx->a = reference_nz(ctx, (uint8_t)((value >> 1) | carry_in));
}

/* Instruction-shaped oracle for the horizontal fixed-point routine. */
static void reference_motion_x_do(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;
    uint8_t saved_value;

    ctx->a = reference_nz(ctx, ram[(uint8_t)(0x57u + ctx->x)]);
    reference_asl(ctx);
    reference_asl(ctx);
    reference_asl(ctx);
    reference_asl(ctx);
    ram[0x01u] = ctx->a;
    ctx->a = reference_nz(ctx, ram[(uint8_t)(0x57u + ctx->x)]);
    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_lsr(ctx);
    reference_cmp(ctx, ctx->a, 0x08u);
    if ((ctx->status & NATIVE_C) != 0u)
        ctx->a = reference_nz(ctx, (uint8_t)(ctx->a | 0xf0u));
    ram[0x00u] = ctx->a;
    ctx->y = reference_nz(ctx, 0u);
    reference_cmp(ctx, ctx->a, 0u);
    if ((ctx->status & NATIVE_N) != 0u)
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y - 1u));
    ram[0x02u] = ctx->y;

    ctx->a = reference_nz(ctx, ram[0x0400u + ctx->x]);
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx, ram[0x01u]);
    ram[0x0400u + ctx->x] = ctx->a;
    ctx->a = reference_nz(ctx, 0u);
    reference_rol(ctx);
    saved_value = ctx->a;
    reference_ror(ctx);
    ctx->a = reference_nz(ctx, ram[(uint8_t)(0x86u + ctx->x)]);
    reference_adc(ctx, ram[0x00u]);
    ram[(uint8_t)(0x86u + ctx->x)] = ctx->a;
    ctx->a = reference_nz(ctx, ram[(uint8_t)(0x6du + ctx->x)]);
    reference_adc(ctx, ram[0x02u]);
    ram[(uint8_t)(0x6du + ctx->x)] = ctx->a;
    ctx->a = reference_nz(ctx, saved_value);
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx, ram[0x00u]);
}

static void reference_motion_x(SmbNeoNativeContext *ctx) {
    ctx->x = reference_nz(ctx, (uint8_t)(ctx->x + 1u));
    reference_motion_x_do(ctx);
    if (ctx->suspended != 0u || ctx->unsupported != 0u)
        return;
    ctx->x = reference_nz(ctx, ctx->ram[0x08u]);
}

static void reference_motion_x_player(SmbNeoNativeContext *ctx) {
    ctx->a = reference_nz(ctx, ctx->ram[0x070eu]);
    if (ctx->a == 0u) {
        ctx->x = reference_nz(ctx, ctx->a);
        reference_motion_x_do(ctx);
    }
}

/* Instruction-shaped oracle for the shared vertical integrator. */
static void reference_motion_gravity(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;
    uint8_t saved_value = ctx->a;

    ctx->a = reference_nz(ctx, ram[0x0416u + ctx->x]);
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx, ram[0x0433u + ctx->x]);
    ram[0x0416u + ctx->x] = ctx->a;
    ctx->y = reference_nz(ctx, 0u);
    ctx->a = reference_nz(ctx, ram[(uint8_t)(0x9fu + ctx->x)]);
    if ((ctx->status & NATIVE_N) != 0u)
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y - 1u));
    ram[0x07u] = ctx->y;
    reference_adc(ctx, ram[(uint8_t)(0xceu + ctx->x)]);
    ram[(uint8_t)(0xceu + ctx->x)] = ctx->a;
    ctx->a = reference_nz(ctx, ram[(uint8_t)(0xb5u + ctx->x)]);
    reference_adc(ctx, ram[0x07u]);
    ram[(uint8_t)(0xb5u + ctx->x)] = ctx->a;

    ctx->a = reference_nz(ctx, ram[0x0433u + ctx->x]);
    ctx->status &= (uint8_t)~NATIVE_C;
    reference_adc(ctx, ram[0x00u]);
    ram[0x0433u + ctx->x] = ctx->a;
    ctx->a = reference_nz(ctx, ram[(uint8_t)(0x9fu + ctx->x)]);
    reference_adc(ctx, 0u);
    ram[(uint8_t)(0x9fu + ctx->x)] = ctx->a;
    reference_cmp(ctx, ctx->a, ram[0x02u]);
    if ((ctx->status & NATIVE_N) == 0u) {
        ctx->a = reference_nz(ctx, ram[0x0433u + ctx->x]);
        reference_cmp(ctx, ctx->a, 0x80u);
        if ((ctx->status & NATIVE_C) != 0u) {
            ctx->a = reference_nz(ctx, ram[0x02u]);
            ram[(uint8_t)(0x9fu + ctx->x)] = ctx->a;
            ctx->a = reference_nz(ctx, 0u);
            ram[0x0433u + ctx->x] = ctx->a;
        }
    }

    ctx->a = reference_nz(ctx, saved_value);
    if ((ctx->status & NATIVE_Z) != 0u)
        return;
    ctx->a = reference_nz(ctx, ram[0x02u]);
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->y = reference_nz(ctx, ctx->a);
    ctx->y = reference_nz(ctx, (uint8_t)(ctx->y + 1u));
    ram[0x07u] = ctx->y;
    ctx->a = reference_nz(ctx, ram[0x0433u + ctx->x]);
    ctx->status |= NATIVE_C;
    reference_sbc(ctx, ram[0x01u]);
    ram[0x0433u + ctx->x] = ctx->a;
    ctx->a = reference_nz(ctx, ram[(uint8_t)(0x9fu + ctx->x)]);
    reference_sbc(ctx, 0u);
    ram[(uint8_t)(0x9fu + ctx->x)] = ctx->a;
    reference_cmp(ctx, ctx->a, ram[0x07u]);
    if ((ctx->status & NATIVE_N) == 0u)
        return;
    ctx->a = reference_nz(ctx, ram[0x0433u + ctx->x]);
    reference_cmp(ctx, ctx->a, 0x80u);
    if ((ctx->status & NATIVE_C) != 0u)
        return;
    ctx->a = reference_nz(ctx, ram[0x07u]);
    ram[(uint8_t)(0x9fu + ctx->x)] = ctx->a;
    ctx->a = reference_nz(ctx, 0xffu);
    ram[0x0433u + ctx->x] = ctx->a;
}

static void reference_motion_gravity_do(SmbNeoNativeContext *ctx) {
    ctx->ram[0x02u] = ctx->a;
    ctx->a = reference_nz(ctx, 0u);
    reference_motion_gravity(ctx);
}

static void reference_motion_fall(SmbNeoNativeContext *ctx) {
    ctx->ram[0x00u] = ctx->y;
    ctx->x = reference_nz(ctx, (uint8_t)(ctx->x + 1u));
    reference_motion_gravity_do(ctx);
    if (ctx->suspended != 0u || ctx->unsupported != 0u)
        return;
    ctx->x = reference_nz(ctx, ctx->ram[0x08u]);
}

static void reference_motion_fall_fast(SmbNeoNativeContext *ctx) {
    ctx->y = reference_nz(ctx, 0x7fu);
    ctx->a = reference_nz(ctx, 0x02u);
    reference_motion_fall(ctx);
}

static void reference_motion_fall_slow(SmbNeoNativeContext *ctx) {
    ctx->y = reference_nz(ctx, 0x0fu);
    ctx->a = reference_nz(ctx, 0x02u);
    reference_motion_fall(ctx);
}

static void reference_motion_fall_mid(SmbNeoNativeContext *ctx) {
    ctx->y = reference_nz(ctx, 0x1cu);
    ctx->a = reference_nz(ctx, 0x03u);
    reference_motion_fall(ctx);
}

static uint8_t pattern_byte(uint32_t seed, unsigned address) {
    uint32_t value = seed ^ (uint32_t)address * UINT32_C(0x45d9f3b);
    value ^= value >> 16;
    value *= UINT32_C(0x45d9f3b);
    value ^= value >> 16;
    return (uint8_t)value;
}

static void prepare_context(
    SmbNeoNativeContext *ctx,
    uint8_t *ram,
    uint8_t *extra_ram,
    uint32_t seed
) {
    unsigned address;
    memset(ctx, 0, sizeof(*ctx));
    for (address = 0; address < RAM_SIZE; ++address)
        ram[address] = pattern_byte(seed, address);
    for (address = 0; address < EXTRA_RAM_SIZE; ++address)
        extra_ram[address] = pattern_byte(seed ^ UINT32_C(0xa5a5f00d), address);
    ctx->a = pattern_byte(seed, 0x8001u);
    ctx->x = pattern_byte(seed, 0x8002u);
    ctx->y = pattern_byte(seed, 0x8003u);
    ctx->status = pattern_byte(seed, 0x8004u);
    ctx->suspended = (uint8_t)((seed & 0x3ffu) == 0x155u);
    ctx->unsupported = (uint8_t)((seed & 0x7ffu) == 0x2aau);
    ctx->calls = seed ^ UINT32_C(0x31415926);
    ctx->last_symbol = (uint16_t)(seed ^ UINT32_C(0x27182818));
    ctx->ram = ram;
    ctx->extra_ram = extra_ram;
}

static int compare_case(
    const char *suite,
    uint32_t ordinal,
    MotionFunction reference_function,
    MotionFunction fast_function,
    SmbNeoNativeContext *reference,
    SmbNeoNativeContext *actual,
    uint8_t *reference_ram,
    uint8_t *actual_ram,
    uint8_t *reference_extra,
    uint8_t *actual_extra
) {
    unsigned address;
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

    for (address = 0; address < RAM_SIZE; ++address) {
        if (reference_ram[address] != actual_ram[address]) {
            fprintf(stderr, "%s case=%" PRIu32 " RAM[%04x] "
                    "expected=%02x actual=%02x\n", suite, ordinal, address,
                    reference_ram[address], actual_ram[address]);
            return 1;
        }
    }
    if (memcmp(reference_extra, actual_extra, EXTRA_RAM_SIZE) != 0) {
        fprintf(stderr, "%s case=%" PRIu32 " modified extra RAM\n",
                suite, ordinal);
        return 1;
    }
    return 0;
}

static int run_one(
    const char *suite,
    uint32_t ordinal,
    MotionFunction reference_function,
    MotionFunction fast_function,
    uint8_t forced_x,
    int force_x
) {
    uint8_t reference_ram[RAM_SIZE];
    uint8_t actual_ram[RAM_SIZE];
    uint8_t reference_extra[EXTRA_RAM_SIZE];
    uint8_t actual_extra[EXTRA_RAM_SIZE];
    SmbNeoNativeContext reference;
    SmbNeoNativeContext actual;
    uint32_t seed = ordinal * UINT32_C(0x9e3779b9) + UINT32_C(0x7f4a7c15);

    prepare_context(&reference, reference_ram, reference_extra, seed);
    actual = reference;
    memcpy(actual_ram, reference_ram, sizeof(actual_ram));
    memcpy(actual_extra, reference_extra, sizeof(actual_extra));
    actual.ram = actual_ram;
    actual.extra_ram = actual_extra;
    if (force_x) {
        reference.x = forced_x;
        actual.x = forced_x;
    }
    return compare_case(
        suite, ordinal, reference_function, fast_function,
        &reference, &actual, reference_ram, actual_ram,
        reference_extra, actual_extra
    );
}

static int test_motion_x_do_grid(void) {
    unsigned object_index;
    unsigned speed;
    for (object_index = 0; object_index < 256u; ++object_index) {
        for (speed = 0; speed < 256u; ++speed) {
            uint32_t ordinal = (uint32_t)(object_index * 256u + speed);
            uint8_t reference_ram[RAM_SIZE];
            uint8_t actual_ram[RAM_SIZE];
            uint8_t reference_extra[EXTRA_RAM_SIZE];
            uint8_t actual_extra[EXTRA_RAM_SIZE];
            SmbNeoNativeContext reference;
            SmbNeoNativeContext actual;
            uint32_t seed = ordinal * UINT32_C(0x9e3779b9) +
                UINT32_C(0x6a09e667);

            prepare_context(&reference, reference_ram, reference_extra, seed);
            reference.x = (uint8_t)object_index;
            reference_ram[(uint8_t)(0x57u + reference.x)] = (uint8_t)speed;
            actual = reference;
            memcpy(actual_ram, reference_ram, sizeof(actual_ram));
            memcpy(actual_extra, reference_extra, sizeof(actual_extra));
            actual.ram = actual_ram;
            actual.extra_ram = actual_extra;
            if (compare_case(
                    "motion-x-do-grid", ordinal,
                    reference_motion_x_do, vs_native_fast_motion_x_do,
                    &reference, &actual, reference_ram, actual_ram,
                    reference_extra, actual_extra
                ) != 0)
                return 1;
        }
    }
    return 0;
}

static int test_gravity_states(void) {
    unsigned index;
    static const uint8_t caps[] = {0u, 1u, 2u, 3u, 0x7fu, 0x80u, 0xffu};
    for (index = 0; index < MOTION_GRAVITY_CASES; ++index) {
        uint8_t reference_ram[RAM_SIZE];
        uint8_t actual_ram[RAM_SIZE];
        uint8_t reference_extra[EXTRA_RAM_SIZE];
        uint8_t actual_extra[EXTRA_RAM_SIZE];
        SmbNeoNativeContext reference;
        SmbNeoNativeContext actual;
        uint32_t seed = index * UINT32_C(0x85ebca6b) + UINT32_C(0xbb67ae85);
        uint8_t object_index = (uint8_t)index;

        prepare_context(&reference, reference_ram, reference_extra, seed);
        reference.x = object_index;
        reference.a = (index & 1u) != 0u
            ? (uint8_t)(1u + (index >> 8)) : 0u;
        reference_ram[0x02u] = caps[index % (sizeof(caps) / sizeof(caps[0]))];
        reference_ram[(uint8_t)(0x9fu + object_index)] = (uint8_t)(index >> 5);
        reference_ram[0x0433u + object_index] = (uint8_t)(index * 29u);
        actual = reference;
        memcpy(actual_ram, reference_ram, sizeof(actual_ram));
        memcpy(actual_extra, reference_extra, sizeof(actual_extra));
        actual.ram = actual_ram;
        actual.extra_ram = actual_extra;
        if (compare_case(
                "motion-gravity-states", index,
                reference_motion_gravity, vs_native_fast_motion_gravity,
                &reference, &actual, reference_ram, actual_ram,
                reference_extra, actual_extra
            ) != 0)
            return 1;
    }
    return 0;
}

static int test_wrappers(void) {
    static const struct {
        const char *name;
        MotionFunction reference_function;
        MotionFunction fast_function;
    } wrappers[] = {
        {"motion-x", reference_motion_x, vs_native_fast_motion_x},
        {"motion-x-player", reference_motion_x_player,
            vs_native_fast_motion_x_player},
        {"motion-fall-fast", reference_motion_fall_fast,
            vs_native_fast_motion_fall_fast},
        {"motion-fall-slow", reference_motion_fall_slow,
            vs_native_fast_motion_fall_slow},
        {"motion-fall-mid", reference_motion_fall_mid,
            vs_native_fast_motion_fall_mid},
        {"motion-fall", reference_motion_fall, vs_native_fast_motion_fall},
        {"motion-gravity-do", reference_motion_gravity_do,
            vs_native_fast_motion_gravity_do}
    };
    unsigned wrapper;
    unsigned index;

    for (wrapper = 0; wrapper < sizeof(wrappers) / sizeof(wrappers[0]);
         ++wrapper) {
        for (index = 0; index < WRAPPER_CASES; ++index) {
            uint32_t ordinal = UINT32_C(0x100000) * (wrapper + 1u) + index;
            if (run_one(
                    wrappers[wrapper].name, ordinal,
                    wrappers[wrapper].reference_function,
                    wrappers[wrapper].fast_function,
                    (uint8_t)index, 1
                ) != 0)
                return 1;
        }
    }
    return 0;
}

static int test_null_contract(void) {
    static MotionFunction functions[] = {
        vs_native_fast_motion_x,
        vs_native_fast_motion_x_player,
        vs_native_fast_motion_x_do,
        vs_native_fast_motion_fall_fast,
        vs_native_fast_motion_fall_slow,
        vs_native_fast_motion_fall_mid,
        vs_native_fast_motion_fall,
        vs_native_fast_motion_gravity_do,
        vs_native_fast_motion_gravity
    };
    unsigned index;
    for (index = 0; index < sizeof(functions) / sizeof(functions[0]); ++index) {
        SmbNeoNativeContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.a = 0x12u;
        ctx.x = 0x34u;
        ctx.y = 0x56u;
        ctx.status = 0xa5u;
        functions[index](NULL);
        functions[index](&ctx);
        if (ctx.unsupported != 1u || ctx.a != 0x12u || ctx.x != 0x34u ||
            ctx.y != 0x56u || ctx.status != 0xa5u) {
            fprintf(stderr, "null contract failed for function %u\n", index);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    if (test_motion_x_do_grid() != 0 ||
        test_gravity_states() != 0 ||
        test_wrappers() != 0 ||
        test_null_contract() != 0)
        return 1;
    printf("native fast motion: %" PRIu64
           " exact ROM-free cases passed (including every X/speed pair)\n",
           cases_run);
    return 0;
}
