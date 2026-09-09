#include "vs_native_fast_meta.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void smbneo_native_fn_meta_render_area_8aaf(SmbNeoNativeContext *ctx);

enum { RAM_SIZE = 0x0800, PRG_SIZE = 0x8000, CASES = 131072 };
static uint32_t rng = UINT32_C(0x8aaf13c0);

static uint8_t next_byte(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (uint8_t)rng;
}

static void fill(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; ++i)
        data[i] = next_byte();
}

static void compare_case(unsigned index) {
    static uint8_t want_ram[RAM_SIZE], got_ram[RAM_SIZE];
    static uint8_t prg[PRG_SIZE];
    SmbNeoNativeContext want = {0}, got;

    fill(want_ram, sizeof(want_ram));
    fill(prg, sizeof(prg));
    want_ram[0x0340u] = 0u;
    want_ram[0x0726u] = (uint8_t)index;
    want_ram[0x071fu] = (uint8_t)(index >> 1);
    want_ram[0x0721u] = (uint8_t)(index * 31u);
    for (unsigned palette = 0u; palette < 4u; ++palette) {
        uint16_t address = (uint16_t)(0x9000u +
            ((index * 257u + palette * 0x123u) & 0x6e00u));
        prg[0x0d09u + palette] = (uint8_t)address;
        prg[0x0d0du + palette] = (uint8_t)(address >> 8);
    }
    memcpy(got_ram, want_ram, sizeof(got_ram));
    want.a = next_byte();
    want.x = next_byte();
    want.y = next_byte();
    want.status = next_byte();
    want.ram = want_ram;
    want.prg = prg;
    got = want;
    got.ram = got_ram;

    smbneo_native_fn_meta_render_area_8aaf(&want);
    assert(vs_native_fast_meta_render_area(&got));
    assert(want.a == got.a);
    assert(want.x == got.x);
    assert(want.y == got.y);
    assert(want.status == got.status);
    assert(want.suspended == got.suspended);
    assert(want.unsupported == got.unsupported);
    assert(memcmp(want_ram, got_ram, sizeof(want_ram)) == 0);
}

static void fallback_cases(void) {
    uint8_t ram[RAM_SIZE], before_ram[RAM_SIZE], prg[PRG_SIZE];
    SmbNeoNativeContext ctx = {0}, before;

    fill(ram, sizeof(ram));
    fill(prg, sizeof(prg));
    ctx.ram = ram;
    ctx.prg = prg;
    assert(!vs_native_fast_meta_render_area(NULL));
    before = ctx;
    ctx.ram = NULL;
    assert(!vs_native_fast_meta_render_area(&ctx));
    ctx = before;
    ctx.prg = NULL;
    assert(!vs_native_fast_meta_render_area(&ctx));
    ctx = before;
    ram[0x0340u] = 1u;
    memcpy(before_ram, ram, sizeof(ram));
    assert(!vs_native_fast_meta_render_area(&ctx));
    assert(memcmp(ram, before_ram, sizeof(ram)) == 0);
    ram[0x0340u] = 0u;
    prg[0x0d09u] = 0u;
    prg[0x0d0du] = 0u;
    memcpy(before_ram, ram, sizeof(ram));
    assert(!vs_native_fast_meta_render_area(&ctx));
    assert(memcmp(ram, before_ram, sizeof(ram)) == 0);
}

int main(void) {
    for (unsigned i = 0; i < CASES; ++i)
        compare_case(i);
    fallback_cases();
    printf("VS native metatile compositor: %u exact cases passed\n", CASES);
    return 0;
}
