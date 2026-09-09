#include "vs_native_fast_actor.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The generated direct-C routine is the byte-accurate model.  This test uses
 * deterministic synthetic PRG bytes, never an original ROM. */
void smbneo_native_fn_render_actor_e7da(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_actor_chr_pair_eb07(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_chr_pair_eb0f(SmbNeoNativeContext *ctx);

static uint32_t random_state = 0x51a7c3e9u;

static uint8_t random_byte(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return (uint8_t)random_state;
}

static void random_bytes(uint8_t *data, size_t size) {
    for (size_t i = 0u; i < size; ++i)
        data[i] = random_byte();
}

static SmbNeoNativeContext context_for(uint8_t *ram, const uint8_t *prg) {
    SmbNeoNativeContext ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.a = random_byte();
    ctx.x = random_byte();
    ctx.y = random_byte();
    ctx.status = random_byte();
    ctx.ram = ram;
    ctx.prg = prg;
    return ctx;
}

static void assert_machine_equal(
    const SmbNeoNativeContext *want,
    const SmbNeoNativeContext *got,
    const uint8_t *want_ram,
    const uint8_t *got_ram
) {
    assert(want->a == got->a);
    assert(want->x == got->x);
    assert(want->y == got->y);
    assert(want->status == got->status);
    assert(want->suspended == got->suspended);
    assert(want->unsupported == got->unsupported);
    assert(memcmp(want_ram, got_ram, 2048u) == 0);
}

static void check_pair_entries(const uint8_t *prg) {
    uint8_t want_ram[2048];
    uint8_t got_ram[2048];

    for (unsigned seed = 0u; seed < 32768u; ++seed) {
        SmbNeoNativeContext want;
        SmbNeoNativeContext got;

        random_bytes(want_ram, sizeof(want_ram));
        memcpy(got_ram, want_ram, sizeof(got_ram));
        want = context_for(want_ram, prg);
        got = want;
        got.ram = got_ram;
        smbneo_native_fn_render_chr_pair_eb0f(&want);
        assert(vs_native_fast_render_chr_pair(&got));
        assert_machine_equal(&want, &got, want_ram, got_ram);

        random_bytes(want_ram, sizeof(want_ram));
        memcpy(got_ram, want_ram, sizeof(got_ram));
        want = context_for(want_ram, prg);
        got = want;
        got.ram = got_ram;
        smbneo_native_fn_render_actor_chr_pair_eb07(&want);
        assert(vs_native_fast_render_actor_chr_pair(&got));
        assert_machine_equal(&want, &got, want_ram, got_ram);
    }
}

static void model_actor_tiles(SmbNeoNativeContext *ctx) {
    ctx->y = ctx->ram[0xeb];
    ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) |
        (ctx->y & NATIVE_N) | (ctx->y == 0u ? NATIVE_Z : 0u));
    for (unsigned pair = 0u; pair < 3u; ++pair)
        smbneo_native_fn_render_actor_chr_pair_eb07(ctx);
}

static void check_actor_tiles(const uint8_t *prg) {
    uint8_t want_ram[2048];
    uint8_t got_ram[2048];

    for (unsigned seed = 0u; seed < 32768u; ++seed) {
        SmbNeoNativeContext want;
        SmbNeoNativeContext got;

        random_bytes(want_ram, sizeof(want_ram));
        memcpy(got_ram, want_ram, sizeof(got_ram));
        want = context_for(want_ram, prg);
        got = want;
        got.ram = got_ram;
        model_actor_tiles(&want);
        assert(vs_native_fast_render_actor_tiles(&got));
        assert_machine_equal(&want, &got, want_ram, got_ram);
    }
}

static void check_render_actor(const uint8_t *prg) {
    uint8_t want_ram[2048];
    uint8_t got_ram[2048];

    for (unsigned seed = 0u; seed < 65536u; ++seed) {
        unsigned slot = seed % 6u;
        SmbNeoNativeContext want;
        SmbNeoNativeContext got;

        random_bytes(want_ram, sizeof(want_ram));
        want_ram[8] = (uint8_t)slot;
        want_ram[0x06e5u + slot] = (uint8_t)((random_byte() % 59u) * 4u);
        want_ram[0x03d1] &= 0x1fu;
        /* Bias IDs toward every special animation/rendering branch while
         * retaining arbitrary values for table-boundary coverage. */
        if ((seed & 3u) != 0u) {
            static const uint8_t ids[] = {
                0, 1, 2, 3, 5, 6, 7, 8, 0x0c, 0x0d, 0x11, 0x12,
                0x15, 0x17, 0x18, 0x32, 0x33, 0x35
            };
            want_ram[0x16u + slot] = ids[seed % (sizeof(ids) / sizeof(ids[0]))];
        }
        want_ram[0x036a] %= 3u;
        memcpy(got_ram, want_ram, sizeof(got_ram));
        want = context_for(want_ram, prg);
        want.x = (uint8_t)slot;
        got = want;
        got.ram = got_ram;

        smbneo_native_fn_render_actor_e7da(&want);
        assert(vs_native_fast_render_actor(&got));
        assert_machine_equal(&want, &got, want_ram, got_ram);
    }
}

static void check_fallbacks(const uint8_t *prg) {
    uint8_t ram[2048];
    uint8_t before_ram[2048];

    for (unsigned kind = 0u; kind < 5u; ++kind) {
        SmbNeoNativeContext ctx;
        SmbNeoNativeContext before;

        random_bytes(ram, sizeof(ram));
        ctx = context_for(ram, prg);
        ctx.x = 2u;
        ram[8] = 2u;
        ram[0x06e7] = 40u;
        ram[0x03d1] = 0u;
        switch (kind) {
        case 0u: ctx.x = 6u; break;
        case 1u: ram[8] = 1u; break;
        case 2u: ram[0x06e7] = 41u; break;
        case 3u: ram[0x06e7] = 236u; break;
        default: ram[0x03d1] = 0x20u; break;
        }
        before = ctx;
        memcpy(before_ram, ram, sizeof(ram));
        assert(!vs_native_fast_render_actor(&ctx));
        assert(memcmp(&ctx, &before, sizeof(ctx)) == 0);
        assert(memcmp(ram, before_ram, sizeof(ram)) == 0);
    }
}

int main(void) {
    static uint8_t prg[32768];

    random_bytes(prg, sizeof(prg));
    check_pair_entries(prg);
    check_actor_tiles(prg);
    check_render_actor(prg);
    check_fallbacks(prg);
    puts("VS native actor fast paths: 163840 exact synthetic/model cases and "
         "5 no-mutation fallbacks passed");
    return 0;
}
