#include "vs_machine.h"
#include <stdio.h>
#include "test_check.h"

/* ROM-free adapter tests do not execute a game program. */
void vs_program_run(VsCpu *c, unsigned budget) { (void)c; (void)budget; }

typedef struct {
    uint8_t data[16384];
    uint32_t trace;
    unsigned writes, batches;
} VideoSink;

static void observe_write(void *ctx, uint16_t address, uint8_t value) {
    VideoSink *sink = ctx;
    sink->data[address] = value;
    sink->trace = (sink->trace * 16777619u) ^ ((uint32_t)address << 8) ^ value;
    ++sink->writes;
}

static uint8_t observe_run(void *ctx, uint16_t address, const uint8_t *source,
                           uint8_t count, uint16_t increment, uint8_t repeat) {
    VideoSink *sink = ctx;
    assert(count && address >= 0x2000u);
    assert((uint32_t)address + (count - 1u) * increment < 0x3f00u);
    ++sink->batches;
    for (unsigned i = 0; i < count; ++i) {
        observe_write(ctx, address, source[repeat ? 0u : i]);
        address = (uint16_t)(address + increment);
    }
    return 1;
}

static uint8_t reject_run(void *ctx, uint16_t address, const uint8_t *source,
                          uint8_t count, uint16_t increment, uint8_t repeat) {
    (void)ctx; (void)address; (void)source;
    (void)count; (void)increment; (void)repeat;
    return 0;
}

static void check_ppu_runs(const uint8_t *prg, const uint8_t *chr) {
    static const uint16_t destinations[] = {
        0, 0x1ff0, 0x2000, 0x201f, 0x23c0, 0x27ff,
        0x2fff, 0x3000, 0x3ede, 0x3eff, 0x3f00, 0x3ff0
    };
    static VsMachine ordinary, batched;
    static VideoSink wanted, got;
    uint8_t source[64];
    unsigned cases = 0;

    for (unsigned i = 0; i < sizeof(source); ++i)
        source[i] = (uint8_t)(i * 37u + 9u);
    for (unsigned d = 0; d < sizeof(destinations) / sizeof(destinations[0]); ++d)
        for (unsigned count = 0; count < 64u; ++count)
            for (unsigned mode = 0; mode < 16u; ++mode) {
                uint8_t repeat = mode & 1u;
                vs_machine_init(&ordinary, prg, chr);
                vs_machine_init(&batched, prg, chr);
                memset(&wanted, 0, sizeof(wanted));
                memset(&got, 0, sizeof(got));
                ordinary.address = batched.address = destinations[d];
                ordinary.ctrl = batched.ctrl = (mode & 2u) ? 4u : 0u;
                ordinary.video_write = batched.video_write = observe_write;
                ordinary.video_context = &wanted;
                batched.video_context = &got;
                if (mode & 4u)
                    batched.video_run = observe_run;
                if (mode & 8u)
                    batched.video_run = reject_run;
                for (unsigned i = 0; i < count; ++i)
                    vs_bus_write(&ordinary.bus, 0x2007u, source[repeat ? 0u : i]);
                batched.bus.ppu_run(&batched, source, (uint8_t)count, repeat);
                assert(ordinary.address == batched.address);
                assert(ordinary.ctrl == batched.ctrl);
                assert(ordinary.toggle == batched.toggle);
                assert(memcmp(ordinary.nametable, batched.nametable, 2048) == 0);
                assert(memcmp(ordinary.palette, batched.palette, 32) == 0);
                assert(wanted.writes == got.writes && wanted.trace == got.trace);
                assert(memcmp(wanted.data, got.data, sizeof(wanted.data)) == 0);
                ++cases;
            }
    printf("VS PPU runs: %u horizontal/vertical, repeated, palette and wrap cases passed\n", cases);
}

int main(void) {
    static VsMachine m;
    static uint8_t prg[32768], chr[16384];
    vs_machine_init(&m, prg, chr);
    for (unsigned i = 0; i < 2048; ++i) {
        m.bus.ram[i] = (uint8_t)(i * 13 + (i >> 8));
        m.bus.extra_ram[i] = (uint8_t)(i * 11 + (i >> 8));
    }
    for (unsigned i = 0; i < sizeof(prg); ++i) prg[i] = (uint8_t)(i * 17 + (i >> 8));
    for (unsigned page = 0; page < 256; ++page) {
        if (page >= 0x20 && page < 0x60) continue; /* I/O has observable reads. */
        for (unsigned offset = 0; offset < 256; ++offset) {
            m.oam_address = (uint8_t)offset;
            vs_bus_write(&m.bus, 0x4014, (uint8_t)page);
            assert(m.oam_address == offset);
            for (unsigned i = 0; i < 256; ++i)
                assert(m.oam[(i + offset) & 255] == vs_bus_read(&m.bus, (uint16_t)(page * 256 + i)));
        }
    }
    puts("VS sprite DMA: every RAM mirror, extra RAM/ROM fallback and all OAM wrap offsets passed");
    check_ppu_runs(prg, chr);
}
