/* Compare semantic helpers with the original generated instruction functions. */
#include "vs_cpu.h"
#include <stdio.h>
#include "test_check.h"
void vs_program_reference(VsCpu *, unsigned);

static unsigned cases;
typedef struct {
    unsigned count;
    uint32_t events[300];
} PpuTrace;

static void trace_ppu_write(void *ctx, uint16_t address, uint8_t value) {
    PpuTrace *trace = ctx;
    assert(trace->count < sizeof(trace->events) / sizeof(trace->events[0]));
    trace->events[trace->count++] = ((uint32_t)address << 8) | value;
}

static void trace_ppu_run(void *ctx, const uint8_t *source,
                          uint8_t count, uint8_t repeat) {
    for (unsigned i = 0; i < count; ++i)
        trace_ppu_write(ctx, 0x2007u, source[repeat ? 0u : i]);
}

static void compare(const uint8_t *prg, uint16_t entry, unsigned x, unsigned y, unsigned pattern) {
    VsBus reference, optimized;
    VsCpu want, got;
    PpuTrace expected_writes, actual_writes;
    vs_bus_init(&reference, prg, prg + 32768);
    for (unsigned i = 0; i < 2048; ++i) reference.ram[i] = (uint8_t)(i * 13 + pattern);
    reference.ram[3] = (uint8_t)((pattern & ~2u) | ((x & 256u) ? 2u : 0u));
    vs_cpu_init(&want, &reference);
    want.pc = entry; want.x = (uint8_t)x; want.y = (uint8_t)y;
    want.a = (uint8_t)pattern; want.p = (uint8_t)(x ^ y ^ pattern);
    want.s = (uint8_t)(pattern + y);
    if (entry == 0xd5bd || entry == 0xe7da || entry == 0xe9a8 ||
        entry == 0xeac1 || entry == 0xc9ba || entry == 0xd98a ||
        entry == 0xdf17 || entry == 0xe525 || entry == 0xbeea ||
        entry == 0xeeaa || entry == 0x914a ||
        ((entry == 0xae40 || entry == 0xae71) && x < 256u))
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
    if (entry == 0xb2fd) {
        reference.ram[0x1d] = (uint8_t)(x & 3u);
        reference.ram[0x070e] = (uint8_t)((x >> 2) & 1u);
        reference.ram[0x0a] = (uint8_t)y;
        reference.ram[0x0b] = (uint8_t)y;
        reference.ram[0x0c] = (uint8_t)(y & 3u);
        reference.ram[0x0490] = (uint8_t)(pattern >> 8);
        reference.ram[0x0d] = (x & 8u) ? 0x80u : 0u;
        reference.ram[0x0704] = (uint8_t)((x >> 4) & 1u);
        reference.ram[0x0782] = (uint8_t)((x >> 5) & 1u);
        reference.ram[0x0783] = (uint8_t)((pattern >> 16) & 1u);
        reference.ram[0x074e] = (uint8_t)((x >> 6) & 3u);
        reference.ram[0x0700] = (uint8_t)(pattern >> 8);
        reference.ram[0x047d] = (uint8_t)((pattern >> 10) & 1u);
        reference.ram[0x0e] = (pattern & 1u) ? 7u : 8u;
        reference.ram[0x0754] = (uint8_t)(pattern & 1u);
        reference.ram[0x45] = (uint8_t)(1u + ((pattern >> 2) & 1u));
        reference.ram[0x33] = (uint8_t)(1u + ((pattern >> 1) & 1u));
        reference.ram[0x0703] = (uint8_t)((pattern >> 5) & 1u);
    }
    if (entry == 0xb43c) {
        reference.ram[0x0700] = (uint8_t)y;
        reference.ram[0x06fc] = (uint8_t)x;
        reference.ram[0x45] = (uint8_t)(pattern & 3u);
    }
    if (entry == 0xb479) {
        want.a = (uint8_t)(x & 3u);
        reference.ram[0x0490] = (uint8_t)((x >> 2) & 3u);
        reference.ram[0x57] = (uint8_t)y;
        reference.ram[0x0705] = (uint8_t)(pattern >> 8);
        reference.ram[0x0701] = (uint8_t)((x >> 4) & 1u);
        reference.ram[0x0702] = (uint8_t)pattern;
        reference.ram[0x0450] = (uint8_t)(0u - ((pattern >> 16) & 63u));
        reference.ram[0x0456] = (uint8_t)((pattern >> 16) & 63u);
    }
    if (entry == 0xae40 || entry == 0xae71) {
        reference.ram[0x0723] = (uint8_t)(x & 1u);
        reference.ram[0x0785] = (uint8_t)((x >> 1) & 1u);
        reference.ram[0x0755] = (uint8_t)y;
        reference.ram[0x06ff] = (uint8_t)(pattern >> 8);
        reference.ram[0x03a1] = (uint8_t)((x >> 2) & 3u);
        reference.ram[0x0c] = (uint8_t)((x >> 4) & 3u);
        reference.ram[0x071a] = (uint8_t)(pattern >> 16);
        reference.ram[0x071c] = (uint8_t)pattern;
        reference.ram[0x071b] = (uint8_t)(reference.ram[0x071a] + 1u);
        reference.ram[0x071d] = (uint8_t)(reference.ram[0x071c] - 1u);
        reference.ram[0x6d] = (uint8_t)(reference.ram[0x071a] +
            ((x >> 6) & 3u) - 1u);
        reference.ram[0x86] = (uint8_t)(pattern >> 5);
    }
    if (entry == 0x914a) {
        unsigned pointer = 0x0300u + (y & 255u);
        unsigned count = 1u + (pattern % 63u);
        unsigned repeat = (pattern >> 6) & 1u;
        unsigned next = pointer + (repeat ? 4u : count + 3u);
        reference.ram[0] = (uint8_t)pointer;
        reference.ram[1] = (uint8_t)(pointer >> 8);
        reference.ram[pointer] = (uint8_t)(0x20u | (x & 31u));
        reference.ram[pointer + 1u] = (uint8_t)pattern;
        reference.ram[pointer + 2u] = (uint8_t)(count |
            (repeat << 6) | ((pattern >> 1) & 0x80u));
        reference.ram[next] = 0u;
        want.a = reference.ram[pointer];
        want.y = 0u;
    }
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
    if (entry == 0xeeaa) {
        reference.ram[0x06e4] = (uint8_t)((y % 57u) * 4u);
        reference.ram[0x0e] = (uint8_t)(pattern & 15u);
        reference.ram[0x0711] = (uint8_t)(pattern & 7u);
        reference.ram[0x0781] = (uint8_t)((pattern >> 4) & 7u);
        reference.ram[0x57] = (uint8_t)((pattern >> 7) & 1u);
        reference.ram[0x0c] = (uint8_t)((pattern >> 8) & 3u);
        reference.ram[0x03d0] = (uint8_t)(pattern >> 5);
    }
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
        unsigned slot = (x & 255u) % 6u;
        unsigned bowser = (pattern >> 12) & 7u;
        want.x = (uint8_t)slot;
        reference.ram[8] = (uint8_t)slot;
        reference.ram[0x16u + slot] = (uint8_t)(x >> 8);
        reference.ram[0x1eu + slot] = (uint8_t)pattern;
        reference.ram[0x06e5u + slot] = (uint8_t)((y % 59u) * 4u);
        reference.ram[0x036a] = bowser < 3u ? (uint8_t)bowser : 0;
        reference.ram[0x0363] = (uint8_t)(pattern >> 5);
        reference.ram[0x0747] = (uint8_t)((y >> 6) & 1u);
        reference.ram[0x0796u + slot] = (uint8_t)((pattern >> 4) & 7u);
        reference.ram[0x078au + slot] = (uint8_t)(pattern >> 2);
        reference.ram[0x070e] = (uint8_t)(pattern & 3u);
        reference.ram[0x075f] = (uint8_t)(y & 7u);
        reference.ram[0x078f] = (uint8_t)pattern;
        reference.ram[0x03d1] = (uint8_t)(pattern >> 8);
    }
    if (entry == 0xe525) {
        unsigned slot = x % 6u;
        want.x = (uint8_t)slot;
        reference.ram[8] = (uint8_t)slot;
        reference.ram[0x06e5u + slot] = (uint8_t)((y % 59u) * 4u);
        reference.ram[0x074e] = (uint8_t)(pattern & 3u);
        reference.ram[0x06cc] = (uint8_t)((pattern >> 2) & 1u);
        reference.ram[0x0743] = (uint8_t)((pattern >> 3) & 1u);
    }
    if (entry == 0xbeea) {
        unsigned slot = x % 22u;
        want.x = (uint8_t)slot;
        want.a = (uint8_t)(pattern & 1u);
        reference.ram[0x9fu + slot] = (uint8_t)y;
        reference.ram[0x0433u + slot] = (uint8_t)(pattern >> 1);
        reference.ram[0] = (uint8_t)(pattern >> 9);
        reference.ram[1] = (uint8_t)(pattern >> 7);
        reference.ram[2] = (uint8_t)(pattern >> 5);
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
    if (entry == 0x914a) {
        expected_writes.count = actual_writes.count = 0u;
        reference.context = &expected_writes;
        optimized.context = &actual_writes;
        reference.ppu_write = optimized.ppu_write = trace_ppu_write;
        optimized.ppu_run = (x & 1u) ? trace_ppu_run : NULL;
    }
    vs_program_reference(&want, 200000);
    vs_program_run(&got, 200000);
    if (entry == 0x914a) {
        assert(expected_writes.count == actual_writes.count);
        assert(memcmp(expected_writes.events, actual_writes.events,
                      expected_writes.count * sizeof(uint32_t)) == 0);
    }
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
    for (unsigned seed = 0; seed < 65536; ++seed)
        compare(data, 0x914a, seed >> 8, seed, seed * 173);
    /* Whole player policies: every speed/button byte, grounded/airborne/swim/
     * climb modes, fractional carry/borrow, skid and camera boundaries. */
    for (unsigned seed = 0; seed < 65536; ++seed) {
        compare(data, 0xb2fd, seed >> 8, seed, seed * 179);
        compare(data, 0xb43c, seed >> 8, seed, seed * 181);
        compare(data, 0xb479, seed >> 8, seed, seed * 191);
        compare(data, 0xae40, seed >> 8, seed, seed * 193);
        compare(data, 0xae71, seed >> 8, seed, seed * 197);
    }
    /* Also retain the literal scroll path for wrapped/aliased stack layouts. */
    for (unsigned seed = 0; seed < 4096; ++seed) {
        compare(data, 0xae40, 256u + (seed >> 4), seed, seed * 199);
        compare(data, 0xae71, 256u + (seed >> 4), seed, seed * 211);
    }
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
        compare(data, 0xe7da, (seed % 6u) | ((seed >> 10) << 8),
                seed >> 8, seed * 103);
        compare(data, 0xe525, seed % 6u, seed >> 8, seed * 157);
        compare(data, 0xbeea, seed % 22u, seed >> 8, seed * 163);
        compare(data, 0xeeaa, seed >> 8, seed, seed * 167);
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
