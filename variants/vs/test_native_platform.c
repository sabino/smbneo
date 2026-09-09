#include "vs_native_platform.h"
#include "vs_native_fast_video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned writes;
    unsigned runs;
    uint16_t last_address;
    uint8_t last_count;
    uint16_t last_increment;
    uint8_t last_repeat;
} VideoSpy;

static void video_write(void *context, uint16_t address, uint8_t value) {
    VideoSpy *spy = context;
    ++spy->writes;
    spy->last_address = address;
    (void)value;
}

static uint8_t video_run(void *context, uint16_t address, const uint8_t *source,
                         uint8_t count, uint16_t increment, uint8_t repeat) {
    VideoSpy *spy = context;
    ++spy->runs;
    spy->last_address = address;
    spy->last_count = count;
    spy->last_increment = increment;
    spy->last_repeat = repeat;
    (void)source;
    return 1;
}

static uint8_t video_run_decline(void *context, uint16_t address,
                                 const uint8_t *source, uint8_t count,
                                 uint16_t increment, uint8_t repeat) {
    (void)video_run(context, address, source, count, increment, repeat);
    return 0;
}

static void native_write8(void *context, uint16_t address, uint8_t value) {
    VsNativePlatform *platform = (VsNativePlatform *)context;
    vs_bus_write(&platform->bus, address, value);
}

static void ppu_address(VsBus *bus, uint16_t address) {
    vs_bus_write(bus, 0x2006, (uint8_t)(address >> 8));
    vs_bus_write(bus, 0x2006, (uint8_t)address);
}

static void test_bus_ppu_and_hooks(void) {
    uint8_t prg[0x8000], chr[0x4000];
    VsNativePlatform p;
    VideoSpy spy = {0};
    memset(prg, 0x5a, sizeof(prg));
    for (unsigned i = 0; i < sizeof(chr); ++i) chr[i] = (uint8_t)i;
    vs_native_platform_init(&p, prg, chr, 0x10);
    vs_native_platform_set_video_hooks(&p, &spy, video_write, video_run);
    assert(vs_native_platform_bus(&p) == &p.bus);
    assert(vs_native_platform_const_bus(&p) == &p.bus);
    assert(vs_native_platform_vram(&p, 0x0000) == chr[0]);
    assert(vs_native_platform_vram(&p, 0x2000) == p.nametable[0]);

    VsBus *bus = &p.bus;
    ppu_address(bus, 0x2000);
    vs_bus_write(bus, 0x2007, 0x31);
    assert(p.nametable[0] == 0x31 && spy.writes == 1);
    ppu_address(bus, 0x3f10);
    vs_bus_write(bus, 0x2007, 0x7f);
    assert(p.palette[0] == 0x3f && spy.writes == 2);

    p.ctrl = 4;
    ppu_address(bus, 0x2020);
    {
        const uint8_t bytes[] = {1, 2, 3};
        bus->ppu_run(bus->context, bytes, 3, 0);
    }
    assert(spy.runs == 1 && spy.last_address == 0x2020 &&
           spy.last_count == 3 && spy.last_increment == 32 &&
           p.address == 0x2080 && p.nametable[0x20] == 1 &&
           p.nametable[0x40] == 2 && p.nametable[0x60] == 3);
    vs_native_platform_inputs(&p, 0x12, 0x34, 0x20);
    assert(p.bus.pads[0] == 0x12 && p.bus.pads[1] == 0x34 &&
           p.bus.dips == 0x10 && p.bus.coin_service == 0x20);
    assert(vs_bus_read(bus, 0x8000) == 0x5a);
}

static void test_ppu_buffer_palette_and_dma(void) {
    uint8_t chr[0x4000];
    VsNativePlatform p;
    memset(chr, 0, sizeof(chr));
    vs_native_platform_init(&p, 0, chr, 0);
    VsBus *bus = &p.bus;
    p.nametable[0] = 0x77;
    ppu_address(bus, 0x2000);
    assert(vs_bus_read(bus, 0x2007) == 0);
    assert(vs_bus_read(bus, 0x2007) == 0x77);
    ppu_address(bus, 0x3f00);
    vs_bus_write(bus, 0x2007, 0x12);
    assert(p.palette[0] == 0x12);
    p.oam_address = 0xf0;
    for (unsigned i = 0; i < sizeof(p.bus.ram); ++i) p.bus.ram[i] = (uint8_t)i;
    vs_bus_write(bus, 0x4014, 0);
    for (unsigned i = 0; i < 16; ++i) assert(p.oam[0xf0 + i] == (uint8_t)i);
    for (unsigned i = 0; i < 0xf0; ++i) assert(p.oam[i] == (uint8_t)(i + 16));
    vs_bus_write(bus, 0x4000, 0x55);
    assert(p.apu[0] == 0x55 && p.apu_writes == 1);
}

static void test_fast_video_bridge(void) {
    uint8_t ram_record[] = {0x20, 0x20, 0x83, 0x11, 0x22, 0x33};
    uint8_t chr[0x4000] = {0};
    VsNativePlatform first, second;
    VsNativeFastVideoHooks first_hooks, second_hooks, null_hooks;
    VideoSpy first_spy = {0}, second_spy = {0};
    SmbNeoNativeContext native = {0};

    vs_native_platform_init(&first, NULL, chr, 0x10u);
    vs_native_platform_init(&second, NULL, chr, 0x10u);
    vs_native_platform_set_video_hooks(&first, &first_spy,
                                       video_write, video_run);
    vs_native_platform_set_video_hooks(&second, &second_spy,
                                       video_write, video_run);
    vs_native_platform_fast_video_hooks(&first, &first_hooks);
    vs_native_platform_fast_video_hooks(&second, &second_hooks);
    vs_native_platform_fast_video_hooks(NULL, &null_hooks);
    vs_native_platform_fast_video_hooks(&first, NULL);
    assert(first_hooks.opaque == &first && second_hooks.opaque == &second);
    assert(first_hooks.ppu_data_run != NULL &&
           second_hooks.ppu_data_run != NULL &&
           null_hooks.opaque == NULL && null_hooks.ppu_data_run == NULL);

    /* The callback is per-instance and commits the platform's complete fast
     * nametable run, including the external renderer notification. */
    first.ctrl = 4u;
    ppu_address(&first.bus, 0x2020u);
    assert(first_hooks.ppu_data_run(first_hooks.opaque, 0x2020u,
                                    ram_record + 3u, 3u, 32u, false));
    assert(first_spy.runs == 1u && first_spy.writes == 0u);
    assert(first.nametable[0x020u] == 0x11u &&
           first.nametable[0x040u] == 0x22u &&
           first.nametable[0x060u] == 0x33u &&
           first.address == 0x2080u);
    assert(second_spy.runs == 0u && second.nametable[0x020u] == 0u);

    second.ctrl = 0u;
    ppu_address(&second.bus, 0x21feu);
    assert(second_hooks.ppu_data_run(second_hooks.opaque, 0x21feu,
                                     ram_record + 3u, 3u, 1u, true));
    assert(second_spy.runs == 1u && second_spy.last_repeat == 1u);
    assert(second.nametable[0x1feu] == 0x11u &&
           second.nametable[0x1ffu] == 0x11u &&
           second.nametable[0x200u] == 0x11u &&
           second.address == 0x2201u);

    /* A renderer decline still returns committed: VsBus falls back to its
     * ordered PPU writes, updating storage, address and the scalar sink. */
    memset(&first_spy, 0, sizeof(first_spy));
    vs_native_platform_set_video_hooks(&first, &first_spy,
                                       video_write, video_run_decline);
    first.ctrl = 0u;
    ppu_address(&first.bus, 0x2300u);
    assert(first_hooks.ppu_data_run(first_hooks.opaque, 0x2300u,
                                    ram_record + 3u, 3u, 1u, false));
    assert(first_spy.runs == 1u && first_spy.writes == 3u);
    assert(first.nametable[0x300u] == 0x11u &&
           first.nametable[0x301u] == 0x22u &&
           first.nametable[0x302u] == 0x33u &&
           first.address == 0x2303u);

    /* Palette and other exceptional destinations are committed by the same
     * full bus routine through its scalar path, never renderer-only batching. */
    memset(&first_spy, 0, sizeof(first_spy));
    ppu_address(&first.bus, 0x3f10u);
    assert(first_hooks.ppu_data_run(first_hooks.opaque, 0x3f10u,
                                    ram_record + 3u, 1u, 1u, false));
    assert(first.palette[0] == (0x11u & 63u));
    assert(first.address == 0x3f11u && first_spy.runs == 0u &&
           first_spy.writes == 1u);

    /* Declines are transactional, allowing the helper's scalar fallback. */
    {
        const uint16_t before_address = first.address;
        const unsigned before_writes = first_spy.writes;
        assert(!first_hooks.ppu_data_run(first_hooks.opaque, 0x2000u,
                                         ram_record, 3u, 1u, false));
        assert(!first_hooks.ppu_data_run(first_hooks.opaque, before_address,
                                         ram_record, 3u, 32u, false));
        assert(!first_hooks.ppu_data_run(first_hooks.opaque, before_address,
                                         ram_record, 0u, 1u, false));
        assert(!first_hooks.ppu_data_run(first_hooks.opaque, before_address,
                                         NULL, 3u, 1u, false));
        assert(first.address == before_address &&
               first_spy.writes == before_writes);
    }

    /* End-to-end: one semantic display-list command reaches the full bridge. */
    vs_native_platform_init(&first, NULL, chr, 0x10u);
    memset(&first_spy, 0, sizeof(first_spy));
    vs_native_platform_set_video_hooks(&first, &first_spy,
                                       video_write, video_run);
    vs_native_platform_fast_video_hooks(&first, &first_hooks);
    memcpy(first.bus.ram + 0x0300u, ram_record, sizeof(ram_record));
    first.bus.ram[0] = 0x00u;
    first.bus.ram[1] = 0x03u;
    native.a = 0x20u;
    native.x = 0xa5u;
    native.y = 0u;
    native.status = 0xffu;
    native.ram = first.bus.ram;
    native.opaque = &first;
    native.write8 = native_write8;
    assert(vs_native_fast_ppu_displist_command(&native, &first_hooks));
    assert(first_spy.runs == 1u && first_spy.last_address == 0x2020u &&
           first_spy.last_count == 3u && first_spy.last_increment == 32u);
    assert(first.nametable[0x020u] == 0x11u &&
           first.nametable[0x040u] == 0x22u &&
           first.nametable[0x060u] == 0x33u);
    assert(first.bus.ram[0] == 0x06u && first.bus.ram[1] == 0x03u);
    assert(native.a == 0u && native.x == 0u && native.y == 5u &&
           native.status == (uint8_t)((0xffu &
               (uint8_t)~(NATIVE_C | NATIVE_V | NATIVE_N | NATIVE_Z)) |
               NATIVE_Z));
}

int main(void) {
    test_bus_ppu_and_hooks();
    test_ppu_buffer_palette_and_dma();
    test_fast_video_bridge();
    puts("native VS platform: bus, PPU, palette, OAM DMA, APU, inputs and hooks passed");
    return 0;
}
