#include "vs_native_fast_actor_policy.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The generated direct-C functions are the instruction-shaped oracle.  They
 * contain neither original ROM bytes nor a runtime CPU dispatcher. */
void smbneo_native_fn_actor_A_00_proc_c9ba(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_oob_proc_d5bd(SmbNeoNativeContext *ctx);

enum {
    RAM_SIZE = 0x0800,
    PRG_SIZE = 0x8000,
    ACTOR_BASE_EXHAUSTIVE_CASES = 256 * 6 * 32,
    ACTOR_BASE_RANDOM_CASES = 65536,
    ACTOR_OOB_STRUCTURED_CASES = 256 * 6 * 16,
    ACTOR_OOB_RANDOM_CASES = 65536
};

static uint32_t random_state = UINT32_C(0x7a11c0de);
static uint64_t cases_run;

static uint8_t random_byte(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return (uint8_t)random_state;
}

static void random_bytes(uint8_t *data, size_t size) {
    for (size_t index = 0u; index < size; ++index)
        data[index] = random_byte();
}

static SmbNeoNativeContext context_for(
    uint8_t *ram,
    uint8_t *extra_ram,
    const uint8_t *prg
) {
    SmbNeoNativeContext ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.a = random_byte();
    ctx.x = random_byte();
    ctx.y = random_byte();
    ctx.status = random_byte();
    ctx.ram = ram;
    ctx.extra_ram = extra_ram;
    ctx.prg = prg;
    return ctx;
}

static void assert_context_equal(
    const SmbNeoNativeContext *want,
    const SmbNeoNativeContext *got,
    const uint8_t *want_ram,
    const uint8_t *got_ram,
    const uint8_t *want_extra,
    const uint8_t *got_extra
) {
    assert(want->a == got->a);
    assert(want->x == got->x);
    assert(want->y == got->y);
    assert(want->status == got->status);
    assert(want->suspended == got->suspended);
    assert(want->unsupported == got->unsupported);
    assert(memcmp(want_ram, got_ram, RAM_SIZE) == 0);
    assert(memcmp(want_extra, got_extra, RAM_SIZE) == 0);
}

static void compare_actor_base(
    uint8_t *want_ram,
    uint8_t *got_ram,
    uint8_t *want_extra,
    uint8_t *got_extra,
    const uint8_t *prg,
    uint8_t slot
) {
    SmbNeoNativeContext want = context_for(want_ram, want_extra, prg);
    SmbNeoNativeContext got;

    want.x = slot;
    want_ram[0x08u] = slot;
    memcpy(got_ram, want_ram, RAM_SIZE);
    memcpy(got_extra, want_extra, RAM_SIZE);
    got = want;
    got.ram = got_ram;
    got.extra_ram = got_extra;

    smbneo_native_fn_actor_A_00_proc_c9ba(&want);
    assert(vs_native_fast_actor_proc_base(&got));
    assert_context_equal(
        &want, &got, want_ram, got_ram, want_extra, got_extra
    );
    ++cases_run;
}

static void check_actor_base_exhaustive(const uint8_t *prg) {
    uint8_t want_ram[RAM_SIZE];
    uint8_t got_ram[RAM_SIZE];
    uint8_t want_extra[RAM_SIZE];
    uint8_t got_extra[RAM_SIZE];

    for (unsigned state = 0u; state < 256u; ++state) {
        for (unsigned slot = 0u; slot < 6u; ++slot) {
            for (unsigned variant = 0u; variant < 32u; ++variant) {
                random_bytes(want_ram, sizeof(want_ram));
                random_bytes(want_extra, sizeof(want_extra));
                want_ram[0x1eu + slot] = (uint8_t)state;
                want_ram[0x58u + slot] = (uint8_t)(
                    (variant << 3) ^ state ^ (slot * 37u)
                );
                want_ram[0x0796u + slot] = (uint8_t)(
                    variant == 0u ? 0u :
                    variant == 1u ? 14u : random_byte()
                );
                want_ram[0x16u + slot] = (uint8_t)(
                    variant == 1u ? 6u :
                    variant == 2u ? 0x2eu : random_byte()
                );
                want_ram[0x09u] = (uint8_t)variant;
                want_ram[0x076au] = (uint8_t)((variant >> 1) & 1u);
                compare_actor_base(
                    want_ram, got_ram, want_extra, got_extra,
                    prg, (uint8_t)slot
                );
            }
        }
    }
}

static void check_actor_base_random(const uint8_t *prg) {
    uint8_t want_ram[RAM_SIZE];
    uint8_t got_ram[RAM_SIZE];
    uint8_t want_extra[RAM_SIZE];
    uint8_t got_extra[RAM_SIZE];

    for (unsigned iteration = 0u;
         iteration < ACTOR_BASE_RANDOM_CASES;
         ++iteration) {
        uint8_t slot = (uint8_t)(iteration % 6u);

        random_bytes(want_ram, sizeof(want_ram));
        random_bytes(want_extra, sizeof(want_extra));
        compare_actor_base(
            want_ram, got_ram, want_extra, got_extra, prg, slot
        );
    }
}

static void compare_actor_oob(
    uint8_t *want_ram,
    uint8_t *got_ram,
    uint8_t *want_extra,
    uint8_t *got_extra,
    const uint8_t *prg,
    uint8_t slot
) {
    SmbNeoNativeContext want = context_for(want_ram, want_extra, prg);
    SmbNeoNativeContext got;

    want.x = slot;
    memcpy(got_ram, want_ram, RAM_SIZE);
    memcpy(got_extra, want_extra, RAM_SIZE);
    got = want;
    got.ram = got_ram;
    got.extra_ram = got_extra;

    smbneo_native_fn_actor_oob_proc_d5bd(&want);
    assert(vs_native_fast_actor_oob_proc(&got));
    assert_context_equal(
        &want, &got, want_ram, got_ram, want_extra, got_extra
    );
    ++cases_run;
}

static void check_actor_oob_structured(const uint8_t *prg) {
    static const uint8_t low_boundaries[] = {
        0x00u, 0x01u, 0x37u, 0x38u, 0x47u, 0x48u, 0x7fu, 0x80u,
        0xb7u, 0xb8u, 0xc7u, 0xc8u, 0xfeu, 0xffu, 0x10u, 0xf0u
    };
    uint8_t want_ram[RAM_SIZE];
    uint8_t got_ram[RAM_SIZE];
    uint8_t want_extra[RAM_SIZE];
    uint8_t got_extra[RAM_SIZE];

    for (unsigned id = 0u; id < 256u; ++id) {
        for (unsigned slot = 0u; slot < 6u; ++slot) {
            for (unsigned boundary = 0u;
                 boundary < sizeof(low_boundaries) / sizeof(low_boundaries[0]);
                 ++boundary) {
                random_bytes(want_ram, sizeof(want_ram));
                random_bytes(want_extra, sizeof(want_extra));
                want_ram[0x16u + slot] = (uint8_t)id;
                want_ram[0x071cu] = low_boundaries[boundary];
                want_ram[0x071du] = low_boundaries[
                    (boundary + 7u) %
                    (sizeof(low_boundaries) / sizeof(low_boundaries[0]))
                ];
                want_ram[0x071au] = (uint8_t)(boundary * 17u);
                want_ram[0x071bu] = (uint8_t)(boundary * 17u + 1u);
                want_ram[0x87u + slot] = low_boundaries[
                    (boundary + id) & 15u
                ];
                want_ram[0x6eu + slot] = (uint8_t)(
                    boundary * 17u + ((id >> 4) & 3u)
                );
                want_ram[0x1eu + slot] = (uint8_t)(
                    boundary == 5u ? 5u : random_byte()
                );
                compare_actor_oob(
                    want_ram, got_ram, want_extra, got_extra,
                    prg, (uint8_t)slot
                );
            }
        }
    }
}

static void check_actor_oob_random(const uint8_t *prg) {
    uint8_t want_ram[RAM_SIZE];
    uint8_t got_ram[RAM_SIZE];
    uint8_t want_extra[RAM_SIZE];
    uint8_t got_extra[RAM_SIZE];

    for (unsigned iteration = 0u;
         iteration < ACTOR_OOB_RANDOM_CASES;
         ++iteration) {
        uint8_t slot = (uint8_t)(iteration % 6u);

        random_bytes(want_ram, sizeof(want_ram));
        random_bytes(want_extra, sizeof(want_extra));
        compare_actor_oob(
            want_ram, got_ram, want_extra, got_extra, prg, slot
        );
    }
}

static void assert_same_object(
    const SmbNeoNativeContext *want,
    const SmbNeoNativeContext *got,
    const uint8_t *want_ram,
    const uint8_t *got_ram
) {
    assert(memcmp(want, got, sizeof(*want)) == 0);
    assert(memcmp(want_ram, got_ram, RAM_SIZE) == 0);
}

static void check_fallbacks(const uint8_t *prg) {
    uint8_t ram[RAM_SIZE];
    uint8_t before_ram[RAM_SIZE];
    uint8_t extra_ram[RAM_SIZE];

    assert(!vs_native_fast_actor_proc_base(NULL));
    assert(!vs_native_fast_actor_oob_proc(NULL));

    for (unsigned kind = 0u; kind < 6u; ++kind) {
        SmbNeoNativeContext ctx;
        SmbNeoNativeContext before;

        random_bytes(ram, sizeof(ram));
        random_bytes(extra_ram, sizeof(extra_ram));
        ctx = context_for(ram, extra_ram, prg);
        ctx.x = 2u;
        ram[0x08u] = 2u;
        switch (kind) {
        case 0u: ctx.ram = NULL; break;
        case 1u: ctx.prg = NULL; break;
        case 2u: ctx.x = 6u; break;
        case 3u: ram[0x08u] = 1u; break;
        case 4u: ctx.suspended = 1u; break;
        default: ctx.unsupported = 1u; break;
        }
        memcpy(before_ram, ram, sizeof(ram));
        before = ctx;
        assert(!vs_native_fast_actor_proc_base(&ctx));
        assert_same_object(&before, &ctx, before_ram, ram);
    }

    for (unsigned kind = 0u; kind < 4u; ++kind) {
        SmbNeoNativeContext ctx;
        SmbNeoNativeContext before;

        random_bytes(ram, sizeof(ram));
        random_bytes(extra_ram, sizeof(extra_ram));
        ctx = context_for(ram, extra_ram, prg);
        ctx.x = 2u;
        switch (kind) {
        case 0u: ctx.ram = NULL; break;
        case 1u: ctx.x = 6u; break;
        case 2u: ctx.suspended = 1u; break;
        default: ctx.unsupported = 1u; break;
        }
        memcpy(before_ram, ram, sizeof(ram));
        before = ctx;
        assert(!vs_native_fast_actor_oob_proc(&ctx));
        assert_same_object(&before, &ctx, before_ram, ram);
    }
}

int main(void) {
    static uint8_t prg[PRG_SIZE];
    static uint8_t prg_before[PRG_SIZE];

    random_bytes(prg, sizeof(prg));
    memcpy(prg_before, prg, sizeof(prg));
    check_actor_base_exhaustive(prg);
    check_actor_base_random(prg);
    check_actor_oob_structured(prg);
    check_actor_oob_random(prg);
    check_fallbacks(prg);
    assert(memcmp(prg, prg_before, sizeof(prg)) == 0);
    assert(cases_run ==
        ACTOR_BASE_EXHAUSTIVE_CASES + ACTOR_BASE_RANDOM_CASES +
        ACTOR_OOB_STRUCTURED_CASES + ACTOR_OOB_RANDOM_CASES);
    printf(
        "VS native actor policy: %" PRIu64
        " exact instruction-oracle cases and 10 no-mutation fallbacks passed\n",
        cases_run
    );
    return 0;
}
