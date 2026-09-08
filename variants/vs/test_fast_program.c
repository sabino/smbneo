/* Compare semantic helpers with the original generated instruction functions. */
#include "vs_cpu.h"
#include <stdio.h>
#include "test_check.h"
void vs_program_reference(VsCpu *, unsigned);

static unsigned cases;
static void compare(const uint8_t *prg, uint16_t entry, unsigned x, unsigned y, unsigned pattern) {
    VsBus reference, optimized;
    VsCpu want, got;
    vs_bus_init(&reference, prg, prg + 32768);
    for (unsigned i = 0; i < 2048; ++i) reference.ram[i] = (uint8_t)(i * 13 + pattern);
    reference.ram[3] = (uint8_t)((pattern & ~2u) | ((x & 256u) ? 2u : 0u));
    vs_cpu_init(&want, &reference);
    want.pc = entry; want.x = (uint8_t)x; want.y = (uint8_t)y;
    want.a = (uint8_t)pattern; want.p = (uint8_t)(x ^ y ^ pattern);
    want.s = (uint8_t)(pattern + y);
    vs_push(&want, 0xff); vs_push(&want, 0xfe);
    optimized = reference; got = want; got.bus = &optimized;
    vs_program_reference(&want, 200000);
    vs_program_run(&got, 200000);
    assert(want.pc == 0xffff && want.fault && got.pc == want.pc && got.fault);
    assert(want.a == got.a && want.x == got.x && want.y == got.y && want.s == got.s && want.p == got.p);
    assert(want.idle == got.idle && want.in_nmi == got.in_nmi && want.yielded == got.yielded);
    assert(memcmp(reference.ram, optimized.ram, sizeof(reference.ram)) == 0);
    assert(memcmp(reference.extra_ram, optimized.extra_ram, sizeof(reference.extra_ram)) == 0);
    ++cases;
}
int main(int argc, char **argv) {
    static uint8_t data[49152];
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    assert(fseek(f, 16, SEEK_SET) == 0);
    assert(fread(data, 1, sizeof(data), f) == sizeof(data)); fclose(f);
    for (unsigned y = 0; y < 256; ++y) for (unsigned x = 0; x < 512; ++x)
        compare(data, 0xf1e7, x, y, (x * 17 + y * 31) & 255);
    for (unsigned y = 0; y < 256; y += 4) for (unsigned status = 0; status < 256; ++status)
        compare(data, 0x8275, 0, y, status);
    printf("VS semantic kernels: %u exact register/status/stack/RAM comparisons passed\n", cases);
}
