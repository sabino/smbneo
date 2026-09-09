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
    if (entry == 0xd5bd || entry == 0xe7da || entry == 0xe9a8 ||
        entry == 0xeac1 || entry == 0xc9ba || entry == 0xd98a ||
        entry == 0xdf17)
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
    if (entry == 0x8aaf) {
        reference.ram[0x0340] = x >= 256u ? (uint8_t)x : 0;
        reference.ram[0x071f] = (uint8_t)(pattern & 7u);
        reference.ram[0x0720] = (uint8_t)(0x20u | (pattern & 4u));
        reference.ram[0x0721] = (uint8_t)pattern;
        reference.ram[0x0726] = (uint8_t)(pattern >> 1);
        for (unsigned row = 0; row < 13; ++row)
            reference.ram[0x06a1u + row] =
                (uint8_t)(pattern + row * (x + 17u));
    }
    if (entry == 0xef41)
        reference.ram[7] = (uint8_t)(1u + (pattern & 3u));
    if (entry == 0xbfea || entry == 0xbf60) {
        reference.ram[(uint8_t)(0x0fu + x)] = 0;
        reference.ram[0x071f] = (uint8_t)(pattern & 7u);
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
    if (entry == 0xc9ba) {
        unsigned slot = x % 6u;
        want.x = (uint8_t)slot;
        reference.ram[8] = (uint8_t)slot;
        reference.ram[0x1eu + slot] = 0;
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
    if (entry == 0xe7da) {
        unsigned slot = x % 6u;
        want.x = (uint8_t)slot;
        reference.ram[8] = (uint8_t)slot;
        reference.ram[0x16u + slot] = 6;
        reference.ram[0x1eu + slot] = 0;
        reference.ram[0x036a] = 0;
        reference.ram[0x0747] = 0;
        reference.ram[0x0796u + slot] = (uint8_t)(pattern % 5u);
        reference.ram[0x03d1] = (uint8_t)(pattern & 0x1fu);
    }
    if (entry == 0xd7aa)
        reference.ram[9] |= 1u;
    if (entry == 0xd98a) {
        unsigned slot = x % 6u;
        want.x = (uint8_t)slot;
        reference.ram[8] = (uint8_t)slot;
        switch (pattern & 7u) {
        case 0:
            reference.ram[9] &= (uint8_t)~1u;
            break;
        case 1:
            reference.ram[9] |= 1u;
            reference.ram[0x074e] = 0;
            break;
        case 2:
            reference.ram[9] |= 1u;
            reference.ram[0x074e] = 1;
            reference.ram[0x16u + slot] = 0x15;
            break;
        default:
            reference.ram[9] |= 1u;
            reference.ram[0x074e] = 1;
            reference.ram[0x16u + slot] = 6;
            reference.ram[0x03d8u + slot] = 0;
            reference.ram[0x03d1] &= 0xf0u;
            for (unsigned actor = 0; actor <= slot; ++actor) {
                unsigned box = actor * 4u + 4u;
                reference.ram[0x0fu + actor] = actor == slot ||
                    ((pattern >> actor) & 1u) ? 1 : 0;
                reference.ram[0x16u + actor] = 6;
                reference.ram[0x03d8u + actor] = 0;
                reference.ram[0x04acu + box] = actor == slot
                    ? 100 : (uint8_t)(20u + actor * 12u);
                reference.ram[0x04acu + box + 1u] = 100;
                reference.ram[0x04aeu + box] = actor == slot
                    ? 110 : (uint8_t)(25u + actor * 12u);
                reference.ram[0x04aeu + box + 1u] = 110;
            }
            break;
        }
    }
    if (entry == 0xdf17) {
        unsigned slot = x % 6u;
        unsigned mode = pattern & 7u;
        want.x = (uint8_t)slot;
        reference.ram[8] = (uint8_t)slot;
        reference.ram[0x1eu + slot] = 0;
        reference.ram[0x16u + slot] = 6;
        reference.ram[0x46u + slot] = 0;
        if (mode == 0) {
            reference.ram[0x1eu + slot] = 0x20u;
        } else if (mode == 1) {
            reference.ram[0xcfu + slot] = 0;
        } else if (mode == 2 || mode == 3) {
            reference.ram[0xcfu + slot] = 0x88;
            reference.ram[0x16u + slot] = mode == 2 ? 0x0d : 0x11;
        } else {
            unsigned object = slot + 1u;
            unsigned adder = 0x15u;
            unsigned sum;
            unsigned column;
            unsigned pointer;
            unsigned adjusted_y;
            uint8_t tile = mode == 7 ? 0 : 0x50;

            reference.ram[0xcfu + slot] = mode == 6 ? 0x80 :
                (uint8_t)(0x88u + (mode == 5));
            sum = prg[0xe306u - 0x8000u + adder]
                + reference.ram[0x86u + object];
            column = (unsigned)((((reference.ram[0x6du + object]
                + (sum > 0xffu)) & 1u) << 4) | (((uint8_t)sum) >> 4));
            pointer = (column & 0x10u ? 0x05d0u : 0x0500u)
                + (column & 15u);
            adjusted_y = (uint8_t)((((reference.ram[0xceu + object]
                + prg[0xe322u - 0x8000u + adder]) & 0xf0u) - 0x20u));
            reference.ram[(pointer + adjusted_y) & 0x07ffu] = tile;
        }
    }
    if (entry == 0xe054) {
        unsigned slot = x % 6u;
        unsigned mode = pattern & 3u;
        unsigned adder;
        unsigned object;
        unsigned sum;
        unsigned column;
        unsigned pointer;
        unsigned adjusted_y;

        want.x = (uint8_t)slot;
        reference.ram[8] = (uint8_t)slot;
        reference.ram[0xcfu + slot] = mode == 0 ? 0x10 : 0x70;
        reference.ram[0x46u + slot] = mode < 2 ? 0 : (uint8_t)(mode - 1u);
        if (mode >= 2) {
            adder = mode == 2 ? 0x17u : 0x16u;
            object = slot + 1u;
            sum = prg[0xe306u - 0x8000u + adder]
                + reference.ram[0x86u + object];
            column = (unsigned)((((reference.ram[0x6du + object]
                + (sum > 0xffu)) & 1u) << 4) | (((uint8_t)sum) >> 4));
            pointer = (column & 0x10u ? 0x05d0u : 0x0500u)
                + (column & 15u);
            adjusted_y = (uint8_t)((((reference.ram[0xceu + object]
                + prg[0xe322u - 0x8000u + adder]) & 0xf0u) - 0x20u));
            reference.ram[(pointer + adjusted_y) & 0x07ffu] = 0;
        }
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
    for (unsigned seed = 0; seed < 65536; ++seed)
        compare(data, 0x8aaf, seed >> 8, seed, seed * 151);
    /* Nonempty offsets can alias the attribute buffer. Check every offset's
     * fallback, both column parities and both metatile sides. */
    for (unsigned offset = 1; offset < 256; ++offset)
        for (unsigned mode = 0; mode < 4; ++mode)
            compare(data, 0x8aaf, 256u + offset, offset * 11u, mode);
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
        compare(data, 0xbf60, seed % 5u, seed >> 8, seed * 97);
        compare(data, 0xc9ba, seed % 6u, seed >> 8, seed * 101);
        compare(data, 0xe7da, seed % 6u, seed >> 8, seed * 103);
        compare(data, 0xd7aa, seed % 6u, seed >> 8, seed * 107);
        compare(data, 0xd98a, seed % 6u, seed >> 8, seed * 109);
        compare(data, 0xdf17, seed % 6u, seed >> 8, seed * 113);
        compare(data, 0xe0b1, seed % 6u, seed >> 8, seed * 127);
        compare(data, 0xe2de, seed % 21u, seed >> 8, seed * 131);
        compare(data, 0xe104, seed % 6u, seed >> 8, seed * 137);
        compare(data, 0xe054, seed % 6u, seed >> 8, seed * 139);
        compare(data, 0xe10b, seed >> 8, seed, seed * 149);
    }
    printf("VS semantic kernels: %u exact register/status/stack/RAM comparisons passed\n", cases);
}
