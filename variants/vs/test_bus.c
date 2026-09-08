#include "vs_bus.h"
#include <assert.h>
#include <stdio.h>
#include "test_check.h"

static unsigned calls;
static uint16_t last_address;
static uint8_t last_value;
static uint8_t read_ppu(void *ctx, uint16_t address) {
    assert(ctx == &calls); ++calls; last_address = address; return 0x82;
}
static void write_io(void *ctx, uint16_t address, uint8_t value) {
    assert(ctx == &calls); ++calls; last_address = address; last_value = value;
}
static void dma(void *ctx, uint16_t address) {
    assert(ctx == &calls); ++calls; last_address = address;
}

int main(void) {
    static uint8_t prg[32768], chr[16384];
    VsBus bus;
    for (unsigned i = 0; i < sizeof(prg); ++i) prg[i] = (uint8_t)(i ^ (i >> 8));
    for (unsigned i = 0; i < sizeof(chr); ++i) chr[i] = (uint8_t)(i ^ (i >> 8));
    vs_bus_init(&bus, prg, chr);
    assert(bus.dips == 0x10);
    for (unsigned i = 0; i < 2048; ++i) {
        vs_bus_write(&bus, (uint16_t)i, (uint8_t)i);
        vs_bus_write(&bus, (uint16_t)(0x6000 + i), (uint8_t)~i);
        for (unsigned mirror = 0; mirror < 4; ++mirror) {
            assert(vs_bus_read(&bus, (uint16_t)(i + mirror * 2048)) == (uint8_t)i);
            assert(vs_bus_read(&bus, (uint16_t)(0x6000 + i + mirror * 2048)) == (uint8_t)~i);
        }
    }
    for (unsigned i = 0; i < 32768; ++i) {
        vs_bus_write(&bus, (uint16_t)(0x8000 + i), 0);
        assert(vs_bus_read(&bus, (uint16_t)(0x8000 + i)) == prg[i]);
    }
    for (unsigned d = 0; d < 256; ++d) {
        vs_bus_inputs(&bus, 0xa5, 0x5a, (uint8_t)d, 0xff);
        vs_bus_write(&bus, 0x4016, 1);
        assert((vs_bus_read(&bus, 0x4016) & 1) == 1);
        assert((vs_bus_read(&bus, 0x4016) & 1) == 1);
        vs_bus_write(&bus, 0x4016, 0);
        for (unsigned bit = 0; bit < 10; ++bit) {
            assert(vs_bus_read(&bus, 0x4016) == (((0xa5u >> bit) & 1) | 0x64 | ((d & 3) << 3)));
            assert(vs_bus_read(&bus, 0x4017) == (((0x5au >> bit) & 1) | (d & 0xfc)));
        }
    }
    for (unsigned bank = 0; bank < 2; ++bank) {
        vs_bus_write(&bus, 0x4016, (uint8_t)(bank << 2));
        for (unsigned i = 0; i < 8192; ++i)
            assert(vs_bus_chr_read(&bus, (uint16_t)i) == chr[bank * 8192 + i]);
    }
    vs_bus_write(&bus, 0x4020, 0xaa);
    vs_bus_write(&bus, 0x4020, 0xab);
    vs_bus_write(&bus, 0x4020, 0xab);
    assert(bus.coin_counter == 1);
    assert(vs_bus_read(&bus, 0x4020) == 0xab);
    assert(vs_bus_read(&bus, 0x4120) == 0xab);
    assert(vs_bus_read(&bus, 0x4120) == 0xab);
    assert(bus.coin_counter == 2);
    bus.context = &calls; bus.ppu_read = read_ppu;
    bus.ppu_write = write_io; bus.apu_write = write_io; bus.oam_dma = dma;
    assert(vs_bus_read(&bus, 0x3ffa) == 0x82 && last_address == 0x2002);
    vs_bus_write(&bus, 0x3fff, 3);
    assert(last_address == 0x2007 && last_value == 3);
    vs_bus_write(&bus, 0x4017, 0x40);
    assert(last_address == 0x4017 && last_value == 0x40);
    vs_bus_write(&bus, 0x4014, 2);
    assert(last_address == 0x200 && calls == 4);
    puts("VS bus: RAM, banked data, all DIP settings, controllers, coin edges and I/O passed");
    return 0;
}
