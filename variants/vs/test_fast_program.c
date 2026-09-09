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
    if (entry == 0xd5bd || entry == 0xe9a8 || entry == 0xeac1)
        want.s = 0xfd;
    vs_push(&want, 0xff); vs_push(&want, 0xfe);
    reference.pads[0] = (uint8_t)pattern;
    reference.pads[1] = (uint8_t)(pattern * 37u + y);
    reference.latch[0] = (uint8_t)(pattern ^ x);
    reference.latch[1] = (uint8_t)(pattern + x + y);
    reference.strobe = (uint8_t)(pattern & 1u);
    reference.dips = (uint8_t)(pattern * 11u);
    reference.coin_service = (uint8_t)(pattern & 0x64u);
    if (entry == 0xba8a)
        memset(reference.ram + 0x2a, 0, 9);
    if (entry == 0xef41)
        reference.ram[7] = (uint8_t)(1u + (pattern & 3u));
    if (entry == 0xbfea) {
        reference.ram[0x0745] = 0;
        reference.ram[0x06cd] = 0;
        reference.ram[0x0739] = 0;
        reference.ram[0x00e9] = 0;
        reference.ram[0x00ea] = 0x60;
        reference.extra_ram[0] = 0x78;
        reference.extra_ram[1] = (uint8_t)pattern;
        reference.ram[0x073a] = 2;
        reference.ram[0x073b] = (uint8_t)(pattern & 1u);
        reference.ram[0x071d] = 0;
        reference.ram[0x071b] = 0;
        reference.ram[0x06cb] = 0;
        reference.ram[0x0398] = 0;
    }
    if (entry == 0xeac1) {
        reference.ram[8] = (uint8_t)(x % 6u);
        reference.ram[0x03d1] = (uint8_t)(pattern & 0x1fu);
    }
    if (entry == 0xe9a8) {
        unsigned slot = x % 6u;
        reference.ram[8] = (uint8_t)slot;
        reference.ram[0xeb] = (uint8_t)(pattern & 0xe0u);
        reference.ram[0x06e5u + slot] = (uint8_t)(pattern & 0xe0u);
        reference.ram[0xef] = 6;
        reference.ram[0xec] = 0;
        reference.ram[0x036a] = 0;
        reference.ram[0x0109] = 0;
        reference.ram[0x03d1] = 0;
    }
    optimized = reference; got = want; got.bus = &optimized;
    vs_program_reference(&want, 200000);
    vs_program_run(&got, 200000);
    if (!(want.fault && got.pc == want.pc && got.fault)) {
        fprintf(stderr,
                "entry=%04x seed=%u/%u/%u terminal want=%04x/%u got=%04x/%u\n",
                entry, x, y, pattern, want.pc, want.fault, got.pc, got.fault);
        assert(0);
    }
    if (!(want.a == got.a && want.x == got.x && want.y == got.y && want.s == got.s && want.p == got.p)) {
        fprintf(stderr, "entry=%04x seed=%u/%u/%u want=%02x,%02x,%02x,%02x,%02x got=%02x,%02x,%02x,%02x,%02x\n",
                entry, x, y, pattern, want.a, want.x, want.y, want.s, want.p,
                got.a, got.x, got.y, got.s, got.p);
        for (unsigned i = 1758; i < 1790; ++i)
            if (reference.ram[i] != optimized.ram[i])
                fprintf(stderr, "ram[%u] want=%02x got=%02x\n", i, reference.ram[i], optimized.ram[i]);
        assert(0);
    }
    assert(want.idle == got.idle && want.in_nmi == got.in_nmi && want.yielded == got.yielded);
    if (memcmp(reference.ram, optimized.ram, sizeof(reference.ram)) != 0) {
        for (unsigned i = 0; i < sizeof(reference.ram); ++i) {
            if (reference.ram[i] != optimized.ram[i]) {
                fprintf(stderr,
                        "entry=%04x seed=%u/%u/%u ram[%04x] want=%02x got=%02x\n",
                        entry, x, y, pattern, i, reference.ram[i], optimized.ram[i]);
                break;
            }
        }
        assert(0);
    }
    assert(memcmp(reference.extra_ram, optimized.extra_ram, sizeof(reference.extra_ram)) == 0);
    assert(memcmp(reference.latch, optimized.latch, sizeof(reference.latch)) == 0);
    assert(reference.strobe == optimized.strobe);
    assert(reference.chr_bank == optimized.chr_bank);
    assert(reference.ram_control == optimized.ram_control);
    assert(reference.counter_latch == optimized.counter_latch);
    assert(reference.counter_line == optimized.counter_line);
    assert(reference.coin_counter == optimized.coin_counter);
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
    for (unsigned seed = 0; seed < 65536; ++seed)
        compare(data, 0x8212, seed >> 8, seed, seed * 29);
    for (unsigned seed = 0; seed < 65536; ++seed) {
        compare(data, 0xeb07, seed >> 8, seed, seed * 7);
        compare(data, 0xeb0f, seed >> 8, seed, seed * 11);
        compare(data, 0xf15b, seed >> 8, seed, seed * 17);
        compare(data, 0xf19e, seed >> 8, seed, seed * 19);
        compare(data, 0xe348, seed >> 8, seed, seed * 23);
    }
    for (unsigned seed = 0; seed < 65536; ++seed)
        compare(data, 0x9125, seed & 1u, seed >> 8, seed * 29);
    for (unsigned seed = 0; seed < 65536; ++seed)
        compare(data, 0x9256, seed >> 8, seed, seed * 31);
    for (unsigned seed = 0; seed < 65536; ++seed)
        compare(data, 0xba8a, seed >> 8, seed, seed * 47);
    for (unsigned seed = 0; seed < 65536; ++seed) {
        compare(data, 0xf125, seed >> 8, seed, seed * 37);
        compare(data, 0xbe1e, seed >> 8, seed, seed * 41);
        compare(data, 0xe1f2, seed >> 8, seed, seed * 43);
    }
    for (unsigned seed = 0; seed < 65536; ++seed) {
        compare(data, 0xbe11, seed >> 8, seed, seed * 53);
        compare(data, 0xbe18, seed >> 8, seed, seed * 59);
        compare(data, 0xf08f, seed >> 8, seed, seed * 61);
        compare(data, 0xf0b7, seed >> 8, seed, seed * 67);
        compare(data, 0xef41, seed >> 8, seed, seed * 71);
        compare(data, 0xd5bd, seed % 6u, seed >> 8, seed * 73);
        compare(data, 0xbfea, seed % 5u, seed >> 8, seed * 79);
        compare(data, 0xeac1, seed >> 8, seed, seed * 83);
        compare(data, 0xe9a8, seed >> 8, seed, seed * 89);
    }
    printf("VS semantic kernels: %u exact register/status/stack/RAM comparisons passed\n", cases);
}
